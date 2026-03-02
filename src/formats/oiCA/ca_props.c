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

#include "types/container/ref_ptr.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiDL/interface.h"

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
