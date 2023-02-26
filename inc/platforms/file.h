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
#include "types/types.h"
#include "types/string.h"
#include "types/file.h"

typedef struct Error Error;

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

Error File_getInfo(String loc, FileInfo *info);
Error File_resolvex(String loc, Bool *isVirtual, U64 maxFilePathLimit, String *result);

Bool FileInfo_freex(FileInfo *fileInfo);

Error File_foreach(String loc, FileCallback callback, void *userData, Bool isRecursive);

Error File_remove(String loc, Ns maxTimeout);
Error File_add(String loc, EFileType type, Ns maxTimeout);

Error File_rename(String loc, String newFileName, Ns maxTimeout);
Error File_move(String loc, String directoryName, Ns maxTimeout);

Error File_queryFileObjectCount(String loc, EFileType type, Bool isRecursive, U64 *res);		//Includes files only
Error File_queryFileObjectCountAll(String loc, Bool isRecursive, U64 *res);						//Includes folders + files

Error File_queryFileCount(String loc, Bool isRecursive, U64 *res);
Error File_queryFolderCount(String loc, Bool isRecursive, U64 *res);

Bool File_has(String loc);
Bool File_hasType(String loc, EFileType type);

Bool File_hasFile(String loc);
Bool File_hasFolder(String loc);

Error File_write(Buffer buf, String loc, Ns maxTimeout);		//Read when a file is available (up to maxTimeout)
Error File_read(String loc, Ns maxTimeout, Buffer *output);		//Write when a file is available (up to maxTimeout)

impl Error File_removeVirtual(String loc, Ns maxTimeout);						//Can only operate on //access
impl Error File_addVirtual(String loc, EFileType type, Ns maxTimeout);			//Can only operate on folders in //access
impl Error File_renameVirtual(String loc, String newFileName, Ns maxTimeout);
impl Error File_moveVirtual(String loc, String directoryName, Ns maxTimeout);

impl Error File_writeVirtual(Buffer buf, String loc, Ns maxTimeout);
impl Error File_readVirtual(String loc, Buffer *output, Ns maxTimeout);

impl Error File_getInfoVirtual(String loc, FileInfo *info);
impl Error File_foreachVirtual(String loc, FileCallback callback, void *userData, Bool isRecursive);
impl Error File_queryFileObjectCountVirtual(String loc, EFileType type, Bool isRecursive, U64 *res);		//Inc files only
impl Error File_queryFileObjectCountAllVirtual(String loc, Bool isRecursive, U64 *res);						//Inc folders + files

impl Error File_loadVirtual(String loc, const U32 encryptionKey[8]);		//Load a virtual section
impl Bool File_isVirtualLoaded(String loc);		//Check if a virtual section is loaded
impl Error File_unloadVirtual(String loc);		//Unload a virtual section

//TODO: make it more like a DirectStorage-like api
