/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//formats/oiDL/dl_read.c

#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_headers.h"
#include "formats/oiDL/dl_entry.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/container/encryption_stream.h"
#include "types/container/buffer_encrypt.h"
#include "types/base/constants.h"
#include "types/base/mathi.h"
#include "types/math/vec4i.h"

Bool DLFile_read(
	StreamRef *file,
	U64 *startOffset,
	const U32 encryptionKey[8],
	I32x4 iv,
	Bool isSubFile,
	Bool forceKeepInStreams,
	const Allocator *alloc,
	const RefPtrType *encryptionStreamType,
	DLFile *dlFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocate = false;

	I32x4 tag = I32x4_zero();
	StreamCursor cursor = (StreamCursor) { 0 };
	StreamCursor cursorEntry = (StreamCursor) { 0 };
	Buffer tmp = Buffer_createNull();
	StreamRef *dataStream = NULL;

	if (!isSubFile)
		iv = I32x4_zero();

	if (!dlFile)
		retError(clean, Error_nullPointer(2, "DLFile_read()::dlFile is required"));

	if(!file || !startOffset)
		retError(clean, Error_nullPointer(!file ? 0 : 1, "DLFile_read()::file is required"));

	if(DLFile_isAllocated(dlFile))
		retError(clean, Error_invalidOperation(0, "DLFile_read()::dlFile isn't empty, might indicate memleak"));

	if (*startOffset & 15)
		retError(clean, Error_unsupportedOperation(0, "DLFile_read() at misaligned startOffset is unsupported (16-byte)"));

	gotoIfError3(clean, StreamCursor_create(file, 0, false, alloc, &cursor, e_rr));

	OxStream *stream = RefPtr_data(file, OxStream);

	U64 streamOff = *startOffset;

	if (!isSubFile) {

		U32 magic;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, &streamOff, &magic, alloc, e_rr));

		if(magic != DLHeader_MAGIC)
			retError(clean, Error_invalidParameter(0, 0, "DLFile_read() requires magicNumber prefix"));
	}

	//Read from binary

	DLHeader header;
	gotoIfError3(clean, StreamCursor_consume(&cursor, &streamOff, &header, sizeof(header), alloc, e_rr));

	EXXDataSizeType entrySizeType = (EXXDataSizeType)(header.sizeTypes & 3);
	//EXXDataSizeType uncompressedSizeType = (EXXDataSizeType)((header.sizeTypes >> 2) & 3);
	EXXDataSizeType dataSizeType = (EXXDataSizeType)(header.sizeTypes >> 4);

	//Validate header

	if(header.version != EDLVersion_V1_0)
		retError(clean, Error_invalidParameter(0, 1, "DLFile_read() header.version is invalid"));

	if(header.type >> 4)                                //TODO: Compression
		retError(clean, Error_unsupportedOperation(1, "DLFile_read() compression not supported yet"));

	if((header.type & 0xF) >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 4, "DLFile_read() invalid encryption type"));

	if(header.sizeTypes >> 6)
		retError(clean, Error_invalidParameter(0, 7, "DLFile_read() header.sizeTypes is invalid"));

	//Ensure encryption key isn't provided if we're not encrypting

	if(encryptionKey && !(header.type & 0xF))
		retError(clean, Error_invalidOperation(3, "DLFile_read() encryptionKey is provided but no encryption is used"));

	if(!encryptionKey && (header.type & 0xF))
		retError(clean, Error_unauthorized(0, "DLFile_read() encryptionKey is required"));

	//Entry size

	U64 entryCount;
	gotoIfError3(clean, StreamCursor_consumeSizeType(&cursor, &streamOff, entrySizeType, &entryCount, alloc, e_rr));

	//Extending header

	U64 entrySizeExtension = 0;

	if (header.flags & EDLFlags_HasExtendedData) {

		DLExtraInfo extraInfo;
		gotoIfError3(clean, StreamCursor_consume(&cursor, &streamOff, &extraInfo, sizeof(extraInfo), alloc, e_rr));

		entrySizeExtension = extraInfo.perEntryExtendedData;

		//We could do something here for new version parsing or extensions.

		streamOff += extraInfo.extendedHeader;
	}

	//Entry counts

	U64 entryStride = entrySizeExtension + SIZE_BYTE_TYPE[dataSizeType];

	U64 entryStart = streamOff;

	U64 dataSize = 0;
	U64 allDataLeq32KiB = 0;

	for (U64 i = 0; i < entryCount; ++i) {

		U64 entryLen;
		gotoIfError3(clean, StreamCursor_consumeSizeType(&cursor, &streamOff, dataSizeType, &entryLen, alloc, e_rr));

		if (dataSize + entryLen < dataSize)
			retError(clean, Error_overflow(0, dataSize + entryLen, U64_MAX, "DLFile_read() overflow"));

		if (entryLen <= DLFile_smallLen)
			allDataLeq32KiB += entryLen;

		dataSize += entryLen;
	}

	//Decrypt

	U64 chunkSize = 0;            //No chunkSize by default (32KiB for stream cursors)
	Bool isEncrypted = header.type & 0xF;

	U8 chunkSize2 = 0;

	if (header.flags & EDLFlags_UseAESChunksA)
		chunkSize2 |= 1;

	if (header.flags & EDLFlags_UseAESChunksB)
		chunkSize2 |= 2;

	U64 fileStart;

	if (isEncrypted) {

		U64 startHeader = *startOffset;
		U64 endHeader = streamOff;

		//Get tag and iv

		if (!isSubFile)
			gotoIfError3(clean, StreamCursor_consume(&cursor, &streamOff, &iv, 12, alloc, e_rr));

		gotoIfError3(clean, StreamCursor_consume(&cursor, &streamOff, &tag, sizeof(I32x4), alloc, e_rr));

		//Validate remainder

		streamOff = (streamOff + 15) &~ 15;

		chunkSize = DLHeader_chunkSizes[chunkSize2];

		U64 realDataSize = EncryptionStream_underlyingSize(chunkSize, dataSize);

		if (streamOff + realDataSize > stream->size)
			retError(clean, Error_outOfBounds(
				0, streamOff + realDataSize, stream->size, "DLFile_read() doesn't contain enough data"
			));

		//Copy previously loaded data to run AAD over.
		//We do this so that parts of the oiDL can be decrypted rather than requiring the whole thing to be.
		//This only happens if the oiDL can't fit neatly into the cursor's cache, otherwise we can just grab it from there.

		if (StreamCursor_contains(&cursor, startHeader, endHeader - startHeader))
			tmp = Buffer_createRefConst(cursor.cacheData.ptr + (startHeader - cursor.lastLocation), endHeader - startHeader);

		else {
			gotoIfError3(clean, Buffer_createUninitializedBytes(endHeader - startHeader, alloc, &tmp, e_rr));
			gotoIfError3(clean, StreamCursor_consume(
				&cursor, &startHeader, tmp.ptrNonConst, endHeader - startHeader, alloc, e_rr
			));
		}

		gotoIfError3(clean, Buffer_decryptAuto(
			NULL,                //Instead of decrypting on the fly, we only validate AAD
			&tmp,
			encryptionKey,
			tag,
			iv,
			e_rr
		));

		tag = I32x4_zero();

		gotoIfError3(clean, EncryptionStream_create(
			file,
			streamOff,
			encryptionKey,
			iv,
			chunkSize,
			dataSize,
			encryptionStreamType,
			&dataStream,
			e_rr
		));

		fileStart = 0;        //We're now relative to the encryption stream

		Buffer_free(&tmp, alloc);
	}

	else {
		streamOff = (streamOff + 15) & ~15;
		fileStart = streamOff;
		dataStream = file;
		RefPtr_inc(dataStream);
	}

	//Allocate DLFile

	DLSettings settings = (DLSettings) {
		.compressionType = (XXCompressionType) (header.type >> 4),
		.encryptionType = (XXEncryptionType) (header.type & 0xF),
		.dataType = header.flags & EDLFlags_IsString ? EDLDataType_String : EDLDataType_Data,
		.flags = EDLSettingsFlags_None,
		.chunkSize = (U32)chunkSize
	};

	if (!chunkSize)
		chunkSize = 32 * KIBI - sizeof(CryptoChunk);        //Ensure we get exactly 32 KiB cache size

	//If we have any lazy entries we will need the encryption keys for later.
	//Copy them.
	//(Lazy entries are always possible if they span multiple chunks)

	if (isEncrypted) {

		Buffer_memcpy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRefConst(encryptionKey, sizeof(settings.encryptionKey))
		);

		Buffer_memcpy(
			Buffer_createRef(settings.iv, sizeof(settings.iv)),
			Buffer_createRefConst(&iv, sizeof(iv))
		);
	}

	//Create DLFile
	//Allocation strategy:
	//Small allocations land into the cache for up to 1MiB (when entry <= DLFile_smallLen).
	//Medium allocations land into their own allocation up to 4MiB (when entry <= DLFile_medLen but not small).
	//Large allocations remain in stream.
	//This means that by default, a DLFile's data can take up only a max of 5MiB (excluding metadata).

	gotoIfError3(clean, DLFile_create(&settings, U64_min(allDataLeq32KiB, MIBI), alloc, dlFile, e_rr));
	allocate = true;

	//Per entry
	//Either:    (U8 entries[])[N] or
	//            chunks[N] where chunk:
	//                I32x4 tag, U8 data[chunkSize] with iv = root iv + chunkId

	U64 dataOff = fileStart;

	for (U64 i = 0, cacheOff = 0, allocCounter = 0; i < entryCount; ++i) {

		U64 entryi = entryStart + i * entryStride;
		U64 entryLen;
		gotoIfError3(clean, StreamCursor_consumeSizeType(&cursor, &entryi, dataSizeType, &entryLen, alloc, e_rr));

		Bool isSmallAlloc = cacheOff < 1 * MIBI && entryLen <= DLFile_smallLen;
		Bool isMediumAlloc = allocCounter < 4 * MIBI && entryLen <= DLFile_medLen && entryLen > DLFile_smallLen;

		if ((!isSmallAlloc && !isMediumAlloc) || forceKeepInStreams) {
			RefPtr *ptr = dataStream;
			RefPtr_inc(ptr);
			gotoIfError3(clean, DLFile_addEntryStream(dlFile, &ptr, dataOff, entryLen, alloc, e_rr));
			dataOff += entryLen;
			continue;
		}

		//Find seek offset

		//We'll create a new cursor if our previous cursor is too small,
		// because the previous cursor is constantly looking at entryStart + i * entryStride

		Bool isInPrimaryCursor = !isEncrypted && StreamCursor_contains(&cursor, dataOff, entryLen);

		if (!isInPrimaryCursor && !cursorEntry.cacheData.ptr)
			gotoIfError3(clean, StreamCursor_create(
				dataStream, chunkSize + sizeof(CryptoChunk), false, alloc, &cursorEntry, e_rr
			));

		StreamCursor *dataCursor = isInPrimaryCursor ? &cursor : &cursorEntry;

		//Allocate space somewhere, into tmp

		if (isSmallAlloc) {

			//An oiDL whose small entries are all zero length asks for no cache at all, so the base stays null
			// and offsetting it is undefined even by the zero cacheOff is then.
			//A null ref of length 0 is what Buffer_createRef would have produced anyway.

			tmp = Buffer_length(dlFile->cache)
				? Buffer_createRef(dlFile->cache.ptrNonConst + cacheOff, entryLen)
				: Buffer_createNull();

			cacheOff += entryLen;
		}

		else {
			gotoIfError3(clean, Buffer_createUninitializedBytes(entryLen, alloc, &tmp, e_rr));
			allocCounter += entryLen;
		}

		//Load/decrypt data

		gotoIfError3(clean, StreamCursor_read(dataCursor, tmp, dataOff, 0, entryLen, false, alloc, e_rr));
		dataOff += entryLen;

		switch(settings.dataType) {

			case EDLDataType_Data:
				gotoIfError3(clean, DLFile_addEntry(dlFile, &tmp, alloc, e_rr));
				break;

			case EDLDataType_String:
			default: {
				CharString str = CharString_createRefSizedConst((const C8*)tmp.ptr, entryLen, false);
				gotoIfError3(clean, DLFile_addEntryString(dlFile, &str, alloc, e_rr));
				tmp = Buffer_createNull();
				break;
			}
		}
	}

	streamOff += isEncrypted ? EncryptionStream_underlyingSize(chunkSize, dataSize) : dataSize;

	if(!isSubFile && streamOff != stream->size)
		retError(clean, Error_invalidState(1, "DLFile_read() contained extra data, not allowed if it's not a subfile"));

	*startOffset = streamOff;

clean:

	if (!s_uccess && allocate)
		DLFile_free(dlFile, alloc);

	if (dataStream)
		RefPtr_dec(&dataStream);

	StreamCursor_close(&cursorEntry, alloc);
	StreamCursor_close(&cursor, alloc);
	Buffer_free(&tmp, alloc);

	iv = I32x4_zero();
	tag = I32x4_zero();

	return s_uccess;
}
