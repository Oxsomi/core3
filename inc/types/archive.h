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

	CharString path;
	Buffer data;
	EFileType type;		//If true, data should be empty
	Ns timestamp;		//Shouldn't be set if isFolder. Will disappear

} ArchiveEntry;


typedef struct Archive {
	List entries;			//<ArchiveEntry>
} Archive;

Error Archive_create(Allocator alloc, Archive *archive);
Bool Archive_free(Archive *archive, Allocator alloc);

Bool Archive_hasFile(Archive archive, CharString path, Allocator alloc);
Bool Archive_hasFolder(Archive archive, CharString path, Allocator alloc);
Bool Archive_has(Archive archive, CharString path, Allocator alloc);

Error Archive_addDirectory(Archive *archive, CharString path, Allocator alloc);
Error Archive_addFile(Archive *archive, CharString path, Buffer data, Ns timestamp, Allocator alloc);

Error Archive_updateFileData(Archive *archive, CharString path, Buffer data, Allocator alloc);

Error Archive_getFileData(Archive archive, CharString path, Buffer *data, Allocator alloc);
Error Archive_getFileDataConst(Archive archive, CharString path, Buffer *data, Allocator alloc);

Error Archive_removeFile(Archive *archive, CharString path, Allocator alloc);
Error Archive_removeFolder(Archive *archive, CharString path, Allocator alloc);
Error Archive_remove(Archive *archive, CharString path, Allocator alloc);

Error Archive_rename(
	Archive *archive, 
	CharString loc, 
	CharString newFileName, 
	Allocator alloc
);

U64 Archive_getIndex(Archive archive, CharString path, Allocator alloc);		//Get index in archive

Error Archive_move(
	Archive *archive, 
	CharString loc, 
	CharString directoryName, 
	Allocator alloc
);

Error Archive_getInfo(Archive archive, CharString loc, FileInfo *info, Allocator alloc);

Error Archive_queryFileObjectCount(
	Archive archive, 
	CharString loc, 
	EFileType type, 
	Bool isRecursive, 
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFileObjectCountAll(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFileCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFolderCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_foreach(
	Archive archive,
	CharString loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Allocator alloc
);
