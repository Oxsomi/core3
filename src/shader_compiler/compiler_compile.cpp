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

//shader_compiler/compiler_compile.cpp

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

Bool Compiler_compile(
	const Compiler *comp,
	const CompilerSettings *settings,
	const SHBinaryIdentifier *toCompile,
	const Allocator *alloc,
	CompileResult *result,
	Error *e_rr
) {

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;

	Bool s_uccess = true;
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *error = NULL;
	IDxcBlob *resultBlob = NULL;
	Bool hasErrors = false;
	CharString tempStr = CharString_createNull();
	CharString tempStr1 = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	CharString tmpFile = CharString_createNull();
	ListCharString stringsUTF8 = ListCharString{};        //One day, Microsoft will fix their stuff, I hope.

	Bool requiresLink = toCompile->uniforms.length || (settings->isLib && settings->containsGfxOrComp);

	Compiler_defineStrings;

	if(!interfaces->utils || !result)
		retError(clean, Error_alreadyDefined(!interfaces->utils ? 0 : 2, "Compiler_compile()::comp is required"));

	if(!CharString_length(settings->string))
		retError(clean, Error_invalidParameter(1, 0, "Compiler_compile()::settings->string is required"));

	if(toCompile->defines.length & 1)
		retError(clean, Error_invalidParameter(2, 0, "Compiler_compile()::toCompile->defines.length should be aligned to 2"));

	if(settings->outputType >= ESHBinaryType_Count || settings->format >= ECompilerFormat_Count)
		retError(clean, Error_invalidParameter(1, 0, "Compiler_compile()::settings contains invalid format or outputType"));

	gotoIfError3(clean, Compiler_setupIncludePaths(&stringsUTF8, settings, alloc, e_rr));

	try {

		Compiler_resetIncludeHandler(interfaces->includeHandler);        //Ensure we don't reuse stale caches

		result->isSuccess = false;

		U32 lastExtension = 0;

		for(U32 i = 0; i < ESHExtension_Count; ++i)
			if ((toCompile->extensions >> i) & 1)
				lastExtension = i + 1;

		Bool isRt = !!(toCompile->extensions & ESHExtension_RayQuery);

		if(settings->isRt) {
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC_EXT_RAYTRACING", alloc, e_rr));
			isRt = true;
		}

		//keep-all keeps unused resources bound and reflected (D3D_SIF_UNUSED); reserve-all only reserves their slots.
		gotoIfError3(clean, Compiler_registerArgCStr(
			&stringsUTF8,
			settings->keepUnusedRegisters ?
			"-fhlsl-unused-resource-bindings=keep-all" : "-fhlsl-unused-resource-bindings=reserve-all",
			alloc, e_rr
		));

		//Libraries are linked (e.g. raytracing pipelines) before their reflection is read.
		//DXC moves reflection metadata out of the DXIL part into a separate STAT part by default, and the linker
		//keeps only the DXIL parts, so the linked module reflects every cbuffer with 0 variables.
		//Keep reflection in the DXIL part for libraries so the struct annotations survive linking.
		if(settings->isLib && settings->outputType == ESHBinaryType_DXIL)
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Qkeep_reflect_in_dxil", alloc, e_rr));

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Zpc", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-O3", alloc, e_rr));

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-HV", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "202x", alloc, e_rr));

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Wconversion", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Wdouble-promotion", alloc, e_rr));

		//-Zi is here for the linked DXIL path, which needs the extra metadata to survive linking.
		//On SPIRV it means something else entirely: with no -fspv-debug= to narrow it down, DXC turns on EVERY
		// debug category (see "By default turn on all categories" in HLSLOptions.cpp), debugInfoSource
		// included, which makes the emitter read each OpSource file off disk to embed its text.
		//Every include of ours is virtual, so each of those reads fails and DXC throws an exception it catches
		// itself, once per include, and a release SPIRV binary ends up carrying the whole source for nothing.
		//So SPIRV only gets it when debug info was actually asked for, where line 168 also narrows it down.

		if(settings->debug || (requiresLink && settings->outputType != ESHBinaryType_SPIRV)) {
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Zi", alloc, e_rr));
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Qembed_debug", alloc, e_rr));
		}

		if(toCompile->extensions & ESHExtension_16BitTypes)
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-enable-16bit-types", alloc, e_rr));

		if (settings->outputType == ESHBinaryType_SPIRV) {

			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-spirv", alloc, e_rr));
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-use-dx-layout", alloc, e_rr));

			//The SPIRV backend drops unused bindings regardless of the unused-resource-bindings mode, so preserve them too

			if(settings->keepUnusedRegisters)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-preserve-bindings", alloc, e_rr));

			if(settings->debug)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-debug=vulkan-with-source", alloc, e_rr));

			ESHExtension vk13 = (ESHExtension) (
				ESHExtension_CoopVec | ESHExtension_CoopMat | ESHExtension_CoopFP8 | ESHExtension_CoopVecTraining
			);

			Bool isCoop = !!(toCompile->extensions & vk13);
			Bool isMeshTask =
				toCompile->stageType == ESHPipelineStage_MeshExt ||
				toCompile->stageType == ESHPipelineStage_TaskExt;

			const C8 *targetEnvArg;

			switch(Compiler_requiredSpirvVersion(isRt, isCoop, isMeshTask)) {
				case ESpirvVersion_1_6:    targetEnvArg = "-fspv-target-env=vulkan1.3";          break;    //SPIR-V 1.6
				case ESpirvVersion_1_4:    targetEnvArg = "-fspv-target-env=vulkan1.1spirv1.4";  break;    //SPIR-V 1.4
				default:                   targetEnvArg = "-fspv-target-env=vulkan1.1";          break;    //SPIR-V 1.3
			}

			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, targetEnvArg, alloc, e_rr));
				
			if(
				toCompile->stageType == ESHPipelineStage_Vertex ||
				toCompile->stageType == ESHPipelineStage_Domain ||
				toCompile->stageType == ESHPipelineStage_GeometryExt ||
				toCompile->stageType == ESHPipelineStage_MeshExt ||
				settings->isLib
			)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-invert-y", alloc, e_rr));

			if(toCompile->stageType == ESHPipelineStage_Pixel || settings->isLib)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-use-dx-position-w", alloc, e_rr));

			if(CharString_length(toCompile->entrypoint))
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-entrypoint-name=main", alloc, e_rr));

			gotoIfError3(clean, Compiler_registerArgCStr(
				&stringsUTF8, "-fspv-extension=SPV_EXT_descriptor_indexing", alloc, e_rr
			));

			if(
				toCompile->stageType >= ESHPipelineStage_RtStartExt &&
				toCompile->stageType <= ESHPipelineStage_RtEndExt
			)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_ray_tracing", alloc, e_rr
				));

			if(
				toCompile->stageType == ESHPipelineStage_MeshExt ||
				toCompile->stageType == ESHPipelineStage_TaskExt
			)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_EXT_mesh_shader", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_ComputeDeriv)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_compute_shader_derivatives", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_16BitTypes)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_16bit_storage", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_Multiview)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_multiview", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_RayMotionBlur)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_ray_tracing_motion_blur", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_RayQuery)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_ray_query", alloc, e_rr
				));

			//Full bindless (ResourceDescriptorHeap/SamplerDescriptorHeap lowered to SPV_EXT_descriptor_heap).
			//-fspv-use-descriptor-heap opts into the real heap lowering (the default is emulation through
			// descriptor indexing runtime arrays, which OxC3 doesn't accept as registers).
			//The -fspv-extension list is an allow list, so the extension must also be added for it to be legal.

			if(toCompile->extensions & ESHExtension_DescriptorHeap) {

				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-use-descriptor-heap", alloc, e_rr));

				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_EXT_descriptor_heap", alloc, e_rr
				));

				//The heap lowering accesses the heaps through OpUntypedAccessChainKHR

				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_untyped_pointers", alloc, e_rr
				));
			}

			//NOTE: F32/F64 atomics have no native HLSL intrinsic and are expressed via inline SPIR-V
			// ([[vk::ext_extension("SPV_EXT_shader_atomic_float_add")]] on the atomic function).
			//That inline attribute already declares the extension,
			// and passing -fspv-extension=SPV_EXT_shader_atomic_float_add makes DXC fail with "unknown SPIR-V extension"
			// (it isn't in DXC's -fspv-extension whitelist).
			//So we deliberately do NOT whitelist it here; DXC emits OpAtomicFAddEXT from the inline declaration.
		}

		else {

			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-auto-binding-space", alloc, e_rr));
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "0", alloc, e_rr));

			if(toCompile->extensions & ESHExtension_PAQ)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-enable-payload-qualifiers", alloc, e_rr));
		}

		//-E <entrypointName>

		if (CharString_length(toCompile->entrypoint)) {
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-E", alloc, e_rr));
			gotoIfError3(clean, Compiler_registerArgStrConst(&stringsUTF8, toCompile->entrypoint, alloc, e_rr));
		}

		//-T <target>

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-T", alloc, e_rr));

		const C8 *targetPrefix = ESHPipelineStage_getStagePrefix(
			settings->isLib ? ESHPipelineStage_Count : (ESHPipelineStage) toCompile->stageType
		);

		U32 major = toCompile->shaderVersion >> 8;
		U32 minor = (U8)toCompile->shaderVersion;

		gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr, "%s_%" PRIu32"_%" PRIu32, targetPrefix, major, minor));
		gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr));
		tempStr = CharString_createNull();

		//$$<X> foreach uniform
		//Define $$<X> as a real preprocessor macro for BOTH backends, so `#ifdef $$<X>` works either way.
		//DXIL points it at the function export, SPIRV at the spec-constant global (both named $$specConst_<X>).

		Bool isDXIL = settings->outputType == ESHBinaryType_DXIL;

		for (U32 i = 0; i < toCompile->uniforms.length; ++i) {

			CharString uniformName = toCompile->uniforms.ptr[i].name;

			gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr,

				isDXIL ? "-D$$%.*s=($$specConst_%.*s())" : "-D$$%.*s=$$specConst_%.*s",

				(int) CharString_length(uniformName),
				uniformName.ptr,

				(int) CharString_length(uniformName),
				uniformName.ptr
			));

			gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr));
			tempStr = CharString_createNull();
		}

		//Add exports or spec constants to input
		//SPIRV:
		//#line 1 "Spec constants (SPIRV)"
		//[[vk::constant_id(N)]] const T $$specConst_%.*s = (zero);
		//#line 1
		//DXIL:
		//#line 1 "Spec constants (DXIL)"
		//T $$specConst_%.*s();
		//#line 1

		if (toCompile->uniforms.length) {

			gotoIfError3(clean, CharString_createCopy(
				CharString_createRefCStrConst(
					isDXIL ? "#line 1 \"Spec constants (DXIL)\"\n" : "#line 1 \"Spec constants (SPIRV)\"\n"
				),
				alloc,
				&tmpFile,
				e_rr
			));

			Bool has16Bit = toCompile->extensions & ESHExtension_16BitTypes;
			Bool hasInt64 = toCompile->extensions & ESHExtension_I64;
			Bool hasF64   = toCompile->extensions & ESHExtension_F64;

			EHLSLStringifyFlags flags = EHLSLStringifyFlags_None;

			if(has16Bit)
				flags = EHLSLStringifyFlags(flags | EHLSLStringifyFlags_Has16Bit);

			if(hasF64)
				flags = EHLSLStringifyFlags(flags | EHLSLStringifyFlags_HasF64);

			if(hasInt64)
				flags = EHLSLStringifyFlags(flags | EHLSLStringifyFlags_HasI64);

			for (U64 i = 0; i < toCompile->uniforms.length; ++i) {

				SHUniformRuntime uniform = toCompile->uniforms.ptr[i];
				TypeId type = ETypeId_arr[uniform.typeIdShort];

				gotoIfError3(clean, CharString_createFromETypeIdHLSL(type, flags, alloc, &tempStr, e_rr));

				if(isDXIL) {
					gotoIfError3(clean, CharString_format(alloc, &tempStr1, e_rr, "%s $$specConst_%.*s();\n",
						tempStr.ptr,
						(int) CharString_length(uniform.name), uniform.name.ptr
					));
				}

				else {

					SHValue value = SHValue{};
					gotoIfError3(clean, SHValue_stringifyHLSL(&value, type, flags, alloc, &tempStr2, e_rr));

					gotoIfError3(clean, CharString_format(
						alloc, &tempStr1, e_rr,
						"[[vk::constant_id(%" PRIu64 ")]] const %s $$specConst_%.*s = %s;\n",
						i,
						tempStr.ptr,
						(int) CharString_length(uniform.name), uniform.name.ptr,
						tempStr2.ptr
					));
				}

				gotoIfError3(clean, CharString_appendString(&tmpFile, &tempStr1, alloc, e_rr));
				CharString_free(&tempStr, alloc);
				CharString_free(&tempStr1, alloc);
				CharString_free(&tempStr2, alloc);
			}

			{
				const CharString lineStr = CharString_createRefCStrConst("#line 1\n");
				gotoIfError3(clean, CharString_appendString(&tmpFile, &lineStr, alloc, e_rr));
			}
			gotoIfError3(clean, CharString_appendString(&tmpFile, &settings->string, alloc, e_rr));
		}

		//$<X> foreach define

		for(U32 i = 0; i < toCompile->defines.length; i += 2) {

			CharString defineName  = toCompile->defines.ptr[i];
			CharString defineValue = toCompile->defines.ptr[i + 1];

			gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr,

				!CharString_length(defineValue) ? "-D$%.*s" : "-D$%.*s=%.*s",

				(int) CharString_length(defineName),
				defineName.ptr,

				(int) CharString_length(defineValue),
				defineValue.ptr
			));

			gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr));
			tempStr = CharString_createNull();
		}

		//__OXC_EXT_<X> foreach extension

		for(U32 i = 0; i < lastExtension; ++i)
			if ((toCompile->extensions >> i) & 1) {
				gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr, "-D__OXC_EXT_%s", ESHExtension_defines[i]));
				gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr));
				tempStr = CharString_createNull();
			}

		//Format major, minor, patch and version

		const C8 *formats[] = {
			"-D__OXC_MAJOR=%" PRIu64,
			"-D__OXC_MINOR=%" PRIu64,
			"-D__OXC_PATCH=%" PRIu64,
			"-D__OXC_VERSION=%" PRIu64,
		};

		const U64 formatInts[] = {
			OXC3_MAJOR,
			OXC3_MINOR,
			OXC3_PATCH,
			OXC3_VERSION
		};

		for(U64 i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
			gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr, formats[i], formatInts[i]));
			gotoIfError3(clean, ListCharString_pushBack(&stringsUTF8, tempStr, alloc, e_rr));
			tempStr = CharString_createNull();
		}

		Compiler_convertToWString(stringsUTF8, clean)

		//Compile

		DxcBuffer buffer{
			.Ptr = tmpFile.ptr ? tmpFile.ptr : settings->string.ptr,
			.Size = tmpFile.ptr ? CharString_length(tmpFile) : CharString_length(settings->string),
			.Encoding = DXC_CP_UTF8
		};

		HRESULT hr = interfaces->compiler->Compile(
			&buffer,
			(LPCWSTR*) strings.ptr, (int) strings.length,
			Compiler_getIncludeHandler(interfaces->includeHandler),
			IID_PPV_ARGS(&dxcResult)
		);

		if(FAILED(hr))
			retError(clean, Error_invalidState(0, "Compiler_compile() \"Compile\" failed"));

		hr = dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), NULL);

		if(FAILED(hr))
			retError(clean, Error_invalidState(1, "Compiler_compile() fetch errors failed"));

		if(error && error->GetStringLength()) {
			CharString errs = CharString_createRefSizedConst(error->GetStringPointer(), error->GetStringLength(), false);
			gotoIfError3(clean, Compiler_parseErrors(errs, alloc, &result->compileErrors, &hasErrors, e_rr));
		}

		if(error) {
			error->Release();
			error = NULL;
		}

		if (hasErrors)
			goto clean;

		hr = dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&resultBlob), NULL);

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_compile() fetch hlsl failed"));

			//Fail loudly rather than silently emitting an empty binary
			// that only trips a confusing SHFile_read error much later.
			//DXC can produce no object without a reported error (e.g. a DXIL validation reject of a still-experimental op),
			// so guard against a zero-length blob explicitly.
			if(!resultBlob || !resultBlob->GetBufferSize())
				retError(clean, Error_invalidState(
					3, "Compiler_compile() DXC produced an empty binary (compile or validation failed silently)"
				));

		gotoIfError3(clean, Buffer_createCopy(
			Buffer_createRefConst(resultBlob->GetBufferPointer(), resultBlob->GetBufferSize()),
			alloc,
			&result->binary,
			e_rr
		));

		if (settings->infoAboutIncludes)
			gotoIfError3(clean, Compiler_copyIncludes(result, interfaces->includeHandler, alloc, e_rr));

		result->type = ECompileResultType_Binary;
		result->isSuccess = true;

	} catch (std::exception&) {
		retError(clean, Error_invalidState(1, "Compiler_compile() raised an internal exception"));
	}

clean:

	if(!s_uccess && result)
		CompileResult_free(result, alloc);

	if(resultBlob)
		resultBlob->Release();

	if(dxcResult)
		dxcResult->Release();

	if(error)
		error->Release();

	Compiler_freeStrings;
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr1, alloc);
	CharString_free(&tempStr2, alloc);
	CharString_free(&tmpFile, alloc);
	ListCharString_freeUnderlying(&stringsUTF8, alloc);
	return s_uccess;
}
