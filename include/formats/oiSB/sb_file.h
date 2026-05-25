/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//formats/oiSB/sb_file.h

#pragma once
#include "formats/oiDL/dl_file.h"
#include "formats/oiSB/sb_variable.h"
#include "types/container/list_predeclare.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

typedef enum ESBSettingsFlags {
	ESBSettingsFlags_None				= 0,
	ESBSettingsFlags_HideMagicNumber	= 1 << 0,		//Only valid if the oiSH can be 100% confidently detected otherwise
	ESBSettingsFlags_IsTightlyPacked	= 1 << 1,
	ESBSettingsFlags_CreateNoReserve	= 1 << 2,		//Only for SBFile_create avoid reserve (when you already know the size)
	ESBSettingsFlags_Invalid			= 0xFFFFFFFF << 2
} ESBSettingsFlags;

typedef struct SBFile {

	DLFile names;				//[0, structNames - 1], [structNames, structNames + varNames - 1]

	ListSBStruct structs;
	ListSBVar vars;
	ListListU32 arrays;

	ESBSettingsFlags flags;		//flags and bufferSize are assumed to be a single U64 combined (need 8-byte alignment)
	U32 bufferSize;

	U64 hash;					//Appending to the SBFile will automatically refresh this

} SBFile;

TList(SBFile);

Bool SBFile_create(
	ESBSettingsFlags flags,
	U32 bufferSize,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
);

Bool SBFile_createCopy(
	const SBFile *src,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
);

void SBFile_free(SBFile *shFile, const Allocator *alloc);

Bool SBFile_addStruct(SBFile *sbFile, CharString *name, SBStruct sbStruct, const Allocator *alloc, Error *e_rr);

Bool SBFile_addVariableAsType(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,		//root = U16_MAX
	ESBType type,
	ESBVarFlag flags,
	ListU32 *arrays,
	const Allocator *alloc,
	Error *e_rr
);

Bool SBFile_addVariableAsStruct(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,		//root = U16_MAX
	U16 structId,
	ESBVarFlag flags,
	ListU32 *arrays,
	const Allocator *alloc,
	Error *e_rr
);

Bool SBFile_write(
	const SBFile *sbFile,
	const Allocator *alloc,
	StreamRef *streamRef,	//Pass NULL to calculate length only (*offset)
	U64 *offset,
	Error *e_rr
);

Bool SBFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SBFile *sbFile, Error *e_rr);

//Logs stringified SBFile directly
void SBFile_print(const SBFile *sbFile, U64 indenting, U16 parent, Bool isRecursive, const Allocator *alloc);

//Doesn't work on layouts that mismatch (only order of structs/variables or some flags may vary)
Bool SBFile_combine(const SBFile *a, const SBFile *b, const Allocator *alloc, SBFile *combined, Error *e_rr);

void ListSBFile_freeUnderlying(ListSBFile *files, const Allocator *alloc);

//File headers

//File spec (docs/oiSB.md)

typedef enum ESBVersion {
	ESBVersion_Undefined,
	ESBVersion_V0_1,		//Unsupported
	ESBVersion_V1_2			//Current
} ESBVersion;

typedef enum ESBFlag {
	ESBFlag_None				= 0,
	ESBFlag_IsTightlyPacked		= 1 << 0,
	ESBFlag_Unsupported			= 0xFFFFFFFF << 1
} ESBFlag;

typedef struct SBHeader {

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
	U8 flags;					//ESBFlag
	U16 arrays;

	U16 structs;
	U16 vars;

	U32 bufferSize;

} SBHeader;

#define SBHeader_MAGIC 0x4253696F

#ifdef __cplusplus
	}
#endif
