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

#include "types/list_impl.h"
#include "formats/oiDL.h"
#include "types/time.h"
#include "types/error.h"

TListImpl(DLEntry);

static const U8 DLHeader_V1_0  = 0;

//Helper functions to create it

Error DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile) {

	if(!dlFile)
		return Error_nullPointer(0, "DLFile_create()::dlFile is required");

	if(dlFile->entries.ptr)
		return Error_invalidOperation(0, "DLFile_create()::dlFile isn't empty, might indicate memleak");

	if(settings.compressionType >= EXXCompressionType_Count)
		return Error_invalidParameter(0, 0, "DLFile_create()::settings.compressionType is invalid");

	if(settings.compressionType > EXXCompressionType_None)
		return Error_invalidOperation(0, "DLFile_create() compression not supported yet");		//TODO:

	if(settings.encryptionType >= EXXEncryptionType_Count)
		return Error_invalidParameter(0, 1, "DLFile_create()::settings.encryptionType is invalid");

	if(settings.dataType >= EDLDataType_Count)
		return Error_invalidParameter(0, 2, "DLFile_create()::settings.dataType is invalid");

	if(settings.flags & EDLSettingsFlags_Invalid)
		return Error_invalidParameter(0, 3, "DLFile_create()::settings.flags contained unsupported flag");

	dlFile->entries = (ListDLEntry) { 0 };
	Error err = ListDLEntry_reserve(&dlFile->entries, 100, alloc);

	if(err.genericError)
		return err;

	dlFile->settings = settings;
	return Error_none();
}

Bool DLFile_free(DLFile *dlFile, Allocator alloc) {

	if(!dlFile || !dlFile->entries.ptr)
		return true;

	for (U64 i = 0; i < dlFile->entries.length; ++i) {

		DLEntry entry = dlFile->entries.ptr[i];

		if(dlFile->settings.dataType == EDLDataType_Ascii)
			CharString_free(&entry.entryString, alloc);

		else Buffer_free(&entry.entryBuffer, alloc);
	}

	ListDLEntry_free(&dlFile->entries, alloc);
	*dlFile = (DLFile) { 0 };
	return true;
}

//Writing

Error DLFile_addEntry(DLFile *dlFile, Buffer entryBuf, Allocator alloc) {

	if(!dlFile || !dlFile->entries.ptr)
		return Error_nullPointer(0, "DLFile_addEntry()::dlFile is required");

	if(dlFile->settings.dataType != EDLDataType_Data)
		return Error_invalidOperation(0, "DLFile_addEntry() is unsupported if type isn't Data");

	DLEntry entry = { .entryBuffer = entryBuf };
	return ListDLEntry_pushBack(&dlFile->entries, entry, alloc);
}

Error DLFile_addEntryAscii(DLFile *dlFile, CharString entryStr, Allocator alloc) {

	if(!dlFile || !dlFile->entries.ptr)
		return Error_nullPointer(0, "DLFile_addEntryAscii()::dlFile is required");

	if(dlFile->settings.dataType != EDLDataType_Ascii)
		return Error_invalidOperation(0, "DLFile_addEntryAscii() is unsupported if type isn't Ascii");

	if(!CharString_isValidAscii(entryStr))
		return Error_invalidParameter(1, 0, "DLFile_addEntryAscii()::entryStr isn't valid Ascii");

	DLEntry entry = { .entryString = entryStr };
	return ListDLEntry_pushBack(&dlFile->entries, entry, alloc);
}

Error DLFile_addEntryUTF8(DLFile *dlFile, Buffer entryBuf, Allocator alloc) {

	if(!dlFile || !dlFile->entries.ptr)
		return Error_nullPointer(0, "DLFile_addEntryUTF8()::dlFile is required");

	if(dlFile->settings.dataType != EDLDataType_UTF8)
		return Error_invalidOperation(0, "DLFile_addEntryUTF8() is unsupported if type isn't UTF8");

	if(!Buffer_isUTF8(entryBuf, 1))
		return Error_invalidParameter(1, 0, "DLFile_addEntryAscii()::entryBuf isn't valid UTF8");

	DLEntry entry = { .entryBuffer = entryBuf };

	return ListDLEntry_pushBack(&dlFile->entries, entry, alloc);
}

//We currently don't support compression yet. But once Buffer_compress/uncompress is available, it should be easy.

Error DLFile_write(DLFile dlFile, Allocator alloc, Buffer *result) {

	if(!dlFile.entries.ptr)
		return Error_nullPointer(0, "DLFile_write()::dlFile is required");

	if(!result)
		return Error_nullPointer(2, "DLFile_write()::result is required");

	if(result->ptr)
		return Error_invalidOperation(0, "DLFile_write()::result wasn't empty, might indicate memleak");

	//Get header size (excluding compressedSize + entryCount)

	U64 hashSize = !dlFile.settings.compressionType ? 0 : (dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ? 32 : 4);
	U64 headerSize = sizeof(DLHeader) + hashSize;

	if(!(dlFile.settings.flags & EDLSettingsFlags_HideMagicNumber))		//Magic number (can be hidden by parent; such as oiCA)
		headerSize += sizeof(U32);

	//Get data size

	U64 outputSize = 0, maxSize = 0;

	for (U64 i = 0; i < dlFile.entries.length; ++i) {

		DLEntry entry = dlFile.entries.ptr[i];

		U64 len = 
			dlFile.settings.dataType != EDLDataType_Ascii ? Buffer_length(entry.entryBuffer) : 
			CharString_length(entry.entryString);

		if(outputSize + len < outputSize)
			return Error_overflow(0, outputSize + len, outputSize, "DLFile_write() overflow");

		outputSize += len;
		maxSize = U64_max(maxSize, len);
	}

	EXXDataSizeType dataSizeType = EXXDataSizeType_getRequiredType(maxSize);
	U8 dataSizeTypeSize = SIZE_BYTE_TYPE[dataSizeType];
	U64 entrySizes = dataSizeTypeSize * dlFile.entries.length;

	if(entrySizes / dataSizeTypeSize != dlFile.entries.length)
		return Error_overflow(0, entrySizes, entrySizes / dataSizeTypeSize, "DLFile_write() overflow (2)");

	EXXDataSizeType entrySizeType = EXXDataSizeType_getRequiredType(dlFile.entries.length);
	headerSize += SIZE_BYTE_TYPE[entrySizeType];

	U64 entrySizesOffset = headerSize;
	headerSize += entrySizes;

	if(dlFile.settings.encryptionType)
		headerSize += sizeof(I32x4) + 12;

	EXXDataSizeType uncompressedSizeType = EXXDataSizeType_getRequiredType(outputSize);

	if(dlFile.settings.compressionType)
		headerSize += SIZE_BYTE_TYPE[uncompressedSizeType];

	if(outputSize + headerSize < outputSize)
		return Error_overflow(0, outputSize + headerSize, outputSize, "DLFile_write() overflow (3)");

	//Create our final uncompressed buffer

	Buffer uncompressedData = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(outputSize + headerSize, alloc, &uncompressedData);

	if(err.genericError)
		return err;

	U8 *sizes = (U8*)uncompressedData.ptr + entrySizesOffset;
	U8 *dat = (U8*)uncompressedData.ptr + headerSize;

	for (U64 i = 0; i < dlFile.entries.length; ++i) {

		DLEntry entry = dlFile.entries.ptr[i];
		Buffer buf = 
			dlFile.settings.dataType != EDLDataType_Ascii ? entry.entryBuffer : CharString_bufferConst(entry.entryString);

		U64 len = Buffer_length(buf);

		volatile U64 t = Buffer_forceWriteSizeType(sizes + dataSizeTypeSize * i, dataSizeType, len); t;

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
			(U8)EXXDataSizeType_getRequiredType(dlFile.entries.length) | 
			((U8)EXXDataSizeType_getRequiredType(outputSize) << 2) | 
			((U8)EXXDataSizeType_getRequiredType(maxSize) << 4)
	};

	headerIt += sizeof(DLHeader);

	headerIt += Buffer_forceWriteSizeType(headerIt, entrySizeType, dlFile.entries.length);

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

	Buffer compressedOutput;

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
	
		U32 key[8] = { 0 };

		Bool b = Buffer_eq(
			Buffer_createRefConst(key, sizeof(key)), 
			Buffer_createRefConst(dlFile.settings.encryptionKey, sizeof(key))
		);

		if ((err = Buffer_encrypt(

			compressedOutput, 
			Buffer_createRefConst(uncompressedData.ptr, headerSize - sizeof(I32x4) - 12), 

			EBufferEncryptionType_AES256GCM, 
			EBufferEncryptionFlags_GenerateIv | (b ? EBufferEncryptionFlags_GenerateKey : EBufferEncryptionFlags_None),

			dlFile.settings.encryptionKey,

			(I32x4*)headerIt,
			(I32x4*)((U8*)headerIt + 12)

		)).genericError) {
			Buffer_free(&compressedOutput, alloc);
			Buffer_free(&uncompressedData, alloc);
			return err;
		}

		headerIt += 12 + sizeof(I32x4);
		headerSize += 12 + sizeof(I32x4);
	}

	//Shortcut because we didn't allocate extra memory, so our final data is already present in the buffer.
	//No need to allocate again

	if (!dlFile.settings.compressionType) {
		*result = uncompressedData;
		return Error_none();
	}

	//Compression happened, we need to merge compressed data and header.

	Buffer headerBuf = Buffer_createRefConst(uncompressedData.ptr, headerSize);

	err = Buffer_combine(headerBuf, compressedOutput, alloc, result);
	Buffer_free(&compressedOutput, alloc);
	Buffer_free(&uncompressedData, alloc);
	return err;
}

Error DLFile_read(
	Buffer file, 
	const U32 encryptionKey[8], 
	Bool isSubfile,
	Allocator alloc, 
	DLFile *dlFile
) {

	if(!dlFile)
		return Error_nullPointer(2, "DLFile_read()::dlFile is required");

	if(dlFile->entries.ptr)
		return Error_invalidOperation(0, "DLFile_read()::dlFile is empty, might indicate memleak");

	Buffer entireFile = file;

	file = Buffer_createRefFromBuffer(file, true);
	Error err = Error_none();

	if (!isSubfile) {

		U32 magic;
		_gotoIfError(clean, Buffer_consume(&file, &magic, sizeof(magic)));
		
		if(magic != DLHeader_MAGIC)
			_gotoIfError(clean, Error_invalidParameter(0, 0, "DLFile_read() requires magicNumber prefix"));
	}

	//Read from binary

	DLHeader header;
	_gotoIfError(clean, Buffer_consume(&file, &header, sizeof(header)));

	//Validate header

	if(header.version != DLHeader_V1_0)
		_gotoIfError(clean, Error_invalidParameter(0, 1, "DLFile_read() header.version is invalid"));

	if(header.flags & (EDLFlags_UseAESChunksA | EDLFlags_UseAESChunksB))		//TODO: AES chunks
		_gotoIfError(clean, Error_unsupportedOperation(0, "DLFile_read() AES chunks not supported yet"));

	if(header.type >> 4)								//TODO: Compression
		_gotoIfError(clean, Error_unsupportedOperation(1, "DLFile_read() compression not supported yet"));

	if(header.flags & EDLFlags_UseSHA256)				//TODO: SHA256
		_gotoIfError(clean, Error_unsupportedOperation(2, "DLFile_read() SHA256 not supported yet"));

	if((header.type & 0xF) >= EXXEncryptionType_Count)
		_gotoIfError(clean, Error_invalidParameter(0, 4, "DLFile_read() invalid encryption type"));

	if(header.sizeTypes >> 6)
		_gotoIfError(clean, Error_invalidParameter(0, 7, "DLFile_read() header.sizeTypes is invalid"));

	//Ensure encryption key isn't provided if we're not encrypting

	if(encryptionKey && !(header.type & 0xF))
		_gotoIfError(clean, Error_invalidOperation(3, "DLFile_read() encryptionKey is provided but no encryption is used"));

	if(!encryptionKey && (header.type & 0xF))
		_gotoIfError(clean, Error_unauthorized(0, "DLFile_read() encryptionKey is required"));

	//

	EXXDataSizeType entrySizeType			= (EXXDataSizeType)(header.sizeTypes & 3);
	EXXDataSizeType uncompressedSizeType	= (EXXDataSizeType)((header.sizeTypes >> 2) & 3);
	EXXDataSizeType dataSizeType			= (EXXDataSizeType)(header.sizeTypes >> 4);

	//Entry size

	U64 entryCount;
	_gotoIfError(clean, Buffer_consumeSizeType(&file, entrySizeType, &entryCount));

	//Extending header

	U64 entrySizeExtension = 0;

	if (header.flags & EDLFlags_HasExtendedData) {

		DLExtraInfo extraInfo;
		_gotoIfError(clean, Buffer_consume(&file, &extraInfo, sizeof(extraInfo)));

		entrySizeExtension = extraInfo.perEntryExtendedData;

		//We could do something here for new version parsing or extensions.

		_gotoIfError(clean, Buffer_consume(&file, NULL, extraInfo.extendedHeader));
	}

	//Entry counts

	U64 entryStride = entrySizeExtension + SIZE_BYTE_TYPE[dataSizeType];

	const U8 *entryStart = file.ptr;
	_gotoIfError(clean, Buffer_consume(&file, NULL, entryStride * entryCount));

	U64 dataSize = 0;

	for (U64 i = 0; i < entryCount; ++i) {

		const U8 *entryi = entryStart + i * entryStride;

		U64 entryLen = Buffer_forceReadSizeType(entryi, dataSizeType);

		if(dataSize + entryLen < dataSize)
			return Error_overflow(0, dataSize + entryLen, U64_MAX, "DLFile_read() overflow");

		dataSize += entryLen;
	}

	//Uncompressed size

	if (header.type >> 4) {

		U64 uncompressedSize;
		_gotoIfError(clean, Buffer_consumeSizeType(&file, uncompressedSizeType, &uncompressedSize));

		//TODO: Decompression

		//TODO: Hash
	}

	//Unencrypt

	if (header.type & 0xF) {

		if(Buffer_isConstRef(entireFile))
			return Error_constData(0, 0, "DLFile_read() file needs to be writable to allow decryption");

		//Get tag and iv

		Buffer headerExEnc = file;

		I32x4 iv = I32x4_zero();
		_gotoIfError(clean, Buffer_consume(&file, &iv, 12));

		I32x4 tag;
		_gotoIfError(clean, Buffer_consume(&file, &tag, sizeof(I32x4)));

		//Validate remainder

		if(Buffer_length(file) < dataSize)
			_gotoIfError(
				clean, Error_outOfBounds(
					0, file.ptr + dataSize - entireFile.ptr, Buffer_length(entireFile), 
					"DLFile_read() doesn't contain enough data"
				)
			);

		if(!isSubfile && Buffer_length(file) != dataSize)
			_gotoIfError(clean, Error_invalidState(
				0, "DLFile_read() contained extra data, which isn't allowed if it's not a subfile"
			));

		//Decrypt

		_gotoIfError(clean, Buffer_decrypt(
			Buffer_createRef((U8*)file.ptr, dataSize),
			Buffer_createRefConst(entireFile.ptr, headerExEnc.ptr - entireFile.ptr),
			EBufferEncryptionType_AES256GCM,
			encryptionKey,
			tag,
			iv
		));
	}

	//Allocate DLFile

	DLSettings settings = (DLSettings) { 

		.compressionType = (EXXCompressionType) (header.type >> 4),
		.encryptionKey = (EXXEncryptionType) (header.type & 0xF),

		.dataType = header.flags & EDLFlags_UTF8 ? EDLDataType_UTF8 : (
			header.flags & EDLFlags_IsString ? EDLDataType_Ascii : EDLDataType_Data
		),

		.flags = header.flags & EDLFlags_UseSHA256 ? EDLSettingsFlags_UseSHA256 : EDLSettingsFlags_None
	};

	//Not copying encryption key, because you probably don't want to store it.
	//And you already have it.

	//Create DLFile

	_gotoIfError(clean, DLFile_create(settings, alloc, dlFile));

	//Per entry

	for (U64 i = 0; i < entryCount; ++i) {

		const U8 *entryi = entryStart + i * entryStride;

		U64 entryLen = Buffer_forceReadSizeType(entryi, dataSizeType);

		const U8 *ptr = file.ptr;
		_gotoIfError(clean, Buffer_consume(&file, NULL, entryLen));

		Buffer buf = Buffer_createRefConst(ptr, entryLen);

		switch(settings.dataType) {

			case EDLDataType_Data:	err = DLFile_addEntry(dlFile, buf, alloc);		break;
			case EDLDataType_UTF8:	err = DLFile_addEntryUTF8(dlFile, buf, alloc);	break;

			case EDLDataType_Ascii:	
				err = DLFile_addEntryAscii(dlFile, CharString_createRefSizedConst((const C8*)ptr, entryLen, false), alloc);
				break;
		}

		_gotoIfError(clean, err);
	}

	if(!isSubfile && Buffer_length(file))
		_gotoIfError(clean, Error_invalidState(1, "DLFile_read() contained extra data, not allowed if it's not a subfile"));

	if(file.ptr)
		dlFile->readLength = file.ptr - entireFile.ptr;

	else dlFile->readLength = Buffer_length(entireFile);

	return Error_none();

clean:
	DLFile_free(dlFile, alloc);
	return err;
}
