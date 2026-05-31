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

//platforms/file.c

#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/types.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "types/container/buffer.h"
#include "types/container/list.h"
#include "types/container/string_helper.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"
#include "types/base/thread.h"
#include "types/base/mathi.h"
#include "types/base/error.h"
#include "types/base/constants.h"

#include <stdio.h>
#include <sys/stat.h>

//Private file functions

typedef Bool (*VirtualFileFunc)(void *userData, const CharString *resolved, const Allocator *alloc, Error *e_rr);

Bool File_virtualOp(
	const CharString *loc,
	Ns maxTimeout,
	VirtualFileFunc f,
	void *userData,
	Bool isWrite,
	const Allocator *alloc,
	Error *e_rr
);

//These are write operations, we don't support them yet. (TODO:)

static inline Bool File_removeVirtual(const CharString *loc, Ns maxTimeout, const Allocator *alloc, Error *e_rr) {
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, alloc, e_rr);
}

static inline Bool File_addVirtual(const CharString *loc, EFileType type, const Allocator *alloc, Error *e_rr) {
	(void)type;
	return File_virtualOp(loc, 0, NULL, NULL, true, alloc, e_rr);
}

static inline Bool File_renameVirtual(
	const CharString *loc,
	const CharString *newFileName,
	Ns maxTimeout,
	const Allocator *alloc,
	Error *e_rr
) {
	(void)newFileName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, alloc, e_rr);
}

static inline Bool File_moveVirtual(
	const CharString *loc,
	const CharString *directoryName,
	Ns maxTimeout,
	const Allocator *alloc,
	Error *e_rr
) {
	(void)directoryName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, alloc, e_rr);
}

static inline Bool File_writeVirtual(
	const Buffer *buf,
	const CharString *loc,
	Ns maxTimeout,
	const Allocator *alloc,
	Error *e_rr
) {
	(void)buf;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, alloc, e_rr);
}

//Works on almost all virtual files

Bool File_readVirtual(const CharString *loc, Buffer *output, Ns maxTimeout, const Allocator *alloc, Error *e_rr);

Bool File_getInfoVirtual(const CharString *loc, FileInfo *info, const Allocator *alloc, Error *e_rr);

Bool File_foreachVirtual(
	const CharString *loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	const Allocator *alloc,
	Error *e_rr
);

Bool File_queryFileObjectCountVirtual(
	const CharString *loc,
	EFileType type,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
);

Bool File_queryFileObjectCountAllVirtual(
	const CharString *loc,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
);

//Physical implementations
//These all assume we pass them a null terminated string that has been fully resolved.
//These make their way directly to the OS. They'll be limited to 1023 for portability reasons.
//    Windows: \\?\ need to be fully resolved; \\?\C:\programming\test (not //?/C:/test or //?C:test)
//    Unix:    Null terminated string

impl Bool File_getInfoPhysical(const CharString *str, FileInfo *info, const Allocator *alloc, Error *e_rr);
impl Bool File_addPhysical(const CharString *str, Bool isFile, const Allocator *alloc, Error *e_rr);
impl Bool File_removePhysical(const CharString *str, Ns maxTimeout, const Allocator *alloc, Error *e_rr);

impl Bool File_renamePhysical(
	const CharString *loc,
	const CharString *newFileName,
	Ns maxTimeout,
	const Allocator *alloc,
	Error *e_rr
);

impl Bool File_movePhysical(
	const CharString *loc,
	const CharString *directoryName,
	Ns maxTimeout,
	const Allocator *alloc,
	Error *e_rr
);

impl Bool File_openPhysical(
	const CharString *resolved,
	Ns maxTimeout,
	EFileOpenType type,
	FileHandle *fileHandle,
	const Allocator *alloc,
	Error *e_rr
);

impl Bool FileHandle_writePhysical(FileHandle *handle, U64 offset, U64 length, const Buffer *buf, Error *e_rr);
impl Bool FileHandle_readPhysical(FileHandle *handle, U64 offset, U64 length, Buffer *buf, Error *e_rr);

impl void FileHandle_closePhysical(const void *handle, const Allocator *alloc);

//Generic implementations (can call both virtual/physical and handle all resolve logic)

Bool File_getInfo(const CharString *loc, FileInfo *info, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!loc || !info)
		retError(clean, Error_nullPointer(!loc ? 0 : 1, "File_getInfo()::loc and info are required"));

	if(info->path.ptr)
		retError(clean, Error_invalidOperation(0, "File_getInfo()::info was already defined, may indicate memleak"));

	if(!CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_getInfo()::loc wasn't a valid file path"));

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_getInfoVirtual(loc, info, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));
	gotoIfError3(clean, File_getInfoPhysical(&resolved, info, alloc, e_rr));

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

typedef struct FileCounter {
	EFileType type;
	Bool useType;
	U64 counter;
} FileCounter;

static Bool countFileType(const FileInfo *info, FileCounter *counter, const Allocator *alloc, Error *e_rr) {

	(void) e_rr; (void) alloc;

	if (!counter->useType) {
		++counter->counter;
		return true;
	}

	if(info->type == counter->type)
		++counter->counter;

	return true;
}

Bool File_queryFileObjectCount(
	const CharString *loc,
	EFileType type,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!loc || !res)
		retError(clean, Error_nullPointer(!loc ? 0 : 3, "File_queryFileObjectCount()::res is required"));

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	if(!CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_queryFileObjectCount()::res must be a valid file path"));

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_queryFileObjectCountVirtual(loc, type, isRecursive, res, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	//Normal counter for local files

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	gotoIfError3(clean, File_foreach(loc, false, (FileCallback) countFileType, &counter, isRecursive, alloc, e_rr));
	*res = counter.counter;

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool File_queryFileObjectCountAll(const CharString *loc, Bool isRecursive, U64 *res, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!loc || !res)
		retError(clean, Error_nullPointer(!loc ? 0 : 2, "File_queryFileObjectCountAll()::res is required"));

	if(!CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_queryFileObjectCountAll()::res must be a valid file path"));

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_queryFileObjectCountAllVirtual(loc, isRecursive, res, alloc, e_rr));
		goto clean;
	}

	//Normal counter for local files

	FileCounter counter = (FileCounter) { 0 };
	gotoIfError3(clean, File_foreach(
		loc, !Platform_instance->useWorkingDir, (FileCallback) countFileType, &counter, isRecursive, alloc, e_rr
	));

	*res = counter.counter;

clean:
	return s_uccess;
}

Bool File_add(
	const CharString *loc,
	EFileType type,
	Bool createParentOnly,
	const Allocator *alloc,
	Error *e_rr
) {
	CharString resolved = CharString_createNull();
	ListCharString str = (ListCharString) { 0 };
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = true;

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_add()::loc must be a valid file path"));

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_addVirtual(loc, type, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	if(!CharString_containsSensitive(&resolved, '/', 0, 0)) {
		gotoIfError3(clean, File_addPhysical(&resolved, type == EFileType_File, alloc, e_rr));
		goto clean;
	}

	{
		if(!CharString_startsWithStringInsensitive(&resolved, &Platform_instance->defaultDir, 0))
			retError(clean, Error_unauthorized(0, "File_add() escaped working directory. This is not supported."));

		const CharStringSplit split = (CharStringSplit) {
			.s = &resolved,
			.allocator = alloc,
			.result = &str
		};

		gotoIfError3(clean, CharString_splitSensitive(&split, '/', e_rr));
	}

	U64 dirCount = str.length - 1;

	U64 startComponent = 0;

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		if(dirCount > 0 && CharString_equals(str.ptr[0], '?', EStringCase_Sensitive))
			startComponent = 1;
	#endif

	//Walk backwards to find deepest existing ancestor

	U64 firstMissing = dirCount;
	for (U64 i = dirCount; i > startComponent; --i) {

		C8 *end = CharString_end(str.ptr[i - 1]);
		C8 prev = *end;
		*end = '\0';

		const C8 *start = str.ptr[0].ptr;

		CharString section = CharString_createRefSizedConst(start, end - start, false);

		Error errTmp = Error_none();
		Bool exists = File_getInfo(&section, &info, alloc, &errTmp);
		EFileType parentType = info.type;
		FileInfo_free(&info, alloc);

		*end = prev;

		if(!exists && errTmp.genericError != EGenericError_NotFound)
			retError(clean, errTmp);

		if(exists) {

			if(parentType != EFileType_Folder)
				retError(clean, Error_invalidOperation(2, "File_add() one of the parents wasn't a folder"));

			break;
		}

		firstMissing = i - 1;
	}

	//Create forward from firstMissing

	for (U64 i = firstMissing; i < dirCount; ++i) {

		C8 *end = CharString_end(str.ptr[i]);
		C8 prev = *end;
		*end = '\0';

		const C8 *start = str.ptr[0].ptr;
		CharString section = CharString_createRefSizedConst(start, end - start, false);

		gotoIfError3(clean, File_addPhysical(&section, false, alloc, e_rr));
		*end = prev;
	}

	if(!createParentOnly)
		gotoIfError3(clean, File_addPhysical(&resolved, type == EFileType_File, alloc, e_rr));

clean:
	FileInfo_free(&info, alloc);
	CharString_free(&resolved, alloc);
	ListCharString_free(&str, alloc);
	return s_uccess;
}

Bool File_remove(const CharString *loc, Ns maxTimeout, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_remove()::loc must be a valid file path"));

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_removeVirtual(loc, maxTimeout, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));
	gotoIfError3(clean, File_removePhysical(&resolved, maxTimeout, alloc, e_rr));

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool File_rename(const CharString *loc, const CharString *newFileName, Ns maxTimeout, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_rename()::loc must be a valid file path"));

	if(!newFileName || !CharString_isValidFileName(*newFileName))
		retError(clean, Error_invalidParameter(1, 0, "File_rename()::newFileName must be a valid file name"));

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_renameVirtual(loc, newFileName, maxTimeout, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	if(!File_has(&resolved, alloc))
		retError(clean, Error_notFound(0, 0, "File_rename()::loc doesn't exist"));

	gotoIfError3(clean, File_renamePhysical(&resolved, newFileName, maxTimeout, alloc, e_rr));

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool File_move(const CharString *loc, const CharString *directoryName, Ns maxTimeout, const Allocator *alloc, Error *e_rr) {

	CharString resolved = CharString_createNull();
	CharString resolvedDir = CharString_createNull();
	CharString resolvedDest = CharString_createNull();
	Bool s_uccess = true;

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_move()::loc must be a valid file path"));

	if(!directoryName || !CharString_isValidFilePath(*directoryName))
		retError(clean, Error_invalidParameter(1, 0, "File_move()::directoryName must be a valid file path"));

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_moveVirtual(loc, directoryName, maxTimeout, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	Bool isVirtual2 = false;
	gotoIfError3(clean, File_resolve(
		directoryName, &isVirtual2, 0, &Platform_instance->defaultDir, alloc, &resolvedDir, e_rr
	));

	if(isVirtual2)
		retError(clean, Error_invalidOperation(0, "File_move() destination can't be a virtual path"));

	if(!File_has(&resolved, alloc))
		retError(clean, Error_notFound(0, 0, "File_move()::loc can't be found"));

	if(!File_hasFolder(&resolvedDir, alloc))
		retError(clean, Error_notFound(0, 1, "File_move()::directoryName can't be found"));

	//Build destination path: resolvedDir / filename
	CharString fileName = CharString_createNull();
	if(!CharString_cutBeforeLastSensitive(&resolved, '/', &fileName))
		fileName = resolved;

	gotoIfError3(clean, CharString_format(
		alloc, &resolvedDest, e_rr, "%.*s/%.*s",
		CharString_length(resolvedDir), resolvedDir.ptr,
		CharString_length(fileName), fileName.ptr
	));

	gotoIfError3(clean, File_movePhysical(&resolved, &resolvedDest, maxTimeout, alloc, e_rr));

clean:
	CharString_free(&resolved, alloc);
	CharString_free(&resolvedDir, alloc);
	CharString_free(&resolvedDest, alloc);
	return s_uccess;
}

void FileHandle_close(FileHandle *handle, const Allocator *alloc) {

	if(!handle)
		return;

	if(handle->ext)
		FileHandle_closePhysical(handle->ext, alloc);

	*handle = (FileHandle) { 0 };
}

RefPtrType FileHandle_makeType(const Allocator *alloc) {
	return (RefPtrType) {
		.typeId = (ETypeId) EPlatformsTypeId_FileHandle,
		.length = (U32) sizeof(FileHandle),
		.alloc = alloc,
		.free = (ObjectFreeFunc) FileHandle_close
	};
}

Bool File_open(
	const CharString *loc,
	Ns maxTimeout,
	EFileOpenType type,
	Bool create,
	const RefPtrType *ptrType,
	FileHandleRef **handle,
	Error *e_rr
) {
	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	Bool created = false;

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_open()::loc must be a valid file path"));

	if(!ptrType || ptrType->length != sizeof(FileHandle) || ptrType->typeId != (ETypeId) EPlatformsTypeId_FileHandle)
		retError(clean, Error_invalidParameter(1, 0, "File_open()::ptrType is invalid"));

	if(!handle)
		retError(clean, Error_nullPointer(2, "File_open()::handle is required"));

	if(*handle)
		retError(clean, Error_invalidOperation(0, "File_open()::handle was filled, may indicate memleak"));

	if(File_isVirtual(*loc))
		retError(clean, Error_invalidOperation(0, "File_open() is not permitted on virtual files, only physical"));

	Bool isVirtual = false;
	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, ptrType->alloc, &resolved, e_rr));

	if(type >= EFileOpenType_Write && create) {

		Error err = Error_none();
		Bool success = File_add(&resolved, EFileType_File, false, ptrType->alloc, &err);

		if(!success && err.genericError != EGenericError_AlreadyDefined)
			retError(clean, err);
	}

	gotoIfError3(clean, RefPtr_create(ptrType, handle, e_rr));
	created = true;

	gotoIfError3(clean, File_openPhysical(&resolved, maxTimeout, type, RefPtr_data(*handle, FileHandle), ptrType->alloc, e_rr));

clean:

	if(created && !s_uccess)
		RefPtr_dec(handle);

	CharString_free(&resolved, ptrType->alloc);
	return s_uccess;
}

Bool FileHandleRef_write(FileHandleRef *handleRef, U64 offset, U64 length, const Buffer *buf, Error *e_rr) {

	Bool s_uccess = true;

	if(!handleRef || !buf)
		retError(clean, Error_nullPointer(!handleRef ? 0 : 3, "FileHandleRef_write()::handle and buf are required"));

	if(!handleRef->refPtrType || handleRef->refPtrType->typeId != (ETypeId) EPlatformsTypeId_FileHandle)
		retError(clean, Error_invalidOperation(0, "FileHandleRef_write()::handle is an invalid type"));

	FileHandle *handle = RefPtr_data(handleRef, FileHandle);

	if(!handle->ext)
		retError(clean, Error_invalidOperation(0, "FileHandleRef_write()::handle is not open"));

	if(!EFileHandle_isWrite(handle))
		retError(clean, Error_invalidOperation(1, "FileHandleRef_write()::handle is not opened for writing"));

	if(!length)
		length = Buffer_length(*buf);

	U64 bufl = Buffer_length(*buf);

	if(length > Buffer_length(*buf))
		retError(clean, Error_outOfBounds(2, length, bufl, "FileHandleRef_write() length exceeds buffer size"));

	if(length)
		gotoIfError3(clean, FileHandle_writePhysical(handle, offset, length, buf, e_rr));

clean:
	return s_uccess;
}

Bool FileHandleRef_read(const FileHandleRef *handleRef, U64 off, U64 len, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	if(!handleRef || !output)
		retError(clean, Error_nullPointer(!handleRef ? 0 : 3, "FileHandleRef_read()::handle and output are required"));

	if(!handleRef->refPtrType || handleRef->refPtrType->typeId != (ETypeId) EPlatformsTypeId_FileHandle)
		retError(clean, Error_invalidOperation(0, "FileHandleRef_read()::handle is an invalid type"));

	FileHandle *handle = RefPtr_data(handleRef, FileHandle);

	if(!handle->ext)
		retError(clean, Error_invalidOperation(0, "FileHandleRef_read()::handle is not open"));

	if(!EFileHandle_isRead(handle))
		retError(clean, Error_invalidOperation(1, "FileHandleRef_read()::handle is not opened for reading"));

	if(Buffer_isConstRef(*output))
		retError(clean, Error_invalidOperation(2, "FileHandleRef_read()::output must be writable"));

	U64 fileSize = FileHandle_fileSize(handle);

	if(!fileSize && !off && !len)        //Empty files exist too
		goto clean;

	if(off >= fileSize)
		retError(clean, Error_outOfBounds(1, off, fileSize, "FileHandleRef_read() offset out of bounds"));

	U64 size = !len ? fileSize - off : len;

	if(off + size > fileSize)
		retError(clean, Error_outOfBounds(2, off + size, fileSize, "FileHandleRef_read() offset + length out of bounds"));

	if(size > Buffer_length(*output))
		retError(clean, Error_outOfBounds(3, size, Buffer_length(*output), "FileHandleRef_read() size exceeds output buffer"));

	if(size)
		gotoIfError3(clean, FileHandle_readPhysical(handle, off, size, output, e_rr));

clean:
	return s_uccess;
}

Bool File_write(
	const Buffer *buf,
	const CharString *loc,
	U64 off,
	U64 len,
	Ns maxTimeout,
	Bool createParent,
	const RefPtrType *ptrType,
	Error *e_rr
) {

	Bool s_uccess = true;
	FileHandleRef *handle = NULL;

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_write()::loc must be a valid file path"));

	if(!ptrType || ptrType->length != sizeof(FileHandle) || ptrType->typeId != (ETypeId) EPlatformsTypeId_FileHandle)
		retError(clean, Error_invalidParameter(1, 0, "File_write()::ptrType is invalid"));

	if(File_isVirtual(*loc)) {
		gotoIfError3(clean, File_writeVirtual(buf, loc, maxTimeout, ptrType->alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_open(loc, maxTimeout, EFileOpenType_Write, createParent, ptrType, &handle, e_rr));
	gotoIfError3(clean, FileHandleRef_write(handle, off, len, buf, e_rr));

clean:
	RefPtr_dec(&handle);
	return s_uccess;
}

Bool File_read(
	const CharString *loc,
	Ns maxTimeout,
	U64 off,
	U64 len,
	const RefPtrType *ptrType,
	Buffer *output,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocate = false;
	FileHandleRef *handle = NULL;

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_read()::loc must be a valid file path"));

	if(!ptrType || ptrType->length != sizeof(FileHandle) || ptrType->typeId != (ETypeId) EPlatformsTypeId_FileHandle)
		retError(clean, Error_invalidParameter(1, 0, "File_read()::ptrType is invalid"));

	if(!output)
		retError(clean, Error_nullPointer(2, "File_read()::output is required"));

	if(output->ptr)
		retError(clean, Error_invalidOperation(0, "File_read()::output was filled, may indicate memleak"));

	if(File_isVirtual(*loc)) {
		gotoIfError3(clean, File_readVirtual(loc, output, maxTimeout, ptrType->alloc, e_rr));
		allocate = true;
		goto clean;
	}

	gotoIfError3(clean, File_open(loc, maxTimeout, EFileOpenType_Read, false, ptrType, &handle, e_rr));

	{
		const FileHandle *fh = RefPtr_data(handle, FileHandle);
		U64 fileSize = FileHandle_fileSize(fh);

		if(!fileSize && !off && !len)        //Empty files exist too
			goto clean;

		if(off >= fileSize)
			retError(clean, Error_invalidOperation(0, "File_read() offset out of bounds"));

		U64 size = !len ? fileSize - off : len;

		gotoIfError3(clean, Buffer_createUninitializedBytes(size, ptrType->alloc, output, e_rr));
		allocate = true;

		gotoIfError3(clean, FileHandleRef_read(handle, off, size, output, e_rr));
	}

clean:
	if(!s_uccess && allocate)
		Buffer_free(output, ptrType->alloc);

	RefPtr_dec(&handle);
	return s_uccess;
}

//Virtual file support

Bool File_virtualOp(
	const CharString *loc,
	Ns maxTimeout,
	VirtualFileFunc f,
	void *userData,
	Bool isWrite,
	const Allocator *alloc,
	Error *e_rr
) {

	(void)maxTimeout;

	Bool s_uccess = true;
	Bool isVirtual = false;
	CharString resolved = CharString_createNull();

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	if(!isVirtual)
		retError(clean, Error_unsupportedOperation(0, "File_virtualOp()::loc should resolve to virtual path (//*)"));

	const CharString access = CharString_createRefCStrConst("access/");
	const CharString function = CharString_createRefCStrConst("function/");
	const CharString network = CharString_createRefCStrConst("network/");

	if (CharString_startsWithStringInsensitive(&resolved, &access, 0)) {
		//TODO: Allow //access folder
		retError(clean, Error_unimplemented(0, "File_virtualOp()::loc //access/ not supported yet"));
	}

	if (CharString_startsWithStringInsensitive(&resolved, &function, 0)) {
		//TODO: Allow //function folder (user callbacks)
		retError(clean, Error_unimplemented(1, "File_virtualOp()::loc //function/ not supported yet"));
	}

	if (CharString_startsWithStringInsensitive(&resolved, &network, 0)) {
		//TODO: Allow //network folder (access to \\ on windows)
		retError(clean, Error_unimplemented(1, "File_virtualOp()::loc //network/ not supported yet"));
	}

	if(isWrite)
		retError(clean, Error_constData(
			1, 0,
			"File_virtualOp()::isWrite is disallowed when acting on virtual files "
			"(except //access/*, //network/* or //function/*)"
		));

	if(!f)
		retError(clean, Error_unimplemented(2, "File_virtualOp()::f is required"));

	gotoIfError3(clean, f(userData, &resolved, alloc, e_rr));

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

//Read operations

Bool File_resolveVirtual(
	const CharString *loc,
	CharString *subPath,
	const VirtualSection **section,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	CharString locLower  = CharString_createNull();
	CharString locSlash  = CharString_createNull();        //locLower + trailing /
	CharString secSlash  = CharString_createNull();        //section->path + trailing /

	if(!loc || !subPath || !section)
		retError(clean, Error_nullPointer(!loc ? 0 : (!subPath ? 2 : 3), "File_resolveVirtual()::loc is required"));

	*subPath  = CharString_createNull();
	*section  = NULL;

	//Root always exists, nothing to resolve

	if(!CharString_length(*loc))
		goto clean;

	if(!SpinLock_isLockedForThread(&Platform_instance->virtualSectionsLock))
		retError(clean, Error_invalidState(0, "File_resolveVirtual() requires virtualSectionsLock"));

	gotoIfError3(clean, CharString_createCopy(*loc, alloc, &locLower, e_rr));
	CharString_toLower(&locLower);

	gotoIfError3(clean, CharString_createCopy(locLower, alloc, &locSlash, e_rr));
	gotoIfError3(clean, CharString_append(&locSlash, '/', alloc, e_rr));

	for(U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {
		const VirtualSection *sec = Platform_instance->virtualSections.ptr + i;

		//Exact match on section root, it's a folder, no subPath
		if(CharString_equalsStringInsensitive(&locLower, &sec->path)) {
			*section = sec;
			goto clean;
		}

		//loc is a parent directory of this section, it's a folder, no subPath
		if(CharString_startsWithStringInsensitive(&sec->path, &locSlash, 0)) {

			if(sec->loadedAndId)
				goto clean;

			continue;
		}

		//loc is inside this section, return subPath relative to section
		CharString_free(&secSlash, alloc);
		gotoIfError3(clean, CharString_createCopy(sec->path, alloc, &secSlash, e_rr));
		gotoIfError3(clean, CharString_append(&secSlash, '/', alloc, e_rr));

		if(CharString_startsWithStringInsensitive(&locLower, &secSlash, 0)) {
			CharString sub = CharString_createNull();
			CharString_cut(&locLower, CharString_length(secSlash), 0, &sub);
			gotoIfError3(clean, CharString_createCopy(sub, alloc, subPath, e_rr));
			*section = sec;
			goto clean;
		}
	}

	retError(clean, Error_notFound(0, 0, "File_resolveVirtual() can't find section"));

clean:
	CharString_free(&locLower, alloc);
	CharString_free(&locSlash, alloc);
	CharString_free(&secSlash, alloc);
	return s_uccess;
}

Bool File_readVirtualInternal(Buffer *output, const CharString *loc, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	CharString subPath  = CharString_createNull();
	const VirtualSection *section = NULL;
	StreamRef *streamRef = NULL;

	const ELockAcquire acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);
	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_readVirtualInternal() couldn't lock virtualSectionsLock"));

	gotoIfError3(clean, File_resolveVirtual(loc, &subPath, &section, alloc, e_rr));

	if(!section || !subPath.ptr)
		retError(clean, Error_invalidOperation(0, "File_readVirtualInternal() loc is a virtual folder, not a file"));

	if(!section->loadedAndId)
		retError(clean, Error_invalidOperation(1, "File_readVirtualInternal() section not loaded"));

	const CAFile *caFile = &Platform_instance->archives.ptr[section->loadedAndId << 1 >> 1];
	CAHandle handle = CAFile_resolve(caFile, subPath);

	if(handle == CAHandle_Invalid)
		retError(clean, Error_notFound(0, 0, "File_readVirtualInternal() file not found in archive"));

	if(!CAHandle_isFile(handle))
		retError(clean, Error_invalidOperation(2, "File_readVirtualInternal() handle is a folder, not a file"));

	//Check if data is a stream or fully loaded

	U64 streamOff = U64_MAX;
	streamRef = CAFile_getDataStream(caFile, handle, &streamOff);

	if(streamRef && streamOff != U64_MAX) {

		U64 fileSize = CAFile_fileSize(caFile, handle);
		gotoIfError3(clean, Buffer_createUninitializedBytes(fileSize, alloc, output, e_rr));

		OxStream *stream = RefPtr_data(streamRef, OxStream);

		gotoIfError3(clean, stream->read(stream, streamOff, fileSize, *output, alloc, e_rr));
		RefPtr_dec(&streamRef);

	} else {

		if(!CAFile_isLoaded(caFile, handle))
			retError(clean, Error_invalidOperation(3, "File_readVirtualInternal() file data not loaded and not a stream"));

		//Data is fully loaded, copy the buffer so caller owns it safely
		//(section could be unloaded in parallel after we release the lock)

		Bool isValid = false;
		Buffer tmp = CAFile_getDataConst(caFile, handle, &isValid);

		if(!isValid)
			retError(clean, Error_invalidOperation(4, "File_readVirtualInternal() CAFile_getDataConst returned invalid"));

		gotoIfError3(clean, Buffer_createCopy(tmp, alloc, output, e_rr));
	}

clean:

	RefPtr_dec(&streamRef);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	CharString_free(&subPath, alloc);
	return s_uccess;
}

Bool File_readVirtual(const CharString *loc, Buffer *output, Ns maxTimeout, const Allocator *alloc, Error *e_rr) {

	if(!output) {
		if(e_rr) *e_rr = Error_nullPointer(1, "File_readVirtual()::output is required");
		return false;
	}

	return File_virtualOp(
		loc, maxTimeout,
		(VirtualFileFunc) File_readVirtualInternal,
		output,
		false,
		alloc,
		e_rr
	);
}

Bool File_getInfoVirtualInternal(FileInfo *info, const CharString *loc, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();
	const VirtualSection *section = NULL;

	const ELockAcquire acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);
	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_getInfoVirtualInternal() couldn't lock virtualSectionsLock"));

	gotoIfError3(clean, File_resolveVirtual(loc, &subPath, &section, alloc, e_rr));

	//loc is root or a parent directory of a section, always a folder
	if(!section) {

		const CharString display = CharString_length(*loc) ? *loc : CharString_createRefCStrConst("//.");
		CharString path = CharString_createNull();
		gotoIfError3(clean, CharString_createCopy(display, alloc, &path, e_rr));
		*info = (FileInfo) {
			.path   = path,
			.type   = EFileType_Folder,
			.access = EFileAccess_Read
		};

	} else {
		if(!section->loadedAndId)
			retError(clean, Error_invalidOperation(0, "File_getInfoVirtualInternal() section not loaded"));

		const CAFile *caFile = &Platform_instance->archives.ptr[section->loadedAndId << 1 >> 1];
		CAHandle handle = CAFile_resolve(caFile, subPath);

		//Full path is just loc since it's already resolved and normalized

		CharString path = CharString_createNull();
		gotoIfError3(clean, CharString_createCopy(*loc, alloc, &path, e_rr));

		*info = (FileInfo) {
			.path      = path,
			.type      = CAHandle_isFile(handle) ? EFileType_File : EFileType_Folder,
			.access    = EFileAccess_Read,
			.timestamp = CAFile_fileTime(caFile, handle),
			.fileSize  = CAFile_fileSize(caFile, handle)
		};
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	if(!s_uccess)
		FileInfo_free(info, alloc);

	CharString_free(&subPath, alloc);
	return s_uccess;
}

Bool File_getInfoVirtual(const CharString *loc, FileInfo *info, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!info)
		retError(clean, Error_nullPointer(1, "File_getInfoVirtual()::info is required"));

	if(info->path.ptr)
		retError(clean, Error_invalidOperation(0, "File_getInfoVirtual()::info isn't empty, might indicate memleak"));

	gotoIfError3(clean, File_virtualOp(
		loc, 1 * MS,
		(VirtualFileFunc) File_getInfoVirtualInternal,
		info,
		false,
		alloc,
		e_rr
	));

clean:
	return s_uccess;
}

typedef struct ForeachFile {
	FileCallback callback;
	void *userData;
	Bool isRecursive;
	CharString currentPath;
} ForeachFile;

Bool File_foreachVirtualInternal(void *userData, const CharString *resolved, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	ForeachFile *foreach     = (ForeachFile*)userData;
	CharString resolvedLower = CharString_createNull();
	CharString resolvedSlash = CharString_createNull();
	CharString secLower      = CharString_createNull();
	CharString secSlash      = CharString_createNull();
	ListCharString visited   = (ListCharString) { 0 };
	ELockAcquire acq         = ELockAcquire_Invalid;
	Bool foundAny            = false;

	gotoIfError3(clean, CharString_createCopy(*resolved, alloc, &resolvedLower, e_rr));
	CharString_toLower(&resolvedLower);

	gotoIfError3(clean, CharString_createCopy(resolvedLower, alloc, &resolvedSlash, e_rr));
	gotoIfError3(clean, CharString_append(&resolvedSlash, '/', alloc, e_rr));

	//Slash count of the query path (without trailing slash)
	//Used to determine which depth sections/entries appear at
	U64 baseCount = CharString_countAllSensitive(&resolvedLower, '/', 2);

	acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);
	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_foreachVirtualInternal() couldn't lock virtualSectionsLock"));

	for(U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {
		const VirtualSection *section = Platform_instance->virtualSections.ptr + i;

		CharString_free(&secLower, alloc);
		CharString_free(&secSlash, alloc);

		gotoIfError3(clean, CharString_createCopy(section->path, alloc, &secLower, e_rr));
		CharString_toLower(&secLower);

		gotoIfError3(clean, CharString_createCopy(secLower, alloc, &secSlash, e_rr));
		gotoIfError3(clean, CharString_append(&secSlash, '/', alloc, e_rr));

		//Skip sections unrelated to our query:
		//- section starts with our path (section is at or below query)
		//- section equals query exactly
		//- our path starts with section/ (query is inside section)

		Bool sectionBelowQuery  = CharString_startsWithStringInsensitive(&secLower,   &resolvedSlash, 0);
		Bool sectionIsQuery     = CharString_equalsStringInsensitive(&secLower,        &resolvedLower);
		Bool queryInsideSection = CharString_startsWithStringInsensitive(&resolvedLower, &secSlash, 0);

		if(!sectionBelowQuery && !sectionIsQuery && !queryInsideSection)
			continue;

		foundAny = true;

		//When iterating root, emit immediate child directories as virtual folders
		//De-duplicate since multiple sections may share the same top-level dir
		if(!CharString_length(resolvedLower)) {

			CharString parent = CharString_createNull();

			if(!CharString_cutAfterFirstSensitive(&secLower, '/', &parent))
				retError(clean, Error_invalidState(0, "File_foreachVirtualInternal() cutAfterFirst failed"));

			Bool contains = false;
			for(U64 j = 0; j < visited.length; ++j)
				if(CharString_equalsStringSensitive(&parent, &visited.ptr[j])) {
					contains = true;
					break;
				}

			if(!contains) {

				gotoIfError3(clean, ListCharString_pushBack(&visited, parent, alloc, e_rr));

				FileInfo info = (FileInfo) {
					.path   = parent,
					.type   = EFileType_Folder,
					.access = EFileAccess_Read
				};

				gotoIfError3(clean, foreach->callback(&info, foreach->userData, alloc, e_rr));
			}
		}

		//Emit the section itself as a folder at the right depth
		U64 secSlashCount = CharString_countAllSensitive(&secLower, '/', 0);
		Bool emitSection = foreach->isRecursive ? secSlashCount >= baseCount : secSlashCount == baseCount;

		if(emitSection) {

			FileInfo info = (FileInfo) {
				.path   = secLower,
				.type   = EFileType_Folder,
				.access = EFileAccess_Read
			};

			gotoIfError3(clean, foreach->callback(&info, foreach->userData, alloc, e_rr));
		}

		//Enumerate section contents if loaded
		if(section->loadedAndId) {

			const CAFile *caFile = &Platform_instance->archives.ptr[section->loadedAndId << 1 >> 1];

			//Compute subpath within section (empty = section root)
			CharString child = CharString_createNull();
			if(queryInsideSection)
				CharString_cut(&resolvedLower, CharString_length(secSlash), 0, &child);

			//currentPath is used by File_virtualCallback to prepend section prefix
			foreach->currentPath = secSlash;

			CAHandle handle = CAFile_resolve(caFile, child);

			if(handle == CAHandle_Invalid)
				retError(clean, Error_notFound(0, 0, "File_foreachVirtualInternal() caFile child not found"));

			gotoIfError3(clean, CAFile_foreach(
				caFile,
				handle,
				foreach->callback,
				foreach->userData,
				foreach->isRecursive,
				alloc,
				e_rr
			));
		}
	}

	//Not an error if nothing found at root, root always exists
	if(!foundAny && CharString_length(resolvedLower))
		retError(clean, Error_notFound(0, 0, "File_foreachVirtualInternal() couldn't find virtual section"));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	ListCharString_free(&visited, alloc);
	CharString_free(&resolvedLower, alloc);
	CharString_free(&resolvedSlash, alloc);
	CharString_free(&secLower, alloc);
	CharString_free(&secSlash, alloc);
	return s_uccess;
}

Bool File_foreachVirtual(
	const CharString *loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	const Allocator *alloc,
	Error *e_rr
) {

	if(!callback) {
		if(e_rr) *e_rr = Error_nullPointer(1, "File_foreachVirtual()::callback is required");
		return false;
	}

	ForeachFile foreachFile = (ForeachFile) {
		.callback = callback,
		.userData = userData,
		.isRecursive = isRecursive
	};

	return File_virtualOp(
		loc, 1 * SECOND,
		(VirtualFileFunc) File_foreachVirtualInternal,
		&foreachFile,
		false,
		alloc,
		e_rr
	);
}

Bool File_queryFileObjectCountVirtual(
	const CharString *loc,
	EFileType type,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(3, "File_queryFileObjectCountVirtual()::res is required"));

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	gotoIfError3(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive, alloc, e_rr));
	*res = counter.counter;

clean:
	return s_uccess;
}

Bool File_queryFileObjectCountAllVirtual(
	const CharString *loc,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(2, "File_queryFileObjectCountAllVirtual()::res is required"));

	FileCounter counter = (FileCounter) { 0 };
	gotoIfError3(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive, alloc, e_rr));
	*res = counter.counter;

clean:
	return s_uccess;
}

impl Bool File_loadVirtualInternal1(
	FileLoadVirtual *userData,
	const CharString *loc,
	Bool allowLoad,
	const RefPtrType *memoryStreamType,
	const RefPtrType *encStreamType,
	const Allocator *alloc,
	Error *e_rr
);

Bool File_loadVirtualInternal(FileLoadVirtual *userData, const CharString *loc, const Allocator *alloc, Error *e_rr) {
	return File_loadVirtualInternal1(userData, loc, true, userData->memoryStreamType, userData->encStreamType, alloc, e_rr);
}

Bool File_unloadVirtualInternal(void *userData, const CharString *loc, const Allocator *alloc, Error *e_rr) {

	(void)userData;

	CharString isChild = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;
	Bool s_uccess = true;

	if(!loc)
		retError(clean, Error_nullPointer(0, "File_unloadVirtualInternal()::loc is required"));

	gotoIfError3(clean, CharString_createCopy(*loc, alloc, &isChild, e_rr));

	if(CharString_length(isChild))
		gotoIfError3(clean, CharString_append(&isChild, '/', alloc, e_rr));        //Don't append to root

	acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_unloadVirtualInternal() couldn't lock virtualSectionsLock"));

	for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {

		VirtualSection *section = Platform_instance->virtualSections.ptrNonConst + i;

		if(
			!CharString_equalsStringInsensitive(loc, &section->path) &&
			!CharString_startsWithStringInsensitive(&section->path, &isChild, 0)
		)
			continue;

		if(section->loadedAndId) {
			CAFile_free(&Platform_instance->archives.ptrNonConst[section->loadedAndId << 1 >> 1], alloc);
			ListCAFile_erase(&Platform_instance->archives, section->loadedAndId << 1 >> 1, NULL);
			section->loadedAndId = 0;
		}
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	CharString_free(&isChild, alloc);
	return s_uccess;
}

Bool File_isVirtualLoaded(const CharString *loc, const Allocator *alloc, Error *e_rr) {
	FileLoadVirtual virt = (FileLoadVirtual) { 0 };
	return File_virtualOp(loc, 1 * MS, (VirtualFileFunc) File_loadVirtualInternal, &virt, false, alloc, e_rr);
}

Bool File_loadVirtual(
	const CharString *loc,
	const RefPtrType *memoryStreamType,
	const RefPtrType *encStreamType, 
	const U32 encryptionKey[8],
	const Allocator *alloc,
	Error *e_rr
) {

	FileLoadVirtual virt = (FileLoadVirtual) {
		.doLoad = true,
		.encryptionKey = encryptionKey,
		.memoryStreamType = memoryStreamType,
		.encStreamType = encStreamType
	};

	return File_virtualOp(loc, 1 * MS, (VirtualFileFunc) File_loadVirtualInternal, &virt, false, alloc, e_rr);
}

Bool File_unloadVirtual(const CharString *loc, const Allocator *alloc, Error *e_rr) {
	return File_virtualOp(loc, 10 * MS, File_unloadVirtualInternal, NULL, false, alloc, e_rr);
}
