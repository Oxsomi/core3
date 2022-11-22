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

inline Bool File_isVirtual(String loc) { return String_getAt(loc, 0) == '/' && String_getAt(loc, 1) == '/'; }

Error File_foreach(String loc, FileCallback callback, void *userData, Bool isRecursive);

Error File_remove(String loc, Ns maxTimeout);
Error File_add(String loc, FileType type, Ns maxTimeout);

Error File_rename(String loc, String newFileName, Ns maxTimeout);
Error File_move(String loc, String directoryName, Ns maxTimeout);

Error File_queryFileObjectCount(String loc, FileType type, Bool isRecursive, U64 *res);		//Includes files only
Error File_queryFileObjectCountAll(String loc, Bool isRecursive, U64 *res);					//Includes folders + files

inline Error File_queryFileCount(String loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, FileType_File, isRecursive, res); 
}

inline Error File_queryFolderCount(String loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, FileType_Folder, isRecursive, res); 
}

Error File_resolve(String loc, Bool *isVirtual, String *result);
Bool File_exists(String loc);
Bool File_existsAsType(String loc, FileType type);

inline Bool File_fileExists(String loc) { return File_existsAsType(loc, FileType_File); }
inline Bool File_folderExists(String loc) { return File_existsAsType(loc, FileType_Folder); }

Error File_write(Buffer buf, String loc, Ns maxTimeout);		//Read when a file is available (up to maxTimeout)
Error File_read(String loc, Buffer *output, Ns maxTimeout);		//Write when a file is available (up to maxTimeout)

impl Error File_removeVirtual(String loc, Ns maxTimeout);						//Can only operate on //access
impl Error File_addVirtual(String loc, FileType type, Ns maxTimeout);			//Can only operate on folders in //access
impl Error File_renameVirtual(String loc, String newFileName, Ns maxTimeout);
impl Error File_moveVirtual(String loc, String directoryName, Ns maxTimeout);

impl Error File_writeVirtual(Buffer buf, String loc, Ns maxTimeout);
impl Error File_readVirtual(String loc, Buffer *output, Ns maxTimeout);

impl Error File_getInfoVirtual(String loc, FileInfo *info);
impl Error File_foreachVirtual(String loc, FileCallback callback, void *userData, Bool isRecursive);
impl Error File_queryFileObjectCountVirtual(String loc, FileType type, Bool isRecursive, U64 *res);		//Inc files only
impl Error File_queryFileObjectCountAllVirtual(String loc, Bool isRecursive, U64 *res);					//Inc folders + files

//TODO: make it more like a DirectStorage-like api
