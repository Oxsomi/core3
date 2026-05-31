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

//types/container/file.h

#pragma once
#include "types/base/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;

typedef enum EFileType {
	EFileType_Folder,
	EFileType_File,
	EFileType_Invalid,
	EFileType_Any = EFileType_Invalid
} EFileType;

typedef enum EFileAccess {
	EFileAccess_None,            //Invalid, never returned
	EFileAccess_Read,
	EFileAccess_Write,
	EFileAccess_ReadWrite
} EFileAccess;

typedef struct FileInfo {
	EFileType type;
	EFileAccess access;
	Ns timestamp;        //In units that the file system supports. Normally that unit is seconds.
	CharString path;
	U64 fileSize;
} FileInfo;

typedef Bool (*FileCallback)(const FileInfo*, void*, const Allocator*, Error*);

#define MAX_OXC_PATH 1023    //Safest path across all platforms (Apple, Windows, Linux-based)

Bool File_resolve(
	const CharString *loc,
	Bool *isVirtual,
	U64 maxFilePathLimit,
	const CharString *absoluteDir,
	const Allocator *alloc,
	CharString *result,
	Error *e_rr
);

Bool File_makeRelative(
	const CharString absoluteDir,        //Can't escape absoluteDir with baseFolder or subFile. Must end with /
	const CharString base,                //File in which the parent is located (e.g. myFolder/test.txt)
	const CharString subFile,            //File to made relative to the parent of base (e.g. myOtherFolder/test.txt)
	U64 maxFilePathLimit,
	const Allocator *alloc,
	CharString *result,
	Error *e_rr
);

static inline Bool File_isVirtual(CharString loc) {
	return CharString_getAt(loc, 0) == '/' && CharString_getAt(loc, 1) == '/';
}

void FileInfo_free(FileInfo *info, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
