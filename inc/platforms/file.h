/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "types/types.h"
#include "types/string.h"
#include "types/file.h"

//There are two types of files; virtual and local.
//Virtual are embedded into the binary, very nice for easy portable installation and harder to modify for avg users.
//	Writable virtual files can only be in the //access directory.
//	These are just links to existing local files, but are made accessible by the platform for access by the app.
//	As much, naming a folder in the root "access" is disallowed for virtual files.
//Local are in the current working directory.
//OxC3 doesn't support reading from other directories; this is to prevent unwanted access to other files.
//Importing/exporting other files in other locations will have to be done through
// the //access folder using the platform's explorer (TBD).
//
//Virtual files are prefixed with // even though that can be valid
//E.g. //test.json refers to test.json embedded in the binary
//	while test.json, ./test.json or .//test.json refers to test.json in the local file
//
//Backslashes are automatically resolved to forward slash

Error File_getInfo(CharString loc, FileInfo *info);
Error File_resolvex(CharString loc, Bool *isVirtual, U64 maxFilePathLimit, CharString *result);

Bool FileInfo_freex(FileInfo *fileInfo);

Error File_foreach(CharString loc, FileCallback callback, void *userData, Bool isRecursive);

Error File_remove(CharString loc, Ns maxTimeout);
Error File_add(CharString loc, EFileType type, Ns maxTimeout);

Error File_rename(CharString loc, CharString newFileName, Ns maxTimeout);
Error File_move(CharString loc, CharString directoryName, Ns maxTimeout);

Error File_queryFileObjectCount(CharString loc, EFileType type, Bool isRecursive, U64 *res);		//Includes files only
Error File_queryFileObjectCountAll(CharString loc, Bool isRecursive, U64 *res);						//Includes folders + files

Error File_queryFileCount(CharString loc, Bool isRecursive, U64 *res);
Error File_queryFolderCount(CharString loc, Bool isRecursive, U64 *res);

Bool File_has(CharString loc);
Bool File_hasType(CharString loc, EFileType type);

Bool File_hasFile(CharString loc);
Bool File_hasFolder(CharString loc);

Error File_write(Buffer buf, CharString loc, Ns maxTimeout);		//Read when a file is available (up to maxTimeout)
Error File_read(CharString loc, Ns maxTimeout, Buffer *output);		//Write when a file is available (up to maxTimeout)

typedef struct FileLoadVirtual {
	Bool doLoad;
	const U32 *encryptionKey;
} FileLoadVirtual;

Error File_removeVirtual(CharString loc, Ns maxTimeout);						//Can only operate on //access
Error File_addVirtual(CharString loc, EFileType type, Ns maxTimeout);			//Can only operate on folders in //access
Error File_renameVirtual(CharString loc, CharString newFileName, Ns maxTimeout);
Error File_moveVirtual(CharString loc, CharString directoryName, Ns maxTimeout);

Error File_writeVirtual(Buffer buf, CharString loc, Ns maxTimeout);
Error File_readVirtual(CharString loc, Buffer *output, Ns maxTimeout);

Error File_getInfoVirtual(CharString loc, FileInfo *info);
Error File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive);
Error File_queryFileObjectCountVirtual(CharString loc, EFileType type, Bool isRecursive, U64 *res);		//Inc files only
Error File_queryFileObjectCountAllVirtual(CharString loc, Bool isRecursive, U64 *res);						//Inc folders + files

Error File_loadVirtual(CharString loc, const U32 encryptionKey[8]);		//Load a virtual section
Bool File_isVirtualLoaded(CharString loc);		//Check if a virtual section is loaded
Error File_unloadVirtual(CharString loc);		//Unload a virtual section

//TODO: make it more like a DirectStorage-like api
