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
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/math/math.h"

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

		.version = EDLVersion_V1_0,

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
