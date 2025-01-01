/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "formats/oiDL/dl_file.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"

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

	I32x4 iv = I32x4_zero();
	I32x4 tag = I32x4_zero();

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

	if(header.version != EDLVersion_V1_0)
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

		gotoIfError2(clean, Buffer_consume(&file, &iv, 12))
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

		iv = I32x4_zero();
		tag = I32x4_zero();
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

	iv = I32x4_zero();
	tag = I32x4_zero();

	if(!s_uccess && allocate)
		DLFile_free(dlFile, alloc);

	return s_uccess;
}
