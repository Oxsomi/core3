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

//formats/oiSB/sb_file.c

#include "formats/oiSB/sb_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/list_impl.h"
#include "types/container/log.h"
#include "types/container/list_basic_types.h"
#include "types/base/constants.h"

TListImpl(SBFile);

Bool SBFile_create(
	ESBSettingsFlags flags,
	U32 bufferSize,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;
	Bool allocated = false;

	if (!sbFile)
		retError(clean, Error_nullPointer(0, "SBFile_create()::sbFile is required"));

	if(sbFile->vars.length)
		retError(clean, Error_invalidOperation(0, "SBFile_create()::sbFile isn't empty, might indicate memleak"));

	Bool avoidReserve = flags & ESBSettingsFlags_CreateNoReserve;
	flags &= ~ESBSettingsFlags_CreateNoReserve;

	if(flags & ESBSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "SBFile_create()::flags contained unsupported flag"));

	if(!bufferSize)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_create()::bufferSize is required"));

	if (!avoidReserve) {

		DLSettings settings = (DLSettings){
			.compressionType = EXXCompressionType_None,
			.encryptionType = EXXEncryptionType_None,
			.dataType = EDLDataType_String,
			.flags = EDLSettingsFlags_HideMagicNumber
		};

		gotoIfError3(clean, DLFile_create(&settings, KIBI, alloc, &sbFile->names, e_rr));
		allocated = true;

		gotoIfError3(clean, DLFile_reserve(&sbFile->names, 8, alloc, e_rr));
		gotoIfError3(clean, ListSBStruct_reserve(&sbFile->structs, 4, alloc, e_rr));
		gotoIfError3(clean, ListSBVar_reserve(&sbFile->vars, 4, alloc, e_rr));
		gotoIfError3(clean, ListListU32_reserve(&sbFile->arrays, 2, alloc, e_rr));
	}

	ESBSettingsFlags flagsReal = flags;
	sbFile->flags = flags &~ (ESBSettingsFlags_HideMagicNumber | ESBSettingsFlags_HasSharedLayout);
	sbFile->bufferSize = bufferSize;

	//flags and bufferSize are a U32-aligned pair, so hashing them as one in-place U64 can be a misaligned load.

	U64 flagsAndSize;
	Buffer_memcpy(
		Buffer_createRef(&flagsAndSize, sizeof(flagsAndSize)),
		Buffer_createRefConst(&sbFile->flags, sizeof(U64))
	);

	sbFile->hash = Buffer_fnv1a64Single(flagsAndSize, Buffer_fnv1a64Offset);

	//We don't want HideMagicNumber to influence the hash, nor HasSharedLayout: the claim is validation
	// metadata, not layout identity, so identical layouts hash equal whether or not one carries it
	// (SBFile_combine also grants it after the hash exists).

	sbFile->flags = flagsReal;

clean:

	if (allocated && !s_uccess)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

Bool SBFile_createCopy(
	const SBFile *src,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if (!sbFile || !src)
		retError(clean, Error_nullPointer(!src ? 0 : 2, "SBFile_createCopy()::sbFile and src are required"));

	if(sbFile->bufferSize)
		retError(clean, Error_invalidParameter(2, 0, "SBFile_createCopy()::sbFile is filled, indicates memleak"));

	gotoIfError3(clean, ListSBStruct_createCopy(src->structs, alloc, &sbFile->structs, e_rr));
	allocated = true;
	gotoIfError3(clean, ListSBVar_createCopy(src->vars, alloc, &sbFile->vars, e_rr));
	gotoIfError3(clean, DLFile_createCopy(&src->names, alloc, &sbFile->names, e_rr));
	gotoIfError3(clean, ListListU32_createCopyUnderlying(&src->arrays, alloc, &sbFile->arrays, e_rr));

	sbFile->bufferSize = src->bufferSize;
	sbFile->flags = src->flags;
	sbFile->hash = src->hash;

clean:

	if(allocated && !s_uccess)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

void SBFile_free(SBFile *sbFile, const Allocator *alloc) {

	if(!sbFile)
		return;

	ListSBStruct_free(&sbFile->structs, alloc);
	ListSBVar_free(&sbFile->vars, alloc);
	DLFile_free(&sbFile->names, alloc);
	ListListU32_freeUnderlying(&sbFile->arrays, alloc);
}

void ListSBFile_freeUnderlying(ListSBFile *files, const Allocator *alloc) {

	if(!files)
		return;

	for(U64 i = 0; i < files->length; ++i)
		SBFile_free(&files->ptrNonConst[i], alloc);

	ListSBFile_free(files, alloc);
}

void SBFile_print(const SBFile *sbFile, U64 indenting, U16 parent, Bool isRecursive, const Allocator *alloc) {

	if(!sbFile) {
		Log_debugLn(alloc, "SBFile_print() invalid sbFile");
		return;
	}

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SBFile_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	for (U64 i = 0; i < sbFile->vars.length; ++i) {

		SBVar var = sbFile->vars.ptr[i];

		if(var.parentId != parent)
			continue;

		CharString varName = sbFile->names.entryStrings.ptr[sbFile->structs.length + i];
		Bool isArray = var.arrayDimOrArrayId;

		CharString typeName =
			var.structId == U16_MAX ? CharString_createRefCStrConst(ESBType_name((ESBType)var.type)) :
			sbFile->names.entryStrings.ptr[var.structId];

		SBStruct strct = (SBStruct) { 0 };

		if(var.structId != U16_MAX)
			strct = sbFile->structs.ptr[var.structId];

		Bool usedSPIRV = var.flags & ESBVarFlag_IsUsedVarSPIRV;
		Bool usedDXIL = var.flags & ESBVarFlag_IsUsedVarDXIL;

		const C8 *used = usedSPIRV && usedDXIL ? "SPIRV & DXIL: Used" : (
			usedSPIRV ? "SPIRV: Used" : (usedDXIL ? "DXIL: Used" : "SPIRV & DXIL: Unused")
		);

		Log_debug(
			alloc,
			!isArray ? ELogOptions_NewLine : ELogOptions_None,
			!strct.stride ? "%s0x%08"PRIx32": %.*s (%s): %.*s" :
			"%s0x%08"PRIx32": %.*s (%s): %.*s (Stride: %"PRIu32")",
			indent,
			var.offset,
			(int) CharString_length(varName),
			varName.ptr,
			used,
			(int) CharString_length(typeName),
			typeName.ptr,
			strct.stride
		);

		if (isArray) {

			ListU32 array = (ListU32) { 0 };
			U32 v = var.arrayDimOrArrayId;
			
			if (var.arrayDimOrArrayId >> 15)
				array = sbFile->arrays.ptr[var.arrayDimOrArrayId & (U16)I16_MAX];

			else ListU32_createRefConst(&v, 1, &array, NULL);

			for(U64 j = 0; j < array.length; ++j)
				Log_debug(
					alloc,
					j + 1 == array.length ? ELogOptions_NewLine : ELogOptions_None,
					"[%"PRIu32"]", array.ptr[j]
				);
		}

		if(isRecursive && strct.stride)
			SBFile_print(sbFile, indenting + 1, (U16) i, true, alloc);
	}
}

//The widest scalar under a variable: a leaf's own component size, a struct's widest member.
//Children always carry higher ids than their parent, but the tree is walked directly since layouts
// stay small enough that clarity beats a scratch buffer.

static U8 SBFile_scalarAlignment(const SBFile *sbFile, U16 varId) {

	const SBVar var = sbFile->vars.ptr[varId];

	if (var.structId == U16_MAX)
		return (U8)(1 << ESBType_getStride((ESBType) var.type));

	U8 align = 1;

	for (U64 i = 0; i < sbFile->vars.length; ++i)
		if (sbFile->vars.ptr[i].parentId == varId) {

			U8 childAlign = SBFile_scalarAlignment(sbFile, (U16) i);

			if (childAlign > align)
				align = childAlign;
		}

	return align;
}

Bool SBFile_validateSharedLayout(const SBFile *sbFile, Error *e_rr) {

	Bool s_uccess = true;

	if(!sbFile)
		retError(clean, Error_nullPointer(0, "SBFile_validateSharedLayout()::sbFile is required"));

	const Bool cbufferRows = !(sbFile->flags & ESBSettingsFlags_IsTightlyPacked);

	for (U64 i = 0; i < sbFile->vars.length; ++i) {

		const SBVar var = sbFile->vars.ptr[i];
		const U8 align = SBFile_scalarAlignment(sbFile, (U16) i);

		//The 16-byte rows SBFile_addVariableAsType and SBFile_addVariableAsStruct enforce while adding

		if (cbufferRows) {

			if(var.structId != U16_MAX && (var.offset & 15))
				retError(clean, Error_invalidState(
					2, "SBFile_validateSharedLayout() a cbuffer struct doesn't start a 16-byte row"
				));

			const U32 size = var.structId == U16_MAX ? ESBType_getSize((ESBType) var.type, false) : 0;

			if(size && ((var.offset + size - 1) >> 4) != (var.offset >> 4) && (var.offset & 15))
				retError(clean, Error_invalidState(
					3, "SBFile_validateSharedLayout() a cbuffer variable straddles a 16-byte row"
				));
		}

		if(var.offset & (align - 1))
			retError(clean, Error_invalidState(
				0,
				"SBFile_validateSharedLayout() a variable's offset isn't aligned to its scalar "
				"alignment, so one backend can't express this layout"
			));

		if(var.structId != U16_MAX && (sbFile->structs.ptr[var.structId].stride & (align - 1)))
			retError(clean, Error_invalidState(
				1,
				"SBFile_validateSharedLayout() a struct's stride isn't aligned to its scalar "
				"alignment, so one backend can't express this layout"
			));
	}

clean:
	return s_uccess;
}
