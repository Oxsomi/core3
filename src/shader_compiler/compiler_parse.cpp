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

//shader_compiler/compiler_parse.cpp

#include "types/container/list_impl.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"
#include "types/base/allocator.h"
#include "types/base/c8.h"
#include "types/base/mathi.h"
#include "types/math/flp.h"
#include "types/base/constants.h"
#include "platforms/file.h"
#include "types/container/file_base.h"
#include "platforms/platform.h"
#include "shader_compiler/compiler.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#include <Unknwn.h>
#endif

#define ENABLE_DXC_STATIC_LINKING
#include "dxcompiler/dxcapi.h"
#include "dxcompiler/dxcreflect.h"
#include <exception>
#include "compiler_private.hpp"

class IncludeHandler;

typedef struct CompilerInterfaces {
	IDxcUtils *utils;
	IDxcCompiler3 *compiler;
	IncludeHandler *includeHandler;
	IHLSLReflector *reflector;
} CompilerInterfaces;

Bool Compiler_validateGroupSize(U32 threads[3], Error *e_rr) {

	Bool s_uccess = true;
	U64 totalGroup = 0;

	if(!threads)
		retError(clean, Error_nullPointer(0, "Compiler_validateGroupSize() invalid threads"));

	totalGroup = (U64)threads[0] * threads[1] * threads[2];

	if(!totalGroup)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() needs group size for compute"));

	if(totalGroup > 512)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() group count out of bounds (512)"));

	if(U32_max(threads[0], threads[1]) > 512)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() group count x or y out of bounds (512)"));

	if(threads[2] > 64)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() group count z out of bounds (64)"));

clean:
	return s_uccess;
}

typedef union TempInOutput {
	U64 aU64[2];
	U8 a[16];
} TempInOutput;

Bool Compiler_findEntry(const ListSHEntryRuntime *entry, CharString name, SHEntryRuntime **ptr, Error *e_rr) {

	for(U64 i = 0; i < entry->length; ++i)
		if (CharString_equalsStringSensitive(&entry->ptr[i].entry.name, &name)) {
			*ptr = entry->ptrNonConst + i;
			return true;
		}

	if(e_rr)
		*e_rr = Error_notFound(0, 1, "Compiler_findEntry()::name not found");

	return false;
}

Bool Compiler_finalizeEntrypoint(
	U32 localSize[3],
	U8 payloadSize,
	U8 intersectSize,
	U16 waveSize,
	ESBType inputs[16],
	ESBType outputs[16],
	U8 uniqueInputSemantics,
	ListCharString *uniqueSemantics,
	U8 inputSemantics[16],
	U8 outputSemantics[16],
	const CharString *entryName,
	SpinLock *lock,
	const ListSHEntryRuntime *entries,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	SHEntryRuntime *entry = NULL;
	Bool didInit = false;

	if(!localSize || !inputs || !outputs || !uniqueSemantics || !inputSemantics || !outputSemantics)
		retError(clean, Error_nullPointer(
			0, "Compiler_finalizeEntrypoint() localSize, inputs, outputs, unique/output/inputSemantics are required"
		));

	if(payloadSize > 128)
		retError(clean, Error_outOfBounds(0, payloadSize, 128, "Compiler_finalizeEntrypoint() payload out of bounds"));

	if(intersectSize > 32)
		retError(clean, Error_outOfBounds(0, intersectSize, 32, "Compiler_finalizeEntrypoint() attribute out of bounds"));

	if(localSize[0] || localSize[1] || localSize[2])
		gotoIfError3(clean, Compiler_validateGroupSize(localSize, e_rr));

	TempInOutput input, output;
	TempInOutput inputSemantic, outputSemantic;

	for(U8 i = 0; i < 16; ++i) {
		input.a[i] = (U8) inputs[i];
		output.a[i] = (U8) outputs[i];
		inputSemantic.a[i] = (U8) inputSemantics[i];
		outputSemantic.a[i] = (U8) outputSemantics[i];
	}

	gotoIfError3(clean, Compiler_findEntry(entries, *entryName, &entry, e_rr));

	if(lock) {
		acq = SpinLock_lock(lock, 1 * SECOND);
		if(acq < ELockAcquire_Success)
			retError(clean, Error_invalidState(0, "Compiler_finalizeEntrypoint() couldn't acquire spin lock"));
	}

	didInit = false;

	if (!(entry->isInitializedFlags & 1)) {

		didInit = true;

		//Store payloadSize, intersectionSize, localSize, inputs, outputs

		entry->entry.groupX = (U16) localSize[0];
		entry->entry.groupY = (U16) localSize[1];
		entry->entry.groupZ = (U16) localSize[2];

		entry->entry.payloadSize = payloadSize;
		entry->entry.intersectionSize = intersectSize;
		entry->entry.waveSize = waveSize;

		entry->entry.inputsU64[0] = input.aU64[0];
		entry->entry.inputsU64[1] = input.aU64[1];

		entry->entry.outputsU64[0] = output.aU64[0];
		entry->entry.outputsU64[1] = output.aU64[1];

		entry->entry.uniqueInputSemantics = uniqueInputSemantics;

		entry->entry.inputSemanticNamesU64[0] = inputSemantic.aU64[0];
		entry->entry.inputSemanticNamesU64[1] = inputSemantic.aU64[1];

		entry->entry.outputSemanticNamesU64[0] = outputSemantic.aU64[0];
		entry->entry.outputSemanticNamesU64[1] = outputSemantic.aU64[1];

		gotoIfError3(clean, ListCharString_move(uniqueSemantics, alloc, &entry->entry.semanticNames, e_rr));

		entry->isInitializedFlags |= 1;
	}

	//Compare to ensure we have the exact same properties

	else if(
		entry->entry.groupX != localSize[0] ||
		entry->entry.groupY != localSize[1] ||
		entry->entry.groupZ != localSize[2] ||
		entry->entry.payloadSize != payloadSize ||
		entry->entry.intersectionSize != intersectSize ||
		entry->entry.waveSize != waveSize ||
		entry->entry.inputsU64[0] != input.aU64[0] ||
		entry->entry.inputsU64[1] != input.aU64[1] ||
		entry->entry.outputsU64[0] != output.aU64[0] ||
		entry->entry.outputsU64[1] != output.aU64[1] ||
		entry->entry.uniqueInputSemantics != uniqueInputSemantics ||
		entry->entry.inputSemanticNamesU64[0] != inputSemantic.aU64[0] ||
		entry->entry.inputSemanticNamesU64[1] != inputSemantic.aU64[1] ||
		entry->entry.outputSemanticNamesU64[0] != outputSemantic.aU64[0] ||
		entry->entry.outputSemanticNamesU64[1] != outputSemantic.aU64[1] ||
		entry->entry.semanticNames.length != uniqueSemantics->length
	) {
		retError(clean, Error_invalidState(
			0,
			"Compiler_finalizeEntrypoint() had two mismatching inputs from reflection:\n"
			"numthreads, payloadSize, intersectSize, waveSize, inputs or outputs"
		));
	}

	//More thorough compare with semantics

	else {

		//TODO: Semantics could be ordered differently, might still be okay to merge?

		for(U64 i = 0; i < entry->entry.semanticNames.length; ++i)
			if(!CharString_equalsStringSensitive(&entry->entry.semanticNames.ptr[i], &uniqueSemantics->ptr[i]))
				retError(clean, Error_invalidState(
					0,
					"Compiler_finalizeEntrypoint() had two mismatching semantic names"
				));
	}

clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	if(!didInit && s_uccess)        //Avoid memleak
		ListCharString_freeUnderlying(uniqueSemantics, alloc);

	return s_uccess;
}

U16 Compiler_minFeatureSetStage(ESHPipelineStage stage, U16 waveSizeType) {

	U16 minVersion = OISH_SHADER_MODEL(6, 5);

	if(stage == ESHPipelineStage_WorkgraphExt)
		minVersion = OISH_SHADER_MODEL(6, 8);

	if(waveSizeType == 1)
		minVersion = OISH_SHADER_MODEL(6, 6);

	if(waveSizeType == 2)
		minVersion = OISH_SHADER_MODEL(6, 8);

	return minVersion;
}

U16 Compiler_minFeatureSetExtension(ESHExtension ext) {

	U16 minVersion = OISH_SHADER_MODEL(6, 5);

	//DescriptorHeap = SM6.6 dynamic resources on DXIL (ResourceDescriptorHeap/SamplerDescriptorHeap).
	if(ext & (ESHExtension_AtomicI64 | ESHExtension_ComputeDeriv | ESHExtension_PAQ | ESHExtension_DescriptorHeap))
		minVersion = U16_max(OISH_SHADER_MODEL(6, 6), minVersion);

	if(ext & ESHExtension_WriteMSTexture)
		minVersion = U16_max(OISH_SHADER_MODEL(6, 7), minVersion);

	//SM6.9 native ray features: SER (dx::HitObject reorder) and OMM (RayQuery opacity-micromap flags).
	if(ext & (ESHExtension_RayReorder | ESHExtension_RayMicromapOpacity))
		minVersion = U16_max(OISH_SHADER_MODEL(6, 9), minVersion);

	//SM6.10 features: cooperative vectors/matrix and ray triangle vertex position fetch.
	//Neither is detectable from DXIL,
	// so requiring the model here is what forces a compatible shader model on the DXIL path.

	ESHExtension sm10 = (ESHExtension) (
		ESHExtension_CoopVec | ESHExtension_CoopMat | ESHExtension_CoopFP8 | ESHExtension_CoopVecTraining |
		ESHExtension_RayTriPosition
	);

	if(ext & sm10)
		minVersion = U16_max(OISH_SHADER_MODEL(6, 10), minVersion);

	return minVersion;
}

//Simplified ComPtr to be cross platform
template<typename T>
struct OxComPtr {

	T *t;

	OxComPtr() : t(nullptr) {}
	OxComPtr(T *t): t(t) {}
	~OxComPtr() { if (t) t->Release(); t = nullptr; }

	operator T*() { return t; }
	T **operator&() { return &t; }
	T *operator->() { return t; }
};

Bool Compiler_parseValue(
	SHValue *value,
	U64 &dstOff,
	TypeId typeId,
	const C8 *&str,
	Error *e_rr
) {

	EDataType type = ETypeId_getDataType(typeId);
	EDataTypeStride typeStride = ETypeId_getDataTypeStride(typeId);
	U32 w = ETypeId_getWidth(typeId);
	U32 h = ETypeId_getHeight(typeId);
	TypeId vecType = (TypeId) ETypeId_Undefined;
	C8 next = '\0';

	Bool s_uccess = true;

	// 0
	// true
	//^^^^^

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseValue() ended unexpectedly"));

	next = *str;

	//Value
	//true
	//0
	//-3
	//1.5
	//^

	if (w == 1 && h == 1) {

		switch (type) {

			case EDataType_Bool:

				//true
				//^
				if (next == 't') {

					if(str[1] != 'r' || str[2] != 'u' || str[3] != 'e')
						retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected Bool/B1 'true'"));

					str += 4;
					value->vu64[0] |= (U64)1 << dstOff;
				}

				//false
				//^
				else if (next == 'f') {

					if (str[1] != 'a' || str[2] != 'l' || str[3] != 's' || str[4] != 'e')
						retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected Bool/B1 'false'"));

					str += 5;
				}

				else retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected Bool/B1 'true' or 'false'"));

				++dstOff;
				break;

			case EDataType_Float: {

				//Greedy grab (+-.eEfF0-9)

				const C8 *fStart = str;

				while (C8_isDec(next = *str) ||
					next == 'E' || next == 'e' ||
					next == 'F' || next == 'f' ||
					next == '-' || next == '+' ||
					next == '.'
				)
					++str;

				if(str == fStart)
					retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected a float value"));

				CharString val = CharString_createRefSizedConst(fStart, str - fStart, false);
				F64 res = 0;
				if(!CharString_parseDouble(val, &res))
					retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected a float value"));

				switch(typeStride) {

					case EDataTypeStride_16: {

						F16 v = F64_castF16(res);

						if(!EFloatType_isFinite(EFloatType_F16, v))
							retError(clean, Error_invalidParameter(
								0, 0, "Compiler_parseValue() passed float value not representable as F16"
							));

						value->vu16[dstOff] = v;
						break;
					}

					case EDataTypeStride_32: {

						F32 v = F64_castF32(res);

						//Reinterpret the float bits via a void* launder (strict-aliasing-safe).
						const void *vptr = &v;

						if (!EFloatType_isFinite(EFloatType_F32, *(const U32*)vptr))
							retError(clean, Error_invalidParameter(
								0, 0, "Compiler_parseValue() passed float value not representable as F32"
							));

						value->vf32[dstOff] = v;
						break;
					}

					default: {

						const void *resptr = &res;

						if (!EFloatType_isFinite(EFloatType_F64, *(const U64*)resptr))
							retError(clean, Error_invalidParameter(
								0, 0, "Compiler_parseValue() passed float value not representable as F64"
							));

						value->vf64[dstOff] = res;
						break;
					}
				}

				++dstOff;
				break;
			}

			case EDataType_UInt: {

				if (next == '+')
					++str;

				const C8 *strStart = str;

				while (C8_isDec(*str))
					++str;

				if(str == strStart)
					retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected a uint value"));

				CharString val = CharString_createRefSizedConst(strStart, str - strStart, false);
				U64 res = 0;
				if (!CharString_parseU64(val, &res))
					retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected a uint value"));

				U8 bits = ETypeId_getDataTypeBytes(typeId) << 3;

				if(bits != 64 && (res >> bits))
					retError(clean, Error_outOfBounds(
						0, res, ((U64)1 << bits) - 1, "Compiler_parseValue() parsed uint couldn't fit in the target bits"
					));

				switch(bits) {
					default:    value->vu8[dstOff] = (U8) res;        break;
					case 16:    value->vu16[dstOff] = (U16) res;    break;
					case 32:    value->vu32[dstOff] = (U32) res;    break;
					case 64:    value->vu64[dstOff] = res;            break;
				}

				++dstOff;
				break;
			}

			case EDataType_Int: {

				Bool neg = next == '-';

				if (neg || next == '+')
					++str;

				const C8 *strStart = str;

				while (C8_isDec(*str))
					++str;

				if (str == strStart)
					retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected an int value"));

				CharString val = CharString_createRefSizedConst(strStart, str - strStart, false);
				U64 ures = 0;
				if (!CharString_parseU64(val, &ures))
					retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() expected an int value"));

				//Easier than allowing I64_MIN

				if(ures >> 63)
					retError(clean, Error_invalidParameter(0, 0, "Compiler_parseValue() overflow on int value"));

				I64 res = neg ? -(I64)ures : (I64)ures;

				U8 bits = ETypeId_getDataTypeBytes(typeId) << 3;

				U64 miBits = (U64)1 << (bits - 1);
				I64 mi = -(I64)miBits;
				I64 ma = (I64)((U64)miBits - 1);

				if(bits != 64 && (neg ? (res < mi) : (res > ma)))
					retError(clean, Error_invalidParameter(
						0, 0, "Compiler_parseValue() parsed int couldn't fit in the target bits"
					));

				switch(bits) {
					default:    value->vi8[dstOff]  = (I8) res;        break;
					case 16:    value->vi16[dstOff] = (I16) res;    break;
					case 32:    value->vi32[dstOff] = (I32) res;    break;
					case 64:    value->vi64[dstOff] = res;            break;
				}

				++dstOff;
				break;
			}

			default:
				retError(clean, Error_invalidState(0, "Compiler_parseValue() is an invalid type"));
		}

		goto clean;
	}

	//Vector
	//(x, y, z, w)
	//^

	if (h == 1) {

		if (next != '(')
			retError(clean, Error_invalidState(0, "Compiler_parseValue() expected ("));

		++str;

		TypeId singleType = TypeId(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, typeStride, type));

		for (U64 i = 0; i < w; ++i) {

			//(x, y, z, w)
			// ^  ^  ^  ^

			gotoIfError3(clean, Compiler_parseValue(value, dstOff, singleType, str, e_rr));
				
			//(x, y, z, w)
			//  ^  ^  ^

			if (i != w - 1) {

				if (!Compiler_skipWhitespace(str))
					retError(clean, Error_invalidState(0, "Compiler_parseValue() ended unexpectedly"));

				if(str[0] != ',')
					retError(clean, Error_invalidState(0, "Compiler_parseValue() expected ,"));

				++str;
			}
		}

		//(x, y, z, w)
		//           ^
			
		gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));
		goto clean;
	}

	//Matrix
	//((0, 1), (2, 3), (4, 5))
	//^
	
	vecType = TypeId(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, typeStride, type));

	if (next != '(')
		retError(clean, Error_invalidState(0, "Compiler_parseValue() expected ("));

	++str;

	for (U64 i = 0; i < h; ++i) {

		//((0, 1), (2, 3), (4, 5))
		// ^       ^       ^

		gotoIfError3(clean, Compiler_parseValue(value, dstOff, vecType, str, e_rr));

		//((0, 1), (2, 3), (4, 5))
		//       ^       ^

		if (i != w - 1) {

			if (!Compiler_skipWhitespace(str))
				retError(clean, Error_invalidState(0, "Compiler_parseValue() ended unexpectedly"));

			if(str[0] != ',')
				retError(clean, Error_invalidState(0, "Compiler_parseValue() expected ,"));

			++str;
		}
	}

	//((0, 1), (2, 3), (4, 5))
	//                       ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_registerUniform(
	SHEntryRuntime &entry,
	const C8 *&str,
	const Allocator *alloc,
	Error *e_rr
) {

	const C8 *idenStart = nullptr;

	CharString uniformType = CharString_createNull();
	CharString uniformName = CharString_createNull();
	CharString tmp = CharString_createNull();

	SHValue value = SHValue{ { 0 } };
	U64 dstOff = 0;
	U64 valLen = 0;
	U64 uniformDatLen = entry.uniformData.length;

	TypeId typeId = (TypeId) ETypeId_Undefined;

	Bool didInit = entry.isInitializedFlags & 2;
	Bool contains = false;

	Bool s_uccess = true;

	//type name = value;
	//^

	gotoIfError3(clean, Compiler_consumeIdentifier(str, idenStart, e_rr));

	uniformType = CharString_createRefSizedConst(idenStart, str - idenStart, false);
	typeId = ETypeId_parse(uniformType);

	if (typeId == ETypeId_Undefined || typeId == (TypeId) ETypeId_C8)
		retError(clean, Error_invalidState(
			0,
			"Compiler_registerUniform() invalid syntax, expected type = ((U/I/F)(8/16/32/64)/B)"
		));

	valLen = ETypeId_getBytes(typeId);

	//In the future we could add support, but would require adding multiple spec constants, quite annoying.

	if(ETypeId_getWidth(typeId) > 1 || ETypeId_getHeight(typeId) > 1)
		retError(clean, Error_invalidState(
			0,
			"Compiler_registerUniform() Vectors and matrices are unsupported, spirv doesn't support non scalar spec constants"
		));

	//type name = value;
	//     ^

	gotoIfError3(clean, Compiler_consumeIdentifier(str, idenStart, e_rr));

	uniformName = CharString_createRefSizedConst(idenStart, str - idenStart, false);

	for (U64 i = 0; i < CharString_length(uniformName); ++i) {

		C8 c = uniformName.ptr[i];
		Bool isDec = C8_isDec(c);

		if (!i && isDec)
			retError(clean, Error_alreadyDefined(0, "Compiler_registerUniform() can't start with a number"));

		if (!isDec && !C8_isAlpha(c) && c != '_')
			retError(clean, Error_alreadyDefined(0, "Compiler_registerUniform() name must be [A-Za-z_]+[0-9A-Za-z_]*"));
	}

	//type name = value;
	//         ^^

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_consumeIdentifier() ended unexpectedly"));

	if (str[0] != '=')
		retError(clean, Error_invalidState(0, "Compiler_registerUniform() invalid syntax, expected 'type name = value'"));

	++str;

	//type name = value;
	//              ^

	gotoIfError3(clean, Compiler_parseValue(&value, dstOff, typeId, str, e_rr));

	//Validate if uniform already exists

	for(U64 i = 0; i < entry.uniforms.length; ++i)
		if (CharString_equalsStringSensitive(&entry.uniforms.ptr[i].name, &uniformName)) {
			contains = true;
			break;
		}
	
	if (!didInit && contains) {
		retError(clean, Error_invalidState(0, "Compiler_registerUniform() uniform already present"));
	}

	else if(didInit && !contains)
		retError(clean, Error_invalidState(0, "Compiler_registerUniform() uniform not present in previous definition"));

	//Insert uniform

	if(!didInit) {

		gotoIfError3(clean, CharString_createCopy(uniformName, alloc, &tmp, e_rr));

		if(uniformDatLen + valLen >= U16_MAX)
			retError(clean, Error_invalidState(0, "Compiler_registerUniform() uniform buffer data is limited to 65535"));

		SHUniformRuntime uniform = SHUniformRuntime{
			.name = tmp,
			.typeIdShort = ETypeId_toShortId(typeId),
			.dataOffset = (U16)uniformDatLen
		};

		TypeId val = ETypeId_arr[uniform.typeIdShort];

		if(val != typeId)
			retError(clean, Error_invalidState(0, "Compiler_registerUniform() ETypeId_toShortId misfunctioning"));

		gotoIfError3(clean, ListSHUniformRuntime_pushBack(&entry.uniforms, uniform, alloc, e_rr));
		entry.uniformStride += (U16) valLen;
	}

	//Insert uniform data
	
	gotoIfError3(clean, ListU8_resize(&entry.uniformData, uniformDatLen + valLen, alloc, e_rr));

	Buffer_memcpy(
		Buffer_createRef(entry.uniformData.ptrNonConst + uniformDatLen, valLen),
		Buffer_createRefConst(&value, valLen)
	);

	tmp = CharString_createNull();        //Moved

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

Bool Compiler_parseUniformsAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	U32 uniformCount = 0;

	//oxc::uniforms ( B1 x = true, U32 y = 1 )
	//oxc::uniforms ( B1 x = true )
	//oxc::uniforms ( )
	//             ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::uniforms ( B1 x = true, U32 y = 1 )
	//oxc::uniforms ( B1 x = true )
	//oxc::uniforms ( )
	//               ^

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseUniformsAnnot() uniforms annotation ended unexpectedly"));

	//oxc::uniforms ( )
	//                ^

	if (str[0] == ')') {

		++str;

		//oxc::uniforms ( )
		//oxc::uniforms ( ) abc
		//                 ^^

		if (Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(
				0, "Compiler_parseUniformsAnnot() uniforms annotation had unknown syntax appended"
			));
		
		if ((entry.isInitializedFlags & 2) && entry.uniforms.length)
			retError(clean, Error_invalidState(
				0, "Compiler_parseUniformsAnnot() oxc::uniforms annotation mismatches uniform count"
			));

		entry.isInitializedFlags |= 2;
		goto clean;
	}

	//oxc::uniforms ( B1 x = true, U32 y = 1 )
	//oxc::uniforms ( B1 x = true )
	//                  ^

	gotoIfError3(clean, Compiler_registerUniform(entry, str, alloc, e_rr));
	uniformCount = 1;

	//[[oxc::uniforms(U32 x = 21, B1 y = false)]]
	//[[oxc::uniforms(U32 x = 21)]]
	//                            ^

	while(true) {

		//oxc::uniforms(U32 x = 21 , B1 y = false)
		//oxc::uniforms(U32 x = 21 )
		//                          ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseUniformsAnnot() uniforms annotation ended unexpectedly"));

		//oxc::uniforms(U32 x = 21, B1 y = false) abc
		//oxc::uniforms(U32 x = 21, B1 y = false)
		//oxc::uniforms(U32 x = 21)
		//                          ^^            ^^

		if (str[0] == ')') {

			++str;

			if (Compiler_skipWhitespace(str))
				retError(clean, Error_invalidState(
					0, "Compiler_parseUniformsAnnot() uniforms annotation had unknown syntax appended"
				));

			break;
		}

		//oxc::uniforms(U32 x = 21, B1 y = false)
		//                          ^

		if (*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseUniformsAnnot() uniforms annotation expected ,"));

		++str;

		//oxc::uniforms(U32 x = 21, B1 y = false)
		//                           ^

		gotoIfError3(clean, Compiler_registerUniform(entry, str, alloc, e_rr));
		++uniformCount;
	}

	if (uniformCount != entry.uniforms.length)
		retError(clean, Error_invalidParameter(
			0, 4, "Compiler_parseUniformsAnnot() oxc::uniforms mismatches in count with other oxc::uniforms"
		));

	entry.isInitializedFlags |= 2;

clean:
	return s_uccess;
}

Bool Compiler_parse(
	const Compiler *comp,
	const CompilerSettings *settings,
	const Allocator *alloc,
	CompileResult *result,
	Error *e_rr
) {

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ListU16 tmpWStr = ListU16{};
	#else
		ListU32 tmpWStr = ListU32{};
	#endif

	CharString tmp = CharString_createNull();
	Bool s_uccess = true;

	SHEntryRuntime runtimeEntry = SHEntryRuntime{};
	CompilerInterfaces *interfaces = nullptr;

	Bool hasErrors = false;
	OxComPtr<IDxcResult> hlslReflectRes;
	OxComPtr<IDxcBlob> reflectBinary;
	OxComPtr<IHLSLReflectionData> reflectionData;
	OxComPtr<IDxcBlobEncoding> source;
	D3D12_HLSL_REFLECTION_DESC reflDesc;
	HRESULT hr = S_OK;

	ListCharString stringsUTF8 = ListCharString{};        //One day, Microsoft will fix their stuff, I hope.
	Compiler_defineStrings;

	if (!result)
		retError(clean, Error_nullPointer(3, "Compiler_parse()::result is required"));

	result->type = ECompileResultType_SHEntryRuntime;
		
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		gotoIfError3(clean, CharString_toUTF16(settings->path, alloc, &tmpWStr, e_rr));
	#else
		gotoIfError3(clean, CharString_toUTF32(settings->path, alloc, &tmpWStr, e_rr));
	#endif

	interfaces = (CompilerInterfaces*)comp->interfaces;

	if (!interfaces->reflector || !interfaces->utils)
		retError(clean, Error_nullPointer(3, "Compiler_parse()::interfaces->reflector & utils are required"));

	if (!CharString_length(settings->string))
		retError(clean, Error_invalidParameter(1, 0, "Compiler_parse()::settings->string is required"));

	if(CharString_length(settings->string) >> 32)
		retError(clean, Error_invalidOperation(0, "Compiler_parse() string out of bounds"));

	//TODO: Handle preprocess?

	hr = interfaces->utils->CreateBlobFromPinned(
		settings->string.ptr, (U32) CharString_length(settings->string), DXC_CP_UTF8, &source
	);

	if (FAILED(hr))
		retError(clean, Error_invalidState(0, "Compiler_parse() source couldn't be wrapped into IDxcBlobEncoding"));

	gotoIfError3(clean, Compiler_setupIncludePaths(&stringsUTF8, settings, alloc, e_rr));

	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-reflect-functions", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-enable-16bit-types", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-enable-payload-qualifiers", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-T", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "lib_6_10", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC_PREPROCESS", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-HV", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "202x", alloc, e_rr));

	//if (settings->outputType == ESHBinaryType_SPIRV)
	//    gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-spirv", alloc, e_rr));

	//We will pretend that all extensions are enabled, this will avoid parser errors when extensions are used.

	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC_EXT_RAYTRACING", alloc, e_rr));

		//Format major, minor, patch and version

	static const C8 *formats[] = {
		"-D__OXC_MAJOR=%" PRIu64,
		"-D__OXC_MINOR=%" PRIu64,
		"-D__OXC_PATCH=%" PRIu64,
		"-D__OXC_VERSION=%" PRIu64,
	};

	static const U64 formatInts[] = {
		OXC3_MAJOR,
		OXC3_MINOR,
		OXC3_PATCH,
		OXC3_VERSION
	};

	for(U64 i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
		gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, formats[i], formatInts[i]));
		gotoIfError3(clean, ListCharString_pushBack(&stringsUTF8, tmp, alloc, e_rr));
		tmp = CharString_createNull();
	}

	//__OXC_EXT_<X> foreach extension

	for(U32 i = 0; i < ESHExtension_Count; ++i) {
		gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, "-D__OXC_EXT_%s", ESHExtension_defines[i]));
		gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tmp, alloc, e_rr));
		tmp = CharString_createNull();
	}

	//Reflect at the highest shader model (a SM6.10 library) so every feature is available to the reflection
	//parse, including the SM6.9/6.10 raytracing ops (SER / OMM / triangle position fetch). Reflection is
	//stage-agnostic here (the oxc::stage / [shader(...)] annotation drives the real stage), so a lib target
	//that can hold any stage is the right choice.
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-T", alloc, e_rr));
	gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "lib_6_10", alloc, e_rr));

	Compiler_convertToWString(stringsUTF8, clean);

	Compiler_resetIncludeHandler(interfaces->includeHandler);        //Ensure we don't reuse stale caches

	hr = interfaces->reflector->FromSource(
		source,
		(const wchar_t*)tmpWStr.ptr,
		(LPCWSTR*) strings.ptr, U32(strings.length),
		nullptr, 0,
		Compiler_getIncludeHandler(interfaces->includeHandler),
		&hlslReflectRes
	);

	if (hlslReflectRes) {

		OxComPtr<IDxcBlobUtf8> err;
		hr = hlslReflectRes->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&err), NULL);

		if (FAILED(hr))
			retError(clean, Error_invalidState(1, "Compiler_parse() fetch errors failed"));

		if (err && err->GetStringLength()) {
			CharString errs = CharString_createRefSizedConst(err->GetStringPointer(), err->GetStringLength(), false);
			gotoIfError3(clean, Compiler_parseErrors(errs, alloc, &result->compileErrors, &hasErrors, e_rr));
		}
	}

	if (hasErrors)
		goto clean;

	if (FAILED(hr) || !hlslReflectRes)
		retError(clean, Error_invalidState(0, "Compiler_parse() failed to call IHLSLReflector::FromSource"));

	hr = hlslReflectRes->GetResult(&reflectBinary);

	if (FAILED(hr) || !reflectBinary)
		retError(clean, Error_invalidState(0, "Compiler_parse() failed to get result binary"));

	hr = interfaces->reflector->FromBlob(reflectBinary, &reflectionData);

	if(FAILED(hr))
		retError(clean, Error_invalidState(0, "Compiler_parse() failed to deserialize result binary"));

	if (FAILED(hr = reflectionData->GetDesc(&reflDesc)))
		retError(clean, Error_invalidState(0, "Compiler_parse() failed to get reflection desc"));

	//Find all functions with at least an oxc or dxc annotation
	//We basically parse the annotations to generate ListSHEntryRuntime

	for (U32 i = 0; i < reflDesc.FunctionCount; ++i) {

		D3D12_HLSL_FUNCTION_DESC funcDesc;

		if (FAILED(hr = reflectionData->GetFunctionDesc(i, &funcDesc)))
			retError(clean, Error_invalidState(0, "Compiler_parse() failed to get reflection desc"));
		
		CharString funcName = CharString_createRefCStrConst(funcDesc.Name);

		D3D12_HLSL_NODE nodeDesc;

		if (FAILED(hr = reflectionData->GetNodeDesc(funcDesc.NodeId, &nodeDesc)))
			retError(clean, Error_invalidState(0, "Compiler_parse() failed to get node desc"));

		if (!nodeDesc.AnnotationCount)
			continue;

		//We found a potential entrypoint, check for shader or stage annotation

		runtimeEntry.entry.stage = ESHPipelineStage_Count;

		for (uint32_t j = 0; j < nodeDesc.AnnotationCount; ++j) {

			D3D12_HLSL_ANNOTATION annot;
			if (FAILED(hr = reflectionData->GetAnnotationByIndex(funcDesc.NodeId, j, &annot)))
				retError(clean, Error_invalidState(0, "Compiler_parse() failed to get node annot"));

			gotoIfError3(clean, Compiler_parseAnnot(annot, funcName, runtimeEntry, alloc, e_rr));
		}
		
		//If we didn't find a stage, but we did find annotations that match, we need to free them

		if (runtimeEntry.entry.stage == ESHPipelineStage_Count)
			SHEntryRuntime_free(&runtimeEntry, alloc);

		//Otherwise we found an entry

		else {

			U16 minVersion = Compiler_minFeatureSetStage(ESHPipelineStage(runtimeEntry.entry.stage), 0);

			//Ensure all shader versions are compatible with minimum featureset

			Bool containsValidVersion = minVersion == OISH_SHADER_MODEL(6, 5);

			for (U64 j = 0; j < runtimeEntry.shaderVersions.length; ++j)
				if (runtimeEntry.shaderVersions.ptr[j] >= minVersion) {
					containsValidVersion = true;
					break;
				}

			if (!containsValidVersion)
				retError(clean, Error_invalidState(
					0,
					"Compiler_parse() one of the shader models was incompatible with minimum shader featureset "
					"of WaveSize or stage"
				));

			//Find a shader version that has the requirements.
			//If there's no model available, then it should be ok.

			Bool isRt =
				runtimeEntry.entry.stage >= ESHPipelineStage_RtStartExt &&
				runtimeEntry.entry.stage <= ESHPipelineStage_RtEndExt;

			for (U64 j = 0; j < runtimeEntry.extensions.length; ++j) {

				if(
					runtimeEntry.entry.stage != ESHPipelineStage_RaygenExt &&
					(runtimeEntry.extensions.ptr[j] & ESHExtension_RayReorder)
				)
					retError(clean, Error_invalidState(
						0,
						"Compiler_parse() one of the non raygen stages uses RayReorder, which isn't supported"
					));

				if(!isRt && (runtimeEntry.extensions.ptr[j] & ESHExtension_PAQ))
					retError(clean, Error_invalidState(
						0,
						"Compiler_parse() one of the non raytracing stages uses PAQ, which isn't supported"
					));

				if(
					runtimeEntry.entry.stage != ESHPipelineStage_Compute &&
					runtimeEntry.entry.stage != ESHPipelineStage_MeshExt &&
					runtimeEntry.entry.stage != ESHPipelineStage_TaskExt &&
					(runtimeEntry.extensions.ptr[j] & ESHExtension_ComputeDeriv)
				)
					retError(clean, Error_invalidState(
						0,
						"Compiler_parse() one of the non compute/mesh/task stages uses ComputeDeriv, which isn't supported"
					));

				if(!runtimeEntry.shaderVersions.length)
					continue;

				U16 reqVersion = Compiler_minFeatureSetExtension(ESHExtension(runtimeEntry.extensions.ptr[j]));

				containsValidVersion = false;

				for (U64 k = 0; k < runtimeEntry.shaderVersions.length; ++k)
					if (runtimeEntry.shaderVersions.ptr[k] >= reqVersion) {
						containsValidVersion = true;
						break;
					}

				if(!containsValidVersion)
					retError(clean, Error_invalidState(
						0, "Compiler_parse() one of the shader extensions was incompatible with all shader models"
					));
			}

			//Validate groups with stage

			if(!runtimeEntry.vendorMask)
				runtimeEntry.vendorMask = U16_MAX;

			//Ready for push

			if(result->shEntriesRuntime.length + 1 >= U16_MAX)
				retError(clean, Error_invalidState(
					0, "Compiler_parse() found way too many SHEntries. Found U16_MAX!"
				));

			if(SHEntryRuntime_getCombinations(&runtimeEntry) + 1 >= U16_MAX)
				retError(clean, Error_invalidState(
					0, "Compiler_parse() found way too runtimeEntry combinations. Found U16_MAX!"
				));

			//Validate uniforms used with stage intrinsic instead of shader.

			if(runtimeEntry.uniforms.length && !runtimeEntry.isShaderAnnotation)
				retError(clean, Error_invalidState(
					0,
					"Compiler_parse() tried to enable [[oxc::uniforms(...)]] but [oxc::stage(...)] intrinsic is used. "
					"This is illegal, because it uses libraries to link with DXIL (and is suboptimal otherwise)."
				));

			//Validate if uniforms are the same as all other uniforms (excluding contents)

			if (result->shEntriesRuntime.length) {

				SHEntryRuntime first = result->shEntriesRuntime.ptr[0];

				if (first.uniforms.length != runtimeEntry.uniforms.length)
					retError(clean, Error_invalidState(
						0,
						"Compiler_parse() one of the entrypoints enabled uniforms but didn't have the same uniform count"
					));

				if (first.uniformStride != runtimeEntry.uniformStride)
					retError(clean, Error_invalidState(
						0, "Compiler_parse() one of the entrypoints enabled uniforms but didn't have the same types"
					));

				for (U64 j = 0; j < first.uniforms.length; ++j) {

					U64 k = 0;

					for (; k < runtimeEntry.uniforms.length; ++k)
						if (CharString_equalsStringSensitive(
							&runtimeEntry.uniforms.ptr[k].name,
							&first.uniforms.ptr[j].name
						))
							break;

					if (k == runtimeEntry.uniforms.length)
						retError(clean, Error_invalidState(
							0, "Compiler_parse() one of the required uniforms is missing between the next entry"
						));

					if (runtimeEntry.uniforms.ptr[k].typeIdShort != first.uniforms.ptr[j].typeIdShort)
						retError(clean, Error_invalidState(
							0, "Compiler_parse() one of the uniforms did match name but has a mismatching type"
						));

					if (runtimeEntry.uniforms.ptr[k].dataOffset != first.uniforms.ptr[j].dataOffset)
						retError(clean, Error_invalidState(
							0, "Compiler_parse() one of the uniforms did match order"
						));
				}
			}

			//Ensure we didn't define the same uniform multiple times

			U64 stride = runtimeEntry.uniformStride;

			for (U64 j = 1; j < U64_safeDiv(runtimeEntry.uniformData.length, stride); ++j)
				for (U64 k = 0; k < j; ++k)
					if(Buffer_eq(
						Buffer_createRefConst(runtimeEntry.uniformData.ptr + stride * k, stride),
						Buffer_createRefConst(runtimeEntry.uniformData.ptr + stride * j, stride)
					))
						retError(clean, Error_invalidState(
							0, "Compiler_parse() some of the uniform combinations are duplicated"
						));

			//Defines reference parsedLiterals, which are owned by the Parser.
			//The parser goes out of scope at the end of the function, so we have to copy it.
			//We won't do this for other params, because they're references to the input string
			//which will be alive after this function.

			if(runtimeEntry.defineNameValues.length) {
				ListCharString tmpArr = ListCharString{};
				gotoIfError3(clean, ListCharString_createCopyUnderlying(&runtimeEntry.defineNameValues, alloc, &tmpArr, e_rr));
				ListCharString_freeUnderlying(&runtimeEntry.defineNameValues, alloc);
				runtimeEntry.defineNameValues = tmpArr;
			}

			gotoIfError3(clean, ListSHEntryRuntime_pushBack(&result->shEntriesRuntime, runtimeEntry, alloc, e_rr));
			runtimeEntry = SHEntryRuntime{};
		}
	}

	result->type = ECompileResultType_SHEntryRuntime;
	result->isSuccess = true;

clean:

	if(!s_uccess && result)
		CompileResult_free(result, alloc);

	SHEntryRuntime_free(&runtimeEntry, alloc);
	
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ListU16_free(&tmpWStr, alloc);
	#else
		ListU32_free(&tmpWStr, alloc);
	#endif

	Compiler_freeStrings;
	CharString_free(&tmp, alloc);
	ListCharString_freeUnderlying(&stringsUTF8, alloc);
	return s_uccess;
}
