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

#include "formats/oiDL/dl_file.h"
#include "types/container/buffer.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/ref_ptr.h"
#include "types/container/stream.h"
#include "types/container/types.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/mathi.h"

//We currently don't support compression yet. But once Buffer_compress/decompress is available, it should be easy.

Bool DLFile_write(const DLFile *dlFile, const Allocator *alloc, StreamRef *streamRef, U64 *startOffset, Error *e_rr) {

	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };
	Buffer tmp = Buffer_createNull();
	Buffer loadCache = Buffer_createNull();

	if(!DLFile_isAllocated(dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_write()::dlFile is required"));

	if(!startOffset)
		retError(clean, Error_nullPointer(2, "DLFile_write()::startOffset is required"));

	if(!streamRef || streamRef->refPtrType->typeId != EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(2, "DLFile_write()::streamRef is required"));

	Stream *stream = RefPtr_data(streamRef, Stream);

	if (dlFile->settings.compressionType)		//TODO: Compression
		retError(clean, Error_unsupportedOperation(0, "DLFile_write() doesn't support compression yet"));

	if(dlFile->settings.chunkSize && !dlFile->settings.encryptionType)
		retError(clean, Error_unsupportedOperation(0, "DLFile_write() had chunkSize but no encryption"));

	if(*startOffset & 15)
		retError(clean, Error_unsupportedOperation(0, "DLFile_write() at misaligned startOffset is unsupported (16-byte)"));

	U64 totalSize = 0, maxSize = 0;
	U64 entryCount = DLFile_entryCount(dlFile);

	if (entryCount >> 48)
		retError(clean, Error_outOfBounds(0, entryCount, (U64)1 << 48, "DLFile_write() entryCount out of bounds"));

	Bool isPartiallyLoaded = false;

	for (U64 i = 0; i < entryCount; ++i) {

		U64 l = DLFile_entrySize(dlFile, i);
		maxSize = U64_max(maxSize, l);

		if (!DLFile_isFullyLoaded(dlFile, i))
			isPartiallyLoaded = true;

		if (totalSize + l < totalSize)
			retError(clean, Error_overflow(0, totalSize + l, totalSize, "DLFile_write() overflow"));

		totalSize += l;
	}

	if (totalSize >> 48)
		retError(clean, Error_outOfBounds(0, totalSize, (U64)1 << 48, "DLFile_write() totalSize out of bounds"));

	U64 chunkSize = 0;
	U8 chunkSize2 = 0;
	EDLFlags dlFlags = EDLFlags_None;
	Bool isEncrypted = dlFile->settings.encryptionType;

	if (isEncrypted) {

		if (dlFile->settings.chunkSize)
			chunkSize = dlFile->settings.chunkSize;

		//Auto detect chunk size based on total entry size.
		//In this case:
		//>=8MiB switches to 1MiB chunks
		//>=64MiB switches to 8MiB chunks
		//64MiB chunks aren't used for memory reasons.

		else {

			chunkSize = DLHeader_chunkSizes[0];

			for (U8 i = 1; i < 3; ++i) {

				if (totalSize <= 8 * DLHeader_chunkSizes[i])
					break;

				chunkSize = DLHeader_chunkSizes[i];
			}
		}

		U8 i = 0;

		for (; i < 4; ++i)
			if (chunkSize == DLHeader_chunkSizes[i]) {
				chunkSize2 = i;
				break;
			}

		if (i == 4)
			retError(clean, Error_unsupportedOperation(0, "DLFile_write() had invalid chunkSize"));

		if (chunkSize2 & 1)
			dlFlags |= EDLFlags_UseAESChunksA;

		if (chunkSize2 & 2)
			dlFlags |= EDLFlags_UseAESChunksB;
	}

	if (!chunkSize)
		chunkSize = 32 * KIBI;

	//Add the chunks

	if (isEncrypted)
		totalSize += ((totalSize + chunkSize - 1) >> DLHeader_chunkSizesShifts[chunkSize2]) * sizeof(CryptoChunk);

	//Get header size

	U64 headerSize = sizeof(DLHeader);

	if(!(dlFile->settings.flags & EDLSettingsFlags_HideMagicNumber))	//Magic number (can be hidden by parent; such as oiCA)
		headerSize += sizeof(U32);

	//Get data size

	const EXXDataSizeType dataSizeType = EXXDataSizeType_getRequiredType(maxSize);
	const U8 dataSizeTypeSize = SIZE_BYTE_TYPE[dataSizeType];

	const EXXDataSizeType entrySizeType = EXXDataSizeType_getRequiredType(entryCount);
	headerSize += SIZE_BYTE_TYPE[entrySizeType];

	const U64 entrySizesOffset = headerSize;
	headerSize += dataSizeTypeSize * entryCount;

	if (isEncrypted) {
		headerSize += sizeof(I32x4) + 12;		//Tag for AAD and IV
		headerSize += (16 - ((*startOffset + headerSize) & 15)) & 15;	//Alignment
	}

	//Add to stream

	gotoIfError3(clean, Stream_reserve(stream, *startOffset + headerSize + totalSize, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_create(streamRef, chunkSize + sizeof(CryptoChunk), true, alloc, &cursor, e_rr));
	
	Bool isString = dlFile->settings.dataType == EDLDataType_String;

	if (isString)
		dlFlags |= EDLFlags_IsString;

	U64 start = *startOffset;

	DLHeader header = (DLHeader) {
		.version = EDLVersion_V1_0,
		.flags = (U8)dlFlags,
		.type = (U8)dlFile->settings.encryptionType,
		.sizeTypes = (U8)entrySizeType | ((U8)dataSizeType << 4)
	};

	if (!(dlFile->settings.flags & EDLSettingsFlags_HideMagicNumber))
		gotoIfError3(clean, StreamCursor_appendU32(&cursor, startOffset, DLHeader_MAGIC, alloc, e_rr));

	gotoIfError3(clean, StreamCursor_append(&cursor, startOffset, &header, sizeof(header), alloc, e_rr));

	gotoIfError3(clean, StreamCursor_appendSizeType(&cursor, startOffset, entryCount, entrySizeType, alloc, e_rr));

	for (U64 i = 0; i < entryCount; ++i) {
		U64 l = DLFile_entrySize(dlFile, i);
		gotoIfError3(clean, StreamCursor_appendSizeType(&cursor, startOffset, l, dataSizeType, alloc, e_rr));
	}

	//Encryption header

	if (isEncrypted) {

		if (StreamCursor_contains(&cursor, start, *startOffset - start)) {
			tmp = Buffer_createRefFromBuffer(cursor.cacheData, true);
			gotoIfError3(clean, Buffer_offset(&tmp, start - cursor.lastLocation, e_rr));
		}

		//Slow path, because our region is out of cache, so we need to flush to stream and readback

		else {

			gotoIfError3(clean, Buffer_createUninitializedBytes(*startOffset - start, alloc, &tmp, e_rr));

			gotoIfError3(clean, StreamCursor_setReadOnly(&cursor, alloc, e_rr));	//Flush to make changes visible

			U64 where = start;
			gotoIfError3(clean, StreamCursor_consume(&cursor, &where, tmp.ptr, *startOffset - start, alloc, e_rr));

			gotoIfError3(clean, StreamCursor_setWritable(&cursor, alloc, e_rr));	//Return to writable
		}

		const U32 key[8] = { 0 };

		const Bool hasKey = Buffer_neq(
			Buffer_createRefConst(dlFile->settings.encryptionKey, sizeof(key)),
			Buffer_createRefConst(key, sizeof(key))
		);

		I32x4 iv = I32x4_zero(), tag = I32x4_zero();
		
		gotoIfError3(clean, Buffer_encryptAuto(

			NULL,
			&tmp,

			!hasKey,
			dlFile->settings.encryptionKey,

			&tag,
			&iv,
			e_rr
		));

		Buffer_free(&tmp, alloc);

		gotoIfError3(clean, StreamCursor_append(&cursor, startOffset, &iv, 3 * sizeof(U32), alloc, e_rr));
		gotoIfError3(clean, StreamCursor_append(&cursor, startOffset, &tag, sizeof(tag), alloc, e_rr));

		*startOffset = (*startOffset + 15) & ~15;		//Align to 16-byte

		//We need our stream cursor to be reset to the first chunk, since we'll do the encryption in our cache.

		gotoIfError3(clean, StreamCursor_flush(&cursor, alloc, e_rr));
	}

	//Contents

	if (isPartiallyLoaded)
		gotoIfError3(clean, Buffer_createUninitializedBytes(chunkSize, alloc, &loadCache, e_rr));

	for (U64 i = 0; i < entryCount; ++i) {

		U64 siz = DLFile_entrySize(dlFile, i);
		Bool isFullyLoaded = DLFile_isFullyLoaded(dlFile, i);
		StreamCursor inputStream = (StreamCursor) { 0 };

		DLEntryStream stream = dlFile->entryStreams.ptr[i];

		if (!isFullyLoaded)
			gotoIfError3(clean, StreamCursor_create(stream.stream, &loadCache, false, alloc, &inputStream, e_rr));

		//No encryption it's easy, we'll just append the entire thing at once

		//Copy from buffer/string

		if (isFullyLoaded) {
			const void *ptr = isString ? dlFile->entryStrings.ptr[i].ptr : dlFile->entryBuffers.ptr[i].ptr;
			gotoIfError3(clean, StreamCursor_append(&cursor, startOffset, ptr, siz, alloc, e_rr));
		}

		//Copy from stream
		else {

			gotoIfError3(clean, StreamCursor_copyStream(
				&cursor, &inputStream, stream.dataOff + stream.startOff, *startOffset, siz, alloc, e_rr
			));

			*startOffset += siz;
		}

		//Close, but preserve the allocation for next time

		if (!isFullyLoaded)
			gotoIfError3(clean, StreamCursor_closeAndKeepCache(&inputStream, alloc, &loadCache, e_rr));
	}

clean:
	Buffer_free(&loadCache, alloc);
	Buffer_free(&tmp, alloc);
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
