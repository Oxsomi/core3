/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#ifdef __cplusplus
	extern "C" {
#endif

Bool File_getInfo(CharString loc, FileInfo *info, Allocator alloc, Error *e_rr);
Bool File_getInfox(CharString loc, FileInfo *info, Error *e_rr);
Bool File_resolvex(CharString loc, Bool *isVirtual, U64 maxFilePathLimit, CharString *result, Error *e_rr);
Bool File_makeRelativex(CharString base, CharString subFile, U64 maxFilePathLimit, CharString *result, Error *e_rr);

Bool FileInfo_freex(FileInfo *fileInfo);

Bool File_foreach(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr);

Bool File_remove(CharString loc, Ns maxTimeout, Error *e_rr);
Bool File_add(CharString loc, EFileType type, Ns maxTimeout, Bool createParentOnly, Error *e_rr);

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

Bool File_open(CharString loc, Ns timeout, EFileOpenType type, Bool create, FileHandle *handle, Error *e_rr);
void FileHandle_close(FileHandle *handle);

Bool FileHandle_write(const FileHandle *handle, Buffer buf, U64 offset, U64 length, Error *e_rr);
Bool File_write(Buffer buf, CharString loc, U64 off, U64 len, Ns maxTimeout, Bool createParent, Error *e_rr);

Bool FileHandle_read(const FileHandle *handle, U64 off, U64 len, Buffer output, Error *e_rr);
Bool File_read(CharString loc, Ns maxTimeout, U64 off, U64 len, Buffer *output, Error *e_rr);

typedef struct FileLoadVirtual {
	Bool doLoad;
	const U32 *encryptionKey;
} FileLoadVirtual;

Bool File_loadVirtual(CharString loc, const U32 encryptionKey[8], Error *e_rr);		//Load a virtual section
Bool File_isVirtualLoaded(CharString loc, Error *e_rr);								//Check if a virtual section is loaded
Bool File_unloadVirtual(CharString loc, Error *e_rr);								//Unload a virtual section

//FileStream for handling bigger files and more efficiently jumping around.

typedef struct Stream Stream;
typedef Bool (*StreamFunc)(Stream *stream, U64 offset, U64 length, Buffer buf, Allocator alloc, Error *e_rr);

typedef struct Stream {

	union {
		StreamFunc read;		//Only if isReadonly
		StreamFunc write;
	};

	FileHandle handle;			//Only valid if it's a file stream, however fileSize has to be set always

	Buffer cacheData;			//Temporary cache

	U64 lastLocation;
	U64 lastWriteLocation;		//Because the stream can be bigger than the actual file

	Bool isReadonly;
	U8 padding[7];

} Stream;

Bool File_openStream(CharString loc, Ns timeout, EFileOpenType type, Bool create, U64 cache, Stream *output, Error *e_rr);
Bool FileHandle_openStream(FileHandle *handle, U64 cache, Stream *stream, Error *e_rr);		//Takes over FileHandle

Bool Stream_write(Stream *stream, Buffer buf, U64 srcOff, U64 dstOff, U64 length, Bool bypassCache, Error *e_rr);

//buf.length == 0 && length indicates; load to internal stream only
Bool Stream_read(Stream *stream, Buffer buf, U64 srcOff, U64 dstOff, U64 length, Bool bypassCache, Error *e_rr);

void Stream_close(Stream *stream);

//TODO: make it more like a DirectStorage-like api

#ifdef __cplusplus
	}
#endif
