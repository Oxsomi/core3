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
#include "types/container/file.h"
#include "formats/oiXX/oiXX.h"
#include "formats/oiDL/dl_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ECASettingsFlags {

	ECASettingsFlags_None				= 0,
	ECASettingsFlags_IncludeDate		= 1 << 0,			//--date
	ECASettingsFlags_IncludeFullDate	= 1 << 1,			//--full-date (automatically sets --date)

	ECASettingsFlags_Invalid			= 0xFFFFFFFF << 2,
	ECASettingsFlags_DateFlags			= ECASettingsFlags_IncludeDate | ECASettingsFlags_IncludeFullDate

} ECASettingsFlags;

typedef struct CASettings {

	ECASettingsFlags flags;
	U32 padding;

	//Compared as U64[5]

	EXXCompressionType compressionType;
	EXXEncryptionType encryptionType;
	U32 encryptionKey[8];

} CASettings;

typedef struct CAFolderInfo {
	U16 parent;
	U16 dirOffset;
	U16 dirCount;
	U16 fileCount;
	U64 fileOffset;
} CAFolderInfo;

TList(CAFolderInfo);

typedef struct CAFileInfo {
	U64 parentTimestamp;	//16 : 48 (timestamp in ms)
} CAFileInfo;

TList(CAFileInfo);

static inline CAFileInfo CAFileInfo_create(U16 parent, Ns timestamp) {

	Ns timestampTrunc = timestamp / MS;

	if (timestampTrunc >> 48) {
		CAFileInfo v = { 0 };
		return v;
	}

	CAFileInfo v = { ((U64)parent << 48) | timestampTrunc };
	return v;
}

static inline U16 CAFileInfo_getParent(CAFileInfo info) {
	return (U16)(info.parentTimestamp >> 48);
}

static inline Ns CAFileInfo_getTimestamp(CAFileInfo info) {
	return (info.parentTimestamp << 16 >> 16) * MS;
}

//File limits

static const U32 CAFile_maxFilePathSize = 192;
static const U32 CAFile_maxFileNameSize = 96;
static const U32 CAFile_maxRecursionSize = 128;		//Must match chainSize (walking file parents)

//Check docs/oiCA.md for the file spec

typedef struct CAFile {
	DLFile names;
	DLFile content;
	ListCAFolderInfo folders;	//0th is reserved as root folder
	ListCAFileInfo files;
	CASettings settings;		//Must remain 8-byte aligned
} CAFile;

//Create and delete

Bool CAFile_create(
	const CASettings *settings,
	U64 reservedFiles,
	U64 reservedFolders,
	const Allocator *alloc,
	CAFile *caFile,
	Error *e_rr
);

Bool CAFile_createCopy(const CAFile *caFile, const Allocator *alloc, CAFile *result, Error *e_rr);

void CAFile_free(CAFile *caFile, const Allocator *alloc);

//Getters

//>> 63: isFolder, the rest is the handle (file or folder).
// (U64)-1 = invalid,
// 0 = root (if folder)
//NOTE: CAHandle can be invalidated when files are added/removed/moved, so make sure to re-calculate when applicable.
typedef U64 CAHandle;

static const U64 CAHandle_Invalid = (U64)-1;
static const CAHandle CAHandle_Root = (U64)1 << 63;

static inline U64 CAHandle_getId(CAHandle handle) { return handle << 1 >> 1; }
static inline Bool CAHandle_isFolder(CAHandle handle) { return handle >> 63; }
static inline Bool CAHandle_isFile(CAHandle handle) { return !CAHandle_isFolder(handle); }

static inline CAHandle CAHandle_makeFolder(U64 id) { return ((U64)1 << 63) | id; }	//Unsafe, only use if you know it's a dir
static inline CAHandle CAHandle_makeFile(U64 id) { return id; }						//Unsafe, only use if you know it's a file

static inline Bool CAHandle_isRoot(CAHandle handle) {
	return CAHandle_isFolder(handle) && !CAHandle_getId(handle);
}

U64 CAFile_fileCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive);
U64 CAFile_dirCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive);

static inline U64 CAFile_fileObjectCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive) {
	return CAFile_fileCount(caFile, fileHandle, recursive) + CAFile_dirCount(caFile, fileHandle, recursive);
}

U64 CAFile_fileSize(const CAFile *caFile, CAHandle fileHandle);
U64 CAFile_fileParent(const CAFile *caFile, CAHandle fileHandle);
Ns CAFile_fileTime(const CAFile *caFile, CAHandle fileHandle);

CAHandle CAFile_resolveSubFile(const CAFile *caFile, CAHandle parentDir, CharString fileName);
CAHandle CAFile_resolveSubFolder(const CAFile *caFile, CAHandle parentDir, CharString fileName);

static inline CAHandle CAFile_resolveSubObject(const CAFile *caFile, CAHandle parentDir, CharString fileName) {

	CAHandle h = CAFile_resolveSubFolder(caFile, parentDir, fileName);

	if (h != CAHandle_Invalid)
		return h;

	return CAFile_resolveSubFile(caFile, parentDir, fileName);
}

CAHandle CAFile_resolveFile(const CAFile *caFile, CharString fullFileName);
CAHandle CAFile_resolveFolder(const CAFile *caFile, CharString fullFileName);

static inline CAHandle CAFile_resolve(const CAFile *caFile, CharString fullFileName) {

	CAHandle h = CAFile_resolveFolder(caFile, fullFileName);

	if (h != CAHandle_Invalid)
		return h;

	return CAFile_resolveFile(caFile, fullFileName);
}

static inline Bool CAFile_hasSubFile(const CAFile *caFile, CAHandle parentDir, CharString fileName) {
	return CAFile_resolveSubFile(caFile, parentDir, fileName) != CAHandle_Invalid;
}

static inline Bool CAFile_hasSubFolder(const CAFile *caFile, CAHandle parentDir, CharString fileName) {
	return CAFile_resolveSubFolder(caFile, parentDir, fileName) != CAHandle_Invalid;
}

static inline Bool CAFile_hasSubObject(const CAFile *caFile, CAHandle parentDir, CharString fileName) {
	return CAFile_resolveSubObject(caFile, parentDir, fileName) != CAHandle_Invalid;
}

CAHandle CAFile_dirAt(const CAFile *caFile, CAHandle fileHandle, U64 id);
CAHandle CAFile_fileAt(const CAFile *caFile, CAHandle fileHandle, U64 id);

static inline CAHandle CAFile_fileObjectAt(const CAFile *caFile, CAHandle fileHandle, U64 id) {

	U64 dirCount = CAFile_dirCount(caFile, fileHandle, false);

	if (id < dirCount)
		return CAFile_dirAt(caFile, fileHandle, id);

	return CAFile_fileAt(caFile, fileHandle, id - dirCount);
}

//Get unqualified name of file.
//Returns empty only if root or if invalid handle.
CharString CAFile_getName(const CAFile *caFile, CAHandle handle);

//Get unqualified name of file.
//Returns empty only if root.
Bool CAFile_getFullName(const CAFile *caFile, CAHandle handle, const Allocator *alloc, CharString *result, Error *e_rr);

//Returns ref to existing data.
// isValid is false for invalid handles, folders or streams.
Buffer CAFile_getData(CAFile *caFile, CAHandle fileHandle, Bool *isValid);

//Returns ref to existing data.
// isValid is false for invalid handles, folders or streams.
Buffer CAFile_getDataConst(const CAFile *caFile, CAHandle fileHandle, Bool *isValid);

//Returns ref to existing data stream (increments stream ref).
// *streamOff is U64_MAX for invalid handles, folders or fully loaded data, otherwise is offset in the stream.
StreamRef *CAFile_getDataStream(const CAFile *caFile, CAHandle fileHandle, U64 *streamOff);

//This will compare the two files at a and b.
// Both files have to be buffers or streams that are seekable, otherwise it'll error.
// Keep in mind that this is a full compare, which could take very long with big files.
// As such, this should only be used in tools that are expected to take a long time.
Bool CAFile_dataEqual(
	const CAFile *a, CAHandle aFile,
	const CAFile *b, CAHandle bFile,
	const Allocator *alloc,
	ECompareResult *equal,
	Error *e_rr
);

Bool CAFile_isLoaded(const CAFile *caFile, CAHandle fileHandle);			//Returns false for streams

//Setters

Bool CAFile_setTime(CAFile *caFile, CAHandle fileHandle, Ns time, Error *e_rr);

//Set data of a file, only valid if it's a file.
// Moves 'buf' if not a ref, otherwise copies.
Bool CAFile_setData(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Buffer *buf, Error *e_rr);

//Set data of a file to a stream, only valid if it's a file.
// Moves stream to content (moves ref)
Bool CAFile_setDataStream(
	CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, StreamRef **stream, U64 off, U64 len, Error *e_rr
);

//Rename a file.
// Moves name if not a ref, otherwise copies.
Bool CAFile_rename(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, CharString *name, Error *e_rr);

Bool CAFile_move(CAFile *caFile, CAHandle fileHandle, CAHandle newParent, const Allocator *alloc, Error *e_rr);

//Adding/removing

CAHandle CAFile_add(
	CAFile *caFile, CAHandle parent, CharString *name, Ns time, Bool isFile, const Allocator *alloc, Error *e_rr
);

static inline CAHandle CAFile_addFile(
CAFile *caFile, CAHandle parent, CharString *name, Ns time, const Allocator *alloc, Error *e_rr
) {
	return CAFile_add(caFile, parent, name, time, true, alloc, e_rr);
}

static inline CAHandle CAFile_addFolder(
	CAFile *caFile, CAHandle parent, CharString *name, const Allocator *alloc, Error *e_rr
) {
	return CAFile_add(caFile, parent, name, 0, false, alloc, e_rr);
}

Bool CAFile_removeFile(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr);
Bool CAFile_removeFolder(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr);
Bool CAFile_remove(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr);

Bool CAFile_getInfo(const CAFile *caFile, CAHandle handle, FileInfo *info, const Allocator *alloc, Error *e_rr);

Bool CAFile_foreach(
	const CAFile *caFile,
	CAHandle fileHandle,
	FileCallback callback,
	void *object,
	Bool recurse,
	const Allocator *alloc,
	Error *e_rr
);

//Serialize

Bool CAFile_write(
	const CAFile *caFile,
	const RefPtrType *encStreamType,
	StreamRef *result,
	U64 *startOffset,
	const Allocator *alloc,
	Error *e_rr
);

Bool CAFile_read(
	StreamRef *file,
	const RefPtrType *encStreamType,
	U64 startOffset,
	const U32 encryptionKey[8],
	const Allocator *alloc,
	CAFile *caFile,
	Error *e_rr
);

//Combine

typedef enum EArchiveCombineMode {
	EArchiveCombineMode_RequireSame,							//Files are only allowed to merge if same contents
	EArchiveCombineMode_Rename,									//Try to rename the file on conflict
	EArchiveCombineMode_AcceptA,								//First archive is leading on conflict
	EArchiveCombineMode_AcceptB,								//Second archive is leading on conflict
	EArchiveCombineMode_Count
} EArchiveCombineMode;

typedef enum EArchiveCombineFlags {
	EArchiveCombineFlags_None = 0,
	EArchiveCombineFlags_ResolveLatestTimestamp = 1 << 0,	//Resolve timestamp with latest, as long as data matches
	EArchiveCombineFlags_ResolveAcceptLatest = 1 << 1		//Override file with latest file contents, otherwise conflict
} EArchiveCombineFlags;

Bool CAFile_combine(
	const CAFile *a,
	const CAFile *b,
	EArchiveCombineMode combineMode,
	EArchiveCombineFlags combineFlags,
	const Allocator *alloc,
	CAFile *combined,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
