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

#include "types/base/string_read_helper.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_load.h"

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
