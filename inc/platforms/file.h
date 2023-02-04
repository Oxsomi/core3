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
impl Error File_queryFileObjectCountAllVirtual(String loc, Bool isRecursive, U64 *res);					//Inc folders + files

//TODO: make it more like a DirectStorage-like api
