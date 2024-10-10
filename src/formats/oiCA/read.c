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

Ns CAFile_loadDate(U16 time, U16 date) {
	return Time_date(
		1980 + (date >> 9), 1 + ((date >> 5) & 0xF), date & 0x1F,
		time >> 11, (time >> 5) & 0x3F, (time & 0x1F) << 1, 0, false
	);
}

Bool CAFile_read(Buffer file, const U32 encryptionKey[8], Allocator alloc, CAFile *caFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocate = false;

	if (!caFile)
		retError(clean, Error_nullPointer(2, "CAFile_read()::caFile is required"))

	if (caFile->archive.entries.ptr)
		retError(clean, Error_invalidParameter(2, 0, "CAFile_read()::caFile isn't empty, could indicate memleak"))

	if (Buffer_length(file) < sizeof(CAHeader))
		retError(clean, Error_outOfBounds(
			0, sizeof(CAHeader), Buffer_length(file), "CAFile_read()::file doesn't contain header"
		))

	Buffer filePtr = Buffer_createRefFromBuffer(file, Buffer_isConstRef(file));

	Buffer tmpData = Buffer_createNull();
	DLFile fileNames = (DLFile) { 0 };
	Archive archive = (Archive) { 0 };
	CharString tmpPath = CharString_createNull();

	gotoIfError3(clean, Archive_create(alloc, &archive, e_rr))

	//Validate header

	CAHeader header;
	gotoIfError2(clean, Buffer_consume(&filePtr, &header, sizeof(header)))

	if(header.magicNumber != CAHeader_MAGIC)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_read()::file contained invalid header"))

	if(header.version != ECAVersion_V1_0)
		retError(clean, Error_invalidParameter(0, 1, "CAFile_read()::file header doesn't have correct version"))

	if(header.flags & (ECAFlags_UseAESChunksA | ECAFlags_UseAESChunksB))		//TODO: AES chunks
		retError(clean, Error_unsupportedOperation(0, "CAFile_read() AES chunks not supported yet"))

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
	gotoIfError2(clean, Buffer_consume(&filePtr, &fileCount, header.flags & ECAFlags_FilesCountLong ? 4 : 2))

	U16 dirCount = 0;
	gotoIfError2(clean, Buffer_consume(&filePtr, &dirCount, header.flags & ECAFlags_DirectoriesCountLong ? 2 : 1))

	if(dirCount >= (header.flags & ECAFlags_DirectoriesCountLong ? U16_MAX : U8_MAX))
		retError(clean, Error_invalidParameter(0, 7, "CAFile_read() directory count can't be the max bit value"))

	if(fileCount >= (header.flags & ECAFlags_FilesCountLong ? U32_MAX : U16_MAX))
		retError(clean, Error_invalidParameter(0, 8, "CAFile_read() file count can't be the max bit value"))

	//Validate extended data

	CAExtraInfo extra = (CAExtraInfo) { 0 };

	if(header.flags & ECAFlags_HasExtendedData) {
		gotoIfError2(clean, Buffer_consume(&filePtr, &extra, sizeof(extra)))
		gotoIfError2(clean, Buffer_consume(&filePtr, NULL, extra.headerExtensionSize))
	}

	//Check for encryption

	I32x4 iv = I32x4_zero(), tag = I32x4_zero();

	if ((header.type & 0xF) == EXXEncryptionType_AES256GCM) {

		U64 headerLen = filePtr.ptr - file.ptr;

		gotoIfError2(clean, Buffer_consume(&filePtr, &iv, 12))
		gotoIfError2(clean, Buffer_consume(&filePtr, &tag, 16))

		gotoIfError2(clean, Buffer_decrypt(
			filePtr,
			Buffer_createRefConst(file.ptr, headerLen),
			EBufferEncryptionType_AES256GCM,
			encryptionKey,
			tag,
			iv
		))
	}

	gotoIfError3(clean, DLFile_read(filePtr, NULL, true, alloc, &fileNames, e_rr))
	gotoIfError2(clean, Buffer_consume(&filePtr, NULL, fileNames.readLength))

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

	U64 fileSize = (U64)fileCount * fileStride;
	U64 folderSize = (U64)dirCount * folderStride;

	if (Buffer_length(filePtr) < fileSize + folderSize)
		retError(clean, Error_outOfBounds(
			0, fileSize + folderSize, Buffer_length(filePtr), "CAFile_read() files out of bounds"
		))

	//Now we can add dir to the archive

	for (U64 i = 0; i < dirCount; ++i) {

		const U8 *diri = filePtr.ptr + folderStride * i;

		CharString name = fileNames.entryStrings.ptr[i];

		if(!CharString_isValidFileName(name))
			retError(clean, Error_invalidParameter(0, 0, "CAFile_read() directory has invalid name"))

		gotoIfError2(clean, CharString_createCopy(name, alloc, &tmpPath))

		U16 parent = dirStride == 2 ? *(const U16*)diri : *diri;

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

	const U8 *fileIt = filePtr.ptr + folderSize;
	gotoIfError2(clean, Buffer_offset(&filePtr, fileSize + folderSize))

	for (U64 i = 0; i < fileCount; ++i) {

		const U8 *filei = fileIt + fileStride * i;

		CharString name = fileNames.entryStrings.ptr[(U64)i + dirCount];

		if(!CharString_isValidFileName(name))
			retError(clean, Error_invalidParameter(0, 1, "CAFile_read() file has invalid name"))

		gotoIfError2(clean, CharString_createCopy(name, alloc, &tmpPath))

		//Load parent

		U16 parent = dirStride == 2 ? *(const U16*)filei : *filei;
		filei += dirStride;

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
				timestamp = *(Ns*)filei;
				filei += sizeof(Ns);
			}

			else {

				timestamp = CAFile_loadDate(*(const U16*)(filei + 1), *(const U16*)filei);
				filei += sizeof(U16) * 2;

				if (timestamp == U64_MAX)
					timestamp = 0;
			}
		}

		//Grab data of file.
		//This introduces a copy because Archive needs a separate alloc per file

		U64 bufferSize = Buffer_forceReadSizeType(filei, sizeType);

		gotoIfError2(clean, Buffer_createUninitializedBytes(bufferSize, alloc, &tmpData))

		const U8 *dataPtr = filePtr.ptr;
		gotoIfError2(clean, Buffer_offset(&filePtr, bufferSize))

		Buffer_copy(tmpData, Buffer_createRefConst(dataPtr, bufferSize));

		//Move path and data to file

		gotoIfError3(clean, Archive_addFile(&archive, tmpPath, &tmpData, timestamp, alloc, e_rr))
		CharString_free(&tmpPath, alloc);
	}

	if(Buffer_length(filePtr))
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

	//Not copying encryption key, because you probably don't want to store it.
	//And you already have it.

clean:

	if(!s_uccess && allocate)
		CAFile_free(caFile, alloc);

	Buffer_free(&tmpData, alloc);
	CharString_free(&tmpPath, alloc);
	Archive_free(&archive, alloc);
	DLFile_free(&fileNames, alloc);
	return s_uccess;
}
