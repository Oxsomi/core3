/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/string.h"

typedef enum EFileType {
	EFileType_Folder,
	EFileType_File,
	EFileType_Invalid,
	EFileType_Any = EFileType_Invalid
} EFileType;

typedef enum EFileAccess {
	EFileAccess_None,			//Invalid, never returned
	EFileAccess_Read,
	EFileAccess_Write,
	EFileAccess_ReadWrite
} EFileAccess;

typedef struct FileInfo {

	EFileType type;
	EFileAccess access;
	Ns timestamp;		//In units that the file system supports. Normally that unit is seconds.
	String path;
	U64 fileSize;

} FileInfo;

typedef Error (*FileCallback)(FileInfo, void*);

typedef struct Allocator Allocator;
typedef struct String String;

Error File_resolve(
	String loc,
	Bool *isVirtual,
	U64 maxFilePathLimit,
	String absoluteDir,
	Allocator alloc,
	String *result
);

Bool File_isVirtual(String loc);

Bool FileInfo_free(FileInfo *info, Allocator alloc);
