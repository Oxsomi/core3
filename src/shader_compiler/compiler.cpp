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
#include "types/platform_types.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <Unknwn.h>
#endif

#define ENABLE_DXC_STATIC_LINKING
#include <dxcompiler/dxcapi.h>
#include <spirv-tools/optimizer.hpp>
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

		//Little hack to handle builtin shaders, by using "virtual files" //myTest.hlsl

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

				if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("@resources.hlsl")))
					tmpTmp = CharString_createRefCStrConst(resources);

				else if(CharString_equalsStringInsensitive(resolved, CharString_createRefCStrConst("@types.hlsl")))
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

SpinLock lockThread = SpinLock{ .lockedThreadId = { 0 }, .active = true, .pad = { 0 } };
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
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC3", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-HV", alloc, e_rr))
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "202x", alloc, e_rr))

		if (settings.outputType == ESHBinaryType_SPIRV)		//-spirv
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-spirv", alloc, e_rr))

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

U64 compileId = 0;

Bool Compiler_compile(
	Compiler comp,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
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
	spvtools::Optimizer optimizer{ SPV_ENV_VULKAN_1_2 };

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

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC3", alloc, e_rr))
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
				toCompile.stageType == ESHPipelineStage_GeometryExt
			)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-invert-y", alloc, e_rr))

			else if(toCompile.stageType == ESHPipelineStage_Pixel)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-use-dx-position-w", alloc, e_rr))

			if(CharString_length(toCompile.entrypoint))
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-entrypoint-name=main", alloc, e_rr))
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

		//__OXC3_EXT_<X> foreach extension

		for(U32 i = 0; i < lastExtension; ++i)
			if ((toCompile.extensions >> i) & 1) {
				gotoIfError2(clean, CharString_format(alloc, &tempStr, "-D__OXC3_EXT_%s", ESHExtension_defines[i]))
				gotoIfError3(clean, Compiler_registerArgStr(&stringsUTF8, tempStr, alloc, e_rr))
				tempStr = CharString_createNull();
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

		//Prevent dxil.dll load, sign it ourselves :)
		if (settings.outputType == ESHBinaryType_DXIL) {

			//Ensure we have a valid DXIL file

			if(
				Buffer_length(result->binary) <= 0x14 ||
				*(const U32*)result->binary.ptr != C8x4('D', 'X', 'B', 'C')
			)
				retError(clean, Error_invalidState(2, "Compiler_compile() DXIL returned is invalid"))
			
			//Offset to beyond hash is used to create messed up MD5

			Buffer tmp = Buffer_createRefFromBuffer(result->binary, true);
			Buffer_offset(&tmp, 0x14);
			I32x4 hash = Buffer_md5dxc(tmp);

			//Copy into file, it is now signed :)

			Buffer_copy(
				Buffer_createRef((U8*)result->binary.ptr + sizeof(U32), sizeof(hash)),
				Buffer_createRefConst(&hash, sizeof(hash))
			);
		}

		else if (settings.outputType == ESHBinaryType_SPIRV) {
		
			//Ensure we have a valid SPIRV file

			U64 binLen = Buffer_length(result->binary);

			if(
				binLen < 0x8 ||
				(binLen & 3) ||
				*(const U32*)result->binary.ptr != 0x07230203
			)
				retError(clean, Error_invalidState(2, "Compiler_compile() SPIRV returned is invalid"))

			if(!settings.debug) {

				const void *resultPtr = result->binary.ptr;

				optimizer.SetMessageConsumer(
					[](spv_message_level_t level, const C8 *source, const spv_position_t &position, const C8 *msg) -> void {
				
						const C8 *format = "%s:L#%zu:%zu (index: %zu): %s";

						switch(level) {

							case SPV_MSG_FATAL:
							case SPV_MSG_INTERNAL_ERROR:
							case SPV_MSG_ERROR:
								Log_errorLnx(format, source, position.line, position.column, position.index, msg);
								break;

							case SPV_MSG_WARNING:
								Log_warnLnx(format, source, position.line, position.column, position.index, msg);
								break;

							default:
								Log_debugLnx(format, source, position.line, position.column, position.index, msg);
								break;
						}
					}
				);

				optimizer.RegisterPassesFromFlags({ "-O", "--legalize-hlsl" });
				optimizer.RegisterPass(spvtools::CreateStripDebugInfoPass()).RegisterPass(spvtools::CreateStripReflectInfoPass());

				std::vector<U32> tmp;

				if(!optimizer.Run((const U32*)resultPtr, binLen >> 2, &tmp))
					retError(clean, Error_invalidState(0, "Compiler_compile() stripping spirv failed"))

				Buffer_free(&result->binary, alloc);
				gotoIfError2(clean, Buffer_createCopy(Buffer_createRefConst(tmp.data(), (U64)tmp.size() << 2), alloc, &result->binary))
			}
		}

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
