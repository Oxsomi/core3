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

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	#include "platforms/ext/listx_impl.h"
#else
	#include "types/container/list_impl.h"
#endif

#include "formats/oiSH/sh_file.h"
#include "types/container/log.h"
#include "types/base/type_id.h"
#include "types/math/flp.h"

TListImpl(SHFile);

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	
	#include "platforms/platform.h"

	Bool SHFile_createx(ESHSettingsFlags flags, U32 compilerVersion, U32 sourceHash, SHFile *shFile, Error *e_rr) {
		return SHFile_create(flags, compilerVersion, sourceHash, Platform_instance->alloc, shFile, e_rr);
	}

	void SHFile_freex(SHFile *shFile) {
		SHFile_free(shFile, Platform_instance->alloc);
	}

	void SHFile_printx(SHFile a) {
		SHFile_print(a, Platform_instance->alloc);
	}

	Bool SHFile_addBinaryx(SHFile *shFile, SHBinaryInfo *binaries, Error *e_rr) {
		return SHFile_addBinary(shFile, binaries, Platform_instance->alloc, e_rr);
	}

	Bool SHFile_addEntrypointx(SHFile *shFile, SHEntry *entry, Error *e_rr) {
		return SHFile_addEntrypoint(shFile, entry, Platform_instance->alloc, e_rr);
	}

	Bool SHFile_addIncludex(SHFile *shFile, SHInclude *include, Error *e_rr) {
		return SHFile_addInclude(shFile, include, Platform_instance->alloc, e_rr);
	}

	Bool SHFile_writex(SHFile shFile, Buffer *result, Error *e_rr) {
		return SHFile_write(shFile, Platform_instance->alloc, result, e_rr);
	}

	Bool SHFile_readx(Buffer file, Bool isSubFile, SHFile *shFile, Error *e_rr) {
		return SHFile_read(file, isSubFile, Platform_instance->alloc, shFile, e_rr);
	}

	Bool SHFile_combinex(SHFile a, SHFile b, SHFile *combined, Error *e_rr) {
		return SHFile_combine(a, b, Platform_instance->alloc, combined, e_rr);
	}

	void ListSHInclude_freeUnderlyingx(ListSHInclude *includes) {
		ListSHInclude_freeUnderlying(includes, Platform_instance->alloc);
	}

	void SHInclude_freex(SHInclude *include) {
		SHInclude_free(include, Platform_instance->alloc);
	}
#endif

void SHFile_print(SHFile a, Allocator alloc) {

	Log_debugLn(
		alloc,
		"Source hash: %"PRIx32" and OxC3 version: %"PRIu32".%"PRIu32".%"PRIu32,
		a.sourceHash,
		OXC3_GET_MAJOR(a.compilerVersion),
		OXC3_GET_MINOR(a.compilerVersion),
		OXC3_GET_PATCH(a.compilerVersion)
	);

	for(U64 i = 0; i < a.binaries.length; ++i) {
		Log_debugLn(alloc, "SHBinaryInfo at %"PRIu64, i);
		SHBinaryInfo_print(a.binaries.ptr[i], alloc);
	}

	for(U64 i = 0; i < a.entries.length; ++i) {
		Log_debugLn(alloc, "SHEntry at %"PRIu64, i);
		SHEntry_print(a.entries.ptr[i], alloc);
	}

	for(U64 i = 0; i < a.includes.length; ++i) {
		SHInclude incl = a.includes.ptr[i];
		CharString inc = incl.relativePath;
		Log_debugLn(alloc, "SHInclude at %"PRIu64" (%.*s %"PRIx32")", i, (int) CharString_length(inc), inc.ptr, incl.crc32c);
	}
}

Bool SHFile_create(
	ESHSettingsFlags flags,
	U32 compilerVersion,
	U32 sourceHash,
	Allocator alloc,
	SHFile *shFile,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!shFile)
		retError(clean, Error_nullPointer(0, "SHFile_create()::shFile is required"))

	if(shFile->entries.ptr)
		retError(clean, Error_invalidOperation(0, "SHFile_create()::shFile isn't empty, might indicate memleak"))

	if(flags & ESHSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 3, "SHFile_create()::flags contained unsupported flag"))

	gotoIfError2(clean, ListSHEntry_reserve(&shFile->entries, 8, alloc))
	gotoIfError2(clean, ListSHBinaryInfo_reserve(&shFile->binaries, 4, alloc))
	gotoIfError2(clean, ListSHInclude_reserve(&shFile->includes, 16, alloc))

	shFile->flags = flags;
	shFile->compilerVersion = compilerVersion;
	shFile->sourceHash = sourceHash;

clean:
	return s_uccess;
}

void SHFile_free(SHFile *shFile, Allocator alloc) {

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

		for(U64 i = 0; i < ESHBinaryType_Count; ++i)
			Buffer_free(&binary->binaries[i], alloc);
	}

	ListSHEntry_freeUnderlying(&shFile->entries, alloc);
	ListSHBinaryInfo_free(&shFile->binaries, alloc);
	ListSHInclude_freeUnderlying(&shFile->includes, alloc);

	*shFile = (SHFile) { 0 };
}

Bool SHValue_stringifyOne(
	const SHValue *value,
	ETypeId typeId,
	U64 *counter,
	Allocator alloc,
	CharString *val,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	if(!value)
		retError(clean, Error_nullPointer(0, "SHValue_stringify() value is missing"))

	if(*counter) {
		gotoIfError2(clean, CharString_append(val, ',', alloc))
		gotoIfError2(clean, CharString_append(val, ' ', alloc))
	}

	U32 w = ETypeId_getWidth(typeId);
	U32 h = ETypeId_getWidth(typeId);
	EDataType type = ETypeId_getDataType(typeId);
	EDataTypeStride stride = ETypeId_getDataTypeStride(typeId);

	//Scalar;
	//true
	//1
	//1.5
	//-32
	//^

	if (w == 1 && h == 1) {

		switch (type) {

			default: {
				const C8 *v = (value->vu64[0] >> *counter) & 1 ? "true" : "false";
				gotoIfError2(clean, CharString_appendString(val, CharString_createRefCStrConst(v), alloc))
				break;
			}

			case EDataType_Float: {
			
				F64 v;

				switch (stride) {
					default:					v = F16_castF64(value->vu16[*counter]);		break;
					case EDataTypeStride_32:	v = value->vf32[*counter];					break;
					case EDataTypeStride_64:	v = value->vf64[*counter];					break;
				}

				gotoIfError2(clean, CharString_format(alloc, &tmp, "%g", v))
				break;
			}

			case EDataType_Int:	{
				
				I64 vi;

				switch (stride) {
					default:					vi = value->vi8[*counter];	break;
					case EDataTypeStride_16:	vi = value->vi16[*counter];	break;
					case EDataTypeStride_32:	vi = value->vi32[*counter];	break;
					case EDataTypeStride_64:	vi = value->vi64[*counter];	break;
				}

				U64 v = vi < 0 ? ((~(U64)vi) + 1) : (U64)vi;

				if(vi < 0)
					gotoIfError2(clean, CharString_append(val, '-', alloc))

				gotoIfError2(clean, CharString_createDec(v, 0, alloc, &tmp))
				break;
			}

			case EDataType_UInt: {

				U64 v;

				switch (stride) {
					default:					v = value->vu8[*counter];	break;
					case EDataTypeStride_16:	v = value->vu16[*counter];	break;
					case EDataTypeStride_32:	v = value->vu32[*counter];	break;
					case EDataTypeStride_64:	v = value->vu64[*counter];	break;
				}

				gotoIfError2(clean, CharString_createDec(v, 0, alloc, &tmp))
				break;
			}
		}

		++*counter;
		goto clean;
	}

	//Vector
	//(1, 2, 3)
	//^

	if (h == 1) {

		ETypeId single = makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, stride, type);

		gotoIfError2(clean, CharString_append(val, '(', alloc))

		for(U64 i = 0; i < w; ++i)
			gotoIfError3(clean, SHValue_stringifyOne(value, single, counter, alloc, val, e_rr))
			
		gotoIfError2(clean, CharString_append(val, ')', alloc))
		goto clean;
	}

	//Matrix
	//((1, 2, 3), (4, 5, 6), (7, 8, 9))
	
	ETypeId vec = makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, stride, type);

	gotoIfError2(clean, CharString_append(val, '(', alloc))

	for(U64 i = 0; i < h; ++i)
		gotoIfError3(clean, SHValue_stringifyOne(value, vec, counter, alloc, val, e_rr))
			
	gotoIfError2(clean, CharString_append(val, ')', alloc))

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

Bool SHValue_stringify(const SHValue *value, ETypeId typeId, Allocator alloc, CharString *val, Error *e_rr) {

	Bool s_uccess = true;

	if(!val || CharString_length(*val))
		retError(clean, Error_invalidState(0, "SHValue_stringify()::val is required but should be empty"))

	U64 counter = 0;
	gotoIfError3(clean, SHValue_stringifyOne(value, typeId, &counter, alloc, val, e_rr))

clean:
	return s_uccess;
}
