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
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/file.h"
#include "types/math.h"
#include "formats/oiSB.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <Unknwn.h>
#endif

#define ENABLE_DXC_STATIC_LINKING
#include "dxcompiler/dxcapi.h"
#include "optimizer.hpp"
#include "SPIRV-Reflect/spirv_reflect.h"
#include "directx/d3d12shader.h"
#include <exception>

const C8 *resources =
	#include "shader_compiler/shaders/resources.hlsli"
	;

const C8 *types =
	#include "shader_compiler/shaders/types.hlsli"
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

	virtual ~IncludeHandler() {
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

		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			gotoIfError2(clean, CharString_createFromUTF16((const U16*)fileNameStr, U64_MAX, alloc, &fileName))
		#else
			gotoIfError2(clean, CharString_createFromUTF32((const U32*)fileNameStr, U64_MAX, alloc, &fileName))
		#endif

		//Little hack to handle builtin shaders, by using "virtual files" //myTest.hlsli

		lastAt = CharString_findLastStringSensitive(fileName, CharString_createRefCStrConst("@"), 0, 0);
		isBuiltin = lastAt != U64_MAX;

		if (isBuiltin) {

			CharString tmp = CharString_createNull();

			if(!CharString_cut(fileName, lastAt, 0, &tmp) || !CharString_length(tmp))
				retError(clean, Error_invalidState(0, "IncludeHandler::LoadSource expected source after //"))

			gotoIfError2(clean, CharString_createCopy(tmp, alloc, &resolved))
		}

		else gotoIfError3(clean, File_resolve(
			fileName, &isVirtual, 256, Platform_instance.workingDirectory, alloc, &resolved, e_rr
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

				gotoIfError3(clean, File_getInfo(resolved, &fileInfo, alloc, e_rr))

				IncludeInfo prevInclude = includedFiles.ptr[i].includeInfo;

				//When timestamp has changed, it might be dirty.
				//When fileSize has, it definitely is dirty.

				if (fileInfo.timestamp != prevInclude.timestamp || fileInfo.fileSize != prevInclude.fileSize) {

					//Unfortunately peanut butter, we have to hash the whole file.
					//Luckily, CRC32C is quite fast.

					if (fileInfo.fileSize == prevInclude.fileSize) {

						gotoIfError3(clean, File_read(resolved, 1 * SECOND, &tempBuffer, e_rr))

						gotoIfError2(clean, CharString_createCopy(
							CharString_createRefSizedConst((const C8*)tempBuffer.ptr, Buffer_length(tempBuffer), false),
							alloc,
							&tempFile
						))

						Buffer_free(&tempBuffer, alloc);

						if(!CharString_eraseAllSensitive(&tempFile, '\r', 0, 0))
							retError(clean, Error_invalidState(0, "IncludeHandler::LoadSource couldn't erase \\rs"))

						U32 crc32c = Buffer_crc32c(CharString_bufferConst(tempFile));
						CharString_free(&tempFile, alloc);

						if(crc32c != prevInclude.crc32c)
							validCache = false;
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

			if(!isBuiltin) {

				gotoIfError3(clean, File_getInfo(resolved, &fileInfo, alloc, e_rr))
				gotoIfError3(clean, File_read(resolved, 1 * SECOND, &tempBuffer, e_rr))

				Ns timestamp = fileInfo.timestamp;
				FileInfo_free(&fileInfo, alloc);

				if(Buffer_length(tempBuffer) >> 32)
					retError(clean, Error_outOfBounds(
						0, Buffer_length(tempBuffer), U32_MAX,
						"IncludeHandler::LoadSource CreateBlobFromPinned requires 32-bit buffers max"
					))

				gotoIfError2(clean, CharString_createCopy(
					CharString_createRefSizedConst((const C8*)tempBuffer.ptr, Buffer_length(tempBuffer), false),
					alloc,
					&tempFile
				))

				if(!CharString_eraseAllSensitive(&tempFile, '\r', 0, 0))
					retError(clean, Error_invalidState(1, "IncludeHandler::LoadSource couldn't erase \\rs"))

				U32 crc32c = Buffer_crc32c(CharString_bufferConst(tempFile));

				IncludedFile inc = IncludedFile{};
				inc.includeInfo = IncludeInfo{
					.fileSize = (U32) Buffer_length(tempBuffer),
					.crc32c = crc32c,
					.timestamp = timestamp,
					.counter = 1,
					.file = CharString_createNull()
				};

				inc.globalCounter = 1;

				//Move buffer to includedData

				Buffer_free(&tempBuffer, alloc);

				inc.includeInfo.file = resolved;
				inc.data = tempFile;

				gotoIfError2(clean, ListIncludedFile_pushBack(&includedFiles, inc, alloc))
				resolved = CharString_createNull();
				tempFile = CharString_createNull();

			} else {

				CharString tmpTmp = CharString_createNull();

				if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("@resources.hlsli")))
					tmpTmp = CharString_createRefCStrConst(resources);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("@types.hlsli")))
					tmpTmp = CharString_createRefCStrConst(types);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("@nvShaderExtnEnums.h")))
					tmpTmp = CharString_createRefCStrConst(nvShaderExtnEnums);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("@nvHLSLExtnsInternal.h")))
					tmpTmp = CharString_createRefCStrConst(nvHLSLExtnsInternal);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("@nvHLSLExtns.h"))) {

					//Because of the C limit of 64KiB per string constant, we need two string constants and merge them

					CharString tmp = CharString_createRefCStrConst(nvHLSLExtns);
					gotoIfError2(clean, CharString_createCopy(tmp, alloc, &tempFile))

					tmp = CharString_createRefCStrConst(nvHLSLExtns2);
					gotoIfError2(clean, CharString_appendString(&tempFile, tmp, alloc))
				}

				else retError(clean, Error_notFound(0, 0, "IncludeHandler::LoadSource builtin file not found"))

				if(tmpTmp.ptr)
					gotoIfError2(clean, CharString_createCopy(tmpTmp, alloc, &tempFile))

				if(!CharString_eraseAllSensitive(&tempFile, '\r', 0, 0))
					retError(clean, Error_invalidState(1, "IncludeHandler::LoadSource couldn't erase \\rs"))

				U32 crc32c = Buffer_crc32c(CharString_bufferConst(tempFile));

				IncludedFile inc = IncludedFile{};

				inc.includeInfo = IncludeInfo{
					.fileSize = (U32) CharString_length(tempFile),
					.crc32c = crc32c,
					.timestamp = 0,
					.counter = 1,
					.file = resolved
				};

				inc.globalCounter = 1;
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

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, _COM_Outptr_ void**) override {
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override {	return 0; }
	ULONG STDMETHODCALLTYPE Release() override { return 0; }
};

SpinLock lockThread;
Bool hasInitialized;

Bool Compiler_setup(Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = SpinLock_lock(&lockThread, 0);

	if (acq >= ELockAcquire_Success) {		//First to lock is first to initialize

		if(!hasInitialized) {

			HRESULT hr = DxcInitialize();

			if(FAILED(hr))
				retError(clean, Error_invalidState(0, "Compiler_setup() couldn't initialize DXC"));

			hasInitialized = true;
		}
	}

	//Wait for thread to finish, since the first thread is the only one that can initialize

	else acq = SpinLock_lock(&lockThread, U64_MAX);

	if(!hasInitialized)
		retError(clean, Error_invalidState(0, "Compiler_setup() one of the other threads couldn't initialize DXC"))

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&lockThread);

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

	CharString tmp = CharString_createNull();
	ListIncludedFile files = ListIncludedFile{};

	if(!comp || !infos)
		retError(clean, Error_nullPointer(!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp and infos are required"))

	interfaces = (CompilerInterfaces*) comp->interfaces;

	if(!interfaces->includeHandler)
		retError(clean, Error_nullPointer(
			!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp->interfaces includeHandler is missing"
		))

	files = interfaces->includeHandler->getIncludedFiles();

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

	ELockAcquire acq = SpinLock_lock(&lockThread, U64_MAX);

	if(acq < ELockAcquire_Success)
		return;

	if(!hasInitialized)
		goto clean;

	try {
		DxcShutdown(true);
	} catch(std::exception&){}

	hasInitialized = false;

clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&lockThread);
}

//Thank you Microsoft!
//Very nice!!
//What's wrong with UTF8?

#if _PLATFORM_TYPE == PLATFORM_WINDOWS

	#define Compiler_defineStrings																		\
		ListU16 tempStrUTF16 = ListU16{};																\
		ListListU16 stringsUTF16 = ListListU16{};														\
		ListU16PtrConst strings = ListU16PtrConst{}

	#define Compiler_freeStrings																		\
		ListU16_free(&tempStrUTF16, alloc);																\
		ListListU16_freeUnderlying(&stringsUTF16, alloc);												\
		ListU16PtrConst_free(&strings, alloc)

	#define Compiler_convertToWString(strs, label) 														\
		for(U64 ii = 0; ii < strs.length; ++ii) {														\
			gotoIfError2(label, CharString_toUTF16(strs.ptr[ii], alloc, &tempStrUTF16))					\
			gotoIfError2(label, ListU16PtrConst_pushBack(&strings, tempStrUTF16.ptr, alloc))			\
			gotoIfError2(label, ListListU16_pushBack(&stringsUTF16, tempStrUTF16, alloc))				\
			tempStrUTF16 = ListU16{};																	\
		}

#else

	#define Compiler_defineStrings																		\
		ListU32 tempStrUTF32 = ListU32{};																\
		ListListU32 stringsUTF32 = ListListU32{};														\
		ListU32PtrConst strings = ListU32PtrConst{}

	#define Compiler_freeStrings																		\
		ListU32_free(&tempStrUTF32, alloc);																\
		ListListU32_freeUnderlying(&stringsUTF32, alloc);												\
		ListU32PtrConst_free(&strings, alloc)

	#define Compiler_convertToWString(strs, label) 														\
		for(U64 ii = 0; ii < strs.length; ++ii) {														\
			gotoIfError2(label, CharString_toUTF32(strs.ptr[ii], alloc, &tempStrUTF32))					\
			gotoIfError2(label, ListU32PtrConst_pushBack(&strings, tempStrUTF32.ptr, alloc))			\
			gotoIfError2(label, ListListU32_pushBack(&stringsUTF32, tempStrUTF32, alloc))				\
			tempStrUTF32 = ListU32{};																	\
		}

#endif

Bool Compiler_setupIncludePaths(ListCharString *dst, CompilerSettings settings, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	Bool isVirtual = false;

	//-I x for include dir

	if(CharString_length(settings.includeDir)) {

		gotoIfError3(clean, File_resolve(
			settings.includeDir, &isVirtual, 256, Platform_instance.workingDirectory, alloc, &tempStr, e_rr
		))

		gotoIfError2(clean, ListCharString_pushBack(dst, CharString_createRefCStrConst("-I"), alloc))
		gotoIfError2(clean, ListCharString_pushBack(dst, tempStr, alloc))
		tempStr = CharString_createNull();
	}

	//<file> -I <file's parent> to resolve errors to the origin file and use relative includes

	if(CharString_length(settings.path)) {

		gotoIfError3(clean, File_resolve(
			settings.path, &isVirtual, 256, Platform_instance.workingDirectory, alloc, &tempStr, e_rr
		))

		gotoIfError2(clean, ListCharString_pushBack(dst, tempStr, alloc))
		tempStr = CharString_createRefStrConst(tempStr);

		if(!CharString_cutAfterLastSensitive(tempStr, '/', &tempStr2))
			retError(clean, Error_invalidState(0, "Compiler_setupIncludePaths() can't find parent directory"))

		gotoIfError2(clean, ListCharString_pushBack(dst, CharString_createRefCStrConst("-I"), alloc))
		gotoIfError2(clean, ListCharString_pushBack(dst, tempStr2, alloc))
		tempStr2 = tempStr = CharString_createNull();
	}

clean:
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	return s_uccess;
}

Bool Compiler_copyIncludes(CompileResult *result, IncludeHandler *includeHandler, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tempStr = CharString_createNull();
	ListIncludedFile files = ListIncludedFile{};

	gotoIfError2(clean, ListIncludeInfo_resize(&result->includeInfo, includeHandler->getCounter(), alloc))

	files = includeHandler->getIncludedFiles();

	for(U64 i = 0, j = 0; i < files.length; ++i)
		if (files.ptr[i].includeInfo.counter) {		//Exclude inactive includes

			IncludeInfo copy = files.ptr[i].includeInfo;
			gotoIfError2(clean, CharString_createCopy(copy.file, alloc, &tempStr))

			copy.file = tempStr;
			result->includeInfo.ptrNonConst[j] = copy;
			tempStr = CharString_createNull();

			++j;
		}

clean:
	CharString_free(&tempStr, alloc);
	return s_uccess;
}

Bool Compiler_registerArgStr(ListCharString *strings, CharString str, Allocator alloc, Error *e_rr) {
	Bool s_uccess = true;
	gotoIfError2(clean, ListCharString_pushBack(strings, str, alloc))
clean:
	return s_uccess;
}

Bool Compiler_registerArgStrConst(ListCharString *strings, CharString str, Allocator alloc, Error *e_rr) {
	return Compiler_registerArgStr(strings, CharString_createRefStrConst(str), alloc, e_rr);
}

//Only with const C8* that will always be in mem
Bool Compiler_registerArgCStr(ListCharString *strings, const C8 *str, Allocator alloc, Error *e_rr) {
	return Compiler_registerArgStr(strings, CharString_createRefCStrConst(str), alloc, e_rr);
}

Bool Compiler_preprocess(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result, Error *e_rr) {

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp.interfaces;

	Bool s_uccess = true;
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *error = NULL;
	Bool hasErrors = false;
	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	ListCharString stringsUTF8 = ListCharString{};		//One day, Microsoft will fix their stuff, I hope.

	Compiler_defineStrings;

	if(!interfaces->utils || !result)
		retError(clean, Error_alreadyDefined(!interfaces->utils ? 0 : 2, "Compiler_preprocess()::comp is required"))

	if(!CharString_length(settings.string))
		retError(clean, Error_invalidParameter(1, 0, "Compiler_preprocess()::settings.string is required"))

	if(settings.outputType >= ESHBinaryType_Count || settings.format >= ECompilerFormat_Count)
		retError(clean, Error_invalidParameter(1, 0, "Compiler_preprocess()::settings contains invalid format or outputType"))

	gotoIfError3(clean, Compiler_setupIncludePaths(&stringsUTF8, settings, alloc, e_rr))

	try {

		interfaces->includeHandler->reset();		//Ensure we don't reuse stale caches

		result->isSuccess = false;

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-P", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-HV", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "202x", alloc, e_rr))

		if (settings.outputType == ESHBinaryType_SPIRV)		//-spirv
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-spirv", alloc, e_rr))

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
			gotoIfError2(clean, CharString_format(alloc, &tempStr, formats[i], formatInts[i]))
			gotoIfError2(clean, ListCharString_pushBack(&stringsUTF8, tempStr, alloc))
			tempStr = CharString_createNull();
		}

		Compiler_convertToWString(stringsUTF8, clean)

		//Compile

		DxcBuffer buffer{
			.Ptr = settings.string.ptr,
			.Size = CharString_length(settings.string),
			.Encoding = DXC_CP_UTF8
		};

		HRESULT hr = interfaces->compiler->Compile(
			&buffer,
			(LPCWSTR*) strings.ptr, (int) strings.length,
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

		if (settings.infoAboutIncludes)
			gotoIfError3(clean, Compiler_copyIncludes(result, interfaces->includeHandler, alloc, e_rr))

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

	Compiler_freeStrings;
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	ListCharString_freeUnderlying(&stringsUTF8, alloc);
	return s_uccess;
}

Bool spvTypeToESBType(SpvReflectTypeDescription *desc, ESBType *type, Error *e_rr) {

	Bool s_uccess = true;
	SpvReflectNumericTraits numeric = desc->traits.numeric;

	ESBPrimitive prim = ESBPrimitive_Invalid;
	ESBStride stride = ESBStride_X8;
	ESBVector vector = ESBVector_N1;
	ESBMatrix matrix = ESBMatrix_N1;

	if(!desc || !type)
		retError(clean, Error_nullPointer(!desc ? 0 : 1, "spvTypeToESBType()::desc and type are required"))

	switch (desc->type_flags) {

		case SPV_REFLECT_TYPE_FLAG_BOOL:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:

			if(numeric.scalar.signedness || numeric.scalar.width != 32)
				retError(clean, Error_unsupportedOperation(
					0, "spvTypeToESBType()::desc has an unrecognized type (signed bool or size != 32)"
				))

			prim = ESBPrimitive_UInt;
			break;

		case SPV_REFLECT_TYPE_FLAG_INT:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:
			prim = numeric.scalar.signedness ? ESBPrimitive_Int : ESBPrimitive_UInt;
			break;

		case SPV_REFLECT_TYPE_FLAG_FLOAT:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:

			prim = ESBPrimitive_Float;

			if(numeric.scalar.signedness || (
				numeric.scalar.width != 16 && 
				numeric.scalar.width != 32 && 
				numeric.scalar.width != 64
			))
				retError(clean, Error_unsupportedOperation(
					0, "spvTypeToESBType()::desc has an unrecognized type (signed floatXX or size != [16, 32, 64])"
				))

			break;

		default:
			retError(clean, Error_unsupportedOperation(0, "spvTypeToESBType()::desc has an unrecognized type"))
	}

	switch(numeric.scalar.width) {

		case  8:	stride = ESBStride_X8;		break;
		case 16:	stride = ESBStride_X16;		break;
		case 32:	stride = ESBStride_X32;		break;
		case 64:	stride = ESBStride_X64;		break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (8, 16, 32, 64)"
			))
	}

	switch(numeric.matrix.column_count ? numeric.matrix.column_count : numeric.vector.component_count) {

		case 0:
		case 1:		vector = ESBVector_N1;		break;

		case 2:		vector = ESBVector_N2;		break;
		case 3:		vector = ESBVector_N3;		break;
		case 4:		vector = ESBVector_N4;		break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (vecN)"
			))
	}

	switch(numeric.matrix.row_count) {

		case 0:
		case 1:		matrix = ESBMatrix_N1;		break;

		case 2:		matrix = ESBMatrix_N2;		break;
		case 3:		matrix = ESBMatrix_N3;		break;
		case 4:		matrix = ESBMatrix_N4;		break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (matWxH)"
			))
	}

	if(numeric.matrix.stride && numeric.matrix.stride != 0x10)
		retError(clean, Error_unsupportedOperation(0, "spvTypeToESBType()::desc has matrix with stride != 16"))

	*type = (ESBType) ESBType_create(stride, prim, vector, matrix);

clean:
	return s_uccess;
}

Bool spvMapCapabilityToESHExtension(SpvCapability capability, ESHExtension *extension, Error *e_rr) {

	Bool s_uccess = true;
	ESHExtension ext = (ESHExtension)(1 << ESHExtension_Count);

	switch (capability) {

		//Shader types

		case SpvCapabilityTessellation:
		case SpvCapabilityGeometry:

		case SpvCapabilityRayTracingKHR:
		case SpvCapabilityMeshShadingEXT:
			break;

		//RT:

		case SpvCapabilityRayTracingOpacityMicromapEXT:
			ext = ESHExtension_RayMicromapOpacity;
			break;

		case SpvCapabilityRayQueryKHR:
			ext = ESHExtension_RayQuery;
			break;

		case SpvCapabilityRayTracingMotionBlurNV:
			ext = ESHExtension_RayMotionBlur;
			break;

		case SpvCapabilityShaderInvocationReorderNV:
			ext = ESHExtension_RayReorder;
			break;

		case SpvCapabilityAtomicFloat32AddEXT:
		case SpvCapabilityAtomicFloat32MinMaxEXT:
			ext = ESHExtension_AtomicF32;
			break;

		case SpvCapabilityAtomicFloat64AddEXT:
		case SpvCapabilityAtomicFloat64MinMaxEXT:
			ext = ESHExtension_AtomicF64;
			break;

		case SpvCapabilityGroupNonUniformArithmetic:
			ext = ESHExtension_SubgroupArithmetic;
			break;

		case SpvCapabilityGroupNonUniformShuffle:
			ext = ESHExtension_SubgroupShuffle;
			break;

		case SpvCapabilityMultiView:
			ext = ESHExtension_Multiview;
			break;

		//Types

		case SpvCapabilityStorageBuffer16BitAccess:
		case SpvCapabilityStorageUniform16:
		case SpvCapabilityStoragePushConstant16:
		case SpvCapabilityStorageInputOutput16:

		case SpvCapabilityInt16:
		case SpvCapabilityFloat16:
			ext = ESHExtension_16BitTypes;
			break;

		case SpvCapabilityFloat64:
			ext = ESHExtension_F64;
			break;

		case SpvCapabilityInt64:
			ext = ESHExtension_I64;
			break;

		case SpvCapabilityInt64Atomics:
			ext = (ESHExtension)(ESHExtension_I64 | ESHExtension_AtomicI64);
			break;

		case SpvCapabilityComputeDerivativeGroupLinearNV:
			ext = ESHExtension_ComputeDeriv;
			break;

		case SpvCapabilityImageMSArray:
		case SpvCapabilityStorageImageMultisample:
			ext = ESHExtension_WriteMSTexture;
			break;

		//No-op, not important, always supported

		case SpvCapabilityShader:
		case SpvCapabilityMatrix:
		case SpvCapabilityAtomicStorage:

		case SpvCapabilityRuntimeDescriptorArray:
		case SpvCapabilityShaderNonUniform:
		case SpvCapabilityUniformTexelBufferArrayDynamicIndexing:
		case SpvCapabilityStorageTexelBufferArrayDynamicIndexing:
		case SpvCapabilityUniformBufferArrayNonUniformIndexing:
		case SpvCapabilitySampledImageArrayNonUniformIndexing:
		case SpvCapabilityStorageBufferArrayNonUniformIndexing:
		case SpvCapabilityStorageImageArrayNonUniformIndexing:
		case SpvCapabilityUniformTexelBufferArrayNonUniformIndexing:
		case SpvCapabilityStorageTexelBufferArrayNonUniformIndexing:

		case SpvCapabilityGroupNonUniform:
		case SpvCapabilityGroupNonUniformVote:
		case SpvCapabilityGroupNonUniformBallot:
		case SpvCapabilitySubgroupVoteKHR:
		case SpvCapabilitySubgroupBallotKHR:

		case SpvCapabilityStorageImageExtendedFormats:
		case SpvCapabilityImageQuery:
		case SpvCapabilityDerivativeControl:

		case SpvCapabilityInputAttachment:
		case SpvCapabilityMinLod:
		case SpvCapabilityUniformBufferArrayDynamicIndexing:
		case SpvCapabilitySampledImageArrayDynamicIndexing:
		case SpvCapabilityStorageBufferArrayDynamicIndexing:
		case SpvCapabilityStorageImageArrayDynamicIndexing:

		case SpvCapabilitySampledCubeArray:
		case SpvCapabilitySampled1D:
		case SpvCapabilityImage1D:
		case SpvCapabilityImageCubeArray:
			break;

		//Unsupported

		//Provisional

		case SpvCapabilityRayQueryProvisionalKHR:
		case SpvCapabilityRayTracingProvisionalKHR:

		//AMD

		case SpvCapabilityGroups:
					
		case SpvCapabilityFloat16ImageAMD:
		case SpvCapabilityImageGatherBiasLodAMD:
		case SpvCapabilityFragmentMaskAMD:
		case SpvCapabilityImageReadWriteLodAMD:

		//QCOM

		case SpvCapabilityTextureSampleWeightedQCOM:
		case SpvCapabilityTextureBoxFilterQCOM:
		case SpvCapabilityTextureBlockMatchQCOM:

		//NV
					
		case SpvCapabilitySampleMaskOverrideCoverageNV:
		case SpvCapabilityGeometryShaderPassthroughNV:
		case SpvCapabilityShaderViewportMaskNV:
		case SpvCapabilityShaderStereoViewNV:
		case SpvCapabilityPerViewAttributesNV:
		case SpvCapabilityMeshShadingNV:
		case SpvCapabilityImageFootprintNV:
		case SpvCapabilityComputeDerivativeGroupQuadsNV:
		case SpvCapabilityGroupNonUniformPartitionedNV:
		case SpvCapabilityRayTracingNV:
		case SpvCapabilityCooperativeMatrixNV:
		case SpvCapabilityShaderSMBuiltinsNV:
		case SpvCapabilityBindlessTextureNV:

		//Intel

		case SpvCapabilitySubgroupShuffleINTEL:
		case SpvCapabilitySubgroupBufferBlockIOINTEL:
		case SpvCapabilitySubgroupImageBlockIOINTEL:
		case SpvCapabilitySubgroupImageMediaBlockIOINTEL:
		case SpvCapabilityRoundToInfinityINTEL:
		case SpvCapabilityFloatingPointModeINTEL:
		case SpvCapabilityIntegerFunctions2INTEL:
		case SpvCapabilityFunctionPointersINTEL:
		case SpvCapabilityIndirectReferencesINTEL:
		case SpvCapabilityAsmINTEL:

		case SpvCapabilityVectorComputeINTEL:
		case SpvCapabilityVectorAnyINTEL:

		case SpvCapabilitySubgroupAvcMotionEstimationINTEL:
		case SpvCapabilitySubgroupAvcMotionEstimationIntraINTEL:
		case SpvCapabilitySubgroupAvcMotionEstimationChromaINTEL:
		case SpvCapabilityVariableLengthArrayINTEL:
		case SpvCapabilityFunctionFloatControlINTEL:
		case SpvCapabilityFPGAMemoryAttributesINTEL:
		case SpvCapabilityFPFastMathModeINTEL:
		case SpvCapabilityArbitraryPrecisionIntegersINTEL:
		case SpvCapabilityArbitraryPrecisionFloatingPointINTEL:
		case SpvCapabilityUnstructuredLoopControlsINTEL:
		case SpvCapabilityFPGALoopControlsINTEL:
		case SpvCapabilityKernelAttributesINTEL:
		case SpvCapabilityFPGAKernelAttributesINTEL:
		case SpvCapabilityFPGAMemoryAccessesINTEL:
		case SpvCapabilityFPGAClusterAttributesINTEL:
		case SpvCapabilityLoopFuseINTEL:
		case SpvCapabilityFPGADSPControlINTEL:
		case SpvCapabilityMemoryAccessAliasingINTEL:
		case SpvCapabilityFPGAInvocationPipeliningAttributesINTEL:
		case SpvCapabilityFPGABufferLocationINTEL:
		case SpvCapabilityArbitraryPrecisionFixedPointINTEL:
		case SpvCapabilityUSMStorageClassesINTEL:
		case SpvCapabilityRuntimeAlignedAttributeINTEL:
		case SpvCapabilityIOPipesINTEL:
		case SpvCapabilityBlockingPipesINTEL:
		case SpvCapabilityFPGARegINTEL:

		case SpvCapabilityLongConstantCompositeINTEL:
		case SpvCapabilityOptNoneINTEL:

		case SpvCapabilityDebugInfoModuleINTEL:
		case SpvCapabilityBFloat16ConversionINTEL:
		case SpvCapabilitySplitBarrierINTEL:
		case SpvCapabilityFPGAKernelAttributesv2INTEL:
		case SpvCapabilityFPGALatencyControlINTEL:
		case SpvCapabilityFPGAArgumentInterfacesINTEL:

		//Possible in the future? TODO:

		case SpvCapabilityShaderViewportIndexLayerEXT:
		case SpvCapabilityFragmentBarycentricKHR:
		case SpvCapabilityDemoteToHelperInvocation:
		case SpvCapabilityExpectAssumeKHR:
		case SpvCapabilityCooperativeMatrixKHR:
		case SpvCapabilityBitInstructions:

		case SpvCapabilityMultiViewport:
		case SpvCapabilityShaderLayer:
		case SpvCapabilityShaderViewportIndex:

		case SpvCapabilityFragmentShaderSampleInterlockEXT:
		case SpvCapabilityFragmentShaderShadingRateInterlockEXT:
		case SpvCapabilityFragmentShaderPixelInterlockEXT:

		case SpvCapabilityRayTraversalPrimitiveCullingKHR:
		case SpvCapabilityRayTracingPositionFetchKHR:
		case SpvCapabilityRayQueryPositionFetchKHR:
		case SpvCapabilityRayCullMaskKHR:

		case SpvCapabilityAtomicFloat16AddEXT:
		case SpvCapabilityAtomicFloat16MinMaxEXT:

		case SpvCapabilityInputAttachmentArrayDynamicIndexing:
		case SpvCapabilityInputAttachmentArrayNonUniformIndexing:

		case SpvCapabilityGroupNonUniformShuffleRelative:
		case SpvCapabilityGroupNonUniformClustered:
		case SpvCapabilityGroupNonUniformQuad:
		case SpvCapabilityGroupNonUniformRotateKHR:
		case SpvCapabilityGroupUniformArithmeticKHR:

		case SpvCapabilityDotProductInputAll:
		case SpvCapabilityDotProductInput4x8Bit:
		case SpvCapabilityDotProductInput4x8BitPacked:
		case SpvCapabilityDotProduct:

		case SpvCapabilityVulkanMemoryModel:
		case SpvCapabilityVulkanMemoryModelDeviceScope:

		case SpvCapabilityShaderClockKHR:
		case SpvCapabilityFragmentFullyCoveredEXT:
		case SpvCapabilityFragmentDensityEXT:
		case SpvCapabilityPhysicalStorageBufferAddresses:

		case SpvCapabilityDenormPreserve:
		case SpvCapabilityDenormFlushToZero:
		case SpvCapabilitySignedZeroInfNanPreserve:
		case SpvCapabilityRoundingModeRTE:
		case SpvCapabilityRoundingModeRTZ:
		case SpvCapabilityStencilExportEXT:

		case SpvCapabilityInt64ImageEXT:

		case SpvCapabilityStorageBuffer8BitAccess:
		case SpvCapabilityUniformAndStorageBuffer8BitAccess:
		case SpvCapabilityStoragePushConstant8:

		case SpvCapabilityDeviceGroup:
		case SpvCapabilityVariablePointersStorageBuffer:
		case SpvCapabilityVariablePointers:
		case SpvCapabilitySampleMaskPostDepthCoverage:

		case SpvCapabilityWorkgroupMemoryExplicitLayoutKHR:
		case SpvCapabilityWorkgroupMemoryExplicitLayout8BitAccessKHR:
		case SpvCapabilityWorkgroupMemoryExplicitLayout16BitAccessKHR:

		case SpvCapabilityDrawParameters:
		case SpvCapabilityUniformDecoration:

		case SpvCapabilityCoreBuiltinsARM:

		case SpvCapabilityInterpolationFunction:
		case SpvCapabilityTransformFeedback:
		case SpvCapabilitySampledBuffer:
		case SpvCapabilityImageBuffer:

		case SpvCapabilityTileImageColorReadAccessEXT:
		case SpvCapabilityTileImageDepthReadAccessEXT:
		case SpvCapabilityTileImageStencilReadAccessEXT:

		case SpvCapabilityFragmentShadingRateKHR:

		case SpvCapabilityGeometryStreams:
		case SpvCapabilityStorageImageReadWithoutFormat:
		case SpvCapabilityStorageImageWriteWithoutFormat:

		case SpvCapabilityImageRect:
		case SpvCapabilitySampledRect:
		case SpvCapabilityGenericPointer:
		case SpvCapabilityInt8:
		case SpvCapabilitySparseResidency:
		case SpvCapabilitySampleRateShading:

		case SpvCapabilityImageGatherExtended:
		case SpvCapabilityClipDistance:
		case SpvCapabilityCullDistance:

		case SpvCapabilityAtomicStorageOps:

		case SpvCapabilityTessellationPointSize:
		case SpvCapabilityGeometryPointSize:

		//Unsupported, we don't support kernels, only shaders

		case SpvCapabilityAddresses:
		case SpvCapabilityLinkage:

		case SpvCapabilityKernel:
		case SpvCapabilityFloat16Buffer:
		case SpvCapabilityVector16:
		case SpvCapabilityImageBasic:
		case SpvCapabilityImageReadWrite:
		case SpvCapabilityImageMipmap:
		case SpvCapabilityPipes:
		case SpvCapabilityDeviceEnqueue:
		case SpvCapabilityLiteralSampler:
		case SpvCapabilitySubgroupDispatch:
		case SpvCapabilityNamedBarrier:
		case SpvCapabilityPipeStorage:

			retError(clean, Error_invalidState(
				2, "spvMapCapabilityToESHExtension() SPIRV contained capability that isn't supported in oiSH"
			))

		case SpvCapabilityMax:
			retError(clean, Error_invalidState(
				2, "spvMapCapabilityToESHExtension() SPIRV contained invalid capability that isn't supported in SPIRV-Headers"
			))
	}

	//Handled separately to ensure there's no default case in the switch,
	//so that new capabilities are reported when SPIRV-Header update on some compilers.

	if(capability > SpvCapabilityMax)
		retError(clean, Error_invalidState(
			2, "spvMapCapabilityToESHExtension() SPIRV contained invalid capability that isn't supported in SPIRV-Headers"
		))

	*extension = ext;

clean:
	return s_uccess;
}

Bool SpvReflectFormatToESBType(SpvReflectFormat format, ESBType *type, Error *e_rr) {
	
	Bool s_uccess = true;

	switch (format) {

		case SPV_REFLECT_FORMAT_R16_UINT:				*type = ESBType_U16;		break;
		case SPV_REFLECT_FORMAT_R16_SINT:				*type = ESBType_I16;		break;
		case SPV_REFLECT_FORMAT_R16_SFLOAT:				*type = ESBType_F16;		break;

		case SPV_REFLECT_FORMAT_R16G16_UINT:			*type = ESBType_U16x2;		break;
		case SPV_REFLECT_FORMAT_R16G16_SINT:			*type = ESBType_I16x2;		break;
		case SPV_REFLECT_FORMAT_R16G16_SFLOAT:			*type = ESBType_F16x2;		break;

		case SPV_REFLECT_FORMAT_R16G16B16_UINT:			*type = ESBType_U16x3;		break;
		case SPV_REFLECT_FORMAT_R16G16B16_SINT:			*type = ESBType_I16x3;		break;
		case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:		*type = ESBType_F16x3;		break;

		case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:		*type = ESBType_U16x4;		break;
		case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:		*type = ESBType_I16x4;		break;
		case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:	*type = ESBType_F16x4;		break;

		case SPV_REFLECT_FORMAT_R32_UINT:				*type = ESBType_U32;		break;
		case SPV_REFLECT_FORMAT_R32_SINT:				*type = ESBType_I32;		break;
		case SPV_REFLECT_FORMAT_R32_SFLOAT:				*type = ESBType_F32;		break;

		case SPV_REFLECT_FORMAT_R32G32_UINT:			*type = ESBType_U32x2;		break;
		case SPV_REFLECT_FORMAT_R32G32_SINT:			*type = ESBType_I32x2;		break;
		case SPV_REFLECT_FORMAT_R32G32_SFLOAT:			*type = ESBType_F32x2;		break;

		case SPV_REFLECT_FORMAT_R32G32B32_UINT:			*type = ESBType_U32x3;		break;
		case SPV_REFLECT_FORMAT_R32G32B32_SINT:			*type = ESBType_I32x3;		break;
		case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:		*type = ESBType_F32x3;		break;

		case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:		*type = ESBType_U32x4;		break;
		case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:		*type = ESBType_I32x4;		break;
		case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:	*type = ESBType_F32x4;		break;

		case SPV_REFLECT_FORMAT_R64_UINT:				*type = ESBType_U64;		break;
		case SPV_REFLECT_FORMAT_R64_SINT:				*type = ESBType_I64;		break;
		case SPV_REFLECT_FORMAT_R64_SFLOAT:				*type = ESBType_F64;		break;

		case SPV_REFLECT_FORMAT_R64G64_UINT:			*type = ESBType_U64x2;		break;
		case SPV_REFLECT_FORMAT_R64G64_SINT:			*type = ESBType_I64x2;		break;
		case SPV_REFLECT_FORMAT_R64G64_SFLOAT:			*type = ESBType_F64x2;		break;

		case SPV_REFLECT_FORMAT_R64G64B64_UINT:			*type = ESBType_U64x3;		break;
		case SPV_REFLECT_FORMAT_R64G64B64_SINT:			*type = ESBType_I64x3;		break;
		case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:		*type = ESBType_F64x3;		break;

		case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:		*type = ESBType_U64x4;		break;
		case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:		*type = ESBType_I64x4;		break;
		case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:	*type = ESBType_F64x4;		break;

		default:
			retError(clean, Error_invalidState(
				0, "SpvReflectFormatToESBType() couldn't map SPV_REFLECT_FORMAT to ESBType"
			))
	}

clean:
	return s_uccess;
}

Bool SpvCalculateStructLength(const SpvReflectTypeDescription *typeDesc, U64 *result, Error *e_rr) {

	U64 len = 0;
	Bool s_uccess = true;

	for (U64 i = 0; i < typeDesc->member_count; ++i) {

		SpvReflectTypeDescription typeDesck = typeDesc->members[i];

		SpvReflectTypeFlags disallowed =
			SPV_REFLECT_TYPE_FLAG_EXTERNAL_MASK | SPV_REFLECT_TYPE_FLAG_REF | SPV_REFLECT_TYPE_FLAG_VOID;

		if(typeDesck.type_flags & disallowed)
			retError(clean, Error_invalidState(
				0, "SpvCalculateStructLength() can't be called on a struct that contains external data or a ref or void"
			))

		U64 currLen = 0;

		//Array specifies stride, so that's easy

		if (typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY) {

			U64 arrayLen = typeDesck.traits.array.stride;

			for (U64 j = 0; j < typeDesck.traits.array.dims_count; ++j) {

				U64 prevArrayLen = arrayLen;
				arrayLen *= typeDesck.traits.array.dims[j];

				if(arrayLen < prevArrayLen)
					retError(clean, Error_overflow(
						0, arrayLen, prevArrayLen, "SpvCalculateStructLength() arrayLen overflow"
					))
			}

			currLen = arrayLen;
		}

		//Struct causes recursion

		else if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)
			gotoIfError3(clean, SpvCalculateStructLength(typeDesck.struct_type_description, &currLen, e_rr))

		//Otherwise, we can easily calculate it via SpvReflectNumericTraits

		else {

			const SpvReflectNumericTraits *numeric = &typeDesck.traits.numeric;

			currLen = numeric->scalar.width >> 3;

			if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
				currLen = 
					!(typeDesck.decoration_flags & SPV_REFLECT_DECORATION_ROW_MAJOR) ?
					numeric->matrix.stride * numeric->matrix.column_count :
					numeric->matrix.stride * numeric->matrix.row_count;
			
			else if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
				currLen *= numeric->vector.component_count;
		}

		U64 prevLen = len;
		len += currLen;

		if(len < prevLen)
			retError(clean, Error_overflow(
				0, len, prevLen, "SpvCalculateStructLength() len overflow"
			))
	}

clean:

	if(len)
		*result = len;

	return s_uccess;
}

Bool DxilMapToESHExtension(U64 flags, ESHExtension *ext, Error *e_rr) {

	Bool s_uccess = true;

	U64 defaultOps =
		D3D_SHADER_REQUIRES_TYPED_UAV_LOAD_ADDITIONAL_FORMATS |
		D3D_SHADER_REQUIRES_STENCIL_REF |
		D3D_SHADER_REQUIRES_EARLY_DEPTH_STENCIL |
		D3D_SHADER_REQUIRES_UAVS_AT_EVERY_STAGE |
		D3D_SHADER_REQUIRES_64_UAVS |
		D3D_SHADER_REQUIRES_LEVEL_9_COMPARISON_FILTERING |
		D3D_SHADER_REQUIRES_WAVE_OPS;

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
		D3D_SHADER_REQUIRES_WRITEABLE_MSAA_TEXTURES
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
		ESHExtension_WriteMSTexture
	};

	flags &= ~defaultOps;

	for (U64 i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {

		if(!(flags & extensionMap[i]))
			continue;

		flags &= ~extensionMap[i];
		*ext = (ESHExtension)(*ext | extensions[i]);
	}

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

Bool Compiler_validateGroupSize(U32 threads[3], Error *e_rr) {
	
	Bool s_uccess = true;
	U64 totalGroup = (U64)threads[0] * threads[1] * threads[2];

	if(!totalGroup)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() needs group size for compute"))

	if(totalGroup > 512)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() group count out of bounds (512)"))

	if(U32_max(threads[0], threads[1]) > 512)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() group count x or y out of bounds (512)"))

	if(threads[2] > 64)
		retError(clean, Error_invalidOperation(2, "Compiler_validateGroupSize() group count z out of bounds (64)"))

clean:
	return s_uccess;
}

typedef union TempInOutput {
	U8 a[16];
	U64 aU64[2];
} TempInOutput;

Bool Compiler_findEntry(ListSHEntryRuntime entry, CharString name, SHEntryRuntime **ptr, Error *e_rr) {

	for(U64 i = 0; i < entry.length; ++i)
		if (CharString_equalsStringSensitive(entry.ptr[i].entry.name, name)) {
			*ptr = entry.ptrNonConst + i;
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
	CharString entryName,
	SpinLock *lock,
	ListSHEntryRuntime entries,
	Allocator alloc,
	Error *e_rr
) {
	
	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	SHEntryRuntime *entry = NULL;
	Bool didInit = false;

	TempInOutput input, output;
	TempInOutput inputSemantic, outputSemantic;

	for(U8 i = 0; i < 16; ++i) {
		input.a[i] = (U8) inputs[i];
		output.a[i] = (U8) outputs[i];
		inputSemantic.a[i] = (U8) inputSemantics[i];
		outputSemantic.a[i] = (U8) outputSemantics[i];
	}

	gotoIfError3(clean, Compiler_findEntry(entries, entryName, &entry, e_rr))

	if(lock) {
		acq = SpinLock_lock(lock, 1 * SECOND);
		if(acq < ELockAcquire_Success)
			retError(clean, Error_invalidState(0, "Compiler_finalizeEntrypoint() couldn't acquire spin lock"))
	}

	didInit = !entry->isInitialized;

	if (!entry->isInitialized) {

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

		gotoIfError3(clean, ListCharString_move(uniqueSemantics, alloc, &entry->entry.semanticNames, e_rr))

		entry->isInitialized = true;
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
	)
		retError(clean, Error_invalidState(
			0,
			"Compiler_finalizeEntrypoint() had two mismatching inputs from reflection:\n"
			"numthreads, payloadSize, intersectSize, waveSize, inputs or outputs"
		))

	//More thorough compare with semantics

	else {

		//TODO: Semantics could be ordered differently, might still be okay to merge?

		for(U64 i = 0; i < entry->entry.semanticNames.length; ++i)
			if(!CharString_equalsStringSensitive(entry->entry.semanticNames.ptr[i], uniqueSemantics->ptr[i]))
				retError(clean, Error_invalidState(
					0,
					"Compiler_finalizeEntrypoint() had two mismatching semantic names"
				))
	}


clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	if(!didInit && s_uccess)		//Avoid memleak
		ListCharString_freeUnderlying(uniqueSemantics, alloc);

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
				input->ReturnType != D3D_RETURN_TYPE_SINT ||
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

Bool Compiler_processDXIL(
	Buffer *result,
	ListSHRegisterRuntime *registers,
	DxcBuffer reflectionData,
	CompilerInterfaces *interfaces,
	SHBinaryIdentifier toCompile,
	SpinLock *lock,
	ListSHEntryRuntime entries,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ID3D12ShaderReflection1 *dxilRefl{};
	ID3D12LibraryReflection1 *dxilReflLib{};

	const void *resultPtr = result->ptr;
	Bool isLib = !CharString_length(toCompile.entrypoint);
	ESHExtension exts = ESHExtension_None;
	ListCharString strings{};
	U8 inputSemanticCount = 0;

	//Prevent dxil.dll load, sign it ourselves :)

	if(isLib && FAILED(interfaces->utils->CreateReflection(&reflectionData, IID_PPV_ARGS(&dxilReflLib))))
		retError(clean, Error_invalidState(0, "Compiler_processDXIL() lib reflection is invalid"))

	else if (!isLib && FAILED(interfaces->utils->CreateReflection(&reflectionData, IID_PPV_ARGS(&dxilRefl))))
		retError(clean, Error_invalidState(0, "Compiler_processDXIL() shader reflection is invalid"))

	//Payload / intersection data reflection

	if (isLib) {
				
		D3D12_LIBRARY_DESC lib = D3D12_LIBRARY_DESC{};
		if(FAILED(dxilReflLib->GetDesc(&lib)))
			retError(clean, Error_invalidState(0, "Compiler_processDXIL() couldn't get D3D12_LIBRARY_DESC"))

		gotoIfError3(clean, DxilMapToESHExtension(lib.Flags, &exts, e_rr))

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
					
		gotoIfError3(clean, DxilMapToESHExtension(refl.Flags, &exts, e_rr))

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
		*(const U32*)resultPtr != C8x4('D', 'X', 'B', 'C') ||
		I32x4_eq4(I32x4_load4((const I32*)((const U8*)resultPtr + sizeof(U32))), I32x4_zero())		//Unsigned
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

Bool Compiler_convertMemberSPIRV(
	SBFile *sbFile,
	const SpvReflectBlockVariable *var,
	U16 parent,
	U32 offset,
	Bool isPacked,
	Allocator alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	ESBType shType = (ESBType) 0;
	U64 perElementStride = 0;
	U16 structId = U16_MAX;
	U64 expectedSize = perElementStride;
	CharString str = CharString_createRefCStrConst(var->name);
	ListU32 arrays = ListU32{};

	if(var->array.dims_count > SPV_REFLECT_MAX_ARRAY_DIMS)
		retError(clean, Error_invalidState(
			0, "Compiler_convertMemberSPIRV() array dimensions out of bounds"
		))

	if(var->array.dims_count && !var->array.stride)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() array stride unset"))

	for(U64 m = 0; m < var->array.dims_count; ++m)
		if(!var->array.dims[m] || var->array.spec_constant_op_ids[m] != U32_MAX)
			retError(clean, Error_invalidState(
				0, "Compiler_convertMemberSPIRV() invalid array data (0 or has spec constant op)"
			))

	if(var->flags && var->flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED)
		retError(clean, Error_invalidState(
			0, "Compiler_convertMemberSPIRV() unsupported value in cbuffer member"
		))

	if(!(var->type_description->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)) {
		gotoIfError3(clean, spvTypeToESBType(var->type_description, &shType, e_rr))
		perElementStride = var->array.dims_count ? var->array.stride : ESBType_getSize(shType, isPacked);
	}

	else {

		perElementStride = var->array.dims_count ? var->array.stride : var->size;

		U32 stride = var->array.dims_count ? var->array.stride : var->size;

		CharString structName = CharString_createRefCStrConst(var->type_description->type_name);

		U64 j = 0;

		for (; j < sbFile->structs.length; ++j) {

			SBStruct strct = sbFile->structs.ptr[j];
			CharString structNamej = sbFile->structNames.ptr[j];

			if(
				CharString_equalsStringSensitive(structName, structNamej) && strct.stride == stride
			)
				break;
		}

		//Insert type

		if (j == sbFile->structs.length)
			gotoIfError3(clean, SBFile_addStruct(
				sbFile, &structName, SBStruct{ .stride = stride }, alloc, e_rr
			))

		structId = (U16) j;
	}
					
	expectedSize = perElementStride;

	for(U64 m = 0; m < var->array.dims_count; ++m)
		expectedSize *= var->array.dims[m];

	if(var->size > expectedSize)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() var had mismatching size"))

	if(var->array.dims_count)
		gotoIfError2(clean, ListU32_createRefConst(var->array.dims, var->array.dims_count, &arrays))

	if(shType != (ESBType) 0)
		gotoIfError3(clean, SBFile_addVariableAsType(
			sbFile,
			&str,
			offset + var->offset, parent, shType,
			var->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		))

	else {

		U16 newParent = (U16) sbFile->vars.length;

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			&str,
			offset + var->offset, parent, structId,
			var->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		))

		if(!var->member_count || !var->members)
			retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() missing member_count or members"))

		for (U64 j = 0; j < var->member_count; ++j)
			gotoIfError3(clean, Compiler_convertMemberSPIRV(
				sbFile, &var->members[j], newParent, offset + var->offset, isPacked, alloc, e_rr
			))
	}

clean:
	return s_uccess;
}

Bool Compiler_convertShaderBufferSPIRV(
	SpvReflectDescriptorBinding *binding,
	Bool isPacked,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;

	//StructuredBuffer; the inner element represents the whole buffer

	ESBSettingsFlags packedFlags = isPacked ? ESBSettingsFlags_IsTightlyPacked : (ESBSettingsFlags) 0;

	if (!binding->block.padded_size) {

		if(binding->block.member_count != 1 || !binding->block.members)
			retError(clean, Error_invalidState(
				0, "Compiler_convertShaderBufferSPIRV()::binding is missing member count or members"
			))

		SpvReflectBlockVariable *innerStruct = binding->block.members;

		if(!innerStruct->member_count || !innerStruct->members || !innerStruct->padded_size) {

			ESBType type{};
			gotoIfError3(clean, spvTypeToESBType(innerStruct->type_description, &type, e_rr))

			if(type && (innerStruct->members || innerStruct->member_count || innerStruct->padded_size))
				retError(clean, Error_invalidState(
					0, "Compiler_convertShaderBufferSPIRV() inner struct is assumed to be a type, but has invalid members"
				))
			
			U32 paddedSize = ESBType_getSize(type, isPacked);

			if(!isPacked)
				paddedSize = (paddedSize + 15) &~ 15;

			gotoIfError3(clean, SBFile_create(packedFlags, paddedSize, alloc, sbFile, e_rr))
			CharString elementName = CharString_createRefCStrConst("$Element");
			ListU32 arrays{};

			if(innerStruct->array.dims_count)
				gotoIfError2(clean, ListU32_createRefConst(innerStruct->array.dims, innerStruct->array.dims_count, &arrays))
			
			gotoIfError3(clean, SBFile_addVariableAsType(
				sbFile,
				&elementName,
				0, U16_MAX, type,
				binding->block.flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
				arrays.length ? &arrays : NULL,
				alloc, e_rr
			))

			goto clean;
		}

		const C8 *structNameC = innerStruct->type_description->type_name;
		CharString structName = CharString_createRefCStrConst(structNameC);

		gotoIfError3(clean, SBFile_create(packedFlags, innerStruct->padded_size, alloc, sbFile, e_rr))
		gotoIfError3(clean, SBFile_addStruct(sbFile, &structName, SBStruct{ .stride = innerStruct->padded_size }, alloc, e_rr))

		CharString element = CharString_createRefCStrConst("$Element");

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			&element,
			0, U16_MAX, 0,
			innerStruct->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			NULL,
			alloc,
			e_rr
		))

		for (U64 l = 0; l < innerStruct->member_count; ++l) {
			SpvReflectBlockVariable var = innerStruct->members[l];
			gotoIfError3(clean, Compiler_convertMemberSPIRV(sbFile, &var, 0, 0, isPacked, alloc, e_rr))
		}

		goto clean;
	}

	//CBuffer or storage buffer (without dynamic entries)

	gotoIfError3(clean, SBFile_create(ESBSettingsFlags_None, binding->block.padded_size, alloc, sbFile, e_rr))

	for (U64 l = 0; l < binding->block.member_count; ++l) {
		SpvReflectBlockVariable var = binding->block.members[l];
		gotoIfError3(clean, Compiler_convertMemberSPIRV(sbFile, &var, U16_MAX, 0, isPacked, alloc, e_rr))
	}

clean:
	if(!s_uccess && sbFile)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

Bool Compiler_convertRegisterSPIRV(
	ListSHRegisterRuntime *registers,
	SpvReflectDescriptorBinding *binding,
	U32 expectedSet,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	Bool isUnused = !binding->accessed;
	CharString name = CharString_createRefCStrConst(binding->name);

	ListU32 arrays{};
	SBFile sbFile{};

	U64 flatLen = 1;

	ESHBufferType bufferType = ESHBufferType_Count;
	Bool shouldBeBufferWrite = false;
	
	const void *imagePtr = &binding->image;
	const U64 *imagePtrU64 = (const U64*) imagePtr;
	
	const void *blockPtr = &binding->block.name;
	const U64 *blockPtrU64 = (const U64*) blockPtr;

	const void *numericTraits = &binding->block.numeric;
	const U64 *numericTraitsU64 = (const U64*) numericTraits;

	static_assert(
		sizeof(binding->block.numeric) == sizeof(U64) * 3,
		"Compiler_convertShaderBufferSPIRV() does compares in U64[3] of binding->block.numeric, but size changed"
	);

	static_assert(
		sizeof(binding->image) == sizeof(U64) * 3,
		"Compiler_convertRegisterSPIRV() does compares in U64[3] of binding->image, but size changed"
	);

	constexpr U8 blockSize = 40;

	static_assert(
		offsetof(SpvReflectBlockVariable, member_count) - offsetof(SpvReflectBlockVariable, name) == sizeof(U64) * blockSize,
		"Compiler_convertRegisterSPIRV() expected SpvReflectBlockVariable to be made of 43 U64s + 2x U32"
	);

	SHBindings bindings;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		bindings.arr[i] = SHBinding{ .space = U32_MAX, .binding = U32_MAX };

	bindings.arr[ESHBinaryType_SPIRV] = SHBinding{ .space = binding->set, .binding = binding->binding };

	if(expectedSet != binding->set)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() binding->set != parent->set"
		))

	if(binding->binding == U32_MAX && binding->set == U32_MAX)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() binding = U32_MAX, set = U32_MAX is reserved"
		))

	if(binding->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT && binding->input_attachment_index)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() input attachment index is invalid on non input attachment"
		))

	if(binding->byte_address_buffer_offset_count || binding->byte_address_buffer_offsets || binding->user_type)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() unsupported BAB offsets/count and user_type"
		))

	if(binding->array.dims_count) {

		gotoIfError2(clean, ListU32_createRefConst(binding->array.dims, binding->array.dims_count, &arrays))

		for(U64 i = 0; i < binding->array.dims_count; ++i) {

			flatLen *= binding->array.dims[i];

			if(!flatLen || flatLen >> 32)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid flat length (out of bounds or 0)"
				))
		}

		if(flatLen != binding->count)
			retError(clean, Error_invalidState(
				1, "Compiler_convertRegisterSPIRV() register flat length mismatches binding count"
			))
	}

	switch (binding->descriptor_type) {

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {

			SpvReflectDecorationFlags sbufferFlags =
				SPV_REFLECT_DECORATION_ROW_MAJOR | SPV_REFLECT_DECORATION_COLUMN_MAJOR | SPV_REFLECT_DECORATION_NON_WRITABLE;

			if(
				!binding->block.member_count ||
				(binding->block.decoration_flags && binding->block.decoration_flags != SPV_REFLECT_DECORATION_NON_WRITABLE) ||
				(binding->block.flags && binding->block.flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED) ||
				numericTraitsU64[0] ||
				numericTraitsU64[1] ||
				numericTraitsU64[2] ||
				imagePtrU64[0] ||
				imagePtrU64[1] ||
				imagePtrU64[2] ||
				!binding->block.members ||
				(binding->decoration_flags &~ sbufferFlags)
			)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() invalid constant buffer data"
				))
				
			CharString typeName = CharString_createRefCStrConst(binding->type_description->type_name);
			Bool isAtomic = binding->uav_counter_id != U32_MAX || binding->uav_counter_binding;

			bufferType = ESHBufferType_StorageBuffer;

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				bufferType = ESHBufferType_ConstantBuffer;

			else if(CharString_startsWithStringSensitive(typeName, CharString_createRefCStrConst("type."), 0)) {

				typeName.ptr += 5;
				typeName.lenAndNullTerminated -= 5;

				Bool shouldBeWrite = false;

				if (CharString_startsWithStringSensitive(typeName, CharString_createRefCStrConst("RW"), 0)) {
					typeName.ptr += 2;
					typeName.lenAndNullTerminated -= 2;
					shouldBeWrite = true;
				}

				if (CharString_equalsStringSensitive(typeName, CharString_createRefCStrConst("ByteAddressBuffer")))
					bufferType = ESHBufferType_ByteAddressBuffer;

				else {

					CharString appendBuffer = CharString_createRefCStrConst("AppendStructuredBuffer.");
					CharString consumeBuffer = CharString_createRefCStrConst("ConsumeStructuredBuffer.");
					CharString structuredBuffer = CharString_createRefCStrConst("StructuredBuffer.");

					if (
						CharString_startsWithStringSensitive(typeName, appendBuffer, 0) ||
						CharString_startsWithStringSensitive(typeName, consumeBuffer, 0)
					) {

						if(shouldBeWrite)
							retError(clean, Error_invalidState(
								0, "Compiler_convertRegisterSPIRV() invalid RW prefix for append/consume buffer"
							))

						bufferType = ESHBufferType_StructuredBufferAtomic;
						shouldBeWrite = true;
					}

					//TODO: Remember counter binding for SPIRV.

					else if(CharString_equalsStringSensitive(typeName, CharString_createRefCStrConst("ACSBuffer.counter")))
						goto clean;

					else if(CharString_startsWithStringSensitive(typeName, structuredBuffer, 0))
						bufferType = ESHBufferType_StructuredBuffer;

					else retError(clean, Error_invalidState(
						0, "Compiler_convertRegisterSPIRV() invalid RW prefix for append/consume buffer"
					))
				}

				shouldBeBufferWrite = shouldBeWrite;
			}

			if(bufferType == ESHBufferType_StorageBuffer && isAtomic)
				bufferType = ESHBufferType_StorageBufferAtomic;

			if(
				bufferType != ESHBufferType_StorageBufferAtomic &&
				bufferType != ESHBufferType_StructuredBufferAtomic && 
				isAtomic
			)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (has atomic, but invalid type)"
				))

			if(bufferType != ESHBufferType_ByteAddressBuffer && bufferType != ESHBufferType_AccelerationStructure) {

				//TODO: Storage buffer can have "static" part that's constant sized

				if (
					(
						bufferType == ESHBufferType_StructuredBuffer ||
						bufferType == ESHBufferType_StructuredBufferAtomic ||
						bufferType == ESHBufferType_StorageBuffer ||
						bufferType == ESHBufferType_StorageBufferAtomic
					) != (!binding->block.size || !binding->block.padded_size)
				)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() buffer requires size and/or padded size to be set/unset"
					))

				gotoIfError3(clean, Compiler_convertShaderBufferSPIRV(
					binding,
					binding->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					alloc,
					&sbFile,
					e_rr
				))
			}

			break;
		}

		default:
			break;
	}

	if (bufferType == ESHBufferType_Count) {

		for(U8 i = 0; i < blockSize; ++i)
			if(blockPtrU64[i])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid register had buffer decorations but wasn't one"
				))
	
		if(
			binding->uav_counter_binding ||
			binding->uav_counter_id != U32_MAX ||
			binding->block.spirv_id ||
			binding->block.members ||
			binding->block.type_description ||
			binding->block.word_offset.offset
		)
			retError(clean, Error_invalidState(
				1, "Compiler_convertRegisterSPIRV() invalid register had buffer decorations but wasn't one"
			))
	}

	switch (binding->descriptor_type) {

		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		
			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_CBV)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not cbv)"
				))

			if(arrays.ptr)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() arrays aren't allowed on uniform buffers"
				))

			if(binding->decoration_flags)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected decoration flags on resource"
				))

			if(binding->uav_counter_id != U32_MAX || binding->uav_counter_binding)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() uav_counter_id or uav_counter_binding can't be set on cbv"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_ConstantBuffer,
				false,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			))

			goto clean;

		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {
		
			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SAMPLER)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not sampler)"
				))

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid sampler register"
				))
				
			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				false,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			))

			goto clean;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			Log_debugLn(alloc, "Combined image sampler");		//TODO:
			break;

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {

				if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() sampled image didn't have SRV resource flag"
					))
					
				if(binding->image.image_format || binding->image.sampled != 1)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unexpected image data on sampled image"
					))
			}
			else {

				if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_UAV)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() storage image didn't have UAV resource flag"
					))
					
				if(!binding->image.image_format || binding->image.sampled != 2)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unexpected image data on storage image"
					))
			}

			if(binding->decoration_flags)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected decoration flags on image"
				))

			if(binding->image.ms && (binding->image.dim != SpvDim2D || binding->image.depth != 2))
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected multi sample image"
				))

			ESHTextureType type = ESHTextureType_Texture2D;
			Bool isArray = binding->image.arrayed;
			U8 reqDepth = 0;

			if(binding->image.ms) {
				type = ESHTextureType_Texture2DMS;
				reqDepth = 2;
			}

			else switch(binding->image.dim) {
				
				case SpvDim1D:				reqDepth = 1;	type = ESHTextureType_Texture1D;	break;
				case SpvDim2D:				reqDepth = 2;	type = ESHTextureType_Texture2D;	break;
				case SpvDim3D:				reqDepth = 3;	type = ESHTextureType_Texture3D;	break;
				case SpvDimCube:			reqDepth = 3;	type = ESHTextureType_TextureCube;	break;

				case SpvDimRect:
				case SpvDimBuffer:
				case SpvDimSubpassData:
				default:
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unsupported image type"
					))
			}
			
			if(binding->image.depth != reqDepth)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() Unexpected image depth"
				))
				
			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
					registers,
					type,
					isArray,
					false,
					(U8)((!isUnused) << ESHBinaryType_SPIRV),
					ESHTexturePrimitive_Count,
					&name,
					arrays.length ? &arrays : NULL,
					bindings,
					alloc,
					e_rr
				))

			else {

				ETextureFormatId formatId = ETextureFormatId_Undefined;

				switch (binding->image.image_format) {
					case SpvImageFormatRgba32f:		formatId = ETextureFormatId_RGBA32f;	break;
					case SpvImageFormatRgba16f:		formatId = ETextureFormatId_RGBA16f;	break;
					case SpvImageFormatR32f:		formatId = ETextureFormatId_R32f;		break;
					case SpvImageFormatRgba8:		formatId = ETextureFormatId_RGBA8;		break;
					case SpvImageFormatRgba8Snorm:	formatId = ETextureFormatId_RGBA8s;		break;
					case SpvImageFormatRg32f:		formatId = ETextureFormatId_RG32f;		break;
					case SpvImageFormatRg16f:		formatId = ETextureFormatId_RG16f;		break;
					case SpvImageFormatR16f:		formatId = ETextureFormatId_R16f;		break;
					case SpvImageFormatRgba16:		formatId = ETextureFormatId_RGBA16;		break;
					case SpvImageFormatRgb10A2:		formatId = ETextureFormatId_BGR10A2;	break;
					case SpvImageFormatRg16:		formatId = ETextureFormatId_RG16;		break;
					case SpvImageFormatRg8:			formatId = ETextureFormatId_RG8;		break;
					case SpvImageFormatR16:			formatId = ETextureFormatId_R16;		break;
					case SpvImageFormatR8:			formatId = ETextureFormatId_R8;			break;
					case SpvImageFormatRgba16Snorm:	formatId = ETextureFormatId_RGBA16s;	break;
					case SpvImageFormatRg16Snorm:	formatId = ETextureFormatId_RG16s;		break;
					case SpvImageFormatRg8Snorm:	formatId = ETextureFormatId_RG8s;		break;
					case SpvImageFormatR16Snorm:	formatId = ETextureFormatId_R16s;		break;
					case SpvImageFormatR8Snorm:		formatId = ETextureFormatId_R8s;		break;
					case SpvImageFormatRgba32i:		formatId = ETextureFormatId_RGBA32i;	break;
					case SpvImageFormatRgba16i:		formatId = ETextureFormatId_RGBA16i;	break;
					case SpvImageFormatRgba8i:		formatId = ETextureFormatId_RGBA8i;		break;
					case SpvImageFormatR32i:		formatId = ETextureFormatId_R32i;		break;
					case SpvImageFormatRg32i:		formatId = ETextureFormatId_RG32i;		break;
					case SpvImageFormatRg16i:		formatId = ETextureFormatId_RG16i;		break;
					case SpvImageFormatRg8i:		formatId = ETextureFormatId_RG8i;		break;
					case SpvImageFormatR16i:		formatId = ETextureFormatId_R16i;		break;
					case SpvImageFormatR8i:			formatId = ETextureFormatId_R8i;		break;
					case SpvImageFormatRgba32ui:	formatId = ETextureFormatId_RGBA32u;	break;
					case SpvImageFormatRgba16ui:	formatId = ETextureFormatId_RGBA16u;	break;
					case SpvImageFormatRgba8ui:		formatId = ETextureFormatId_RGBA8u;		break;
					case SpvImageFormatR32ui:		formatId = ETextureFormatId_R32u;		break;
					case SpvImageFormatRg32ui:		formatId = ETextureFormatId_RG32u;		break;
					case SpvImageFormatRg16ui:		formatId = ETextureFormatId_RG16u;		break;
					case SpvImageFormatRg8ui:		formatId = ETextureFormatId_RG8u;		break;
					case SpvImageFormatR16ui:		formatId = ETextureFormatId_R16u;		break;
					case SpvImageFormatR8ui:		formatId = ETextureFormatId_R8u;		break;

					default:
					case SpvImageFormatRgb10a2ui:
					case SpvImageFormatR64ui:
					case SpvImageFormatR64i:
					case SpvImageFormatR11fG11fB10f:
						retError(clean, Error_invalidState(
							1, "Compiler_convertRegisterSPIRV() unsupported image format: rg11fb10f, r64i, r64ui, rgb10a2ui"
						))
				}
			
				gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
					registers,
					type,
					isArray,
					(U8)((!isUnused) << ESHBinaryType_SPIRV),
					ESHTexturePrimitive_Count,
					formatId,
					&name,
					arrays.ptr ? &arrays : NULL,
					bindings,
					alloc,
					e_rr
				))
			}

			break;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
		
			if(
				binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV &&
				binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_UAV
			)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not uav/srv)"
				))

			Bool isWrite = !(binding->block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);

			if((binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_UAV) != isWrite)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (expected uav or srv but got the opposite)"
				))

			if(shouldBeBufferWrite != isWrite)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (type had RW, but buffer didn't)"
				))

			Bool hasSBFile = 
				bufferType != ESHBufferType_ByteAddressBuffer &&
				bufferType != ESHBufferType_AccelerationStructure;

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				bufferType,
				isWrite,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				arrays.length ? &arrays : NULL,
				hasSBFile ? &sbFile : NULL,
				bindings,
				alloc,
				e_rr
			))

			goto clean;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not SRV)"
				))

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid RTAS register"
				))
		
			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_AccelerationStructure,
				false,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				arrays.length ? &arrays : NULL,
				NULL,
				bindings,
				alloc,
				e_rr
			))

			break;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
				retError(clean, Error_invalidState(
					2, "Compiler_convertRegisterSPIRV() mismatching resource_type (not SRV)"
				))

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() invalid input attachment register"
				))

			if(binding->input_attachment_index >> 16)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() input attachment register out of bounds"
				))
				
			gotoIfError3(clean, ListSHRegisterRuntime_addSubpassInput(
				registers,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				bindings,
				(U16) binding->input_attachment_index,
				alloc,
				e_rr
			))

			break;

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		default:
			retError(clean, Error_invalidState(
				1,
				"Compiler_convertRegisterSPIRV() unsupported descriptor type "
				"(uniform/storage buffer dynamic or storage/uniform texel buffer)"
			))
	}

clean:
	SBFile_free(&sbFile, alloc);
	return s_uccess;
}

Bool Compiler_processSPIRV(
	Buffer *result,
	ListSHRegisterRuntime *registers,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	SpinLock *lock,
	ListSHEntryRuntime entries,
	Allocator alloc,
	Error *e_rr
) {
		
	//Ensure we have a valid SPIRV file

	const void *resultPtr = result->ptr;
	U64 binLen = Buffer_length(*result);

	Bool s_uccess = true;
	SpvReflectResult res = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
	ESHExtension exts = ESHExtension_None;
	SpvReflectShaderModule spvMod{};
	spvtools::Optimizer optimizer{ SPV_ENV_UNIVERSAL_1_5 };

	ListCharString strings{};
	U8 inputSemanticCount = 0;

	if(
		binLen < 0x8 ||
		(binLen & 3) ||
		*(const U32*)resultPtr != 0x07230203
	)
		retError(clean, Error_invalidState(2, "Compiler_processSPIRV() SPIRV returned is invalid"))

	//Reflect binary information, since our own parser doesn't have the info yet.

	res = spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NO_COPY, binLen, resultPtr, &spvMod);

	if(res != SPV_REFLECT_RESULT_SUCCESS)
		retError(clean, Error_invalidState(2, "Compiler_processSPIRV() SPIRV returned couldn't be reflected"))

	//Validate capabilities.
	//This makes sure that we only output a binary that's supported by oiSH and no unknown extensions are used.

	for (U64 i = 0; i < spvMod.capability_count; ++i) {

		ESHExtension ext = (ESHExtension)(1 << ESHExtension_Count);
		gotoIfError3(clean, spvMapCapabilityToESHExtension(spvMod.capabilities[i].value, &ext, e_rr))

		//Check if extension was known to oiSH

		if(!(ext >> ESHExtension_Count))
			exts = (ESHExtension) (exts | ext);
	}

	if((toCompile.extensions & exts) != exts)
		retError(clean, Error_invalidState(
			2, "Compiler_processSPIRV() SPIRV contained capability that wasn't enabled by oiSH file (use annotations)"
		))

	//Check entrypoints

	for(U64 i = 0; i < spvMod.entry_point_count; ++i) {
			
		SpvReflectEntryPoint entrypoint = spvMod.entry_points[i];
		Bool searchPayload = false;
		Bool searchIntersection = false;

		U8 payloadSize = 0, intersectSize = 0;

		U32 localSize[3] = { 0 };

		ESHPipelineStage stage = ESHPipelineStage_Count;

		switch (entrypoint.spirv_execution_model) {

			case SpvExecutionModelIntersectionKHR:
				searchIntersection = true;
				searchPayload = true;
				stage = ESHPipelineStage_IntersectionExt;
				break;

			case SpvExecutionModelAnyHitKHR:
				searchPayload = true;
				stage = ESHPipelineStage_AnyHitExt;
				break;

			case SpvExecutionModelClosestHitKHR:
				searchPayload = true;
				stage = ESHPipelineStage_ClosestHitExt;
				break;

			case SpvExecutionModelMissKHR:
				searchPayload = true;
				stage = ESHPipelineStage_MissExt;
				break;

			case SpvExecutionModelCallableKHR:
				searchPayload = true;
				stage = ESHPipelineStage_CallableExt;
				break;

			case SpvExecutionModelMeshEXT:
			case SpvExecutionModelTaskEXT:
			case SpvExecutionModelGLCompute: {

				switch (entrypoint.spirv_execution_model) {
					case SpvExecutionModelMeshEXT:	stage = ESHPipelineStage_MeshExt;	break;
					case SpvExecutionModelTaskEXT:	stage = ESHPipelineStage_TaskExt;	break;
					default:						stage = ESHPipelineStage_Compute;	break;

				}
						
				localSize[0] = entrypoint.local_size.x;
				localSize[1] = entrypoint.local_size.y;
				localSize[2] = entrypoint.local_size.z;
				gotoIfError3(clean, Compiler_validateGroupSize(localSize, e_rr))
				break;
			}

			case SpvExecutionModelRayGenerationKHR:			stage = ESHPipelineStage_RaygenExt;		break;
			case SpvExecutionModelVertex:					stage = ESHPipelineStage_Vertex;		break;
			case SpvExecutionModelFragment:					stage = ESHPipelineStage_Pixel;			break;
			case SpvExecutionModelGeometry:					stage = ESHPipelineStage_GeometryExt;	break;
			case SpvExecutionModelTessellationControl:		stage = ESHPipelineStage_Hull;			break;
			case SpvExecutionModelTessellationEvaluation:	stage = ESHPipelineStage_Domain;		break;

			default:
				retError(clean, Error_invalidState(
					2, "Compiler_processSPIRV() SPIRV contained unsupported execution model"
				))
		}

		if (searchPayload || searchIntersection)
			for (U64 j = 0; j < entrypoint.interface_variable_count; ++j) {

				SpvReflectInterfaceVariable var = entrypoint.interface_variables[j];

				Bool isPayload = var.storage_class == SpvStorageClassIncomingRayPayloadKHR;
				Bool isIntersection = var.storage_class == SpvStorageClassHitAttributeKHR;

				if(!isPayload && !isIntersection)
					continue;

				//Get struct size

				if(
					!var.type_description ||
					var.type_description->op != SpvOpTypeStruct ||
					var.type_description->type_flags != (SPV_REFLECT_TYPE_FLAG_STRUCT | SPV_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK)
				)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() struct payload or intersection isn't a struct"
					))

				U64 structSize = 0;
				gotoIfError3(clean, SpvCalculateStructLength(var.type_description, &structSize, e_rr))

				//Validate payload/intersect size

				if (isPayload) {

					if(structSize > 128)
						retError(clean, Error_outOfBounds(
							0, structSize, 128, "Compiler_processSPIRV() payload out of bounds"
						))

					payloadSize = (U8) structSize;
					continue;
				}

				if(structSize > 32)
					retError(clean, Error_outOfBounds(
						0, structSize, 32, "Compiler_processSPIRV() intersection attribute out of bounds"
					))

				intersectSize = (U8) structSize;
			}

		if(searchPayload && !payloadSize)
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() payload wasn't found in SPIRV"))

		if(searchIntersection && !intersectSize)
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() intersection attribute wasn't found in SPIRV"))

		if(stage == ESHPipelineStage_Count)
			retError(clean, Error_invalidState(
				0, "Compiler_processSPIRV() SPIRV entrypoint couldn't be mapped to ESHPipelineStage"
			))

		Bool isGfx =
			!(stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt) &&
			stage != ESHPipelineStage_WorkgraphExt &&
			stage != ESHPipelineStage_Compute;

		//Reflect inputs & outputs

		ESBType inputs[16] = {};
		ESBType outputs[16] = {};
		U8 inputSemantics[16] = {};
		U8 outputSemantics[16] = {};

		if (isGfx) {

			for (U64 j = 0; j < (U64)entrypoint.input_variable_count + entrypoint.output_variable_count; ++j) {

				Bool isOutput = j >= entrypoint.input_variable_count;

				const SpvReflectInterfaceVariable *input =
					isOutput ? entrypoint.output_variables[j - entrypoint.input_variable_count] :
					entrypoint.input_variables[j];

				if(input->built_in != (SpvBuiltIn)-1)		//We don't care about builtins
					continue;

				ESBType *inputType = isOutput ? outputs : inputs;
				U8 *inputSemantic = isOutput ? outputSemantics : inputSemantics;

				if(input->location >= 16)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() input/output location out of bounds (allowed up to 16)"
					))

				if(inputType[input->location])
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() input/output location is already defined"
					))

				gotoIfError3(clean, SpvReflectFormatToESBType(input->format, &inputType[input->location], e_rr))

				//Grab and parse semantic

				if(!input->name || !input->name[0])
					continue;

				CharString semantic = CharString_createRefCStrConst(input->name);
				CharString inVar = CharString_createRefCStrConst("in.var.");
				CharString outVar = CharString_createRefCStrConst("out.var.");

				Bool isInVar = CharString_startsWithStringSensitive(semantic, inVar, 0);
				Bool isOutVar = CharString_startsWithStringSensitive(semantic, outVar, 0);

				if(!isInVar && !isOutVar)
					continue;

				U64 offset = isInVar ? CharString_length(inVar) : CharString_length(outVar);
				semantic.ptr += offset;
				semantic.lenAndNullTerminated -= offset;

				U64 semanticValue = 0;

				U64 semanticl = CharString_length(semantic);
				U64 firstSemanticId = semanticl;

				while(firstSemanticId && C8_isDec(CharString_getAt(semantic, firstSemanticId - 1))) {
					--firstSemanticId;
				}

				if(!firstSemanticId)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() bitfield accidentally detected as semantic"
					))

				CharString semanticValueStr = CharString_createRefSizedConst(
					semantic.ptr + firstSemanticId, semanticl - firstSemanticId, true
				);

				CharString realSemanticName = CharString_createRefSizedConst(
					semantic.ptr, firstSemanticId, firstSemanticId == semanticl
				);

				if (firstSemanticId != semanticl && !CharString_parseDec(semanticValueStr, &semanticValue))
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() couldn't parse semantic value"
					))

				U64 semanticName = 0;

				if(
					!CharString_equalsStringInsensitive(realSemanticName, CharString_createRefCStrConst("SV_TARGET")) &&
					!CharString_equalsStringInsensitive(realSemanticName, CharString_createRefCStrConst("TEXCOORD"))
				) {
					U64 start = isOutput ? inputSemanticCount : 0;
					U64 end = isOutput ? strings.length : inputSemanticCount;
					U64 k = start;

					for(; k < end; ++k)
						if(CharString_equalsStringInsensitive(strings.ptr[k], realSemanticName))
							break;

					if(k == end)
						gotoIfError2(clean, ListCharString_pushBack(&strings, realSemanticName, alloc))

					if(!isOutput && k == end)
						++inputSemanticCount;

					semanticName = (k - start) + 1;
				}

				if(semanticName >= 16)
					retError(clean, Error_invalidState(
						1, "Compiler_processSPIRV() unique semantic name out of bounds"
					))

				if(semanticValue >= 15)
					retError(clean, Error_invalidState(
						1, "Compiler_processSPIRV() unique semantic id out of bounds"
					))

				semanticValue |= (U8)(semanticName << 4);
				inputSemantic[input->location] = !semanticName ? 0 : (U8) semanticValue;
			}
		}

		//Grab constant buffers

		for (U64 j = 0; j < entrypoint.descriptor_set_count; ++j) {

			SpvReflectDescriptorSet descriptorSet = entrypoint.descriptor_sets[j];

			for (U64 k = 0; k < descriptorSet.binding_count; ++k) {

				SpvReflectDescriptorBinding *binding = descriptorSet.bindings[k];

				if(!binding)
					continue;

				gotoIfError3(clean, Compiler_convertRegisterSPIRV(registers, binding, descriptorSet.set, alloc, e_rr))
			}
		}

		gotoIfError3(clean, Compiler_finalizeEntrypoint(
			localSize, payloadSize, intersectSize, 0,
			inputs, outputs,
			inputSemanticCount, &strings, inputSemantics, outputSemantics,
			CharString_length(toCompile.entrypoint) ? toCompile.entrypoint :
			CharString_createRefCStrConst(entrypoint.name),
			lock, entries,
			alloc, e_rr
		))
	}

	//Strip debug and optimize

	if(!settings.debug) {

		optimizer.SetMessageConsumer(
			[alloc](spv_message_level_t level, const C8 *source, const spv_position_t &position, const C8 *msg) -> void {
				
				const C8 *format = "%s:L#%zu:%zu (index: %zu): %s";

				switch(level) {

					case SPV_MSG_FATAL:
					case SPV_MSG_INTERNAL_ERROR:
					case SPV_MSG_ERROR:
						Log_errorLn(alloc, format, source, position.line, position.column, position.index, msg);
						break;

					case SPV_MSG_WARNING:
						Log_warnLn(alloc, format, source, position.line, position.column, position.index, msg);
						break;

					default:
						Log_debugLn(alloc, format, source, position.line, position.column, position.index, msg);
						break;
				}
			}
		);

		optimizer.RegisterPassesFromFlags({ "-O", "--legalize-hlsl" });
		optimizer.RegisterPass(spvtools::CreateStripDebugInfoPass()).RegisterPass(spvtools::CreateStripReflectInfoPass());

		std::vector<U32> tmp;

		if(!optimizer.Run((const U32*)resultPtr, binLen >> 2, &tmp))
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() stripping spirv failed"))

		Buffer_free(result, alloc);
		gotoIfError2(clean, Buffer_createCopy(Buffer_createRefConst(tmp.data(), (U64)tmp.size() << 2), alloc, result))
	}

clean:

	ListCharString_freeUnderlying(&strings, alloc);

	spvReflectDestroyShaderModule(&spvMod);
	return s_uccess;
}

Bool Compiler_compile(
	Compiler comp,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	SpinLock *lock,
	ListSHEntryRuntime entries,
	Allocator alloc,
	CompileResult *result,
	Error *e_rr
) {

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp.interfaces;

	Bool s_uccess = true;
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *error = NULL;
	IDxcBlob *resultBlob = NULL;
	Bool hasErrors = false;
	CharString tempStr = CharString_createNull();
	ListCharString stringsUTF8 = ListCharString{};		//One day, Microsoft will fix their stuff, I hope.

	Compiler_defineStrings;

	if(!interfaces->utils || !result)
		retError(clean, Error_alreadyDefined(!interfaces->utils ? 0 : 2, "Compiler_compile()::comp is required"))

	if(!CharString_length(settings.string))
		retError(clean, Error_invalidParameter(1, 0, "Compiler_compile()::settings.string is required"))

	if(toCompile.uniforms.length & 1)
		retError(clean, Error_invalidParameter(2, 0, "Compiler_compile()::toCompile.uniforms.length should be aligned to 2"))

	if(settings.outputType >= ESHBinaryType_Count || settings.format >= ECompilerFormat_Count)
		retError(clean, Error_invalidParameter(1, 0, "Compiler_compile()::settings contains invalid format or outputType"))

	gotoIfError3(clean, Compiler_setupIncludePaths(&stringsUTF8, settings, alloc, e_rr))

	try {

		interfaces->includeHandler->reset();		//Ensure we don't reuse stale caches

		result->isSuccess = false;

		U32 lastExtension = 0;

		for(U32 i = 0; i < ESHExtension_Count; ++i)
			if ((toCompile.extensions >> i) & 1)
				lastExtension = i + 1;

		if(toCompile.stageType >= ESHPipelineStage_RtStartExt && toCompile.stageType <= ESHPipelineStage_RtEndExt)
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC_EXT_RAYTRACING", alloc, e_rr))

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Zpc", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, settings.debug ? "-Od" : "-O3", alloc, e_rr))

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-HV", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "202x", alloc, e_rr))

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Wconversion", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Wdouble-promotion", alloc, e_rr))

		if(settings.debug)
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Zi", alloc, e_rr))

		if(toCompile.extensions & ESHExtension_16BitTypes)
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-enable-16bit-types", alloc, e_rr))

		if (settings.outputType == ESHBinaryType_SPIRV) {

			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-spirv", alloc, e_rr))
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-use-dx-layout", alloc, e_rr))
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-target-env=vulkan1.2", alloc, e_rr))

			if(
				toCompile.stageType == ESHPipelineStage_Vertex ||
				toCompile.stageType == ESHPipelineStage_Domain ||
				toCompile.stageType == ESHPipelineStage_GeometryExt ||
				toCompile.stageType == ESHPipelineStage_MeshExt
			)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-invert-y", alloc, e_rr))

			else if(toCompile.stageType == ESHPipelineStage_Pixel)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-use-dx-position-w", alloc, e_rr))

			if(CharString_length(toCompile.entrypoint))
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-entrypoint-name=main", alloc, e_rr))

			gotoIfError3(clean, Compiler_registerArgCStr(
				&stringsUTF8, "-fspv-extension=SPV_EXT_descriptor_indexing", alloc, e_rr
			))

			if(
				toCompile.stageType >= ESHPipelineStage_RtStartExt &&
				toCompile.stageType <= ESHPipelineStage_RtEndExt
			)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_ray_tracing", alloc, e_rr
				))

			if(
				toCompile.stageType == ESHPipelineStage_MeshExt ||
				toCompile.stageType == ESHPipelineStage_TaskExt
			)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_EXT_mesh_shader", alloc, e_rr
				))

			if(toCompile.extensions & ESHExtension_ComputeDeriv)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_compute_shader_derivatives", alloc, e_rr
				))

			if(toCompile.extensions & ESHExtension_16BitTypes)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_16bit_storage", alloc, e_rr
				))
			
			if(toCompile.extensions & ESHExtension_Multiview)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_multiview", alloc, e_rr
				))
			
			if(toCompile.extensions & ESHExtension_RayReorder)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_shader_invocation_reorder", alloc, e_rr
				))
			
			if(toCompile.extensions & ESHExtension_RayMotionBlur)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_ray_tracing_motion_blur", alloc, e_rr
				))
			
			if(toCompile.extensions & ESHExtension_RayQuery)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_ray_query", alloc, e_rr
				))
			
			if(toCompile.extensions & ESHExtension_RayMicromapOpacity)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_EXT_opacity_micromap", alloc, e_rr
				))
			
			if(toCompile.extensions & ESHExtension_RayMicromapDisplacement)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_displacement_micromap", alloc, e_rr
				))
			
			if(toCompile.extensions & (ESHExtension_AtomicF32 | ESHExtension_AtomicF64)) {

				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_EXT_shader_atomic_float_add", alloc, e_rr
				))

				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_EXT_shader_atomic_float_min_max", alloc, e_rr
				))
			}
		}

		else {

			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Qstrip_debug", alloc, e_rr))
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Qstrip_reflect", alloc, e_rr))
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-auto-binding-space", alloc, e_rr))
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "0", alloc, e_rr))

			if(toCompile.extensions & ESHExtension_PAQ)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-enable-payload-qualifiers", alloc, e_rr))
		}

		//-E <entrypointName>

		if (CharString_length(toCompile.entrypoint)) {
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-E", alloc, e_rr))
			gotoIfError3(clean, Compiler_registerArgStrConst(&stringsUTF8, toCompile.entrypoint, alloc, e_rr))
		}

		//-T <target>
		
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-T", alloc, e_rr))

		const C8 *targetPrefix = ESHPipelineStage_getStagePrefix((ESHPipelineStage) toCompile.stageType);

		U32 major = toCompile.shaderVersion >> 8;
		U32 minor = (U8)toCompile.shaderVersion;

		gotoIfError2(clean, CharString_format(alloc, &tempStr, "%s_%" PRIu32"_%" PRIu32, targetPrefix, major, minor))
		gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr))
		tempStr = CharString_createNull();

		//$<X> foreach uniform

		for(U32 i = 0; i < toCompile.uniforms.length; i += 2) {

			CharString uniformName  = toCompile.uniforms.ptr[i];
			CharString uniformValue = toCompile.uniforms.ptr[i + 1];

			gotoIfError2(clean, CharString_format(

				alloc, &tempStr,

				!CharString_length(uniformValue) ? "-D$%.*s" : "-D$%.*s=%.*s", 

				(int) CharString_length(uniformName),
				uniformName.ptr,

				(int) CharString_length(uniformValue),
				uniformValue.ptr
			))

			gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr))
			tempStr = CharString_createNull();
		}

		//__OXC_EXT_<X> foreach extension

		for(U32 i = 0; i < lastExtension; ++i)
			if ((toCompile.extensions >> i) & 1) {
				gotoIfError2(clean, CharString_format(alloc, &tempStr, "-D__OXC_EXT_%s", ESHExtension_defines[i]))
				gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr))
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
			gotoIfError2(clean, CharString_format(alloc, &tempStr, formats[i], formatInts[i]))
			gotoIfError2(clean, ListCharString_pushBack(&stringsUTF8, tempStr, alloc))
			tempStr = CharString_createNull();
		}

		Compiler_convertToWString(stringsUTF8, clean)

		//Compile

		DxcBuffer buffer{
			.Ptr = settings.string.ptr,
			.Size = CharString_length(settings.string),
			.Encoding = DXC_CP_UTF8
		};

		HRESULT hr = interfaces->compiler->Compile(
			&buffer,
			(LPCWSTR*) strings.ptr, (int) strings.length,
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
			Buffer_createRefConst(resultBlob->GetBufferPointer(), resultBlob->GetBufferSize()),
			alloc,
			&result->binary
		))

		if (settings.outputType == ESHBinaryType_DXIL) {
		
			resultBlob->Release();
			resultBlob = NULL;
			hr = dxcResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&resultBlob), NULL);

			if (FAILED(hr) || !resultBlob)
				retError(clean, Error_invalidState(0, "Compiler_processDXIL() fetch reflection failed"))

			//Convert to reflection data

			DxcBuffer reflectionData = DxcBuffer{
				.Ptr = resultBlob->GetBufferPointer(),
				.Size = resultBlob->GetBufferSize(),
				.Encoding = 0
			};

			gotoIfError3(clean, Compiler_processDXIL(
				&result->binary, &result->registers, reflectionData, interfaces, toCompile, lock, entries, alloc, e_rr
			))
		}

		else if (settings.outputType == ESHBinaryType_SPIRV)
			gotoIfError3(clean, Compiler_processSPIRV(
				&result->binary, &result->registers, settings, toCompile, lock, entries, alloc, e_rr
			))

		else retError(clean, Error_invalidState(2, "Compiler_compile() unsupported type. Only supporting DXIL and SPIRV"))

		if (settings.infoAboutIncludes)
			gotoIfError3(clean, Compiler_copyIncludes(result, interfaces->includeHandler, alloc, e_rr))

		result->type = ECompileResultType_Binary;
		result->isSuccess = true;

	} catch (std::exception&) {
		retError(clean, Error_invalidState(1, "Compiler_compile() raised an internal exception"))
	}

clean:

	if(!s_uccess && result)
		CompileResult_free(result, alloc);

	if(dxcResult)
		dxcResult->Release();

	if(error)
		error->Release();

	Compiler_freeStrings;
	CharString_free(&tempStr, alloc);
	ListCharString_freeUnderlying(&stringsUTF8, alloc);
	return s_uccess;
}

Bool Compiler_createDisassembly(Compiler comp, ESHBinaryType type, Buffer buf, Allocator alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	U64 binLen = Buffer_length(buf);
	const void *resultPtr = buf.ptr;
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *blobUtf8 = NULL;

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp.interfaces;

	switch (type) {

		case ESHBinaryType_SPIRV: {

			if(
				binLen < 0x8 ||
				(binLen & 3) ||
				*(const U32*)resultPtr != 0x07230203
			)
				retError(clean, Error_invalidState(0, "Compiler_createDisassembly() SPIRV is invalid"))

			spvtools::SpirvTools tool{ SPV_ENV_VULKAN_1_2 };

			spv_binary_to_text_options_t opts = (spv_binary_to_text_options_t) (
				SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
				SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT |
				SPV_BINARY_TO_TEXT_OPTION_REORDER_BLOCKS |
				SPV_BINARY_TO_TEXT_OPTION_COMMENT |
				SPV_BINARY_TO_TEXT_OPTION_SHOW_BYTE_OFFSET |
				SPV_BINARY_TO_TEXT_OPTION_INDENT
			);

			std::string str;
			if(!tool.Disassemble((const U32*)resultPtr, binLen >> 2, &str, opts))
				retError(clean, Error_invalidOperation(0, "Compiler_createDisassembly() SPIRV couldn't be disassembled"))

			gotoIfError2(clean, CharString_createCopy(
				CharString_createRefSizedConst(str.c_str(), str.size(), true), alloc, result
			))

			break;
		}

		case ESHBinaryType_DXIL: {
		
			if(
				binLen <= 0x14 ||
				*(const U32*)resultPtr != C8x4('D', 'X', 'B', 'C')
			)
				retError(clean, Error_invalidState(0, "Compiler_createDisassembly() DXIL is invalid"))

			DxcBuffer buffer {
				.Ptr = resultPtr,
				.Size = binLen,
				.Encoding = 0
			};

			HRESULT hr = interfaces->compiler->Disassemble(
				&buffer,
				IID_PPV_ARGS(&dxcResult)
			);

			if(FAILED(hr))
				retError(clean, Error_invalidOperation(0, "Compiler_createDisassembly() DXIL couldn't be disassembled"))

			hr = dxcResult->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&blobUtf8), NULL);

			if(FAILED(hr))
				retError(clean, Error_invalidOperation(1, "Compiler_createDisassembly() DXIL disassembly couldn't be obtained"))

			CharString str = CharString_createRefSizedConst(blobUtf8->GetStringPointer(), blobUtf8->GetStringLength(), false);
			gotoIfError2(clean, CharString_createCopy(str, alloc, result))

			break;
		}

		default:
			retError(clean, Error_unimplemented(0, "Compiler_createDisassembly() has invalid type"))
	}

clean:

	if(dxcResult)
		dxcResult->Release();

	if(blobUtf8)
		blobUtf8->Release();

	return s_uccess;
}
