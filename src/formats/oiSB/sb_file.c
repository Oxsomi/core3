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

#ifdef ALLOW_SB_OXC3_PLATFORMS
	#include "platforms/ext/listx_impl.h"
#else
	#include "types/container/list_impl.h"
#endif

#include "types/container/log.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiDL/dl_file.h"

TListImpl(SBFile);

#ifdef ALLOW_SB_OXC3_PLATFORMS

	#include "platforms/platform.h"

	Bool SBFile_createx(
		ESBSettingsFlags flags,
		U32 bufferSize,
		SBFile *sbFile,
		Error *e_rr
	) {
		return SBFile_create(flags, bufferSize, Platform_instance->alloc, sbFile, e_rr);
	}

	Bool SBFile_createCopyx(SBFile src, SBFile *dst, Error *e_rr) {
		return SBFile_createCopy(src, Platform_instance->alloc, dst, e_rr);
	}

	void SBFile_freex(SBFile *shFile) {
		SBFile_free(shFile, Platform_instance->alloc);
	}

	Bool SBFile_addStructx(SBFile *sbFile, CharString *name, SBStruct sbStruct, Error *e_rr) {
		return SBFile_addStruct(sbFile, name, sbStruct, Platform_instance->alloc, e_rr);
	}

	Bool SBFile_addVariableAsTypex(
		SBFile *sbFile,
		CharString *name,
		U32 offset,
		U16 parentId,		//root = U16_MAX
		ESBType type,
		ESBVarFlag flags,
		ListU32 *arrays,
		Error *e_rr
	) {
		return SBFile_addVariableAsType(
			sbFile,
			name,
			offset,
			parentId,
			type,
			flags,
			arrays,
			Platform_instance->alloc,
			e_rr
		);
	}

	Bool SBFile_addVariableAsStructx(
		SBFile *sbFile,
		CharString *name,
		U32 offset,
		U16 parentId,		//root = U16_MAX
		U16 structId,
		ESBVarFlag flags,
		ListU32 *arrays,
		Error *e_rr
	) {
		return SBFile_addVariableAsStruct(
			sbFile,
			name,
			offset,
			parentId,
			structId,
			flags,
			arrays,
			Platform_instance->alloc,
			e_rr
		);
	}

	Bool SBFile_writex(SBFile sbFile, Buffer *result, Error *e_rr) {
		return SBFile_write(sbFile, Platform_instance->alloc, result, e_rr);
	}

	Bool SBFile_readx(Buffer file, Bool isSubFile, SBFile *sbFile, Error *e_rr) {
		return SBFile_read(file, isSubFile, Platform_instance->alloc, sbFile, e_rr);
	}

	void SBFile_printx(SBFile sbFile, U64 indenting, U16 parent, Bool isRecursive) {
		SBFile_print(sbFile, indenting, parent, isRecursive, Platform_instance->alloc);
	}

	void ListSBFile_freeUnderlyingx(ListSBFile *files) {
		ListSBFile_freeUnderlying(files, Platform_instance->alloc);
	}

	Bool SBFile_combinex(SBFile a, SBFile b, SBFile *combined, Error *e_rr) {
		return SBFile_combine(a, b, Platform_instance->alloc, combined, e_rr);
	}

#endif

Bool SBFile_create(
	ESBSettingsFlags flags,
	U32 bufferSize,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;

	if(!sbFile)
		retError(clean, Error_nullPointer(0, "SBFile_create()::sbFile is required"))

	if(sbFile->vars.length)
		retError(clean, Error_invalidOperation(0, "SBFile_create()::sbFile isn't empty, might indicate memleak"))

	if(flags & ESBSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "SBFile_create()::flags contained unsupported flag"))

	if(!bufferSize)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_create()::bufferSize is required"))

	gotoIfError2(clean, ListSBStruct_reserve(&sbFile->structs, 16, alloc))
	gotoIfError2(clean, ListSBVar_reserve(&sbFile->vars, 16, alloc))
	gotoIfError2(clean, ListCharString_reserve(&sbFile->structNames, 16, alloc))
	gotoIfError2(clean, ListCharString_reserve(&sbFile->varNames, 16, alloc))
	gotoIfError2(clean, ListListU32_reserve(&sbFile->arrays, 16, alloc))

	sbFile->flags = flags;
	sbFile->bufferSize = bufferSize;

	const void *flagsPtr = &sbFile->flags;
	sbFile->hash = Buffer_fnv1a64Single(*(const U64*)flagsPtr, Buffer_fnv1a64Offset);

clean:
	return s_uccess;
}

Bool SBFile_createCopy(
	SBFile src,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!sbFile)
		retError(clean, Error_nullPointer(2, "SBFile_createCopy()::sbFile is required"))

	if(sbFile->vars.ptr)
		retError(clean, Error_invalidParameter(2, 0, "SBFile_createCopy()::sbFile is filled, indicates memleak"))

	allocated = true;
	gotoIfError2(clean, ListSBStruct_createCopy(src.structs, alloc, &sbFile->structs))
	gotoIfError2(clean, ListSBVar_createCopy(src.vars, alloc, &sbFile->vars))
	gotoIfError2(clean, ListCharString_createCopyUnderlying(src.structNames, alloc, &sbFile->structNames))
	gotoIfError2(clean, ListCharString_createCopyUnderlying(src.varNames, alloc, &sbFile->varNames))
	gotoIfError3(clean, ListListU32_createCopyUnderlying(src.arrays, alloc, &sbFile->arrays, e_rr))

	sbFile->bufferSize = src.bufferSize;
	sbFile->flags = src.flags;
	sbFile->hash = src.hash;

clean:

	if(allocated && !s_uccess)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

void SBFile_free(SBFile *sbFile, Allocator alloc) {

	if(!sbFile)
		return;

	ListSBStruct_free(&sbFile->structs, alloc);
	ListSBVar_free(&sbFile->vars, alloc);
	ListCharString_freeUnderlying(&sbFile->structNames, alloc);
	ListCharString_freeUnderlying(&sbFile->varNames, alloc);
	ListListU32_freeUnderlying(&sbFile->arrays, alloc);
}

static const U8 SBHeader_V1_2 = 2;

void ListSBFile_freeUnderlying(ListSBFile *files, Allocator alloc) {

	if(!files)
		return;

	for(U64 i = 0; i < files->length; ++i)
		SBFile_free(&files->ptrNonConst[i], alloc);

	ListSBFile_free(files, alloc);
}

void SBFile_print(SBFile sbFile, U64 indenting, U16 parent, Bool isRecursive, Allocator alloc) {

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SBFile_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	for (U64 i = 0; i < sbFile.vars.length; ++i) {

		SBVar var = sbFile.vars.ptr[i];

		if(var.parentId != parent)
			continue;

		CharString varName = sbFile.varNames.ptr[i];
		Bool isArray = var.arrayIndex != U16_MAX;

		CharString typeName =
			var.structId == U16_MAX ? CharString_createRefCStrConst(ESBType_name((ESBType)var.type)) :
			sbFile.structNames.ptr[var.structId];

		SBStruct strct = (SBStruct) { 0 };

		if(var.structId != U16_MAX)
			strct = sbFile.structs.ptr[var.structId];

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

			ListU32 array = sbFile.arrays.ptr[var.arrayIndex];

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
