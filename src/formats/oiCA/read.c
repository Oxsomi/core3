/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "formats/oiCA/ca_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/base/time.h"

#include "platforms/file.h"

Ns CAFile_loadDate(U16 time, U16 date) {
	return Time_date(
		1980 + (date >> 9), 1 + ((date >> 5) & 0xF), date & 0x1F,
		time >> 11, (time >> 5) & 0x3F, (time & 0x1F) << 1, 0, false
	);
}

Bool CAFile_read(Stream *stream, const U32 encryptionKey[8], Allocator alloc, CAFile *caFile, Stream **fileData, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocate = false;

	if (!caFile)
		retError(clean, Error_nullPointer(2, "CAFile_read()::caFile is required"))

	if (caFile->archive.entries.ptr)
		retError(clean, Error_invalidParameter(2, 0, "CAFile_read()::caFile isn't empty, could indicate memleak"))

	if (!stream || stream->handle.fileSize < sizeof(CAHeader))
		retError(clean, Error_outOfBounds(
			0, sizeof(CAHeader), !stream ? 0 : stream->handle.fileSize, "CAFile_read()::file doesn't contain header"
		))

	Buffer tmpData = Buffer_createNull();
	Buffer tmpData1 = Buffer_createNull();
	DLFile fileNames = (DLFile) { 0 };
	Archive archive = (Archive) { 0 };
	CharString tmpPath = CharString_createNull();
	I32x4 iv = I32x4_zero(), tag = I32x4_zero();
	Stream readStream = (Stream) { 0 };					//For example a decryption or decompression stream

	gotoIfError3(clean, Archive_create(alloc, &archive, e_rr))

	//Validate header

	U64 off = 0;
	CAHeader header;
	gotoIfError3(clean, Stream_read(stream, Buffer_createRef(&header, sizeof(header)), off, 0, 0, false, e_rr))
	off += sizeof(header);

	if(header.magicNumber != CAHeader_MAGIC)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_read()::file contained invalid header"))

	if(header.version != ECAVersion_V1_0)
		retError(clean, Error_invalidParameter(0, 1, "CAFile_read()::file header doesn't have correct version"))

	if((header.flags & ECAFlags_ChunkMask) && !header.type)
		retError(clean, Error_unsupportedOperation(
			0, "CAFile_read() chunks are disabled for unencrypted and uncompressed files"
		))

	if(header.type >> 4)								//TODO: Compression
		retError(clean, Error_unsupportedOperation(1, "CAFile_read() decompression not supported yet"))

	if(header.flags & ECAFlags_UseSHA256)				//TODO: SHA256
		retError(clean, Error_unsupportedOperation(3, "CAFile_read() SHA256 not supported yet"))

	if((header.type & 0xF) >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 4, "CAFile_read() encryption type unsupported"))

	//Ensure encryption key isn't provided if we're not encrypting

	if(encryptionKey && !(header.type & 0xF))
		retError(clean, Error_invalidOperation(3, "CAFile_read() encryption key provided but encryption isn't used"))

	if(!encryptionKey && (header.type & 0xF))
		retError(clean, Error_unauthorized(0, "CAFile_read() encryption key is required if encryption is used"))

	//Validate file and dir count

	U32 fileCount = 0;
	U64 counterSiz = header.flags & ECAFlags_FilesCountLong ? 4 : 2;
	gotoIfError3(clean, Stream_read(stream, Buffer_createRef(&fileCount, counterSiz), off, 0, 0, false, e_rr))
	off += counterSiz;

	U16 dirCount = 0;
	counterSiz = header.flags & ECAFlags_DirectoriesCountLong ? 2 : 1;
	gotoIfError3(clean, Stream_read(stream, Buffer_createRef(&dirCount, counterSiz), off, 0, 0, false, e_rr))
	off += counterSiz;

	if(dirCount >= (header.flags & ECAFlags_DirectoriesCountLong ? U16_MAX : U8_MAX))
		retError(clean, Error_invalidParameter(0, 7, "CAFile_read() directory count can't be the max bit value"))

	if(fileCount >= (header.flags & ECAFlags_FilesCountLong ? U32_MAX : U16_MAX))
		retError(clean, Error_invalidParameter(0, 8, "CAFile_read() file count can't be the max bit value"))

	//Validate extended data

	CAExtraInfo extra = (CAExtraInfo) { 0 };

	if(header.flags & ECAFlags_HasExtendedData) {
		gotoIfError3(clean, Stream_read(stream, Buffer_createRef(&extra, sizeof(extra)), off, 0, 0, false, e_rr))
		off += sizeof(extra) + extra.headerExtensionSize;
	}

	//Check for encryption

	static const U64 chunkLengths[4] = { 262144, 1048576, 10485760, 104857600 };

	U64 chunkLength = chunkLengths[(header.flags & ECAFlags_ChunkMask) >> ECAFlags_ChunkShift];

	if(!header.type)
		chunkLength = stream->handle.fileSize - off;

	U64 chunks = (stream->handle.fileSize - off + chunkLength - 1) / chunkLength;

	if ((header.type & 0xF) == EXXEncryptionType_AES256GCM) {

		U64 tagStart = off;

		if(chunks > 1)
			off += chunks * sizeof(I32x4);

		U64 headerLen = off;

		if(chunks * chunkLength > (4 * GIBI - 3) * sizeof(I32x4))
			retError(clean, Error_invalidParameter(
				0, 9, "CAFile_read() file is potentially insecure due to IV reuse (64GB encryption limit)"
			))

		//Input additional data as input as well as tags 

		gotoIfError2(clean, Buffer_createUninitializedBytes(
			chunks > 1 ? off + sizeof(I32x4) + 12 : stream->handle.fileSize, alloc, &tmpData1
		))

		gotoIfError3(clean, Stream_read(stream, tmpData1, 0, 0, 0, false, e_rr))

		Buffer filePtr = Buffer_createRefConst(tmpData1.ptr + off, Buffer_length(tmpData1) - off);
		gotoIfError2(clean, Buffer_consume(&filePtr, &iv, 12))
		gotoIfError2(clean, Buffer_consume(&filePtr, &tag, 16))

		off += 12 + 16;
		U64 len = stream->handle.fileSize - off;

		if(chunks > 1)
			filePtr = Buffer_createNull();		//Additional data only

		gotoIfError2(clean, Buffer_decrypt(
			filePtr,
			Buffer_createRefConst(tmpData1.ptr, headerLen),
			EBufferEncryptionType_AES256GCM,
			encryptionKey,
			tag,
			iv
		))

		if(chunks == 1)
			gotoIfError3(clean, Stream_openMemoryRead(&tmpData1, off, 0, fileData, e_rr))

		else {

			gotoIfError2(clean, Buffer_createCopy(
				Buffer_createRefConst(tmpData1.ptr + tagStart, chunks * sizeof(I32x4)), alloc, &tmpData
			))

			gotoIfError3(clean, Stream_openDecryptionStream(stream, off, 0, &tmpData, iv, len, chunkLength, fileData, e_rr))
		}

		if(chunks > 1)			//Store iv so each block can be decrypted individually
			Buffer_free(&tmpData1, alloc);		//Was used for AD only

		iv = I32x4_zero();
		tag = I32x4_zero();
	}

	else gotoIfError3(clean, Stream_openStream(stream, off, 0, fileData, e_rr))

	gotoIfError3(clean, DLFile_read(fileData, NULL, true, alloc, &fileNames, e_rr))
	off = fileNames.readLength;

	//Validate DLFile
	//File name validation is done when the entries are inserted into the Archive

	if (
		fileNames.settings.dataType != EDLDataType_Ascii ||
		fileNames.settings.compressionType ||
		fileNames.settings.encryptionType ||
		fileNames.settings.flags
	)
		retError(clean, Error_invalidOperation(
			0, "CAFile_read() embedded oiDL needs to be ascii without compression/encryption or flags"
		))

	if(DLFile_entryCount(fileNames) != (U64)fileCount + dirCount)
		retError(clean, Error_invalidState(0, "CAFile_read() embedded oiDL has mismatching name count with file count"))

	//Ensure we have enough allocated for all files and directories

	EXXDataSizeType sizeType = (header.flags >> ECAFlags_FileSizeType_Shift) & ECAFlags_FileSizeType_Mask;

	U64 dirStride = header.flags & ECAFlags_DirectoriesCountLong ? 2 : 1;

	U16 rootDir = dirStride == 2 ? U16_MAX : U8_MAX;

	U64 folderStride = (U64) dirStride + extra.directoryExtensionSize;
	U64 fileStride = (U64) dirStride + SIZE_BYTE_TYPE[sizeType] + extra.fileExtensionSize;

	if (header.flags & ECAFlags_FilesHaveDate)
		fileStride += header.flags & ECAFlags_FilesHaveExtendedDate ? sizeof(U64) : sizeof(U16) * 2;

	//Now we can add dir to the archive

	for (U64 i = 0; i < dirCount; ++i) {

		U16 parent = 0;
		gotoIfError3(clean, Stream_read(fileData, Buffer_createRef(&parent, dirStride), off, 0, 0, false, e_rr))

		if(fileData->handle.fileSize < off + folderStride)
			retError(clean, Error_invalidParameter(0, 0, "CAFile_read() extended folder out of bounds"))

		off += folderStride;

		CharString name = fileNames.entryStrings.ptr[i];

		if(!CharString_isValidFileName(name))
			retError(clean, Error_invalidParameter(0, 0, "CAFile_read() directory has invalid name"))

		gotoIfError2(clean, CharString_createCopy(name, alloc, &tmpPath))

		if (parent != rootDir) {

			if (parent >= i)
				retError(clean, Error_invalidOperation(1, "CAFile_read() parent directory index of folder out of bounds"))

			CharString parentName = archive.entries.ptr[parent].path;

			gotoIfError2(clean, CharString_insert(&tmpPath, '/', 0, alloc))
			gotoIfError2(clean, CharString_insertString(&tmpPath, parentName, 0, alloc))
		}

		gotoIfError3(clean, Archive_addDirectory(&archive, tmpPath, alloc, e_rr))

		tmpPath = CharString_createNull();
	}

	//Add file

	U64 fileSize = (U64)fileCount * fileStride;
	U64 fileOff = off + fileSize;

	for (U64 i = 0; i < fileCount; ++i) {

		if(fileData->handle.fileSize < off + fileStride)
			retError(clean, Error_invalidParameter(0, 0, "CAFile_read() file out of bounds"))

		CharString name = fileNames.entryStrings.ptr[(U64)i + dirCount];

		if(!CharString_isValidFileName(name))
			retError(clean, Error_invalidParameter(0, 1, "CAFile_read() file has invalid name"))

		gotoIfError2(clean, CharString_createCopy(name, alloc, &tmpPath))

		//Load parent

		U16 parent = 0;
		gotoIfError3(clean, Stream_read(fileData, Buffer_createRef(&parent, dirStride), off, 0, 0, false, e_rr))
		off += dirStride;

		if (parent != rootDir) {

			if (parent >= dirCount)
				retError(clean, Error_invalidOperation(2, "CAFile_read() parent directory index of file out of bounds"))

			CharString parentName = archive.entries.ptr[parent].path;

			gotoIfError2(clean, CharString_insert(&tmpPath, '/', 0, alloc))
			gotoIfError2(clean, CharString_insertString(&tmpPath, parentName, 0, alloc))
		}

		//Load timestamp

		Ns timestamp = 0;

		if (header.flags & ECAFlags_FilesHaveDate) {

			if(header.flags & ECAFlags_FilesHaveExtendedDate) {
				gotoIfError3(clean, Stream_read(fileData, Buffer_createRef(&timestamp, sizeof(Ns)), off, 0, 0, false, e_rr))
				off += sizeof(Ns);
			}

			else {

				U16 timeStampShort[2];

				gotoIfError3(clean, Stream_read(
					fileData, Buffer_createRef(timeStampShort, sizeof(timeStampShort)), off, 0, 0, false, e_rr
				))

				timestamp = CAFile_loadDate(timeStampShort[1], timeStampShort[0]);
				off += sizeof(timeStampShort);

				if (timestamp == U64_MAX)
					timestamp = 0;
			}
		}

		//Grab data of file.
		//This introduces a copy because Archive needs a separate alloc per file

		U64 bufferSize = 0;

		gotoIfError3(clean, Stream_read(
			fileData, Buffer_createRef(&bufferSize, (U64)1 << sizeType), off, 0, 0, false, e_rr
		))

		off += (U64)1 << sizeType;

		if(fileOff + bufferSize > fileData->handle.fileSize)
			retError(clean, Error_invalidParameter(0, 0, "CAFile_read() file buffer out of bounds"))

		gotoIfError3(clean, Archive_addFile(&archive, tmpPath, fileOff, bufferSize, timestamp, alloc, e_rr))
		CharString_free(&tmpPath, alloc);

		fileOff += bufferSize;
	}

	if(fileOff != fileData->handle.fileSize)
		retError(clean, Error_invalidState(0, "CAFile_read() had leftover data after oiCA, this is illegal"))

	caFile->archive = archive;
	archive = (Archive) { 0 };
	allocate = true;

	caFile->settings = (CASettings) {
		.compressionType = (EXXCompressionType)(header.type >> 4),
		.encryptionType = (EXXEncryptionType)(header.type & 0xF),
		.flags = (
			header.flags & ECAFlags_UseSHA256 ? ECASettingsFlags_UseSHA256 :
			ECASettingsFlags_None
		) | (
			header.flags & ECAFlags_FilesHaveExtendedDate ?
			ECASettingsFlags_IncludeFullDate | ECASettingsFlags_IncludeDate :
			ECASettingsFlags_None
		) | (
			header.flags & ECAFlags_FilesHaveDate ?
			ECASettingsFlags_IncludeDate :
			ECASettingsFlags_None
		)
	};

	//Not copying encryption key, because you probably don't want to store it anywhere.
	//And you already have it.

clean:

	iv = I32x4_zero();
	tag = I32x4_zero();

	if(!s_uccess && allocate)
		CAFile_free(caFile, alloc);

	Stream_free(readStream, alloc);
	Buffer_free(&tmpData, alloc);
	Buffer_free(&tmpData1, alloc);
	CharString_free(&tmpPath, alloc);
	Archive_free(&archive, alloc);
	DLFile_free(&fileNames, alloc);
	return s_uccess;
}
