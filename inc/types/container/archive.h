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
#include "types/container/list.h"
#include "types/container/string.h"
#include "types/container/file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EFileType EFileType;

typedef struct ArchiveEntry {

	CharString path;
	Buffer data;
	EFileType type;												//If true, data should be empty
	Ns timestamp;												//Shouldn't be set if isFolder. Will disappear

} ArchiveEntry;

TList(ArchiveEntry);

typedef struct Archive {
	ListArchiveEntry entries;
} Archive;

typedef enum EArchiveCombineMode {
	EArchiveCombineMode_RequireSame,							//Files are only allowed to merge if same contents
	EArchiveCombineMode_Rename,									//Try to rename the file on conflict
	EArchiveCombineMode_AcceptA,								//First archive is leading on conflict
	EArchiveCombineMode_AcceptB,								//Second archive is leading on conflict
	EArchiveCombineMode_Count
} EArchiveCombineMode;

typedef enum EArchiveCombineFlags {
	EArchiveCombineFlags_None					= 0,
	EArchiveCombineFlags_ResolveLatestTimestamp = 1 << 0,		//Resolve timestamp with latest, as long as data matches
	EArchiveCombineFlags_ResolveAcceptLatest	= 1 << 1		//Override file with latest file contents, otherwise conflict
} EArchiveCombineFlags;

typedef struct ArchiveCombineSettings {
	EArchiveCombineMode mode;
	EArchiveCombineFlags flags;
} ArchiveCombineSettings;

Bool Archive_create(Allocator alloc, Archive *archive, Error *e_rr);
Bool Archive_createCopy(Archive a, Allocator alloc, Archive *archive, Error *e_rr);
Bool Archive_free(Archive *archive, Allocator alloc);

Bool Archive_combine(Archive a, Archive b, ArchiveCombineSettings settings, Allocator alloc, Archive *combined, Error *e_rr);

Bool Archive_hasFile(Archive archive, CharString path, Allocator alloc);
Bool Archive_hasFolder(Archive archive, CharString path, Allocator alloc);
Bool Archive_has(Archive archive, CharString path, Allocator alloc);

Bool Archive_addDirectory(Archive *archive, CharString path, Allocator alloc, Error *e_rr);
Bool Archive_addFile(Archive *archive, CharString path, Buffer *data, Ns timestamp, Allocator alloc, Error *e_rr);

Bool Archive_updateFileData(Archive *archive, CharString path, Buffer data, Allocator alloc, Error *e_rr);

Bool Archive_getFileData(Archive archive, CharString path, Buffer *data, Allocator alloc, Error *e_rr);
Bool Archive_getFileDataConst(Archive archive, CharString path, Buffer *data, Allocator alloc, Error *e_rr);

Bool Archive_removeFile(Archive *archive, CharString path, Allocator alloc, Error *e_rr);
Bool Archive_removeFolder(Archive *archive, CharString path, Allocator alloc, Error *e_rr);
Bool Archive_remove(Archive *archive, CharString path, Allocator alloc, Error *e_rr);

Bool Archive_rename(Archive *archive, CharString loc, CharString newFileName, Allocator alloc, Error *e_rr);
Bool Archive_move(Archive *archive, CharString loc, CharString directoryName, Allocator alloc, Error *e_rr);

U64 Archive_getIndex(Archive archive, CharString path, Allocator alloc);		//Get index in archive
Bool Archive_getInfo(Archive archive, CharString path, FileInfo *info, Allocator alloc, Error *e_rr);

Bool Archive_queryFileEntryCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc,
	Error *e_rr
);

Bool Archive_queryFileCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc,
	Error *e_rr
);

Bool Archive_queryFolderCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc,
	Error *e_rr
);

Bool Archive_foreach(
	Archive archive,
	CharString loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Allocator alloc,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
