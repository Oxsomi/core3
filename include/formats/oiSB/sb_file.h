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
	ESBSettingsFlags_None                = 0,
	ESBSettingsFlags_HideMagicNumber    = 1 << 0,        //Only valid if the oiSH can be 100% confidently detected otherwise
	ESBSettingsFlags_IsTightlyPacked    = 1 << 1,
	ESBSettingsFlags_CreateNoReserve    = 1 << 2,        //SBFile_create only: skips the reserve and leaves names for the caller
	ESBSettingsFlags_Invalid            = 0xFFFFFFFF << 3
} ESBSettingsFlags;

typedef struct SBFile {

	DLFile names;                //[0, structNames - 1], [structNames, structNames + varNames - 1]

	ListSBStruct structs;        //Indexed by struct id, owned by the file
	ListSBVar vars;              //Indexed by var id (SBVar::parentId), owned by the file
	ListListU32 arrays;          //Dimension lists (1 to 32 entries each) of SBVar::arrayDimOrArrayId, owned by the file

	ESBSettingsFlags flags;
	U32 bufferSize;              //Size of the described buffer in bytes, never 0

	U64 hash;                    //Appending to the SBFile will automatically refresh this

} SBFile;

TList(SBFile);

//Whether any member of the file is set, which means creating into it would leak.
//SBFile_create, SBFile_createCopy, SBFile_read and SBFile_combine refuse an output for which this is true.

Bool SBFile_isFilled(const SBFile *sbFile);

//The out SBFile has to be zeroed on entry (SBFile_isFilled is what is checked) and is zeroed again on failure.
//bufferSize is in bytes and non zero. Members have to be added in the order they're first seen (docs/file/oiSB.md).

Bool SBFile_create(
	ESBSettingsFlags flags,
	U32 bufferSize,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
);

//Deep copy including the hash. sbFile has to be zeroed (SBFile_isFilled is what is checked) and is zeroed again on failure.

Bool SBFile_createCopy(
	const SBFile *src,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
);

//Frees everything and zeroes the struct, so it can be created into again.

void SBFile_free(SBFile *sbFile, const Allocator *alloc);

//Internal hash steps shared by sb_variable.c and sb_combine.c (see docs/file/oiSB.md).

void SBStruct_applyHash(U64 *hashRes, SBStruct strct, CharString name, U64 index);
void SBVar_applyHash(U64 *hashRes, SBVar var, CharString name, U64 index);
void ListU32_applyHash(U64 *hashRes, ListU32 array, U64 index);

//Ids are indices: the added struct's id is structs.length and the added variable's id is vars.length, as read before the call.
//stride is in bytes and non zero. Names are at most 128 bytes.
//A failed add leaves the file and its hash as they were.
//name: a ref string is copied, an owned one is moved into the file (*name is emptied on success).
// After a failure free *name anyway, it is either still yours or already empty.

Bool SBFile_addStruct(SBFile *sbFile, CharString *name, SBStruct sbStruct, const Allocator *alloc, Error *e_rr);

//Variables: offset is in bytes from the start of the buffer, also for members of a struct (not relative to the parent).
//arrays is NULL for a non array, otherwise 1 to 32 non zero dimensions that multiply to at most U32_MAX bytes.
//A ref list is copied, an owned list is moved into the file or freed when an inline or identical entry is reused;
// either way *arrays is emptied on success. A failed call leaves *arrays as it was.
//parentId has to be a struct variable and the member has to fit in it, in every enclosing array and in the buffer.
//Names are unique per parent.

Bool SBFile_addVariableAsType(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,         //root = U16_MAX
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
	U16 parentId,         //root = U16_MAX
	U16 structId,
	ESBVarFlag flags,
	ListU32 *arrays,
	const Allocator *alloc,
	Error *e_rr
);

Bool SBFile_write(
	const SBFile *sbFile,
	const Allocator *alloc,
	StreamRef *streamRef, //Pass NULL to calculate length only (*offset)
	U64 *offset,          //Byte offset in the stream, 16-byte aligned. Advanced past the file on success only
	Error *e_rr
);

//offset is a 16-byte aligned byte offset in the stream and advances as data is consumed, also when the read fails.
//sbFile has to be zeroed (SBFile_isFilled is what is checked) and is zeroed again on failure.
//isSubFile: no magic number is expected and trailing data is left alone (the parent owns the stream);
// otherwise the file has to end exactly at the end of the stream.

Bool SBFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SBFile *sbFile, Error *e_rr);

//Logs stringified SBFile directly. parent is a var id (U16_MAX = root), indenting is a tab count below SHORTSTRING_LEN
// (otherwise an error is logged and nothing is printed).
//A variable that refers to a name, struct, array or parent that doesn't exist logs an error and is skipped.
void SBFile_print(const SBFile *sbFile, U64 indenting, U16 parent, Bool isRecursive, const Allocator *alloc);

//Whether two buffers are the same layout. The hash only prefilters: fnv1a's step is a multiply by an odd prime
//and so a bijection, which makes a colliding pair something a writer CONSTRUCTS rather than searches for, and
//every input to it came out of a file.

//Structs, variables (including use flags), arrays and names have to match in order. Only HideMagicNumber is ignored.
//NULL equals NULL and nothing else.

Bool SBFile_equals(const SBFile *a, const SBFile *b);

//a and b need the same bufferSize, flags (except HideMagicNumber) and variable and struct counts.
//Use flags are or'ed and a flat array can be unflattened to b's dimensions. combined has to be zeroed and is zeroed on failure.

Bool SBFile_combine(const SBFile *a, const SBFile *b, const Allocator *alloc, SBFile *combined, Error *e_rr);

//Frees every file and then the list itself
void ListSBFile_freeUnderlying(ListSBFile *files, const Allocator *alloc);

//File headers

//File spec (docs/file/oiSB.md)

typedef enum ESBVersion {
	ESBVersion_Undefined,
	ESBVersion_V0_1,           //Unsupported
	ESBVersion_V1_2            //Current
} ESBVersion;

typedef enum ESBFlag {
	ESBFlag_None               = 0,
	ESBFlag_IsTightlyPacked    = 1 << 0,
	ESBFlag_Unsupported        = 0xFFFFFFFF << 1
} ESBFlag;

typedef struct SBHeader {

	U8 version;                //major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
	U8 flags;                  //ESBFlag
	U16 arrays;

	U16 structs;
	U16 vars;

	U32 bufferSize;

} SBHeader;

#define SBHeader_MAGIC 0x4253696F

#ifdef __cplusplus
	}
#endif
