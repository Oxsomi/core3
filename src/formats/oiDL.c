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

#include "types/list_impl.h"
#include "formats/oiDL.h"
#include "types/time.h"
#include "types/error.h"
#include "types/math.h"

static const U8 DLHeader_V1_0 = 0;

//Helper functions to create it

Bool DLFile_createIntern(DLSettings settings, Allocator alloc, U64 reserve, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile)
		retError(clean, Error_nullPointer(0, "DLFile_create()::dlFile is required"))

	if(DLFile_isAllocated(*dlFile))
		retError(clean, Error_invalidOperation(0, "DLFile_create()::dlFile isn't empty, might indicate memleak"))

	if(settings.compressionType >= EXXCompressionType_Count)
		retError(clean, Error_invalidParameter(0, 0, "DLFile_create()::settings.compressionType is invalid"))

	if(settings.compressionType > EXXCompressionType_None)
		retError(clean, Error_invalidOperation(0, "DLFile_create() compression not supported yet"))		//TODO:

	if(settings.encryptionType >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 1, "DLFile_create()::settings.encryptionType is invalid"))

	if(settings.dataType >= EDLDataType_Count)
		retError(clean, Error_invalidParameter(0, 2, "DLFile_create()::settings.dataType is invalid"))

	if(settings.flags & EDLSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 3, "DLFile_create()::settings.flags contained unsupported flag"))

	dlFile->entryBuffers = (ListBuffer) { 0 };		//ListBuffer and ListCharString are same size

	if(reserve) {

		if(settings.dataType != EDLDataType_Ascii)
			gotoIfError2(clean, ListBuffer_reserve(&dlFile->entryBuffers, reserve, alloc))

		else gotoIfError2(clean, ListCharString_reserve(&dlFile->entryStrings, reserve, alloc))
	}

	dlFile->settings = settings;

clean:
	return s_uccess;
}

U64 DLFile_entryCount(DLFile file) {
	return file.entryBuffers.length;		//Union of entryBuffers and entryStrings contains length at same position
}

Bool DLFile_isAllocated(DLFile file) {
	return file.entryBuffers.ptr;			//Union of entryBuffers and entryStrings contains length at same position
}

Bool DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile, Error *e_rr) {
	return DLFile_createIntern(settings, alloc, 64, dlFile, e_rr);
}

Bool DLFile_free(DLFile *dlFile, Allocator alloc) {

	if(!dlFile || !DLFile_isAllocated(*dlFile))
		return true;

	if (dlFile->settings.dataType != EDLDataType_Ascii) {

		for (U64 i = 0; i < DLFile_entryCount(*dlFile); ++i)
			Buffer_free(&dlFile->entryBuffers.ptrNonConst[i], alloc);

		ListBuffer_free(&dlFile->entryBuffers, alloc);
	}

	else ListCharString_freeUnderlying(&dlFile->entryStrings, alloc);

	*dlFile = (DLFile) { 0 };
	return true;
}

CharString DLFile_stringAt(DLFile dlFile, U64 i, Bool *success) {

	if(dlFile.settings.dataType == EDLDataType_Data || i >= dlFile.entryStrings.length) {

		if(success)
			*success = false;

		return CharString_createNull();
	}

	if(success)
		*success = true;

	if(dlFile.settings.dataType == EDLDataType_Ascii)
		return dlFile.entryStrings.ptr[i];

	Buffer buf = dlFile.entryBuffers.ptr[i];
	return CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false);
}

Bool DLFile_entryEqualsString(DLFile dlFile, U64 i, CharString str) {
	Bool success = false;
	return CharString_equalsStringSensitive(DLFile_stringAt(dlFile, i, &success), str) && success;
}

U64 DLFile_find(DLFile dlFile, U64 start, U64 end, CharString str) {

	for (U64 j = start; j < dlFile.entryBuffers.length && j < end; ++j)
		if (DLFile_entryEqualsString(dlFile, j, str))
			return j;

	return U64_MAX;
}

//Writing

Bool DLFile_addEntry(DLFile *dlFile, Buffer entryBuf, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile || !DLFile_isAllocated(*dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntry()::dlFile is required"))

	if(dlFile->settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntry() is unsupported if type isn't Data"))

	gotoIfError2(clean, ListBuffer_pushBack(&dlFile->entryBuffers, entryBuf, alloc))

clean:
	return s_uccess;
}

Bool DLFile_createBufferList(DLSettings settings, ListBuffer buffers, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidOperation(0, "DLFile_createBufferList() is unsupported if settings.type isn't Data"))

	gotoIfError3(clean, DLFile_createIntern(settings, alloc, 0, dlFile, e_rr))
	dlFile->entryBuffers = buffers;

clean:
	return s_uccess;
}

Bool DLFile_addEntryAscii(DLFile *dlFile, CharString entryStr, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile || !dlFile->entryStrings.ptr)
		retError(clean, Error_nullPointer(0, "DLFile_addEntryAscii()::dlFile is required"))

	if(dlFile->settings.dataType != EDLDataType_Ascii)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntryAscii() is unsupported if type isn't Ascii"))

	if(!CharString_isValidAscii(entryStr))
		retError(clean, Error_invalidParameter(1, 0, "DLFile_addEntryAscii()::entryStr isn't valid Ascii"))

	gotoIfError2(clean, ListCharString_pushBack(&dlFile->entryStrings, entryStr, alloc))

clean:
	return s_uccess;
}

Bool DLFile_createAsciiList(DLSettings settings, ListCharString strings, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(settings.dataType != EDLDataType_Ascii)
		retError(clean, Error_invalidOperation(0, "DLFile_createAsciiList() is unsupported if settings.type isn't Ascii"))

	for(U64 i = 0; i < strings.length; ++i)
		if(!CharString_isValidAscii(strings.ptr[i]))
			retError(clean, Error_invalidParameter(1, 0, "DLFile_createAsciiList()::strings[i] isn't valid Ascii"))

	gotoIfError3(clean, DLFile_createIntern(settings, alloc, 0, dlFile, e_rr))

	dlFile->entryStrings = strings;

clean:
	return s_uccess;
}

Bool DLFile_createAsciiListIntern(DLSettings settings, ListBuffer *strings, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	ListCharString strs = (ListCharString) { 0 };
	gotoIfError2(clean, ListCharString_resize(&strs, strings->length, alloc))

	for(U64 i = 0; i < strings->length; ++i) {

		const Buffer buf = strings->ptr[i];
		CharString str;

		if(Buffer_isConstRef(buf))
			str = CharString_createRefSizedConst((const C8*)buf.ptr, Buffer_length(buf), false);

		else if(Buffer_isRef(buf))
			str = CharString_createRefSized((C8*)buf.ptr, Buffer_length(buf), false);

		else str = (CharString) {
			.ptr = (const C8*) buf.ptr,
			.capacityAndRefInfo = Buffer_length(buf),
			.lenAndNullTerminated = Buffer_length(buf)
		};

		strs.ptrNonConst[i] = str;
	}

	gotoIfError3(clean, DLFile_createAsciiList(settings, strs, alloc, dlFile, e_rr))

clean:

	if(!s_uccess)
		ListBuffer_free(strings, alloc);

	return s_uccess;
}

Bool DLFile_addEntryUTF8(DLFile *dlFile, Buffer entryBuf, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile || !DLFile_isAllocated(*dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntryUTF8()::dlFile is required"))

	if(dlFile->settings.dataType != EDLDataType_UTF8)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntryUTF8() is unsupported if type isn't UTF8"))

	if(!Buffer_isUTF8(entryBuf, 1))
		retError(clean, Error_invalidParameter(1, 0, "DLFile_addEntryAscii()::entryBuf isn't valid UTF8"))

	gotoIfError2(clean, ListBuffer_pushBack(&dlFile->entryBuffers, entryBuf, alloc))

clean:
	return s_uccess;
}

Bool DLFile_createUTF8List(DLSettings settings, ListBuffer buffers, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(settings.dataType != EDLDataType_UTF8)
		retError(clean, Error_invalidOperation(0, "DLFile_createUTF8List() is unsupported if settings.type isn't UTF8"))

	for(U64 i = 0; i < buffers.length; ++i)
		if(!Buffer_isUTF8(buffers.ptr[i], 1))
			retError(clean, Error_invalidParameter(
				1, (U32)i, "DLFile_createUTF8List()::buffers[i] isn't valid UTF8"
			))

	gotoIfError3(clean, DLFile_createIntern(settings, alloc, buffers.length, dlFile, e_rr))
	dlFile->entryBuffers = buffers;

clean:
	return s_uccess;
}

Bool DLFile_createList(DLSettings settings, ListBuffer *buffers, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(!buffers)
		retError(clean, Error_nullPointer(1, "DLFile_createList()::buffers is required"))

	switch (settings.dataType) {

		case EDLDataType_Ascii:
			gotoIfError3(clean, DLFile_createAsciiListIntern(settings, buffers, alloc, dlFile, e_rr))
			break;

		case EDLDataType_UTF8:
			gotoIfError3(clean, DLFile_createUTF8List(settings, *buffers, alloc, dlFile, e_rr))
			break;

		default:
			gotoIfError3(clean, DLFile_createBufferList(settings, *buffers, alloc, dlFile, e_rr))
			break;
	}

clean:

	if(s_uccess)
		*buffers = (ListBuffer) { 0 };		//Moved

	return s_uccess;
}

//We currently don't support compression yet. But once Buffer_compress/decompress is available, it should be easy.

Bool DLFile_write(DLFile dlFile, Allocator alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;
	Buffer compressedOutput = Buffer_createNull();
	Buffer uncompressedData = Buffer_createNull();

	if(!DLFile_isAllocated(dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_write()::dlFile is required"))

	if(!result)
		retError(clean, Error_nullPointer(2, "DLFile_write()::result is required"))

	if(result->ptr)
		retError(clean, Error_invalidOperation(0, "DLFile_write()::result wasn't empty, might indicate memleak"))

	//Get header size (excluding compressedSize + entryCount)

	const U64 hashSize = !dlFile.settings.compressionType ? 0 : (dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ? 32 : 4);
	U64 headerSize = sizeof(DLHeader) + hashSize;

	if(!(dlFile.settings.flags & EDLSettingsFlags_HideMagicNumber))		//Magic number (can be hidden by parent; such as oiCA)
		headerSize += sizeof(U32);

	//Get data size

	U64 outputSize = 0, maxSize = 0;

	for (U64 i = 0; i < DLFile_entryCount(dlFile); ++i) {

		const U64 len =
			dlFile.settings.dataType != EDLDataType_Ascii ? Buffer_length(dlFile.entryBuffers.ptr[i]) :
			CharString_length(dlFile.entryStrings.ptr[i]);

		if(outputSize + len < outputSize)
			retError(clean, Error_overflow(0, outputSize + len, outputSize, "DLFile_write() overflow"))

		outputSize += len;
		maxSize = U64_max(maxSize, len);
	}

	const EXXDataSizeType dataSizeType = EXXDataSizeType_getRequiredType(maxSize);
	const U8 dataSizeTypeSize = SIZE_BYTE_TYPE[dataSizeType];
	const U64 entrySizes = dataSizeTypeSize * DLFile_entryCount(dlFile);

	if(entrySizes / dataSizeTypeSize != DLFile_entryCount(dlFile))
		retError(clean, Error_overflow(0, entrySizes, entrySizes / dataSizeTypeSize, "DLFile_write() overflow (2)"))

	const EXXDataSizeType entrySizeType = EXXDataSizeType_getRequiredType(DLFile_entryCount(dlFile));
	headerSize += SIZE_BYTE_TYPE[entrySizeType];

	const U64 entrySizesOffset = headerSize;
	headerSize += entrySizes;

	if(dlFile.settings.encryptionType)
		headerSize += sizeof(I32x4) + 12;

	const EXXDataSizeType uncompressedSizeType = EXXDataSizeType_getRequiredType(outputSize);

	if(dlFile.settings.compressionType)
		headerSize += SIZE_BYTE_TYPE[uncompressedSizeType];

	if(outputSize + headerSize < outputSize)
		retError(clean, Error_overflow(0, outputSize + headerSize, outputSize, "DLFile_write() overflow (3)"))

	//Create our final uncompressed buffer

	gotoIfError2(clean, Buffer_createUninitializedBytes(outputSize + headerSize, alloc, &uncompressedData))

	U8 *sizes = (U8*)uncompressedData.ptr + entrySizesOffset;
	U8 *dat = (U8*)uncompressedData.ptr + headerSize;

	for (U64 i = 0; i < DLFile_entryCount(dlFile); ++i) {

		const Buffer buf =
			dlFile.settings.dataType != EDLDataType_Ascii ? dlFile.entryBuffers.ptr[i] :
			CharString_bufferConst(dlFile.entryStrings.ptr[i]);

		const U64 len = Buffer_length(buf);

		Buffer_forceWriteSizeType(sizes + dataSizeTypeSize * i, dataSizeType, len);

		Buffer_copy(Buffer_createRef(dat, len), buf);
		dat += len;
	}

	//Prepend header and hash

	U32 hash[8] = { 0 };

	U8 *headerIt = (U8*)uncompressedData.ptr;

	if (!(dlFile.settings.flags & EDLSettingsFlags_HideMagicNumber)) {
		*(U32*)headerIt = DLHeader_MAGIC;
		headerIt += sizeof(U32);
	}

	*((DLHeader*)headerIt) = (DLHeader) {

		.version = DLHeader_V1_0,

		.flags = (U8) (

			(
				dlFile.settings.compressionType ? (
					dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ? EDLFlags_UseSHA256 :
					EDLFlags_None
				) :
				EDLFlags_None
			) |

			(dlFile.settings.dataType == EDLDataType_Ascii ? EDLFlags_IsString : (
				dlFile.settings.dataType == EDLDataType_UTF8 ? EDLFlags_IsString | EDLFlags_UTF8 : EDLFlags_None
			))
		),

		.type = (U8)((dlFile.settings.compressionType << 4) | dlFile.settings.encryptionType),

		.sizeTypes =
			(U8)EXXDataSizeType_getRequiredType(DLFile_entryCount(dlFile)) |
			((U8)EXXDataSizeType_getRequiredType(outputSize) << 2) |
			((U8)EXXDataSizeType_getRequiredType(maxSize) << 4)
	};

	headerIt += sizeof(DLHeader);

	headerIt += Buffer_forceWriteSizeType(headerIt, entrySizeType, DLFile_entryCount(dlFile));

	Buffer_copy(Buffer_createRef(headerIt, entrySizes), Buffer_createRefConst(sizes, entrySizes));
	headerIt += entrySizes;		//Already filled

	if (dlFile.settings.compressionType)
		headerIt += Buffer_forceWriteSizeType(headerIt, uncompressedSizeType, outputSize);

	//Copy empty hash

	if(dlFile.settings.compressionType)
		Buffer_copy(
			Buffer_createRef(headerIt, hashSize),
			Buffer_createRefConst(hash, hashSize)
		);

	//Hash

	if(dlFile.settings.compressionType) {

		//Generate hash

		if (dlFile.settings.flags & EDLSettingsFlags_UseSHA256)
			Buffer_sha256(uncompressedData, hash);

		else hash[0] = Buffer_crc32c(uncompressedData);

		//Copy real hash to our header

		Buffer_copy(
			Buffer_createRef(headerIt, hashSize),
			Buffer_createRefConst(hash, hashSize)
		);

		headerIt += hashSize;
	}

	//Compress

	//U64 uncompressedSize = Buffer_length(uncompressedData) - headerSize;

	/*if (dlFile.settings.compressionType) {

		//TODO: Impossible to reach for now, but implement this once it exists

		if ((err = Buffer_compress(
				uncompressedData, BufferCompressionType_Brotli11, alloc, &compressedOutput
			)).genericError
		) {
			Buffer_free(&uncompressedData, alloc);
			return err;
		}

		Buffer_free(&uncompressedData, alloc);

	}

	else {*/
		compressedOutput = Buffer_createRef(
			(U8*)uncompressedData.ptr + headerSize,
			Buffer_length(uncompressedData) - headerSize
		);
	//}

	//Encrypt

	if (dlFile.settings.encryptionType) {

		//TODO: Support chunks
		//		Select no chunks if <40MiB
		//		Select 10MiB if at least 4 threads can be kept busy.
		//		Select 50MiB if ^ and utilization is about the same (e.g. 24 threads doing 10MiB would need 1.2GiB).
		//		Select 100MiB if ^ (24 threads would need 2.4GiB).

		const U32 key[8] = { 0 };

		const Bool b = Buffer_eq(
			Buffer_createRefConst(key, sizeof(key)),
			Buffer_createRefConst(dlFile.settings.encryptionKey, sizeof(key))
		);

		gotoIfError2(clean, Buffer_encrypt(

			compressedOutput,
			Buffer_createRefConst(uncompressedData.ptr, headerSize - sizeof(I32x4) - 12),

			EBufferEncryptionType_AES256GCM,
			EBufferEncryptionFlags_GenerateIv | (b ? EBufferEncryptionFlags_GenerateKey : EBufferEncryptionFlags_None),

			dlFile.settings.encryptionKey,

			(I32x4*)headerIt,
			(I32x4*)((U8*)headerIt + 12)
		))

		headerIt += 12 + sizeof(I32x4);
		headerSize += 12 + sizeof(I32x4);
	}

	//Shortcut because we didn't allocate extra memory, so our final data is already present in the buffer.
	//No need to allocate again

	if (!dlFile.settings.compressionType) {
		*result = uncompressedData;
		goto clean;
	}

	//Compression happened, we need to merge compressed data and header.

	const Buffer headerBuf = Buffer_createRefConst(uncompressedData.ptr, headerSize);

	gotoIfError2(clean, Buffer_combine(headerBuf, compressedOutput, alloc, result))
	Buffer_free(&compressedOutput, alloc);
	Buffer_free(&uncompressedData, alloc);

clean:
	return s_uccess;
}

Bool DLFile_read(
	Buffer file,
	const U32 encryptionKey[8],
	Bool isSubFile,
	Allocator alloc,
	DLFile *dlFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocate = false;

	if(!dlFile)
		retError(clean, Error_nullPointer(2, "DLFile_read()::dlFile is required"))

	if(DLFile_isAllocated(*dlFile))
		retError(clean, Error_invalidOperation(0, "DLFile_read()::dlFile isn't empty, might indicate memleak"))

	Buffer entireFile = file;

	file = Buffer_createRefFromBuffer(file, true);

	if (!isSubFile) {

		U32 magic;
		gotoIfError2(clean, Buffer_consume(&file, &magic, sizeof(magic)))

		if(magic != DLHeader_MAGIC)
			retError(clean, Error_invalidParameter(0, 0, "DLFile_read() requires magicNumber prefix"))
	}

	//Read from binary

	DLHeader header;
	gotoIfError2(clean, Buffer_consume(&file, &header, sizeof(header)))

	//Validate header

	if(header.version != DLHeader_V1_0)
		retError(clean, Error_invalidParameter(0, 1, "DLFile_read() header.version is invalid"))

	if(header.flags & (EDLFlags_UseAESChunksA | EDLFlags_UseAESChunksB))		//TODO: AES chunks
		retError(clean, Error_unsupportedOperation(0, "DLFile_read() AES chunks not supported yet"))

	if(header.type >> 4)								//TODO: Compression
		retError(clean, Error_unsupportedOperation(1, "DLFile_read() compression not supported yet"))

	if(header.flags & EDLFlags_UseSHA256)				//TODO: SHA256
		retError(clean, Error_unsupportedOperation(2, "DLFile_read() SHA256 not supported yet"))

	if((header.type & 0xF) >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 4, "DLFile_read() invalid encryption type"))

	if(header.sizeTypes >> 6)
		retError(clean, Error_invalidParameter(0, 7, "DLFile_read() header.sizeTypes is invalid"))

	//Ensure encryption key isn't provided if we're not encrypting

	if(encryptionKey && !(header.type & 0xF))
		retError(clean, Error_invalidOperation(3, "DLFile_read() encryptionKey is provided but no encryption is used"))

	if(!encryptionKey && (header.type & 0xF))
		retError(clean, Error_unauthorized(0, "DLFile_read() encryptionKey is required"))

	//

	EXXDataSizeType entrySizeType			= (EXXDataSizeType)(header.sizeTypes & 3);
	EXXDataSizeType uncompressedSizeType	= (EXXDataSizeType)((header.sizeTypes >> 2) & 3);
	EXXDataSizeType dataSizeType			= (EXXDataSizeType)(header.sizeTypes >> 4);

	//Entry size

	U64 entryCount;
	gotoIfError2(clean, Buffer_consumeSizeType(&file, entrySizeType, &entryCount))

	//Extending header

	U64 entrySizeExtension = 0;

	if (header.flags & EDLFlags_HasExtendedData) {

		DLExtraInfo extraInfo;
		gotoIfError2(clean, Buffer_consume(&file, &extraInfo, sizeof(extraInfo)))

		entrySizeExtension = extraInfo.perEntryExtendedData;

		//We could do something here for new version parsing or extensions.

		gotoIfError2(clean, Buffer_consume(&file, NULL, extraInfo.extendedHeader))
	}

	//Entry counts

	U64 entryStride = entrySizeExtension + SIZE_BYTE_TYPE[dataSizeType];

	const U8 *entryStart = file.ptr;
	gotoIfError2(clean, Buffer_consume(&file, NULL, entryStride * entryCount))

	U64 dataSize = 0;

	for (U64 i = 0; i < entryCount; ++i) {

		const U8 *entryi = entryStart + i * entryStride;

		U64 entryLen = Buffer_forceReadSizeType(entryi, dataSizeType);

		if(dataSize + entryLen < dataSize)
			retError(clean, Error_overflow(0, dataSize + entryLen, U64_MAX, "DLFile_read() overflow"))

		dataSize += entryLen;
	}

	//Uncompressed size

	if (header.type >> 4) {

		U64 uncompressedSize;
		gotoIfError2(clean, Buffer_consumeSizeType(&file, uncompressedSizeType, &uncompressedSize))

		//TODO: Decompression

		//TODO: Hash
	}

	//Decrypt

	if (header.type & 0xF) {

		if(Buffer_isConstRef(entireFile))
			retError(clean, Error_constData(
				0, 0, "DLFile_read() file needs to be writable to allow decryption"
			))

		//Get tag and iv

		Buffer headerExEnc = file;

		I32x4 iv = I32x4_zero();
		gotoIfError2(clean, Buffer_consume(&file, &iv, 12))

		I32x4 tag;
		gotoIfError2(clean, Buffer_consume(&file, &tag, sizeof(I32x4)))

		//Validate remainder

		if(Buffer_length(file) < dataSize)
			retError(
				clean, Error_outOfBounds(
					0, file.ptr + dataSize - entireFile.ptr, Buffer_length(entireFile),
					"DLFile_read() doesn't contain enough data"
				)
			)

		if(!isSubFile && Buffer_length(file) != dataSize)
			retError(clean, Error_invalidState(
				0, "DLFile_read() contained extra data, which isn't allowed if it's not a subfile"
			))

		//Decrypt

		gotoIfError2(clean, Buffer_decrypt(
			Buffer_createRef((U8*)file.ptr, dataSize),
			Buffer_createRefConst(entireFile.ptr, headerExEnc.ptr - entireFile.ptr),
			EBufferEncryptionType_AES256GCM,
			encryptionKey,
			tag,
			iv
		))
	}

	//Allocate DLFile

	DLSettings settings = (DLSettings) {

		.compressionType = (EXXCompressionType) (header.type >> 4),
		.encryptionType = (EXXEncryptionType) (header.type & 0xF),

		.dataType = header.flags & EDLFlags_UTF8 ? EDLDataType_UTF8 : (
			header.flags & EDLFlags_IsString ? EDLDataType_Ascii : EDLDataType_Data
		),

		.flags = header.flags & EDLFlags_UseSHA256 ? EDLSettingsFlags_UseSHA256 : EDLSettingsFlags_None
	};

	//Not copying encryption key, because you probably don't want to store it.
	//And you already have it.

	//Create DLFile

	gotoIfError3(clean, DLFile_create(settings, alloc, dlFile, e_rr))
	allocate = true;

	//Per entry

	for (U64 i = 0; i < entryCount; ++i) {

		const U8 *entryi = entryStart + i * entryStride;

		U64 entryLen = Buffer_forceReadSizeType(entryi, dataSizeType);

		const U8 *ptr = file.ptr;
		gotoIfError2(clean, Buffer_consume(&file, NULL, entryLen))

		Buffer buf = Buffer_createRefConst(ptr, entryLen);

		switch(settings.dataType) {

			case EDLDataType_Data:	gotoIfError3(clean, DLFile_addEntry(dlFile, buf, alloc, e_rr))		break;
			case EDLDataType_UTF8:	gotoIfError3(clean, DLFile_addEntryUTF8(dlFile, buf, alloc, e_rr))	break;

			default:

				gotoIfError3(clean, DLFile_addEntryAscii(
					dlFile, CharString_createRefSizedConst((const C8*)ptr, entryLen, false), alloc, e_rr
				))

				break;
		}
	}

	if(!isSubFile && Buffer_length(file))
		retError(clean, Error_invalidState(1, "DLFile_read() contained extra data, not allowed if it's not a subfile"))

	if(file.ptr)
		dlFile->readLength = file.ptr - entireFile.ptr;

	else dlFile->readLength = Buffer_length(entireFile);

clean:

	if(!s_uccess && allocate)
		DLFile_free(dlFile, alloc);

	return s_uccess;
}

Bool DLFile_combine(DLFile a, DLFile b, Allocator alloc, DLFile *combined, Error *e_rr) {

	Bool s_uccess = true;
	const void *aSettingsPtr = &a.settings;
	const void *bSettingsPtr = &b.settings;

	for(U64 i = 0; i < 6; ++i)
		if(((const U64*)aSettingsPtr)[i] != ((const U64*)bSettingsPtr)[i])
			retError(clean, Error_invalidParameter(1, 0, "DLFile_combine()::a is incompatible with b"))
		
	gotoIfError3(clean, DLFile_create(a.settings, alloc, combined, e_rr))

	if(a.settings.dataType == EDLDataType_Ascii) {

		U64 aj = a.entryStrings.length;
		U64 j = aj + b.entryStrings.length;
		gotoIfError2(clean, ListCharString_reserve(&combined->entryStrings, j, alloc))

		for(U64 i = 0; i < j; ++i)
			gotoIfError3(clean, DLFile_addEntryAscii(
				combined, i < aj ? a.entryStrings.ptr[i] : b.entryStrings.ptr[i - aj], alloc, e_rr
			))
	}

	else {
		
		U64 aj = a.entryBuffers.length;
		U64 j = aj + b.entryBuffers.length;
		gotoIfError2(clean, ListBuffer_reserve(&combined->entryBuffers, j, alloc))

		for(U64 i = 0; i < j; ++i)
			if(a.settings.dataType == EDLDataType_Data)
				gotoIfError3(clean, DLFile_addEntry(
					combined, i < aj ? a.entryBuffers.ptr[i] : b.entryBuffers.ptr[i - aj], alloc, e_rr
				))

			else gotoIfError3(clean, DLFile_addEntryUTF8(
				combined, i < aj ? a.entryBuffers.ptr[i] : b.entryBuffers.ptr[i - aj], alloc, e_rr
			))
	}

clean:
	return s_uccess;
}
