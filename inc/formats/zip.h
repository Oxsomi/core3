#pragma once
#include "types/types.h"

typedef struct Allocator Allocator;
typedef struct Error Error;
typedef struct List List;
typedef struct String String;

typedef struct ZipEntry {

	String path;
	Buffer data;
	Bool isEncrypted;

} ZipEntry;

typedef struct ZipSettings {

	U32 segmentSize;		//How many bytes a zip file is split into

} ZipSettings;

typedef struct ZipFile {

	List entries;
	ZipSettings settings;

} ZipFile;

Error Zip_create(ZipFile *zipFile, ZipSettings settings, Allocator alloc);
Error Zip_free(ZipFile *zipFile, Allocator alloc);

Error Zip_addEntry(ZipFile *zipFile, ZipEntry entry);
Error Zip_removeEntry(ZipFile *zipFile, ZipEntry entry);

Error Zip_write(ZipFile zipFile, Allocator alloc, Buffer *result);
