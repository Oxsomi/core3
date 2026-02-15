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

#pragma once
#include "types/base/types.h"
#include "types/container/string.h"
#include "types/container/file.h"
#include "types/container/stream.h"

#ifdef __cplusplus
	extern "C" {
#endif

Bool File_getInfo(CharString loc, FileInfo *info, Allocator alloc, Error *e_rr);
Bool File_getInfox(CharString loc, FileInfo *info, Error *e_rr);
Bool File_resolvex(CharString loc, Bool *isVirtual, Bool isAppDir, U64 maxFilePathLimit, CharString *result, Error *e_rr);
Bool File_makeRelativex(CharString base, CharString subFile, U64 maxFilePathLimit, CharString *result, Error *e_rr);

Bool FileInfo_freex(FileInfo *fileInfo);

Bool File_foreach(CharString loc, Bool inAppDir, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr);

Bool File_remove(CharString loc, Ns maxTimeout, Error *e_rr);
Bool File_add(CharString loc, EFileType type, Ns maxTimeout, Bool createParentOnly, Allocator alloc, Error *e_rr);
Bool File_addx(CharString loc, EFileType type, Ns maxTimeout, Bool createParentOnly, Error *e_rr);

Bool File_rename(CharString loc, CharString newFileName, Ns maxTimeout, Error *e_rr);
Bool File_move(CharString loc, CharString directoryName, Ns maxTimeout, Error *e_rr);

//Includes files only
Bool File_queryFileObjectCount(CharString loc, EFileType type, Bool isRecursive, U64 *res, Error *e_rr);

//Includes folders + files
Bool File_queryFileObjectCountAll(CharString loc, Bool isRecursive, U64 *res, Error *e_rr);

Bool File_queryFileCount(CharString loc, Bool isRecursive, U64 *res, Error *e_rr);
Bool File_queryFolderCount(CharString loc, Bool isRecursive, U64 *res, Error *e_rr);

Bool File_has(CharString loc);
Bool File_hasType(CharString loc, EFileType type);

Bool File_hasFile(CharString loc);
Bool File_hasFolder(CharString loc);

typedef enum EFileOpenType {
	EFileOpenType_Read,				//Read only
	EFileOpenType_Write				//E.g. ios::ate or wb
} EFileOpenType;

typedef U8 FileOpenType;

//TODO: Make this a RefPtr
//TODO: Make it use HANDLE* or int fd to allow reading without fseek
typedef struct FileHandle {

	void *ext;

	CharString filePath;

	FileOpenType type;
	Bool ownsHandle;				//For if the handle is a copy and shouldn't be affected by File_close
	U8 padding[6];

	U64 fileSize;					//Only if readonly, indicates file size

} FileHandle;

//Manually ensure that all child FileHandles are disposed before the parent FileHandle is closed.
Bool FileHandle_createRef(const FileHandle *input, FileHandle *output, Error *e_rr);

Bool File_open(CharString loc, Ns timeout, EFileOpenType type, Bool create, Allocator alloc, FileHandle *handle, Error *e_rr);
Bool File_openx(CharString loc, Ns timeout, EFileOpenType type, Bool create, FileHandle *handle, Error *e_rr);

void FileHandle_close(FileHandle *handle, const Allocator *alloc);
void FileHandle_closex(FileHandle *handle);

Bool FileHandle_write(const FileHandle *handle, U64 offset, U64 length, Buffer buf, Error *e_rr);
Bool File_write(Buffer buf, CharString loc, U64 off, U64 len, Ns maxTimeout, Bool createParent, Allocator alloc, Error *e_rr);
Bool File_writex(Buffer buf, CharString loc, U64 off, U64 len, Ns maxTimeout, Bool createParent, Error *e_rr);

Bool FileHandle_read(const FileHandle *handle, U64 off, U64 len, Buffer output, Error *e_rr);
Bool File_read(CharString loc, Ns maxTimeout, U64 off, U64 len, Allocator alloc, Buffer *output, Error *e_rr);
Bool File_readx(CharString loc, Ns maxTimeout, U64 off, U64 len, Buffer *output, Error *e_rr);

typedef struct FileLoadVirtual {
	Bool doLoad;
	const U32 *encryptionKey;
} FileLoadVirtual;

Bool File_loadVirtual(CharString loc, const U32 encryptionKey[8], Error *e_rr);		//Load a virtual section
Bool File_isVirtualLoaded(CharString loc, Error *e_rr);								//Check if a virtual section is loaded
Bool File_unloadVirtual(CharString loc, Error *e_rr);								//Unload a virtual section

//FileStream for handling bigger files and more efficiently jumping around.

typedef struct FileStream {
	Stream parent;
	FileHandle handle;
} FileStream;

Bool File_openStream(
	const CharString *loc,
	Ns timeout,
	EFileOpenType type,
	Bool create,
	U64 cache,
	const Allocator *alloc,
	FileStream *output,
	Error *e_rr
);

//Takes over FileHandle
Bool FileHandle_openStream(FileHandle *handle, U64 cache, const Allocator *alloc, FileStream *stream, Error *e_rr);

//Simplified functions

Bool File_openStreamx(
	const CharString *loc,
	Ns timeout,
	EFileOpenType type,
	Bool create,
	U64 cache,
	FileStream *output,
	Error *e_rr
);

//Takes over FileHandle
Bool FileHandle_openStreamx(FileHandle *handle, U64 cache, FileStream *stream, Error *e_rr);

//TODO: make it more like a DirectStorage-like api

#ifdef __cplusplus
	}
#endif
