/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#pragma once
#include "types/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;

typedef struct CASettings CASettings;
typedef struct CAFile CAFile;
typedef struct ArchiveEntry ArchiveEntry;

typedef struct DLSettings DLSettings;
typedef struct DLFile DLFile;

typedef enum ESHSettingsFlags ESHSettingsFlags;
typedef enum ESHExtension ESHExtension;
typedef enum ESHBinaryType ESHBinaryType;

typedef struct SHEntry SHEntry;
typedef struct SHBinaryIdentifier SHBinaryIdentifier;
typedef struct SHBinaryInfo SHBinaryInfo;
typedef struct SHEntryRuntime SHEntryRuntime;
typedef struct SHInclude SHInclude;
typedef struct SHFile SHFile;

typedef struct ListCharString ListCharString;
typedef struct ListSHEntryRuntime ListSHEntryRuntime;
typedef struct ListSHInclude ListSHInclude;

//oiCA

Bool CAFile_freex(CAFile *caFile);

Error CAFile_writex(CAFile caFile, Buffer *result);
Error CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile);

//oiDL

Error DLFile_createx(DLSettings settings, DLFile *dlFile);
Bool DLFile_freex(DLFile *dlFile);

Error DLFile_createListx(DLSettings settings, ListBuffer *buffers, DLFile *dlFile);
Error DLFile_createUTF8Listx(DLSettings settings, ListBuffer buffers, DLFile *dlFile);
Error DLFile_createBufferListx(DLSettings settings, ListBuffer buffers, DLFile *dlFile);
Error DLFile_createAsciiListx(DLSettings settings, ListCharString strings, DLFile *dlFile);

Error DLFile_addEntryx(DLFile *dlFile, Buffer entry);
Error DLFile_addEntryAsciix(DLFile *dlFile, CharString entry);
Error DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry);

Error DLFile_writex(DLFile dlFile, Buffer *result);
Error DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile);

//oiSH

Bool SHFile_createx(ESHSettingsFlags flags, U32 compilerVersion, U32 sourceHash, SHFile *shFile, Error *e_rr);
void SHFile_freex(SHFile *shFile);

Bool SHFile_addBinaryx(SHFile *shFile, SHBinaryInfo *binaries, Error *e_rr);	//Moves entry
Bool SHFile_addEntrypointx(SHFile *shFile, SHEntry *entry, Error *e_rr);		//Moves entry->name
Bool SHFile_addIncludex(SHFile *shFile, SHInclude *include, Error *e_rr);		//Moves include->relativePath

Bool SHFile_writex(SHFile shFile, Buffer *result, Error *e_rr);
Bool SHFile_readx(Buffer file, Bool isSubFile, SHFile *shFile, Error *e_rr);
Bool SHFile_combinex(SHFile a, SHFile b, SHFile *combined, Error *e_rr);

void SHEntry_printx(SHEntry entry);
void SHEntryRuntime_printx(SHEntryRuntime entry);
void SHBinaryInfo_printx(SHBinaryInfo binary);

void SHBinaryIdentifier_freex(SHBinaryIdentifier *identifier);
void SHBinaryInfo_freex(SHBinaryInfo *info);
void SHEntry_freex(SHEntry *entry);

void SHEntryRuntime_freex(SHEntryRuntime *entry);
void ListSHEntryRuntime_freeUnderlyingx(ListSHEntryRuntime *entry);

void ListSHInclude_freeUnderlyingx(ListSHInclude *includes);
void SHInclude_freex(SHInclude *include);

#ifdef __cplusplus
	}
#endif
