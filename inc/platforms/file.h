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
#include "types/types.h"
#include "types/string.h"
#include "types/file.h"

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

Bool File_write(Buffer buf, CharString loc, Ns maxTimeout, Error *e_rr);			//Write when a file is available
Bool File_read(CharString loc, Ns maxTimeout, Buffer *output, Error *e_rr);			//Read when a file is available

typedef struct FileLoadVirtual {
	Bool doLoad;
	const U32 *encryptionKey;
} FileLoadVirtual;

Bool File_loadVirtual(CharString loc, const U32 encryptionKey[8], Error *e_rr);		//Load a virtual section
Bool File_isVirtualLoaded(CharString loc, Error *e_rr);								//Check if a virtual section is loaded
Bool File_unloadVirtual(CharString loc, Error *e_rr);								//Unload a virtual section

//TODO: make it more like a DirectStorage-like api

#ifdef __cplusplus
	}
#endif
