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

//shader_compiler/compiler.cpp

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

const C8 *nvHLSLExtns3 =
	#include "nvHLSLExtns3.h"
	;

const C8 *nvShaderExtnEnums =
	#include "nvShaderExtnEnums.h"
	;

const C8 *nvHLSLExtnsInternal =
	#include "nvHLSLExtnsInternal.h"
	;

const C8 *nvHLSLExtnsInternal2 =
	#include "nvHLSLExtnsInternal2.h"
	;

//This file is only because DXC doesn't have a C interface.
//So we need to wrap C++ in C, so we can call it from C.

typedef class IncludeHandler IncludeHandler;

typedef struct CompilerInterfaces {        //Also defined in compiler_dxil
	IDxcUtils *utils;
	IDxcCompiler3 *compiler;
	IncludeHandler *includeHandler;
	IHLSLReflector *reflector;
} CompilerInterfaces;

class IncludeHandler : public IDxcIncludeHandler {

	IDxcUtils *utils;
	ListIncludedFile includedFiles{};
	ListU64 isPresent{};
	const Allocator *alloc;
	U64 counter{};            //Unique file counter in the current file

public:

	inline IncludeHandler(IDxcUtils *utils, const Allocator *alloc): utils(utils), alloc(alloc) { }

	//Useful so includes can be cached instead of re-fetched from file each time.
	//This has to be called in between compiles to ensure the include handler knows it's the first time re-using.
	inline void reset() {

		counter = 0;

		for (U64 i = 0; i < includedFiles.length; ++i)
			includedFiles.ptrNonConst[i].includeInfo.counter = 0;

		Buffer_unsetAllBits(ListU64_buffer(isPresent), NULL);
	}

	inline U64 getCounter() const { return counter; }
	inline ListIncludedFile getIncludedFiles() const {
		ListIncludedFile res = ListIncludedFile{};
		ListIncludedFile_createRefConst(includedFiles.ptr, includedFiles.length, &res, NULL);
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

		const RefPtrType fileHandleType = FileHandle_makeType(alloc);
		HRESULT hr = S_OK;
		Buffer tempBuffer = Buffer_createNull();
		CharString tempFile = CharString_createNull();
		FileInfo fileInfo = FileInfo{};
		Bool isVirtual = false;
		Bool isBuiltin = false;
		U64 i = 0;
		U64 lastAt = U64_MAX;

		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			gotoIfError3(clean, CharString_createFromUTF16((const U16*)fileNameStr, U64_MAX, alloc, &fileName, e_rr));
		#else
			gotoIfError3(clean, CharString_createFromUTF32((const U32*)fileNameStr, U64_MAX, alloc, &fileName, e_rr));
		#endif

		//Little hack to handle builtin shaders, by using "virtual files" //myTest.hlsli

		const CharString atStr = CharString_createRefCStrConst("@");
		lastAt = CharString_findLastStringSensitive(&fileName, &atStr, 0, 0);
		isBuiltin = lastAt != U64_MAX;

		if (isBuiltin) {

			CharString tmp = CharString_createNull();

			if(!CharString_cut(&fileName, lastAt, 0, &tmp) || !CharString_length(tmp))
				retError(clean, Error_invalidState(0, "IncludeHandler::LoadSource expected source after @"));

			gotoIfError3(clean, CharString_createCopy(tmp, alloc, &resolved, e_rr));
		}

		else gotoIfError3(clean, File_resolve(
			&fileName, &isVirtual, 256, &Platform_instance->defaultDir, alloc, &resolved, e_rr
		));

		for (; i < includedFiles.length; ++i)
			if(CharString_equalsStringSensitive(&resolved, &includedFiles.ptr[i].includeInfo.file))
				break;

		//We already included it in an earlier run.
		//This could mean it's either the same file (and thus needs to be empty) or it's in cache.

		if (i != includedFiles.length) {

			//We need to validate the cache first
			//It's possible the compiler exists so long that files on disk have been changed

			Bool validCache = true;

			if(!isBuiltin) {        //Builtins don't exist on disk, so they can't really be hot reloaded

				gotoIfError3(clean, File_getInfo(&resolved, &fileInfo, alloc, e_rr));

				IncludeInfo prevInclude = includedFiles.ptr[i].includeInfo;

				//When timestamp has changed, it might be dirty.
				//When fileSize has, it definitely is dirty.

				if (fileInfo.timestamp != prevInclude.timestamp || fileInfo.fileSize != prevInclude.fileSize) {

					//Unfortunately peanut butter, we have to hash the whole file.
					//Luckily, CRC32C is quite fast.

					if (fileInfo.fileSize == prevInclude.fileSize) {

						gotoIfError3(clean, File_read(&resolved, 100 * MS, 0, 0, &fileHandleType, &tempBuffer, e_rr));

						gotoIfError3(clean, CharString_createCopy(
							CharString_createRefSizedConst((const C8*)tempBuffer.ptr, Buffer_length(tempBuffer), false),
							alloc,
							&tempFile,
							e_rr
						));

						Buffer_free(&tempBuffer, alloc);

						if(!CharString_eraseAllSensitive(&tempFile, '\r', 0, 0))
							retError(clean, Error_invalidState(0, "IncludeHandler::LoadSource couldn't erase \\rs"));

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
				gotoIfError3(clean, ListIncludedFile_popLocation(&includedFiles, i, &includedFile, e_rr));

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

				gotoIfError3(clean, File_getInfo(&resolved, &fileInfo, alloc, e_rr));
				gotoIfError3(clean, File_read(&resolved, 100 * MS, 0, 0, &fileHandleType, &tempBuffer, e_rr));

				Ns timestamp = fileInfo.timestamp;
				FileInfo_free(&fileInfo, alloc);

				if(Buffer_length(tempBuffer) >> 32)
					retError(clean, Error_outOfBounds(
						0, Buffer_length(tempBuffer), U32_MAX,
						"IncludeHandler::LoadSource CreateBlobFromPinned requires 32-bit buffers max"
					));

				gotoIfError3(clean, CharString_createCopy(
					CharString_createRefSizedConst((const C8*)tempBuffer.ptr, Buffer_length(tempBuffer), false),
					alloc,
					&tempFile,
					e_rr
				));

				if(!CharString_eraseAllSensitive(&tempFile, '\r', 0, 0))
					retError(clean, Error_invalidState(1, "IncludeHandler::LoadSource couldn't erase \\rs"));

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

				gotoIfError3(clean, ListIncludedFile_pushBack(&includedFiles, inc, alloc, e_rr));
				resolved = CharString_createNull();
				tempFile = CharString_createNull();

			} else {

				CharString tmpTmp = CharString_createNull();

				if(CharString_equalsCStringInsensitive(&resolved, "@resources.hlsli"))
					tmpTmp = CharString_createRefCStrConst(resources);

				else if(CharString_equalsCStringInsensitive(&resolved, "@types.hlsli"))
					tmpTmp = CharString_createRefCStrConst(types);

				else if(CharString_equalsCStringInsensitive(&resolved, "@nvShaderExtnEnums.h"))
					tmpTmp = CharString_createRefCStrConst(nvShaderExtnEnums);

				else if(CharString_equalsCStringInsensitive(&resolved, "@nvHLSLExtnsInternal.h")) {
					
					//Because of the C limit of 64KiB per string constant, we need two string constants and merge them

					CharString tmp = CharString_createRefCStrConst(nvHLSLExtnsInternal);
					gotoIfError3(clean, CharString_createCopy(tmp, alloc, &tempFile, e_rr));

					tmp = CharString_createRefCStrConst(nvHLSLExtnsInternal2);
					gotoIfError3(clean, CharString_appendString(&tempFile, &tmp, alloc, e_rr));
				}

				else if(CharString_equalsCStringInsensitive(&resolved, "@nvHLSLExtns.h")) {

					//Because of the C limit of 64KiB per string constant, we need two string constants and merge them

					CharString tmp = CharString_createRefCStrConst(nvHLSLExtns);
					gotoIfError3(clean, CharString_createCopy(tmp, alloc, &tempFile, e_rr));

					tmp = CharString_createRefCStrConst(nvHLSLExtns2);
					gotoIfError3(clean, CharString_appendString(&tempFile, &tmp, alloc, e_rr));

					tmp = CharString_createRefCStrConst(nvHLSLExtns3);
					gotoIfError3(clean, CharString_appendString(&tempFile, &tmp, alloc, e_rr));
				}

				else retError(clean, Error_notFound(0, 0, "IncludeHandler::LoadSource builtin file not found"));

				if(tmpTmp.ptr)
					gotoIfError3(clean, CharString_createCopy(tmpTmp, alloc, &tempFile, e_rr));

				if(!CharString_eraseAllSensitive(&tempFile, '\r', 0, 0))
					retError(clean, Error_invalidState(1, "IncludeHandler::LoadSource couldn't erase \\rs"));

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

				gotoIfError3(clean, ListIncludedFile_pushBack(&includedFiles, inc, alloc, e_rr));
				resolved = CharString_createNull();
				tempFile = CharString_createNull();
			}

			if((i >> 6) >= isPresent.length)
				gotoIfError3(clean, ListU64_pushBack(&isPresent, 0, alloc, e_rr));

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

	ULONG STDMETHODCALLTYPE AddRef() override {    return 0; }
	ULONG STDMETHODCALLTYPE Release() override { return 0; }
};

SpinLock lockThread = { 0 };
Bool hasInitialized = false;

Bool Compiler_setup(Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = SpinLock_lock(&lockThread, 0);

	if (acq >= ELockAcquire_Success) {        //First to lock is first to initialize

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
		retError(clean, Error_invalidState(0, "Compiler_setup() one of the other threads couldn't initialize DXC"));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&lockThread);

	return s_uccess;
}

Bool Compiler_create(const Allocator *alloc, Compiler *comp, Error *e_rr) {

	Bool s_uccess = true;
	CompilerInterfaces *interfaces = NULL;

	gotoIfError3(clean, Compiler_setup(e_rr));

	if(!comp)
		retError(clean, Error_nullPointer(1, "Compiler_create()::comp is required"));

	interfaces = (CompilerInterfaces*) comp->interfaces;

	if(interfaces->utils)
		retError(clean, Error_alreadyDefined(1, "Compiler_create()::comp must be empty"));

	try {

		//TODO: Call DxcCreateInstance2 with custom IMalloc.
		//Problem; we don't know the size of any allocation.
		//Would require each allocation to lock and push in ListDebugAllocation, even in Release mode!

		//IMalloc *malloc = ...;

		//Create utils

		HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&interfaces->utils));

		if(FAILED(hr))
			retError(clean, Error_invalidState(0, "Compiler_create() IDxcUtils couldn't be created"));

		//Create reflector

		hr = DxcCreateInstance(CLSID_DxcReflector, IID_PPV_ARGS(&interfaces->reflector));

		if(FAILED(hr))
			retError(clean, Error_invalidState(0, "Compiler_create() IHLSLReflector couldn't be created"));

		//Create include handler

		interfaces->includeHandler = new IncludeHandler(interfaces->utils, alloc);

		//Create compiler

		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&interfaces->compiler));

		if(FAILED(hr))
			retError(clean, Error_invalidState(2, "Compiler_create() IDxcCompiler3 couldn't be created"));

	} catch (...) {
		retError(clean, Error_invalidState(1, "Compiler_create() raised an internal exception"));
	}

clean:

	if(!s_uccess)
		Compiler_free(comp, alloc);

	return s_uccess;
}

void Compiler_free(Compiler *comp, const Allocator *alloc) {

	(void)alloc;

	if(!comp)
		return;

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;

	if(interfaces->utils)
		interfaces->utils->Release();

	if(interfaces->reflector)
		interfaces->reflector->Release();

	if(interfaces->compiler)
		interfaces->compiler->Release();

	if(interfaces->includeHandler)
		delete interfaces->includeHandler;

	*comp = Compiler{};
}

Bool Compiler_mergeIncludeInfo(Compiler *comp, const Allocator *alloc, ListIncludeInfo *infos, Error *e_rr) {

	Bool s_uccess = true;
	CompilerInterfaces *interfaces = NULL;

	CharString tmp = CharString_createNull();
	ListIncludedFile files = ListIncludedFile{};

	if(!comp || !infos)
		retError(clean, Error_nullPointer(!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp and infos are required"));

	interfaces = (CompilerInterfaces*) comp->interfaces;

	if(!interfaces->includeHandler)
		retError(clean, Error_nullPointer(
			!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp->interfaces includeHandler is missing"
		));

	files = interfaces->includeHandler->getIncludedFiles();

	for (U64 i = 0; i < files.length; ++i) {

		IncludedFile info = files.ptr[i];
		U64 j = 0;

		//If the file isn't the same, we can't merge it.
		//In this case, the file size, hash and others can differ.
		//This means we might have duplicate entries by file name, but is required to accurately represent the state.

		for(; j < infos->length; ++j)
			if(
				CharString_equalsStringSensitive(&infos->ptr[j].file, &info.includeInfo.file) &&
				infos->ptr[j].crc32c == info.includeInfo.crc32c && infos->ptr[j].fileSize == info.includeInfo.fileSize
			)
				break;

		//Add new entry

		if(j == infos->length) {

			gotoIfError3(clean, CharString_createCopy(info.includeInfo.file, alloc, &tmp, e_rr));

			info.includeInfo.counter = info.globalCounter;
			info.includeInfo.file = tmp;
			gotoIfError3(clean, ListIncludeInfo_pushBack(infos, info.includeInfo, alloc, e_rr));
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
		DxcShutdown();
	} catch(...){}

	hasInitialized = false;

clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&lockThread);
}

//What's wrong with UTF8?

#if _PLATFORM_TYPE == PLATFORM_WINDOWS

	#define Compiler_defineStrings                                                                        \
		ListU16 tempStrUTF16 = ListU16{};                                                                \
		ListListU16 stringsUTF16 = ListListU16{};                                                        \
		ListU16PtrConst strings = ListU16PtrConst{}

	#define Compiler_freeStrings                                                                        \
		ListU16_free(&tempStrUTF16, alloc);                                                                \
		ListListU16_freeUnderlying(&stringsUTF16, alloc);                                                \
		ListU16PtrConst_free(&strings, alloc)

	#define Compiler_convertToWString(strs, label)                                                         \
		for(U64 ii = 0; ii < strs.length; ++ii) {                                                        \
			gotoIfError3(label, CharString_toUTF16(strs.ptr[ii], alloc, &tempStrUTF16, e_rr));                    \
			gotoIfError3(label, ListU16PtrConst_pushBack(&strings, tempStrUTF16.ptr, alloc, e_rr));            \
			gotoIfError3(label, ListListU16_pushBack(&stringsUTF16, tempStrUTF16, alloc, e_rr));                \
			tempStrUTF16 = ListU16{};                                                                    \
		}
#else
	#define Compiler_defineStrings                                                                        \
		ListU32 tempStrUTF32 = ListU32{};                                                                \
		ListListU32 stringsUTF32 = ListListU32{};                                                        \
		ListU32PtrConst strings = ListU32PtrConst{}

	#define Compiler_freeStrings                                                                        \
		ListU32_free(&tempStrUTF32, alloc);                                                                \
		ListListU32_freeUnderlying(&stringsUTF32, alloc);                                                \
		ListU32PtrConst_free(&strings, alloc)

	#define Compiler_convertToWString(strs, label)                                                         \
		for(U64 ii = 0; ii < strs.length; ++ii) {                                                        \
			gotoIfError3(label, CharString_toUTF32(strs.ptr[ii], alloc, &tempStrUTF32, e_rr));                    \
			gotoIfError3(label, ListU32PtrConst_pushBack(&strings, tempStrUTF32.ptr, alloc, e_rr));            \
			gotoIfError3(label, ListListU32_pushBack(&stringsUTF32, tempStrUTF32, alloc, e_rr));                \
			tempStrUTF32 = ListU32{};                                                                    \
		}
#endif

Bool Compiler_setupIncludePaths(ListCharString *dst, const CompilerSettings *settings, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	Bool isVirtual = false;

	//-I x per include dir

	for(U64 i = 0; i < settings->includeDirs.length; ++i) {

		CharString includeDir = settings->includeDirs.ptr[i];

		if(!CharString_length(includeDir))
			continue;

		gotoIfError3(clean, File_resolve(
			&includeDir, &isVirtual, 256, &Platform_instance->defaultDir, alloc, &tempStr, e_rr
		));

		gotoIfError3(clean, ListCharString_pushBack(dst, CharString_createRefCStrConst("-I"), alloc, e_rr));
		gotoIfError3(clean, ListCharString_pushBack(dst, tempStr, alloc, e_rr));
		tempStr = CharString_createNull();
	}

	//<file> -I <file's parent> to resolve errors to the origin file and use relative includes

	if(CharString_length(settings->path)) {

		gotoIfError3(clean, File_resolve(
			&settings->path, &isVirtual, 256, &Platform_instance->defaultDir, alloc, &tempStr, e_rr
		));

		gotoIfError3(clean, ListCharString_pushBack(dst, tempStr, alloc, e_rr));
		tempStr = CharString_createRefStrConst(tempStr);

		if(!CharString_cutAfterLastSensitive(&tempStr, '/', &tempStr2))
			retError(clean, Error_invalidState(0, "Compiler_setupIncludePaths() can't find parent directory"));

		gotoIfError3(clean, ListCharString_pushBack(dst, CharString_createRefCStrConst("-I"), alloc, e_rr));
		gotoIfError3(clean, ListCharString_pushBack(dst, tempStr2, alloc, e_rr));
		tempStr2 = tempStr = CharString_createNull();
	}

clean:
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	return s_uccess;
}

Bool Compiler_copyIncludes(CompileResult *result, IncludeHandler *includeHandler, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tempStr = CharString_createNull();
	ListIncludedFile files = ListIncludedFile{};

	gotoIfError3(clean, ListIncludeInfo_resize(&result->includeInfo, includeHandler->getCounter(), alloc, e_rr));

	files = includeHandler->getIncludedFiles();

	for(U64 i = 0, j = 0; i < files.length; ++i)
		if (files.ptr[i].includeInfo.counter) {        //Exclude inactive includes

			IncludeInfo copy = files.ptr[i].includeInfo;
			gotoIfError3(clean, CharString_createCopy(copy.file, alloc, &tempStr, e_rr));

			copy.file = tempStr;
			result->includeInfo.ptrNonConst[j] = copy;
			tempStr = CharString_createNull();

			++j;
		}

clean:
	CharString_free(&tempStr, alloc);
	return s_uccess;
}

Bool Compiler_registerArgStr(ListCharString *strings, CharString str, const Allocator *alloc, Error *e_rr) {
	Bool s_uccess = true;
	gotoIfError3(clean, ListCharString_pushBack(strings, str, alloc, e_rr));
clean:
	return s_uccess;
}

Bool Compiler_registerArgStrConst(ListCharString *strings, CharString str, const Allocator *alloc, Error *e_rr) {
	return Compiler_registerArgStr(strings, CharString_createRefStrConst(str), alloc, e_rr);
}

//Only with const C8* that will always be in mem
Bool Compiler_registerArgCStr(ListCharString *strings, const C8 *str, const Allocator *alloc, Error *e_rr) {
	return Compiler_registerArgStr(strings, CharString_createRefCStrConst(str), alloc, e_rr);
}

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

		interfaces->includeHandler->reset();        //Ensure we don't reuse stale caches

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

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fhlsl-unused-resource-bindings=reserve-all", alloc, e_rr));

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-D__OXC", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Zpc", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-O3", alloc, e_rr));

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-HV", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "202x", alloc, e_rr));

		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Wconversion", alloc, e_rr));
		gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Wdouble-promotion", alloc, e_rr));

		if(settings->debug || requiresLink) {
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Zi", alloc, e_rr));
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-Qembed_debug", alloc, e_rr));
		}

		if(toCompile->extensions & ESHExtension_16BitTypes)
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-enable-16bit-types", alloc, e_rr));

		if (settings->outputType == ESHBinaryType_SPIRV) {

			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-spirv", alloc, e_rr));
			gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fvk-use-dx-layout", alloc, e_rr));

			if(settings->debug)
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-debug=vulkan-with-source", alloc, e_rr));

			if(!isRt) {
				gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-target-env=vulkan1.1", alloc, e_rr));
			}

			else gotoIfError3(clean, Compiler_registerArgCStr(&stringsUTF8, "-fspv-target-env=vulkan1.1spirv1.4", alloc, e_rr));
				
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

			if(toCompile->extensions & ESHExtension_RayReorder)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_shader_invocation_reorder", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_RayMotionBlur)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_NV_ray_tracing_motion_blur", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_RayQuery)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_KHR_ray_query", alloc, e_rr
				));

			if(toCompile->extensions & ESHExtension_RayMicromapOpacity)
				gotoIfError3(clean, Compiler_registerArgCStr(
					&stringsUTF8, "-fspv-extension=SPV_EXT_opacity_micromap", alloc, e_rr
				));

			//NOTE: F32/F64 atomics have no native HLSL intrinsic and are expressed via inline SPIR-V
			//([[vk::ext_extension("SPV_EXT_shader_atomic_float_add")]] on the atomic function). That inline
			//attribute already declares the extension, and passing -fspv-extension=SPV_EXT_shader_atomic_float_add
			//makes DXC fail with "unknown SPIR-V extension" (it isn't in DXC's -fspv-extension whitelist). So we
			//deliberately do NOT whitelist it here; DXC emits OpAtomicFAddEXT from the inline declaration.
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
				ETypeId type = ETypeId_arr[uniform.typeIdShort];

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

					gotoIfError3(clean, CharString_format(alloc, &tempStr1, e_rr, "[[vk::constant_id(%" PRIu64 ")]] const %s $$specConst_%.*s = %s;\n",
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
			interfaces->includeHandler,
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

//Call HLSL reflection API to acquire functions + annotations

ESHPipelineStage Compiler_parseStage(CharString stageName) {

	Buffer buf = CharString_bufferConst(stageName);
	U64 stageNameLen = CharString_length(stageName);
	U32 c8x4 = Buffer_readU32(buf, 0, NULL, NULL);

	switch (c8x4) {

		default:
			break;

		case C8x4('v', 'e', 'r', 't'):        //vertex

			if(stageNameLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('e', 'x'))
				return ESHPipelineStage_Vertex;

			break;

		case C8x4('d', 'o', 'm', 'a'):        //domain

			if(stageNameLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('i', 'n'))
				return ESHPipelineStage_Domain;

			break;

		case C8x4('p', 'i', 'x', 'e'):        //pixel

			if(stageNameLen == 5 && stageName.ptr[4] == 'l')
				return ESHPipelineStage_Pixel;

			break;

		case C8x4('g', 'e', 'o', 'm'):        //geometry

			if(stageNameLen == 8 && Buffer_readU32(buf, 4, NULL, NULL) == C8x4('e', 't', 'r', 'y'))
				return ESHPipelineStage_GeometryExt;

			break;

		case C8x4('c', 'o', 'm', 'p'):        //compute

			if(stageNameLen == 7 && Buffer_readU32(buf, 3, NULL, NULL) == C8x4('p', 'u', 't', 'e'))
				return ESHPipelineStage_Compute;

			break;

		case C8x4('n', 'o', 'd', 'e'):        //node
			if(stageNameLen == 4)            return ESHPipelineStage_WorkgraphExt;
			break;

		case C8x4('m', 'e', 's', 'h'):        //mesh
			if(stageNameLen == 4)            return ESHPipelineStage_MeshExt;
			break;

		case C8x4('t', 'a', 's', 'k'):        //task
			if(stageNameLen == 4)            return ESHPipelineStage_TaskExt;
			break;

		case C8x4('h', 'u', 'l', 'l'):        //hull
			if(stageNameLen == 4)            return ESHPipelineStage_Hull;
			break;

		//Raytracing

		case C8x4('m', 'i', 's', 's'):        //miss
			if(stageNameLen == 4)            return ESHPipelineStage_MissExt;
			break;

		case C8x4('a', 'n', 'y', 'h'):        //anyhit

			if(stageNameLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('i', 't'))
				return ESHPipelineStage_AnyHitExt;

			break;

		case C8x4('c', 'l', 'o', 's'):        //closesthit

			if(stageNameLen == 10 && Buffer_readU64(buf, 2, NULL, NULL) == C8x8('o', 's', 'e', 's', 't', 'h', 'i', 't'))
				return ESHPipelineStage_ClosestHitExt;

			break;

		case C8x4('r', 'a', 'y', 'g'):        //raygeneration

			if(
				stageNameLen == 13 &&
				Buffer_readU64(buf, 4, NULL, NULL) == C8x8('e', 'n', 'e', 'r', 'a', 't', 'i', 'o') &&
				stageName.ptr[12] == 'n'
			)
				return ESHPipelineStage_RaygenExt;

			break;

		case C8x4('i', 'n', 't', 'e'):        //intersection

			if(stageNameLen == 12 && Buffer_readU64(buf, 4, NULL, NULL) == C8x8('r', 's', 'e', 'c', 't', 'i', 'o', 'n'))
				return ESHPipelineStage_IntersectionExt;

			break;
	}

	return ESHPipelineStage_Count;
}

ESHVendor Compiler_parseVendor(CharString vendor) {

	Buffer buf = CharString_bufferConst(vendor);
	U64 len = CharString_length(vendor);

	switch(len) {

		default:
			return ESHVendor_Count;

		case 2:        //NV
			return Buffer_readU16(buf, 0, NULL, NULL) == C8x2('N', 'V') ? ESHVendor_NV : ESHVendor_Count;

		case 3:        //AMD, ARM
			switch (Buffer_readU16(buf, 0, NULL, NULL)) {
				default:                return ESHVendor_Count;
				case C8x2('A', 'M'):    return vendor.ptr[2] == 'D' ? ESHVendor_AMD : ESHVendor_Count;
				case C8x2('A', 'R'):    return vendor.ptr[2] == 'M' ? ESHVendor_ARM : ESHVendor_Count;
			}

		case 4:        //QCOM, INTC, IMGT, MSFT, APPL, SMSG, HWEI
			switch (Buffer_readU32(buf, 0, NULL, NULL)) {
				default:                            return ESHVendor_Count;
				case C8x4('Q', 'C', 'O', 'M'):        return ESHVendor_QCOM;
				case C8x4('I', 'N', 'T', 'C'):        return ESHVendor_INTC;
				case C8x4('I', 'M', 'G', 'T'):        return ESHVendor_IMGT;
				case C8x4('M', 'S', 'F', 'T'):        return ESHVendor_MSFT;
				case C8x4('A', 'P', 'P', 'L'):        return ESHVendor_APPL;
				case C8x4('S', 'M', 'S', 'G'):        return ESHVendor_SMSG;
				case C8x4('H', 'W', 'E', 'I'):        return ESHVendor_HWEI;
			}
	}
}

ESHExtension Compiler_parseExtension(CharString extensionName) {

	Buffer buf = CharString_bufferConst(extensionName);
	U64 stageNameLen = CharString_length(extensionName);
	U16 c8x2 = Buffer_readU16(buf, 0, NULL, NULL);

	switch (c8x2) {

		case C8x2('F', '6'):    //F64
			if(stageNameLen == 3 && extensionName.ptr[2] == '4')    return ESHExtension_F64;
			break;

		case C8x2('I', '6'):    //I64
			if(stageNameLen == 3 && extensionName.ptr[2] == '4')    return ESHExtension_I64;
			break;

		case C8x2('P', 'A'):    //PAQ
			if(stageNameLen == 3 && extensionName.ptr[2] == 'Q')    return ESHExtension_PAQ;
			break;

		case C8x2('1', '6'):    //16BitTypes

			if(stageNameLen == 10 && Buffer_readU64(buf, 2, NULL, NULL) == C8x8('B', 'i', 't', 'T', 'y', 'p', 'e', 's'))
				return ESHExtension_16BitTypes;

			break;

		case C8x2('M', 'u'):    //Multiview

			if(stageNameLen == 9 && Buffer_readU64(buf, 1, NULL, NULL) == C8x8('u', 'l', 't', 'i', 'v', 'i', 'e', 'w'))
				return ESHExtension_Multiview;

			break;

		case C8x2('C', 'o'):    //ComputeDeriv

			if(
				stageNameLen == 12 &&
				Buffer_readU64(buf,  2, NULL, NULL) == C8x8('m', 'p', 'u', 't', 'e', 'D', 'e', 'r') &&
				Buffer_readU16(buf, 10, NULL, NULL) == C8x2('i', 'v')
			)
				return ESHExtension_ComputeDeriv;

			break;

		case C8x2('W', 'r'):

			if (
				stageNameLen == 14 &&
				Buffer_readU64(buf,  2, NULL, NULL) == C8x8('i', 't', 'e', 'M', 'S', 'T', 'e', 'x') &&
				Buffer_readU32(buf, 10, NULL, NULL) == C8x4('t', 'u', 'r', 'e')
			)
				return ESHExtension_WriteMSTexture;

			break;

		case C8x2('M', 'e'):

			if (
				stageNameLen == 16 &&
				Buffer_readU64(buf,  2, NULL, NULL) == C8x8('s', 'h', 'T', 'a', 's', 'k', 'T', 'e') &&
				Buffer_readU32(buf, 10, NULL, NULL) == C8x4('x', 'D', 'e', 'r') &&
				Buffer_readU16(buf, 14, NULL, NULL) == C8x2('i', 'v')
			)
				return ESHExtension_MeshTaskTexDeriv;

			break;

		case C8x2('A', 't'):    //AtomicI64, AtomicF32, AtomicF64

			if(stageNameLen == 9)
				switch (Buffer_readU64(buf, 1, NULL, NULL)) {
					case C8x8('t', 'o', 'm', 'i', 'c', 'I', '6', '4'):        return ESHExtension_AtomicI64;
					case C8x8('t', 'o', 'm', 'i', 'c', 'F', '3', '2'):        return ESHExtension_AtomicF32;
					case C8x8('t', 'o', 'm', 'i', 'c', 'F', '6', '4'):        return ESHExtension_AtomicF64;
				}

			break;

		case C8x2('S', 'u'):    //SubgroupArithmetic, SubgroupShuffle, SubgroupOperations

			if(stageNameLen == 15) {            //SubgroupShuffle
				if(
					Buffer_readU64(buf, 0, NULL, NULL) == C8x8('S', 'u', 'b', 'g', 'r', 'o', 'u', 'p') &&
					Buffer_readU64(buf, 7, NULL, NULL) == C8x8('p', 'S', 'h', 'u', 'f', 'f', 'l', 'e')
				)
					return ESHExtension_SubgroupShuffle;
			}

			else if (stageNameLen == 18) {        //SubgroupArithmetic, SubgroupOperations

				if(
					Buffer_readU64(buf,  2, NULL, NULL) == C8x8('b', 'g', 'r', 'o', 'u', 'p', 'A', 'r') &&
					Buffer_readU64(buf, 10, NULL, NULL) == C8x8('i', 't', 'h', 'm', 'e', 't', 'i', 'c')
				)
					return ESHExtension_SubgroupArithmetic;
					
				else if(
					Buffer_readU64(buf,  2, NULL, NULL) == C8x8('b', 'g', 'r', 'o', 'u', 'p', 'O', 'p') &&
					Buffer_readU64(buf, 10, NULL, NULL) == C8x8('e', 'r', 'a', 't', 'i', 'o', 'n', 's')
				)
					return ESHExtension_SubgroupOperations;

			}

			break;

		case C8x2('R', 'a'):    //RayQuery, RayMicromapOpacity, RayMotionBlur, RayReorder

			if(stageNameLen == 8 && Buffer_readU64(buf, 0, NULL, NULL) == C8x8('R', 'a', 'y', 'Q', 'u', 'e', 'r', 'y'))
				return ESHExtension_RayQuery;

			else if(stageNameLen >= 10)
				switch (Buffer_readU64(buf, 2, NULL, NULL)) {

					case C8x8('y', 'M', 'i', 'c', 'r', 'o', 'm', 'a'):        //RayMicromapOpacity

						if(
							stageNameLen == 18 && Buffer_readU64(buf, 10, NULL, NULL) == C8x8('p', 'O', 'p', 'a', 'c', 'i', 't', 'y')
						)
							return ESHExtension_RayMicromapOpacity;

						break;

					case C8x8('y', 'M', 'o', 't', 'i', 'o', 'n', 'B'):        //RayMotionBlur

						if(stageNameLen == 13 && Buffer_readU32(buf, 10, NULL, NULL) == C8x4('B', 'l', 'u', 'r'))
							return ESHExtension_RayMotionBlur;

						break;

					case C8x8('y', 'R', 'e', 'o', 'r', 'd', 'e', 'r'):        //RayReorder

						if(stageNameLen == 10)
							return ESHExtension_RayReorder;

						break;
				}

			break;
	}

	return (ESHExtension)(1u << ESHExtension_Count);
}

Bool Compiler_registerExtension(U32 *extensions, CharString extensionName, Error *e_rr) {

	Bool s_uccess = true;

	ESHExtension extension = Compiler_parseExtension(extensionName);

	if(extension >> ESHExtension_Count)
		retError(clean, Error_invalidParameter(
			0, 5, "Compiler_registerExtension() unrecognized extension in extension annotation"
		));

	if(*extensions & extension)
		retError(clean, Error_invalidParameter(
			0, 6, "Compiler_registerExtension() duplicate extension found"
		));

	*extensions |= extension;

clean:
	return s_uccess;
}

Bool Compiler_registerExtensions(ListU32 *extensionsRegistered, U32 extensions, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(ListU32_contains(*extensionsRegistered, extensions, 0, NULL))
		retError(clean, Error_alreadyDefined(0, "Compiler_registerExtensions() extensions already defined"));

	gotoIfError3(clean, ListU32_pushBack(extensionsRegistered, extensions, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_registerVendor(U16 *vendors, CharString vendorName, Error *e_rr) {

	Bool s_uccess = true;

	//Find vendor

	ESHVendor vendor = Compiler_parseVendor(vendorName);

	if(vendor == ESHVendor_Count)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerVendor() unrecognized vendor in vendor annotation"
		));

	if((*vendors >> vendor) & 1)
		retError(clean, Error_invalidParameter(
			0, 2, "Compiler_registerVendor() duplicate vendor found"
		));

	*vendors |= (U16)(1 << vendor);

clean:
	return s_uccess;
}

Bool Compiler_registerModel(ListU16 *vendors, const C8 *strStart, const C8 *strEnd, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	U16 version = 0;
	U8 maj = 0, mi = 0;

	//Find model version

	if (!C8_isDec(strStart[0]))
		retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected maj.min"));

	maj = C8_dec(strStart[0]);

	if (maj < 6)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model maj version is too low to be supported (must be >=6)"
		));

	++strStart;

	if (strStart >= strEnd || strStart[0] != '.')
		retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected maj.min"));

	++strStart;

	if (strStart >= strEnd || !C8_isDec(strStart[0]))
		retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected min (maj.min)"));

	mi = C8_dec(strStart[0]);
	++strStart;

	if (strStart < strEnd) {

		if(!C8_isDec(strStart[0]))
			retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected min (maj.min)"));

		mi = mi * 10 + C8_dec(strStart[0]);
		++strStart;

		if(strStart != strEnd)
			retError(clean, Error_invalidParameter(0, 1, "Compiler_registerModel() expected min (max 2 digits) (maj.min)"));
	}

	if(maj == 6 && mi < 5)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model maj.min version is too low to be supported (must be >=6.5)"
		));

	if (maj > 6 || mi > 10)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model version is too high, only supported up to 6.10"
		));

	version = OISH_SHADER_MODEL(maj, mi);

	if(ListU16_contains(*vendors, version, 0, NULL))
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_registerModel() model version was referenced multiple times"
		));

	gotoIfError3(clean, ListU16_pushBack(vendors, version, alloc, e_rr));

clean:
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

	if(ext & (ESHExtension_AtomicI64 | ESHExtension_ComputeDeriv | ESHExtension_PAQ))
		minVersion = U16_max(OISH_SHADER_MODEL(6, 6), minVersion);

	if(ext & ESHExtension_WriteMSTexture)
		minVersion = U16_max(OISH_SHADER_MODEL(6, 7), minVersion);

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

Bool Compiler_skipWhitespace(const C8 *&ptr) {

	while (C8_isWhitespace(*ptr))
		++ptr;

	return *ptr;
}

Bool Compiler_skipAlphaNumeric(const C8 *&ptr) {

	while (C8_isAlphaNumeric(*ptr))
		++ptr;

	return *ptr;
}

Bool Compiler_skipNumericDot(const C8 *&ptr) {

	while (C8_isDec(*ptr) || *ptr == '.')
		++ptr;

	return *ptr;
}

Bool Compiler_skipRndBracket(const C8 *&str, Bool isLeft, Error *e_rr) {

	Bool s_uccess = true;

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipRndBracket() reached end of token"));

	if (str[0] != (isLeft ? '(' : ')'))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipRndBracket() expected ()"));

	++str;

clean:
	return s_uccess;
}

Bool Compiler_skipDblQuot(const C8 *&str, Bool isLeft, Error *e_rr) {

	Bool s_uccess = true;

	if (isLeft && !Compiler_skipWhitespace(str))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipDblQuot() reached end of token"));

	if (str[0] != '"')
		retError(clean, Error_invalidParameter(0, 0, "Compiler_skipDblQuot() expected ()"));

	++str;

clean:
	return s_uccess;
}

Bool Compiler_consumeString(const C8 *&str, const C8 *&strStart, const C8 *&strEnd, Error *e_rr) {

	Bool s_uccess = true;

	// "test"
	//^^

	gotoIfError3(clean, Compiler_skipDblQuot(str, true, e_rr));

	// "test"
	//  ^^^^

	strEnd = strStart = str;

	//Still a loop like this because at some point we might add escaping anyways.

	while (true) {

		C8 c = *strEnd;
		++strEnd;

		if(!c)
			retError(clean, Error_invalidParameter(0, 0, "Compiler_consumeString() unexpected end of token"));

		//Because we reference a literal string when using defines/uniforms/etc.
		// Rather than a copied/processed string.

		if (c == '\\')
			retError(clean, Error_invalidParameter(0, 0, "Compiler_consumeString() escaping characters is unsupported"));

		// "test"
		// "test"
		//      ^

		if (c == '"') {
			--strEnd;
			break;
		}
	}

	str = strEnd + 1;

clean:
	return s_uccess;
}

Bool Compiler_consumeIdentifier(const C8 *&str, const C8 *&start, Error *e_rr) {

	Bool s_uccess = true;

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_consumeIdentifier() ended unexpectedly"));

	start = str;
	C8 c;

	while ((c = *str) != '\0' && !C8_isWhitespace(c) && !C8_isSymbol(c))
		++str;

clean:
	return s_uccess;
}

Bool Compiler_parseShaderStageAnnot(
	const C8 *&str, SHEntryRuntime &entry, CharString functionName, const Allocator *alloc, Error *e_rr
) {
	
	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;
	ESHPipelineStage stage = ESHPipelineStage_Count;

	Bool s_uccess = true;

	if (entry.entry.stage != ESHPipelineStage_Count)
		retError(clean, Error_invalidParameter(
			0, 0, "Compiler_parseShaderStageAnnot() shader already had shader or stage annotation"
		));
	
	//shader     ( "test" )
	//oxc::stage ( "test" )
	//          ^^^^^^^^^^^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));
	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));
	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));

	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseShaderStageAnnot() expected end of token"));

	//Parse stage

	stage = Compiler_parseStage(CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false));

	if (stage == ESHPipelineStage_Count)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_parseShaderStageAnnot() unrecognized stage in shader annotation"
		));

	gotoIfError3(clean, CharString_createCopy(functionName, alloc, &entry.entry.name, e_rr));

	entry.entry.stage = SHPipelineStage(stage);

	//isRt = RT stage; containsGfxOrComp = not RT and not workgraph (folded into runtimeFlags)

	entry.runtimeFlags &= (U8)~(ESHEntryRuntimeFlag_IsRt | ESHEntryRuntimeFlag_ContainsGfxOrComp);

	if (stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt)
		entry.runtimeFlags |= (U8)ESHEntryRuntimeFlag_IsRt;

	else if (stage != ESHPipelineStage_WorkgraphExt)
		entry.runtimeFlags |= (U8)ESHEntryRuntimeFlag_ContainsGfxOrComp;

clean:
	return s_uccess;
}

Bool Compiler_parseShaderAnnot(
	const D3D12_HLSL_ANNOTATION &annot,
	CharString functionName,
	const C8 *&str,
	U64 annotLen,
	SHEntryRuntime &entry,
	const Allocator *alloc,
	Error *e_rr
) {

	Buffer buf = Buffer_createRefConst(str, annotLen);
	U32 c8x4 = 0;

	Bool s_uccess = true;

	if (!annot.IsBuiltin)    //not [shader("")]
		goto clean;

	c8x4 = Buffer_readU32(buf, 0, NULL, NULL);

	//[shader("vertex")]
	// ^

	if (c8x4 != C8x4('s', 'h', 'a', 'd') || Buffer_readU16(buf, 4, NULL, NULL) != C8x2('e', 'r'))
		goto clean;

	str += annotLen;

	//[shader("vertex")]
	//       ^

	gotoIfError3(clean, Compiler_parseShaderStageAnnot(str, entry, functionName, alloc, e_rr));
	entry.isShaderAnnotation = true;

clean:
	return s_uccess;
}

Bool Compiler_parseStageAnnot(
	SHEntryRuntime &entry, CharString functionName, const C8 *&str, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	//[[oxc::stage("vertex")]]
	//              ^

	gotoIfError3(clean, Compiler_parseShaderStageAnnot(str, entry, functionName, alloc, e_rr));

	entry.isShaderAnnotation = false;
	entry.runtimeFlags |= ESHEntryRuntimeFlag_ContainsGfxOrComp;

clean:
	return s_uccess;
}

Bool Compiler_parseModelAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	const C8 *strStart = nullptr;
	const C8 *strEnd = nullptr;
	
	//oxc::model ( "6.5" )
	//          ^^^^^^^^^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));
	gotoIfError3(clean, Compiler_consumeString(str, strStart, strEnd, e_rr));
	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));
		
	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseModelAnnot() expected end of token"));

	gotoIfError3(clean, Compiler_registerModel(&entry.shaderVersions, strStart, strEnd, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_parseVendorAnnot(SHEntryRuntime &entry, const C8 *&str, Error *e_rr) {

	Bool s_uccess = true;

	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;

	//oxc::vendor ( "NV", "AMD" )
	//           ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::vendor ( "NV" , "AMD" )
	//             ^^^^^

	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

	gotoIfError3(clean, Compiler_registerVendor(
		&entry.vendorMask, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
	));

	while (true) {

		//oxc::vendor ( "NV" , "AMD" )
		//                  ^       ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseVendorAnnot() vendor annotation ended unexpectedly"));

		//oxc::vendor ( "NV" , "AMD" )
		//                           ^

		if (*str == ')')
			break;

		//oxc::vendor ( "NV" , "AMD" )
		//                   ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseVendorAnnot() vendor annotation expected ,"));

		++str;

		//oxc::vendor ( "NV" , "AMD" )
		//                    ^^^^^^

		gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

		gotoIfError3(clean, Compiler_registerVendor(
			&entry.vendorMask, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
		));
	}

	//oxc::vendor ( "NV" , "AMD" )
	//                           ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));

	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseVendorAnnot() expected end of token"));

clean:
	return s_uccess;
}

Bool Compiler_registerBinaryType(U8 *binaryTypes, CharString name, Error *e_rr) {

	Bool s_uccess = true;
	ESHBinaryType type = ESHBinaryType_Count;

	//"spv" / "spirv" -> SPIRV, "dxil" -> DXIL (case insensitive)

	if(CharString_equalsCStringInsensitive(&name, "spv") || CharString_equalsCStringInsensitive(&name, "spirv"))
		type = ESHBinaryType_SPIRV;

	else if(CharString_equalsCStringInsensitive(&name, "dxil"))
		type = ESHBinaryType_DXIL;

	else retError(clean, Error_invalidParameter(
		0, 1, "Compiler_registerBinaryType() unrecognized binary type (expected \"spv\"/\"spirv\" or \"dxil\")"
	));

	if((*binaryTypes >> type) & 1)
		retError(clean, Error_invalidParameter(
			0, 2, "Compiler_registerBinaryType() duplicate binary type found"
		));

	*binaryTypes |= (U8)(1 << type);

clean:
	return s_uccess;
}

Bool Compiler_parseBinaryAnnot(SHEntryRuntime &entry, const C8 *&str, Error *e_rr) {

	Bool s_uccess = true;

	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;
	U8 binaryTypes = 0;

	//oxc::binary ( "spv" , "dxil" )
	//           ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::binary ( "spv" , "dxil" )
	//             ^^^^^

	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

	gotoIfError3(clean, Compiler_registerBinaryType(
		&binaryTypes, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
	));

	while (true) {

		//oxc::binary ( "spv" , "dxil" )
		//                   ^        ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseBinaryAnnot() binary annotation ended unexpectedly"));

		//oxc::binary ( "spv" , "dxil" )
		//                             ^

		if (*str == ')')
			break;

		//oxc::binary ( "spv" , "dxil" )
		//                    ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseBinaryAnnot() binary annotation expected ,"));

		++str;

		//oxc::binary ( "spv" , "dxil" )
		//                      ^^^^^^

		gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

		gotoIfError3(clean, Compiler_registerBinaryType(
			&binaryTypes, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
		));
	}

	//oxc::binary ( "spv" , "dxil" )
	//                             ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));

	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseBinaryAnnot() expected end of token"));

	entry.binaryTypes = binaryTypes;

clean:
	return s_uccess;
}

Bool Compiler_parseExtensionAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const C8 *annotStart = NULL;
	const C8 *annotEnd = NULL;
	U32 extensions = 0;

	//oxc::extension ( "16BitTypes", "I64" )
	//              ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::extension ( "16BitTypes", "I64" )
	//oxc::extension ( )
	//                ^

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() extension annotation ended unexpectedly"));

	if (str[0] == ')') {

		++str;

		if (Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(
				0, "Compiler_parseExtensionAnnot() extension annotation had unknown syntax appended"
			));

		goto clean;
	}

	//oxc::extension ( "16BitTypes", "I64" )
	//                 ^^^^^^^^^^^^

	gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

	gotoIfError3(clean, Compiler_registerExtension(
		&extensions, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
	));

	while (true) {

		//oxc::extension ( "16BitTypes" , "I64" )
		//                             ^       ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() extension annotation ended unexpectedly"));

		//oxc::extension ( "16BitTypes" , "I64" )
		//                                      ^

		if (*str == ')')
			break;

		//oxc::extension ( "16BitTypes" , "I64" )
		//                              ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() extension annotation expected ,"));

		++str;

		//oxc::extension ( "16BitTypes" , "I64" )
		//                               ^^^^^^

		gotoIfError3(clean, Compiler_consumeString(str, annotStart, annotEnd, e_rr));

		gotoIfError3(clean, Compiler_registerExtension(
			&extensions, CharString_createRefSizedConst(annotStart, annotEnd - annotStart, false), e_rr
		));
	}

	//oxc::extension ( "16BitTypes" , "I64" )
	//                                      ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));
		
	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseExtensionAnnot() expected end of token"));

clean:
	if(s_uccess)
		gotoIfError3(clean1, Compiler_registerExtensions(
			&entry.extensions, extensions, alloc, e_rr
		));

clean1:
	return s_uccess;
}

Bool Compiler_parseDefine(SHEntryRuntime &entry, const C8 *&str, U8 &defineCount, const Allocator *alloc, Error *e_rr) {

	const C8 *strStart = NULL;
	const C8 *strEnd = NULL;

	Bool s_uccess = true;

	CharString defineName = CharString_createNull();
	CharString defineValue = CharString_createNull();

	//"X" = "12345"
	//"Y"
	//^

	gotoIfError3(clean, Compiler_consumeString(str, strStart, strEnd, e_rr));

	//"X" = "12345"
	//"Y"
	//    ^
	
	if(!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidParameter(0, 0, "Compiler_parseDefine() reached unexpected end of token"));

	defineName = CharString_createRefSizedConst(strStart, strEnd - strStart, false);

	for(U64 i = 0; i < CharString_length(defineName); ++i)
		if((C8_isSymbol(defineName.ptr[i]) && defineName.ptr[i] != '_') || C8_isWhitespace(defineName.ptr[i]))
			retError(clean, Error_alreadyDefined(0, "Compiler_parseDefine() can't contain symbols or whitespace"));
			
	//Scan last strings for the same define

	for(
		U64 i = (entry.defineNameValues.length >> 1) - 1;
		i != (entry.defineNameValues.length >> 1) - defineCount - 1;
		--i
	)
		if (CharString_equalsStringSensitive(&defineName, &entry.defineNameValues.ptr[i << 1]))
			retError(clean, Error_alreadyDefined(0, "Compiler_parseDefine() already contains define"));

	//Let caller handle invalid syntax or ,).
	//And push current define / with empty value to allow , to work.

	gotoIfError3(clean, ListCharString_pushBack(&entry.defineNameValues, defineName, alloc, e_rr));

	if (*str != '=') {
		gotoIfError3(clean, ListCharString_pushBack(&entry.defineNameValues, defineValue, alloc, e_rr));
		goto clean;
	}

	++str;

	//"X" = "12345"
	//     ^

	gotoIfError3(clean, Compiler_consumeString(str, strStart, strEnd, e_rr));
	defineValue = CharString_createRefSizedConst(strStart, strEnd - strStart, false);

	gotoIfError3(clean, ListCharString_pushBack(&entry.defineNameValues, defineValue, alloc, e_rr));

clean:
	return s_uccess;
}

Bool Compiler_parseDefinesAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	U8 defineCount = 0;

	//oxc::defines ( "X" = "Y" )
	//oxc::defines ( "X" )
	//oxc::defines ( )
	//            ^^

	gotoIfError3(clean, Compiler_skipRndBracket(str, true, e_rr));

	//oxc::defines ( "X" = "Y" )
	//oxc::defines ( "X" )
	//oxc::defines ( )
	//              ^

	if (!Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() defines annotation ended unexpectedly"));

	if (str[0] == ')') {

		++str;

		if (Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(
				0, "Compiler_parseDefinesAnnot() defines annotation had unknown syntax appended"
			));

		goto clean;
	}

	//oxc::defines ( "X" = "Y" )
	//oxc::defines ( "X" )
	//               ^

	gotoIfError3(clean, Compiler_parseDefine(entry, str, defineCount, alloc, e_rr));
	++defineCount;

	while (true) {

		//oxc::defines ( "X" = "Y" )
		//oxc::defines ( "X" = "Y" , "Z" )
		//oxc::defines ( "X" )
		//                  ^     ^     ^

		if (!Compiler_skipWhitespace(str))
			retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() defines annotation ended unexpectedly"));

		//oxc::defines ( "X" = "Y" )
		//oxc::defines ( "X" = "Y" , "Z" )
		//oxc::defines ( "X" )
		//                   ^     ^     ^

		if (*str == ')')
			break;

		//oxc::defines ( "X" = "Y" , "Z" )
		//                         ^

		if(*str != ',')
			retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() defines annotation expected ,"));

		++str;

		//oxc::defines ( "X" = "Y" , "Z" )
		//                          ^^^^

		gotoIfError3(clean, Compiler_parseDefine(entry, str, defineCount, alloc, e_rr));
		++defineCount;
	}

	//oxc::defines ( "X" = "Y" , "Z" )
	//                               ^

	gotoIfError3(clean, Compiler_skipRndBracket(str, false, e_rr));
		
	//Ensure nothing is leftover

	if (Compiler_skipWhitespace(str))
		retError(clean, Error_invalidState(0, "Compiler_parseDefinesAnnot() expected end of token"));

clean:

	if (s_uccess)
		gotoIfError3(clean, ListU8_pushBack(&entry.definesPerCompilation, defineCount, alloc, e_rr));

	goto clean1;

clean1:
	return s_uccess;
}

Bool Compiler_parseValue(
	SHValue *value,
	U64 &dstOff,
	ETypeId typeId,
	const C8 *&str,
	Error *e_rr
) {

	EDataType type = ETypeId_getDataType(typeId);
	EDataTypeStride typeStride = ETypeId_getDataTypeStride(typeId);
	U32 w = ETypeId_getWidth(typeId);
	U32 h = ETypeId_getHeight(typeId);
	ETypeId vecType = ETypeId_Undefined;
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

						if (!EFloatType_isFinite(EFloatType_F32, *(const U32*)&v))
							retError(clean, Error_invalidParameter(
								0, 0, "Compiler_parseValue() passed float value not representable as F32"
							));

						value->vf32[dstOff] = v;
						break;
					}

					default:

						if (!EFloatType_isFinite(EFloatType_F64, *(const U64*)&res))
							retError(clean, Error_invalidParameter(
								0, 0, "Compiler_parseValue() passed float value not representable as F64"
							));

						value->vf64[dstOff] = res;
						break;
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

		ETypeId singleType = ETypeId(makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, typeStride, type));

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
	
	vecType = ETypeId(makeTypeId(LIBRARYID_DEFAULT, 0, w, 1, typeStride, type));

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

	SHValue value = SHValue{ 0 };
	U64 dstOff = 0;
	U64 valLen = 0;
	U64 uniformDatLen = entry.uniformData.length;

	ETypeId typeId = ETypeId_Undefined;

	Bool didInit = entry.isInitializedFlags & 2;
	Bool contains = false;

	Bool s_uccess = true;

	//type name = value;
	//^

	gotoIfError3(clean, Compiler_consumeIdentifier(str, idenStart, e_rr));

	uniformType = CharString_createRefSizedConst(idenStart, str - idenStart, false);
	typeId = ETypeId_parse(uniformType);

	if (typeId == ETypeId_Undefined || typeId == ETypeId_C8)
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

		ETypeId val = ETypeId_arr[uniform.typeIdShort];

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

Bool Compiler_parseOxcAnnot(
	CharString functionName,
	const C8 *&str,
	U64 annotLen,
	SHEntryRuntime &entry,
	const Allocator *alloc,
	Error *e_rr
) {

	Buffer buf = Buffer_createRefConst(str, annotLen);
	Bool s_uccess = true;
	U32 c8x4 = 0;
	const C8 *annotEnd = str + annotLen;

	//oxc
	//^

	if (Buffer_readU16(buf, 0, NULL, NULL) != C8x2('o', 'x') || buf.ptr[2] != 'c')
		goto clean;

	//oxc::X
	//   ^

	str = annotEnd;

	if (!Compiler_skipWhitespace(str))
		goto clean;

	if (*str != ':' || str[1] != ':')
		goto clean;

	str += 2;

	if (!Compiler_skipWhitespace(str))
		goto clean;

	annotEnd = str;
	if (!Compiler_skipAlphaNumeric(annotEnd))
		goto clean;

	annotLen = annotEnd - str;

	if (annotLen < 5 || annotLen > 9)    //Skip unknown
		goto clean;

	buf = Buffer_createRefConst(str, annotLen);
	c8x4 = Buffer_readU32(buf, 0, NULL, NULL);

	switch (c8x4) {

	case C8x4('s', 't', 'a', 'g'):        //oxc::stage()

		//[[oxc::stage("vertex")]]
		//       ^
		if (annotLen == 5 && str[4] == 'e') {
			str += 5;
			gotoIfError3(clean, Compiler_parseStageAnnot(entry, functionName, str, alloc, e_rr));
		}

		goto clean;

	case C8x4('m', 'o', 'd', 'e'):        //oxc::model()

		//[[oxc::model("6.8")]]
		//       ^
		if (annotLen == 5 && str[4] == 'l') {
			str += 5;
			gotoIfError3(clean, Compiler_parseModelAnnot(entry, str, alloc, e_rr));
		}

		break;

	case C8x4('v', 'e', 'n', 'd'):        //oxc::vendor()

		//[[oxc::vendor("NV", "AMD")]]
		//       ^
		if (annotLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('o', 'r')) {
			str += 6;
			gotoIfError3(clean, Compiler_parseVendorAnnot(entry, str, e_rr));
		}

		break;

	case C8x4('b', 'i', 'n', 'a'):        //oxc::binary()

		//[[oxc::binary("spv", "dxil")]]
		//       ^
		if (annotLen == 6 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('r', 'y')) {
			str += 6;
			gotoIfError3(clean, Compiler_parseBinaryAnnot(entry, str, e_rr));
		}

		break;

	case C8x4('d', 'e', 'f', 'i'):        //oxc::defines()

		//[[oxc::defines("X", "Y", "Z")]]
		//[[oxc::defines("X" = "123", "Y" = "ABC")]]
		//         ^
		if (annotLen == 7 && Buffer_readU16(buf, 4, NULL, NULL) == C8x2('n', 'e') && buf.ptr[6] == 's') {
			str += 7;
			gotoIfError3(clean, Compiler_parseDefinesAnnot(entry, str, alloc, e_rr));
		}

		break;

	case C8x4('u', 'n', 'i', 'f'):        //oxc::uniforms()

		//[[oxc::uniforms(U8x4 x = (1, 2, 3, 4))]]
		//[[oxc::uniforms(B1 b = true)]]
		//         ^
		if (annotLen == 8 && Buffer_readU32(buf, 4, NULL, NULL) == C8x4('o', 'r', 'm', 's')) {
			str += 8;
			gotoIfError3(clean, Compiler_parseUniformsAnnot(entry, str, alloc, e_rr));
		}

		break;

	case C8x4('e', 'x', 't', 'e'):        //oxc::extension()

		//[[oxc::extension()]]
		//       ^
		if (
			annotLen == 9 &&
			Buffer_readU64(buf, 1, NULL, NULL) == C8x8('x', 't', 'e', 'n', 's', 'i', 'o', 'n')
		) {
			str += 9;
			gotoIfError3(clean, Compiler_parseExtensionAnnot(entry, str, alloc, e_rr));
		}

		break;
	}

clean:
	return s_uccess;
}

Bool Compiler_parseAnnot(
	const D3D12_HLSL_ANNOTATION &annot,
	CharString funcName,
	SHEntryRuntime &entry,
	const Allocator *alloc,
	Error *e_rr
) {

	//Skip invalid annotations and find length

	const C8 *str = annot.Name;
	const C8 *annotEnd = NULL;
	U64 annotLen = 0;

	Bool s_uccess = true;

	if (!Compiler_skipWhitespace(str))
		goto clean;

	annotEnd = str;
	if (!Compiler_skipAlphaNumeric(annotEnd))
		goto clean;

	annotLen = annotEnd - str;

	if (annotLen != 3 && annotLen != 6)    //Ignore non oxc and shader annots
		goto clean;

	//[shader()]
	// ^

	if (annotLen == 6) {
		gotoIfError3(clean, Compiler_parseShaderAnnot(annot, funcName, str, annotLen, entry, alloc, e_rr));
		goto clean;
	}

	if (annot.IsBuiltin)    //not [[oxc::]]
		goto clean;

	//[[oxc::defines()]]
	//[[oxc::extension()]]
	//[[oxc::vendor()]]
	//[[oxc::model()]]
	//[[oxc::stage()]]
	//[[oxc::uniforms()]]
	//    ^

	gotoIfError3(clean, Compiler_parseOxcAnnot(funcName, str, annotLen, entry, alloc, e_rr));

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
		gotoIfError3(clean, CharString_toUTF32(settings->path, alloc, &tmpWStr));
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

	Compiler_convertToWString(stringsUTF8, clean);

	interfaces->includeHandler->reset();        //Ensure we don't reuse stale caches

	hr = interfaces->reflector->FromSource(
		source,
		(const wchar_t*)tmpWStr.ptr,
		(LPCWSTR*) strings.ptr, U32(strings.length),
		nullptr, 0,
		interfaces->includeHandler,
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
