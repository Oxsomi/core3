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
#include "types/base/c8.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"

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

	//Which binary types this oiSH actually serialized (aggregated over every SHBinaryInfo).
	//With per-entrypoint [[oxc::binary(...)]] this can differ from what was requested, so it's worth surfacing at a glance.

	Log_debug(alloc, ELogOptions_None, "Serialized binary types:");

	Bool anyBinaryType = false;

	for (U8 j = 0; j < EGfxBinaryType_Count; ++j) {

		Bool has = false;

		for (U64 i = 0; i < a->binaries.length && !has; ++i)
			if (Buffer_length(a->binaries.ptr[i].binaries[j]))
				has = true;

		if (has) {
			Log_debug(alloc, ELogOptions_None, " %s", EGfxBinaryType_names[j]);
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

		for(U64 j = 0; j < EGfxBinaryType_Count; ++j)
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

		for(U64 i = 0; i < EGfxBinaryType_Count; ++i)
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

				//HLSL takes the bits, since a decimal is rounded twice, once printed and once read back by the compiler.
				//A half without 16 bit types is a float in HLSL, of the same value.

				if (flavor == EStringifyFlavor_HLSL) {

					switch (stride) {

						default:

							if (flags & EHLSLStringifyFlags_Has16Bit) {
								gotoIfError3(clean, CharString_format(
									alloc, &tmp, e_rr, "asfloat16((uint16_t) 0x%04" PRIX16 ")", value->vu16[*counter]
								));
							}

							else {
								const F32 f = F16_castF32(value->vu16[*counter]);
								U32 bits = 0;
								Buffer_memcpy(Buffer_createRef(&bits, sizeof(bits)), Buffer_createRefConst(&f, sizeof(f)));
								gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, "asfloat(0x%08" PRIX32 "u)", bits));
							}

							break;

						case EDataTypeStride_32:

							gotoIfError3(clean, CharString_format(
								alloc, &tmp, e_rr, "asfloat(0x%08" PRIX32 "u)", value->vu32[*counter]
							));

							break;

						case EDataTypeStride_64:

							gotoIfError3(clean, CharString_format(
								alloc, &tmp, e_rr, "asdouble(0x%08" PRIX32 "u, 0x%08" PRIX32 "u)",
								(U32) value->vu64[*counter], (U32) (value->vu64[*counter] >> 32)
							));

							break;
					}

					break;
				}

				//Printed with the fewest digits that read back to the same bits through CharString_parseDouble, the reader
				//an annotation and a uniform lookup (SHValue_parse) go through, so a lookup can take what is printed.
				//The most a type needs is its round trip count; past that the reader is what's inexact.

				F64 v;
				int maxDigits;

				switch (stride) {
					default:                    v = F16_castF64(value->vu16[*counter]);    maxDigits = 5;     break;
					case EDataTypeStride_32:    v = value->vf32[*counter];                 maxDigits = 9;     break;
					case EDataTypeStride_64:    v = value->vf64[*counter];                 maxDigits = 17;    break;
				}

				for (int digits = 1; digits <= maxDigits; ++digits) {

					CharString_free(&tmp, alloc);
					gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, "%.*g", digits, v));

					F64 back = 0;

					if (!CharString_parseDouble(tmp, &back))
						continue;

					U64 backBits = 0, bits = 0;

					switch (stride) {

						default:
							backBits = F64_castF16(back);
							bits = value->vu16[*counter];
							break;

						case EDataTypeStride_32: {
							const F32 f = F64_castF32(back);
							Buffer_memcpy(Buffer_createRef(&backBits, sizeof(U32)), Buffer_createRefConst(&f, sizeof(f)));
							bits = value->vu32[*counter];
							break;
						}

						case EDataTypeStride_64:
							Buffer_memcpy(Buffer_createRef(&backBits, sizeof(U64)), Buffer_createRefConst(&back, sizeof(back)));
							bits = value->vu64[*counter];
							break;
					}

					if (backBits == bits)
						break;
				}

				break;
			}

			case EDataType_Int:    {
				
				I64 vi;

				switch (stride) {
					default:                    vi = value->vi8[*counter];     break;
					case EDataTypeStride_16:    vi = value->vi16[*counter];    break;
					case EDataTypeStride_32:    vi = value->vi32[*counter];    break;
					case EDataTypeStride_64:    vi = value->vi64[*counter];    break;
				}

				if(vi < 0)
					gotoIfError3(clean, CharString_append(val, '-', alloc, e_rr));

				number.v = vi < 0 ? ((~(U64) vi) + 1) : (U64) vi;
				gotoIfError3(clean, CharString_createDec(&number, e_rr));
				break;
			}

			case EDataType_UInt: {

				U64 v;

				switch (stride) {
					default:                    v = value->vu8[*counter];     break;
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

Bool SHValue_parse(CharString str, TypeId typeId, SHValue *value, Error *e_rr) {

	Bool s_uccess = true;

	if(!value)
		retError(clean, Error_nullPointer(2, "SHValue_parse()::value is required"));

	*value = (SHValue) { 0 };

	if(ETypeId_isObject(typeId) || ETypeId_getWidth(typeId) != 1 || ETypeId_getHeight(typeId) != 1)
		retError(clean, Error_invalidParameter(1, 0, "SHValue_parse()::typeId has to be a scalar"));

	const EDataType type = ETypeId_getDataType(typeId);
	const EDataTypeStride stride = ETypeId_getDataTypeStride(typeId);
	const U8 bits = (U8) (8 << stride);

	CharString digits = str;
	Bool neg = false;

	switch(type) {

		case EDataType_Bool:

			if(CharString_equalsCStringSensitive(&str, "true"))
				value->vu8[0] = 1;

			else if(!CharString_equalsCStringSensitive(&str, "false"))
				retError(clean, Error_invalidParameter(0, 0, "SHValue_parse()::str has to be true or false for a B1"));

			break;

		case EDataType_UInt:
		case EDataType_Int: {

			if(CharString_length(digits) && (digits.ptr[0] == '+' || digits.ptr[0] == '-')) {
				neg = digits.ptr[0] == '-';
				digits = CharString_createRefSizedConst(digits.ptr + 1, CharString_length(digits) - 1, false);
			}

			U64 v = 0;

			if(!CharString_length(digits) || !C8_isDec(digits.ptr[0]) || !CharString_parseU64(digits, &v))
				retError(clean, Error_invalidParameter(0, 0, "SHValue_parse()::str has to be a decimal integer"));

			if(type == EDataType_UInt) {

				if(neg && v)
					retError(clean, Error_invalidParameter(0, 0, "SHValue_parse()::str is negative for an unsigned type"));

				if(bits != 64 && (v >> bits))
					retError(clean, Error_outOfBounds(
						0, v, ((U64) 1 << bits) - 1, "SHValue_parse()::str doesn't fit the type"
					));

				switch(stride) {
					case EDataTypeStride_8:     value->vu8[0]  = (U8)  v;     break;
					case EDataTypeStride_16:    value->vu16[0] = (U16) v;     break;
					case EDataTypeStride_32:    value->vu32[0] = (U32) v;     break;
					default:                    value->vu64[0] = v;           break;
				}

				break;
			}

			//The magnitude may be one past the largest positive value, for the smallest negative one.

			const U64 limit = (U64) 1 << (bits - 1);

			if(neg ? v > limit : v >= limit)
				retError(clean, Error_outOfBounds(0, v, limit, "SHValue_parse()::str doesn't fit the type"));

			const I64 vi = neg ? (I64) (~v + 1) : (I64) v;

			switch(stride) {
				case EDataTypeStride_8:     value->vi8[0]  = (I8)  vi;     break;
				case EDataTypeStride_16:    value->vi16[0] = (I16) vi;     break;
				case EDataTypeStride_32:    value->vi32[0] = (I32) vi;     break;
				default:                    value->vi64[0] = vi;           break;
			}

			break;
		}

		case EDataType_Float: {

			F64 v = 0;

			if(!CharString_parseDouble(str, &v))
				retError(clean, Error_invalidParameter(0, 0, "SHValue_parse()::str has to be a number"));

			Bool finite = false;

			switch(stride) {

				case EDataTypeStride_16:
					value->vu16[0] = F64_castF16(v);
					finite = EFloatType_isFinite(EFloatType_F16, value->vu16[0]);
					break;

				case EDataTypeStride_32:
					value->vf32[0] = F64_castF32(v);
					finite = EFloatType_isFinite(EFloatType_F32, value->vu32[0]);
					break;

				default:
					value->vf64[0] = v;
					finite = EFloatType_isFinite(EFloatType_F64, value->vu64[0]);
					break;
			}

			if(!finite)
				retError(clean, Error_invalidParameter(0, 0, "SHValue_parse()::str isn't finite in the type"));

			break;
		}

		default:
			retError(clean, Error_invalidParameter(1, 0, "SHValue_parse()::typeId has no value syntax"));
	}

clean:
	return s_uccess;
}

Bool SHValue_equals(const SHValue *a, const SHValue *b, TypeId typeId) {

	if(!a || !b)
		return a == b;

	if(ETypeId_getDataType(typeId) == EDataType_Bool)
		return (a->vu8[0] & 1) == (b->vu8[0] & 1);

	const U64 len = ETypeId_getBytes(typeId);
	return Buffer_eq(Buffer_createRefConst(a, len), Buffer_createRefConst(b, len));
}
