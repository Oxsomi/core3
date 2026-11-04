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

#include "ca_internal.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/stream.h"
#include "types/container/container_types.h"
#include "types/container/list_basic_types.h"
#include "types/math/vec4i.h"
#include "types/base/time.h"
#include "types/base/string_read.h"

Ns CAFile_loadDate(U16 time, U16 date) {

	Date d = (Date) {
		.year = 1980 + (date >> 9),
		.month = (date >> 5) & 0xF,
		.day = date & 0x1F,
		.hour = time >> 11,
		.minute = (time >> 5) & 0x3F,
		.second = (time & 0x1F) << 1
	};

	return Time_date(&d, false);
}

//The parent of the entry whose name is caFile->names[nameId]: folders come first, then files.

static U64 CAFile_nameParent(const CAFile *caFile, U64 nameId) {
	return nameId < caFile->folders.length ? caFile->folders.ptr[nameId].parent :
		CAFileInfo_getParent(caFile->files.ptr[nameId - caFile->folders.length]);
}

//Orders name ids by parent, then by case folded name.

static ECompareResult CAFile_compareNames(const void *aPtr, const void *bPtr, void *context) {

	const CAFile *caFile = (const CAFile*) context;
	const U64 a = *(const U64*) aPtr, b = *(const U64*) bPtr;
	const U64 parentA = CAFile_nameParent(caFile, a), parentB = CAFile_nameParent(caFile, b);

	if (parentA != parentB)
		return parentA < parentB ? ECompareResult_Lt : ECompareResult_Gt;

	return CharString_compareInsensitive(&caFile->names.entryStrings.ptr[a], &caFile->names.entryStrings.ptr[b]);
}

void DLFile_prepMoveStringRef(DLFile *src, CharString *str, DLFile *dst) {

	Bool isConst = CharString_isConstRef(*str);
	Buffer contentBuf = isConst ? CharString_bufferConst(*str) : CharString_buffer(*str);
	U64 bufl = Buffer_length(contentBuf);

	//Move directly into our pre-allocated cache buffer (we know the size from the prev DLFile load)
	//An oiDL whose entries are all zero length allocates no cache, leaving a null base that the end pointer
	// below would offset, which is undefined even by zero.
	//Nothing lies inside a cache that doesn't exist, so the check short circuits to the same answer.

	if (
		Buffer_length(src->cache) &&
		contentBuf.ptr >= src->cache.ptr &&
		contentBuf.ptr < src->cache.ptr + Buffer_length(src->cache)
	) {

		Buffer subArea = Buffer_createRef(dst->cache.ptrNonConst + (contentBuf.ptr - src->cache.ptr), bufl);
		Buffer_memcpy(subArea, contentBuf);

		*str =
			isConst ? CharString_createRefSizedConst((const C8*) subArea.ptr, bufl, false) :
			CharString_createRefSized((C8*) subArea.ptrNonConst, bufl, false);
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
	ListCAFileInfo fileMetas = (ListCAFileInfo) { 0 };   //packed parent + timestamp per file
	ListU16 pathLengths = (ListU16) { 0 };               //Per folder (root first) the length of its path
	ListU16 depths = (ListU16) { 0 };                    //Per folder the folders above it, the root not counted
	ListU64 byName = (ListU64) { 0 };                    //Name ids sorted by parent, then folded name
	I32x4 iv = I32x4_zero();
	U32 headerKey[8] = { 0 };
	DLFile names = (DLFile) { 0 };
	DLFile content = (DLFile) { 0 };
	CASettings settings = (CASettings) { 0 };        //Holds a copy of the key until the CAFile takes its own

	if (!file || file->refPtrType->typeId != (TypeId)EContainerTypeId_Stream)
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

	if (header.version != (U8) ECAVersion_V1_0)
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

	//A key means the caller expects an encrypted archive, so a plaintext one is refused before anything is parsed from it

	if (!isEncrypted && encryptionKey)
		retError(clean, Error_invalidOperation(3, "CAFile_read()::encryptionKey is provided but no encryption is used"));

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
	
	U16 sentinelDir = dirCountLong ? (U16) 0xFFFF : (U16) 0xFF;

	//The counts are only trusted as far as the stream holds their records, so nothing is reserved from a bare header.
	//Every record is at most 2 + 8 + 255 bytes, so the sums can't overflow.

	const U64 dirRecordSize  = (dirCountLong ? 2 : 1) + dirExtSize;
	const U64 fileRecordSize = (dirCountLong ? 2 : 1) + (hasDate ? (hasExtendedDate ? 8 : 4) : 0) + fileExtSize;
	const U64 recordsEnd     = readOffset + dirCount * dirRecordSize + fileCount * fileRecordSize;

	{
		const OxStream *fileStream = RefPtr_data(file, OxStream);
		const U64 bytesLeft = readOffset > fileStream->size ? 0 : fileStream->size - readOffset;

		if (bytesLeft < recordsEnd - readOffset)
			retError(clean, Error_outOfBounds(
				0, recordsEnd - readOffset, bytesLeft,
				"CAFile_read()::dirCount and fileCount exceed the bytes left in the stream"
			));
	}

	//Encrypted: the iv and tag follow the records, so the whole header, records included, is authenticated before
	// anything is reserved or parsed from the counts.

	if (isEncrypted) {

		U64 tagOffset = recordsEnd;
		gotoIfError3(clean, StreamCursor_consume(&cursor, &tagOffset, &iv, 3 * sizeof(U32), alloc, e_rr));

		I32x4 tag = I32x4_zero();
		gotoIfError3(clean, StreamCursor_consume(&cursor, &tagOffset, &tag, sizeof(I32x4), alloc, e_rr));

		const U64 headerRegionSize = recordsEnd - startOffset;

		if (StreamCursor_contains(&cursor, startOffset, headerRegionSize))
			tmp = Buffer_createRefConst(
				cursor.cacheData.ptr + (startOffset - cursor.lastLocation),
				headerRegionSize
			);

		else {
			gotoIfError3(clean, Buffer_createUninitializedBytes(headerRegionSize, alloc, &tmp, e_rr));

			U64 where = startOffset;
			gotoIfError3(clean, StreamCursor_consume(
				&cursor, &where, tmp.ptrNonConst, headerRegionSize, alloc, e_rr
			));
		}

		gotoIfError3(clean, Buffer_deriveEncryptionKey(
			encryptionKey, iv, EEncryptionKeyDomain_CAHeader, headerKey, e_rr
		));

		gotoIfError3(clean, Buffer_decryptAuto(NULL, &tmp, headerKey, tag, iv, e_rr));

		tag = I32x4_zero();
		Buffer_free(&tmp, alloc);
	}

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
		U16 parentMem = (parentDisk == sentinelDir) ? 0 : (U16) (parentDisk + 1);
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

		U16 parentMem = (parentDisk == sentinelDir) ? 0 : (U16) (parentDisk + 1);

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

				if (timestamp == U64_MAX)
					retError(clean, Error_invalidParameter(0, 3, "CAFile_read()::file has an invalid date"));
			}
		}

		readOffset += fileExtSize;

		gotoIfError3(clean, ListCAFileInfo_pushBack(&fileMetas, CAFileInfo_create(parentMem, timestamp), alloc, e_rr));
	}

	//The records end where the authenticated region did, and the iv and tag after them were read above

	if (readOffset != recordsEnd)
		retError(clean, Error_invalidState(2, "CAFile_read()::records didn't end where their sizes said"));

	if (isEncrypted)
		readOffset += 3 * sizeof(U32) + sizeof(I32x4);

	readOffset = (readOffset + 15) & ~15;
	StreamCursor_close(&cursor, alloc);

	//Read DLFile names and content

	settings = (CASettings) {
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

	readOffset = (readOffset + 15) & ~(U64) 15;

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

	//Every name must be available during hierarchy reconstruction. The oiDL cache is bounded, so names
	// beyond its capacity need individual allocations even though each name fits within the size limit.

	for (U64 i = 0; i < names.entryStreams.length; ++i) {

		if (DLFile_entrySize(&names, i) > CAFile_maxFileNameSize)
			retError(clean, Error_outOfBounds(0, DLFile_entrySize(&names, i), CAFile_maxFileNameSize,
				"CAFile_read()::name entry exceeds maximum file name size"
			));

		gotoIfError3(clean, DLFile_loadEntry(&names, i, alloc, e_rr));
	}

	//Pass 2: the lists are filled in disk order straight from the records, checked as a whole and laid out once.
	//Adding entry by entry would fix up every folder's offsets on each add, which grows with entries times folders.
	//Folders are on disk after their parent, so a forward pass knows a parent's path and depth before its children.

	gotoIfError3(clean, CAFile_create(&settings, fileCount, dirCount, alloc, caFile, e_rr));

	gotoIfError3(clean, DLFile_reserve(&caFile->names, fileCount + dirCount + 1, alloc, e_rr));
	gotoIfError3(clean, DLFile_reserve(&caFile->content, fileCount, alloc, e_rr));

	gotoIfError3(clean, DLFile_initCache(&caFile->names, Buffer_length(names.cache), alloc, e_rr));
	gotoIfError3(clean, DLFile_initCache(&caFile->content, Buffer_length(content.cache), alloc, e_rr));

	allocated = true;

	gotoIfError3(clean, ListU16_reserve(&pathLengths, dirCount + 1, alloc, e_rr));
	gotoIfError3(clean, ListU16_reserve(&depths, dirCount + 1, alloc, e_rr));
	gotoIfError3(clean, ListU16_pushBack(&pathLengths, 0, alloc, e_rr));
	gotoIfError3(clean, ListU16_pushBack(&depths, 0, alloc, e_rr));

	for (U64 i = 0; i < dirCount + fileCount; ++i) {

		const Bool isFolder = i < dirCount;
		const U16 parent = isFolder ? dirParents.ptr[i] : CAFileInfo_getParent(fileMetas.ptr[i - dirCount]);

		//We're moving this directly from our temp DLFile
		CharString *name = &names.entryStrings.ptrNonConst[i + 1];

		if (!CharString_isValidFileName(*name))
			retError(clean, Error_invalidParameter(0, 4, "CAFile_read()::name has invalid characters"));

		const U64 pathLength = (parent ? pathLengths.ptr[parent] + 1 : 0) + CharString_length(*name);
		const U64 depth = parent ? depths.ptr[parent] + 1 : 0;

		if (pathLength > CAFile_maxFilePathSize)
			retError(clean, Error_invalidParameter(0, 5, "CAFile_read()::a path exceeds 192 chars"));

		if (depth >= CAFile_maxRecursionSize)
			retError(clean, Error_invalidParameter(0, 6, "CAFile_read()::a path is nested too deep"));

		if (isFolder) {
			gotoIfError3(clean, ListU16_pushBack(&pathLengths, (U16) pathLength, alloc, e_rr));
			gotoIfError3(clean, ListU16_pushBack(&depths, (U16) depth, alloc, e_rr));
			gotoIfError3(clean, ListCAFolderInfo_pushBack(&caFile->folders, (CAFolderInfo) { .parent = parent }, alloc, e_rr));
		}

		else gotoIfError3(clean, ListCAFileInfo_pushBack(&caFile->files, fileMetas.ptr[i - dirCount], alloc, e_rr));

		DLFile_prepMoveStringRef(&names, name, &caFile->names);
		gotoIfError3(clean, DLFile_addEntryString(&caFile->names, name, alloc, e_rr));
	}

	//Names within one folder must differ case folded; sorted by parent and then name, any two that don't are neighbours

	gotoIfError3(clean, ListU64_reserve(&byName, dirCount + fileCount, alloc, e_rr));

	for (U64 i = 1; i <= dirCount + fileCount; ++i)
		gotoIfError3(clean, ListU64_pushBack(&byName, i, alloc, e_rr));

	if (!ListU64_sortCustom(byName, CAFile_compareNames, caFile))
		retError(clean, Error_invalidState(1, "CAFile_read()::couldn't sort the names"));

	for (U64 i = 1; i < byName.length; ++i)
		if (CAFile_compareNames(&byName.ptr[i - 1], &byName.ptr[i], caFile) == ECompareResult_Eq)
			retError(clean, Error_alreadyDefined(0, "CAFile_read()::a folder holds two names that only differ by case"));

	for (U64 i = 0; i < fileCount; ++i) {

		Buffer empty = Buffer_createNull();
		gotoIfError3(clean, DLFile_addEntry(&caFile->content, &empty, alloc, e_rr));

		//Set file data, use the stream reference directly if the entry is still stream-backed,
		// otherwise hand over the loaded buffer.
		//This avoids an unnecessary copy for large files.

		DLEntryStream entryStream = content.entryStreams.ptr[i];

		if (entryStream.stream) {

			gotoIfError3(clean, CAFile_setDataStream(
				caFile, CAHandle_makeFile(i), alloc, &entryStream.stream, entryStream.dataOff, entryStream.len, e_rr
			));

			content.entryStreams.ptrNonConst[i] = (DLEntryStream) { 0 };        //Moved

		} else {

			Buffer contentBuf = content.entryBuffers.ptr[i];    //Moving it or copying depending on type
			U64 bufl = Buffer_length(contentBuf);

			//Move directly into our pre-allocated cache buffer (we know the size from the prev DLFile load)
			//Guarded on a non-empty cache for the same reason as the names variant above: an archive whose
			// files are all empty allocates no cache, and the end pointer would offset a null base.

			if (
				Buffer_length(content.cache) &&
				contentBuf.ptr >= content.cache.ptr &&
				contentBuf.ptr < content.cache.ptr + Buffer_length(content.cache)
			) {
				Buffer subArea =
					Buffer_createRef(caFile->content.cache.ptrNonConst + (contentBuf.ptr - content.cache.ptr), bufl);
				Buffer_memcpy(subArea, contentBuf);
				gotoIfError3(clean, DLFile_setEntry(&caFile->content, i, &subArea, alloc, e_rr));
			}

			//We can only move the real buffer if it's a dedicated allocation
			else {
				gotoIfError3(clean, DLFile_setEntry(&caFile->content, i, &contentBuf, alloc, e_rr));
				content.entryBuffers.ptrNonConst[i] = Buffer_createNull();
			}
		}
	}

	gotoIfError3(clean, CAFile_relayout(caFile, alloc, e_rr));

	DLFile_free(&names, alloc);        //Even though all data has been moved, we still have the lists themselves.
	DLFile_free(&content, alloc);

clean:
	Buffer_clearAllSecure(Buffer_createRef(headerKey, sizeof(headerKey)));
	Buffer_clearAllSecure(Buffer_createRef(&settings, sizeof(settings)));
	iv = I32x4_zero();
	StreamCursor_close(&cursor, alloc);
	Buffer_free(&tmp, alloc);
	ListU16_free(&dirParents, alloc);
	ListCAFileInfo_free(&fileMetas, alloc);
	ListU16_free(&pathLengths, alloc);
	ListU16_free(&depths, alloc);
	ListU64_free(&byName, alloc);
	DLFile_free(&names, alloc);
	DLFile_free(&content, alloc);

	if(!s_uccess && allocated)
		CAFile_free(caFile, alloc);

	return s_uccess;
}
