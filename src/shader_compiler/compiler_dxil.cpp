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

#include "platforms/ext/listx_impl.h"
#include "shader_compiler/compiler.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/container/file.h"
#include "types/math/math.h"
#include "formats/oiSB/sb_file.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Unknwn.h>
#endif

#define ENABLE_DXC_STATIC_LINKING
#include "dxcompiler/dxcapi.h"
#include "directx/d3d12shader.h"
#include <exception>

Bool DxilMapToESHExtension(U64 flags, ESHExtension *ext, ESHExtension *demotion, Error *e_rr) {

	Bool s_uccess = true;

	U64 defaultOps =
		D3D_SHADER_REQUIRES_TYPED_UAV_LOAD_ADDITIONAL_FORMATS |
		D3D_SHADER_REQUIRES_STENCIL_REF |
		D3D_SHADER_REQUIRES_EARLY_DEPTH_STENCIL |
		D3D_SHADER_REQUIRES_UAVS_AT_EVERY_STAGE |
		D3D_SHADER_REQUIRES_64_UAVS |
		D3D_SHADER_REQUIRES_LEVEL_9_COMPARISON_FILTERING;

	U64 extensionMap[] = {
		D3D_SHADER_REQUIRES_RAYTRACING_TIER_1_1,
		D3D_SHADER_REQUIRES_NATIVE_16BIT_OPS,
		D3D_SHADER_REQUIRES_INT64_OPS,
		D3D_SHADER_REQUIRES_VIEW_ID,
		D3D_SHADER_REQUIRES_DOUBLES,
		D3D_SHADER_REQUIRES_11_1_DOUBLE_EXTENSIONS,
		D3D_SHADER_REQUIRES_ATOMIC_INT64_ON_TYPED_RESOURCE,
		D3D_SHADER_REQUIRES_ATOMIC_INT64_ON_GROUP_SHARED,
		D3D_SHADER_REQUIRES_DERIVATIVES_IN_MESH_AND_AMPLIFICATION_SHADERS,
		D3D_SHADER_REQUIRES_WRITEABLE_MSAA_TEXTURES,
		D3D_SHADER_REQUIRES_WAVE_OPS
	};

	ESHExtension extensions[] = {
		ESHExtension_RayQuery,
		ESHExtension_16BitTypes,
		ESHExtension_I64,
		ESHExtension_Multiview,
		ESHExtension_F64,
		ESHExtension_F64,
		ESHExtension_AtomicI64,
		ESHExtension_AtomicI64,
		ESHExtension_MeshTaskTexDeriv,
		ESHExtension_WriteMSTexture,
		ESHExtension_SubgroupOperations
	};

	flags &= ~defaultOps;
	ESHExtension tmp = ESHExtension_None;

	for (U64 i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {

		if(!(flags & extensionMap[i]))
			continue;

		flags &= ~extensionMap[i];
		tmp = (ESHExtension)(tmp | extensions[i]);
	}

	*demotion = (ESHExtension)((~tmp) & ESHExtension_DxilNative);
	*ext = tmp;

	if(flags)
		retError(clean, Error_unsupportedOperation(0, "DxilMapToESHExtension() contained an unsupported extension"))

clean:
	return s_uccess;
}

Bool Compiler_convertWaveSizeParam(U32 threads, U8 *threadsShort, Error *e_rr) {

	Bool s_uccess = true;

	switch (threads) {
		case 0:			*threadsShort = 0;		break;
		case 1:			*threadsShort = 1;		break;
		case 2:			*threadsShort = 2;		break;
		case 4:			*threadsShort = 3;		break;
		case 8:			*threadsShort = 4;		break;
		case 16:		*threadsShort = 5;		break;
		case 32:		*threadsShort = 6;		break;
		case 64:		*threadsShort = 7;		break;
		case 128:		*threadsShort = 8;		break;
		case 256:		*threadsShort = 9;		break;
		default:
			retError(clean, Error_invalidParameter(threads, 0, "Compiler_convertWaveSizeParam()::threads is invalid"))
	}

clean:
	return s_uccess;
}

Bool Compiler_convertWaveSize(
	U32 waveSizeRecommended,
	U32 waveSizeMin,
	U32 waveSizeMax,
	U16 *waveSizes,
	Error *e_rr
) {

	Bool s_uccess = true;
	U8 recommend = 0, waveMin = 0, waveMax = 0;

	U32 waveSizeRecommendedTmp = waveSizeRecommended;
	U32 waveSizeMinTmp = waveSizeMin;
	U32 waveSizeMaxTmp = waveSizeMax;

	if(!waveSizeMaxTmp)
		waveSizeMaxTmp = 128;

	if(!waveSizeMinTmp)
		waveSizeMinTmp = 4;

	if(!waveSizeRecommendedTmp)
		waveSizeRecommendedTmp = (waveSizeMinTmp + waveSizeMaxTmp) / 2;		//Not base2, but don't care

	if(
		U32_min(waveSizeMinTmp, U32_min(waveSizeMaxTmp, waveSizeRecommendedTmp)) < 4 ||
		U32_max(waveSizeMinTmp, U32_max(waveSizeMaxTmp, waveSizeRecommendedTmp)) > 128
	)
		retError(clean, Error_invalidState(0, "Compiler_convertWaveSize() couldn't get groupSize; out of bounds"))

	gotoIfError3(clean, Compiler_convertWaveSizeParam(waveSizeRecommended, &recommend, e_rr))
	gotoIfError3(clean, Compiler_convertWaveSizeParam(waveSizeMin, &waveMin, e_rr))
	gotoIfError3(clean, Compiler_convertWaveSizeParam(waveSizeMax, &waveMax, e_rr))

	if(waveSizeMin || waveSizeMax)
		*waveSizes = ((U16)waveSizeMin << 4) | ((U16)waveSizeMax << 8) | ((U16)recommend << 12);

	else *waveSizes = recommend;

clean:
	return s_uccess;
}

Bool Compiler_convertMemberDXIL(
	SBFile *sbFile,
	ID3D12ShaderReflectionType *type,
	CharString *name,
	U16 parent,
	U32 globalOffset,
	Bool isPacked,
	Bool isUnused,
	U32 size,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	D3D12_SHADER_TYPE_DESC typeDesc{};

	ESBType shType{};

	U64 perElementStride{};
	U64 expectedSize{};
	ListU32 arrays{};
	U16 structId = U16_MAX;
	U32 elementSize = 1;

	if(!type || FAILED(type->GetDesc(&typeDesc)))
		retError(clean, Error_invalidState(
			0, "Compiler_convertMemberDXIL() DXIL contained constant buffer variable type with no desc"
		))

	if(typeDesc.Elements)
		elementSize = typeDesc.Elements;

	if(typeDesc.Class != D3D_SVC_STRUCT) {

		ESBPrimitive prim{};
		ESBStride stride{};
		ESBVector vector{};
		ESBMatrix matrix{};

		switch (typeDesc.Type) {

			case D3D_SVT_DOUBLE:	stride = ESBStride_X64;		prim = ESBPrimitive_Float;		break;
			case D3D_SVT_FLOAT:		stride = ESBStride_X32;		prim = ESBPrimitive_Float;		break;
			case D3D_SVT_FLOAT16:	stride = ESBStride_X16;		prim = ESBPrimitive_Float;		break;

			case D3D_SVT_UINT8:		stride = ESBStride_X8;		prim = ESBPrimitive_UInt;		break;
			case D3D_SVT_UINT16:	stride = ESBStride_X16;		prim = ESBPrimitive_UInt;		break;

			case D3D_SVT_BOOL:
			case D3D_SVT_UINT:		stride = ESBStride_X32;		prim = ESBPrimitive_UInt;		break;
			case D3D_SVT_UINT64:	stride = ESBStride_X64;		prim = ESBPrimitive_UInt;		break;

			case D3D_SVT_INT16:		stride = ESBStride_X16;		prim = ESBPrimitive_Int;		break;
			case D3D_SVT_INT:		stride = ESBStride_X32;		prim = ESBPrimitive_Int;		break;
			case D3D_SVT_INT64:		stride = ESBStride_X64;		prim = ESBPrimitive_Int;		break;

			default:
				retError(clean, Error_invalidState(
					0, "Compiler_convertMemberDXIL() DXIL contained invalid primitive type"
				))
		}

		if (typeDesc.Class == D3D_SVC_MATRIX_COLUMNS) {
			U32 cols = typeDesc.Rows;
			typeDesc.Rows = typeDesc.Columns;
			typeDesc.Columns = cols;
		}

		switch(typeDesc.Columns) {

			case 1:		vector = ESBVector_N1;		break;
			case 2:		vector = ESBVector_N2;		break;
			case 3:		vector = ESBVector_N3;		break;
			case 4:		vector = ESBVector_N4;		break;

			default:
				retError(clean, Error_unsupportedOperation(
					0, "Compiler_convertShaderBufferDXIL()::desc has an unrecognized vector"
				))
		}

		switch(typeDesc.Rows) {

			case 0:
			case 1:		matrix = ESBMatrix_N1;		break;

			case 2:		matrix = ESBMatrix_N2;		break;
			case 3:		matrix = ESBMatrix_N3;		break;
			case 4:		matrix = ESBMatrix_N4;		break;

			default:
				retError(clean, Error_unsupportedOperation(
					0, "Compiler_convertShaderBufferDXIL()::desc has an unrecognized type (matWxH)"
				))
		}

		shType = (ESBType) ESBType_create(stride, prim, vector, matrix);

		perElementStride = ESBType_getSize(shType, isPacked);
		expectedSize = perElementStride * elementSize;
	}

	else {

		expectedSize = size;
		perElementStride = size / elementSize;
		//U32 length = (U32) perElementStride;

		if(!isPacked && elementSize > 1) {
			perElementStride = (perElementStride + 15) &~ 15;
			//length = (U32)(size % perElementStride);
		}

		CharString structName = CharString_createRefCStrConst(typeDesc.Name);

		U64 j = 0;

		for (; j < sbFile->structs.length; ++j) {

			SBStruct strct = sbFile->structs.ptr[j];
			CharString structNamej = sbFile->structNames.ptr[j];

			if(CharString_equalsStringSensitive(structName, structNamej) && strct.stride == perElementStride)
				break;
		}

		//Insert type

		if (j == sbFile->structs.length)
			gotoIfError3(clean, SBFile_addStruct(
				sbFile, &structName, SBStruct{ .stride = (U32) perElementStride }, alloc, e_rr
			))

		structId = (U16) j;
	}

	if(size < expectedSize)
		retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL()::shType had mismatching size"))

	if(typeDesc.Elements)
		gotoIfError2(clean, ListU32_createRefConst(&typeDesc.Elements, 1, &arrays))

	if(typeDesc.Class != D3D_SVC_STRUCT)
		gotoIfError3(clean, SBFile_addVariableAsType(
			sbFile,
			name,
			globalOffset, parent, shType,
			isUnused ? ESBVarFlag_None : ESBVarFlag_IsUsedVarDXIL,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		))

	else {

		U16 newParent = (U16) sbFile->vars.length;

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			name,
			globalOffset, parent, structId,
			isUnused ? ESBVarFlag_None : ESBVarFlag_IsUsedVarDXIL,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		))

		if(!typeDesc.Members)
			retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing Members"))

		for (U64 j = 0; j < typeDesc.Members; ++j) {

			ID3D12ShaderReflectionType *member = type->GetMemberTypeByIndex((U32) j);
			const C8 *memberName = type->GetMemberTypeName((U32) j);

			if(!memberName)
				retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing member or member name"))

			D3D12_SHADER_TYPE_DESC memberDesc{};
			if(FAILED(member->GetDesc(&memberDesc)))
				retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing member desc"))

			U32 memberSize = 0;

			if(j + 1 == typeDesc.Members)
				memberSize = (U32)perElementStride - memberDesc.Offset;

			else {

				D3D12_SHADER_TYPE_DESC neighborDesc{};
				ID3D12ShaderReflectionType *neighbor = type->GetMemberTypeByIndex((U32) (j + 1));
				if(!neighbor || FAILED(neighbor->GetDesc(&neighborDesc)))
					retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing neighbor member desc"))

				memberSize = neighborDesc.Offset - memberDesc.Offset;
			}

			CharString varName = CharString_createRefCStrConst(memberName);
			gotoIfError3(clean, Compiler_convertMemberDXIL(
				sbFile, member, &varName, newParent, globalOffset + memberDesc.Offset, isPacked, isUnused, memberSize, alloc, e_rr
			))
		}
	}

clean:
	return s_uccess;
}

Bool Compiler_convertShaderBufferDXIL(
	const C8 *nameCStr,
	ID3D12FunctionReflection1 *funcRefl,
	ID3D12ShaderReflection1 *shaderRefl,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;
	D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
	D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};
	ID3D12ShaderReflectionConstantBuffer *constantBuffer = NULL;
	ESBSettingsFlags flags = ESBSettingsFlags_None;
	CharString name = CharString_createNull();
	D3D_CBUFFER_TYPE cbufferType = D3D_CT_CBUFFER;

	if(!shaderRefl && !funcRefl)
		retError(clean, Error_nullPointer(
			0, "Compiler_convertShaderBufferDXIL()::shaderRefl or funcRefl is required"
		))

	name = CharString_createRefCStrConst(nameCStr);
	constantBuffer = shaderRefl ? shaderRefl->GetConstantBufferByName(nameCStr) : funcRefl->GetConstantBufferByName(nameCStr);

	if(!constantBuffer || FAILED(constantBuffer->GetDesc(&constantBufferDesc)))
		retError(clean, Error_invalidState(1, "Compiler_convertShaderBufferDXIL() DXIL contained constant buffer but no desc"))

	if(!CharString_equalsStringSensitive(name, CharString_createRefCStrConst(constantBufferDesc.Name)))
		retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() DXIL contained mismatching names"))

	if(shaderRefl && FAILED(shaderRefl->GetResourceBindingDescByName(nameCStr, &resourceDesc)))
		retError(clean, Error_invalidState(
			1, "Compiler_convertShaderBufferDXIL() DXIL didn't contain resource binding for constant buffer"
		))

	if(funcRefl && FAILED(funcRefl->GetResourceBindingDescByName(nameCStr, &resourceDesc)))
		retError(clean, Error_invalidState(
			1, "Compiler_convertShaderBufferDXIL() DXIL didn't contain resource binding for constant buffer"
		))

	if(!CharString_equalsStringSensitive(name, CharString_createRefCStrConst(resourceDesc.Name)))
		retError(clean, Error_invalidState(1, "Compiler_convertShaderBufferDXIL() DXIL contained mismatching names"))

	switch (resourceDesc.Type) {

		case D3D_SIT_CBUFFER:
		case D3D_SIT_STRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
			break;

		default:
			retError(clean, Error_invalidState(
				1, "Compiler_convertShaderBufferDXIL() register type isn't allowed to have a buffer description"
			))
	}

	if(resourceDesc.Type != D3D_SIT_CBUFFER) {
		flags = ESBSettingsFlags_IsTightlyPacked;
		cbufferType = D3D_CT_RESOURCE_BIND_INFO;
	}

	if(
		constantBufferDesc.Type != cbufferType ||
		constantBufferDesc.uFlags ||
		!constantBufferDesc.Size ||
		!constantBufferDesc.Variables
	)
		retError(clean, Error_invalidState(
			0, "Compiler_convertShaderBufferDXIL() DXIL contained shader with unsupported buffer type or flags"
		))

	gotoIfError3(clean, SBFile_create(flags, constantBufferDesc.Size, alloc, sbFile, e_rr))

	for (U32 k = 0; k < constantBufferDesc.Variables; ++k) {

		ID3D12ShaderReflectionVariable *variable = constantBuffer->GetVariableByIndex(k);

		D3D12_SHADER_VARIABLE_DESC variableDesc{};

		const void *startTextureU64 = &variableDesc.StartTexture;
		const void *startSamplerU64 = &variableDesc.StartSampler;

		if(!variable || !variable->GetType() || FAILED(variable->GetDesc(&variableDesc)))
			retError(clean, Error_invalidState(
				0, "Compiler_convertShaderBufferDXIL() DXIL contained buffer variable with no desc or type"
			))

		ID3D12ShaderReflectionType *type = variable->GetType();

		if(
			variableDesc.DefaultValue ||
			!variableDesc.Name ||
			(variableDesc.uFlags && variableDesc.uFlags != D3D_SVF_USED) ||
			(*(const U64*)startTextureU64 && *(const U64*)startTextureU64 != U32_MAX) ||
			(*(const U64*)startSamplerU64 && *(const U64*)startSamplerU64 != U32_MAX)
		)
			retError(clean, Error_invalidState(
				0, "Compiler_convertShaderBufferDXIL() DXIL contained illegal buffer variable"
			))

		const C8 *variableName = variableDesc.Name;
		CharString varName = CharString_createRefCStrConst(variableName);

		U32 varSize = 0;

		if(k + 1 == constantBufferDesc.Variables)
			varSize = constantBufferDesc.Size - variableDesc.StartOffset;

		else {

			ID3D12ShaderReflectionVariable *neighbor = constantBuffer->GetVariableByIndex(k + 1);
			D3D12_SHADER_VARIABLE_DESC neighborDesc{};

			if(!neighbor || FAILED(neighbor->GetDesc(&neighborDesc)))
				retError(clean, Error_invalidState(
					0, "Compiler_convertShaderBufferDXIL() DXIL contained buffer variable with no neighbor"
				))

			varSize = neighborDesc.StartOffset - variableDesc.StartOffset;
		}

		gotoIfError3(clean, Compiler_convertMemberDXIL(
			sbFile,
			type,
			&varName,
			U16_MAX,
			variableDesc.StartOffset,
			!!(flags & ESBSettingsFlags_IsTightlyPacked),
			!(variableDesc.uFlags & D3D_SVF_USED),
			varSize,
			alloc, e_rr
		))
	}

	if(resourceDesc.BindCount > 1)
		retError(clean, Error_invalidState(
			0, "Compiler_convertShaderBufferDXIL()::BindCount is only allowed as 1 with CBuffers"
		))

clean:

	if(!s_uccess && sbFile)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

Bool Compiler_convertRegisterDXIL(
	ListSHRegisterRuntime *registers,
	const D3D12_SHADER_INPUT_BIND_DESC *input,
	ID3D12FunctionReflection1 *funcRefl,
	ID3D12ShaderReflection1 *shaderRefl,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString name = CharString_createRefCStrConst(input->Name);

	SHBindings bindings;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		bindings.arr[i] = SHBinding{ .space = U32_MAX, .binding = U32_MAX };

	bindings.arr[ESHBinaryType_DXIL] = SHBinding{ .space = input->Space, .binding = input->BindPoint };

	ListU32 arrays{};

	U32 texFlags = D3D_SIF_TEXTURE_COMPONENT_0 | D3D_SIF_TEXTURE_COMPONENT_1;
	U32 unknownFlags = D3D_SIF_FORCE_DWORD &~ (D3D_SIF_COMPARISON_SAMPLER | texFlags);

	Bool isReadTexture = input->Type == D3D_SIT_TEXTURE;

	Bool isWriteTexture =
		input->Type == D3D_SIT_UAV_RWTYPED &&
		input->Dimension >= D3D_SRV_DIMENSION_TEXTURE1D &&
		input->Dimension <= D3D_SRV_DIMENSION_TEXTURECUBEARRAY;

	ESHTexturePrimitive prim = ESHTexturePrimitive_Count;
	ESHTextureType registerType = ESHTextureType_Count;
	Bool isArray = false;
	SBFile sbFile = SBFile{};

	if(input->Type == D3D_SIT_CBUFFER) {

		if(!(input->uFlags & D3D_SIF_USERPACKED))
			retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL()::input uFlags need USERPACKED for CBuffer"))

		unknownFlags &=~ D3D_SIF_USERPACKED;
	}

	if(input->BindCount > 1)
		gotoIfError2(clean, ListU32_createRefConst(&input->BindCount, 1, &arrays))

	if(input->uFlags & unknownFlags)
		retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL()::input uFlags can't be set"))

	if((input->uFlags & D3D_SIF_COMPARISON_SAMPLER) && input->Type != D3D_SIT_SAMPLER)
		retError(clean, Error_invalidState(
			0, "Compiler_convertRegisterDXIL()::input uFlags can't be comparison sampler without being a sampler itself"
		))

	if((input->uFlags & texFlags) && !isReadTexture && !isWriteTexture)
		retError(clean, Error_invalidState(
			0, "Compiler_convertRegisterDXIL()::input uFlags texture component 0 and 1 can't be set on a non texture"
		))

	if(isReadTexture || isWriteTexture) {

		switch (input->ReturnType) {

			case D3D_RETURN_TYPE_UNORM:		prim = ESHTexturePrimitive_UNorm;	break;
			case D3D_RETURN_TYPE_SNORM:		prim = ESHTexturePrimitive_SNorm;	break;
			case D3D_RETURN_TYPE_UINT:		prim = ESHTexturePrimitive_UInt;	break;
			case D3D_RETURN_TYPE_SINT:		prim = ESHTexturePrimitive_SInt;	break;
			case D3D_RETURN_TYPE_FLOAT:		prim = ESHTexturePrimitive_Float;	break;

			default:
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL()::input returnType unsupported"
				))
		}

		prim = (ESHTexturePrimitive)(prim | (((input->uFlags >> 2) & 3) << 4));

		if(input->NumSamples && input->NumSamples != U32_MAX)
			retError(clean, Error_invalidState(
				0, "Compiler_convertRegisterDXIL() num samples must be U32_MAX or 0"
			))

		switch (input->Dimension) {

			case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
				isArray = true;
				// fallthrough

			case D3D_SRV_DIMENSION_TEXTURE1D:
				registerType = ESHTextureType_Texture1D;
				break;

			case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
				isArray = true;
				// fallthrough

			case D3D_SRV_DIMENSION_TEXTURE2D:
				registerType = ESHTextureType_Texture2D;
				break;

			case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
				isArray = true;
				// fallthrough

			case D3D_SRV_DIMENSION_TEXTURE2DMS:
				registerType = ESHTextureType_Texture2DMS;
				break;

			case D3D_SRV_DIMENSION_TEXTURE3D:
				registerType = ESHTextureType_Texture3D;
				break;

			case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
				isArray = true;
				// fallthrough

			case D3D_SRV_DIMENSION_TEXTURECUBE:
				registerType = ESHTextureType_TextureCube;
				break;

			default:
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() unknown texture register type"
				))
		}

		if((input->NumSamples != U32_MAX) != (registerType == ESHTextureType_Texture2DMS))
			retError(clean, Error_invalidState(
				0, "Compiler_convertRegisterDXIL() num samples not matching expectation"
			))
	}

	switch (input->Type) {

		case D3D_SIT_CBUFFER:
		case D3D_SIT_STRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:

			gotoIfError3(clean, Compiler_convertShaderBufferDXIL(
				input->Name,
				funcRefl,
				shaderRefl,
				alloc,
				&sbFile,
				e_rr
			))

			if(input->Type != D3D_SIT_CBUFFER && input->NumSamples != sbFile.bufferSize)
				retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL() NumSamples doesn't match buffer size"))

		default:
			break;
	}

	switch (input->Type) {

		case D3D_SIT_TEXTURE:

			gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
				registers,
				registerType,
				isArray,
				false,
				(U8)(1 << ESHBinaryType_DXIL),
				prim,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			))

			break;

		case D3D_SIT_SAMPLER:

			if(input->ReturnType || input->NumSamples || input->Dimension != D3D_SRV_DIMENSION_UNKNOWN)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() sampler had invalid return type, sampleCount or dimension"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				(U8)(1 << ESHBinaryType_DXIL),
				input->uFlags & D3D_SIF_COMPARISON_SAMPLER,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			))

			break;

		case D3D_SIT_UAV_RWTYPED:

			if(!isWriteTexture)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() RWBuffer is unsupported"		//TODO:?
				))

			switch(registerType) {

				case ESHTextureType_Texture3D:
				case ESHTextureType_TextureCube:
					retError(clean, Error_invalidState(
						0, "Compiler_convertRegisterDXIL() RWTexture3D and RWTextureCube don't exist"
					))

				default:
					break;
			}

			gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
				registers,
				registerType,
				isArray,
				(U8)(1 << ESHBinaryType_DXIL),
				prim,
				ETextureFormatId_Undefined,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			))

			break;

		case D3D_SIT_UAV_RWBYTEADDRESS:
		case D3D_SIT_BYTEADDRESS:

			if(
				input->ReturnType != D3D_RETURN_TYPE_MIXED ||
				input->NumSamples ||
				input->Dimension != D3D_SRV_DIMENSION_BUFFER
			)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() sampler had invalid return type, sampleCount or dimension"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_ByteAddressBuffer,
				input->Type == D3D_SIT_UAV_RWBYTEADDRESS,
				(U8)(1 << ESHBinaryType_DXIL),
				&name,
				arrays.length ? &arrays : NULL,
				NULL,
				bindings,
				alloc,
				e_rr
			))

			break;

		case D3D_SIT_CBUFFER:

			if(
				input->BindCount != 1 ||
				input->Dimension ||
				input->NumSamples ||
				input->ReturnType
			)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() cbuffer had invalid return type, sampleCount, bindCount or dimension"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_ConstantBuffer,
				false,
				(U8)(1 << ESHBinaryType_DXIL),
				&name,
				NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			))

			break;

		case D3D_SIT_STRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: {

			U32 stride = input->NumSamples;

			if(
				input->ReturnType != D3D_RETURN_TYPE_MIXED ||
				!stride ||
				input->Dimension != D3D_SRV_DIMENSION_BUFFER
			)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() buffer had invalid return type, sampleCount (stride) or dimension"
				))

			ESHBufferType type =
				input->Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ? ESHBufferType_StructuredBufferAtomic :
				ESHBufferType_StructuredBuffer;

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				type,
				input->Type != D3D_SIT_STRUCTURED,
				(U8)(1 << ESHBinaryType_DXIL),
				&name,
				arrays.length ? &arrays : NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			))

			break;
		}

		case D3D_SIT_RTACCELERATIONSTRUCTURE:

			if(
				(input->ReturnType != D3D_RETURN_TYPE_SINT && input->ReturnType != D3D_RETURN_TYPE_UINT) ||
				input->NumSamples != U32_MAX ||
				input->Dimension != D3D_SRV_DIMENSION_UNKNOWN
			)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() RTAS had invalid return type, sampleCount or dimension"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_AccelerationStructure,
				false,
				(U8)(1 << ESHBinaryType_DXIL),
				&name,
				arrays.length ? &arrays : NULL,
				NULL,
				bindings,
				alloc,
				e_rr
			))

			break;

		case D3D_SIT_UAV_APPEND_STRUCTURED:		//Append and consume are always reported as SBuffer with atomic counter
		case D3D_SIT_UAV_CONSUME_STRUCTURED:

		case D3D_SIT_TBUFFER:
		case D3D_SIT_UAV_FEEDBACKTEXTURE:
			retError(clean, Error_unsupportedOperation(0, "Compiler_convertRegisterDXIL() unsupported input type"))		//TODO:

		default:
			retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL() unknown input type"))
	}

clean:
	SBFile_free(&sbFile, alloc);
	return s_uccess;
}

class IncludeHandler;

typedef struct CompilerInterfaces {
	IDxcUtils *utils;
	IDxcCompiler3 *compiler;
	IncludeHandler *includeHandler;
} CompilerInterfaces;

Bool Compiler_processDXIL(
	Compiler compiler,
	Buffer *result,
	ListSHRegisterRuntime *registers,
	Buffer reflectionData,
	SHBinaryIdentifier toCompile,
	SpinLock *lock,
	ListSHEntryRuntime entries,
	ESHExtension *demotions,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ID3D12ShaderReflection1 *dxilRefl{};
	ID3D12LibraryReflection1 *dxilReflLib{};

	const void *resultPtr = NULL;
	Bool isLib = !CharString_length(toCompile.entrypoint);
	ESHExtension exts = ESHExtension_None;
	ListCharString strings{};
	U8 inputSemanticCount = 0;
	CompilerInterfaces *interfaces = (CompilerInterfaces*) compiler.interfaces;

	DxcBuffer reflDat = DxcBuffer{};

	if(!demotions || !result || !registers)
		retError(clean, Error_nullPointer(0, "Compiler_processSPIRV() demotions, result and registers are required"))

	resultPtr = result->ptr;

	if (Buffer_length(reflectionData))
		reflDat = DxcBuffer{ .Ptr = reflectionData.ptr, .Size = Buffer_length(reflectionData), .Encoding = 0 };

	else {

		DxcBuffer input = DxcBuffer{ .Ptr = resultPtr, .Size = Buffer_length(*result), .Encoding = 0 };

		void *part = NULL;
		U32 partSize = 0;

		if(FAILED(interfaces->utils->GetDxilContainerPart(&input, DXC_PART_REFLECTION_DATA, &part, &partSize)))
			retError(clean, Error_invalidState(
				0, "Compiler_processDXIL() DXIL didn't contain any reflection, but no additional reflection data was specified"
			))

			reflDat = DxcBuffer{ .Ptr = part, .Size = partSize, .Encoding = 0 };
	}

	if(isLib && FAILED(interfaces->utils->CreateReflection(&reflDat, IID_PPV_ARGS(&dxilReflLib))))
		retError(clean, Error_invalidState(0, "Compiler_processDXIL() lib reflection is invalid"))

	else if (!isLib && FAILED(interfaces->utils->CreateReflection(&reflDat, IID_PPV_ARGS(&dxilRefl))))
		retError(clean, Error_invalidState(0, "Compiler_processDXIL() shader reflection is invalid"))

	//Payload / intersection data reflection

	if (isLib) {

		D3D12_LIBRARY_DESC lib = D3D12_LIBRARY_DESC{};
		if(FAILED(dxilReflLib->GetDesc(&lib)))
			retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get D3D12_LIBRARY_DESC"))

		for(U64 i = 0; i < lib.FunctionCount; ++i) {

			ID3D12FunctionReflection1 *funcRefl = NULL;

			if ((i >> 31) || (funcRefl = dxilReflLib->GetFunctionByIndex1((INT)i)) == NULL)
				retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get ID3D12FunctionReflection"))

			D3D12_FUNCTION_DESC funcDesc0 = D3D12_FUNCTION_DESC{};
			D3D12_FUNCTION_DESC1 funcDesc = D3D12_FUNCTION_DESC1{};
			if(FAILED(funcRefl->GetDesc(&funcDesc0)) || FAILED(funcRefl->GetDesc1(&funcDesc)))
				retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() couldn't get D3D12_FUNCTION_DESC1"
				))

			U64 featureFlags = funcRefl->GetRequiresFlags();
			ESHExtension demote = ESHExtension_None;
			gotoIfError3(clean, DxilMapToESHExtension(featureFlags, &exts, &demote, e_rr))

			if(i)
				*demotions = (ESHExtension)(*demotions & demote);

			else *demotions = demote;

			U8 attributeSize = 0;
			U8 payloadSize = 0;
			U32 groupSize[3] = { 0 };
			U16 waveSizes = 0;
			Bool hasGroupSize = false;

			//Reflect payload size & attribute size

			if (
				funcDesc.ShaderType > D3D12_SHVER_RAY_GENERATION_SHADER &&
				funcDesc.ShaderType <= D3D12_SHVER_CALLABLE_SHADER
			) {

				if(funcDesc.RaytracingShader.ParamPayloadSize > 128)
					retError(clean, Error_outOfBounds(
						0, funcDesc.RaytracingShader.ParamPayloadSize, 128, "Compiler_processDXIL() payload out of bounds"
					))

				payloadSize = (U8) funcDesc.RaytracingShader.ParamPayloadSize;

				if(funcDesc.RaytracingShader.AttributeSize > 32)
					retError(clean, Error_outOfBounds(
						0, funcDesc.RaytracingShader.AttributeSize, 32,
						"Compiler_processDXIL() attribute out of bounds"
					))

				attributeSize = (U8) funcDesc.RaytracingShader.AttributeSize;
			}

			//Reflect group size and wave size

			else if (
				funcDesc.ShaderType == D3D12_SHVER_COMPUTE_SHADER ||
				funcDesc.ShaderType == D3D12_SHVER_NODE_SHADER
			) {
				D3D12_COMPUTE_SHADER_DESC computeShader =
					funcDesc.ShaderType == D3D12_SHVER_NODE_SHADER ? funcDesc.NodeShader.ComputeDesc :
					funcDesc.ComputeShader;

				for(U8 j = 0; j < 3; ++j)
					groupSize[j] = computeShader.NumThreads[j];

				hasGroupSize = true;

				gotoIfError3(clean, Compiler_convertWaveSize(
					computeShader.WaveSizePreferred, computeShader.WaveSizeMin, computeShader.WaveSizeMax,
					&waveSizes, e_rr
				))
			}

			else if (funcDesc.ShaderType == D3D12_SHVER_MESH_SHADER) {

				hasGroupSize = true;

				for(U8 j = 0; j < 3; ++j)
					groupSize[j] = funcDesc.MeshShader.NumThreads[j];
			}

			else if (funcDesc.ShaderType == D3D12_SHVER_AMPLIFICATION_SHADER) {

				hasGroupSize = true;

				for(U8 j = 0; j < 3; ++j)
					groupSize[j] = funcDesc.AmplificationShader.NumThreads[j];
			}

			if(hasGroupSize)
				gotoIfError3(clean, Compiler_validateGroupSize(groupSize, e_rr))

			ESBType inputs[16] = {};		//TODO:
			ESBType outputs[16] = {};
			U8 uniqueInputSemantics = 0;
			ListCharString uniqueSemantics = ListCharString{};
			U8 inputSemantics[16] = {};
			U8 outputSemantics[16] = {};

			if(!funcDesc0.Name)
				retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL contained no library name"))

			for (U32 j = 0; j < funcDesc0.ConstantBuffers; ++j) {		//Validate buffers

				ID3D12ShaderReflectionConstantBuffer *constantBuffer = funcRefl->GetConstantBufferByIndex(j);
				D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
				D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};

				if(FAILED(constantBuffer->GetDesc(&constantBufferDesc)))
					retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL contained constant buffer but no desc"))

				if(funcRefl && FAILED(funcRefl->GetResourceBindingDescByName(constantBufferDesc.Name, &resourceDesc)))
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() DXIL didn't contain resource binding for constant buffer"
					))

				switch (resourceDesc.Type) {

					case D3D_SIT_CBUFFER:
					case D3D_SIT_STRUCTURED:
					case D3D_SIT_UAV_RWSTRUCTURED:
					case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
						break;

					default:
						retError(clean, Error_invalidState(
							1, "Compiler_processDXIL() DXIL contained buffer description not bound to a valid resource type"
						))
				}
			}

			for (U32 j = 0; j < funcDesc0.BoundResources; ++j) {

				D3D12_SHADER_INPUT_BIND_DESC input{};
				if(FAILED(funcRefl->GetResourceBindingDesc(j, &input)))
					retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL contained invalid resource"))

				gotoIfError3(clean, Compiler_convertRegisterDXIL(registers, &input, funcRefl, NULL, alloc, e_rr))
			}

			CharString demangled = CharString_createRefCStrConst(funcDesc0.Name);

			if (funcDesc0.Name[0] == '\x1') {	//Mangled

				U64 firstAt = CharString_findFirstSensitive(demangled, '@', 2, 0);

				if(funcDesc0.Name[1] != '?' || firstAt == U64_MAX)
					retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL had invalid name mangling"))

				demangled = CharString_createRefSizedConst(demangled.ptr + 2, firstAt - 2, false);
			}

			gotoIfError3(clean, Compiler_finalizeEntrypoint(
				groupSize, payloadSize, attributeSize, waveSizes,
				inputs, outputs,
				uniqueInputSemantics, &uniqueSemantics, inputSemantics, outputSemantics,
				demangled, lock, entries,
				alloc, e_rr
			))
		}
	}

	//Get input/output

	else {

		ESBType inputs[16] = {};
		ESBType outputs[16] = {};
		U8 inputSemantics[16] = {};
		U8 outputSemantics[16] = {};

		U32 groupSize[3] = { 0 };
		U16 waveSizes = 0;

		D3D12_SHADER_DESC refl = D3D12_SHADER_DESC{};
		if(FAILED(dxilRefl->GetDesc(&refl)))
			retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get D3D12_LIBRARY_DESC"))

		U64 reqFlags = dxilRefl->GetRequiresFlags();
		gotoIfError3(clean, DxilMapToESHExtension(reqFlags, &exts, demotions, e_rr))

		Bool isPixelShader = toCompile.stageType == ESHPipelineStage_Pixel;

		if (
			toCompile.stageType == ESHPipelineStage_Compute ||
			toCompile.stageType == ESHPipelineStage_MeshExt ||
			toCompile.stageType == ESHPipelineStage_TaskExt
		) {
			dxilRefl->GetThreadGroupSize(&groupSize[0], &groupSize[1], &groupSize[2]);
			gotoIfError3(clean, Compiler_validateGroupSize(groupSize, e_rr))
		}

		if (toCompile.stageType == ESHPipelineStage_Compute) {

			U32 waveSizeRecommended, waveSizeMin, waveSizeMax;
			dxilRefl->GetWaveSize(&waveSizeRecommended, &waveSizeMin, &waveSizeMax);

			gotoIfError3(clean, Compiler_convertWaveSize(
				waveSizeRecommended, waveSizeMin, waveSizeMax, &waveSizes, e_rr
			))
		}

		for(U64 j = 0, outputC = 0, inputC = 0; j < (U64)refl.OutputParameters + refl.InputParameters; ++j) {

			Bool isOutput = j < refl.OutputParameters;
			ESBType *inputTypes = isOutput ? outputs : inputs;
			U8 *semantics = isOutput ? outputSemantics : inputSemantics;

			D3D12_SIGNATURE_PARAMETER_DESC signature{};
			if(FAILED(
				isOutput ?
				dxilRefl->GetOutputParameterDesc((UINT) j, &signature) :
				dxilRefl->GetInputParameterDesc((UINT)(j - refl.OutputParameters), &signature)
			))
				retError(clean, Error_invalidState(
					0, "Compiler_processDXIL() couldn't get output D3D12_SIGNATURE_PARAMETER_DESC"
				))

			if(!isPixelShader && signature.SystemValueType != D3D_NAME_UNDEFINED)
				continue;

			if(isPixelShader && signature.SystemValueType != D3D_NAME_TARGET)
				continue;

			if(signature.SemanticIndex >= 16)
				retError(clean, Error_invalidState(
					0, "Compiler_processDXIL() input location out of bounds (allowed up to 16)"
				))

			U8 semanticValue = 0;

			if (!isPixelShader && !CharString_equalsStringInsensitive(
				CharString_createRefCStrConst(signature.SemanticName),
				CharString_createRefCStrConst("TEXCOORD")
			)) {

				U64 start = isOutput ? inputSemanticCount : 0;
				U64 end = isOutput ? strings.length : inputSemanticCount;
				U64 k = start;

				for(; k < end; ++k)
					if(CharString_equalsStringInsensitive(
						strings.ptr[k], CharString_createRefCStrConst(signature.SemanticName)
					))
						break;

				U64 semanticName = (k - start) + 1;

				if(semanticName >= 16)
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() unique semantic name out of bounds"
					))

				if(k == end) {	//Not found, so insert

					gotoIfError2(clean, ListCharString_insert(
						&strings, k, CharString_createRefCStrConst(signature.SemanticName), alloc
					))

					if(!isOutput)
						++inputSemanticCount;
				}

				semanticValue = (U8)((semanticName << 4) | signature.SemanticIndex);
			}

			else semanticValue = (U8) signature.SemanticIndex;

			if(signature.MinPrecision || signature.Stream)
				retError(clean, Error_invalidState(
					0, "Compiler_processDXIL() invalid signature parameter; MinPrecision or Stream"
				))

			ESBPrimitive prim = ESBPrimitive_Invalid;
			ESBStride stride = ESBStride_X8;

			switch (signature.ComponentType) {

				case  D3D_REGISTER_COMPONENT_FLOAT16:	prim = ESBPrimitive_Float;	stride = ESBStride_X16;		break;
				case  D3D_REGISTER_COMPONENT_UINT16:	prim = ESBPrimitive_UInt;	stride = ESBStride_X16;		break;
				case  D3D_REGISTER_COMPONENT_SINT16:	prim = ESBPrimitive_Int;	stride = ESBStride_X16;		break;

				case  D3D_REGISTER_COMPONENT_FLOAT32:	prim = ESBPrimitive_Float;	stride = ESBStride_X32;		break;
				case  D3D_REGISTER_COMPONENT_UINT32:	prim = ESBPrimitive_UInt;	stride = ESBStride_X32;		break;
				case  D3D_REGISTER_COMPONENT_SINT32:	prim = ESBPrimitive_Int;	stride = ESBStride_X32;		break;

				case  D3D_REGISTER_COMPONENT_FLOAT64:	prim = ESBPrimitive_Float;	stride = ESBStride_X64;		break;
				case  D3D_REGISTER_COMPONENT_UINT64:	prim = ESBPrimitive_UInt;	stride = ESBStride_X64;		break;
				case  D3D_REGISTER_COMPONENT_SINT64:	prim = ESBPrimitive_Int;	stride = ESBStride_X64;		break;
				default:
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() invalid component type; expected one of F32, U32 or I32"
					))
			}

			ESBVector vec = ESBVector_N1;

			switch (signature.Mask) {
				case  1:	vec = ESBVector_N1;		break;
				case  3:	vec = ESBVector_N2;		break;
				case  7:	vec = ESBVector_N3;		break;
				case 15:	vec = ESBVector_N4;		break;
				default:
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() invalid signature mask; expected one of 1,3,7,15"
					))
			}

			ESBType type = (ESBType) ESBType_create(stride, prim, vec, ESBMatrix_N1);
			U64 *counter = isOutput ? &outputC : &inputC;

			if(inputTypes[*counter] || *counter >= 16)
				retError(clean, Error_invalidState(
					0, "Compiler_processDXIL() output/input location is already defined or out of bounds"
				))

			semantics[*counter] = semanticValue;
			inputTypes[(*counter)++] = type;
		}

		for (U32 j = 0; j < refl.ConstantBuffers; ++j) {		//Validate buffers

			ID3D12ShaderReflectionConstantBuffer *constantBuffer = dxilRefl->GetConstantBufferByIndex(j);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
			D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};

			if(FAILED(constantBuffer->GetDesc(&constantBufferDesc)))
				retError(clean, Error_invalidState(1, "Compiler_processDXIL() DXIL contained constant buffer but no desc"))

			if(dxilRefl && FAILED(dxilRefl->GetResourceBindingDescByName(constantBufferDesc.Name, &resourceDesc)))
				retError(clean, Error_invalidState(
					1, "Compiler_processDXIL() DXIL didn't contain resource binding for constant buffer"
				))

			switch (resourceDesc.Type) {

				case D3D_SIT_CBUFFER:
				case D3D_SIT_STRUCTURED:
				case D3D_SIT_UAV_RWSTRUCTURED:
				case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
					break;

				default:
					retError(clean, Error_invalidState(
						1, "Compiler_processDXIL() DXIL contained buffer description not bound to a valid resource type"
					))
			}
		}

		for (U32 j = 0; j < refl.BoundResources; ++j) {

			D3D12_SHADER_INPUT_BIND_DESC input{};
			if(FAILED(dxilRefl->GetResourceBindingDesc(j, &input)))
				retError(clean, Error_invalidState(1, "Compiler_processDXIL() DXIL contained invalid resource"))

			gotoIfError3(clean, Compiler_convertRegisterDXIL(registers, &input, NULL, dxilRefl, alloc, e_rr))
		}

		gotoIfError3(clean, Compiler_finalizeEntrypoint(
			groupSize, 0, 0, waveSizes,
			inputs, outputs,
			inputSemanticCount, &strings, inputSemantics, outputSemantics,
			toCompile.entrypoint, lock, entries,
			alloc, e_rr
		))
	}

	if((toCompile.extensions & exts) != exts)
		retError(clean, Error_invalidState(
			2, "Compiler_processDXIL() DXIL contained capability that wasn't enabled by oiSH file (use annotations)"
		))

	//Ensure we have a valid DXIL file

	if(
		Buffer_length(*result) <= 0x14 ||
		Buffer_readU32(*result, 0, NULL) != C8x4('D', 'X', 'B', 'C') ||
		I32x4_eq4(I32x4_load4((const U8*)resultPtr + sizeof(U32)), I32x4_zero())		//Unsigned
	)
		retError(clean, Error_invalidState(2, "Compiler_processDXIL() DXIL returned is invalid"))

clean:

	ListCharString_freeUnderlying(&strings, alloc);

	if(dxilRefl)
		dxilRefl->Release();

	if(dxilReflLib)
		dxilReflLib->Release();

	return s_uccess;
}

Bool Compiler_disassembleDXIL(Compiler comp, Buffer buf, Allocator alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	U64 binLen = Buffer_length(buf);

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp.interfaces;
	HRESULT hr = 0;
	CharString str;
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *blobUtf8 = NULL;

	DxcBuffer buffer {
		.Ptr = buf.ptr,
		.Size = binLen,
		.Encoding = 0
	};

	if(
		binLen <= 0x14 ||
		Buffer_readU32(buf, 0, NULL) != C8x4('D', 'X', 'B', 'C')
	)
		retError(clean, Error_invalidState(0, "Compiler_createDisassembly() DXIL is invalid"))

	hr = interfaces->compiler->Disassemble(&buffer, IID_PPV_ARGS(&dxcResult));

	if(FAILED(hr))
		retError(clean, Error_invalidOperation(0, "Compiler_createDisassembly() DXIL couldn't be disassembled"))

	hr = dxcResult->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&blobUtf8), NULL);

	if(FAILED(hr))
		retError(clean, Error_invalidOperation(1, "Compiler_createDisassembly() DXIL disassembly couldn't be obtained"))

	str = CharString_createRefSizedConst(blobUtf8->GetStringPointer(), blobUtf8->GetStringLength(), false);
	gotoIfError2(clean, CharString_createCopy(str, alloc, result))
	
clean:

	if(dxcResult)
		dxcResult->Release();

	if(blobUtf8)
		blobUtf8->Release();

	return s_uccess;
}
