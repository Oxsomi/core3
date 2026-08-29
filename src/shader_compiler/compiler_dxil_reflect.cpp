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

//shader_compiler/compiler_dxil_reflect.cpp

#include "shader_compiler/compiler.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/math/vec4i.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/c8.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "types/base/platform_types.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#include <Unknwn.h>
#endif

//dxcapi.h must precede d3d12shader.h: on non-Windows it pulls in the WinAdapter that defines IUnknown,
// which directx/d3d12shader.h (via d3dcommon.h) needs before it declares its reflection interfaces.
//On Windows those COM types (IUnknown, REFCLSID, BOOL, LPCWSTR, ...) come from <Windows.h>/<Unknwn.h> above.
#define ENABLE_DXC_STATIC_LINKING
#include "dxcompiler/dxcapi.h"
#include "directx/d3d12shader.h"
#include "dxcompiler/dxcreflect.h"
#include <exception>

Bool Compiler_convertMemberDXIL(
	SBFile *sbFile,
	ID3D12ShaderReflectionType *type,
	CharString *name,
	U16 parent,
	U32 globalOffset,
	Bool isPacked,
	Bool isUnused,
	U32 size,
	const Allocator *alloc,
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
		));

	if(typeDesc.Elements)
		elementSize = typeDesc.Elements;

	if(typeDesc.Class != D3D_SVC_STRUCT) {

		ESBPrimitive prim{};
		ESBStride stride{};
		ESBVector vector{};
		ESBMatrix matrix{};

		switch (typeDesc.Type) {

			case D3D_SVT_DOUBLE:   stride = ESBStride_X64;  prim = ESBPrimitive_Float;  break;
			case D3D_SVT_FLOAT:    stride = ESBStride_X32;  prim = ESBPrimitive_Float;  break;
			case D3D_SVT_FLOAT16:  stride = ESBStride_X16;  prim = ESBPrimitive_Float;  break;

			case D3D_SVT_UINT8:    stride = ESBStride_X8;   prim = ESBPrimitive_UInt;   break;
			case D3D_SVT_UINT16:   stride = ESBStride_X16;  prim = ESBPrimitive_UInt;   break;

			case D3D_SVT_BOOL:
			case D3D_SVT_UINT:     stride = ESBStride_X32;  prim = ESBPrimitive_UInt;   break;
			case D3D_SVT_UINT64:   stride = ESBStride_X64;  prim = ESBPrimitive_UInt;   break;

			case D3D_SVT_INT16:    stride = ESBStride_X16;  prim = ESBPrimitive_Int;    break;
			case D3D_SVT_INT:      stride = ESBStride_X32;  prim = ESBPrimitive_Int;    break;
			case D3D_SVT_INT64:    stride = ESBStride_X64;  prim = ESBPrimitive_Int;    break;

			//Opaque element: on DXIL, DXC does not describe a non-struct structured-buffer element's type
			// (e.g. RWStructuredBuffer<float4> reflects its "$Element" as D3D_SVT_VOID with no rows/cols),
			// whereas SPIRV reflects the real type.
			//Preserve the element *size* by reflecting it as a raw 1..4-wide 32-bit block,
			// so RT/structured-buffer shaders still reflect on DXIL.
			case D3D_SVT_VOID:

				if(!size || (size & 3) || (size >> 2) > 4)
					retError(clean, Error_invalidState(
						0, "Compiler_convertMemberDXIL() opaque element isn't a 1..4-wide 32-bit block"
					));

				stride = ESBStride_X32;
				prim = ESBPrimitive_UInt;
				typeDesc.Columns = size >> 2;
				typeDesc.Rows = 1;
				break;

			default:
				retError(clean, Error_invalidState(
					0, "Compiler_convertMemberDXIL() DXIL contained invalid primitive type"
				));
		}

		if (typeDesc.Class == D3D_SVC_MATRIX_COLUMNS) {
			U32 cols = typeDesc.Rows;
			typeDesc.Rows = typeDesc.Columns;
			typeDesc.Columns = cols;
		}

		switch(typeDesc.Columns) {

			case 1:  vector = ESBVector_N1;  break;
			case 2:  vector = ESBVector_N2;  break;
			case 3:  vector = ESBVector_N3;  break;
			case 4:  vector = ESBVector_N4;  break;

			default:
				retError(clean, Error_unsupportedOperation(
					0, "Compiler_convertShaderBufferDXIL()::desc has an unrecognized vector"
				));
		}

		switch(typeDesc.Rows) {

			case 0:
			case 1:  matrix = ESBMatrix_N1;  break;

			case 2:  matrix = ESBMatrix_N2;  break;
			case 3:  matrix = ESBMatrix_N3;  break;
			case 4:  matrix = ESBMatrix_N4;  break;

			default:
				retError(clean, Error_unsupportedOperation(
					0, "Compiler_convertShaderBufferDXIL()::desc has an unrecognized type (matWxH)"
				));
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
			CharString structNamej = sbFile->names.entryStrings.ptr[j];

			if(CharString_equalsStringSensitive(&structName, &structNamej) && strct.stride == perElementStride)
				break;
		}

		//Insert type

		if (j == sbFile->structs.length)
			gotoIfError3(clean, SBFile_addStruct(
				sbFile, &structName, SBStruct{ .stride = (U32) perElementStride }, alloc, e_rr
			));

		structId = (U16) j;
	}

	if(size < expectedSize)
		retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL()::shType had mismatching size"));

	if(typeDesc.Elements)
		gotoIfError3(clean, ListU32_createRefConst(&typeDesc.Elements, 1, &arrays, e_rr));

	if(typeDesc.Class != D3D_SVC_STRUCT) {
		gotoIfError3(clean, SBFile_addVariableAsType(
			sbFile,
			name,
			globalOffset, parent, shType,
			isUnused ? ESBVarFlag_None : ESBVarFlag_IsUsedVarDXIL,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		));
	}

	else {

		U16 newParent = (U16) sbFile->vars.length;

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			name,
			globalOffset, parent, structId,
			isUnused ? ESBVarFlag_None : ESBVarFlag_IsUsedVarDXIL,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		));

		if(!typeDesc.Members)
			retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing Members"));

		for (U64 j = 0; j < typeDesc.Members; ++j) {

			ID3D12ShaderReflectionType *member = type->GetMemberTypeByIndex((U32) j);
			const C8 *memberName = type->GetMemberTypeName((U32) j);

			if(!memberName)
				retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing member or member name"));

			D3D12_SHADER_TYPE_DESC memberDesc{};
			if(FAILED(member->GetDesc(&memberDesc)))
				retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing member desc"));

			U32 memberSize = 0;

			if(j + 1 == typeDesc.Members)
				memberSize = (U32)perElementStride - memberDesc.Offset;

			else {

				D3D12_SHADER_TYPE_DESC neighborDesc{};
				ID3D12ShaderReflectionType *neighbor = type->GetMemberTypeByIndex((U32) (j + 1));
				if(!neighbor || FAILED(neighbor->GetDesc(&neighborDesc)))
					retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing neighbor member desc"));

				memberSize = neighborDesc.Offset - memberDesc.Offset;
			}

			CharString varName = CharString_createRefCStrConst(memberName);
			gotoIfError3(clean, Compiler_convertMemberDXIL(
				sbFile, member, &varName, newParent, globalOffset + memberDesc.Offset, isPacked, isUnused, memberSize, alloc,
				e_rr
			));
		}
	}

clean:
	return s_uccess;
}

Bool Compiler_convertShaderBufferDXIL(
	const C8 *nameCStr,
	ID3D12FunctionReflection1 *funcRefl,
	ID3D12ShaderReflection1 *shaderRefl,
	Bool &emptyBuffer,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;
	Bool allocated = false;

	D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
	D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};
	ID3D12ShaderReflectionConstantBuffer *constantBuffer = NULL;
	ESBSettingsFlags flags = ESBSettingsFlags_None;
	CharString name = CharString_createNull();
	D3D_CBUFFER_TYPE cbufferType = D3D_CT_CBUFFER;
	U32 variables = 0;
	ID3D12ShaderReflectionVariable *cbufVariableRoot = nullptr;
	ID3D12ShaderReflectionType *cbufVariableTypeRoot = nullptr;
	Bool cbufUsedRoot = false;

	if(!shaderRefl && !funcRefl)
		retError(clean, Error_nullPointer(
			0, "Compiler_convertShaderBufferDXIL()::shaderRefl or funcRefl is required"
		));

	name = CharString_createRefCStrConst(nameCStr);
	constantBuffer = shaderRefl ? shaderRefl->GetConstantBufferByName(nameCStr) : funcRefl->GetConstantBufferByName(nameCStr);

	if(!constantBuffer || FAILED(constantBuffer->GetDesc(&constantBufferDesc)))
		retError(clean, Error_invalidState(1, "Compiler_convertShaderBufferDXIL() DXIL contained constant buffer but no desc"));

	if(!CharString_equalsCStringSensitive(&name, constantBufferDesc.Name))
		retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() DXIL contained mismatching names"));

	if(shaderRefl && FAILED(shaderRefl->GetResourceBindingDescByName(nameCStr, &resourceDesc)))
		retError(clean, Error_invalidState(
			1, "Compiler_convertShaderBufferDXIL() DXIL didn't contain resource binding for constant buffer"
		));

	if(funcRefl && FAILED(funcRefl->GetResourceBindingDescByName(nameCStr, &resourceDesc)))
		retError(clean, Error_invalidState(
			1, "Compiler_convertShaderBufferDXIL() DXIL didn't contain resource binding for constant buffer"
		));

	if(!CharString_equalsCStringSensitive(&name, resourceDesc.Name))
		retError(clean, Error_invalidState(1, "Compiler_convertShaderBufferDXIL() DXIL contained mismatching names"));

	switch (resourceDesc.Type) {

		case D3D_SIT_CBUFFER:
		case D3D_SIT_STRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
			break;

		default:
			retError(clean, Error_invalidState(
				1, "Compiler_convertShaderBufferDXIL() register type isn't allowed to have a buffer description"
			));
	}

	if(resourceDesc.Type != D3D_SIT_CBUFFER) {
		flags = ESBSettingsFlags_IsTightlyPacked;
		cbufferType = D3D_CT_RESOURCE_BIND_INFO;
	}

	if (
		constantBufferDesc.Type != cbufferType ||
		constantBufferDesc.uFlags ||
		!constantBufferDesc.Size
	)
		retError(clean, Error_invalidState(
			0, "Compiler_convertShaderBufferDXIL() DXIL contained shader with unsupported buffer type or flags"
		));

	if (!constantBufferDesc.Variables) {
		emptyBuffer = true;
		goto clean;
	}

	gotoIfError3(clean, SBFile_create(flags, constantBufferDesc.Size, alloc, sbFile, e_rr));
	allocated = true;

	//Possibly a ConstantBuffer<T>, check if inner variable is a member named T with typename T that is a struct.

	variables = constantBufferDesc.Variables;

	if (resourceDesc.Type == D3D_SIT_CBUFFER && constantBufferDesc.Variables == 1) {

		ID3D12ShaderReflectionVariable *variable = constantBuffer->GetVariableByIndex(0);
		D3D12_SHADER_VARIABLE_DESC variableDesc{};

		if (!variable || !variable->GetType() || FAILED(variable->GetDesc(&variableDesc)))
			retError(clean, Error_invalidState(
				0, "Compiler_convertShaderBufferDXIL() DXIL contained buffer variable with no desc or type"
			));
		
		ID3D12ShaderReflectionType *type = variable->GetType();
		D3D12_SHADER_TYPE_DESC typeDesc{};

		if (!type || FAILED(type->GetDesc(&typeDesc)))
			retError(clean, Error_invalidState(
				0, "Compiler_convertMemberDXIL() DXIL contained constant buffer variable type with no desc"
			));

		CharString varName = CharString_createRefCStrConst(variableDesc.Name);

		//We found a ConstantBuffer, we'll do two things:
		// - Pretend we have 1 + inner variable count variables.
		//        This will allow us to validate the root node and variables.
		// - Ignore the root node for the actual contents, it's only for validation.

		if (typeDesc.Class == D3D_SVC_STRUCT && CharString_equalsStringSensitive(&varName, &name)) {
			variables += typeDesc.Members;
			cbufVariableRoot = variable;
			cbufVariableTypeRoot = type;
			cbufUsedRoot = variableDesc.uFlags & D3D_SVF_USED;
		}
	}

	for (U32 k = 0; k < variables; ++k) {

		D3D12_SHADER_VARIABLE_DESC variableDesc{};
		ID3D12ShaderReflectionType *type = nullptr;

		if(!(cbufVariableRoot && k)) {

			ID3D12ShaderReflectionVariable *variable = constantBuffer->GetVariableByIndex(k);

			const void *startTextureU64 = &variableDesc.StartTexture;
			const void *startSamplerU64 = &variableDesc.StartSampler;

			if (!variable || !variable->GetType() || FAILED(variable->GetDesc(&variableDesc)))
				retError(clean, Error_invalidState(
					0, "Compiler_convertShaderBufferDXIL() DXIL contained buffer variable with no desc or type"
				));

			type = variable->GetType();

			if(
				variableDesc.DefaultValue ||
				!variableDesc.Name ||
				(variableDesc.uFlags && variableDesc.uFlags != D3D_SVF_USED) ||
				(*(const U64*)startTextureU64 && *(const U64*)startTextureU64 != U32_MAX) ||
				(*(const U64*)startSamplerU64 && *(const U64*)startSamplerU64 != U32_MAX)
			)
				retError(clean, Error_invalidState(
					0, "Compiler_convertShaderBufferDXIL() DXIL contained illegal buffer variable"
				));
		}

		else {

			type = cbufVariableTypeRoot->GetMemberTypeByIndex(k - 1);
			variableDesc.Name = cbufVariableTypeRoot->GetMemberTypeName(k - 1);

			D3D12_SHADER_TYPE_DESC memberDesc{};
			if (!type || FAILED(type->GetDesc(&memberDesc)))
				retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing member desc"));

			variableDesc.StartOffset = memberDesc.Offset;
			variableDesc.uFlags = cbufUsedRoot ? D3D_SVF_USED : 0;
		}

		if (!k && cbufVariableRoot) //Skip root
			continue;

		const C8 *variableName = variableDesc.Name;
		CharString varName = CharString_createRefCStrConst(variableName);

		U32 varSize = 0;

		if (cbufVariableRoot) {
			
			if(k + 1 == variables)
				varSize = (U32)constantBufferDesc.Size - variableDesc.StartOffset;

			else {

				D3D12_SHADER_TYPE_DESC neighborDesc{};
				ID3D12ShaderReflectionType *neighbor = cbufVariableTypeRoot->GetMemberTypeByIndex((U32) k);
				if(!neighbor || FAILED(neighbor->GetDesc(&neighborDesc)))
					retError(clean, Error_invalidState(0, "Compiler_convertShaderBufferDXIL() missing neighbor member desc"));

				varSize = neighborDesc.Offset - variableDesc.StartOffset;
			}
		}

		else if(k + 1 == constantBufferDesc.Variables)
			varSize = constantBufferDesc.Size - variableDesc.StartOffset;

		else {

			ID3D12ShaderReflectionVariable *neighbor = constantBuffer->GetVariableByIndex(k + 1);
			D3D12_SHADER_VARIABLE_DESC neighborDesc{};

			if(!neighbor || FAILED(neighbor->GetDesc(&neighborDesc)))
				retError(clean, Error_invalidState(
					0, "Compiler_convertShaderBufferDXIL() DXIL contained buffer variable with no neighbor"
				));

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
		));
	}

	if(resourceDesc.BindCount > 1)
		retError(clean, Error_invalidState(
			0, "Compiler_convertShaderBufferDXIL()::BindCount is only allowed as 1 with CBuffers"
		));

clean:

	if(!s_uccess && allocated)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

Bool Compiler_convertRegisterDXIL(
	ListSHRegisterRuntime *registers,
	const D3D12_SHADER_INPUT_BIND_DESC *input,
	ID3D12FunctionReflection1 *funcRefl,
	ID3D12ShaderReflection1 *shaderRefl,
	const Allocator *alloc,
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
	U32 unknownFlags = D3D_SIF_FORCE_DWORD &~ (D3D_SIF_COMPARISON_SAMPLER | texFlags | D3D_SIF_UNUSED);

	Bool isReadTexture = input->Type == D3D_SIT_TEXTURE;

	Bool isWriteTexture =
		input->Type == D3D_SIT_UAV_RWTYPED &&
		input->Dimension >= D3D_SRV_DIMENSION_TEXTURE1D &&
		input->Dimension <= D3D_SRV_DIMENSION_TEXTURECUBEARRAY;

	ESHTexturePrimitive prim = ESHTexturePrimitive_Count;
	ESHTextureType registerType = ESHTextureType_Count;
	Bool isArray = false;
	Bool emptyBuffer = false;
	SBFile sbFile = SBFile{};

	U8 isUsedFlag = (!(input->uFlags & D3D_SIF_UNUSED)) << ESHBinaryType_DXIL;

	if(input->Type == D3D_SIT_CBUFFER) {

		if(!(input->uFlags & D3D_SIF_USERPACKED))
			retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL()::input uFlags need USERPACKED for CBuffer"));

		unknownFlags &=~ D3D_SIF_USERPACKED;
	}

	if(input->BindCount > 1)
		gotoIfError3(clean, ListU32_createRefConst(&input->BindCount, 1, &arrays, e_rr));

	if(input->uFlags & unknownFlags)
		retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL()::input uFlags can't be set"));

	if((input->uFlags & D3D_SIF_COMPARISON_SAMPLER) && input->Type != D3D_SIT_SAMPLER)
		retError(clean, Error_invalidState(
			0, "Compiler_convertRegisterDXIL()::input uFlags can't be comparison sampler without being a sampler itself"
		));

	if((input->uFlags & texFlags) && !isReadTexture && !isWriteTexture)
		retError(clean, Error_invalidState(
			0, "Compiler_convertRegisterDXIL()::input uFlags texture component 0 and 1 can't be set on a non texture"
		));

	if(isReadTexture || isWriteTexture) {

		switch (input->ReturnType) {

			case D3D_RETURN_TYPE_UNORM:  prim = ESHTexturePrimitive_UNorm;  break;
			case D3D_RETURN_TYPE_SNORM:  prim = ESHTexturePrimitive_SNorm;  break;
			case D3D_RETURN_TYPE_UINT:   prim = ESHTexturePrimitive_UInt;   break;
			case D3D_RETURN_TYPE_SINT:   prim = ESHTexturePrimitive_SInt;   break;
			case D3D_RETURN_TYPE_FLOAT:  prim = ESHTexturePrimitive_Float;  break;

			default:
				retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL()::input returnType unsupported"));
		}

		prim = (ESHTexturePrimitive)(prim | (((input->uFlags >> 2) & 3) << 4));

		if(input->NumSamples && input->NumSamples != U32_MAX)
			retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL() num samples must be U32_MAX or 0"));

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
				retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL() unknown texture register type"));
		}

		if((input->NumSamples != U32_MAX) != (registerType == ESHTextureType_Texture2DMS))
			retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL() num samples not matching expectation"));
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
				emptyBuffer,
				alloc,
				&sbFile,
				e_rr
			));

			if(input->Type != D3D_SIT_CBUFFER && input->NumSamples != sbFile.bufferSize)
				retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL() NumSamples doesn't match buffer size"));

		default:
			break;
	}

	if (emptyBuffer) {
		Log_warnLn(alloc, "Compiler_convertShaderBufferDXIL() Buffer with no variables detected");
		goto clean;
	}

	switch (input->Type) {

		case D3D_SIT_TEXTURE:

			gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
				registers,
				registerType,
				isArray,
				false,
				isUsedFlag,
				prim,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			));

			break;

		case D3D_SIT_SAMPLER:

			if(input->ReturnType || input->NumSamples || input->Dimension != D3D_SRV_DIMENSION_UNKNOWN)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() sampler had invalid return type, sampleCount or dimension"
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				isUsedFlag,
				input->uFlags & D3D_SIF_COMPARISON_SAMPLER,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			));

			break;

		case D3D_SIT_UAV_RWTYPED:

			if(!isWriteTexture)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterDXIL() RWBuffer is unsupported"        //TODO:?
				));

			switch(registerType) {

				case ESHTextureType_Texture3D:
				case ESHTextureType_TextureCube:
					retError(clean, Error_invalidState(
						0, "Compiler_convertRegisterDXIL() RWTexture3D and RWTextureCube don't exist"
					));

				default:
					break;
			}

			gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
				registers,
				registerType,
				isArray,
				isUsedFlag,
				prim,
				ETextureFormatId_Undefined,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			));

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
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_ByteAddressBuffer,
				input->Type == D3D_SIT_UAV_RWBYTEADDRESS,
				isUsedFlag,
				&name,
				arrays.length ? &arrays : NULL,
				NULL,
				bindings,
				alloc,
				e_rr
			));

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
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_ConstantBuffer,
				false,
				isUsedFlag,
				&name,
				NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			));

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
				));

			ESHBufferType type =
				input->Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ? ESHBufferType_StructuredBufferAtomic :
				ESHBufferType_StructuredBuffer;

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				type,
				input->Type != D3D_SIT_STRUCTURED,
				isUsedFlag,
				&name,
				arrays.length ? &arrays : NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			));

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
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_AccelerationStructure,
				false,
				isUsedFlag,
				&name,
				arrays.length ? &arrays : NULL,
				NULL,
				bindings,
				alloc,
				e_rr
			));

			break;

		case D3D_SIT_UAV_APPEND_STRUCTURED:        //Append and consume are always reported as SBuffer with atomic counter
		case D3D_SIT_UAV_CONSUME_STRUCTURED:

		case D3D_SIT_TBUFFER:
		case D3D_SIT_UAV_FEEDBACKTEXTURE:
			//TODO:
			retError(clean, Error_unsupportedOperation(0, "Compiler_convertRegisterDXIL() unsupported input type"));

		default:
			retError(clean, Error_invalidState(0, "Compiler_convertRegisterDXIL() unknown input type"));
	}

clean:
	SBFile_free(&sbFile, alloc);
	return s_uccess;
}
