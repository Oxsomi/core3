#pragma once
#include "types/list.h"
#include "types/string.h"
#include "types/file.h"

typedef enum EFileType EFileType;

typedef struct ArchiveEntry {

	String path;
	Buffer data;
	EFileType type;		//If true, data should be empty
	Ns timestamp;		//Shouldn't be set if isFolder. Will disappear

} ArchiveEntry;


typedef struct Archive {
	List entries;			//<ArchiveEntry>
} Archive;

Error Archive_create(Allocator alloc, Archive *archive);
Bool Archive_free(Archive *archive, Allocator alloc);

Bool Archive_hasFile(Archive *archive, String path, Allocator alloc);
Bool Archive_hasFolder(Archive *archive, String path, Allocator alloc);
Bool Archive_has(Archive *archive, String path, Allocator alloc);

Error Archive_addDirectory(Archive *archive, String path, Allocator alloc);
Error Archive_addFile(Archive *archive, String path, Buffer data, Ns timestamp, Allocator alloc);

Error Archive_updateFileData(Archive *archive, String path, Buffer data, Allocator alloc);

Error Archive_getFileData(Archive *archive, String path, Buffer *data, Allocator alloc);
Error Archive_getFileDataConst(Archive *archive, String path, Buffer *data, Allocator alloc);

Error Archive_removeFile(Archive *archive, String path, Allocator alloc);
Error Archive_removeFolder(Archive *archive, String path, Allocator alloc);
Error Archive_remove(Archive *archive, String path, Allocator alloc);

Error Archive_rename(
	Archive *archive, 
	String loc, 
	String newFileName, 
	Allocator alloc
);

Error Archive_move(
	Archive *archive, 
	String loc, 
	String directoryName, 
	Allocator alloc
);

Error Archive_getInfo(Archive *archive, String loc, FileInfo *info, Allocator alloc);

Error Archive_queryFileObjectCount(
	Archive *archive, 
	String loc, 
	EFileType type, 
	Bool isRecursive, 
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFileObjectCountAll(
	Archive *archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFileCount(
	Archive *archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_queryFolderCount(
	Archive *archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
);

Error Archive_foreach(
	Archive *archive,
	String loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Allocator alloc
);
