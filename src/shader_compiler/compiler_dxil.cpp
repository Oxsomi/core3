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

//shader_compiler/compiler_dxil.cpp

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

static inline Bool DxilMapToESHExtension(U64 flags, ESHExtension *ext, ESHExtension *demotion, Error *e_rr) {

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
		//DXC also reports ADVANCED_TEXTURE_OPS for an RWTexture2DMS write (it's the parent SM6.7 feature).
		//OxC3 has no dedicated bit for it, so fold it into WriteMSTexture, which is the use case that trips it.
		D3D_SHADER_REQUIRES_ADVANCED_TEXTURE_OPS,
		D3D_SHADER_REQUIRES_WAVE_OPS,
		//SM6.6 dynamic resources; both heap indexing flags fold into the one DescriptorHeap extension.
		D3D_SHADER_REQUIRES_RESOURCE_DESCRIPTOR_HEAP_INDEXING,
		D3D_SHADER_REQUIRES_SAMPLER_DESCRIPTOR_HEAP_INDEXING,
		D3D_SHADER_REQUIRES_BARYCENTRICS
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
		ESHExtension_WriteMSTexture,        //ADVANCED_TEXTURE_OPS folded into WriteMSTexture (see above)
		ESHExtension_SubgroupOperations,
		ESHExtension_DescriptorHeap,
		ESHExtension_DescriptorHeap,
		ESHExtension_Barycentrics
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
		retError(clean, Error_unsupportedOperation(0, "DxilMapToESHExtension() contained an unsupported extension"));

clean:
	return s_uccess;
}

static inline Bool Compiler_convertWaveSizeParam(U32 threads, U8 *threadsShort, Error *e_rr) {

	Bool s_uccess = true;

	switch (threads) {
		case 0:    *threadsShort = 0;  break;
		case 1:    *threadsShort = 1;  break;
		case 2:    *threadsShort = 2;  break;
		case 4:    *threadsShort = 3;  break;
		case 8:    *threadsShort = 4;  break;
		case 16:   *threadsShort = 5;  break;
		case 32:   *threadsShort = 6;  break;
		case 64:   *threadsShort = 7;  break;
		case 128:  *threadsShort = 8;  break;
		case 256:  *threadsShort = 9;  break;
		default:
			retError(clean, Error_invalidParameter(threads, 0, "Compiler_convertWaveSizeParam()::threads is invalid"));
	}

clean:
	return s_uccess;
}

static inline Bool Compiler_convertWaveSize(
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
		waveSizeRecommendedTmp = (waveSizeMinTmp + waveSizeMaxTmp) / 2;        //Not base2, but don't care

	if(
		U32_min(waveSizeMinTmp, U32_min(waveSizeMaxTmp, waveSizeRecommendedTmp)) < 4 ||
		U32_max(waveSizeMinTmp, U32_max(waveSizeMaxTmp, waveSizeRecommendedTmp)) > 128
	)
		retError(clean, Error_invalidState(0, "Compiler_convertWaveSize() couldn't get groupSize; out of bounds"));

	gotoIfError3(clean, Compiler_convertWaveSizeParam(waveSizeRecommended, &recommend, e_rr));
	gotoIfError3(clean, Compiler_convertWaveSizeParam(waveSizeMin, &waveMin, e_rr));
	gotoIfError3(clean, Compiler_convertWaveSizeParam(waveSizeMax, &waveMax, e_rr));

	if(waveSizeMin || waveSizeMax)
		*waveSizes = ((U16)waveSizeMin << 4) | ((U16)waveSizeMax << 8) | ((U16)recommend << 12);

	else *waveSizes = recommend;

clean:
	return s_uccess;
}

//Compiler_convertRegisterDXIL is implemented in compiler_dxil_reflect.cpp
Bool Compiler_convertRegisterDXIL(
	ListSHRegisterRuntime *registers,
	const D3D12_SHADER_INPUT_BIND_DESC *input,
	ID3D12FunctionReflection1 *funcRefl,
	ID3D12ShaderReflection1 *shaderRefl,
	const Allocator *alloc,
	Error *e_rr
);

class IncludeHandler;

typedef struct CompilerInterfaces {
	IDxcUtils *utils;
	IDxcCompiler3 *compiler;
	IncludeHandler *includeHandler;
	IHLSLReflector *reflector;
} CompilerInterfaces;

extern "C" Bool Compiler_processDXIL(
	const Compiler *compiler,
	Buffer *result,
	ListSHRegisterRuntime *registers,
	Bool isDebug,
	const SHBinaryIdentifier *toCompile,
	SpinLock *lock,
	const ListSHEntryRuntime *entries,
	ESHExtension *demotions,
	ListCompileError *errors,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ID3D12ShaderReflection1 *dxilRefl{};
	ID3D12LibraryReflection1 *dxilReflLib{};
	IDxcBlobEncoding *finalShader{};
	IDxcBlob *finalVersion{};
	IDxcContainerBuilder *containerBuilder{};
	IDxcContainerReflection *containerReflection{};
	IDxcOperationResult *opResult{};
	IDxcBlobEncoding *err{};

	HRESULT hr = S_OK;

	Bool isLib = !CharString_length(toCompile->entrypoint);
	ESHExtension exts = ESHExtension_None;
	ListCharString strings{};
	U8 inputSemanticCount = 0;
	CompilerInterfaces *interfaces = (CompilerInterfaces*) compiler->interfaces;

	DxcBuffer inputBuf = DxcBuffer{};

	if(!demotions || !result || !registers)
		retError(clean, Error_nullPointer(0, "Compiler_processDXIL() demotions, result and registers are required"));

	inputBuf = DxcBuffer{ .Ptr = result->ptr, .Size = Buffer_length(*result), .Encoding = 0 };

	if(isLib && FAILED(hr = interfaces->utils->CreateReflection(&inputBuf, IID_PPV_ARGS(&dxilReflLib)))) {
		retError(clean, Error_invalidState(0, "Compiler_processDXIL() lib reflection is invalid"));
	}

	else if (!isLib && FAILED(hr = interfaces->utils->CreateReflection(&inputBuf, IID_PPV_ARGS(&dxilRefl))))
		retError(clean, Error_invalidState(0, "Compiler_processDXIL() shader reflection is invalid"));

	//Payload / intersection data reflection

	if (isLib) {

		D3D12_LIBRARY_DESC lib = D3D12_LIBRARY_DESC{};
		if(FAILED(dxilReflLib->GetDesc(&lib)))
			retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get D3D12_LIBRARY_DESC"));

		for(U64 i = 0; i < lib.FunctionCount; ++i) {

			ID3D12FunctionReflection1 *funcRefl = NULL;

			if ((i >> 31) || (funcRefl = dxilReflLib->GetFunctionByIndex1((INT)i)) == NULL)
				retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get ID3D12FunctionReflection"));

			D3D12_FUNCTION_DESC funcDesc0 = D3D12_FUNCTION_DESC{};
			D3D12_FUNCTION_DESC1 funcDesc = D3D12_FUNCTION_DESC1{};
			if(FAILED(funcRefl->GetDesc(&funcDesc0)) || FAILED(funcRefl->GetDesc1(&funcDesc)))
				retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get D3D12_FUNCTION_DESC1"));

			U64 featureFlags = funcRefl->GetRequiresFlags();
			ESHExtension demote = ESHExtension_None;
			gotoIfError3(clean, DxilMapToESHExtension(featureFlags, &exts, &demote, e_rr));

			if(i)
				*demotions = (ESHExtension)(*demotions & demote);

			else *demotions = demote;

			U8 attributeSize = 0;
			U8 payloadSize = 0;
			U32 groupSize[3] = { 0 };
			U16 waveSizes = 0;
			Bool hasGroupSize = false;

			switch (funcDesc.ShaderType) {
				case D3D12_SHVER_PIXEL_SHADER:
				case D3D12_SHVER_VERTEX_SHADER:
				case D3D12_SHVER_GEOMETRY_SHADER:
				case D3D12_SHVER_HULL_SHADER:
				case D3D12_SHVER_DOMAIN_SHADER:
				case D3D12_SHVER_COMPUTE_SHADER:
				case D3D12_SHVER_MESH_SHADER:
				case D3D12_SHVER_AMPLIFICATION_SHADER:
					retError(clean, Error_invalidState(
						0,
						"Compiler_processDXIL() "
						"Hull, domain, compute, mesh, amplification, compute, geometry, vertex or pixel shaders have to be "
						"finalized through linking before adding to oiSH file"
					));

				//Library / raytracing / node / callable stages are handled below (they don't require prior linking)
				default:
					break;
			}

			//Reflect payload size & attribute size

			if (
				funcDesc.ShaderType > D3D12_SHVER_RAY_GENERATION_SHADER &&
				funcDesc.ShaderType <= D3D12_SHVER_CALLABLE_SHADER
			) {

				if(funcDesc.RaytracingShader.ParamPayloadSize > 128)
					retError(clean, Error_outOfBounds(
						0, funcDesc.RaytracingShader.ParamPayloadSize, 255, "Compiler_processDXIL() payload out of bounds"
					));

				payloadSize = (U8) funcDesc.RaytracingShader.ParamPayloadSize;

				if(funcDesc.RaytracingShader.AttributeSize > 32)
					retError(clean, Error_outOfBounds(
						0, funcDesc.RaytracingShader.AttributeSize, 32,
						"Compiler_processDXIL() attribute out of bounds"
					));

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
				));
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
				gotoIfError3(clean, Compiler_validateGroupSize(groupSize, e_rr));

			ESBType inputs[16] = {};        //TODO:
			ESBType outputs[16] = {};
			U8 uniqueInputSemantics = 0;
			ListCharString uniqueSemantics = ListCharString{};
			U8 inputSemantics[16] = {};
			U8 outputSemantics[16] = {};

			if(!funcDesc0.Name)
				retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL contained no library name"));

			for (U32 j = 0; j < funcDesc0.ConstantBuffers; ++j) {        //Validate buffers

				ID3D12ShaderReflectionConstantBuffer *constantBuffer = funcRefl->GetConstantBufferByIndex(j);
				D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
				D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};

				if(FAILED(constantBuffer->GetDesc(&constantBufferDesc)))
					retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL contained constant buffer but no desc"));

				if(funcRefl && FAILED(funcRefl->GetResourceBindingDescByName(constantBufferDesc.Name, &resourceDesc)))
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() DXIL didn't contain resource binding for constant buffer"
					));

				switch (resourceDesc.Type) {

					case D3D_SIT_CBUFFER:
					case D3D_SIT_STRUCTURED:
					case D3D_SIT_UAV_RWSTRUCTURED:
					case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
						break;

					default:
						retError(clean, Error_invalidState(
							1, "Compiler_processDXIL() DXIL contained buffer description not bound to a valid resource type"
						));
				}
			}

			for (U32 j = 0; j < funcDesc0.BoundResources; ++j) {

				D3D12_SHADER_INPUT_BIND_DESC input{};
				if(FAILED(funcRefl->GetResourceBindingDesc(j, &input)))
					retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL contained invalid resource"));

				gotoIfError3(clean, Compiler_convertRegisterDXIL(registers, &input, funcRefl, NULL, alloc, e_rr));
			}

			CharString demangled = CharString_createRefCStrConst(funcDesc0.Name);

			if (funcDesc0.Name[0] == '\x1') {    //Mangled

				U64 firstAt = CharString_findFirstSensitive(&demangled, '@', 2, 0);

				if(funcDesc0.Name[1] != '?' || firstAt == U64_MAX)
					retError(clean, Error_invalidState(0, "Compiler_processDXIL() DXIL had invalid name mangling"));

				demangled = CharString_createRefSizedConst(demangled.ptr + 2, firstAt - 2, false);
			}

			gotoIfError3(clean, Compiler_finalizeEntrypoint(
				groupSize, payloadSize, attributeSize, waveSizes,
				inputs, outputs,
				uniqueInputSemantics, &uniqueSemantics, inputSemantics, outputSemantics,
				&demangled, lock, entries,
				alloc, e_rr
			));
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
			retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get D3D12_LIBRARY_DESC"));

		U64 reqFlags = dxilRefl->GetRequiresFlags();
		gotoIfError3(clean, DxilMapToESHExtension(reqFlags, &exts, demotions, e_rr));

		Bool isPixelShader = toCompile->stageType == ESHPipelineStage_Pixel;

		if (
			toCompile->stageType == ESHPipelineStage_Compute ||
			toCompile->stageType == ESHPipelineStage_MeshExt ||
			toCompile->stageType == ESHPipelineStage_TaskExt
		) {
			dxilRefl->GetThreadGroupSize(&groupSize[0], &groupSize[1], &groupSize[2]);
			gotoIfError3(clean, Compiler_validateGroupSize(groupSize, e_rr));
		}

		if (toCompile->stageType == ESHPipelineStage_Compute) {

			U32 waveSizeRecommended, waveSizeMin, waveSizeMax;
			dxilRefl->GetWaveSize(&waveSizeRecommended, &waveSizeMin, &waveSizeMax);

			gotoIfError3(clean, Compiler_convertWaveSize(
				waveSizeRecommended, waveSizeMin, waveSizeMax, &waveSizes, e_rr
			));
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
				));

			if(!isPixelShader && signature.SystemValueType != D3D_NAME_UNDEFINED)
				continue;

			//A pixel shader's inputs are ordinary user semantics (interpolated attributes), like any other stage.
			//Only its outputs are restricted to SV_TARGET; applying that filter to inputs too dropped every pixel
			// input (they're D3D_NAME_UNDEFINED, never D3D_NAME_TARGET), so SPIRV and DXIL reflection disagreed.
			if(isPixelShader && isOutput && signature.SystemValueType != D3D_NAME_TARGET)
				continue;

			if(isPixelShader && !isOutput && signature.SystemValueType != D3D_NAME_UNDEFINED)
				continue;

			//SPIRV dead-code-eliminates a fully-unused input, so it isn't in the SPIRV binary at all;
			// DXIL instead keeps it in the signature (ReadWriteMask == 0, nothing read).
			//Skip it for DXIL too so a shader that declares but never reads an input reflects identically on both backends.
			if(!isOutput && !signature.ReadWriteMask)
				continue;

			if(signature.SemanticIndex >= 16)
				retError(clean, Error_invalidState(
					0, "Compiler_processDXIL() input location out of bounds (allowed up to 16)"
				));

			U8 semanticValue = 0;

			CharString semanticNameStr = CharString_createRefCStrConst(signature.SemanticName);

			//A pixel output's default semantic is SV_TARGET; every other param (all inputs, non-pixel outputs)
			// defaults to TEXCOORD.
			//Only a non-default name is recorded, so pixel inputs must go through here too; otherwise they'd
			// report as the default TEXCOORD instead of their real semantic.
			if (!(isPixelShader && isOutput) && !CharString_equalsCStringInsensitive(&semanticNameStr, "TEXCOORD")) {

				U64 start = isOutput ? inputSemanticCount : 0;
				U64 end = isOutput ? strings.length : inputSemanticCount;
				U64 k = start;

				for(; k < end; ++k)
					if(CharString_equalsStringInsensitive(&strings.ptr[k], &semanticNameStr))
						break;

				U64 semanticName = (k - start) + 1;

				if(semanticName >= 16)
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() unique semantic name out of bounds"
					));

				if(k == end) {    //Not found, so insert

					gotoIfError3(clean, ListCharString_insert(
						&strings, k, CharString_createRefCStrConst(signature.SemanticName), alloc, e_rr
					));

					if(!isOutput)
						++inputSemanticCount;
				}

				semanticValue = (U8)((semanticName << 4) | signature.SemanticIndex);
			}

			else semanticValue = (U8) signature.SemanticIndex;

			if(signature.MinPrecision || signature.Stream)
				retError(clean, Error_invalidState(
					0, "Compiler_processDXIL() invalid signature parameter; MinPrecision or Stream"
				));

			ESBPrimitive prim = ESBPrimitive_Invalid;
			ESBStride stride = ESBStride_X8;

			switch (signature.ComponentType) {

				case  D3D_REGISTER_COMPONENT_FLOAT16:  prim = ESBPrimitive_Float;  stride = ESBStride_X16;  break;
				case  D3D_REGISTER_COMPONENT_UINT16:   prim = ESBPrimitive_UInt;   stride = ESBStride_X16;  break;
				case  D3D_REGISTER_COMPONENT_SINT16:   prim = ESBPrimitive_Int;    stride = ESBStride_X16;  break;

				case  D3D_REGISTER_COMPONENT_FLOAT32:  prim = ESBPrimitive_Float;  stride = ESBStride_X32;  break;
				case  D3D_REGISTER_COMPONENT_UINT32:   prim = ESBPrimitive_UInt;   stride = ESBStride_X32;  break;
				case  D3D_REGISTER_COMPONENT_SINT32:   prim = ESBPrimitive_Int;    stride = ESBStride_X32;  break;

				case  D3D_REGISTER_COMPONENT_FLOAT64:  prim = ESBPrimitive_Float;  stride = ESBStride_X64;  break;
				case  D3D_REGISTER_COMPONENT_UINT64:   prim = ESBPrimitive_UInt;   stride = ESBStride_X64;  break;
				case  D3D_REGISTER_COMPONENT_SINT64:   prim = ESBPrimitive_Int;    stride = ESBStride_X64;  break;
				default:
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() invalid component type; expected one of F32, U32 or I32"
					));
			}

			ESBVector vec = ESBVector_N1;

			switch (signature.Mask) {
				case  1:  vec = ESBVector_N1;  break;
				case  3:  vec = ESBVector_N2;  break;
				case  7:  vec = ESBVector_N3;  break;
				case 15:  vec = ESBVector_N4;  break;
				default:
					retError(clean, Error_invalidState(
						0, "Compiler_processDXIL() invalid signature mask; expected one of 1,3,7,15"
					));
			}

			ESBType type = (ESBType) ESBType_create(stride, prim, vec, ESBMatrix_N1);
			U64 *counter = isOutput ? &outputC : &inputC;

			if(inputTypes[*counter] || *counter >= 16)
				retError(clean, Error_invalidState(
					0, "Compiler_processDXIL() output/input location is already defined or out of bounds"
				));

			semantics[*counter] = semanticValue;
			inputTypes[(*counter)++] = type;
		}

		for (U32 j = 0; j < refl.ConstantBuffers; ++j) {        //Validate buffers

			ID3D12ShaderReflectionConstantBuffer *constantBuffer = dxilRefl->GetConstantBufferByIndex(j);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
			D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};

			if(FAILED(constantBuffer->GetDesc(&constantBufferDesc)))
				retError(clean, Error_invalidState(1, "Compiler_processDXIL() DXIL contained constant buffer but no desc"));

			if(dxilRefl && FAILED(dxilRefl->GetResourceBindingDescByName(constantBufferDesc.Name, &resourceDesc)))
				retError(clean, Error_invalidState(
					1, "Compiler_processDXIL() DXIL didn't contain resource binding for constant buffer"
				));

			switch (resourceDesc.Type) {

				case D3D_SIT_CBUFFER:
				case D3D_SIT_STRUCTURED:
				case D3D_SIT_UAV_RWSTRUCTURED:
				case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
					break;

				default:
					retError(clean, Error_invalidState(
						1, "Compiler_processDXIL() DXIL contained buffer description not bound to a valid resource type"
					));
			}
		}

		for (U32 j = 0; j < refl.BoundResources; ++j) {

			D3D12_SHADER_INPUT_BIND_DESC input{};
			if(FAILED(dxilRefl->GetResourceBindingDesc(j, &input)))
				retError(clean, Error_invalidState(1, "Compiler_processDXIL() DXIL contained invalid resource"));

			gotoIfError3(clean, Compiler_convertRegisterDXIL(registers, &input, NULL, dxilRefl, alloc, e_rr));
		}

		gotoIfError3(clean, Compiler_finalizeEntrypoint(
			groupSize, 0, 0, waveSizes,
			inputs, outputs,
			inputSemanticCount, &strings, inputSemantics, outputSemantics,
			&toCompile->entrypoint, lock, entries,
			alloc, e_rr
		));
	}

	if((toCompile->extensions & exts) != exts)
		retError(clean, Error_invalidState(
			2, "Compiler_processDXIL() DXIL contained capability that wasn't enabled by oiSH file (use annotations)"
		));

	//Strip debug info

	if(!isDebug) {

		hr = DxcCreateInstance(CLSID_DxcContainerBuilder, IID_PPV_ARGS(&containerBuilder));

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_create() IDxcContainerBuilder couldn't be created"));
		
		hr = interfaces->utils->CreateBlobFromPinned(result->ptr, (U32) Buffer_length(*result), DXC_CP_ACP, &finalShader);

		if (FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_processDXIL() Couldn't create IDxcBlob"));

		if(FAILED(containerBuilder->Load(finalShader)))
			retError(clean, Error_invalidOperation(0, "Compiler_processDXIL() Couldn't turn into container"));
	
		//All three parts are optional, and asking to remove one the container doesn't have makes DXC throw
		// DXC_E_MISSING_PART internally (DxcContainerBuilder::RemovePart).
		//DXC catches that itself and turns it into an HRESULT we were ignoring anyway, so it read as harmless,
		// but the throw is fatal under Windows ASan: the catch block faults while unwinding.
		//The container is therefore asked what it actually holds first, and only those parts are removed.

		static const U32 removableParts[] = { DXC_PART_PDB, DXC_PART_PDB_NAME, DXC_PART_REFLECTION_DATA };
		U8 removablePartCount = (U8)(sizeof(removableParts) / sizeof(removableParts[0]));

		Bool hasPart[sizeof(removableParts) / sizeof(removableParts[0])] = { false };

		hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&containerReflection));

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_processDXIL() IDxcContainerReflection couldn't be created"));

		if(FAILED(containerReflection->Load(finalShader)))
			retError(clean, Error_invalidOperation(0, "Compiler_processDXIL() Couldn't reflect container parts"));

		U32 partCount = 0;

		if(FAILED(containerReflection->GetPartCount(&partCount)))
			retError(clean, Error_invalidOperation(0, "Compiler_processDXIL() Couldn't count container parts"));

		for(U32 i = 0; i < partCount; ++i) {

			U32 partKind = 0;

			if(FAILED(containerReflection->GetPartKind(i, &partKind)))
				continue;

			for(U8 j = 0; j < removablePartCount; ++j)
				if(partKind == removableParts[j])
					hasPart[j] = true;
		}

		for(U8 j = 0; j < removablePartCount; ++j)
			if(hasPart[j])
				containerBuilder->RemovePart(removableParts[j]);

		hr = containerBuilder->SerializeContainer(&opResult);

		Bool hasErrors = false;

		if (opResult && SUCCEEDED(opResult->GetErrorBuffer(&err)) && err && err->GetBufferSize()) {
			CharString errStr = CharString_createRefSizedConst((const C8*)err->GetBufferPointer(), err->GetBufferSize(), false);
			gotoIfError3(clean, Compiler_parseErrors(errStr, alloc, errors, &hasErrors, e_rr));
		}

		if (FAILED(hr) || hasErrors)
			retError(clean, Error_invalidOperation(0, "Compiler_processDXIL() DXIL couldn't be assembled"));

		hr = opResult->GetResult(&finalVersion);

		if (FAILED(hr))
			retError(clean, Error_invalidOperation(0, "Compiler_processDXIL() DXIL couldn't be obtained"));

		if(!Buffer_resize(result, finalVersion->GetBufferSize(), false, false, alloc, e_rr))
			retError(clean, Error_invalidState(2, "Compiler_processDXIL() Couldn't allocate copy"));

		Buffer_memcpy(*result, Buffer_createRefConst(finalVersion->GetBufferPointer(), finalVersion->GetBufferSize()));
	}

	//Ensure we have a valid DXIL file

	//DXC signs DXIL through the bundled validator, which only knows shader models up to 6.9, so a 6.10 module
	// (cooperative vectors / triangle position fetch) comes back structurally valid but unsigned (zero hash).
	//D3D12 only accepts 6.10 with experimental shader models enabled anyway, so an unsigned hash is expected there.
	if(
		Buffer_length(*result) <= 0x14 ||
		Buffer_readU32(*result, 0, NULL, NULL) != C8x4('D', 'X', 'B', 'C') ||
		(toCompile->shaderVersion < OISH_SHADER_MODEL(6, 10) &&
			I32x4_eq4(I32x4_load4(result->ptr + sizeof(U32)), I32x4_zero()))                  //Unsigned
	)
		retError(clean, Error_invalidState(2, "Compiler_processDXIL() DXIL returned is invalid"));

clean:

	ListCharString_freeUnderlying(&strings, alloc);

	if(dxilRefl)
		dxilRefl->Release();

	if(finalShader)
		finalShader->Release();

	if(finalVersion)
		finalVersion->Release();

	if(containerBuilder)
		containerBuilder->Release();

	if(containerReflection)
		containerReflection->Release();

	if(opResult)
		opResult->Release();

	if(err)
		err->Release();

	if(dxilReflLib)
		dxilReflLib->Release();

	return s_uccess;
}
