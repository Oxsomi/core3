#pragma once
#include "types/file.h"

typedef struct Error Error;
typedef struct Archive Archive;
typedef struct String String;

Error Archive_createx(Archive *archive);
Bool Archive_freex(Archive *archive);

Bool Archive_hasFilex(Archive archive, String path);
Bool Archive_hasFolderx(Archive archive, String path);
Bool Archive_hasx(Archive archive, String path);

Error Archive_addDirectoryx(Archive *archive, String path);
Error Archive_addFilex(Archive *archive, String path, Buffer data, Ns timestamp);

Error Archive_updateFileDatax(Archive *archive, String path, Buffer data);

Error Archive_getFileDatax(Archive archive, String path, Buffer *data);
Error Archive_getFileDataConstx(Archive archive, String path, Buffer *data);

Error Archive_removeFilex(Archive *archive, String path);
Error Archive_removeFolderx(Archive *archive, String path);
Error Archive_removex(Archive *archive, String path);

Error Archive_renamex(Archive *archive, String loc, String newFileName);
Error Archive_movex(Archive *archive, String loc, String directoryName);

Error Archive_getInfox(Archive archive, String loc, FileInfo *info);

Error Archive_queryFileObjectCountx(
	Archive archive, 
	String loc, 
	EFileType type, 
	Bool isRecursive, 
	U64 *res
);

Error Archive_queryFileObjectCountAllx(Archive archive, String loc, Bool isRecursive, U64 *res);
Error Archive_queryFileCountx(Archive archive, String loc, Bool isRecursive, U64 *res);
Error Archive_queryFolderCountx(Archive archive, String loc, Bool isRecursive, U64 *res);

Error Archive_foreachx(
	Archive archive,
	String loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type
);
