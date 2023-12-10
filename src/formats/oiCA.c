/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "types/file.h"
#include "types/list.h"
#include "types/time.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"

static const U8 CAHeader_V1_0  = 0;

//Helper functions

inline Ns CAFile_loadDate(U16 time, U16 date) {
	return Time_date(
		1980 + (date >> 9), 1 + ((date >> 5) & 0xF), date & 0x1F, 
		time >> 11, (time >> 5) & 0x3F, (time & 0x1F) << 1, 0, false
	);
}

inline Bool CAFile_storeDate(Ns ns, U16 *time, U16 *date) {

	if(!time || !date)
		return false;

	U16 year; U8 month, day, hour, minute, second;
	Bool b = Time_getDate(ns, &year, &month, &day, &hour, &minute, &second, NULL, false);

	if(!b || year < 1980 || year > (1980 + 0x7F))
		return false;

	*time = (second >> 1) | ((U16)minute << 5) | ((U16)hour << 11);
	*date = day | ((U16)month << 5) | ((U16)(year - 1980) << 11);
	return true;
}

//

Error CAFile_create(CASettings settings, Archive archive, CAFile *caFile) {

	if(!caFile)
		return Error_nullPointer(0, "CAFile_create()::caFile is required");

	if(!archive.entries.ptr)
		return Error_nullPointer(1, "CAFile_create()::archive is empty");

	if(caFile->archive.entries.ptr)
		return Error_invalidParameter(2, 0, "CAFile_create()::caFile wasn't empty, could indicate memleak");

	if(settings.compressionType >= EXXCompressionType_Count)
		return Error_invalidParameter(0, 0, "CAFile_create()::settings.compressionType is invalid");

	if(settings.compressionType > EXXCompressionType_None)
		return Error_unsupportedOperation(0, "CAFile_create() compression not supported yet");			//TODO:

	if(settings.encryptionType >= EXXEncryptionType_Count)
		return Error_invalidParameter(0, 1, "CAFile_create()::settings.encryptionType is invalid");

	if(settings.flags & ECASettingsFlags_Invalid)
		return Error_invalidParameter(0, 2, "CAFile_create()::flags is invalid");

	caFile->archive = archive;
	caFile->settings = settings;
	return Error_none();
}

Bool CAFile_free(CAFile *caFile, Allocator alloc) {

	if(!caFile || !caFile->archive.entries.ptr)
		return true;

	Bool b = Archive_free(&caFile->archive, alloc);
	*caFile = (CAFile) { 0 };
	return b;
}

ECompareResult sortParentCountAndFileNames(CharString *a, CharString *b) {

	U64 foldersA = CharString_countAll(*a, '/', EStringCase_Sensitive);
	U64 foldersB = CharString_countAll(*b, '/', EStringCase_Sensitive);

	//We wanna sort on folder count first
	//This ensures the root dirs are always at [0,N] and their children at [N, N+M], etc.

	if (foldersA < foldersB)
		return ECompareResult_Lt;

	if (foldersA > foldersB)
		return ECompareResult_Gt;

	return CharString_compare(*a, *b, EStringCase_Insensitive);
}

//We don't support any compression yet, but should be trivial to add once Buffer_compress/Buffer_decompress is supported.

Error CAFile_write(CAFile caFile, Allocator alloc, Buffer *result) {

	Error err = Error_none();
	List directories = List_createEmpty(sizeof(CharString));
	List files = List_createEmpty(sizeof(CharString));
	DLFile dlFile = (DLFile) { 0 };
	Buffer dlFileBuffer = Buffer_createNull();
	Buffer outputBuffer = Buffer_createNull();

	if(!result)
		_gotoIfError(clean, Error_nullPointer(2, "CAFile_write()::result is required"));

	if(result->ptr)
		_gotoIfError(clean, Error_invalidParameter(2, 0, "CAFile_write()::result isn't empty, might indicate memleak"));

	_gotoIfError(clean, List_reserve(&directories, 128, alloc));
	_gotoIfError(clean, List_reserve(&files, 128, alloc));

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

		ArchiveEntry entry = ((ArchiveEntry*)caFile.archive.entries.ptr)[i];

		//Push back directory names. Size is not known until later.

		if (entry.type == EFileType_Folder) {

			_gotoIfError(clean, List_pushBack(
				&directories, Buffer_createConstRef(&entry.path, sizeof(CharString)), alloc
			));

			if(directories.length >= U16_MAX)
				_gotoIfError(clean, Error_outOfBounds(
					0, 0xFFFF, U16_MAX - 1, "CAFile_write() directories are limited to U16_MAX"
				));

			continue;
		}

		//Push back file names and calculate output buffer

		_gotoIfError(
			clean, 
			List_pushBack(&files, Buffer_createConstRef(&entry.path, sizeof(CharString)), alloc)
		);

		if(files.length >= U32_MAX)
			_gotoIfError(clean, Error_outOfBounds(0, files.length, U32_MAX, "CAFile_write() files are limited to U32_MAX"));
		
		if(outputSize + Buffer_length(entry.data) < outputSize)
			_gotoIfError(clean, Error_overflow(
				0, outputSize + Buffer_length(entry.data), outputSize, "CAFile_write() overflow"
			));

		outputSize += Buffer_length(entry.data);
		biggestFileSize = U64_max(biggestFileSize, Buffer_length(entry.data));
	}

	U64 dirRefSize  = directories.length <= 254 ? 1 : 2;
	U64 fileRefSize = files.length <= 65534 ? 2 : 4;

	EXXDataSizeType sizeType = biggestFileSize <= U8_MAX ? EXXDataSizeType_U8 : (
		biggestFileSize <= U16_MAX ? EXXDataSizeType_U16 : (
			biggestFileSize <= U32_MAX ? EXXDataSizeType_U32 : 
			EXXDataSizeType_U64
			)
		);

	baseFileHeader += SIZE_BYTE_TYPE[sizeType];
	baseFileHeader += dirRefSize;

	//Add directories, files and directory/file count.

	realHeaderSizeExEnc += dirRefSize + fileRefSize;
	realHeaderSize += dirRefSize + fileRefSize;

	U64 fileObjLen = dirRefSize * directories.length + baseFileHeader * files.length;

	if(outputSize + fileObjLen < outputSize)
		_gotoIfError(clean, Error_overflow(0, outputSize + fileObjLen, outputSize, "CAFile_write() overflow (2)"));

	outputSize += fileObjLen;

	//Sort directories and files to ensure our file ids are correct,
	//This is because we have full directory names, so it automatically resolves 
	// in a way where we know where our children are (in a linear way).
	//This sort is different than just sortString. It sorts on parent count first and then alphabetically.
	//This allows us to not have to reorder the files down the road since they're already sorted.

	if(
		!List_sortCustom(directories, (CompareFunction) sortParentCountAndFileNames) || 
		!List_sortCustom(files, (CompareFunction) sortParentCountAndFileNames)
	)
		_gotoIfError(clean, Error_invalidOperation(0, "CAFile_write() couldn't sort files and/or directories"));

	//Allocate and generate DLFile

	DLSettings dlSettings = (DLSettings) { .dataType = EDLDataType_Ascii };

	_gotoIfError(clean, DLFile_create(dlSettings, alloc, &dlFile));

	for(U64 i = 0; i < directories.length; ++i) {

		CharString dir = ((CharString*)directories.ptr)[i];
		CharString dirName = CharString_createNull();

		if(!CharString_cutBeforeLast(dir, '/', EStringCase_Sensitive, &dirName))
			dirName = CharString_createConstRefSized(dir.ptr, CharString_length(dir), CharString_isNullTerminated(dir));

		_gotoIfError(clean, DLFile_addEntryAscii(&dlFile, dirName, alloc));
	}

	for(U64 i = 0; i < files.length; ++i) {

		CharString file = ((CharString*)files.ptr)[i];
		CharString fileName = CharString_createNull();

		if(!CharString_cutBeforeLast(file, '/', EStringCase_Sensitive, &fileName))
			fileName = CharString_createConstRefSized(file.ptr, CharString_length(file), CharString_isNullTerminated(file));

		_gotoIfError(clean, DLFile_addEntryAscii(&dlFile, fileName, alloc));
	}

	//Complete DLFile as buffer

	dlFile.settings.flags |= EDLSettingsFlags_HideMagicNumber;				//Small optimization

	_gotoIfError(clean, DLFile_write(dlFile, alloc, &dlFileBuffer));

	DLFile_free(&dlFile, alloc);

	//Build up final output buffer
	//DLFile
	//CADirectory[]
	//CAFileObject[]
	//U8[sum(file[i].data)]

	if (outputSize + Buffer_length(dlFileBuffer) < outputSize)
		_gotoIfError(clean, Error_overflow(
			0, outputSize + Buffer_length(dlFileBuffer), outputSize, "CAFile_write() overflow (3)"
		));

	outputSize += Buffer_length(dlFileBuffer);

	if (outputSize + realHeaderSize < outputSize)
		_gotoIfError(clean, Error_overflow(
			0, outputSize + realHeaderSize, outputSize, "CAFile_write() overflow (4)"
		));

	outputSize += realHeaderSize;		//Reserve space for header (even though this won't be compressed)

	_gotoIfError(clean, Buffer_createUninitializedBytes(outputSize, alloc, &outputBuffer));

	//Append DLFile

	Buffer outputBufferIt = Buffer_createRef((U8*)outputBuffer.ptr + realHeaderSize, Buffer_length(outputBuffer) - realHeaderSize);

	_gotoIfError(clean, Buffer_appendBuffer(&outputBufferIt, dlFileBuffer));

	Buffer_free(&dlFileBuffer, alloc);

	//Append CADirectory[]

	U8 *dirPtr = (U8*)outputBufferIt.ptr;

	for (U16 i = 0; i < (U16) directories.length; ++i) {

		CharString dir = ((CharString*)directories.ptr)[i];
		U64 it = CharString_findLast(dir, '/', EStringCase_Sensitive);

		U16 parent = U16_MAX;

		//Add to parent directory

		if (it != U64_MAX) {

			//We need to find the real parent.
			//Since we're sorted alphabetically, we are able to look back from our current directory
			//If we encounter something that doesn't start with our basepath/ then we know we are missing this dir
			//And this means we somehow messed with the input data

			CharString baseDir = CharString_createNull(), realParentDir = CharString_createNull();

			if (
				!CharString_cut(dir, 0, it, &realParentDir) || 
				!CharString_cut(dir, 0, it + 1, &baseDir)
			) 
				_gotoIfError(clean, Error_invalidOperation(0, "CAFile_write() couldn't split directory name"));

			for(U64 j = i - 1; j != U64_MAX; --j)

				if (CharString_equalsString(
					((CharString*)directories.ptr)[j], realParentDir, 
					EStringCase_Insensitive
				)) {
					parent = (U16) j;
					break;
				}

			if(parent == U16_MAX)
				_gotoIfError(clean, Error_invalidState(0, "CAFile_write() couldn't find parent directory of folder"));
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

		CharString file = ((CharString*)files.ptr)[i];
		U64 it = CharString_findLast(file, '/', EStringCase_Sensitive);

		U16 parent = U16_MAX;

		//Add to parent directory

		if (it != U64_MAX) {

			//We need to find the real parent.
			//Since we're sorted alphabetically, we are able to look back from our current directory
			//If we encounter something that doesn't start with our basepath then we know we are missing this dir
			//And this means we somehow messed with the input data

			CharString baseDir = CharString_createNull(), realParentDir = CharString_createNull();

			if (
				!CharString_cut(file, 0, it, &realParentDir) || 
				!CharString_cut(file, 0, it + 1, &baseDir)
			)
				_gotoIfError(clean, Error_invalidOperation(0, "CAFile_write() couldn't split file name"));
			
			for(U64 j = directories.length - 1; j != U64_MAX; --j)

				if (CharString_equalsString(
					((CharString*)directories.ptr)[j], realParentDir, 
					EStringCase_Sensitive
				)) {
					parent = (U16) j;
					break;
				}

			if(parent == U16_MAX)
				_gotoIfError(clean, Error_invalidState(1, "CAFile_write() couldn't find parent directory of file"));
		}

		//Find corresponding file with name

		ArchiveEntry *entry = NULL;

		for(U64 j = 0; j < caFile.archive.entries.length; ++j)
			if (CharString_equalsString(
				((ArchiveEntry*)caFile.archive.entries.ptr + j)->path, file, 
				EStringCase_Sensitive
			)) {
				entry = (ArchiveEntry*)caFile.archive.entries.ptr + j;
				break;
			}

		if (!entry)
			_gotoIfError(clean, Error_invalidState(2, "CAFile_write() couldn't find entry by path"));

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
					_gotoIfError(clean, Error_invalidState(
						1, "CAFile_write() couldn't store file date, please use full date"
					));

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

	List_free(&files, alloc);
	List_free(&directories, alloc);

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
			Buffer_createConstRef(hash, sizeof(hash))
		);*/

	//Compress

	/*if (caFile.settings.compressionType != EXXCompressionType_None) {				TODO:

		Buffer toCompress = Buffer_createConstRef(
			outputBuffer.ptr + realHeaderSize, 
			Buffer_length(outputBuffer) - realHeaderSize
		);

		_gotoIfError(clean, Buffer_compress(toCompress, BufferCompressionType_Brotli11, alloc, &compressedOutput));

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

		Bool b = false;
		Error cmpErr = Buffer_eq(
			Buffer_createConstRef(key, sizeof(key)), 
			Buffer_createConstRef(caFile.settings.encryptionKey, sizeof(key)), 
			&b
		);

		Buffer toEncrypt = Buffer_createRef((U8*)outputBuffer.ptr + realHeaderSize, Buffer_length(outputBuffer) - realHeaderSize);
		Buffer realHeader = Buffer_createConstRef(outputBuffer.ptr, realHeaderSizeExEnc);

		_gotoIfError(clean, Buffer_encrypt(

			toEncrypt, 
			realHeader, 

			EBufferEncryptionType_AES256GCM, 

			EBufferEncryptionFlags_GenerateIv | (
				b || cmpErr.genericError ? EBufferEncryptionFlags_GenerateKey :
				EBufferEncryptionFlags_None
			),

			b || cmpErr.genericError ? NULL : caFile.settings.encryptionKey,

			(I32x4*)headerIt,
			(I32x4*)((U8*)headerIt + 12)
		));
	}

	//Prepend header and hash

	*result = outputBuffer;
	outputBuffer = Buffer_createNull();

clean:
	Buffer_free(&outputBuffer, alloc);
	Buffer_free(&dlFileBuffer, alloc);
	DLFile_free(&dlFile, alloc);
	List_free(&files, alloc);
	List_free(&directories, alloc);
	return err;
}

Error CAFile_read(Buffer file, const U32 encryptionKey[8], Allocator alloc, CAFile *caFile) {

	if (!caFile)
		return Error_nullPointer(2, "CAFile_read()::caFile is required");

	if (caFile->archive.entries.ptr)
		return Error_invalidParameter(2, 0, "CAFile_read()::caFile isn't empty, could indicate memleak");

	if (Buffer_length(file) < sizeof(CAHeader))
		return Error_outOfBounds(0, sizeof(CAHeader), Buffer_length(file), "CAFile_read()::file doesn't contain header");

	Buffer filePtr = Buffer_createRefFromBuffer(file, Buffer_isConstRef(file));

	Buffer tmpData = Buffer_createNull();
	Error err = Error_none();
	DLFile fileNames = (DLFile) { 0 };
	Archive archive = (Archive) { 0 };
	CharString tmpPath = CharString_createNull();

	_gotoIfError(clean, Archive_create(alloc, &archive));

	//Validate header

	CAHeader header;
	_gotoIfError(clean, Buffer_consume(&filePtr, &header, sizeof(header)));

	if(header.magicNumber != CAHeader_MAGIC)
		_gotoIfError(clean, Error_invalidParameter(0, 0, "CAFile_read()::file contained invalid header"));

	if(header.version != CAHeader_V1_0)
		_gotoIfError(clean, Error_invalidParameter(0, 1, "CAFile_read()::file header doesn't have correct version"));

	if(header.flags & (ECAFlags_UseAESChunksA | ECAFlags_UseAESChunksB))		//TODO: AES chunks
		_gotoIfError(clean, Error_unsupportedOperation(0, "CAFile_read() AES chunks not supported yet"));

	if(header.type >> 4)							//TODO: Compression
		_gotoIfError(clean, Error_unsupportedOperation(1, "CAFile_read() decompression not supported yet"));

	if(header.flags & ECAFlags_UseSHA256)				//TODO: SHA256
		_gotoIfError(clean, Error_unsupportedOperation(3, "CAFile_read() SHA256 not supported yet"));

	if((header.type & 0xF) >= EXXEncryptionType_Count)
		_gotoIfError(clean, Error_invalidParameter(0, 4, "CAFile_read() encryption type unsupported"));

	//Ensure encryption key isn't provided if we're not encrypting

	if(encryptionKey && !(header.type & 0xF))
		_gotoIfError(clean, Error_invalidOperation(3, "CAFile_read() encryption key provided but encryption isn't used"));

	if(!encryptionKey && (header.type & 0xF))
		_gotoIfError(clean, Error_unauthorized(0, "CAFile_read() encryption key is required if encryption is used"));

	//Validate file and dir count

	U32 fileCount = 0;
	_gotoIfError(clean, Buffer_consume(&filePtr, &fileCount, header.flags & ECAFlags_FilesCountLong ? 4 : 2));

	U16 dirCount = 0;
	_gotoIfError(clean, Buffer_consume(&filePtr, &dirCount, header.flags & ECAFlags_DirectoriesCountLong ? 2 : 1));

	if(dirCount >= (header.flags & ECAFlags_DirectoriesCountLong ? U16_MAX : U8_MAX))
		_gotoIfError(clean, Error_invalidParameter(0, 7, "CAFile_read() directory count can't be the max bit value"));

	if(fileCount >= (header.flags & ECAFlags_FilesCountLong ? U32_MAX : U16_MAX))
		_gotoIfError(clean, Error_invalidParameter(0, 8, "CAFile_read() file count can't be the max bit value"));

	//Validate extended data

	CAExtraInfo extra = (CAExtraInfo) { 0 };

	if(header.flags & ECAFlags_HasExtendedData) {
		_gotoIfError(clean, Buffer_consume(&filePtr, &extra, sizeof(extra)));
		_gotoIfError(clean, Buffer_consume(&filePtr, NULL, extra.headerExtensionSize));
	}

	//Check for encryption

	I32x4 iv = I32x4_zero(), tag = I32x4_zero();

	if ((header.type & 0xF) == EXXEncryptionType_AES256GCM) {

		U64 headerLen = filePtr.ptr - file.ptr;

		_gotoIfError(clean, Buffer_consume(&filePtr, &iv, 12));
		_gotoIfError(clean, Buffer_consumeI32x4(&filePtr, &tag));

		_gotoIfError(clean, Buffer_decrypt(
			filePtr,
			Buffer_createConstRef(file.ptr, headerLen),
			EBufferEncryptionType_AES256GCM,
			encryptionKey,
			tag,
			iv
		));
	}

	_gotoIfError(clean, DLFile_read(filePtr, NULL, true, alloc, &fileNames));
	_gotoIfError(clean, Buffer_consume(&filePtr, NULL, fileNames.readLength));

	//Validate DLFile
	//File name validation is done when the entries are inserted into the Archive

	if (
		fileNames.settings.dataType != EDLDataType_Ascii || 
		fileNames.settings.compressionType || 
		fileNames.settings.encryptionType ||
		fileNames.settings.flags
	)
		_gotoIfError(clean, Error_invalidOperation(
			0, "CAFile_read() embedded oiDL needs to be ascii without compression/encryption or flags"
		));

	if(fileNames.entries.length != (U64)fileCount + dirCount)
		_gotoIfError(clean, Error_invalidState(0, "CAFile_read() embedded oiDL has mismatching name count with file count"));

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
		_gotoIfError(clean, Error_outOfBounds(
			0, fileSize + folderSize, Buffer_length(filePtr), "CAFile_read() files out of bounds"
		));

	//Now we can add dir to the archive

	for (U64 i = 0; i < dirCount; ++i) {

		const U8 *diri = filePtr.ptr + folderStride * i;

		CharString name = ((DLEntry*)fileNames.entries.ptr)[i].entryString;

		if(!CharString_isValidFileName(name))
			_gotoIfError(clean, Error_invalidParameter(0, 0, "CAFile_read() directory has invalid name"));
		
		_gotoIfError(clean, CharString_createCopy(name, alloc, &tmpPath));

		U16 parent = dirStride == 2 ? *(const U16*)diri : *diri;

		if (parent != rootDir) {

			if (parent >= i)
				_gotoIfError(clean, Error_invalidOperation(1, "CAFile_read() parent directory index of folder out of bounds"));

			CharString parentName = ((ArchiveEntry*)archive.entries.ptr + parent)->path;

			_gotoIfError(clean, CharString_insert(&tmpPath, '/', 0, alloc));
			_gotoIfError(clean, CharString_insertString(&tmpPath, parentName, 0, alloc));
		}

		_gotoIfError(clean, Archive_addDirectory(&archive, tmpPath, alloc));

		tmpPath = CharString_createNull();
	}

	//Add file

	const U8 *fileIt = filePtr.ptr + folderSize;
	_gotoIfError(clean, Buffer_offset(&filePtr, fileSize + folderSize));

	for (U64 i = 0; i < fileCount; ++i) {

		const U8 *filei = fileIt + fileStride * i;

		CharString name = ((DLEntry*)fileNames.entries.ptr)[(U64)i + dirCount].entryString;

		if(!CharString_isValidFileName(name))
			_gotoIfError(clean, Error_invalidParameter(0, 1, "CAFile_read() file has invalid name"));

		_gotoIfError(clean, CharString_createCopy(name, alloc, &tmpPath));

		//Load parent

		U16 parent = dirStride == 2 ? *(const U16*)filei : *filei;
		filei += dirStride;

		if (parent != rootDir) {

			if (parent >= dirCount)
				_gotoIfError(clean, Error_invalidOperation(2, "CAFile_read() parent directory index of file out of bounds"));

			CharString parentName = ((ArchiveEntry*)archive.entries.ptr + parent)->path;

			_gotoIfError(clean, CharString_insert(&tmpPath, '/', 0, alloc));
			_gotoIfError(clean, CharString_insertString(&tmpPath, parentName, 0, alloc));
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
		
		_gotoIfError(clean, Buffer_createUninitializedBytes(bufferSize, alloc, &tmpData));

		const U8 *dataPtr = filePtr.ptr;
		_gotoIfError(clean, Buffer_offset(&filePtr, bufferSize));

		Buffer_copy(tmpData, Buffer_createConstRef(dataPtr, bufferSize));

		//Move path and data to file

		_gotoIfError(clean, Archive_addFile(&archive, tmpPath, tmpData, timestamp, alloc));

		tmpPath = CharString_createNull();
		tmpData = Buffer_createNull();
	}

	if(Buffer_length(filePtr))
		return Error_invalidState(0, "CAFile_read() had leftover data after oiCA, this is illegal");

	caFile->archive = archive;
	archive = (Archive) { 0 };

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

	if(err.genericError)
		CAFile_free(caFile, alloc);

	Buffer_free(&tmpData, alloc);
	CharString_free(&tmpPath, alloc);
	Archive_free(&archive, alloc);
	DLFile_free(&fileNames, alloc);
	return err;
}
