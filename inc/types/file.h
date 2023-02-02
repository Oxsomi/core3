#pragma once
#include "types/string.h"

typedef enum EFileType {
	EFileType_Folder,
	EFileType_File,
	EFileType_Invalid
} EFileType;

typedef enum FileAccess {
	FileAccess_None,			//Invalid, never returned
	FileAccess_Read,
	FileAccess_Write,
	FileAccess_ReadWrite
} FileAccess;

typedef struct FileInfo {

	EFileType type;
	FileAccess access;
	Ns timestamp;		//In units that the file system supports. Normally that unit is seconds.
	String path;
	U64 fileSize;

} FileInfo;

typedef Error (*FileCallback)(FileInfo, void*);

typedef struct Allocator Allocator;
typedef struct String String;

Error File_resolve(
	String loc,
	Bool *isVirtual,
	U64 maxFilePathLimit,
	String absoluteDir,
	Allocator alloc,
	String *result
);

Bool FileInfo_free(FileInfo *info, Allocator alloc);
