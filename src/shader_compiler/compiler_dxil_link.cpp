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

//shader_compiler/compiler_dxil_link.cpp

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

class IncludeHandler;

typedef struct CompilerInterfaces {
	IDxcUtils *utils;
	IDxcCompiler3 *compiler;
	IncludeHandler *includeHandler;
	IHLSLReflector *reflector;
} CompilerInterfaces;

extern "C" Bool Compiler_disassembleDXIL(
	const Compiler *comp, Buffer buf, const Allocator *alloc, CharString *result, Error *e_rr
) {

	Bool s_uccess = true;
	U64 binLen = Buffer_length(buf);

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;
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
		Buffer_readU32(buf, 0, NULL, NULL) != C8x4('D', 'X', 'B', 'C')
	)
		retError(clean, Error_invalidState(0, "Compiler_createDisassembly() DXIL is invalid"));

	hr = interfaces->compiler->Disassemble(&buffer, IID_PPV_ARGS(&dxcResult));

	if(FAILED(hr))
		retError(clean, Error_invalidOperation(0, "Compiler_createDisassembly() DXIL couldn't be disassembled"));

	hr = dxcResult->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&blobUtf8), NULL);

	if(FAILED(hr))
		retError(clean, Error_invalidOperation(1, "Compiler_createDisassembly() DXIL disassembly couldn't be obtained"));

	str = CharString_createRefSizedConst(blobUtf8->GetStringPointer(), blobUtf8->GetStringLength(), false);
	gotoIfError3(clean, CharString_createCopy(str, alloc, result, e_rr));
	
clean:

	if(dxcResult)
		dxcResult->Release();

	if(blobUtf8)
		blobUtf8->Release();

	return s_uccess;
}

extern "C" Bool Compiler_assembleDXIL(
	const Compiler *comp, CharString text, const Allocator *alloc, Buffer *result, Error *e_rr
) {

	Bool s_uccess = true;

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;
	HRESULT hr = 0, status = 0;
	IDxcAssembler *assembler = NULL;
	IDxcBlobEncoding *textBlob = NULL;
	IDxcOperationResult *opResult = NULL;
	IDxcBlob *container = NULL;

	if(!result)
		retError(clean, Error_nullPointer(3, "Compiler_assembleDXIL()::result is required"));

	if(!CharString_length(text))
		retError(clean, Error_invalidParameter(1, 0, "Compiler_assembleDXIL()::text is empty"));

	//Wrap the LL text and assemble it into a DXIL container (AssembleToContainer accepts LL or bitcode)

	hr = interfaces->utils->CreateBlobFromPinned(text.ptr, (U32) CharString_length(text), DXC_CP_UTF8, &textBlob);
	if(FAILED(hr))
		retError(clean, Error_invalidOperation(0, "Compiler_assembleDXIL() couldn't wrap the input text"));

	hr = DxcCreateInstance(CLSID_DxcAssembler, IID_PPV_ARGS(&assembler));
	if(FAILED(hr))
		retError(clean, Error_invalidOperation(1, "Compiler_assembleDXIL() couldn't create the DXC assembler"));

	hr = assembler->AssembleToContainer(textBlob, &opResult);
	if(FAILED(hr))
		retError(clean, Error_invalidOperation(2, "Compiler_assembleDXIL() AssembleToContainer failed"));

	if(FAILED(opResult->GetStatus(&status)) || FAILED(status))
		retError(clean, Error_invalidState(0, "Compiler_assembleDXIL() DXIL text couldn't be assembled"));

	hr = opResult->GetResult(&container);
	if(FAILED(hr) || !container)
		retError(clean, Error_invalidOperation(3, "Compiler_assembleDXIL() couldn't get the assembled container"));

	gotoIfError3(clean, Buffer_createCopy(
		Buffer_createRefConst(container->GetBufferPointer(), (U64) container->GetBufferSize()), alloc, result, e_rr
	));

clean:
	if(container) container->Release();
	if(opResult) opResult->Release();
	if(assembler) assembler->Release();
	if(textBlob) textBlob->Release();
	return s_uccess;
}

extern "C" Bool Compiler_getUniqueEntrypointsDXIL(
	const Compiler *compiler,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	DxcBuffer inputBuf = DxcBuffer{ .Ptr = binary.ptr, .Size = Buffer_length(binary), .Encoding = 0 };
	HRESULT hr = S_OK;

	ID3D12LibraryReflection1 *dxilReflLib{};

	CompilerInterfaces *interfaces = (CompilerInterfaces*) compiler->interfaces;

	Bool freeEp = false;
	Bool alreadyContainsLib = false;    //Avoid re-inserting uniqueEntrypoint of lib
	D3D12_LIBRARY_DESC desc;

	if(!interfaces)
		retError(clean, Error_nullPointer(0, "Compiler_getUniqueEntrypointsDXIL() compiler is required"));

	if(!uniqueEntrypoints)
		retError(clean, Error_nullPointer(3, "Compiler_getUniqueEntrypointsDXIL() uniqueEntrypoints are required"));

	if(uniqueEntrypoints->length)
		retError(clean, Error_invalidParameter(3, 0, "Compiler_getUniqueEntrypointsDXIL() uniqueEntrypoints should be empty"));

	freeEp = true;

	if(FAILED(hr = interfaces->utils->CreateReflection(&inputBuf, IID_PPV_ARGS(&dxilReflLib))))
		retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsDXIL() reflection is invalid, is it a lib file?"));

	//Iterate through lib; we will only output linkable entries if !showAll, otherwise we will show all entrypoints

	if(FAILED(hr = dxilReflLib->GetDesc(&desc)))
		retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsDXIL() couldn't obtain lib info"));

	for(U32 i = 0; i < desc.FunctionCount; ++i) {

		ID3D12FunctionReflection1 *funcRefl = dxilReflLib->GetFunctionByIndex1(i);

		if(!funcRefl)
			retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsDXIL() couldn't obtain function"));

		D3D12_FUNCTION_DESC funcDesc;
		if(FAILED(hr = funcRefl->GetDesc(&funcDesc)))
			retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsDXIL() couldn't obtain function desc"));

		D3D12_FUNCTION_DESC1 funcDesc1;
		if(FAILED(hr = funcRefl->GetDesc1(&funcDesc1)))
			retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsDXIL() couldn't obtain function desc1"));

		const C8 *name = funcDesc.Name;

		ESHPipelineStage stage = ESHPipelineStage_Count;        //Lib

		switch (funcDesc1.ShaderType) {

			case D3D12_SHVER_RAY_GENERATION_SHADER: stage = ESHPipelineStage_RaygenExt;        break;
			case D3D12_SHVER_INTERSECTION_SHADER:   stage = ESHPipelineStage_IntersectionExt;  break;
			case D3D12_SHVER_ANY_HIT_SHADER:        stage = ESHPipelineStage_AnyHitExt;        break;
			case D3D12_SHVER_CLOSEST_HIT_SHADER:    stage = ESHPipelineStage_ClosestHitExt;    break;
			case D3D12_SHVER_MISS_SHADER:           stage = ESHPipelineStage_MissExt;          break;
			case D3D12_SHVER_CALLABLE_SHADER:       stage = ESHPipelineStage_CallableExt;      break;

			case D3D12_SHVER_PIXEL_SHADER:          stage = ESHPipelineStage_Pixel;            break;
			case D3D12_SHVER_VERTEX_SHADER:         stage = ESHPipelineStage_Vertex;           break;
			case D3D12_SHVER_GEOMETRY_SHADER:       stage = ESHPipelineStage_GeometryExt;      break;
			case D3D12_SHVER_HULL_SHADER:           stage = ESHPipelineStage_Hull;             break;
			case D3D12_SHVER_DOMAIN_SHADER:         stage = ESHPipelineStage_Domain;           break;
			case D3D12_SHVER_COMPUTE_SHADER:        stage = ESHPipelineStage_Compute;          break;
			case D3D12_SHVER_MESH_SHADER:           stage = ESHPipelineStage_MeshExt;          break;
			case D3D12_SHVER_AMPLIFICATION_SHADER:  stage = ESHPipelineStage_TaskExt;          break;

			default:
				retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsDXIL() had an invalid shader type"));
		}

		Bool insertPlain = false;

		if(showAll)
			insertPlain = true;

		else {

			if(stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt) {

				if(!alreadyContainsLib)
					gotoIfError3(clean, ListCompilerEntrypoint_pushBack(
						uniqueEntrypoints, CompilerEntrypoint{ .stage = ESHPipelineStage_Count }, alloc, e_rr));

				alreadyContainsLib = true;
			}

			else insertPlain = true;
		}

		if(insertPlain) {

			gotoIfError3(clean, ListCompilerEntrypoint_pushBack(
				uniqueEntrypoints, CompilerEntrypoint{ .stage = stage }, alloc, e_rr));

			CharString nameStr = CharString_createRefCStrConst(name);
			U64 questionMark = CharString_findFirstSensitive(&nameStr, '?', 0, 0);

			if (questionMark != U64_MAX) {        //Mangled name

				U64 atAt = CharString_findFirstSensitive(&nameStr, '@', questionMark + 1, 0);

				if(atAt == U64_MAX || CharString_getAt(nameStr, atAt + 1) != '@')
					retError(clean, Error_invalidState(0, "Compiler_getUniqueEntrypointsDXIL() invalid mangling"));

				nameStr = CharString_createRefSizedConst(nameStr.ptr + questionMark + 1, atAt - questionMark - 1, false);
			}

			gotoIfError3(clean, CharString_createCopy(
				nameStr, alloc, &ListCompilerEntrypoint_last(*uniqueEntrypoints)->name, e_rr
			));
		}
	}

clean:

	if(!s_uccess && freeEp)
		ListCompilerEntrypoint_freeUnderlying(uniqueEntrypoints, alloc);

	if(dxilReflLib)
		dxilReflLib->Release();

	return s_uccess;
}

extern "C" Bool Compiler_linkDXIL(
	const Compiler *comp,
	const ListBuffer *inputs,
	const ListSHUniformRuntime *uniforms,
	Buffer uniformData,
	const CharString *entrypoint,
	U16 shaderVersion,
	ESHPipelineStage stageType,
	ESHExtension exts,
	ListCompileError *errors,
	Buffer *finalResult,
	const Allocator *alloc,
	Error *e_rr
) {
	
	Bool s_uccess = true;

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;
	IDxcLinker *linker = nullptr;
	IDxcOperationResult *result = nullptr;
	IDxcBlob *finalShader = nullptr;
	IDxcBlobEncoding *temp = nullptr;
	IDxcBlobEncoding *errs = nullptr;
	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	CharString tempStr3 = CharString_createNull();
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *error = NULL;
	IDxcBlob *resultBlob = NULL;
	
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ListU16 tmpWStr = ListU16{}, tmpWStr2 = ListU16{}, tmpWStr3 = ListU16{};
		ListListU16 wstrArr = ListListU16{};
		ListU16PtrConst wstrConstArr = ListU16PtrConst{};
	#else
		ListU32 tmpWStr = ListU32{}, tmpWStr2 = ListU32{}, tmpWStr3 = ListU32{};
		ListListU32 wstrArr = ListListU32{};
		ListU32PtrConst wstrConstArr = ListU32PtrConst{};
	#endif

	Bool isShaderAnnotation =
		(stageType >= ESHPipelineStage_RtStartExt && stageType >= ESHPipelineStage_RtEndExt) ||
		stageType >= ESHPipelineStage_Count;            //Maintain lib linking

	Bool hasErrors = false;

	//Create linker

	HRESULT hr = DxcCreateInstance(CLSID_DxcLinker, IID_PPV_ARGS(&linker));
	Bool has16Bit = exts & ESHExtension_16BitTypes;

	if(FAILED(hr))
		retError(clean, Error_invalidState(2, "Compiler_linkDXIL() IDxcLinker couldn't be created"));

	//Compile DXIL with only the uniform exports;

	if(uniforms && uniforms->length) {

		EHLSLStringifyFlags flags = EHLSLStringifyFlags_None;

		if (has16Bit)
			flags = EHLSLStringifyFlags(flags | EHLSLStringifyFlags_Has16Bit);

		if (exts & ESHExtension_F64)
			flags = EHLSLStringifyFlags(flags | EHLSLStringifyFlags_HasF64);

		if (exts & ESHExtension_I64)
			flags = EHLSLStringifyFlags(flags | EHLSLStringifyFlags_HasI64);

		//Stringify uniforms into exports
		//export Type $$specConst_Name() { return ...; }

		for (U64 i = 0; i < uniforms->length; ++i) {

			//Uniform info

			SHUniformRuntime uniform = uniforms->ptr[i];

			if(uniform.typeIdShort >= ETypeId_Max)
				retError(clean, Error_invalidState(2, "Compiler_linkDXIL() typeIdShort out of bounds"));

			TypeId typeId = ETypeId_arr[uniform.typeIdShort];
			U64 len = ETypeId_getBytes(typeId);
			
			if(uniform.dataOffset + len > Buffer_length(uniformData))
				retError(clean, Error_invalidState(2, "Compiler_linkDXIL() uniformData out of bounds"));

			//Format start of function export

			gotoIfError3(clean, CharString_createFromETypeIdHLSL(typeId, flags, alloc, &tempStr3, e_rr));

			gotoIfError3(clean, CharString_format(alloc, &tempStr2, e_rr,
				"export %s $$specConst_%.*s() { return ",
				tempStr3.ptr,
				(int) CharString_length(uniform.name), uniform.name.ptr
			));

			CharString_free(&tempStr3, alloc);

			gotoIfError3(clean, CharString_appendString(&tempStr, &tempStr2, alloc, e_rr));
			CharString_free(&tempStr2, alloc);

			//Turn uniform into real constructor

			SHValue value = SHValue{};
			Buffer_memcpy(
				Buffer_createRef(&value, sizeof(value)),
				Buffer_createRefConst(uniformData.ptr + uniform.dataOffset, len)
			);

			gotoIfError3(clean, SHValue_stringifyHLSL(&value, typeId, flags, alloc, &tempStr2, e_rr));
			gotoIfError3(clean, CharString_appendString(&tempStr, &tempStr2, alloc, e_rr));
			CharString_free(&tempStr2, alloc);

			//Finish function export

			CharString appendClose = CharString_createRefCStrConst("; }\n");
			gotoIfError3(clean, CharString_appendString(&tempStr, &appendClose, alloc, e_rr));
		}
		
		//Compile binary

		DxcBuffer buffer = DxcBuffer{
			.Ptr = tempStr.ptr,
			.Size = CharString_length(tempStr),
			.Encoding = DXC_CP_UTF8
		};

		LPCWSTR args[] = { L"-T", L"lib_6_3", L"-enable-16bit-types" };

		hr = interfaces->compiler->Compile(
			&buffer, args, has16Bit ? 3 : 2, NULL,
			IID_PPV_ARGS(&dxcResult)
		);

		if (FAILED(hr))
			retError(clean, Error_invalidState(0, "Compiler_linkDXIL() Compile uniforms failed"));
		
		CharString_free(&tempStr, alloc);

		hr = dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), NULL);

		if(FAILED(hr))
			retError(clean, Error_invalidState(1, "Compiler_linkDXIL() fetch errors failed"));

		if(error && error->GetStringLength()) {
			CharString errStr = CharString_createRefSizedConst(error->GetStringPointer(), error->GetStringLength(), false);
			gotoIfError3(clean, Compiler_parseErrors(errStr, alloc, errors, &hasErrors, e_rr));
		}

		if(error) {
			error->Release();
			error = NULL;
		}

		if (hasErrors)
			goto clean;

		hr = dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&resultBlob), NULL);

		if (FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_linkDXIL() Fetch dxil failed"));

		LPCWSTR lib = L"uniforms";
		hr = linker->RegisterLibrary(lib, resultBlob);

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_linkDXIL() Couldn't register uniforms binary"));
			
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			gotoIfError3(clean, ListU16PtrConst_pushBack(&wstrConstArr, (const U16*) lib, alloc, e_rr));
		#else
			gotoIfError3(clean, ListU32PtrConst_pushBack(&wstrConstArr, (const U32*) lib, alloc, e_rr));
		#endif
	}

	//Add all libraries

	for (U64 i = 0; i < (!inputs ? 0 : inputs->length); ++i) {

		U64 len = Buffer_length(inputs->ptr[i]);

		if(!len || len >= U32_MAX)
			retError(clean, Error_invalidState(2, "Compiler_linkDXIL() Inputs contained an empty binary"));

		hr = interfaces->utils->CreateBlobFromPinned(inputs->ptr[i].ptr, (U32) len, DXC_CP_ACP, &temp);

		if (FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_linkDXIL() Couldn't create IDxcBlob"));

		gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr, "%" PRIu64, i));
			
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			gotoIfError3(clean, CharString_toUTF16(tempStr, alloc, &tmpWStr, e_rr));
		#else
			gotoIfError3(clean, CharString_toUTF32(tempStr, alloc, &tmpWStr, e_rr));
		#endif

		CharString_free(&tempStr, alloc);
		hr = linker->RegisterLibrary((const wchar_t*) tmpWStr.ptr, temp);

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_linkDXIL() Couldn't register binary"));
			
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			gotoIfError3(clean, ListListU16_pushBack(&wstrArr, tmpWStr, alloc, e_rr));
			gotoIfError3(clean, ListU16PtrConst_pushBack(&wstrConstArr, tmpWStr.ptr, alloc, e_rr));
			tmpWStr = ListU16{};
		#else
			gotoIfError3(clean, ListListU32_pushBack(&wstrArr, tmpWStr, alloc, e_rr));
			gotoIfError3(clean, ListU32PtrConst_pushBack(&wstrConstArr, tmpWStr.ptr, alloc, e_rr));
			tmpWStr = ListU32{};
		#endif

		temp = NULL;  //Moved
	}

	//Turn inputs into UTF16/UTF32

	if (CharString_length(*entrypoint)) {
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			gotoIfError3(clean, CharString_toUTF16(*entrypoint, alloc, &tmpWStr2, e_rr));
		#else
			gotoIfError3(clean, CharString_toUTF32(*entrypoint, alloc, &tmpWStr2, e_rr));
		#endif
	}

	if (isShaderAnnotation) {
		gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr, "lib_%" PRIu8 "_%" PRIu8,
			(U8)(shaderVersion >> 8), (U8)shaderVersion
		));
	}

	else gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr, "%s_%" PRIu8 "_%" PRIu8,
		ESHPipelineStage_getStagePrefix(stageType),
		(U8)(shaderVersion >> 8), (U8)shaderVersion
	));

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		gotoIfError3(clean, CharString_toUTF16(tempStr, alloc, &tmpWStr3, e_rr));
	#else
		gotoIfError3(clean, CharString_toUTF32(tempStr, alloc, &tmpWStr3, e_rr));
	#endif

	CharString_free(&tempStr, alloc);

	//Link

	hr = linker->Link(
		tmpWStr2.ptr ? (const wchar_t*) tmpWStr2.ptr : NULL,
		(const wchar_t*) tmpWStr3.ptr,
		(const LPCWSTR*) wstrConstArr.ptr,
		(U32) wstrConstArr.length,
		nullptr,
		0,
		&result
	);

	if (SUCCEEDED(result->GetErrorBuffer(&errs)) && errs && errs->GetBufferSize()) {
		CharString errStr = CharString_createRefSizedConst((const C8*)errs->GetBufferPointer(), errs->GetBufferSize(), false);
		gotoIfError3(clean, Compiler_parseErrors(errStr, alloc, errors, &hasErrors, e_rr));
	}

	if (FAILED(hr) || hasErrors)
		retError(clean, Error_invalidOperation(0, "Compiler_linkDXIL() DXIL couldn't be linked"));

	//Remove shader reflection

	if (FAILED(result->GetResult(&finalShader)))
		retError(clean, Error_invalidOperation(0, "Compiler_linkDXIL() final DXIL couldn't be obtained"));

	//Note: We don't strip reflection here, because we need it in processDXIL
	
	//Copy to final destination

	gotoIfError3(clean, Buffer_createUninitializedBytes(finalShader->GetBufferSize(), alloc, finalResult, e_rr));
	Buffer_memcpy(*finalResult, Buffer_createRefConst(finalShader->GetBufferPointer(), finalShader->GetBufferSize()));

clean:

	if(linker)
		linker->Release();

	if(result)
		result->Release();

	if(finalShader)
		finalShader->Release();
	
	if(resultBlob)
		resultBlob->Release();

	if(dxcResult)
		dxcResult->Release();

	if(error)
		error->Release();

	if(temp)
		temp->Release();

	if(errs)
		errs->Release();

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ListU16_free(&tmpWStr, alloc);
		ListU16_free(&tmpWStr2, alloc);
		ListU16_free(&tmpWStr3, alloc);
		ListListU16_freeUnderlying(&wstrArr, alloc);
		ListU16PtrConst_free(&wstrConstArr, alloc);
	#else
		ListU32_free(&tmpWStr, alloc);
		ListU32_free(&tmpWStr2, alloc);
		ListU32_free(&tmpWStr3, alloc);
		ListListU32_freeUnderlying(&wstrArr, alloc);
		ListU32PtrConst_free(&wstrConstArr, alloc);
	#endif

	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	CharString_free(&tempStr3, alloc);
	return s_uccess;
}
