/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "formats/oiSB/variable.h"
#include "formats/oiXX/oiXX.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ESBSettingsFlags {
	ESBSettingsFlags_None				= 0,
	ESBSettingsFlags_HideMagicNumber	= 1 << 0,		//Only valid if the oiSH can be 100% confidently detected otherwise
	ESBSettingsFlags_IsUTF8				= 1 << 1,
	ESBSettingsFlags_IsTightlyPacked	= 1 << 2,
	ESBSettingsFlags_Invalid			= 0xFFFFFFFF << 3
} ESBSettingsFlags;

typedef struct SBFile {

	ListCharString structNames;
	ListCharString varNames;

	ListSBStruct structs;
	ListSBVar vars;
	ListListU32 arrays;

	U64 readLength;				//How many bytes were read for this file

	ESBSettingsFlags flags;		//flags and bufferSize are assumed to be a single U64 combined
	U32 bufferSize;

	U64 hash;					//Appending to the SBFile will automatically refresh this

} SBFile;

TList(SBFile);

Bool SBFile_create(
	ESBSettingsFlags flags,
	U32 bufferSize,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
);

Bool SBFile_createCopy(
	SBFile src,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
);

void SBFile_free(SBFile *shFile, Allocator alloc);

Bool SBFile_addStruct(SBFile *sbFile, CharString *name, SBStruct sbStruct, Allocator alloc, Error *e_rr);

Bool SBFile_addVariableAsType(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,		//root = U16_MAX
	ESBType type,
	ESBVarFlag flags,
	ListU32 *arrays,
	Allocator alloc,
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
	Allocator alloc,
	Error *e_rr
);

Bool SBFile_write(SBFile sbFile, Allocator alloc, Buffer *result, Error *e_rr);
Bool SBFile_read(Buffer file, Bool isSubFile, Allocator alloc, SBFile *sbFile, Error *e_rr);

void SBFile_print(SBFile sbFile, U64 indenting, U16 parent, Bool isRecursive, Allocator alloc);

//Doesn't work on layouts that mismatch (only order of structs/variables or some flags may vary)
Bool SBFile_combine(SBFile a, SBFile b, Allocator alloc, SBFile *combined, Error *e_rr);

void ListSBFile_freeUnderlying(ListSBFile *files, Allocator alloc);

#ifndef DISALLOW_SB_OXC3_PLATFORMS

	Bool SBFile_createx(
		ESBSettingsFlags flags,
		U32 bufferSize,
		SBFile *sbFile,
		Error *e_rr
	);

	Bool SBFile_createCopyx(SBFile src, SBFile *dst, Error *e_rr);

	void SBFile_freex(SBFile *shFile);

	Bool SBFile_addStructx(SBFile *sbFile, CharString *name, SBStruct sbStruct, Error *e_rr);

	Bool SBFile_addVariableAsTypex(
		SBFile *sbFile,
		CharString *name,
		U32 offset,
		U16 parentId,		//root = U16_MAX
		ESBType type,
		ESBVarFlag flags,
		ListU32 *arrays,
		Error *e_rr
	);

	Bool SBFile_addVariableAsStructx(
		SBFile *sbFile,
		CharString *name,
		U32 offset,
		U16 parentId,		//root = U16_MAX
		U16 structId,
		ESBVarFlag flags,
		ListU32 *arrays,
		Error *e_rr
	);

	Bool SBFile_writex(SBFile sbFile, Buffer *result, Error *e_rr);
	Bool SBFile_readx(Buffer file, Bool isSubFile, SBFile *sbFile, Error *e_rr);
	void SBFile_printx(SBFile sbFile, U64 indenting, U16 parent, Bool isRecursive);

	void ListSBFile_freeUnderlyingx(ListSBFile *files);

	Bool SBFile_combinex(SBFile a, SBFile b, SBFile *combined, Error *e_rr);

#endif

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
