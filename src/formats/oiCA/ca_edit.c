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

		total += segLen + 1;	//+1 for '/'

		const CAFolderInfo *fi = CAFile_getFolderInfoPtr(caFile, cur);

		if (!fi)
			break;

		cur = CAHandle_makeFolder(fi->parent);
	}

	return total;
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
