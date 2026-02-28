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

#include "types/container/list_impl.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiDL/interface.h"
#include "types/container/ref_ptr.h"
#include "types/container/memory_stream.h"
#include "types/base/string_read_helper.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

TListImpl(CAFolderInfo);
TListImpl(CAFileInfo);

static const U64 CAHandle_RootId = 0;

static inline CAHandle CAHandle_makeFolder(U64 id) { return ((U64)1 << 63) | id; }
static inline CAHandle CAHandle_makeFile(U64 id) { return id; }

static inline Bool CAHandle_folderIndex(CAHandle handle, U16 *idx) {

	if (!CAHandle_isFolder(handle) || !idx)
		return false;

	U64 id = CAHandle_getId(handle);

	if (id == CAHandle_RootId) {
		*idx = U16_MAX;
		return true;
	}

	if (id >= U16_MAX)
		return false;

	*idx = (U16)id;
	return true;
}

static inline Bool CAHandle_fileIndex(CAHandle handle, U64 *idx) {

	if (!CAHandle_isFile(handle) || !idx)
		return false;

	*idx = CAHandle_getId(handle);
	return true;
}

Bool CAFile_create(
	const CASettings *settings,
	U64 reservedFiles,
	U64 reservedFolders,
	const Allocator *alloc,
	CAFile *caFile,
	Error *e_rr
) {
	Bool s_uccess = true;

	if (!caFile)
		retError(clean, Error_nullPointer(4, "CAFile_create()::caFile is required"));

	if (!settings)
		retError(clean, Error_nullPointer(0, "CAFile_create()::settings is required"));

	if (settings->compressionType >= EXXCompressionType_Count)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_create()::settings.compressionType is invalid"));

	if (settings->compressionType > EXXCompressionType_None)
		retError(clean, Error_unsupportedOperation(0, "CAFile_create() compression not supported yet"));		//TODO:

	if (settings->flags & ECASettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_create()::settings->flags contains invalid flags"));

	if(caFile->names.settings.dataType)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_create()::caFile was already allocated, indicating memleak"));

	if (settings->encryptionType >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 1, "CAFile_create()::settings.encryptionType is invalid"));

	if(settings->flags & ECASettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 2, "CAFile_create()::flags is invalid"));

	caFile->settings = *settings;

	//Names

	DLSettings nameSettings = (DLSettings) {
		.compressionType = settings->compressionType,		//TODO: Maybe allow different compression type here later
		.encryptionType  = settings->encryptionType,
		.dataType        = EDLDataType_String
	};

	if (settings->encryptionType)
		Buffer_memcpy(
			Buffer_createRef(nameSettings.encryptionKey, sizeof(nameSettings.encryptionKey)),
			Buffer_createRefConst(settings->encryptionKey, sizeof(settings->encryptionKey))
		);

	gotoIfError3(clean, DLFile_create(&nameSettings, 0, alloc, &caFile->names, e_rr));

	gotoIfError3(clean, DLFile_reserve(&caFile->names, 1 + reservedFiles + reservedFolders, alloc, e_rr));

	//Content

	DLSettings contentSettings = (DLSettings) {
		.compressionType = settings->compressionType,
		.encryptionType  = settings->encryptionType,
		.dataType        = EDLDataType_Data
	};

	if (settings->encryptionType)
		Buffer_memcpy(
			Buffer_createRef(contentSettings.encryptionKey, sizeof(contentSettings.encryptionKey)),
			Buffer_createRefConst(settings->encryptionKey, sizeof(settings->encryptionKey))
		);

	gotoIfError3(clean, DLFile_create(&contentSettings, 0, alloc, &caFile->content, e_rr));

	gotoIfError3(clean, ListCAFolderInfo_reserve(&caFile->folders, 1 + reservedFolders, alloc, e_rr));

	if (reservedFiles) {
		gotoIfError3(clean, DLFile_reserve(&caFile->content, reservedFiles, alloc, e_rr));
		gotoIfError3(clean, ListCAFileInfo_reserve(&caFile->files, reservedFiles, alloc, e_rr));
	}

	//We push root at 0, we do this so we can easily parent/traverse even from the root's perspective

	CharString empty = CharString_createNull();
	gotoIfError3(clean, DLFile_addEntryString(&caFile->names, &empty, alloc, e_rr));

	CAFolderInfo root = (CAFolderInfo) { 0 };
	gotoIfError3(clean, ListCAFolderInfo_pushBack(&caFile->folders, root, alloc, e_rr));

clean:

	if (!s_uccess)
		CAFile_free(caFile, alloc);

	return s_uccess;
}

Bool CAFile_createCopy(const CAFile *caFile, const Allocator *alloc, CAFile *result, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile || !result)
		retError(clean, Error_nullPointer(!caFile ? 0 : 2, "CAFile_createCopy()::caFile and result are required"));

	*result = (CAFile) { 0 };

	gotoIfError3(clean, DLFile_createCopy(&caFile->names, alloc, &result->names, e_rr));
	gotoIfError3(clean, DLFile_createCopy(&caFile->content, alloc, &result->content, e_rr));
	gotoIfError3(clean, ListCAFolderInfo_createCopy(caFile->folders, alloc, &result->folders, e_rr));
	gotoIfError3(clean, ListCAFileInfo_createCopy(caFile->files, alloc, &result->files, e_rr));

	result->settings = caFile->settings;

clean:

	if (!s_uccess)
		CAFile_free(result, alloc);

	return s_uccess;
}

void CAFile_free(CAFile *caFile, const Allocator *alloc) {

	if(!caFile)
		return;

	DLFile_free(&caFile->names, alloc);
	DLFile_free(&caFile->content, alloc);
	ListCAFolderInfo_free(&caFile->folders, alloc);
	ListCAFileInfo_free(&caFile->files, alloc);

	*caFile = (CAFile) { 0 };
}

//Lookups

static inline const CAFolderInfo *CAFile_getFolderInfoPtr(const CAFile *caFile, CAHandle handle) {

	if (!caFile || !CAHandle_isFolder(handle))
		return NULL;

	U64 id = CAHandle_getId(handle);

	if (id >= caFile->folders.length)
		return NULL;

	return &caFile->folders.ptr[id];
}

static inline const CAFileInfo *CAFile_getFileInfoPtr(const CAFile *caFile, CAHandle handle) {

	if (!caFile || !CAHandle_isFile(handle))
		return NULL;

	U64 id = CAHandle_getId(handle);

	if (id >= caFile->files.length)
		return NULL;

	return &caFile->files.ptr[id];
}

//File properties

U64 CAFile_fileSize(const CAFile *caFile, CAHandle fileHandle) {
	return !caFile || CAHandle_isFolder(fileHandle) ? 0 : DLFile_entrySize(&caFile->content, CAHandle_getId(fileHandle));
}

Ns CAFile_fileTime(const CAFile *caFile, CAHandle fileHandle) {
	const CAFileInfo *info = CAFile_getFileInfoPtr(caFile, fileHandle);
	return !info ? (Ns)0 : CAFileInfo_getTimestamp(*info);
}

U64 CAFile_fileParent(const CAFile *caFile, CAHandle fileHandle) {
	const CAFileInfo *file = CAFile_getFileInfoPtr(caFile, fileHandle);
	const CAFolderInfo *folder = CAFile_getFolderInfoPtr(caFile, fileHandle);
	return file ? CAFileInfo_getParent(*file) : (folder ? folder->parent : CAHandle_Root);
}

CharString CAFile_getName(const CAFile *caFile, CAHandle handle) {

	if (!caFile || handle == CAHandle_Invalid || CAHandle_isRoot(handle))
		return CharString_createNull();

	U64 nameIdx;

	if (CAHandle_isFolder(handle))
		nameIdx = CAHandle_getId(handle);

	else nameIdx = caFile->folders.length + CAHandle_getId(handle);

	CharString str = { 0 };

	if (!DLFile_loadedStringAtConst(&caFile->names, nameIdx, &str, NULL))
		return CharString_createNull();

	return str;
}

static inline U64 CAFile_computePathLen(const CAFile *caFile, CAHandle parent, U64 newNameLen, U64 *depth) {

	U64 total = newNameLen;
	CAHandle cur = parent;

	if(depth)
		*depth = 0;

	while (!CAHandle_isRoot(cur)) {

		CharString seg = CAFile_getName(caFile, cur);
		U64 segLen = CharString_length(seg);

		if(depth)
			++*depth;

		if (!segLen)
			break;

		total += segLen + 1;	//+1 for '/'

		const CAFolderInfo *fi = CAFile_getFolderInfoPtr(caFile, cur);

		if (!fi)
			break;

		cur = CAHandle_makeFolder(fi->parent);
	}

	return total;
}

Bool CAFile_getFullName(
	const CAFile *caFile,
	CAHandle handle,
	const Allocator *alloc,
	CharString *result,
	Error *e_rr
) {
	Bool s_uccess = true;
	Bool allocated = false;

	if (!caFile || !result)
		retError(clean, Error_nullPointer(!caFile ? 0 : 3, "CAFile_getFullName()::caFile and result are required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(
			1, 0, "CAFile_getFullName()::result is not empty, indicating possible memleak"
		));

	allocated = true;

	if (handle == CAHandle_Invalid)
		retError(clean, Error_invalidParameter(1, 0, "CAFile_getFullName()::handle is invalid"));

	if (CAHandle_isRoot(handle))
		goto clean;		//Empty string for root

	//Walk up to root collecting handles, then build string in reverse

	CAHandle chain[128];
	U32 depth = 0, length = 0;
	CAHandle cur = handle;

	while (!CAHandle_isRoot(cur) && cur != CAHandle_Invalid) {

		if (depth)
			++length;		//Separator

		if (depth >= CAFile_maxRecursionSize)
			retError(clean, Error_overflow(0, depth, CAFile_maxRecursionSize, "CAFile_getFullName()::path too deep"));

		chain[depth++] = cur;

		if (CAHandle_isFile(cur)) {

			const CAFileInfo *fi = CAFile_getFileInfoPtr(caFile, cur);

			if (!fi)
				retError(clean, Error_invalidParameter(1, 0, "CAFile_getFullName()::invalid handle in chain"));

			length += (U32)DLFile_entrySize(&caFile->names, caFile->folders.length + CAHandle_getId(cur));
			cur = CAHandle_makeFolder(CAFileInfo_getParent(*fi));

		} else {

			const CAFolderInfo *fi = CAFile_getFolderInfoPtr(caFile, cur);

			if (!fi)
				retError(clean, Error_invalidParameter(1, 0, "CAFile_getFullName()::invalid handle in chain"));

			length += (U32)DLFile_entrySize(&caFile->names, CAHandle_getId(cur));
			cur = CAHandle_makeFolder(fi->parent);
		}
	}

	if (length)
		gotoIfError3(clean, CharString_reserve(result, length, alloc, e_rr));

	for (U32 i = depth; i > 0; --i) {

		CharString seg = CAFile_getName(caFile, chain[i - 1]);

		if (!CharString_length(seg))
			retError(clean, Error_invalidState(0, "CAFile_getFullName()::empty name segment"));

		if (CharString_length(*result))
			gotoIfError3(clean, CharString_append(result, '/', alloc, e_rr));

		gotoIfError3(clean, CharString_appendString(result, &seg, alloc, e_rr));
	}

clean:

	if (!s_uccess && allocated)
		CharString_free(result, alloc);

	return s_uccess;
}

//Counts

U64 CAFile_fileCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive) {

	if (!caFile)
		return 0;

	const CAFolderInfo *folder = CAFile_getFolderInfoPtr(caFile, fileHandle);

	if (!folder)
		return 0;

	if (!recursive)
		return folder->fileCount;

	U64 total = folder->fileCount;

	for (U16 i = 0; i < folder->dirCount; ++i)
		total += CAFile_fileCount(caFile, CAHandle_makeFolder(folder->dirOffset + i), true);

	return total;
}

U64 CAFile_dirCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive) {

	if (!caFile)
		return 0;

	const CAFolderInfo *folder = CAFile_getFolderInfoPtr(caFile, fileHandle);

	if (!folder)
		return 0;

	if (!recursive)
		return folder->dirCount;

	U64 total = folder->dirCount;

	for (U16 i = 0; i < folder->dirCount; ++i)
		total += CAFile_dirCount(caFile, CAHandle_makeFolder(folder->dirOffset + i), true);

	return total;
}

//Resolving

CAHandle CAFile_resolveSubFolder(const CAFile *caFile, CAHandle parentDir, CharString fileName) {

	if (!caFile)
		return CAHandle_Invalid;

	const CAFolderInfo *folder = CAFile_getFolderInfoPtr(caFile, parentDir);

	if (!folder)
		return CAHandle_Invalid;

	for (U16 i = 0; i < folder->dirCount; ++i) {

		CAHandle handle = CAHandle_makeFolder(folder->dirOffset + i);
		CharString name = CAFile_getName(caFile, handle);

		if (CharString_equalsStringSensitive(&name, &fileName))
			return handle;
	}

	return CAHandle_Invalid;
}

CAHandle CAFile_resolveSubFile(const CAFile *caFile, CAHandle parentDir, CharString fileName) {

	if (!caFile)
		return CAHandle_Invalid;

	const CAFolderInfo *folder = CAFile_getFolderInfoPtr(caFile, parentDir);

	if (!folder)
		return CAHandle_Invalid;

	for (U16 i = 0; i < folder->fileCount; ++i) {

		CAHandle handle = CAHandle_makeFile(folder->fileOffset + i);
		CharString name = CAFile_getName(caFile, handle);

		if (CharString_equalsStringSensitive(&name, &fileName))
			return handle;
	}

	return CAHandle_Invalid;
}

CAHandle CAFile_resolveFolder(const CAFile *caFile, CharString fullFileName) {

	if (!caFile)
		return CAHandle_Invalid;

	CAHandle cur = CAHandle_Root;
	U64 len = CharString_length(fullFileName);
	U64 start = 0;

	while (start < len) {

		U64 slash = CharString_findFirstSensitive(&fullFileName, '/', start, len - start);
		U64 end = slash == U64_MAX ? len : slash;

		if (end > start) {
			CharString seg = CharString_createRefSizedConst(fullFileName.ptr + start, end - start, false);
			cur = CAFile_resolveSubFolder(caFile, cur, seg);

			if (cur == CAHandle_Invalid)
				return CAHandle_Invalid;
		}

		start = end + 1;
	}

	return cur;
}

CAHandle CAFile_resolveFile(const CAFile *caFile, CharString fullFileName) {

	if (!caFile)
		return CAHandle_Invalid;

	U64 len = CharString_length(fullFileName);

	U64 lastSlash = CharString_findLastSensitive(&fullFileName, '/', 0, len);

	CAHandle parentDir = CAHandle_Root;

	if (lastSlash != U64_MAX) {
		CharString dirPart = CharString_createRefSizedConst(fullFileName.ptr, lastSlash, false);
		parentDir = CAFile_resolveFolder(caFile, dirPart);

		if (parentDir == CAHandle_Invalid)
			return CAHandle_Invalid;
	}

	CharString filePart = CharString_createRefSizedConst(
		fullFileName.ptr + (lastSlash == U64_MAX ? 0 : lastSlash + 1),
		lastSlash == U64_MAX ? len : len - lastSlash - 1,
		false
	);

	return CAFile_resolveSubFile(caFile, parentDir, filePart);
}

//File objects at

CAHandle CAFile_dirAt(const CAFile *caFile, CAHandle fileHandle, U64 id) {

	if (!caFile)
		return CAHandle_Invalid;

	const CAFolderInfo *folder = CAFile_getFolderInfoPtr(caFile, fileHandle);

	if (!folder || id >= folder->dirCount)
		return CAHandle_Invalid;

	return CAHandle_makeFolder(folder->dirOffset + id);
}

CAHandle CAFile_fileAt(const CAFile *caFile, CAHandle fileHandle, U64 id) {

	if (!caFile)
		return CAHandle_Invalid;

	const CAFolderInfo *folder = CAFile_getFolderInfoPtr(caFile, fileHandle);

	if (!folder || id >= folder->fileCount)
		return CAHandle_Invalid;

	return CAHandle_makeFile(folder->fileOffset + id);
}

//Getters

Buffer CAFile_getData(CAFile *caFile, CAHandle fileHandle, Bool *isValid) {

	if (isValid)
		*isValid = false;

	if (!caFile || !CAHandle_isFile(fileHandle))
		return Buffer_createNull();

	U64 id = CAHandle_getId(fileHandle);

	if (id >= caFile->files.length)
		return Buffer_createNull();

	Buffer buf = Buffer_createNull();

	if (!DLFile_loadedBufferAtConst(&caFile->content, id, &buf, NULL))		//TODO: Non const?
		return Buffer_createNull();

	if (isValid)
		*isValid = true;

	return buf;
}

Buffer CAFile_getDataConst(const CAFile *caFile, CAHandle fileHandle, Bool *isValid) {
	return CAFile_getData((CAFile*)caFile, fileHandle, isValid);
}

StreamRef *CAFile_getDataStream(const CAFile *caFile, CAHandle fileHandle, U64 *streamOff) {

	if (streamOff)
		*streamOff = U64_MAX;

	if (!caFile || !CAHandle_isFile(fileHandle))
		return NULL;

	U64 id = CAHandle_getId(fileHandle);

	if (id >= caFile->files.length || id >= caFile->content.entryStreams.length)
		return NULL;

	DLEntryStream entry = caFile->content.entryStreams.ptr[id];

	if (!entry.stream)
		return NULL;

	if (streamOff)
		*streamOff = entry.dataOff;

	RefPtr_inc(entry.stream);
	return entry.stream;
}

Bool CAFile_isLoaded(const CAFile *caFile, CAHandle fileHandle) {

	if (!caFile || !CAHandle_isFile(fileHandle))
		return false;

	U64 id = CAHandle_getId(fileHandle);
	return DLFile_isFullyLoaded(&caFile->content, id);
}

//Setters

Bool CAFile_setTime(CAFile *caFile, CAHandle fileHandle, Ns time, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile)
		retError(clean, Error_nullPointer(0, "CAFile_setTime()::caFile is required"));

	if (!CAHandle_isFile(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_setTime()::fileHandle must be a file"));

	U64 id = CAHandle_getId(fileHandle);

	if (id >= caFile->files.length)
		retError(clean, Error_outOfBounds(1, id, caFile->files.length, "CAFile_setTime()::file id out of bounds"));

	CAFileInfo *fileInfo = &caFile->files.ptrNonConst[id];
	caFile->files.ptrNonConst[id] = CAFileInfo_create(CAFileInfo_getParent(*fileInfo), time);

clean:
	return s_uccess;
}

Bool CAFile_setData(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Buffer *buf, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile || !buf)
		retError(clean, Error_nullPointer(!caFile ? 0 : 3, "CAFile_setData()::caFile and buf are required"));

	if (!CAHandle_isFile(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_setData()::fileHandle must be a file"));

	U64 id = CAHandle_getId(fileHandle);

	if (id >= caFile->files.length)
		retError(clean, Error_outOfBounds(1, id, caFile->files.length, "CAFile_setData()::file id out of bounds"));

	gotoIfError3(clean, DLFile_setEntry(&caFile->content, id, buf, alloc, e_rr));

clean:
	return s_uccess;
}

Bool CAFile_setDataStream(
	CAFile *caFile,
	CAHandle fileHandle,
	const Allocator *alloc,
	StreamRef **stream,
	U64 off,
	U64 len,
	Error *e_rr
) {
	Bool s_uccess = true;

	if (!caFile || !stream)
		retError(clean, Error_nullPointer(!caFile ? 0 : 3, "CAFile_setDataStream()::caFile and stream are required"));

	if (!CAHandle_isFile(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_setDataStream()::handle must be a file"));

	U64 id = CAHandle_getId(fileHandle);

	if (id >= caFile->files.length)
		retError(clean, Error_outOfBounds(1, id, caFile->files.length, "CAFile_setDataStream()::file id out of bounds"));

	gotoIfError3(clean, DLFile_setStream(&caFile->content, id, stream, off, len, alloc, e_rr));

clean:
	return s_uccess;
}

//Rename / move

Bool CAFile_rename(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, CharString *name, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile || !name)
		retError(clean, Error_nullPointer(!caFile ? 0 : 3, "CAFile_rename()::caFile and name are required"));

	if (fileHandle == CAHandle_Invalid || CAHandle_isRoot(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_rename()::cannot rename root or invalid handle"));

	if (!CharString_isValidFileName(*name) || CharString_length(*name) > CAFile_maxFileNameSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_rename()::invalid name (bad chars or exceeds 96 chars)"));

	CAHandle parent;

	if (CAHandle_isFile(fileHandle)) {

		const CAFileInfo *fi = CAFile_getFileInfoPtr(caFile, fileHandle);

		if (!fi)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_rename()::invalid handle"));

		parent = CAHandle_makeFolder(CAFileInfo_getParent(*fi));

	} else {

		const CAFolderInfo *fi = CAFile_getFolderInfoPtr(caFile, fileHandle);

		if (!fi)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_rename()::invalid handle"));

		parent = CAHandle_makeFolder(fi->parent);
	}

	if (CAFile_computePathLen(caFile, parent, CharString_length(*name), NULL) > CAFile_maxFilePathSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_rename()::full path would exceed 192 chars"));

	if (CAFile_hasSubObject(caFile, parent, *name))
		retError(clean, Error_alreadyDefined(0, "CAFile_rename()::name already exists in parent"));

	U64 nameIdx =
		CAHandle_isFolder(fileHandle) ? CAHandle_getId(fileHandle) :
		caFile->folders.length + CAHandle_getId(fileHandle);

	gotoIfError3(clean, DLFile_setEntryString(&caFile->names, nameIdx, name, alloc, e_rr));

clean:
	return s_uccess;
}

Bool CAFile_move(CAFile *caFile, CAHandle fileHandle, CAHandle newParent, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();
	Buffer tmpBuf = Buffer_createNull();
	DLEntryStream tmpStream = (DLEntryStream) { 0 };
	DLEntryStream tmpStreamStr = (DLEntryStream) { 0 };

	if (!caFile)
		retError(clean, Error_nullPointer(0, "CAFile_move()::caFile is required"));

	if (fileHandle == CAHandle_Invalid || CAHandle_isRoot(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_move()::cannot move root or invalid handle"));

	if (!CAHandle_isFolder(newParent))
		retError(clean, Error_invalidParameter(2, 0, "CAFile_move()::newParent must be a folder"));

	U64 pid = CAHandle_getId(newParent);
	if (pid >= caFile->folders.length)
		retError(clean, Error_outOfBounds(2, pid, caFile->folders.length, "CAFile_move()::newParent out of bounds"));

	//No-op, move to self:

	U64 srcId = CAHandle_getId(fileHandle);
	U64 oldParentId =
		CAHandle_isFile(fileHandle) ? (U64)CAFileInfo_getParent(caFile->files.ptr[srcId]) :
		(U64)caFile->folders.ptr[srcId].parent;

	if (oldParentId == pid)
		goto clean;

	//Can't move a folder into itself or a descendant

	if (CAHandle_isFolder(fileHandle)) {

		U64 cur = pid;

		while (cur) {

			if (cur == srcId)
				retError(clean, Error_invalidParameter(2, 0, "CAFile_move() tried to move into sibling, illegal!"));

			cur = caFile->folders.ptr[cur].parent;
		}

		if (cur == srcId)
			retError(clean, Error_invalidParameter(2, 0, "CAFile_move() tried to move into sibling, illegal!"));
	}

	CharString name = CAFile_getName(caFile, fileHandle);

	U64 depth = 0;
	if (CAFile_computePathLen(caFile, newParent, CharString_length(name), NULL) > CAFile_maxFilePathSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_move()::full path would exceed 192 chars"));

	if(depth >= CAFile_maxRecursionSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_move()::depth out of recursion"));

	if (CAFile_hasSubObject(caFile, newParent, name))
		retError(clean, Error_alreadyDefined(0, "CAFile_move()::name already exists in destination"));

	U16 newParentId = (U16)CAHandle_getId(newParent);	//0 = root

	if (CAHandle_isFile(fileHandle)) {

		CAFolderInfo *oldPar = &caFile->folders.ptrNonConst[oldParentId];
		CAFolderInfo *newPar = &caFile->folders.ptrNonConst[newParentId];

		//Determine insert position in new parent's file block

		U64 dstId =
			newPar->fileCount ? newPar->fileOffset + newPar->fileCount :
			caFile->files.length - 1;

		if (dstId > srcId)		//If moving forward in the array, account for the removal shifting dst
			--dstId;

		//Physically move the entry

		CAFileInfo entry = caFile->files.ptr[srcId];
		entry = CAFileInfo_create(newParentId, CAFileInfo_getTimestamp(entry));

		gotoIfError3(clean, ListCAFileInfo_erase(&caFile->files, srcId, e_rr));
		gotoIfError3(clean, DLFile_removeEntry(&caFile->content, srcId, &tmpBuf, &tmpStream, e_rr));

		U64 nameId = caFile->folders.length + srcId;
		gotoIfError3(clean, DLFile_removeEntryString(&caFile->names, nameId, &tmp, &tmpStreamStr, e_rr));

		gotoIfError3(clean, ListCAFileInfo_insert(&caFile->files, dstId, entry, alloc, e_rr));

		if (tmpStream.stream) {
			gotoIfError3(clean, DLFile_insertStream(&caFile->content, dstId, &tmpStream, alloc, e_rr));
		}
		
		else gotoIfError3(clean, DLFile_insertEntry(&caFile->content, dstId, &tmpBuf, alloc, e_rr));

		if (tmpStreamStr.stream) {
			gotoIfError3(clean, DLFile_insertStream(
				&caFile->names, caFile->folders.length + dstId, &tmpStreamStr, alloc, e_rr
			));
		}

		else gotoIfError3(clean, DLFile_insertEntryString(&caFile->names, caFile->folders.length + dstId, &tmp, alloc, e_rr));

		//Update old parent

		--oldPar->fileCount;
		if (!oldPar->fileCount)
			oldPar->fileOffset = 0;

		//Update new parent

		if (!newPar->fileCount)
			newPar->fileOffset = dstId;

		++newPar->fileCount;

		//Fix up all fileOffsets shifted by the remove then insert

		for (U64 i = 0; i < caFile->folders.length; ++i) {

			CAFolderInfo *f = &caFile->folders.ptrNonConst[i];

			if (f->fileOffset > srcId)
				--f->fileOffset;

			if (i != newParentId && (U64)f->fileOffset >= dstId && i != newParentId)
				++f->fileOffset;
		}

	} else {

		CAFolderInfo *oldPar = &caFile->folders.ptrNonConst[oldParentId];
		CAFolderInfo *newPar = &caFile->folders.ptrNonConst[newParentId];

		//Determine insert position in new parent's dir block

		U16 dstId =
			newPar->dirCount ? newPar->dirOffset + newPar->dirCount :
			(U16)caFile->folders.length - 1;

		if (dstId > srcId)
			--dstId;

		//Snapshot the entry, update parent

		CAFolderInfo entry = caFile->folders.ptr[srcId];
		entry.parent = newParentId;

		//Grab name before removal shifts indices

		U64 nameId = srcId;
		gotoIfError3(clean, DLFile_removeEntryString(&caFile->names, nameId, &tmp, &tmpStreamStr, e_rr));

		gotoIfError3(clean, ListCAFolderInfo_erase(&caFile->folders, srcId, e_rr));

		if (newParentId > srcId)
			--newParentId;

		if (oldParentId > srcId)
			--oldParentId;

		gotoIfError3(clean, ListCAFolderInfo_insert(&caFile->folders, dstId, entry, alloc, e_rr));

		oldPar = &caFile->folders.ptrNonConst[oldParentId];
		newPar = &caFile->folders.ptrNonConst[newParentId];

		if (tmpStreamStr.stream) {
			gotoIfError3(clean, DLFile_insertStream(&caFile->names, dstId, &tmpStreamStr, alloc, e_rr));
		}

		else gotoIfError3(clean, DLFile_insertEntryString(&caFile->names, dstId, &tmp, alloc, e_rr));

		//Update old parent

		--oldPar->dirCount;

		if (!oldPar->dirCount)
			oldPar->dirOffset = 0;

		//Update new parent

		if (!newPar->dirCount)
			newPar->dirOffset = dstId;

		++newPar->dirCount;

		//Fix up all parent/dirOffset references shifted by remove then insert

		for (U64 i = 0; i < caFile->folders.length; ++i) {

			if (i == dstId || i == newParentId)
				continue;

			CAFolderInfo *f = &caFile->folders.ptrNonConst[i];

			if (f->parent > srcId)
				--f->parent;

			if (f->parent >= dstId)
				++f->parent;

			if (f->dirOffset > srcId)
				--f->dirOffset;

			if (f->dirOffset >= dstId)
				++f->dirOffset;
		}

		for (U64 i = 0; i < caFile->files.length; ++i) {

			CAFileInfo *fileInfo = &caFile->files.ptrNonConst[i];
			U16 par2 = CAFileInfo_getParent(*fileInfo);

			if (par2 > srcId)
				--par2;

			if (par2 >= dstId)
				++par2;

			*fileInfo = CAFileInfo_create(par2, CAFileInfo_getTimestamp(*fileInfo));
		}
	}

clean:
	RefPtr_dec(&tmpStream.stream);
	RefPtr_dec(&tmpStreamStr.stream);
	Buffer_free(&tmpBuf, alloc);
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Add

CAHandle CAFile_add(
	CAFile *caFile,
	CAHandle parent,
	CharString *name,
	Ns time,
	Bool isFile,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	CAHandle result = CAHandle_Invalid;

	if (!caFile || !name)
		retError(clean, Error_nullPointer(!caFile ? 0 : 2, "CAFile_add()::caFile and name are required"));

	if (!CAHandle_isFolder(parent))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_add()::parent must be a folder handle"));

	U64 pid2 = CAHandle_getId(parent);
	if (pid2 >= caFile->folders.length)
		retError(clean, Error_outOfBounds(1, pid2, caFile->folders.length, "CAFile_add()::parent out of bounds"));

	if (!CharString_isValidFileName(*name) || CharString_length(*name) > CAFile_maxFileNameSize)
		retError(clean, Error_invalidParameter(2, 0, "CAFile_add()::invalid name (bad chars or exceeds 96 chars)"));

	U64 depth = 0;
	if (CAFile_computePathLen(caFile, parent, CharString_length(*name), &depth) > CAFile_maxFilePathSize)
		retError(clean, Error_invalidParameter(2, 0, "CAFile_add()::full path would exceed 192 chars"));

	if (depth >= CAFile_maxRecursionSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_add()::depth out of recursion"));

	if (CAFile_hasSubObject(caFile, parent, *name))
		retError(clean, Error_alreadyDefined(0, "CAFile_add()::name already exists in parent"));

	U16 parentId = (U16)CAHandle_getId(parent);		//0 = root

	if (isFile) {

		if (caFile->files.length >= ((U64)1 << 48))
			retError(clean, Error_overflow(0, caFile->files.length, (U64)1 << 48, "CAFile_add()::too many files"));

		CAFolderInfo *par = &caFile->folders.ptrNonConst[parentId];

		//Insert position: end of parent's contiguous file block.
		//If parent has no files yet, initialize fileOffset to the insert point.

		U64 insertAt =
			par->fileCount ? par->fileOffset + par->fileCount :
			caFile->files.length;

		if (!par->fileCount)
			par->fileOffset = insertAt;

		CAFileInfo fi = CAFileInfo_create(parentId, time);
		gotoIfError3(clean, ListCAFileInfo_insert(&caFile->files, insertAt, fi, alloc, e_rr));

		Buffer empty = Buffer_createNull();
		gotoIfError3(clean, DLFile_insertEntry(&caFile->content, insertAt, &empty, alloc, e_rr));

		//File names live after all folder names; insert at folders.length + insertAt

		gotoIfError3(clean, DLFile_insertEntryString(&caFile->names, caFile->folders.length + insertAt, name, alloc, e_rr));

		++par->fileCount;

		//Bump every other folder's fileOffset that starts at or after the insert point

		for (U64 i = 0; i < caFile->folders.length; ++i)
			if (i != parentId && caFile->folders.ptr[i].fileOffset >= insertAt)
				++caFile->folders.ptrNonConst[i].fileOffset;

		result = CAHandle_makeFile(insertAt);

	} else {

		if (caFile->folders.length >= U16_MAX)
			retError(clean, Error_overflow(0, caFile->folders.length, U16_MAX, "CAFile_add()::too many folders"));

		CAFolderInfo *par = &caFile->folders.ptrNonConst[parentId];

		//Insert position: end of parent's contiguous dir block.
		//If parent has no subdirs yet, initialise dirOffset to the insert point.

		U16 insertAt =
			par->dirCount ? par->dirOffset + par->dirCount :
			(U16)caFile->folders.length;

		if (!par->dirCount)
			par->dirOffset = insertAt;

		CAFolderInfo fi = {
			.parent     = parentId,
			.dirOffset  = 0,
			.dirCount   = 0,
			.fileCount  = 0,
			.fileOffset = 0
		};

		gotoIfError3(clean, ListCAFolderInfo_insert(&caFile->folders, insertAt, fi, alloc, e_rr));
		par = &caFile->folders.ptrNonConst[parentId];

		//Folder name lives at index insertAt in the names DLFile

		gotoIfError3(clean, DLFile_insertEntryString(&caFile->names, insertAt, name, alloc, e_rr));

		++par->dirCount;

		//Fix up all parent/dirOffset references shifted by the insert

		for (U64 i = 0; i < caFile->folders.length; ++i) {

			if (i == insertAt || i == parentId)
				continue;

			if (caFile->folders.ptr[i].parent >= insertAt)
				++caFile->folders.ptrNonConst[i].parent;

			if (caFile->folders.ptr[i].dirOffset >= insertAt)
				++caFile->folders.ptrNonConst[i].dirOffset;
		}

		for (U64 i = 0; i < caFile->files.length; ++i) {

			CAFileInfo *fileInfo = &caFile->files.ptrNonConst[i];
			U16 par2 = CAFileInfo_getParent(*fileInfo);

			if (par2 >= insertAt)
				caFile->files.ptrNonConst[i] = CAFileInfo_create(par2 + 1, CAFileInfo_getTimestamp(*fileInfo));
		}

		result = CAHandle_makeFolder(insertAt);
	}

clean:
	return s_uccess ? result : CAHandle_Invalid;
}

//Remove

Bool CAFile_remove(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile)
		retError(clean, Error_nullPointer(0, "CAFile_remove()::caFile is required"));

	if (fileHandle == CAHandle_Invalid || CAHandle_isRoot(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_remove() Cannot remove root or invalid handle"));

	if (CAHandle_isFolder(fileHandle)) {

		const CAFolderInfo *fi = CAFile_getFolderInfoPtr(caFile, fileHandle);

		if (!fi)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_remove() Invalid folder handle"));

		if (fi->dirCount || fi->fileCount)
			retError(clean, Error_invalidOperation(0, "CAFile_remove() Folder is not empty, use CAFile_removeFolder"));

		U64 id = CAHandle_getId(fileHandle);
		U16 parentId = fi->parent;

		gotoIfError3(clean, DLFile_remove(&caFile->names, id, alloc, e_rr));
		gotoIfError3(clean, ListCAFolderInfo_erase(&caFile->folders, id, e_rr));

		CAFolderInfo *par = &caFile->folders.ptrNonConst[parentId];

		--par->dirCount;

		if (!par->dirCount)
			par->dirOffset = 0;

		//Fix up references after removal; skip root (index 0, parent = self)
		
		for (U64 i = 1; i < caFile->folders.length; ++i) {

			if (caFile->folders.ptr[i].parent > 0 && caFile->folders.ptr[i].parent >= id)
				--caFile->folders.ptrNonConst[i].parent;

			if (caFile->folders.ptr[i].dirOffset > id)
				--caFile->folders.ptrNonConst[i].dirOffset;
		}

		for (U64 i = 0; i < caFile->files.length; ++i) {

			CAFileInfo *fileInfo = &caFile->files.ptrNonConst[i];
			U16 par2 = CAFileInfo_getParent(*fileInfo);

			if (par2 > 0 && par2 >= id)
				caFile->files.ptrNonConst[i] = CAFileInfo_create(par2 - 1, CAFileInfo_getTimestamp(*fileInfo));
		}

	} else {

		U64 id = CAHandle_getId(fileHandle);

		if (id >= caFile->files.length)
			retError(clean, Error_outOfBounds(1, id, caFile->files.length, "CAFile_remove()::file id out of bounds"));

		U16 parentId = CAFileInfo_getParent(caFile->files.ptr[id]);

		U64 nameId = caFile->folders.length + id;
		gotoIfError3(clean, DLFile_remove(&caFile->names, nameId, alloc, e_rr));
		gotoIfError3(clean, DLFile_remove(&caFile->content, id, alloc, e_rr));
		gotoIfError3(clean, ListCAFileInfo_erase(&caFile->files, id, e_rr));

		CAFolderInfo *par = &caFile->folders.ptrNonConst[parentId];

		--par->fileCount;

		if (!par->fileCount)
			par->fileOffset = 0;

		for (U64 i = 0; i < caFile->folders.length; ++i)
			if (caFile->folders.ptr[i].fileOffset > id)
				--caFile->folders.ptrNonConst[i].fileOffset;
	}

clean:
	return s_uccess;
}

Bool CAFile_removeFile(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile)
		retError(clean, Error_nullPointer(0, "CAFile_removeFile()::caFile is required"));

	if (!CAHandle_isFile(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_removeFile()::handle must be a file"));

	gotoIfError3(clean, CAFile_remove(caFile, fileHandle, alloc, e_rr));

clean:
	return s_uccess;
}

Bool CAFile_removeFolder(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile)
		retError(clean, Error_nullPointer(0, "CAFile_removeFolder()::caFile is required"));

	if (!CAHandle_isFolder(fileHandle) || CAHandle_isRoot(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_removeFolder()::handle must be a non-root folder"));

	const CAFolderInfo *fi = CAFile_getFolderInfoPtr(caFile, fileHandle);

	if (!fi)
		retError(clean, Error_invalidParameter(1, 0, "CAFile_removeFolder()::invalid folder handle"));

	//Snapshot before recursion invalidates the pointer
	U64 fileOffset = fi->fileOffset;
	U16 fileCount  = fi->fileCount;
	U16 dirOffset  = fi->dirOffset;
	U16 dirCount   = fi->dirCount;

	for (U16 i = fileCount; i > 0; --i)
		gotoIfError3(clean, CAFile_remove(caFile, CAHandle_makeFile(fileOffset + i - 1), alloc, e_rr));

	for (U16 i = dirCount; i > 0; --i)
		gotoIfError3(clean, CAFile_removeFolder(caFile, CAHandle_makeFolder(dirOffset + i - 1), alloc, e_rr));

	gotoIfError3(clean, CAFile_remove(caFile, fileHandle, alloc, e_rr));

clean:
	return s_uccess;
}

//Info

Bool CAFile_getInfo(const CAFile *caFile, CAHandle handle, FileInfo *info, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile || !info)
		retError(clean, Error_nullPointer(!caFile ? 0 : 2, "CAFile_getInfo()::caFile and info are required"));

	if (handle == CAHandle_Invalid)
		retError(clean, Error_invalidParameter(1, 0, "CAFile_getInfo()::invalid handle"));

	if(info->path.ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "CAFile_getInfo()::info was allocated, indicating possible memleak"
		));

	gotoIfError3(clean, CAFile_getFullName(caFile, handle, alloc, &info->path, e_rr));

	if (CAHandle_isFolder(handle)) {
		info->type   = EFileType_Folder;
		info->access = EFileAccess_ReadWrite;
	} else {

		const CAFileInfo *fi = CAFile_getFileInfoPtr(caFile, handle);

		if (!fi)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_getInfo()::invalid file handle"));

		info->type      = EFileType_File;
		info->access    = EFileAccess_ReadWrite;
		info->fileSize  = DLFile_entrySize(&caFile->content, CAHandle_getId(handle));
		info->timestamp = CAFileInfo_getTimestamp(*fi);
	}

clean:

	if (!s_uccess && info)
		CharString_free(&info->path, alloc);

	return s_uccess;
}

//Foreach

static Bool CAFile_foreachHelper(
	const CAFile *caFile,
	CAHandle handle,
	FileCallback callback,
	void *object,
	Bool recurse,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	if (!caFile || !callback)
		retError(clean, Error_nullPointer(0, "CAFile_foreachHelper()::caFile and callback are required"));

	U64 dirCount = CAFile_dirCount(caFile, handle, false);

	for (U64 i = 0; i < dirCount; ++i) {

		CAHandle child = CAFile_dirAt(caFile, handle, i);

		FileInfo info = { 0 };
		gotoIfError3(clean, CAFile_getInfo(caFile, child, &info, alloc, e_rr));

		Bool cont = callback(&info, object, e_rr);
		FileInfo_free(&info, alloc);

		if (!cont || (e_rr && e_rr->genericError))
			goto clean;

		if (recurse)
			gotoIfError3(clean, CAFile_foreachHelper(caFile, child, callback, object, true, alloc, e_rr));
	}

	U64 fileCount = CAFile_fileCount(caFile, handle, false);

	for (U64 i = 0; i < fileCount; ++i) {

		CAHandle child = CAFile_fileAt(caFile, handle, i);

		FileInfo info = { 0 };
		gotoIfError3(clean, CAFile_getInfo(caFile, child, &info, alloc, e_rr));

		Bool cont = callback(&info, object, e_rr);
		FileInfo_free(&info, alloc);

		if (!cont || (e_rr && e_rr->genericError))
			goto clean;
	}

clean:
	return s_uccess;
}

Bool CAFile_foreach(
	const CAFile *caFile,
	CAHandle fileHandle,
	FileCallback callback,
	void *object,
	Bool recurse,
	const Allocator *alloc,
	Error *e_rr
) {
	return CAFile_foreachHelper(caFile, fileHandle, callback, object, recurse, alloc, e_rr);
}

Bool CAFile_dataEqual(
	const CAFile *a, CAHandle aFile,
	const CAFile *b, CAHandle bFile,
	const Allocator *alloc,
	ECompareResult *result,
	Error *e_rr
) {
	Bool s_uccess = true;
	RefPtr *aStream = NULL, *bStream = NULL;
	U64 aOff = 0, bOff = 0;

	if (!a || !b || !result)
		retError(clean, Error_nullPointer(
			!a ? 0 : (!b ? 2 : 5),
			"CAFile_dataEqual()::a, b and result are required"
		));

	if (!CAHandle_isFile(aFile) || !CAHandle_isFile(bFile))
		retError(clean, Error_invalidParameter(
			!CAHandle_isFile(aFile) ? 1 : 3, 0,
			"CAFile_dataEqual()::aFile and bFile must be file handles"
		));

	*result = ECompareResult_Eq;

	U64 aSize = CAFile_fileSize(a, aFile);
	U64 bSize = CAFile_fileSize(b, bFile);

	if (aSize != bSize) {
		*result = aSize < bSize ? ECompareResult_Lt : ECompareResult_Gt;
		goto clean;
	}

	Bool aLoaded = CAFile_isLoaded(a, aFile);
	Bool bLoaded = CAFile_isLoaded(b, bFile);

	if (aLoaded && bLoaded) {

		Bool aValid = false, bValid = false;
		Buffer aData = CAFile_getDataConst(a, aFile, &aValid);
		Buffer bData = CAFile_getDataConst(b, bFile, &bValid);

		if (!aValid || !bValid)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::failed to get buffer data"));

		U64 aLen = Buffer_length(aData);
		U64 bLen = Buffer_length(bData);

		if (aLen != bLen) {
			*result = aLen < bLen ? ECompareResult_Lt : ECompareResult_Gt;
			goto clean;
		}

		*result = Buffer_cmp(aData, bData);
		goto clean;
	}

	//At least one side is a stream, normalize both to StreamRef via MemoryStream wrapper if needed

	RefPtrType memType = MemoryStream_makeType(alloc);

	if (aLoaded) {

		Bool aValid = false;
		Buffer aData = CAFile_getDataConst(a, aFile, &aValid);

		if (!aValid)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::failed to get buffer for a"));

		gotoIfError3(clean, MemoryStream_createFromBufferRegion(
			aData, 0, 0, EMemoryStreamFlags_None, &memType, &aStream, e_rr
		));

		aOff = 0;

		bStream = CAFile_getDataStream(b, bFile, &bOff);

		if (!bStream)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::expected stream for b"));

	} else if (bLoaded) {

		Bool bValid = false;
		Buffer bData = CAFile_getDataConst(b, bFile, &bValid);

		if (!bValid)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::failed to get buffer for b"));

		gotoIfError3(clean, MemoryStream_createFromBufferRegion(
			bData, 0, 0, EMemoryStreamFlags_None, &memType, &bStream, e_rr
		));

		bOff = 0;
		aStream = CAFile_getDataStream(a, aFile, &aOff);

		if (!aStream)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::expected stream for a"));

	} else {

		aStream = CAFile_getDataStream(a, aFile, &aOff);
		bStream = CAFile_getDataStream(b, bFile, &bOff);

		if (!aStream || !bStream)
			retError(clean, Error_invalidState(0, "CAFile_dataEqual()::expected streams on both sides"));
	}

	gotoIfError3(clean, Stream_compare(
		aStream, bStream,
		aOff, bOff,
		CAFile_fileSize(a, aFile),
		0,
		alloc,
		result,
		e_rr
	));

clean:
	RefPtr_dec(&aStream);
	RefPtr_dec(&bStream);
	return s_uccess;
}
