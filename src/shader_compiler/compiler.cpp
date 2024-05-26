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

#include "platforms/ext/listx_impl.h"
#include "shader_compiler/compiler.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/file.h"
#include "types/math.h"

#ifdef _WIN32
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <Unknwn.h>
#endif

#define ENABLE_DXC_STATIC_LINKING
#include <dxc/dxcapi.h>
#include <exception>

const C8 *resources =
	#include "shader_compiler/shaders/resources.hlsl"
	;

const C8 *types =
	#include "shader_compiler/shaders/types.hlsl"
	;

const C8 *nvHLSLExtns =
	#include "nvHLSLExtns.h"
	;

const C8 *nvHLSLExtns2 =
	#include "nvHLSLExtns2.h"
	;

const C8 *nvShaderExtnEnums =
	#include "nvShaderExtnEnums.h"
	;

const C8 *nvHLSLExtnsInternal =
	#include "nvHLSLExtnsInternal.h"
	;

//This file is only because DXC doesn't have a C interface.
//So we need to wrap C++ in C, so we can call it from C.

typedef class IncludeHandler IncludeHandler;

typedef struct CompilerInterfaces {
	IDxcUtils *utils;
	IDxcCompiler3 *compiler;
	IncludeHandler *includeHandler;
} CompilerInterfaces;

class IncludeHandler : public IDxcIncludeHandler {

	IDxcUtils *utils;
	ListIncludedFile includedFiles{};
	ListU64 isPresent{};
	Allocator alloc;
	U64 counter{};			//Unique file counter in the current file

public:

	inline IncludeHandler(IDxcUtils *utils, Allocator alloc): utils(utils), alloc(alloc) { }

	//Useful so includes can be cached instead of re-fetched from file each time.
	//This has to be called in between compiles to ensure the include handler knows it's the first time re-using.
	inline void reset() {

		counter = 0;

		for (U64 i = 0; i < includedFiles.length; ++i)
			includedFiles.ptrNonConst[i].includeInfo.counter = 0;

		Buffer_unsetAllBits(ListU64_buffer(isPresent));
	}

	inline U64 getCounter() const { return counter; }
	inline ListIncludedFile getIncludedFiles() const {
		ListIncludedFile res = ListIncludedFile{};
		ListIncludedFile_createRefConst(includedFiles.ptr, includedFiles.length, &res);
		return res;
	}

	inline ~IncludeHandler() {
		ListIncludedFile_freeUnderlying(&includedFiles, alloc);
		ListU64_free(&isPresent, alloc);
	}

	HRESULT STDMETHODCALLTYPE LoadSource(
		_In_ LPCWSTR fileNameStr,
		_COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource
	) override {

		CharString fileName = CharString_createNull();
		CharString resolved = CharString_createNull();
		IDxcBlobEncoding *encoding = NULL;

		Error *e_rr = NULL;
		Bool s_uccess = true;

		HRESULT hr = S_OK;
		Buffer tempBuffer = Buffer_createNull();
		CharString tempFile = CharString_createNull();
		FileInfo fileInfo = FileInfo{};
		Bool isVirtual = false;
		Bool isBuiltin = false;
		U64 i = 0;
		U64 lastAt = U64_MAX;

		gotoIfError2(clean, CharString_createFromUTF16((const U16*)fileNameStr, U64_MAX, alloc, &fileName))

		//Little hack to handle builtin shaders, by using "virtual files" //myTest.hlsl

		lastAt = CharString_findLastStringSensitive(fileName, CharString_createRefCStrConst("@"), 0);
		isBuiltin = lastAt != U64_MAX;

		if (isBuiltin) {

			CharString tmp = CharString_createNull();

			if(!CharString_cut(fileName, lastAt + 1, 0, &tmp) || !CharString_length(tmp))
				retError(clean, Error_invalidState(0, "IncludeHandler::LoadSource expected source after //"))

			gotoIfError2(clean, CharString_createCopy(tmp, alloc, &resolved))
		}

		else gotoIfError2(clean, File_resolve(
			fileName, &isVirtual, MAX_PATH, Platform_instance.workingDirectory, alloc, &resolved
		))

		for (; i < includedFiles.length; ++i)
			if(CharString_equalsStringSensitive(resolved, includedFiles.ptr[i].includeInfo.file))
				break;

		//We already included it in an earlier run.
		//This could mean it's either the same file (and thus needs to be empty) or it's in cache.

		if (i != includedFiles.length) {

			//We need to validate the cache first
			//It's possible the compiler exists so long that files on disk have been changed

			Bool validCache = true;

			if(!isBuiltin) {		//Builtins don't exist on disk, so they can't really be hot reloaded

				gotoIfError2(clean, File_getInfo(resolved, &fileInfo, alloc))

				IncludeInfo prevInclude = includedFiles.ptr[i].includeInfo;

				//When timestamp has changed, it might be dirty.
				//When fileSize has, it definitely is dirty.

				if (fileInfo.timestamp != prevInclude.timestamp || fileInfo.fileSize != prevInclude.fileSize) {

					//Unfortunately peanut butter, we have to hash the whole file.
					//Luckily, CRC32C is quite fast.

					if (fileInfo.fileSize == prevInclude.fileSize) {

						gotoIfError2(clean, File_read(resolved, 1 * SECOND, &tempBuffer))

						U32 crc32c = Buffer_crc32c(tempBuffer);

						if(crc32c != prevInclude.crc32c)
							validCache = false;

						Buffer_free(&tempBuffer, alloc);
					}

					else validCache = false;
				}

				FileInfo_free(&fileInfo, alloc);
			}

			//Continue with the next if we don't have a valid cache

			if(!validCache) {

				IncludedFile includedFile = IncludedFile{};
				gotoIfError2(clean, ListIncludedFile_popLocation(&includedFiles, i, &includedFile))

				IncludedFile_free(&includedFile, alloc);

				i = includedFiles.length;
			}

			//If we already included it in this file, then it should be a NO-OP

			else if((isPresent.ptr[i >> 6] >> (i & 63)) & 1)
				hr = utils->CreateBlobFromPinned("", 0, DXC_CP_ACP, &encoding);

			//Otherwise, we should fetch from our cache

			else {

				hr = utils->CreateBlobFromPinned(
					includedFiles.ptr[i].data.ptr, (U32) CharString_length(includedFiles.ptr[i].data), DXC_CP_UTF8, &encoding
				);

				isPresent.ptrNonConst[i >> 6] |= (U64)1 << (i & 63);
				++counter;
			}

			//Keep track of count

			if(validCache) {
				++includedFiles.ptrNonConst[i].globalCounter;
				++includedFiles.ptrNonConst[i].includeInfo.counter;
			}
		}

		//File is new in cache, we should read it from file

		if(i == includedFiles.length) {

			U32 crc32c = U32_MAX;

			if(!isBuiltin) {

				gotoIfError2(clean, File_getInfo(resolved, &fileInfo, alloc))
				gotoIfError2(clean, File_read(resolved, 1 * SECOND, &tempBuffer))

				crc32c = Buffer_crc32c(tempBuffer);
				Ns timestamp = fileInfo.timestamp;
				FileInfo_free(&fileInfo, alloc);

				if(Buffer_length(tempBuffer) >> 32)
					retError(clean, Error_outOfBounds(
						0, Buffer_length(tempBuffer), U32_MAX,
						"IncludeHandler::LoadSource CreateBlobFromPinned requires 32-bit buffers max"
					))

				IncludedFile inc = IncludedFile{
					.includeInfo = IncludeInfo{
						.fileSize = (U32) Buffer_length(tempBuffer),
						.crc32c = crc32c,
						.timestamp = timestamp,
						.counter = 1
					},
					.globalCounter = 1
				};

				//Move buffer to includedData

				gotoIfError2(clean, CharString_createCopy(
					CharString_createRefSizedConst((const C8*)tempBuffer.ptr, Buffer_length(tempBuffer), false),
					alloc,
					&tempFile
				))

				Buffer_free(&tempBuffer, alloc);

				inc.includeInfo.file = resolved;
				inc.data = tempFile;

				gotoIfError2(clean, ListIncludedFile_pushBack(&includedFiles, inc, alloc))
				resolved = CharString_createNull();
				tempFile = CharString_createNull();

			} else {

				if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("resources.hlsl")))
					tempFile = CharString_createRefCStrConst(resources);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("types.hlsl")))
					tempFile = CharString_createRefCStrConst(types);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("nvShaderExtnEnums.h")))
					tempFile = CharString_createRefCStrConst(nvShaderExtnEnums);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("nvHLSLExtnsInternal.h")))
					tempFile = CharString_createRefCStrConst(nvHLSLExtnsInternal);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("nvHLSLExtns.h"))) {

					//Because of the C limit of 64KiB per string constant, we need two string constants and merge them

					CharString tmp = CharString_createRefCStrConst(nvHLSLExtns);
					gotoIfError2(clean, CharString_createCopy(tmp, alloc, &tempFile))

					tmp = CharString_createRefCStrConst(nvHLSLExtns2);
					gotoIfError2(clean, CharString_appendString(&tempFile, tmp, alloc))
				}

				else retError(clean, Error_notFound(0, 0, "IncludeHandler::LoadSource builtin file not found"))

				crc32c = Buffer_crc32c(CharString_bufferConst(tempFile));

				IncludedFile inc = IncludedFile{
					.includeInfo = IncludeInfo{
						.fileSize = (U32) CharString_length(tempFile),
						.crc32c = crc32c,
						.counter = 1
					},
					.globalCounter = 1
				};

				inc.includeInfo.file = resolved;
				inc.data = tempFile;

				gotoIfError2(clean, ListIncludedFile_pushBack(&includedFiles, inc, alloc))
				resolved = CharString_createNull();
				tempFile = CharString_createNull();
			}

			if((i >> 6) >= isPresent.length)
				gotoIfError2(clean, ListU64_pushBack(&isPresent, 0, alloc))

			hr = utils->CreateBlobFromPinned(
				includedFiles.ptr[i].data.ptr, (U32) CharString_length(includedFiles.ptr[i].data), DXC_CP_UTF8, &encoding
			);

			isPresent.ptrNonConst[i >> 6] |= (U64)1 << (i & 63);
			++counter;
		}

		if(hr)
			goto clean;

		*ppIncludeSource = encoding;
		encoding = NULL;

	clean:

		if(SUCCEEDED(hr) && !s_uccess)
			hr = E_FAIL;

		if(encoding)
			encoding->Release();

		FileInfo_free(&fileInfo, alloc);
		CharString_free(&resolved, alloc);
		CharString_free(&fileName, alloc);
		CharString_free(&tempFile, alloc);
		Buffer_free(&tempBuffer, alloc);
		return hr;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, _COM_Outptr_ void __RPC_FAR* __RPC_FAR*) override {
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override {	return 0; }
	ULONG STDMETHODCALLTYPE Release() override { return 0; }
};

Lock lockThread = Lock{ .active = true };
Bool hasInitialized;

Bool Compiler_setup(Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = Lock_lock(&lockThread, 0);

	if (acq >= ELockAcquire_Success) {		//First to lock is first to initialize

		if(!hasInitialized) {

			HRESULT hr = DxcInitialize();

			if(FAILED(hr))
				retError(clean, Error_invalidState(0, "Compiler_setup() couldn't initialize DXC"));

			hasInitialized = true;
		}
	}

	//Wait for thread to finish, since the first thread is the only one that can initialize

	else acq = Lock_lock(&lockThread, U64_MAX);

	if(!hasInitialized)
		retError(clean, Error_invalidState(0, "Compiler_setup() one of the other threads couldn't initialize DXC"))

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&lockThread);

	return s_uccess;
}

Bool Compiler_create(Allocator alloc, Compiler *comp, Error *e_rr) {

	Bool s_uccess = true;
	CompilerInterfaces *interfaces = NULL;

	gotoIfError3(clean, Compiler_setup(e_rr))

	if(!comp)
		retError(clean, Error_nullPointer(1, "Compiler_create()::comp is required"))

	interfaces = (CompilerInterfaces*) comp->interfaces;

	if(interfaces->utils)
		retError(clean, Error_alreadyDefined(1, "Compiler_create()::comp must be empty"))

	try {

		//TODO: Call DxcCreateInstance2 with custom IMalloc.
		//Problem; we don't know the size of any allocation.
		//Would require each allocation to lock and push in ListDebugAllocation, even in Release mode!

		//IMalloc *malloc = ...;

		//Create utils

		HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&interfaces->utils));

		if(FAILED(hr))
			retError(clean, Error_invalidState(0, "Compiler_create() IDxcUtils couldn't be created. Missing DLL?"))

		//Create include handler

		interfaces->includeHandler = new IncludeHandler(interfaces->utils, alloc);

		//Create compiler

		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&interfaces->compiler));

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_create() IDxcCompiler3 couldn't be created"))

	} catch (std::exception&) {
		retError(clean, Error_invalidState(1, "Compiler_create() raised an internal exception"))
	}

clean:

	if(!s_uccess)
		Compiler_free(comp, alloc);

	return s_uccess;
}

void Compiler_free(Compiler *comp, Allocator alloc) {

	(void)alloc;

	if(!comp)
		return;

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;

	if(interfaces->utils)
		interfaces->utils->Release();

	if(interfaces->compiler)
		interfaces->compiler->Release();

	if(interfaces->includeHandler)
		delete interfaces->includeHandler;

	*comp = Compiler{};
}

Bool Compiler_mergeIncludeInfo(Compiler *comp, Allocator alloc, ListIncludeInfo *infos, Error *e_rr) {

	Bool s_uccess = true;
	CompilerInterfaces *interfaces = NULL;

	ListIncludedFile files = interfaces->includeHandler->getIncludedFiles();
	CharString tmp = CharString_createNull();

	if(!comp || !infos)
		retError(clean, Error_nullPointer(!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp and infos are required"))

	interfaces = (CompilerInterfaces*) comp->interfaces;

	if(!interfaces->includeHandler)
		retError(clean, Error_nullPointer(
			!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp->interfaces includeHandler is missing"
		))

	for (U64 i = 0; i < files.length; ++i) {

		IncludedFile info = files.ptr[i];
		U64 j = 0;

		//If the file isn't the same, we can't merge it.
		//In this case, the file size, hash and others can differ.
		//This means we might have duplicate entries by file name, but is required to accurately represent the state.

		for(; j < infos->length; ++j)
			if(
				CharString_equalsStringSensitive(infos->ptr[j].file, info.includeInfo.file) &&
				infos->ptr[j].crc32c == info.includeInfo.crc32c && infos->ptr[j].fileSize == info.includeInfo.fileSize
			)
				break;

		//Add new entry

		if(j == infos->length) {

			gotoIfError2(clean, CharString_createCopy(info.includeInfo.file, alloc, &tmp))

			info.includeInfo.counter = info.globalCounter;
			info.includeInfo.file = tmp;
			gotoIfError2(clean, ListIncludeInfo_pushBack(infos, info.includeInfo, alloc))
			tmp = CharString_createNull();
		}

		//Merge counter.
		//File timestamp can be safely merged to latest since our fileSize and crc32c are determined to match

		else {
			infos->ptrNonConst[j].counter += info.globalCounter;
			infos->ptrNonConst[j].timestamp = U64_max(infos->ptrNonConst[j].timestamp, info.includeInfo.timestamp);
		}
	}

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

void Compiler_shutdown() {
	DxcShutdown(true);
}

Bool Compiler_preprocess(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result, Error *e_rr) {

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp.interfaces;

	Bool s_uccess = true;
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *error = NULL;
	ListU16 inputFile = ListU16{};
	ListU16 includeDir = ListU16{};
	ListU16 tempWStr = ListU16{};
	Bool hasErrors = false, isVirtual = false;
	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	ListListU16 strings = ListListU16{};

	if(!interfaces->utils || !result)
		retError(clean, Error_alreadyDefined(!interfaces->utils ? 0 : 2, "Compiler_preprocess()::comp is required"))

	if(!CharString_length(settings.string))
		retError(clean, Error_invalidParameter(1, 0, "Compiler_preprocess()::settings.string is required"))

	if(settings.outputType >= ESHBinaryType_Count || settings.format >= ECompilerFormat_Count)
		retError(clean, Error_invalidParameter(1, 0, "Compiler_preprocess()::settings contains invalid format or outputType"))

	gotoIfError2(clean, CharString_toUTF16(settings.path, alloc, &inputFile))

	if(settings.includeDir.ptr) {

		gotoIfError2(clean, File_resolve(
			settings.includeDir, &isVirtual, 256, Platform_instance.workingDirectory, alloc, &tempStr2
		))

		gotoIfError2(clean, CharString_toUTF16(tempStr2, alloc, &includeDir))
		CharString_free(&tempStr2, alloc);
	}

	try {

		interfaces->includeHandler->reset();

		result->isSuccess = false;

		const U16 args[][9] = {
			{ '-', 'P', '\0' },									//Preprocess
			{ '-', 's', 'p', 'i', 'r', 'v', '\0' },				//-spirv (enable spirv generation)
			{ '-', 'I', '\0' },									//-I (include dir)
			{ '-', 'D', '_', '_', 'O', 'X', 'C', '3', '\0' }	//-D__OXC3 to indicate we're compiling from OxC3
		};

		U32 argCounter = 3;

		const U16 *argsPtr[10] = {

			args[0],
			inputFile.ptr,

			args[3]
		};

		if (settings.outputType == ESHBinaryType_SPIRV)		//-spirv
			argsPtr[argCounter++] = args[1];

		if (includeDir.length) {							//-I
			argsPtr[argCounter++] = args[2];
			argsPtr[argCounter++] = includeDir.ptr;
		}

		//Format major, minor, patch and version

		const C8 *formats[] = {
			"-D__OXC3_MAJOR=%" PRIu64,
			"-D__OXC3_MINOR=%" PRIu64,
			"-D__OXC3_PATCH=%" PRIu64,
			"-D__OXC3_VERSION=%" PRIu64,
		};

		const U64 formatInts[] = {
			OXC3_MAJOR,
			OXC3_MINOR,
			OXC3_PATCH,
			OXC3_VERSION
		};

		for(U64 i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {

			gotoIfError2(clean, CharString_format(alloc, &tempStr2, formats[i], formatInts[i]))
		
			gotoIfError2(clean, CharString_toUTF16(tempStr2, alloc, &tempWStr))
			CharString_free(&tempStr2, alloc);

			gotoIfError2(clean, ListListU16_pushBack(&strings, tempWStr, alloc))
			argsPtr[argCounter++] = tempWStr.ptr;
			tempWStr = ListU16{};
		}

		//Compile

		DxcBuffer buffer{
			.Ptr = settings.string.ptr,
			.Size = CharString_length(settings.string)
		};

		HRESULT hr = interfaces->compiler->Compile(
			&buffer,
			(LPCWSTR*) argsPtr, argCounter,
			interfaces->includeHandler,
			IID_PPV_ARGS(&dxcResult)
		);

		if(FAILED(hr))
			retError(clean, Error_invalidState(0, "Compiler_preprocess() \"Compile\" failed"))

		hr = dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), NULL);

		if(FAILED(hr))
			retError(clean, Error_invalidState(1, "Compiler_preprocess() fetch errors failed"))

		if(error && error->GetStringLength()) {
			CharString errs = CharString_createRefSizedConst(error->GetStringPointer(), error->GetStringLength(), false);
			gotoIfError3(clean, Compiler_parseErrors(errs, alloc, &result->compileErrors, &hasErrors, e_rr))
		}

		if(error) {
			error->Release();
			error = NULL;
		}

		if (settings.infoAboutIncludes) {

			gotoIfError2(clean, ListIncludeInfo_resize(&result->includeInfo, interfaces->includeHandler->getCounter(), alloc))

			ListIncludedFile files = interfaces->includeHandler->getIncludedFiles();

			for(U64 i = 0, j = 0; i < files.length; ++i)
				if (files.ptr[i].includeInfo.counter) {		//Exclude inactive includes

					IncludeInfo copy = files.ptr[i].includeInfo;
					gotoIfError2(clean, CharString_createCopy(copy.file, alloc, &tempStr))

					copy.file = tempStr;
					result->includeInfo.ptrNonConst[j] = copy;
					tempStr = CharString_createNull();

					++j;
				}
		}

		if (hasErrors)
			goto clean;

		hr = dxcResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&error), NULL);

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_preprocess() fetch hlsl failed"))

		gotoIfError2(clean, CharString_createCopy(
			CharString_createRefSizedConst(error->GetStringPointer(), error->GetBufferSize(), false),
			alloc,
			&result->text
		))

		result->type = ECompileResultType_Text;
		result->isSuccess = true;

	} catch (std::exception&) {
		retError(clean, Error_invalidState(1, "Compiler_preprocess() raised an internal exception"))
	}

clean:

	if(!s_uccess)
		CompileResult_free(result, alloc);

	if(dxcResult)
		dxcResult->Release();

	if(error)
		error->Release();

	ListListU16_freeUnderlying(&strings, alloc);
	ListU16_free(&inputFile, alloc);
	ListU16_free(&includeDir, alloc);
	ListU16_free(&tempWStr, alloc);
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	return s_uccess;
}

Bool Compiler_compile(
	Compiler comp,
	CompilerSettings settings,
	SHBinaryInfo toCompile,
	Allocator alloc,
	CompileResult *result,
	Error *e_rr
) {

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp.interfaces;

	Bool s_uccess = true;
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *error = NULL;
	IDxcBlob *resultBlob = NULL;
	ListU16 inputFile = ListU16{};
	ListU16 includeDir = ListU16{};
	ListU16 tempWStr = ListU16{};
	Bool hasErrors = false, isVirtual = false;
	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	ListListU16 strings = ListListU16{};

	if(!interfaces->utils || !result)
		retError(clean, Error_alreadyDefined(!interfaces->utils ? 0 : 2, "Compiler_compile()::comp is required"))

	if(!CharString_length(settings.string))
		retError(clean, Error_invalidParameter(1, 0, "Compiler_compile()::settings.string is required"))

	if(settings.outputType >= ESHBinaryType_Count || settings.format >= ECompilerFormat_Count)
		retError(clean, Error_invalidParameter(1, 0, "Compiler_compile()::settings contains invalid format or outputType"))

	gotoIfError2(clean, CharString_toUTF16(settings.path, alloc, &inputFile))

	if(settings.includeDir.ptr) {

		gotoIfError2(clean, File_resolve(
			settings.includeDir, &isVirtual, 256, Platform_instance.workingDirectory, alloc, &tempStr2
		))

		gotoIfError2(clean, CharString_toUTF16(tempStr2, alloc, &includeDir))
		CharString_free(&tempStr2, alloc);
	}

	try {

		interfaces->includeHandler->reset();

		result->isSuccess = false;

		const U16 args[][27] = {

			{ '-', 'P', '\0' },									//Preprocess
			{ '-', 's', 'p', 'i', 'r', 'v', '\0' },				//-spirv (enable spirv generation)
			{ '-', 'I', '\0' },									//-I (include dir)
			{ '-', 'D', '_', '_', 'O', 'X', 'C', '3', '\0' },	//-D__OXC3 to indicate we're compiling from OxC3

			//-auto-binding-space 0

			{ '-', 'a', 'u', 't', 'o', '-', 'b', 'i', 'n', 'd', 'i', 'n', 'g', '-', 's', 'p', 'a', 'c', 'e', '\0' },
			{ '0', '\0' },

			//-enable-payload-qualifiers

			{
				'-', 'e', 'n', 'a', 'b', 'l', 'e', '-', 
				'p', 'a', 'y', 'l', 'o', 'a', 'd', '-', 
				'q', 'u', 'a', 'l', 'i', 'f', 'i', 'e', 'r', 's', '\0'
			},

			//-Zpc, -Qstrip_debug, -Qstrip_reflect

			{ '-', 'Z', 'p', 'c', '\0' },
			{ '-', 'Q', 's', 't', 'r', 'i', 'p', '_', 'd', 'e', 'b', 'u', 'g', '\0' },
			{ '-', 'Q', 's', 't', 'r', 'i', 'p', '_', 'r', 'e', 'f', 'l', 'e', 'c', 't', '\0'},

			//-Od or -O3

			{ '-', 'O', 'd', '\0' },
			{ '-', 'O', '3', '\0' },

			//-Zi for debug

			{ '-', 'Z', 'i', '\0' },

			//-enable-16bit-types

			{ '-', 'e', 'n', 'a', 'b', 'l', 'e', '-', '1', '6', 'b', 'i', 't', '-', 't', 'y', 'p', 'e', 's', '\0' },

			//spirv flags;
			//-fvk-use-dx-layout, -fspv-target-env=vulkan1.2, -fvk-invert-y, -fvk-use-dx-position-w
			//-fspv-entrypoint-name=main

			{ '-', 'f', 'v', 'k', '-', 'u', 's', 'e', '-', 'd', 'x', '-', 'l', 'a', 'y', 'o', 'u', 't', '\0' },

			{ 
				'-', 'f', 's', 'p', 'v', '-', 't', 'a', 'r', 'g', 'e', 't', '-', 'e', 'n', 'v', '=',
				'v', 'u', 'l', 'k', 'a', 'n', '1', '.', '2', '\0'
			},

			{ '-', 'f', 'v', 'k', '-', 'i', 'n', 'v', 'e', 'r', 't', '-', 'y', '\0' },

			{ 
				'-', 'f', 'v', 'k', '-', 'u', 's', 'e', '-', 
				'd', 'x', '-', 'p', 'o', 's', 'i', 't', 'i', 'o', 'n', '-', 'w', '\0'
			},

			{ 
				'-', 'f', 's', 'p', 'v', '-', 'e', 'n', 't', 'r', 'y', 'p', 'o', 'i', 'n', 't', '-', 'n', 'a', 'm', 'e', '=',
				'm', 'a', 'i', 'n', '\0'
			}
		};

		U32 argCounter = 7;

		const U16 *argsPtr[22] = {

			args[0],
			inputFile.ptr,

			args[3],
			args[7],

			args[8],
			args[9],

			settings.debug ? args[10] : args[11]
		};

		if(settings.debug)
			argsPtr[argCounter++] = args[12];					//-Zi

		if(toCompile.extensions & ESHExtension_16BitTypes)
			argsPtr[argCounter++] = args[13];					//-enable-16bit-types

		if (settings.outputType == ESHBinaryType_SPIRV) {

			argsPtr[argCounter++] = args[1];					//-spirv
			argsPtr[argCounter++] = args[14];					//-fvk-use-dx-layout
			argsPtr[argCounter++] = args[15];					//-fspv-target-env=vulkan1.2

			if(
				toCompile.stageType == ESHPipelineStage_Vertex ||
				toCompile.stageType == ESHPipelineStage_Domain ||
				toCompile.stageType == ESHPipelineStage_GeometryExt
			)
				argsPtr[argCounter++] = args[16];				//-fvk-invert-y

			else if(toCompile.stageType == ESHPipelineStage_Pixel)
				argsPtr[argCounter++] = args[17];				//-fvk-use-dx-position-w

			if(!toCompile.hasShaderAnnotation)
				argsPtr[argCounter++] = args[18];				//-fspv-entrypoint-name=main
		}

		else {

			argsPtr[argCounter++] = args[4];					//-auto-binding-space
			argsPtr[argCounter++] = args[5];					//0

			if(toCompile.extensions & ESHExtension_PAQ)
				argsPtr[argCounter++] = args[6];				//-enable-payload-qualifiers
		}

		if (includeDir.length) {								//-I
			argsPtr[argCounter++] = args[2];
			argsPtr[argCounter++] = includeDir.ptr;
		}

		//TODO: -E <entrypointName> -T <target>

		//TODO: __OXC3_EXT_<X> foreach extension

		//Format major, minor, patch and version

		const C8 *formats[] = {
			"-D__OXC3_MAJOR=%" PRIu64,
			"-D__OXC3_MINOR=%" PRIu64,
			"-D__OXC3_PATCH=%" PRIu64,
			"-D__OXC3_VERSION=%" PRIu64,
		};

		const U64 formatInts[] = {
			OXC3_MAJOR,
			OXC3_MINOR,
			OXC3_PATCH,
			OXC3_VERSION
		};

		for(U64 i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {

			gotoIfError2(clean, CharString_format(alloc, &tempStr2, formats[i], formatInts[i]))
		
			gotoIfError2(clean, CharString_toUTF16(tempStr2, alloc, &tempWStr))
			CharString_free(&tempStr2, alloc);

			gotoIfError2(clean, ListListU16_pushBack(&strings, tempWStr, alloc))
			argsPtr[argCounter++] = tempWStr.ptr;
			tempWStr = ListU16{};
		}

		//Compile

		DxcBuffer buffer{
			.Ptr = settings.string.ptr,
			.Size = CharString_length(settings.string)
		};

		HRESULT hr = interfaces->compiler->Compile(
			&buffer,
			(LPCWSTR*) argsPtr, argCounter,
			interfaces->includeHandler,
			IID_PPV_ARGS(&dxcResult)
		);

		if(FAILED(hr))
			retError(clean, Error_invalidState(0, "Compiler_compile() \"Compile\" failed"))

		hr = dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), NULL);

		if(FAILED(hr))
			retError(clean, Error_invalidState(1, "Compiler_compile() fetch errors failed"))

		if(error && error->GetStringLength()) {
			CharString errs = CharString_createRefSizedConst(error->GetStringPointer(), error->GetStringLength(), false);
			gotoIfError3(clean, Compiler_parseErrors(errs, alloc, &result->compileErrors, &hasErrors, e_rr))
		}

		if(error) {
			error->Release();
			error = NULL;
		}

		if (hasErrors)
			goto clean;

		hr = dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&resultBlob), NULL);

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_compile() fetch hlsl failed"))

		gotoIfError2(clean, Buffer_createCopy(
			Buffer_createRefConst(error->GetBufferPointer(), error->GetBufferSize()),
			alloc,
			&result->binary
		))

		result->type = ECompileResultType_Binary;
		result->isSuccess = true;

	} catch (std::exception) {
		retError(clean, Error_invalidState(1, "Compiler_compile() raised an internal exception"))
	}

clean:

	if(!s_uccess)
		CompileResult_free(result, alloc);

	if(dxcResult)
		dxcResult->Release();

	if(error)
		error->Release();

	ListListU16_freeUnderlying(&strings, alloc);
	ListU16_free(&inputFile, alloc);
	ListU16_free(&includeDir, alloc);
	ListU16_free(&tempWStr, alloc);
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	return s_uccess;
}

//TODO: Real compile, check for [extension(I16, F16)] one of the two indicates we need to enable 16-bit types
