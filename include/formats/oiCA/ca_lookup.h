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

//formats/oiCA/ca_lookup.h

#pragma once
#include "types/container/file_base.h"
#include "formats/oiCA/ca_props.h"

#ifdef __cplusplus
	extern "C" {
#endif

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

#ifdef __cplusplus
	}
#endif
