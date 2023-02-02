#pragma once
#include "oiXX.h"
#include "types/archive.h"

typedef enum EFileType EFileType;

typedef enum ECASettingsFlags {
	ECASettingsFlags_None				= 0,
	ECASettingsFlags_IncludeDate		= 1 << 0,			//--date
	ECASettingsFlags_IncludeFullDate	= 1 << 1,			//--full-date (automatically sets --date)
	ECASettingsFlags_UseSHA256			= 1 << 2,			//--sha256
	ECASettingsFlags_Invalid			= 0xFFFFFFFF << 3
} ECASettingsFlags;

typedef struct CASettings {
	EXXCompressionType compressionType;
	EXXEncryptionType encryptionType;
	ECASettingsFlags flags;
	U32 encryptionKey[8];
} CASettings;

//Check docs/oiCA.md for the file spec

typedef struct CAFile {
	Archive archive;
	CASettings settings;
} CAFile;

Error CAFile_create(CASettings settings, Archive archive, Allocator alloc, CAFile *caFile);
Bool CAFile_free(CAFile *caFile, Allocator alloc);

//Serialize

Error CAFile_write(CAFile caFile, Allocator alloc, Buffer *result);
Error CAFile_read(Buffer file, Allocator alloc, CAFile *caFile);
