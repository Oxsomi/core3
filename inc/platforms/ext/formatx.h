#pragma once
#include "types/types.h"

typedef struct Error Error;
typedef struct String String;

typedef struct CASettings CASettings;
typedef struct CAFile CAFile;
typedef struct ArchiveEntry ArchiveEntry;

typedef struct DLSettings DLSettings;
typedef struct DLFile DLFile;

//oiCA

Bool CAFile_freex(CAFile *caFile);
 
Error CAFile_writex(CAFile caFile, Buffer *result);
Error CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile);

//oiDL

Error DLFile_createx(DLSettings settings, DLFile *dlFile);
Bool DLFile_freex(DLFile *dlFile);

Error DLFile_addEntryx(DLFile *dlFile, Buffer entry);
Error DLFile_addEntryAsciix(DLFile *dlFile, String entry);
Error DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry);

Error DLFile_writex(DLFile dlFile, Buffer *result);
Error DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile);
