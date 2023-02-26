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
#include "types/list.h"
#include "types/string.h"
#include "types/file.h"

typedef enum EFileType EFileType;

typedef struct ArchiveEntry {

	String path;
	Buffer data;
	EFileType type;		//If true, data should be empty
	Ns timestamp;		//Shouldn't be set if isFolder. Will disappear

} ArchiveEntry;


typedef struct Archive {
	List entries;			//<ArchiveEntry>
} Archive;

Error Archive_create(Allocator alloc, Archive *archive);
Bool Archive_free(Archive *archive, Allocator alloc);

Bool Archive_hasFile(Archive archive, String path, Allocator alloc);
Bool Archive_hasFolder(Archive archive, String path, Allocator alloc);
Bool Archive_has(Archive archive, String path, Allocator alloc);

Error Archive_addDirectory(Archive *archive, String path, Allocator alloc);
Error Archive_addFile(Archive *archive, String path, Buffer data, Ns timestamp, Allocator alloc);

Error Archive_updateFileData(Archive *archive, String path, Buffer data, Allocator alloc);

Error Archive_getFileData(Archive archive, String path, Buffer *data, Allocator alloc);
Error Archive_getFileDataConst(Archive archive, String path, Buffer *data, Allocator alloc);

Error Archive_removeFile(Archive *archive, String path, Allocator alloc);
Error Archive_removeFolder(Archive *archive, String path, Allocator alloc);
Error Archive_remove(Archive *archive, String path, Allocator alloc);

Error Archive_rename(
	Archive *archive, 
	String loc, 
	String newFileName, 
	Allocator alloc
);

U64 Archive_getIndex(Archive archive, String path, Allocator alloc);		//Get index in archive

Error Archive_move(
	Archive *archive, 
	String loc, 
	String directoryName, 
	Allocator alloc
);

Error Archive_getInfo(Archive archive, String loc, FileInfo *info, Allocator alloc);

Error Archive_queryFileObjectCount(
	Archive archive, 
	String loc, 
	EFileType type, 
	Bool isRecursive, 
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFileObjectCountAll(
	Archive archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFileCount(
	Archive archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFolderCount(
	Archive archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_foreach(
	Archive archive,
	String loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Allocator alloc
);
