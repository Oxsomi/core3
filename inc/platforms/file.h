#pragma once
#include "types/types.h"
#include "types/string.h"

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
//	   while test.json, ./test.json or .//test.json refers to test.json in the local file
//
//Backslashes are automatically resolved to forward slash

typedef enum FileType {
	FileType_Folder,
	FileType_File
} FileType;

typedef enum FileAccess {
	FileAccess_None,			//Invalid, never returned
	FileAccess_Read,
	FileAccess_Write,
	FileAccess_ReadWrite
} FileAccess;

typedef struct FileInfo {

	FileType type;
	FileAccess access;
	Ns timestamp;		//In units that the file system supports. Normally that unit is seconds.
	String path;
	U64 fileSize;

} FileInfo;

typedef Error (*FileCallback)(FileInfo, void*);

Error File_getInfo(String loc, FileInfo *info);
Error FileInfo_free(FileInfo *info);

Error File_foreach(String loc, FileCallback callback, void *userData, bool isRecursive);

Error File_remove(String loc);

Error File_queryFileObjectCount(String loc, FileType type, bool isRecursive, U64 *res);		//Includes files only
Error File_queryFileObjectCountAll(String loc, bool isRecursive, U64 *res);					//Includes folders + files

inline Error File_queryFileCount(String loc, bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, FileType_File, isRecursive, res); 
}

inline Error File_queryFolderCount(String loc, bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, FileType_Folder, isRecursive, res); 
}

Error File_resolve(String loc, Bool *isVirtual, String *result);

Error File_write(Buffer buf, String loc);
Error File_read(String loc, Buffer *output);

impl Error File_removeVirtual(String loc);											//Can only operate on //access
impl Error File_writeVirtual(Buffer buf, String loc);
impl Error File_readVirtual(String loc, Buffer *output);
impl Error File_getInfoVirtual(String loc, FileInfo *info);
impl Error File_foreachVirtual(String loc, FileCallback callback, void *userData, bool isRecursive);
impl Error File_queryFileObjectCountVirtual(String loc, FileType type, bool isRecursive, U64 *res);		//Inc files only
impl Error File_queryFileObjectCountAllVirtual(String loc, bool isRecursive, U64 *res);					//Inc folders + files

//TODO: make it more like a DirectStorage-like api
