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

//platforms/file.h

#pragma once
#include "types/container/string.h"
#include "types/container/file.h"
#include "types/container/stream.h"
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

Bool File_getInfo(const CharString *loc, FileInfo *info, const Allocator *alloc, Error *e_rr);
Bool File_foreach(
	const CharString *loc,
	Bool inAppDir, FileCallback callback,
	void *userData,
	Bool isRecursive,
	const Allocator *alloc,
	Error *e_rr
);

Bool File_remove(const CharString *loc, Ns maxTimeout, const Allocator *alloc, Error *e_rr);

Bool File_add(
	const CharString *loc,
	EFileType type,
	Bool createParentOnly,
	const Allocator *alloc,
	Error *e_rr
);

Bool File_rename(const CharString *loc, const CharString *newFileName, Ns maxTimeout, const Allocator *alloc, Error *e_rr);
Bool File_move(const CharString *loc, const CharString *directoryName, Ns maxTimeout, const Allocator *alloc, Error *e_rr);

//Includes files only
Bool File_queryFileObjectCount(
	const CharString *loc,
	EFileType type,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
);

//Includes folders + files
Bool File_queryFileObjectCountAll(const CharString *loc, Bool isRecursive, U64 *res, const Allocator *alloc, Error *e_rr);

static inline Bool File_queryFileCount(
	const CharString *loc,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
) {
	return File_queryFileObjectCount(loc, EFileType_File, isRecursive, res, alloc, e_rr);
}

static inline Bool File_queryFolderCount(
	const CharString *loc,
	Bool isRecursive,
	U64 *res,
	const Allocator *alloc,
	Error *e_rr
) {
	return File_queryFileObjectCount(loc, EFileType_Folder, isRecursive, res, alloc, e_rr);
}

static inline Bool File_has(const CharString *loc, const Allocator *alloc) {
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = File_getInfo(loc, &info, alloc, NULL);
	FileInfo_free(&info, alloc);
	return s_uccess;
}

static inline Bool File_hasType(const CharString *loc, EFileType type, const Allocator *alloc) {
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = File_getInfo(loc, &info, alloc, NULL);
	const Bool sameType = info.type == type;
	FileInfo_free(&info, alloc);
	return s_uccess && sameType;
}

static inline Bool File_hasFile(const CharString *loc, const Allocator *alloc) {
	return File_hasType(loc, EFileType_File, alloc);
}

static inline Bool File_hasFolder(const CharString *loc, const Allocator *alloc) {
	return File_hasType(loc, EFileType_Folder, alloc);
}

typedef enum EFileOpenType {
	EFileOpenType_Read,				//Read only
	EFileOpenType_Write,			//E.g. ios::ate or wb
	EFileOpenType_ReadWrite
} EFileOpenType;

static inline EFileOpenType EFileOpenType_create(Bool read, Bool write) {
	return read && write ? EFileOpenType_ReadWrite : (write ? EFileOpenType_Write : EFileOpenType_Read);
}

static inline Bool EFileOpenType_isRead(EFileOpenType type) {
	return type == EFileOpenType_Read || type == EFileOpenType_ReadWrite;
}

static inline Bool EFileOpenType_isWrite(EFileOpenType type) {
	return type == EFileOpenType_Write || type == EFileOpenType_ReadWrite;
}

typedef U8 FileOpenType;

typedef struct FileHandle {
	void *ext;						//HANDLE or int (fd)
	U64 fileSizeType;				//(EFileOpenType << 62) | fileSize
} FileHandle;

static inline U64 FileHandle_makeFileSizeType(U64 fileSize, EFileOpenType type) {

	if((fileSize >> 62) || (U64)type > (U64)EFileOpenType_ReadWrite) 
		return U64_MAX;

	return fileSize | ((U64)type << 62);
}

static inline U64 FileHandle_fileSize(const FileHandle *handle) { return handle ? handle->fileSizeType << 2 >> 2 : 0; }

static inline EFileOpenType FileHandle_fileType(const FileHandle *handle) {
	return handle ? handle->fileSizeType >> 62 : EFileOpenType_Read;
}

static inline Bool EFileHandle_isRead(const FileHandle *handle) {
	EFileOpenType openType = FileHandle_fileType(handle);
	return handle && EFileOpenType_isRead(openType);
}

static inline Bool EFileHandle_isWrite(const FileHandle *handle) {
	EFileOpenType openType = FileHandle_fileType(handle);
	return handle && EFileOpenType_isWrite(openType);
}

typedef struct RefPtr RefPtr;
typedef struct RefPtrType RefPtrType;
typedef RefPtr FileHandleRef;

RefPtrType FileHandle_makeType(const Allocator *alloc);

Bool File_open(
	const CharString *loc,
	Ns timeout,
	EFileOpenType type,
	Bool create,
	const RefPtrType *ptrType,
	FileHandleRef **handle,
	Error *e_rr
);

Bool FileHandleRef_write(FileHandleRef *handle, U64 offset, U64 length, const Buffer *buf, Error *e_rr);
Bool File_write(
	const Buffer *buf,
	const CharString *loc,
	U64 off,
	U64 len,
	Ns maxTimeout,
	Bool createParent,
	const RefPtrType *ptrType,
	Error *e_rr
);

Bool FileHandleRef_read(const FileHandleRef *handle, U64 off, U64 len, Buffer *output, Error *e_rr);
Bool File_read(
	const CharString *loc,
	Ns maxTimeout,
	U64 off,
	U64 len,
	const RefPtrType *alloc,
	Buffer *output,
	Error *e_rr
);

typedef struct FileLoadVirtual {
	const U32 *encryptionKey;
	Bool doLoad; U8 pad[7];
	const RefPtrType *memoryStreamType;
	const RefPtrType *encStreamType;
} FileLoadVirtual;

//Load a virtual section
Bool File_loadVirtual(
	const CharString *loc,
	const RefPtrType *memoryStreamType,
	const RefPtrType *encStreamType, 
	const U32 encryptionKey[8],
	const Allocator *alloc,
	Error *e_rr
);

Bool File_isVirtualLoaded(const CharString *loc, const Allocator *alloc, Error *e_rr);	//Check if a virtual section is loaded
Bool File_unloadVirtual(const CharString *loc, const Allocator *alloc, Error *e_rr);	//Unload a virtual section

//FileStream for handling bigger files and more efficiently jumping around.

typedef struct FileStream {
	OxStream parent;
	FileHandleRef *handle;
} FileStream;

typedef RefPtr StreamRef;
typedef RefPtr FileStreamRef;

RefPtrType FileStream_makeType(const Allocator *alloc);

Bool File_openStream(
	const CharString *loc,
	Ns timeout,
	EFileOpenType type,
	Bool create,
	const RefPtrType *fileHandleType,
	const RefPtrType *streamType,
	StreamRef **output,
	Error *e_rr
);

//Takes over FileHandle
Bool FileHandle_openStream(
	FileHandleRef **handle,
	const RefPtrType *streamType,
	StreamRef **streamRef,
	Error *e_rr
);

//TODO: make it more like a DirectStorage-like api

#ifdef __cplusplus
	}
#endif
