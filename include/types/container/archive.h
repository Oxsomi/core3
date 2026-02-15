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
#include "types/container/list.h"
#include "types/container/file.h"
#include "types/base/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct ArchiveEntry {
	CharString path;											//Fully qualified name
	Buffer data;
	EFileType type;												//If EFileType_Folder, data should be empty
	U32 padding;
	Ns timestamp;												//Shouldn't be set if EFileType_Folder
} ArchiveEntry;

TList(ArchiveEntry);

//An archive is case insensitive to allow ease of write from an archive to disk.
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

typedef U8 ArchiveCombineMode;		//EArchiveCombineMode

typedef enum EArchiveCombineFlags {
	EArchiveCombineFlags_None					= 0,
	EArchiveCombineFlags_ResolveLatestTimestamp = 1 << 0,		//Resolve timestamp with latest, as long as data matches
	EArchiveCombineFlags_ResolveAcceptLatest	= 1 << 1		//Override file with latest file contents, otherwise conflict
} EArchiveCombineFlags;

typedef U8 ArchiveCombineFlags;		//EArchiveCombineFlags

typedef struct ArchiveCombineSettings {
	ArchiveCombineMode mode;
	ArchiveCombineFlags flags;
} ArchiveCombineSettings;

Bool Archive_create(const Allocator *alloc, Archive *archive, Error *e_rr);
Bool Archive_createCopy(const Archive *a, const Allocator *alloc, Archive *archive, Error *e_rr);
void Archive_free(Archive *archive, const Allocator *alloc);

typedef struct ArchiveOptionsConst {		//Needs to match ArchiveOptions except Archive*
	const Archive *archive;
	const CharString *path;
	const Allocator *alloc;
} ArchiveOptionsConst;

typedef struct ArchiveOptions {
	Archive *archive;
	const CharString *path;
	const Allocator *alloc;
} ArchiveOptions;

Bool Archive_hasFile(const ArchiveOptionsConst *archive);
Bool Archive_hasFolder(const ArchiveOptionsConst *archive);
Bool Archive_has(const ArchiveOptionsConst *archive);			//Has file or folder

Bool Archive_combine(
	const Archive *a,
	const Archive *b,
	ArchiveCombineSettings settings,
	const Allocator *alloc,
	Archive *combined,
	Error *e_rr
);

Bool Archive_addDirectory(const ArchiveOptions *archive, Error *e_rr);
Bool Archive_addFile(const ArchiveOptions *archive, Buffer *data, Ns timestamp, Error *e_rr);

//Moves ownership from Buffer to archive and preserves constness
Bool Archive_updateFileData(const ArchiveOptions *archive, const Buffer *data, Error *e_rr);

Bool Archive_getFileDataWithConst(const ArchiveOptions *archive, Buffer *data, Bool isConst, Error *e_rr);

static inline Bool Archive_getFileData(const ArchiveOptions *archive, Buffer *data, Error *e_rr) {
	return Archive_getFileDataWithConst(archive, data, false, e_rr);
}

static inline Bool Archive_getFileDataConst(const ArchiveOptions *archive, Buffer *data, Error *e_rr) {
	return Archive_getFileDataWithConst(archive, data, true, e_rr);
}

Bool Archive_removeFile(const ArchiveOptions *archive, Error *e_rr);
Bool Archive_removeFolder(const ArchiveOptions *archive, Error *e_rr);		//Recursively delete folder
Bool Archive_remove(const ArchiveOptions *archive, Error *e_rr);

Bool Archive_rename(const ArchiveOptions *archive, const CharString *newFileName, Error *e_rr);		//Non recursively rename
Bool Archive_move(const ArchiveOptions *archive, const CharString *directoryName, Error *e_rr);		//Non recursively move

U64 Archive_getIndex(const ArchiveOptions *archive);		//Get current index in archive (U64_MAX if missing)
Bool Archive_getInfo(const ArchiveOptions *archive, FileInfo *info, Error *e_rr);	//FileInfo returned needs freeing

typedef struct ArchiveQuery {
	const Archive *archive;
	const CharString *loc;
	Bool isRecursive;
	const Allocator *alloc;
} ArchiveQuery;

Bool Archive_foreach(const ArchiveQuery *query, FileCallback callback, void *userData, EFileType type, Error *e_rr);

static inline Bool Archive_countFile(const FileInfo *info, U64 *res, Error *e_rr) {
	(void)info; (void) e_rr;
	++*res;
	return true;
}

static inline Bool Archive_queryFileObjectCount(const ArchiveQuery *query, EFileType type, U64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if (!res)
		retError(clean, Error_nullPointer(2, "Archive_queryFileObjectCount()::res is required"));

	gotoIfError3(clean, Archive_foreach(query, (FileCallback) Archive_countFile, res, type, e_rr));

clean:
	return s_uccess;
}

static inline Bool Archive_queryFileEntryCount(const ArchiveQuery *query, U64 *res, Error *e_rr) {
	return Archive_queryFileObjectCount(query, EFileType_Any, res, e_rr);
}

static inline Bool Archive_queryFileCount(const ArchiveQuery *query, U64 *res, Error *e_rr) {
	return Archive_queryFileObjectCount(query, EFileType_File, res, e_rr);
}

static inline Bool Archive_queryFolderCount(const ArchiveQuery *query, U64 *res, Error *e_rr) {
	return Archive_queryFileObjectCount(query, EFileType_Folder, res, e_rr);
}

#ifdef __cplusplus
	}
#endif
