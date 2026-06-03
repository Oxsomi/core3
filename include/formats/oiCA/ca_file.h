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

//formats/oiCA/ca_file.h

#pragma once
#include "types/container/list.h"
#include "formats/oiXX/oiXX.h"
#include "formats/oiDL/dl_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ECASettingsFlags {

	ECASettingsFlags_None                = 0,
	ECASettingsFlags_IncludeDate        = 1 << 0,            //--date
	ECASettingsFlags_IncludeFullDate    = 1 << 1,            //--full-date (automatically sets --date)

	ECASettingsFlags_Invalid            = 0xFFFFFFFF << 2,
	ECASettingsFlags_DateFlags            = ECASettingsFlags_IncludeDate | ECASettingsFlags_IncludeFullDate

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
	U64 parentTimestamp;    //16 : 48 (timestamp in ms)
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
static const U32 CAFile_maxRecursionSize = 128;        //Must match chainSize (walking file parents)

//Check docs/oiCA.md for the file spec

typedef struct CAFile {
	DLFile names;
	DLFile content;
	ListCAFolderInfo folders;   //0th is reserved as root folder
	ListCAFileInfo files;
	CASettings settings;        //Must remain 8-byte aligned
} CAFile;

TList(CAFile);

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

static inline CAHandle CAHandle_makeFolder(U64 id) { return ((U64)1 << 63) | id; }    //Unsafe, only use if you know it's a dir
static inline CAHandle CAHandle_makeFile(U64 id) { return id; }                       //Unsafe, only use if you know it's a file

static inline Bool CAHandle_isRoot(CAHandle handle) {
	return CAHandle_isFolder(handle) && !CAHandle_getId(handle);
}

static inline const CAFolderInfo *CAFile_getFolderInfoPtr(const CAFile *caFile, CAHandle handle) {

	if (!caFile || !CAHandle_isFolder(handle))
		return NULL;

	U64 id = CAHandle_getId(handle);

	if (id >= caFile->folders.length)
		return NULL;

	return &caFile->folders.ptr[id];
}

static inline const CAFileInfo *CAFile_getFileInfoPtr(const CAFile *caFile, CAHandle handle) {

	if (!caFile || !CAHandle_isFile(handle))
		return NULL;

	U64 id = CAHandle_getId(handle);

	if (id >= caFile->files.length)
		return NULL;

	return &caFile->files.ptr[id];
}

#ifdef __cplusplus
	}
#endif
