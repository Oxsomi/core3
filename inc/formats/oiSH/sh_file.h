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
#include "formats/oiSH/binaries.h"
#include "formats/oiSH/entries.h"

#ifdef __cplusplus
	extern "C" {
#endif

#define OISH_SHADER_MODEL(maj, min) ((U16)((min) | ((maj) << 8)))

typedef enum ESHSettingsFlags {
	ESHSettingsFlags_None				= 0,
	ESHSettingsFlags_HideMagicNumber	= 1 << 0,		//Only valid if the oiSH can be 100% confidently detected otherwise
	ESHSettingsFlags_IsUTF8				= 1 << 1,		//If one of the entrypoint strings was UTF8
	ESHSettingsFlags_Invalid			= 0xFFFFFFFF << 2
} ESHSettingsFlags;

//Check docs/oiSH.md for the file spec

typedef struct SHInclude {

	CharString relativePath;	//Path relative to oiSH source's directory (e.g. ../Includes/myInclude.hlsli)

	U32 crc32c;					//Content CRC32C. However, if it contains \\r it's removed first!
	U32 padding;

} SHInclude;

TList(SHInclude);

void SHInclude_free(SHInclude *include, Allocator alloc);
void ListSHInclude_freeUnderlying(ListSHInclude *includes, Allocator alloc);

typedef struct SHFile {

	ListSHBinaryInfo binaries;
	ListSHEntry entries;
	ListSHInclude includes;

	U64 readLength;				//How many bytes were read for this file

	ESHSettingsFlags flags;

	U32 compilerVersion;		//OxC3 compiler version

	U32 sourceHash;				//CRC32C of sources

} SHFile;

TList(SHFile);

Bool SHFile_create(
	ESHSettingsFlags flags,
	U32 compilerVersion,
	U32 sourceHash,
	Allocator alloc,
	SHFile *shFile,
	Error *e_rr
);

void SHFile_free(SHFile *shFile, Allocator alloc);

Bool SHFile_addBinaries(
	SHFile *shFile,
	SHBinaryInfo *binaries,				//Moves binaries
	Allocator alloc,
	Error *e_rr
);

Bool SHFile_addEntrypoint(SHFile *shFile, SHEntry *entry, Allocator alloc, Error *e_rr);	//Moves entry->name and binaryIds
Bool SHFile_addInclude(SHFile *shFile, SHInclude *include, Allocator alloc, Error *e_rr);	//Moves include->name

Bool SHFile_write(SHFile shFile, Allocator alloc, Buffer *result, Error *e_rr);
Bool SHFile_read(Buffer file, Bool isSubFile, Allocator alloc, SHFile *shFile, Error *e_rr);

Bool SHFile_combine(SHFile a, SHFile b, Allocator alloc, SHFile *combined, Error *e_rr);

#ifdef __cplusplus
	}
#endif
