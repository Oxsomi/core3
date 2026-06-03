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

//formats/oiCA/ca_read.c

#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/stream.h"
#include "types/container/container_types.h"
#include "types/container/list_basic_types.h"
#include "types/math/vec4i.h"
#include "types/base/time.h"

Ns CAFile_loadDate(U16 time, U16 date) {

	Date d = (Date) {
		.year = 1980 + (date >> 11),
		.month = (date >> 5) & 0xF,
		.day = date & 0x1F,
		.hour = time >> 11,
		.minute = (time >> 5) & 0x3F,
		.second = (time & 0x1F) << 1
	};

	return Time_date(&d, false);
}

void DLFile_prepMoveStringRef(DLFile *src, CharString *str, DLFile *dst) {

	Bool isConst = CharString_isConstRef(*str);
	Buffer contentBuf = isConst ? CharString_bufferConst(*str) : CharString_buffer(*str);
	U64 bufl = Buffer_length(contentBuf);

	//Move directly into our pre-allocated cache buffer (we know the size from the prev DLFile load)
	if (contentBuf.ptr >= src->cache.ptr && contentBuf.ptr < src->cache.ptr + Buffer_length(src->cache)) {

		Buffer subArea = Buffer_createRef(dst->cache.ptrNonConst + (contentBuf.ptr - src->cache.ptr), bufl);
		Buffer_memcpy(subArea, contentBuf);

		*str =
			isConst ? CharString_createRefSizedConst((const C8*)subArea.ptr, bufl, false) :
			CharString_createRefSized((C8*)subArea.ptrNonConst, bufl, false);
	}

	//Else accept string as is
}

Bool CAFile_read(
	StreamRef *file,
	const RefPtrType *encStreamType,
	U64 startOffset,
	const U32 encryptionKey[8],
	const Allocator *alloc,
	CAFile *caFile,
	Error *e_rr
) {
	Bool s_uccess = true;
	Bool allocated = false;

	StreamCursor cursor = (StreamCursor) { 0 };
	Buffer tmp = Buffer_createNull();
	ListU16 dirParents = (ListU16) { 0 };                //mem-space parent index per directory
	ListCAFileInfo fileMetas = (ListCAFileInfo) { 0 };    //packed parent + timestamp per file
	ListU16 dirHandles = (ListU16) { 0 };                //disk index -> live handle map
	I32x4 iv = I32x4_zero();
	DLFile names = (DLFile) { 0 };
	DLFile content = (DLFile) { 0 };

	if (!file || file->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(0, "CAFile_read()::file must be a valid StreamRef"));

	if (!caFile)
		retError(clean, Error_nullPointer(4, "CAFile_read()::caFile is required"));

	if (caFile->folders.ptr)
		retError(clean, Error_invalidOperation(0, "CAFile_read()::caFile isn't empty, might indicate memleak"));

	if (startOffset & 15)
		retError(clean, Error_unsupportedOperation(0, "CAFile_read() at misaligned startOffset is unsupported (16-byte)"));

	//Read the fixed header first with a reasonably sized cursor cache

	gotoIfError3(clean, StreamCursor_create(file, 32 * KIBI, false, alloc, &cursor, e_rr));

	U64 readOffset = startOffset;

	//CAHeader

	CAHeader header;
	gotoIfError3(clean, StreamCursor_consume(&cursor, &readOffset, &header, sizeof(CAHeader), alloc, e_rr));

	if (header.magicNumber != CAHeader_MAGIC)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_read()::invalid magic number, not an oiCA file"));

	if (header.version != (U8)ECAVersion_V1_0)
		retError(clean, Error_invalidParameter(0, 1, "CAFile_read()::unsupported oiCA version"));

	if (header.type >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 2, "CAFile_read()::invalid encryptionType"));

	//Parse flags

	U16 flags = header.flags;

	Bool hasDate         = flags & ECAFlags_FilesHaveDate;
	Bool hasExtendedDate = flags & ECAFlags_FilesHaveExtendedDate;
	Bool dirCountLong    = flags & ECAFlags_DirectoriesCountLong;
	Bool fileCountLong   = flags & ECAFlags_FilesCountLong;

	if (hasExtendedDate)
		hasDate = true;

	EXXEncryptionType encryptionType = (EXXEncryptionType) header.type;
	Bool isEncrypted = encryptionType != EXXEncryptionType_None;

	if (isEncrypted && !encryptionKey)
		retError(clean, Error_unauthorized(0, "CAFile_read()::encryptionKey is required for encrypted files"));

	//Extended header: forwards compatibility
	
	U8 dirExtSize  = 0;
	U8 fileExtSize = 0;

	if (flags & ECAFlags_HasExtendedData) {

		CAExtraInfo extraInfo;
		gotoIfError3(clean, StreamCursor_consume(&cursor, &readOffset, &extraInfo, sizeof(CAExtraInfo), alloc, e_rr));

		dirExtSize  = extraInfo.directoryExtensionSize;
		fileExtSize = extraInfo.fileExtensionSize;

		//Skip any header extension bytes we don't understand
		readOffset += extraInfo.headerExtensionSize;
	}

	//fileCount and dirCount

	U64 fileCount = 0;
	U64 dirCount  = 0;

	if (fileCountLong) {
		U32 fc = 0;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, &readOffset, &fc, alloc, e_rr));
		fileCount = fc;
	} else {
		U16 fc = 0;
		gotoIfError3(clean, StreamCursor_consumeU16(&cursor, &readOffset, &fc, alloc, e_rr));
		fileCount = fc;
	}

	if (dirCountLong) {
		U16 dc = 0;
		gotoIfError3(clean, StreamCursor_consumeU16(&cursor, &readOffset, &dc, alloc, e_rr));
		dirCount = dc;
	} else {
		U8 dc = 0;
		gotoIfError3(clean, StreamCursor_consumeU8(&cursor, &readOffset, &dc, alloc, e_rr));
		dirCount = dc;
	}

	if (dirCount >= U16_MAX)
		retError(clean, Error_outOfBounds(0, dirCount, U16_MAX, "CAFile_read()::too many directories"));

	if (fileCount >= U32_MAX)
		retError(clean, Error_outOfBounds(0, fileCount, U32_MAX, "CAFile_read()::too many files"));
	
	U16 sentinelDir = dirCountLong ? (U16)0xFFFF : (U16)0xFF;

	//Pass 1: read directories[] into a ListU16 of memory-space parent indices.
	//Actual CAFile_addFolder calls happen in pass 2, once the names DLFile is available.

	gotoIfError3(clean, ListU16_reserve(&dirParents, dirCount, alloc, e_rr));

	for (U64 i = 0; i < dirCount; ++i) {

		U16 parentDisk = 0;

		if (dirCountLong) {
			gotoIfError3(clean, StreamCursor_consumeU16(&cursor, &readOffset, &parentDisk, alloc, e_rr));
		} else {
			U8 p = 0;
			gotoIfError3(clean, StreamCursor_consumeU8(&cursor, &readOffset, &p, alloc, e_rr));
			parentDisk = p;
		}

		if (parentDisk != sentinelDir && parentDisk >= i)
			retError(clean, Error_invalidState(0,
				"CAFile_read()::directory references a parent not yet defined"
			));

		//Convert: sentinel -> 0 (root), else disk index + 1
		U16 parentMem = (parentDisk == sentinelDir) ? 0 : (U16)(parentDisk + 1);
		gotoIfError3(clean, ListU16_pushBack(&dirParents, parentMem, alloc, e_rr));

		readOffset += dirExtSize;
	}

	//Pass 1: read files[] into a ListCAFileInfo which packs parent mem-index + timestamp.

	gotoIfError3(clean, ListCAFileInfo_reserve(&fileMetas, fileCount, alloc, e_rr));

	for (U64 i = 0; i < fileCount; ++i) {

		U16 parentDisk = 0;

		if (dirCountLong) {
			gotoIfError3(clean, StreamCursor_consumeU16(&cursor, &readOffset, &parentDisk, alloc, e_rr));
		} else {
			U8 p = 0;
			gotoIfError3(clean, StreamCursor_consumeU8(&cursor, &readOffset, &p, alloc, e_rr));
			parentDisk = p;
		}

		U16 parentMem = (parentDisk == sentinelDir) ? 0 : (U16)(parentDisk + 1);

		if (parentMem > dirCount)
			retError(clean, Error_outOfBounds(0, parentMem, dirCount + 1,
				"CAFile_read()::file references an invalid parent folder"
			));

		Ns timestamp = 0;

		if (hasDate) {
			if (hasExtendedDate) {
				gotoIfError3(clean, StreamCursor_consumeU64(&cursor, &readOffset, &timestamp, alloc, e_rr));
			} else {
				U16 date = 0, time = 0;
				gotoIfError3(clean, StreamCursor_consumeU16(&cursor, &readOffset, &date, alloc, e_rr));
				gotoIfError3(clean, StreamCursor_consumeU16(&cursor, &readOffset, &time, alloc, e_rr));
				timestamp = CAFile_loadDate(time, date);
			}
		}

		readOffset += fileExtSize;

		gotoIfError3(clean, ListCAFileInfo_pushBack(&fileMetas, CAFileInfo_create(parentMem, timestamp), alloc, e_rr));
	}

	//Encryption: read iv + tag, verify AAD over the fixed header region

	if (isEncrypted) {

		gotoIfError3(clean, StreamCursor_consume(&cursor, &readOffset, &iv, 3 * sizeof(U32), alloc, e_rr));

		I32x4 tag = I32x4_zero();
		gotoIfError3(clean, StreamCursor_consume(&cursor, &readOffset, &tag, sizeof(I32x4), alloc, e_rr));

		U64 headerRegionSize = (readOffset - sizeof(I32x4) - 3 * sizeof(U32)) - startOffset;

		if (StreamCursor_contains(&cursor, startOffset, headerRegionSize))
			tmp = Buffer_createRefConst(
				cursor.cacheData.ptr + (startOffset - cursor.lastLocation),
				headerRegionSize
			);

		else {
			gotoIfError3(clean, Buffer_createUninitializedBytes(headerRegionSize, alloc, &tmp, e_rr));
			gotoIfError3(clean, StreamCursor_setReadOnly(&cursor, alloc, e_rr));

			U64 where = startOffset;
			gotoIfError3(clean, StreamCursor_consume(
				&cursor, &where, tmp.ptrNonConst, headerRegionSize, alloc, e_rr
			));

			gotoIfError3(clean, StreamCursor_setWritable(&cursor, e_rr));
		}

		gotoIfError3(clean, Buffer_decryptAuto(NULL, &tmp, encryptionKey, tag, iv, e_rr));

		tag = I32x4_zero();
		Buffer_free(&tmp, alloc);
	}

	readOffset = (readOffset + 15) & ~15;
	StreamCursor_close(&cursor, alloc);

	//Read DLFile names and content

	CASettings settings = (CASettings) {
		.encryptionType = (EXXEncryptionType) header.type,
		.flags =
			(hasDate         ? ECASettingsFlags_IncludeDate     : ECASettingsFlags_None) |
			(hasExtendedDate ? ECASettingsFlags_IncludeFullDate : ECASettingsFlags_None)
	};

	if (isEncrypted && encryptionKey)
		Buffer_memcpy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRefConst(encryptionKey, sizeof(settings.encryptionKey))
		);

	I32x4 nameIv    = I32x4_xor(iv, I32x4_createFromU64x2(0, 1));
	I32x4 contentIv = I32x4_xor(iv, I32x4_createFromU64x2(0, 2));

	gotoIfError3(clean, DLFile_read(
		file, &readOffset, encryptionKey, nameIv, true, false, alloc, encStreamType, &names, e_rr
	));

	readOffset = (readOffset + 15) & ~(U64)15;

	gotoIfError3(clean, DLFile_read(
		file, &readOffset, encryptionKey, contentIv, true, false, alloc, encStreamType, &content, e_rr
	));

	{
		OxStream *s = RefPtr_data(file, OxStream);

		if (readOffset != s->size)
			retError(clean, Error_invalidState(1, "CAFile_read() contained extra data after content DLFile"));
	}

	if (names.entryStreams.length != dirCount + fileCount + 1)
		retError(clean, Error_invalidState(0,
			"CAFile_read()::names DLFile entry count doesn't match dirCount + fileCount"
		));

	if (content.entryStreams.length != fileCount)
		retError(clean, Error_invalidState(0,
			"CAFile_read()::content DLFile entry count doesn't match fileCount"
		));

	if (names.settings.dataType != EDLDataType_String)
		retError(clean, Error_invalidState(0, "CAFile_read()::names DLFile must have string type"));

	if (content.settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidState(0, "CAFile_read()::content DLFile must have data type"));

	//Validate all name entries: must already be in memory (not stream-backed) and within the name size limit.
	//Small entries are loaded into the DLFile cache by DLFile_read automatically, so if an entry is still
	// stream-backed it means it exceeded DLFile_smallLen, which means it also exceeds CAFile_maxFileNameSize
	// and is therefore invalid. We check both to be explicit.
	//This can happen if we exceed 5MiB, which is rare with a max of 96 bytes per file name
	// (54K files at max size, more realistically >327K)

	for (U64 i = 0; i < dirCount + fileCount; ++i) {

		if (names.entryStreams.ptr[i].stream)
			retError(clean, Error_invalidState(0, "CAFile_read()::name entry is stream-backed; exceeds maximum name size"));

		if (DLFile_entrySize(&names, i) > CAFile_maxFileNameSize)
			retError(clean, Error_outOfBounds(0, DLFile_entrySize(&names, i), CAFile_maxFileNameSize,
				"CAFile_read()::name entry exceeds maximum file name size"
			));
	}

	//Pass 2: reconstruct hierarchy via CAFile_add so all parenting bookkeeping is correct.
	//Directories are written root-to-leaf on disk, so a forward pass always resolves parents before children.
	//dirHandles maps on-disk folder index (0-based) to a live CAHandle for parent lookups.

	gotoIfError3(clean, CAFile_create(&settings, fileCount, dirCount, alloc, caFile, e_rr));

	gotoIfError3(clean, DLFile_reserve(&caFile->names, fileCount + dirCount + 1, alloc, e_rr));
	gotoIfError3(clean, DLFile_reserve(&caFile->content, fileCount, alloc, e_rr));

	gotoIfError3(clean, DLFile_initCache(&caFile->names, Buffer_length(names.cache), alloc, e_rr));
	gotoIfError3(clean, DLFile_initCache(&caFile->content, Buffer_length(content.cache), alloc, e_rr));

	allocated = true;
	gotoIfError3(clean, ListU16_reserve(&dirHandles, dirCount, alloc, e_rr));

	for (U64 i = 0; i < dirCount; ++i) {

		U16      parentMem    = dirParents.ptr[i];
		CAHandle parentHandle = (parentMem == 0) ? CAHandle_Root : CAHandle_makeFolder(dirHandles.ptr[parentMem - 1]);

		//We're moving this directly from our temp DLFile
		CharString *name = &names.entryStrings.ptrNonConst[i + 1];

		DLFile_prepMoveStringRef(&names, name, &caFile->names);

		CAHandle handle = CAFile_addFolder(caFile, parentHandle, name, alloc, e_rr);

		if (handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "CAFile_read()::CAFile_addFolder failed"));

		gotoIfError3(clean, ListU16_pushBack(&dirHandles, (U16)CAHandle_getId(handle), alloc, e_rr));
	}

	for (U64 i = 0; i < fileCount; ++i) {

		U16      parentMem    = CAFileInfo_getParent(fileMetas.ptr[i]);
		CAHandle parentHandle = (parentMem == 0) ? CAHandle_Root : CAHandle_makeFolder(dirHandles.ptr[parentMem - 1]);

		//We're moving this directly from our temp DLFile
		CharString *name = &names.entryStrings.ptrNonConst[dirCount + i + 1];

		DLFile_prepMoveStringRef(&names, name, &caFile->names);

		CAHandle fileHandle =
			CAFile_addFile(caFile, parentHandle, name, CAFileInfo_getTimestamp(fileMetas.ptr[i]), alloc, e_rr);

		if (fileHandle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "CAFile_read()::CAFile_addFile failed"));

		//Set file data, use the stream reference directly if the entry is still stream-backed,
		//otherwise hand over the loaded buffer. This avoids an unnecessary copy for large files.

		DLEntryStream entryStream = content.entryStreams.ptr[i];

		if (entryStream.stream) {

			gotoIfError3(clean, CAFile_setDataStream(
				caFile, fileHandle, alloc, &entryStream.stream, entryStream.dataOff, entryStream.len, e_rr
			));

			content.entryStreams.ptrNonConst[i] = (DLEntryStream) { 0 };        //Moved

		} else {

			Buffer contentBuf = content.entryBuffers.ptr[i];    //Moving it or copying depending on type
			U64 bufl = Buffer_length(contentBuf);

			//Move directly into our pre-allocated cache buffer (we know the size from the prev DLFile load)
			if (contentBuf.ptr >= content.cache.ptr && contentBuf.ptr < content.cache.ptr + Buffer_length(content.cache)) {
				Buffer subArea = Buffer_createRef(caFile->content.cache.ptrNonConst + (contentBuf.ptr - content.cache.ptr), bufl);
				Buffer_memcpy(subArea, contentBuf);
				gotoIfError3(clean, CAFile_setData(caFile, fileHandle, alloc, &subArea, e_rr));
			}

			//We can only move the real buffer if it's a dedicated allocation
			else {
				gotoIfError3(clean, CAFile_setData(caFile, fileHandle, alloc, &contentBuf, e_rr));
				content.entryBuffers.ptrNonConst[i] = Buffer_createNull();
			}
		}
	}

	DLFile_free(&names, alloc);        //Even though all data has been moved, we still have the lists themselves.
	DLFile_free(&content, alloc);

clean:
	iv = I32x4_zero();
	StreamCursor_close(&cursor, alloc);
	Buffer_free(&tmp, alloc);
	ListU16_free(&dirParents, alloc);
	ListCAFileInfo_free(&fileMetas, alloc);
	ListU16_free(&dirHandles, alloc);
	DLFile_free(&names, alloc);
	DLFile_free(&content, alloc);

	if(!s_uccess && allocated)
		CAFile_free(caFile, alloc);

	return s_uccess;
}
