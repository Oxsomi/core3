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
#include "compiler_private.hpp"

const C8 *resources =
	#include "shader_compiler/shaders/resources.hlsli"
	;

const C8 *types =
	#include "shader_compiler/shaders/types.hlsli"
	;

//Split out of types.hlsli / resources.hlsli; the umbrella headers include them, so a shader that only
//says #include "@types.hlsli" still sees everything it used to.

const C8 *matHlsli =
	#include "shader_compiler/shaders/mat.hlsli"
	;

const C8 *indirectHlsli =
	#include "shader_compiler/shaders/indirect.hlsli"
	;

const C8 *fixedPointHlsli =
	#include "shader_compiler/shaders/fixed_point.hlsli"
	;

const C8 *packHlsli =
	#include "shader_compiler/shaders/pack.hlsli"
	;

const C8 *bufferHlsli =
	#include "shader_compiler/shaders/buffer.hlsli"
	;

const C8 *appDataHlsli =
	#include "shader_compiler/shaders/appdata.hlsli"
	;

const C8 *extensionsHlsli =
	#include "shader_compiler/shaders/extensions.hlsli"
	;

const C8 *extensionRayReorderHlsl =
	#include "shader_compiler/shaders/extension.RayReorder.hlsli"
	;

const C8 *extensionRayTriPositionHlsl =
	#include "shader_compiler/shaders/extension.RayTriPosition.hlsli"
	;

const C8 *extensionRayMicromapOpacityHlsl =
	#include "shader_compiler/shaders/extension.RayMicromapOpacity.hlsli"
	;

const C8 *extensionAtomicF32Hlsl =
	#include "shader_compiler/shaders/extension.AtomicF32.hlsli"
	;

const C8 *extensionAtomicF64Hlsl =
	#include "shader_compiler/shaders/extension.AtomicF64.hlsli"
	;

const C8 *extensionCoopVecHlsl =
	#include "shader_compiler/shaders/extension.CoopVec.hlsli"
	;

const C8 *extensionCoopMatHlsl =
	#include "shader_compiler/shaders/extension.CoopMat.hlsli"
	;

static const CompilerBuiltInInclude CompilerBuiltInIncludes[] = {

	{ "resources.hlsli",                    resources                    },
	{ "types.hlsli",                        types                        },
	{ "extensions.hlsli",                   extensionsHlsli              },

	//Split out of types.hlsli / resources.hlsli

	{ "mat.hlsli",                          matHlsli                     },
	{ "indirect.hlsli",                     indirectHlsli                },
	{ "fixed_point.hlsli",                  fixedPointHlsli              },
	{ "pack.hlsli",                         packHlsli                    },
	{ "buffer.hlsli",                       bufferHlsli                  },
	{ "appdata.hlsli",                      appDataHlsli                 },

	//Opt in per extension, so a shader only pays for what it asks for

	{ "extension.RayReorder.hlsli",         extensionRayReorderHlsl      },
	{ "extension.RayTriPosition.hlsli",     extensionRayTriPositionHlsl  },
	{ "extension.RayMicromapOpacity.hlsli", extensionRayMicromapOpacityHlsl },
	{ "extension.AtomicF32.hlsli",          extensionAtomicF32Hlsl       },
	{ "extension.AtomicF64.hlsli",          extensionAtomicF64Hlsl       },
	{ "extension.CoopVec.hlsli",            extensionCoopVecHlsl         },
	{ "extension.CoopMat.hlsli",            extensionCoopMatHlsl         }
};

extern "C" {

	#ifdef SHADER_COMPILER_DYNAMIC

		//See compiler.h: a shared build has its own Platform_instance, NULL until the host hands its own across.
		//Mirrors what GraphicsInterface_getTable does for a graphics backend.

		void Compiler_setPlatform(Platform *instance) {
			Platform_instance = instance;
		}

	#endif

	U64 Compiler_builtInIncludeCount() {
		return sizeof(CompilerBuiltInIncludes) / sizeof(CompilerBuiltInIncludes[0]);
	}

	const CompilerBuiltInInclude *Compiler_builtInIncludeAt(U64 i) {
		return i < Compiler_builtInIncludeCount() ? &CompilerBuiltInIncludes[i] : NULL;
	}

	const CompilerBuiltInInclude *Compiler_findBuiltInInclude(CharString name) {

		//The @ is how a shader spells "built-in", but it isn't part of the name

		if(CharString_getAt(name, 0) == '@')
			name = CharString_createRefSizedConst(name.ptr + 1, CharString_length(name) - 1, false);

		for(U64 i = 0; i < Compiler_builtInIncludeCount(); ++i) {

			const CharString entry = CharString_createRefCStrConst(CompilerBuiltInIncludes[i].name);

			if(CharString_equalsStringInsensitive(&name, &entry))
				return &CompilerBuiltInIncludes[i];
		}

		return NULL;
	}
}

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

	inline IncludeHandler(IDxcUtils *utils, const Allocator *alloc): utils(utils), alloc(alloc) {}

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

		//DXC's own builtin headers (dx/linalg.h and the <enable_if> / <type_traits> it pulls in) are served by
		//DXC's compiled-in header fallback, which only triggers for angled includes it can't otherwise resolve.
		//We let those fall through as not-found here so that fallback serves them (see linalg.hlsl).

		//Little hack to handle builtin shaders, by using "virtual files" //myTest.hlsli

		//Braced so the earlier gotoIfError3(clean, ...) doesn't jump across atStr's initializer (GCC C++).
		{
			const CharString atStr = CharString_createRefCStrConst("@");
			lastAt = CharString_findLastStringSensitive(&fileName, &atStr, 0, 0);
		}
		isBuiltin = lastAt != U64_MAX;

		if (isBuiltin) {

			CharString tmp = CharString_createNull();

			if(!CharString_cut(&fileName, lastAt, 0, &tmp) || !CharString_length(tmp))
				retError(clean, Error_invalidState(0, "IncludeHandler::LoadSource expected source after @"));

			gotoIfError3(clean, CharString_createCopy(tmp, alloc, &resolved, e_rr));
		}

		else {

			gotoIfError3(clean, File_resolve(
				&fileName, &isVirtual, 256, &Platform_instance->defaultDir, alloc, &resolved, e_rr
			));

			//File_resolve strips the // marker virtual paths carry, but everything below (the dedup
			//compare, File_getInfo, File_read) routes virtual vs physical on exactly that prefix.
			//Restore it so an include inside a virtual shader resolves back into the virtual file system.

			if(isVirtual) {
				const CharString virtualPrefix = CharString_createRefCStrConst("//");
				gotoIfError3(clean, CharString_insertString(&resolved, &virtualPrefix, 0, alloc, e_rr));
			}

			//DXC normalizes paths while building include candidates, which collapses the leading //
			//marker ("//a/b.hlsli" arrives here as "a/b.hlsli"), so an include living in the virtual
			//file system can show up disguised as a physical path that doesn't exist.
			//Only when the physical interpretation is absent and re-marking the raw name hits a loaded
			//virtual file is the virtual reading taken, so a real physical include always wins.

			else if (!File_has(&resolved, alloc)) {

				CharString virtualized = CharString_createNull();

				gotoIfError3(clean, CharString_format(
					alloc, &virtualized, e_rr, "//%.*s", (int) CharString_length(fileName), fileName.ptr
				));

				if (File_has(&virtualized, alloc)) {
					CharString_free(&resolved, alloc);
					resolved = virtualized;
					isVirtual = true;
				}

				else CharString_free(&virtualized, alloc);
			}
		}

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

				const CompilerBuiltInInclude *builtIn = Compiler_findBuiltInInclude(resolved);

				if(!builtIn)
					retError(clean, Error_notFound(0, 0, "IncludeHandler::LoadSource builtin file not found"));

				tmpTmp = CharString_createRefCStrConst(builtIn->source);

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

		//Note: DxcCreateInstance2 with a custom IMalloc was tried (size-prefixed blocks so Free can recover {ptr, len}).
		//It instantly hard-crashes inside DxcReflector::FromSource (unordered_map in dxcreflection_from_ast).
		//DXC routes global new/delete through the thread-local IMalloc, but some allocations cross allocator
		// boundaries (allocated under the default CRT allocator, freed under the custom one or vice versa),
		// so any pointer-rewriting IMalloc corrupts the heap.
		//Until the DXC fork guarantees symmetric alloc/free through a single IMalloc this can't be tracked.

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

//Helpers so other TUs can use the include handler without seeing the class definition.

void Compiler_resetIncludeHandler(IncludeHandler *includeHandler) {
	includeHandler->reset();
}

IDxcIncludeHandler *Compiler_getIncludeHandler(IncludeHandler *includeHandler) {
	return includeHandler;
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
