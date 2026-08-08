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

//formats/oiSH/sh_file.h

#pragma once
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_entries.h"
#include "types/container/string.h"
#include "types/base/type_id.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

#define OISH_SHADER_MODEL(maj, min) ((U16)((min) | ((maj) << 8)))
#define OISH_SHADER_MODEL8(maj, min) ((U8)((min) | ((maj) << 4)))

static const U16 OISH_SHADER_MODEL_MIN = OISH_SHADER_MODEL(6, 5);     //6.5 at least
static const U16 OISH_SHADER_MODEL_MAX = OISH_SHADER_MODEL(6, 10);    //6.10 max

static const U8 OISH_SHADER_MODEL_MIN8 = OISH_SHADER_MODEL8(6, 5);    //6.5 at least
static const U8 OISH_SHADER_MODEL_MAX8 = OISH_SHADER_MODEL8(6, 10);   //6.10 max

typedef enum ESHSettingsFlags {
	ESHSettingsFlags_None               = 0,
	ESHSettingsFlags_HideMagicNumber    = 1 << 0,          //Only valid if the oiSH can be 100% confidently detected otherwise
	ESHSettingsFlags_ReflectionOnly     = 1 << 1,          //Carries reflection but no compiled binaries
	ESHSettingsFlags_Invalid            = 0xFFFFFFFF << 2
} ESHSettingsFlags;

typedef enum ECompilerWarning {                            //Present here in case shader compiler isn't present
	ECompilerWarning_None               = 0,
	ECompilerWarning_UnusedRegisters    = 1 << 0,
	ECompilerWarning_UnusedConstants    = 1 << 1,
	ECompilerWarning_BufferPadding      = 1 << 2
} ECompilerWarning;

//Check docs/oiSH.md for the file spec

typedef struct SHInclude {

	CharString relativePath;       //Path relative to oiSH source's directory (e.g. ../Includes/myInclude.hlsli)

	U32 crc32c;                    //Content CRC32C. However, if it contains '\r' it's removed first!
	U32 padding;

} SHInclude;

TList(SHInclude);

void SHInclude_free(SHInclude *include, const Allocator *alloc);
void ListSHInclude_freeUnderlying(ListSHInclude *includes, const Allocator *alloc);

typedef union SHValue {        //Intermediate value (can be packed heavily according to type)

	U64 vu64[16];
	U32 vu32[16];
	U16 vu16[16];
	U8  vu8 [16];

	I64 vi64[16];
	I32 vi32[16];
	I16 vi16[16];
	I8  vi8 [16];

	F64 vf64[16];
	F32 vf32[16];

} SHValue;

Bool SHValue_stringify(const SHValue *value, TypeId typeId, const Allocator *alloc, CharString *val, Error *e_rr);
Bool SHValue_stringifyHLSL(
	const SHValue *value,
	TypeId typeId,
	EHLSLStringifyFlags flags,
	const Allocator *alloc,
	CharString *val,
	Error *e_rr
);

typedef struct SHFile {

	ListSHBinaryInfo binaries;
	ListSHEntry entries;
	ListSHInclude includes;

	ESHSettingsFlags flags;

	U32 compilerVersion;        //OxC3 compiler version

	U32 sourceHash;             //CRC32C of sources

} SHFile;

TList(SHFile);

Bool SHFile_create(
	ESHSettingsFlags flags,
	U32 compilerVersion,
	U32 sourceHash,
	const Allocator *alloc,
	SHFile *shFile,
	Error *e_rr
);

void SHFile_free(SHFile *shFile, const Allocator *alloc);

Bool SHFile_addBinary(
	SHFile *shFile,
	SHBinaryInfo *binaries,                //Moves binaries
	const Allocator *alloc,
	Error *e_rr
);

//Moves entry->name and binaryIds
Bool SHFile_addEntrypoint(SHFile *shFile, SHEntry *entry, const Allocator *alloc, Error *e_rr);

//Moves include->name
Bool SHFile_addInclude(SHFile *shFile, SHInclude *include, const Allocator *alloc, Error *e_rr);

Bool SHFile_write(StreamRef *streamRef, U64 *offset, const SHFile *shFile, const Allocator *alloc, Error *e_rr);
Bool SHFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SHFile *shFile, Error *e_rr);

Bool SHFile_combine(const SHFile *a, const SHFile *b, const Allocator *alloc, SHFile *combined, Error *e_rr);

void SHFile_print(const SHFile *a, Bool isVerbose, const Allocator *alloc);

//Runtime check: true only if every binary carries compiled code for at least one backend.
//A reflection-only oiSH (e.g. produced by a reflect pass) returns false; creating a pipeline from it is invalid.
Bool SHFile_isComplete(const SHFile *a);

#ifdef __cplusplus
	}
#endif
