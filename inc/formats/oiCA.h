#pragma once
#include "types/string.h"
#include "types/list.h"

typedef enum ECACompressionType {
	ECACompressionType_None,							//--uncompressed
	ECACompressionType_Brotli11,						//(default)
	ECACompressionType_Brotli1,							//--fast-compress	(speed of compression over storage)
	ECACompressionType_Count
} ECACompressionType;

typedef enum ECAEncryptionType {
	ECAEncryptionType_None,								//(default)
	ECAEncryptionType_AES256,							//-aes <32-byte key (in hex or nyto)>
	ECAEncryptionType_Count
} ECAEncryptionType;

typedef enum ECASettingsFlags {
	ECASettingsFlags_None				= 0,
	ECASettingsFlags_IncludeDate		= 1 << 0,			//--date
	ECASettingsFlags_IncludeFullDate	= 1 << 1,			//--full-date (automatically sets --date)
	ECASettingsFlags_UseSHA256			= 1 << 2,			//--sha256
	ECASettingsFlags_Invalid			= 0xFFFFFFFF << 3
} ECASettingsFlags;

typedef struct CASettings {

	ECACompressionType compressionType;
	ECAEncryptionType encryptionType;
	ECASettingsFlags flags;

	U32 encryptionKey[8];

} CASettings;

typedef struct CAEntry {

	String path;
	Buffer data;
	Bool isFolder;		//If true, data should be empty
	Ns timestamp;		//Shouldn't be set if isFolder. Will disappear

} CAEntry;

//Check docs/oiCA.md for the file spec

typedef struct CAFile {

	List entries;				//<CAEntry>
	CASettings settings;

} CAFile;

Error CAFile_create(CASettings settings, Allocator alloc, CAFile *caFile);
Bool CAFile_free(CAFile *caFile, Allocator alloc);

//Also adds subdirs. CAEntry will belong to CAFile.
//This means that freeing it will free the String + Buffer if they're not a ref
//So be sure to make them a ref if needed.
//
Error CAFile_addEntry(CAFile *caFile, CAEntry entry, Allocator alloc);

Error CAFile_write(CAFile caFile, Allocator alloc, Buffer *result);
Error CAFile_read(Buffer file, Allocator alloc, CAFile *caFile);
