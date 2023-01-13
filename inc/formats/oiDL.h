#pragma once
#include "oiXX.h"
#include "types/list.h"

typedef enum EDLDataType {
	EDLDataType_Data,									//(default)
	EDLDataType_Ascii,									//--ascii
	EDLDataType_UTF8,									//--utf8
	EDLDataType_Count
} EDLDataType;

typedef enum EDLSettingsFlags {
	EDLSettingsFlags_None				= 0,
	EDLSettingsFlags_UseSHA256			= 1 << 0,		//--sha256
	EDLSettingsFlags_Invalid			= 0xFFFFFFFF << 1
} EDLSettingsFlags;

typedef struct DLSettings {

	EXXCompressionType compressionType;
	EXXEncryptionType encryptionType;
	EDLDataType dataType;
	EDLSettingsFlags flags;

	U32 encryptionKey[8];

} DLSettings;

typedef union DLEntry {

	Buffer entryBuffer;
	String entryString;

} DLEntry;

//Check docs/oiDL.md for the file spec

typedef struct DLFile {

	List entries;				//<DLEntry>
	DLSettings settings;

} DLFile;

Error DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile);
Bool DLFile_free(DLFile *dlFile, Allocator alloc);

//DLEntry will belong to DLFile.
//This means that freeing it will free the String + Buffer if they're not a ref
//So be sure to make them a ref if needed.

Error DLFile_addEntry(DLFile *dlFile, Buffer entry, Allocator alloc);
Error DLFile_addEntryAscii(DLFile *dlFile, String entry, Allocator alloc);
//Error DLFile_addEntryUTF8(DLFile *dlFile, String entry, Allocator alloc);

Error DLFile_write(DLFile dlFile, Allocator alloc, Buffer *result);
Error DLFile_read(Buffer file, Allocator alloc, DLFile *dlFile);
