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

//formats/oiCA/ca_edit.c

#include "types/container/ref_ptr.h"
#include "types/base/string_read_helper.h"
#include "types/base/mathi.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"

static inline U64 CAFile_computePathLen(const CAFile *caFile, CAHandle parent, U64 newNameLen, U64 *depth) {

	U64 total = newNameLen;
	CAHandle cur = parent;

	if (depth)
		*depth = 0;

	while (!CAHandle_isRoot(cur)) {

		CharString seg = CAFile_getName(caFile, cur);
		U64 segLen = CharString_length(seg);

		if (depth)
			++*depth;

		if (!segLen)
			break;

		total += segLen + 1;    //+1 for '/'

		const CAFolderInfo *fi = CAFile_getFolderInfoPtr(caFile, cur);

		if (!fi)
			break;

		cur = CAHandle_makeFolder(fi->parent);
	}

	return total;
}

//Whether everything below a folder keeps a legal path and depth once the folder sits at depth 'depth' with a path of
// 'pathLen' chars (its own path after a move or rename).

static Bool CAFile_checkSubtreeFits(const CAFile *caFile, U64 srcId, U64 pathLen, U64 depth, Error *e_rr) {

	Bool s_uccess = true;

	for (U64 i = 1; i < caFile->folders.length; ++i) {

		if (i == srcId)
			continue;

		U64 len = 0, levels = 0, cur = i;

		while (cur && cur != srcId) {

			len += CharString_length(CAFile_getName(caFile, CAHandle_makeFolder(cur))) + 1;
			++levels;
			cur = caFile->folders.ptr[cur].parent;
		}

		if (cur != srcId)
			continue;

		if (pathLen + len > CAFile_maxFilePathSize)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_checkSubtreeFits()::a child's path would exceed 192 chars"));

		if (depth + levels >= CAFile_maxRecursionSize)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_checkSubtreeFits()::a child's depth out of recursion"));
	}

	for (U64 i = 0; i < caFile->files.length; ++i) {

		U64 len = CharString_length(CAFile_getName(caFile, CAHandle_makeFile(i))) + 1, levels = 0;
		U64 cur = CAFileInfo_getParent(caFile->files.ptr[i]);

		while (cur && cur != srcId) {

			len += CharString_length(CAFile_getName(caFile, CAHandle_makeFolder(cur))) + 1;
			++levels;
			cur = caFile->folders.ptr[cur].parent;
		}

		if (cur != srcId)
			continue;

		if (pathLen + len > CAFile_maxFilePathSize)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_checkSubtreeFits()::a child's path would exceed 192 chars"));

		if (depth + 1 + levels >= CAFile_maxRecursionSize)
			retError(clean, Error_invalidParameter(1, 0, "CAFile_checkSubtreeFits()::a child's depth out of recursion"));
	}

clean:
	return s_uccess;
}

//Reorders the entries of a DLFile in place; entry 'base + i' becomes the entry that was at 'base + src[i]'.
//scratch has to hold count entries of both lists.

static void CAFile_permuteEntries(DLFile *dlFile, U64 base, const U64 *src, U64 count, U8 *scratch) {

	const Bool isString = dlFile->settings.dataType == EDLDataType_String;
	const U64 elemSize = isString ? sizeof(CharString) : sizeof(Buffer);

	U8 *list = isString ? (U8*) dlFile->entryStrings.ptrNonConst : (U8*) dlFile->entryBuffers.ptrNonConst;
	U8 *streams = (U8*) dlFile->entryStreams.ptrNonConst;

	U8 *listTmp = scratch;
	U8 *streamTmp = scratch + count * elemSize;

	for (U64 i = 0; i < count; ++i) {

		Buffer_memcpy(
			Buffer_createRef(listTmp + i * elemSize, elemSize),
			Buffer_createRefConst(list + (base + src[i]) * elemSize, elemSize)
		);

		Buffer_memcpy(
			Buffer_createRef(streamTmp + i * sizeof(DLEntryStream), sizeof(DLEntryStream)),
			Buffer_createRefConst(streams + (base + src[i]) * sizeof(DLEntryStream), sizeof(DLEntryStream))
		);
	}

	Buffer_memcpy(
		Buffer_createRef(list + base * elemSize, count * elemSize), Buffer_createRefConst(listTmp, count * elemSize)
	);

	Buffer_memcpy(
		Buffer_createRef(streams + base * sizeof(DLEntryStream), count * sizeof(DLEntryStream)),
		Buffer_createRefConst(streamTmp, count * sizeof(DLEntryStream))
	);
}

//Rebuilds the order of folders and files from the parent links alone.
//Folders are ordered breadth first: the root, then its folders, then theirs; so a folder always precedes its children
// and the folders of one parent are contiguous. Files are grouped by folder in that same order.
//Within a parent the previous relative order is kept. Handles are invalidated.
//Nothing is changed unless the whole rebuild can be done.

static Bool CAFile_relayout(CAFile *caFile, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	Buffer work = Buffer_createNull();
	Buffer scratch = Buffer_createNull();

	const U64 folderCount = caFile->folders.length;
	const U64 fileCount = caFile->files.length;

	//Index arrays: order (new -> old folder), newOf (old -> new folder), kidStart (+1 entry), kids, next,
	// fileStart (per new folder), fileOrder (new -> old file); then the rebuilt folder and file infos.

	const U64 words =
		6 * folderCount + 1 + fileCount + (sizeof(CAFolderInfo) / sizeof(U64)) * folderCount + fileCount;

	gotoIfError3(clean, Buffer_createUninitializedBytes(words * sizeof(U64), alloc, &work, e_rr));
	Buffer_unsetAllBits(work, NULL);

	const U64 scratchEntries = U64_max(folderCount, fileCount);
	const U64 scratchBytes = scratchEntries * (sizeof(Buffer) + sizeof(CharString) + sizeof(DLEntryStream));

	gotoIfError3(clean, Buffer_createUninitializedBytes(scratchBytes, alloc, &scratch, e_rr));

	U64 *order     = (U64*) work.ptrNonConst;
	U64 *newOf     = order + folderCount;
	U64 *kidStart  = newOf + folderCount;
	U64 *kids      = kidStart + folderCount + 1;
	U64 *next      = kids + folderCount;
	U64 *fileStart = next + folderCount;
	U64 *fileOrder = fileStart + folderCount;

	CAFolderInfo *newFolders = (CAFolderInfo*) (fileOrder + fileCount);
	CAFileInfo *newFiles = (CAFileInfo*) (newFolders + folderCount);

	//Folders sorted by parent (a counting sort keeps the old order within a parent)

	for (U64 i = 1; i < folderCount; ++i) {

		U64 parent = caFile->folders.ptr[i].parent;

		if (parent >= folderCount || parent == i)
			retError(clean, Error_invalidState(0, "CAFile_relayout() folder has an invalid parent"));

		++kidStart[parent + 1];
	}

	for (U64 i = 0; i < folderCount; ++i)
		kidStart[i + 1] += kidStart[i];

	for (U64 i = 0; i < folderCount; ++i)
		next[i] = kidStart[i];

	for (U64 i = 1; i < folderCount; ++i)
		kids[next[caFile->folders.ptr[i].parent]++] = i;

	//Breadth first, the queue is the order itself

	U64 queued = 1;

	for (U64 q = 0; q < queued; ++q) {

		const U64 old = order[q];
		const U64 count = kidStart[old + 1] - kidStart[old];

		newFolders[q].dirCount = (U16) count;
		newFolders[q].dirOffset = count ? (U16) queued : 0;

		for (U64 k = kidStart[old]; k < kidStart[old + 1]; ++k)
			order[queued++] = kids[k];
	}

	if (queued != folderCount)
		retError(clean, Error_invalidState(0, "CAFile_relayout() folders aren't all reachable from the root"));

	for (U64 q = 0; q < folderCount; ++q)
		newOf[order[q]] = q;

	//Files grouped by their folder's new index

	for (U64 i = 0; i < fileCount; ++i) {

		U64 parent = CAFileInfo_getParent(caFile->files.ptr[i]);

		if (parent >= folderCount)
			retError(clean, Error_invalidState(0, "CAFile_relayout() file has an invalid parent"));

		++fileStart[newOf[parent]];
	}

	for (U64 q = 0, offset = 0; q < folderCount; ++q) {

		U64 count = fileStart[q];

		newFolders[q].fileCount = (U16) count;
		newFolders[q].fileOffset = count ? offset : 0;
		newFolders[q].parent = q ? (U16) newOf[caFile->folders.ptr[order[q]].parent] : 0;

		fileStart[q] = offset;
		offset += count;
	}

	for (U64 i = 0; i < fileCount; ++i) {

		CAFileInfo info = caFile->files.ptr[i];
		U64 parent = newOf[CAFileInfo_getParent(info)];
		U64 dst = fileStart[parent]++;

		fileOrder[dst] = i;
		newFiles[dst].parentTimestamp = (info.parentTimestamp & (((U64) 1 << 48) - 1)) | (parent << 48);
	}

	//Nothing below can fail

	CAFile_permuteEntries(&caFile->names, 0, order, folderCount, scratch.ptrNonConst);
	CAFile_permuteEntries(&caFile->names, folderCount, fileOrder, fileCount, scratch.ptrNonConst);
	CAFile_permuteEntries(&caFile->content, 0, fileOrder, fileCount, scratch.ptrNonConst);

	Buffer_memcpy(
		Buffer_createRef(caFile->folders.ptrNonConst, folderCount * sizeof(CAFolderInfo)),
		Buffer_createRefConst(newFolders, folderCount * sizeof(CAFolderInfo))
	);

	Buffer_memcpy(
		Buffer_createRef(caFile->files.ptrNonConst, fileCount * sizeof(CAFileInfo)),
		Buffer_createRefConst(newFiles, fileCount * sizeof(CAFileInfo))
	);

clean:
	Buffer_free(&work, alloc);
	Buffer_free(&scratch, alloc);
	return s_uccess;
}

//Rename / move

Bool CAFile_rename(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, CharString *name, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

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

	U64 depth = 0;
	U64 pathLen = CAFile_computePathLen(caFile, parent, CharString_length(*name), &depth);

	if (pathLen > CAFile_maxFilePathSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_rename()::full path would exceed 192 chars"));

	if (CAHandle_isFolder(fileHandle))
		gotoIfError3(clean, CAFile_checkSubtreeFits(caFile, CAHandle_getId(fileHandle), pathLen, depth, e_rr));

	if (CAFile_hasSubObject(caFile, parent, *name))
		retError(clean, Error_alreadyDefined(0, "CAFile_rename()::name already exists in parent"));

	U64 nameIdx =
		CAHandle_isFolder(fileHandle) ? CAHandle_getId(fileHandle) :
		caFile->folders.length + CAHandle_getId(fileHandle);

	//A ref doesn't own its characters, so the archive takes a copy and leaves the caller's string alone.

	CharString *entry = name;

	if (CharString_isRef(*name)) {
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &copy, e_rr));
		entry = &copy;
	}

	gotoIfError3(clean, DLFile_setEntryString(&caFile->names, nameIdx, entry, alloc, e_rr));

clean:
	CharString_free(&copy, alloc);
	return s_uccess;
}

Bool CAFile_move(CAFile *caFile, CAHandle fileHandle, CAHandle newParent, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile)
		retError(clean, Error_nullPointer(0, "CAFile_move()::caFile is required"));

	if (fileHandle == CAHandle_Invalid || CAHandle_isRoot(fileHandle))
		retError(clean, Error_invalidParameter(1, 0, "CAFile_move()::cannot move root or invalid handle"));

	if (!CAHandle_isFolder(newParent))
		retError(clean, Error_invalidParameter(2, 0, "CAFile_move()::newParent must be a folder"));

	U64 pid = CAHandle_getId(newParent);

	if (pid >= caFile->folders.length)
		retError(clean, Error_outOfBounds(2, pid, caFile->folders.length, "CAFile_move()::newParent out of bounds"));

	const Bool isFile = CAHandle_isFile(fileHandle);
	U64 srcId = CAHandle_getId(fileHandle);

	if (srcId >= (isFile ? caFile->files.length : caFile->folders.length))
		retError(clean, Error_outOfBounds(
			1, srcId, isFile ? caFile->files.length : caFile->folders.length,
			"CAFile_move()::fileHandle out of bounds"
		));

	//No-op, move to the folder it's already in

	U64 oldParentId = isFile ? (U64) CAFileInfo_getParent(caFile->files.ptr[srcId]) : (U64) caFile->folders.ptr[srcId].parent;

	if (oldParentId == pid)
		goto clean;

	//Can't move a folder into itself or a descendant

	if (!isFile)
		for (U64 cur = pid; cur; cur = caFile->folders.ptr[cur].parent)
			if (cur == srcId)
				retError(clean, Error_invalidParameter(2, 0, "CAFile_move()::cannot move into itself or its children"));

	CharString name = CAFile_getName(caFile, fileHandle);

	U64 depth = 0;
	U64 pathLen = CAFile_computePathLen(caFile, newParent, CharString_length(name), &depth);

	if (pathLen > CAFile_maxFilePathSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_move()::full path would exceed 192 chars"));

	if(depth >= CAFile_maxRecursionSize)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_move()::depth out of recursion"));

	if (!isFile)
		gotoIfError3(clean, CAFile_checkSubtreeFits(caFile, srcId, pathLen, depth, e_rr));

	if (CAFile_hasSubObject(caFile, newParent, name))
		retError(clean, Error_alreadyDefined(0, "CAFile_move()::name already exists in destination"));

	if (isFile && caFile->folders.ptr[pid].fileCount == U16_MAX)
		retError(clean, Error_overflow(0, U16_MAX, U16_MAX, "CAFile_move()::too many files in the destination folder"));

	//Only the parent link changes, the order of folders and files is rebuilt from the links afterwards.
	//The rebuild fails before it changes anything, so the link can be put back.

	if (isFile) {
		CAFileInfo *info = &caFile->files.ptrNonConst[srcId];
		info->parentTimestamp = (info->parentTimestamp & (((U64) 1 << 48) - 1)) | (pid << 48);
	}

	else caFile->folders.ptrNonConst[srcId].parent = (U16) pid;

	if (!CAFile_relayout(caFile, alloc, e_rr)) {

		if (isFile) {
			CAFileInfo *info = &caFile->files.ptrNonConst[srcId];
			info->parentTimestamp = (info->parentTimestamp & (((U64) 1 << 48) - 1)) | (oldParentId << 48);
		}

		else caFile->folders.ptrNonConst[srcId].parent = (U16) oldParentId;

		s_uccess = false;
		goto clean;
	}

	//A completed move shifts list indices, so bump the debug generation counter to mark prior CAHandles stale.
	++caFile->version;

clean:
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

	U16 parentId = (U16) CAHandle_getId(parent);        //0 = root

	if (isFile) {

		if (caFile->files.length >= ((U64) 1 << 48))
			retError(clean, Error_overflow(0, caFile->files.length, (U64) 1 << 48, "CAFile_add()::too many files"));

		CAFolderInfo *par = &caFile->folders.ptrNonConst[parentId];

		//The per folder count is a U16 in memory and on disk.

		if (par->fileCount == U16_MAX)
			retError(clean, Error_overflow(0, U16_MAX, U16_MAX, "CAFile_add()::too many files in one folder"));

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
			(U16) caFile->folders.length;

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

	//A completed add shifts list indices, so bump the debug generation counter to mark prior CAHandles stale.
	++caFile->version;

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

	//A completed remove shifts list indices, so bump the debug generation counter to mark prior CAHandles stale.
	++caFile->version;

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
