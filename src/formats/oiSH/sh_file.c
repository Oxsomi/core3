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

//formats/oiSH/sh_file.c

#include "types/container/list_impl.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/list_basic_types.h"
#include "types/container/log.h"
#include "types/math/flp.h"
#include "types/base/type_id.h"
#include "types/base/constants.h"

TListImpl(SHFile);

void SHFile_print(const SHFile *a, Bool isVerbose, const Allocator *alloc) {

	if (!a)
		return;

	Log_debugLn(
		alloc,
		"Source hash: %"PRIx32" and OxC3 version: %"PRIu32".%"PRIu32".%"PRIu32,
		a->sourceHash,
		OXC3_GET_MAJOR(a->compilerVersion),
		OXC3_GET_MINOR(a->compilerVersion),
		OXC3_GET_PATCH(a->compilerVersion)
	);

	//Which binary types this oiSH actually serialized (aggregated over every SHBinaryInfo). With per-entrypoint
	//[[oxc::binary(...)]] this can differ from what was requested, so it's worth surfacing at a glance.

	Log_debug(alloc, ELogOptions_None, "Serialized binary types:");

	Bool anyBinaryType = false;

	for (U8 j = 0; j < ESHBinaryType_Count; ++j) {

		Bool has = false;

		for (U64 i = 0; i < a->binaries.length && !has; ++i)
			if (Buffer_length(a->binaries.ptr[i].binaries[j]))
				has = true;

		if (has) {
			Log_debug(alloc, ELogOptions_None, " %s", ESHBinaryType_names[j]);
			anyBinaryType = true;
		}
	}

	Log_debugLn(alloc, "%s", anyBinaryType ? "" : " (none)");

	for(U64 i = 0; i < a->binaries.length; ++i) {
		Log_debugLn(alloc, "SHBinaryInfo at %"PRIu64, i);
		SHBinaryInfo_print(&a->binaries.ptr[i], isVerbose, alloc);
	}

	for(U64 i = 0; i < a->entries.length; ++i) {
		Log_debugLn(alloc, "SHEntry at %"PRIu64, i);
		SHEntry_print(&a->entries.ptr[i], true, alloc);
	}

	for(U64 i = 0; i < a->includes.length; ++i) {
		SHInclude incl = a->includes.ptr[i];
		CharString inc = incl.relativePath;
		Log_debugLn(alloc, "SHInclude at %"PRIu64" (%.*s %"PRIx32")", i, (int) CharString_length(inc), inc.ptr, incl.crc32c);
	}
}

Bool SHFile_create(
	ESHSettingsFlags flags,
	U32 compilerVersion,
	U32 sourceHash,
	const Allocator *alloc,
	SHFile *shFile,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!shFile)
		retError(clean, Error_nullPointer(0, "SHFile_create()::shFile is required"));

	if(shFile->entries.ptr)
		retError(clean, Error_invalidOperation(0, "SHFile_create()::shFile isn't empty, might indicate memleak"));

	if(flags & ESHSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 3, "SHFile_create()::flags contained unsupported flag"));

	gotoIfError3(clean, ListSHEntry_reserve(&shFile->entries, 8, alloc, e_rr));
	gotoIfError3(clean, ListSHBinaryInfo_reserve(&shFile->binaries, 4, alloc, e_rr));
	gotoIfError3(clean, ListSHInclude_reserve(&shFile->includes, 16, alloc, e_rr));

	shFile->flags = flags;
	shFile->compilerVersion = compilerVersion;
	shFile->sourceHash = sourceHash;

clean:
	return s_uccess;
}

Bool SHFile_isComplete(const SHFile *shFile) {

	if(!shFile || !shFile->binaries.length)
		return false;

	for(U64 i = 0; i < shFile->binaries.length; ++i) {

		const SHBinaryInfo *binary = &shFile->binaries.ptr[i];
		Bool hasBinary = false;

		for(U64 j = 0; j < ESHBinaryType_Count; ++j)
			if(Buffer_length(binary->binaries[j])) {
				hasBinary = true;
				break;
			}

		if(!hasBinary)
			return false;
	}

	return true;
}

void SHFile_free(SHFile *shFile, const Allocator *alloc) {

	if(!shFile || !shFile->entries.ptr)
		return;

	for(U64 i = 0; i < shFile->entries.length; ++i) {
		SHEntry *entry = &shFile->entries.ptrNonConst[i];
		CharString_free(&entry->name, alloc);
		ListCharString_freeUnderlying(&entry->semanticNames, alloc);
		ListU16_free(&entry->binaryIds, alloc);
	}

	for(U64 j = 0; j < shFile->binaries.length; ++j) {

		SHBinaryInfo *binary = &shFile->binaries.ptrNonConst[j];

		ListSHRegisterRuntime_freeUnderlying(&binary->registers, alloc);
		CharString_free(&binary->identifier.entrypoint, alloc);
		ListCharString_freeUnderlying(&binary->identifier.defines, alloc);
		ListSHUniformRuntime_freeUnderlying(&binary->identifier.uniforms, alloc);
		ListU8_free(&binary->identifier.uniformData, alloc);

		for(U64 i = 0; i < ESHBinaryType_Count; ++i)
			Buffer_free(&binary->binaries[i], alloc);
	}

	ListSHEntry_freeUnderlying(&shFile->entries, alloc);
	ListSHBinaryInfo_free(&shFile->binaries, alloc);
	ListSHInclude_freeUnderlying(&shFile->includes, alloc);

	*shFile = (SHFile) { 0 };
}

typedef enum EStringifyFlavor {
	EStringifyFlavor_OxC3,
	EStringifyFlavor_HLSL
} EStringifyFlavor;

static Bool SHValue_stringifyOne(
	const SHValue *value,
	TypeId typeId,
	U64 *counter,
	U8 *localCounter,
	EStringifyFlavor flavor,
	EHLSLStringifyFlags flags,
	const Allocator *alloc,
	CharString *val,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	if (!value)
		retError(clean, Error_nullPointer(0, "SHValue_stringify() value is missing"));

	if(*localCounter) {
		gotoIfError3(clean, CharString_append(val, ',', alloc, e_rr));
		gotoIfError3(clean, CharString_append(val, ' ', alloc, e_rr));
	}

	U32 w = ETypeId_getWidth(typeId);
	U32 h = ETypeId_getHeight(typeId);
	EDataType type = ETypeId_getDataType(typeId);
	EDataTypeStride stride = ETypeId_getDataTypeStride(typeId);

	//Scalar;
	//true
	//1
	//1.5
	//-32
	//^

	if (w == 1 && h == 1) {

		CharStringCreateNumber number = (CharStringCreateNumber){
			.leadingZeros = 0,
			.allocator = alloc,
			.result = &tmp
		};

		switch (type) {

			default: {
				const C8 *v = (value->vu64[0] >> *counter) & 1 ? "true" : "false";
				CharString vstr = CharString_createRefCStrConst(v);
				gotoIfError3(clean, CharString_appendString(val, &vstr, alloc, e_rr));
				break;
			}

			case EDataType_Float: {
			
				F64 v;

				switch (stride) {
					default:                    v = F16_castF64(value->vu16[*counter]);        break;
					case EDataTypeStride_32:    v = value->vf32[*counter];                    break;
					case EDataTypeStride_64:    v = value->vf64[*counter];                    break;
				}

				gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, "%g", v));
				break;
			}

			case EDataType_Int:    {
				
				I64 vi;

				switch (stride) {
					default:                    vi = value->vi8[*counter];    break;
					case EDataTypeStride_16:    vi = value->vi16[*counter];    break;
					case EDataTypeStride_32:    vi = value->vi32[*counter];    break;
					case EDataTypeStride_64:    vi = value->vi64[*counter];    break;
				}

				if(vi < 0)
					gotoIfError3(clean, CharString_append(val, '-', alloc, e_rr));

				number.v = vi < 0 ? ((~(U64)vi) + 1) : (U64)vi;
				gotoIfError3(clean, CharString_createDec(&number, e_rr));
				break;
			}

			case EDataType_UInt: {

				U64 v;

				switch (stride) {
					default:                    v = value->vu8[*counter];    break;
					case EDataTypeStride_16:    v = value->vu16[*counter];    break;
					case EDataTypeStride_32:    v = value->vu32[*counter];    break;
					case EDataTypeStride_64:    v = value->vu64[*counter];    break;
				}

				number.v = v;
				gotoIfError3(clean, CharString_createDec(&number, e_rr));
				break;
			}
		}

		gotoIfError3(clean, CharString_appendString(val, &tmp, alloc, e_rr));
		++*counter;
		++*localCounter;
		goto clean;
	}

	//Vector
	//(1, 2, 3)
	//^

	if (h == 1) {

		if(flavor == EStringifyFlavor_HLSL) {

			//float16_t3(1, 2, 3)
			//^

			gotoIfError3(clean, CharString_createFromETypeIdHLSL(typeId, flags, alloc, &tmp, e_rr));
			gotoIfError3(clean, CharString_appendString(val, &tmp, alloc, e_rr));
			CharString_free(&tmp, alloc);
		}

		TypeId single = makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, stride, type);

		gotoIfError3(clean, CharString_append(val, '(', alloc, e_rr));
		U8 localCounteri = 0;

		for(U64 i = 0; i < w; ++i)
			gotoIfError3(clean, SHValue_stringifyOne(value, single, counter, &localCounteri, flavor, flags, alloc, val, e_rr));
			
		gotoIfError3(clean, CharString_append(val, ')', alloc, e_rr));
		++*localCounter;
		goto clean;
	}

	//Matrix
	//((1, 2, 3), (4, 5, 6), (7, 8, 9))

	if(flavor == EStringifyFlavor_HLSL) {

		//float16_t3x3(float16_t3(1, 2, 3), float16_t3(4, 5, 6), float16_t3(7, 8, 9))
		//^

		gotoIfError3(clean, CharString_createFromETypeIdHLSL(typeId, flags, alloc, &tmp, e_rr));
		gotoIfError3(clean, CharString_appendString(val, &tmp, alloc, e_rr));
		CharString_free(&tmp, alloc);
	}
	
	TypeId vec = makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, stride, type);

	gotoIfError3(clean, CharString_append(val, '(', alloc, e_rr));

	U8 localCounteri = 0;

	for(U64 i = 0; i < h; ++i)
		gotoIfError3(clean, SHValue_stringifyOne(value, vec, counter, &localCounteri, flavor, flags, alloc, val, e_rr));
			
	gotoIfError3(clean, CharString_append(val, ')', alloc, e_rr));

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

static inline Bool SHValue_stringifyWithFlavor(
	const SHValue *value,
	TypeId typeId,
	EStringifyFlavor flavor,
	EHLSLStringifyFlags flags,
	const Allocator *alloc,
	CharString *val,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!val || CharString_length(*val))
		retError(clean, Error_invalidState(0, "SHValue_stringify()::val is required but should be empty"));

	U64 counter = 0;
	U8 localCounter = 0;
	gotoIfError3(clean, SHValue_stringifyOne(value, typeId, &counter, &localCounter, flavor, flags, alloc, val, e_rr));

clean:
	return s_uccess;
}

Bool SHValue_stringify(const SHValue *value, TypeId typeId, const Allocator *alloc, CharString *val, Error *e_rr) {
	return SHValue_stringifyWithFlavor(value, typeId, EStringifyFlavor_OxC3, 0, alloc, val, e_rr);
}

Bool SHValue_stringifyHLSL(
	const SHValue *value,
	TypeId typeId,
	EHLSLStringifyFlags flags,
	const Allocator *alloc,
	CharString *val,
	Error *e_rr
) {
	return SHValue_stringifyWithFlavor(value, typeId, EStringifyFlavor_HLSL, flags, alloc, val, e_rr);
}
