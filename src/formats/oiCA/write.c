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
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/time.h"
#include "types/math.h"

Bool CAFile_storeDate(Ns ns, U16 *time, U16 *date) {

	if(!time || !date)
		return false;

	U16 year; U8 month, day, hour, minute, second;
	const Bool b = Time_getDate(ns, &year, &month, &day, &hour, &minute, &second, NULL, false);

	if(!b || year < 1980 || year > (1980 + 0x7F))
		return false;

	*time = (second >> 1) | ((U16)minute << 5) | ((U16)hour << 11);
	*date = day | ((U16)month << 5) | ((U16)(year - 1980) << 11);
	return true;
}

ECompareResult sortParentCountAndFileNames(CharString *a, CharString *b) {

	const U64 foldersA = CharString_countAllSensitive(*a, '/', 0);
	const U64 foldersB = CharString_countAllSensitive(*b, '/', 0);

	//We wanna sort on folder count first
	//This ensures the root dirs are always at [0,N] and their children at [N, N+M], etc.

	if (foldersA < foldersB)
		return ECompareResult_Lt;

	if (foldersA > foldersB)
		return ECompareResult_Gt;

	return CharString_compareInsensitive(*a, *b);
}

//We don't support any compression yet, but should be trivial to add once Buffer_compress/Buffer_decompress is supported.

Bool CAFile_write(CAFile caFile, Allocator alloc, Buffer *result, Error *e_rr) {

	ListCharString directories = { 0 };
	ListCharString files = { 0 };
	DLFile dlFile = (DLFile) { 0 };
	Buffer dlFileBuffer = Buffer_createNull();
	Buffer outputBuffer = Buffer_createNull();
	Bool s_uccess = true;

	if(!result)
		retError(clean, Error_nullPointer(2, "CAFile_write()::result is required"))

	if(result->ptr)
		retError(clean, Error_invalidParameter(2, 0, "CAFile_write()::result isn't empty, might indicate memleak"))

	gotoIfError2(clean, ListCharString_reserve(&directories, 128, alloc))
	gotoIfError2(clean, ListCharString_reserve(&files, 128, alloc))

	//Validate CAFile and calculate files and folders

	U64 outputSize = 0;			//Excluding header, hash and DLFile

	U32 hash[8] = { 0 };

	U64 realHeaderSize = sizeof(CAHeader) + (
		caFile.settings.compressionType ? (
			caFile.settings.flags & ECASettingsFlags_UseSHA256 ? sizeof(hash) : sizeof(hash[0])
		) : 0
	);

	U64 realHeaderSizeExEnc = realHeaderSize;
	realHeaderSize += (caFile.settings.encryptionType ? sizeof(I32x4) + 12 : 0);

	U64 baseFileHeader = (						//FileInfo, parent directory elsewhere
		(
			(caFile.settings.flags & ECASettingsFlags_IncludeFullDate) ||
			(caFile.settings.flags & ECASettingsFlags_IncludeDate)
		) ? (caFile.settings.flags & ECASettingsFlags_IncludeFullDate ? sizeof(Ns) : sizeof(U16) * 2) : 0
	);

	U64 biggestFileSize = 0;

	for (U64 i = 0; i < caFile.archive.entries.length; ++i) {

		ArchiveEntry entry = caFile.archive.entries.ptr[i];

		//Push back directory names. Size is not known until later.

		if (entry.type == EFileType_Folder) {

			gotoIfError2(clean, ListCharString_pushBack(&directories, entry.path, alloc))

			if(directories.length >= U16_MAX)
				retError(clean, Error_outOfBounds(
					0, 0xFFFF, U16_MAX - 1, "CAFile_write() directories are limited to U16_MAX"
				))

			continue;
		}

		//Push back file names and calculate output buffer

		gotoIfError2(clean, ListCharString_pushBack(&files, entry.path, alloc))

		if(files.length >= U32_MAX)
			retError(clean, Error_outOfBounds(0, files.length, U32_MAX, "CAFile_write() files are limited to U32_MAX"))

		if(outputSize + Buffer_length(entry.data) < outputSize)
			retError(clean, Error_overflow(
				0, outputSize + Buffer_length(entry.data), outputSize, "CAFile_write() overflow"
			))

		outputSize += Buffer_length(entry.data);
		biggestFileSize = U64_max(biggestFileSize, Buffer_length(entry.data));
	}

	U64 dirRefSize  = directories.length <= 254 ? 1 : 2;
	U64 fileRefSize = files.length <= 65534 ? 2 : 4;

	EXXDataSizeType sizeType = biggestFileSize <= U8_MAX ? EXXDataSizeType_U8 : (
		biggestFileSize <= U16_MAX ? EXXDataSizeType_U16 : (
			biggestFileSize <= U32_MAX ? EXXDataSizeType_U32 : EXXDataSizeType_U64
		));

	baseFileHeader += SIZE_BYTE_TYPE[sizeType];
	baseFileHeader += dirRefSize;

	//Add directories, files and directory/file count.

	realHeaderSizeExEnc += dirRefSize + fileRefSize;
	realHeaderSize += dirRefSize + fileRefSize;

	U64 fileObjLen = dirRefSize * directories.length + baseFileHeader * files.length;

	if(outputSize + fileObjLen < outputSize)
		retError(clean, Error_overflow(0, outputSize + fileObjLen, outputSize, "CAFile_write() overflow (2)"))

	outputSize += fileObjLen;

	//Sort directories and files to ensure our file ids are correct,
	//This is because we have full directory names, so it automatically resolves
	// in a way where we know where our children are (in a linear way).
	//This sort is different than just sortString. It sorts on parent count first and then alphabetically.
	//This allows us to not have to reorder the files down the road since they're already sorted.

	if(
		!ListCharString_sortCustom(directories, (CompareFunction) sortParentCountAndFileNames) ||
		!ListCharString_sortCustom(files, (CompareFunction) sortParentCountAndFileNames)
	)
		retError(clean, Error_invalidOperation(0, "CAFile_write() couldn't sort files and/or directories"))

	//Allocate and generate DLFile

	DLSettings dlSettings = (DLSettings) { .dataType = EDLDataType_Ascii };

	gotoIfError3(clean, DLFile_create(dlSettings, alloc, &dlFile, e_rr))

	for(U64 i = 0; i < directories.length; ++i) {

		CharString dir = directories.ptr[i];
		CharString dirName = CharString_createNull();

		if(!CharString_cutBeforeLastSensitive(dir, '/', &dirName))
			dirName = CharString_createRefStrConst(dir);

		gotoIfError3(clean, DLFile_addEntryAscii(&dlFile, dirName, alloc, e_rr))
	}

	for(U64 i = 0; i < files.length; ++i) {

		CharString file = files.ptr[i];
		CharString fileName = CharString_createNull();

		if(!CharString_cutBeforeLastSensitive(file, '/', &fileName))
			fileName = CharString_createRefStrConst(file);

		gotoIfError3(clean, DLFile_addEntryAscii(&dlFile, fileName, alloc, e_rr))
	}

	//Complete DLFile as buffer

	dlFile.settings.flags |= EDLSettingsFlags_HideMagicNumber;				//Small optimization

	gotoIfError3(clean, DLFile_write(dlFile, alloc, &dlFileBuffer, e_rr))

	DLFile_free(&dlFile, alloc);

	//Build up final output buffer
	//DLFile
	//CADirectory[]
	//CAFileObject[]
	//U8[sum(file[i].data)]

	if (outputSize + Buffer_length(dlFileBuffer) < outputSize)
		retError(clean, Error_overflow(
			0, outputSize + Buffer_length(dlFileBuffer), outputSize, "CAFile_write() overflow (3)"
		))

	outputSize += Buffer_length(dlFileBuffer);

	if (outputSize + realHeaderSize < outputSize)
		retError(clean, Error_overflow(
			0, outputSize + realHeaderSize, outputSize, "CAFile_write() overflow (4)"
		))

	outputSize += realHeaderSize;		//Reserve space for header (even though this won't be compressed)

	gotoIfError2(clean, Buffer_createUninitializedBytes(outputSize, alloc, &outputBuffer))

	//Append DLFile

	Buffer outputBufferIt = Buffer_createRef(
		(U8*)outputBuffer.ptr + realHeaderSize, Buffer_length(outputBuffer) - realHeaderSize
	);

	gotoIfError2(clean, Buffer_appendBuffer(&outputBufferIt, dlFileBuffer))

	Buffer_free(&dlFileBuffer, alloc);

	//Append CADirectory[]

	U8 *dirPtr = (U8*)outputBufferIt.ptr;

	for (U16 i = 0; i < (U16) directories.length; ++i) {

		CharString dir = directories.ptr[i];
		U64 it = CharString_findLastSensitive(dir, '/', 0, 0);

		U16 parent = U16_MAX;

		//Add to parent directory

		if (it != U64_MAX) {

			//We need to find the real parent.
			//Since we're sorted alphabetically, we are able to look back from our current directory
			//If we encounter something that doesn't start with our basePath/ then we know we are missing this dir
			//And this means we somehow messed with the input data

			CharString baseDir = CharString_createNull(), realParentDir = CharString_createNull();

			if (
				!CharString_cut(dir, 0, it, &realParentDir) ||
				!CharString_cut(dir, 0, it + 1, &baseDir)
			)
				retError(clean, Error_invalidOperation(0, "CAFile_write() couldn't split directory name"))

			for(U64 j = i - 1; j != U64_MAX; --j)

				if (CharString_equalsStringInsensitive(directories.ptr[j], realParentDir)) {
					parent = (U16) j;
					break;
				}

			if(parent == U16_MAX)
				retError(clean, Error_invalidState(0, "CAFile_write() couldn't find parent directory of folder"))
		}

		//Add directory

		if (dirRefSize == 2)
			((U16*)dirPtr)[i] = parent;

		else dirPtr[i] = (U8) parent;
	}

	//Append CAFileObject[]

	U8 *fileObjectPtr = dirPtr + directories.length * dirRefSize;
	U8 *fileDataPtrIt = fileObjectPtr + baseFileHeader * files.length;

	for (U32 i = 0; i < (U32) files.length; ++i) {

		CharString file = files.ptr[i];
		U64 it = CharString_findLastSensitive(file, '/', 0, 0);

		U16 parent = U16_MAX;

		//Add to parent directory

		if (it != U64_MAX) {

			//We need to find the real parent.
			//Since we're sorted alphabetically, we are able to look back from our current directory
			//If we encounter something that doesn't start with our basePath then we know we are missing this dir
			//And this means we somehow messed with the input data

			CharString baseDir = CharString_createNull(), realParentDir = CharString_createNull();

			if (
				!CharString_cut(file, 0, it, &realParentDir) ||
				!CharString_cut(file, 0, it + 1, &baseDir)
			)
				retError(clean, Error_invalidOperation(0, "CAFile_write() couldn't split file name"))

			for(U64 j = directories.length - 1; j != U64_MAX; --j)

				if (CharString_equalsStringInsensitive(directories.ptr[j], realParentDir)) {
					parent = (U16) j;
					break;
				}

			if(parent == U16_MAX)
				retError(clean, Error_invalidState(1, "CAFile_write() couldn't find parent directory of file"))
		}

		//Find corresponding file with name

		ArchiveEntry *entry = NULL;

		for(U64 j = 0; j < caFile.archive.entries.length; ++j)
			if (CharString_equalsStringSensitive(caFile.archive.entries.ptr[j].path, file)) {
				entry = caFile.archive.entries.ptrNonConst + j;
				break;
			}

		if (!entry)
			retError(clean, Error_invalidState(2, "CAFile_write() couldn't find entry by path"))

		//Add file

		U8 *filePtr = fileObjectPtr + baseFileHeader * i;

		if (dirRefSize == 2)
			*((U16*)filePtr) = parent;

		else *filePtr = (U8) parent;

		filePtr += dirRefSize;

		if (
			(caFile.settings.flags & ECASettingsFlags_IncludeFullDate) ||
			(caFile.settings.flags & ECASettingsFlags_IncludeDate)
		) {

			if(caFile.settings.flags & ECASettingsFlags_IncludeFullDate) {
				*(Ns*)filePtr = entry->timestamp;
				filePtr += sizeof(Ns);
			}

			else {

				if (!CAFile_storeDate(entry->timestamp, (U16*)filePtr + 1, (U16*)filePtr))
					retError(clean, Error_invalidState(
						1, "CAFile_write() couldn't store file date, please use full date"
					))

				filePtr += sizeof(U16) * 2;
			}
		}

		Buffer_forceWriteSizeType(filePtr, sizeType, Buffer_length(entry->data));

		//Append file data

		Buffer_copy(Buffer_createRef(fileDataPtrIt, Buffer_length(entry->data)), entry->data);
		fileDataPtrIt += Buffer_length(entry->data);
	}

	//We're done with processing files and folders

	U16 dirCount = (U16) directories.length;
	U32 fileCount = (U32) files.length;

	ListCharString_free(&files, alloc);
	ListCharString_free(&directories, alloc);

	//Generate header

	U8 *headerIt = (U8*)outputBuffer.ptr;

	*((CAHeader*)headerIt) = (CAHeader) {

		.magicNumber = CAHeader_MAGIC,

		.version = 0,		//1.0

		.flags = (U8) (

			(
				caFile.settings.compressionType ? (
					caFile.settings.flags & ECASettingsFlags_UseSHA256 ? ECAFlags_UseSHA256 :
					ECAFlags_None
				) :
				ECAFlags_None
			) |

			(caFile.settings.flags & ECASettingsFlags_IncludeDate ? ECAFlags_FilesHaveDate : ECAFlags_None) |

			(
				caFile.settings.flags & ECASettingsFlags_IncludeFullDate ?
				ECAFlags_FilesHaveDate | ECAFlags_FilesHaveExtendedDate : ECAFlags_None
			) |

			(sizeType << ECAFlags_FileSizeType_Shift)
		),

		.type = (U8)((caFile.settings.compressionType << 4) | caFile.settings.encryptionType)
	};

	headerIt += sizeof(CAHeader);

	//Append file and dir count

	if (fileRefSize == 4)
		*((U32*)headerIt) = fileCount;

	else *((U16*)headerIt) = (U16) fileCount;

	headerIt += fileRefSize;

	if (dirRefSize == 2)
		*((U16*)headerIt) = dirCount;

	else *headerIt = (U8) dirCount;

	headerIt += dirRefSize;

	//Hash

	/*if (caFile.settings.compressionType) {

		if (caFile.settings.flags & ECASettingsFlags_UseSHA256)
			Buffer_sha256(outputBuffer, hash);

		else hash[0] = Buffer_crc32c(outputBuffer);
	}

	//Store hash in header before encryption or finish

	if (caFile.settings.compressionType)
		Buffer_copy(
			Buffer_createRef(headerIt, sizeof(hash)),
			Buffer_createRefConst(hash, sizeof(hash))
		);*/

	//Compress

	/*if (caFile.settings.compressionType != EXXCompressionType_None) {				TODO:

		Buffer toCompress = Buffer_createRefConst(
			outputBuffer.ptr + realHeaderSize,
			Buffer_length(outputBuffer) - realHeaderSize
		);

		gotoIfError(clean, Buffer_compress(toCompress, BufferCompressionType_Brotli11, alloc, &compressedOutput))

		Buffer_free(&outputBuffer, alloc);
	}*/

	//Encrypt

	if (caFile.settings.encryptionType != EXXEncryptionType_None) {

		//TODO: Support chunks
		//		Select no chunks if <40MiB
		//		Select 10MiB if at least 4 threads can be kept busy.
		//		Select 50MiB if ^ and utilization is about the same (e.g. 24 threads doing 10MiB would need 1.2GiB).
		//		Select 100MiB if ^ (24 threads would need 2.4GiB).

		U32 key[8] = { 0 };

		Bool b = Buffer_eq(
			Buffer_createRefConst(key, sizeof(key)),
			Buffer_createRefConst(caFile.settings.encryptionKey, sizeof(key))
		);

		Buffer toEncrypt = Buffer_createRef(
			(U8*)outputBuffer.ptr + realHeaderSize, Buffer_length(outputBuffer) - realHeaderSize
		);

		Buffer realHeader = Buffer_createRefConst(outputBuffer.ptr, realHeaderSizeExEnc);

		I32x4 iv = I32x4_zero();		//Outputs
		I32x4 tag = I32x4_zero();

		gotoIfError2(clean, Buffer_encrypt(

			toEncrypt,
			realHeader,

			EBufferEncryptionType_AES256GCM,

			EBufferEncryptionFlags_GenerateIv | (b ? EBufferEncryptionFlags_GenerateKey : EBufferEncryptionFlags_None),
			b ? NULL : caFile.settings.encryptionKey,

			&iv,
			&tag
		))

		for(U64 i = 0; i < 3; ++i)
			((I32*)headerIt)[i] = ((const I32*)&iv)[i];

		for(U64 i = 0; i < 4; ++i)
			((I32*)headerIt)[3 + i] = ((const I32*)&tag)[i];
	}

	//Prepend header and hash

	*result = outputBuffer;
	outputBuffer = Buffer_createNull();

clean:
	Buffer_free(&outputBuffer, alloc);
	Buffer_free(&dlFileBuffer, alloc);
	DLFile_free(&dlFile, alloc);
	ListCharString_free(&files, alloc);
	ListCharString_free(&directories, alloc);
	return s_uccess;
}