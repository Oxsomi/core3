/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "shader_compiler/compiler.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/file.h"
#include "types/math.h"

#include <wsl/winadapter.h>		//Avoid including Windows.h
#include <dxcapi.h>
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

		Error err = Error_none();
		HRESULT hr = S_OK;
		Buffer tempBuffer = Buffer_createNull();
		CharString tempFile = CharString_createNull();
		FileInfo fileInfo = FileInfo{};
		Bool isVirtual = false;
		Bool isBuiltin = false;
		U64 i = 0;
		U64 firstDoubleSlash = U64_MAX;

		gotoIfError(clean, CharString_createFromUTF16((const U16*)fileNameStr, U64_MAX, alloc, &fileName))

		//Little hack to handle builtin shaders, by using "virtual files" //myTest.hlsl

		firstDoubleSlash = CharString_findFirstStringSensitive(fileName, CharString_createRefCStrConst("//"), 0);
		isBuiltin = firstDoubleSlash != U64_MAX;

		if (isBuiltin) {

			//Find first non double slash
			while(CharString_getAt(fileName, firstDoubleSlash) == '/')
				++firstDoubleSlash;

			CharString tmp = CharString_createNull();

			if(!CharString_cut(fileName, firstDoubleSlash, 0, &tmp) || !CharString_length(tmp))
				gotoIfError(clean, Error_invalidState(0, "IncludeHandler::LoadSource expected source after //"))

			gotoIfError(clean, CharString_createCopy(tmp, alloc, &resolved))
		}

		else gotoIfError(clean, File_resolve(
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

				gotoIfError(clean, File_getInfo(resolved, &fileInfo, alloc))

				IncludeInfo prevInclude = includedFiles.ptr[i].includeInfo;

				//When timestamp has changed, it might be dirty.
				//When fileSize has, it definitely is dirty.

				if (fileInfo.timestamp != prevInclude.timestamp || fileInfo.fileSize != prevInclude.fileSize) {

					//Unfortunately peanut butter, we have to hash the whole file.
					//Luckily, CRC32C is quite fast.

					if (fileInfo.fileSize == prevInclude.fileSize) {

						gotoIfError(clean, File_read(resolved, 1 * SECOND, &tempBuffer))

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
				gotoIfError(clean, ListIncludedFile_popLocation(&includedFiles, i, &includedFile))

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

				gotoIfError(clean, File_getInfo(resolved, &fileInfo, alloc))
				gotoIfError(clean, File_read(resolved, 1 * SECOND, &tempBuffer))

				crc32c = Buffer_crc32c(tempBuffer);
				Ns timestamp = fileInfo.timestamp;
				FileInfo_free(&fileInfo, alloc);

				if(Buffer_length(tempBuffer) >> 32)
					gotoIfError(clean, Error_outOfBounds(
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

				gotoIfError(clean, CharString_createCopy(
					CharString_createRefSizedConst((const C8*)tempBuffer.ptr, Buffer_length(tempBuffer), false),
					alloc,
					&tempFile
				))

				Buffer_free(&tempBuffer, alloc);

				inc.includeInfo.file = resolved;
				inc.data = tempFile;

				gotoIfError(clean, ListIncludedFile_pushBack(&includedFiles, inc, alloc))
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
					gotoIfError(clean, CharString_createCopy(tmp, alloc, &tempFile))

					tmp = CharString_createRefCStrConst(nvHLSLExtns2);
					gotoIfError(clean, CharString_appendString(&tempFile, tmp, alloc))
				}

				else gotoIfError(clean, Error_notFound(0, 0, "IncludeHandler::LoadSource builtin file not found"))

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

				gotoIfError(clean, ListIncludedFile_pushBack(&includedFiles, inc, alloc))
				resolved = CharString_createNull();
				tempFile = CharString_createNull();
			}

			if((i >> 6) >= isPresent.length)
				gotoIfError(clean, ListU64_pushBack(&isPresent, 0, alloc))

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

		if(SUCCEEDED(hr) && err.genericError)
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

Error Compiler_create(Allocator alloc, Compiler *comp) {

	if(!comp)
		return Error_nullPointer(1, "Compiler_create()::comp is required");

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;

	if(interfaces->utils)
		return Error_alreadyDefined(1, "Compiler_create()::comp must be empty");

	Error err = Error_none();

	try {

		//TODO: Call DxcCreateInstance2 with custom IMalloc.
		//Problem; we don't know the size of any allocation.
		//Would require each allocation to lock and push in ListDebugAllocation, even in Release mode!

		//IMalloc *malloc = ...;

		//Create utils

		HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&interfaces->utils));

		if(FAILED(hr))
			gotoIfError(clean, Error_invalidState(0, "Compiler_create() IDxcUtils couldn't be created. Missing DLL?"))

		//Create include handler

		interfaces->includeHandler = new IncludeHandler(interfaces->utils, alloc);

		//Create compiler

		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&interfaces->compiler));

		if(FAILED(hr))
			gotoIfError(clean, Error_invalidState(2, "Compiler_create() IDxcCompiler3 couldn't be created"))

	} catch (std::exception) {
		gotoIfError(clean, Error_invalidState(1, "Compiler_create() raised an internal exception"))
	}

clean:

	if(err.genericError)
		Compiler_free(comp, alloc);
	
	return err;
}

Bool Compiler_free(Compiler *comp, Allocator alloc) {

	(void)alloc;

	if(!comp)
		return true;

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;

	if(interfaces->utils)
		interfaces->utils->Release();

	if(interfaces->compiler)
		interfaces->compiler->Release();

	if(interfaces->includeHandler)
		delete interfaces->includeHandler;

	*comp = Compiler{};
	return true;
}

Error Compiler_mergeIncludeInfo(Compiler *comp, Allocator alloc, ListIncludeInfo *infos) {

	if(!comp || !infos)
		return Error_nullPointer(!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp and infos are required");

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp->interfaces;

	if(!interfaces->includeHandler)
		return Error_nullPointer(!comp ? 0 : 2, "Compiler_mergeIncludeInfo()::comp->interfaces includeHandler is missing");

	ListIncludedFile files = interfaces->includeHandler->getIncludedFiles();
	CharString tmp = CharString_createNull();
	Error err = Error_none();

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

			gotoIfError(clean, CharString_createCopy(info.includeInfo.file, alloc, &tmp))

			info.includeInfo.counter = info.globalCounter;
			info.includeInfo.file = tmp;
			gotoIfError(clean, ListIncludeInfo_pushBack(infos, info.includeInfo, alloc))
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
	return err;
}

Bool Compiler_filterWarning(CharString str) {
	return CharString_startsWithStringSensitive(str, CharString_createRefCStrConst("#pragma once in main file\n"), 0);
}

Error Compiler_preprocess(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result) {

	CompilerInterfaces *interfaces = (CompilerInterfaces*) comp.interfaces;

	if(!interfaces->utils || !result)
		return Error_alreadyDefined(!interfaces->utils ? 0 : 2, "Compiler_preprocess()::comp is required");

	if(!CharString_length(settings.string))
		return Error_invalidParameter(1, 0, "Compiler_preprocess()::settings.string is required");

	if(settings.outputType >= ESHBinaryType_Count || settings.format >= ECompilerFormat_Count)
		return Error_invalidParameter(1, 0, "Compiler_preprocess()::settings contains invalid format or outputType");

	Error err = Error_none();
	IDxcResult *dxcResult = NULL;
	IDxcBlobUtf8 *error = NULL;
	ListU16 inputFile = ListU16{};
	ListU16 includeDir = ListU16{};
	Bool hasErrors = false, isVirtual = false;
	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();

	gotoIfError(clean, CharString_toUTF16(settings.path, alloc, &inputFile))

	if(settings.includeDir.ptr) {
		gotoIfError(clean, File_resolve(settings.includeDir, &isVirtual, 256, Platform_instance.workingDirectory, alloc, &tempStr2))
		gotoIfError(clean, CharString_toUTF16(tempStr2, alloc, &includeDir))
		CharString_free(&tempStr2, alloc);
	}

	try {

		interfaces->includeHandler->reset();

		result->isSuccess = false;

		const U16 args[][7] = {		//Preprocess
			{ '-', 'P', '\0' },
			{ '-', 's', 'p', 'i', 'r', 'v', '\0' },
			{ '-', 'I', '\0' }
		};

		U32 argCounter = 2;

		const U16 *argsPtr[5] = {
			args[0],
			inputFile.ptr
		};

		if (settings.outputType == ESHBinaryType_SPIRV)		//-spirv
			argsPtr[argCounter++] = args[1];

		if (includeDir.length) {							//-I
			argsPtr[argCounter++] = args[2];
			argsPtr[argCounter++] = includeDir.ptr;
		}

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
			gotoIfError(clean, Error_invalidState(0, "Compiler_preprocess() \"Compile\" failed"))

		hr = dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), NULL);

		if(FAILED(hr))
			gotoIfError(clean, Error_invalidState(1, "Compiler_preprocess() fetch errors failed"))

		if(error && error->GetStringLength()) {

			CharString errs = CharString_createRefSizedConst(error->GetStringPointer(), error->GetStringLength(), false);
			U64 off = 0;

			CharString warning = CharString_createRefCStrConst(" warning: ");
			CharString errStr = CharString_createRefCStrConst(" error: ");
			CharString fatalErrStr = CharString_createRefCStrConst(" fatal error: ");

			U64 prevOff = U64_MAX;

			CharString file = CharString_createNull();
			U64 lineId = 0;
			U64 lineOff = 0;
			Bool isError = false;

			while(off < CharString_length(errs)) {

				//Find start of next error

				U64 firstColon = CharString_findFirstSensitive(errs, ':', off);
				U64 errorEnd = firstColon;

				//On Windows, D:/test.hlsl:10:10: warning: x is valid ofc, so we should skip that

				if(!off || C8_isNewLine(CharString_getAt(errs, firstColon - 2)))
					errorEnd = firstColon = CharString_findFirstSensitive(errs, ':', firstColon + 1);

				if(firstColon == U64_MAX)
					break;

				//Sometimes our line starts with "In file included from ", which makes me angry

				{
					U64 i = firstColon;

					for(; i != U64_MAX && !C8_isNewLine(CharString_getAt(errs, i)); --i)
						;

					++i;

					CharString lameString = CharString_createRefCStrConst("In file included from ");

					if (CharString_startsWithStringSensitive(errs, lameString, i)) {

						//Find error start

						while(i != U64_MAX && C8_isNewLine(CharString_getAt(errs, i--)));

						++i;

						if(!off)
							errorEnd = i;

						Bool hasNewLine = false;

						while (true) {

							C8 next = CharString_getAt(errs, ++firstColon);

							if(next == C8_MAX)
								break;

							//Find first non newline once we have encountered the first newline

							Bool isNewLine = C8_isNewLine(next);

							if(hasNewLine && !isNewLine)
								break;

							hasNewLine = isNewLine;
						}

						//Reset colon to be correct

						off = firstColon;
						continue;
					}
				}

				//We ended with no more errors

				if(firstColon >= CharString_length(errs))
					break;

				//In case we are still parsing an error, we have to traverse our : to the first whitespace character.
				//As the start of the file name is the end of our error.
				//We can then finalize the error and push it.

				if (prevOff != U64_MAX) {

					U64 i = errorEnd;

					for(; i != U64_MAX; --i)
						if(C8_isWhitespace(errs.ptr[i]))
							break;

					if(i == U64_MAX)
						gotoIfError(clean, Error_invalidState(1, "Compiler_preprocess() couldn't parse error"))

					CharString errorStr = CharString_createRefSizedConst(errs.ptr + prevOff, i - prevOff, false);

					if(!Compiler_filterWarning(errorStr)) {

						gotoIfError(clean, CharString_createCopy(errorStr, alloc, &tempStr))
						gotoIfError(clean, CharString_createCopy(file, alloc, &tempStr2))

						CompileError cerr = CompileError{
							.lineId = (U16) lineId,
							.typeLineId = (U8) ((lineId >> 16) | ((U8)isError << 7)),
							.lineOffset = (U8) lineOff,
							.error = tempStr,
							.file = tempStr2
						};

						gotoIfError(clean, ListCompileError_pushBack(&result->compileErrors, cerr, alloc))

						tempStr = CharString_createNull();
						tempStr2 = CharString_createNull();
					}

					file = CharString_createNull();
				}

				//Parse file
			
				if(firstColon == U64_MAX || !CharString_cut(errs, off, firstColon - off, &file))
					gotoIfError(clean, Error_invalidState(1, "Compiler_preprocess() couldn't parse file from error"))

				//Parse line

				U64 nextColon = CharString_findFirstSensitive(errs, ':', firstColon + 1);

				CharString tmp = CharString_createNull();

				if(nextColon == U64_MAX || !CharString_cut(errs, firstColon + 1, nextColon - (firstColon + 1), &tmp))
					gotoIfError(clean, Error_invalidState(2, "Compiler_preprocess() couldn't parse lineId from error"))
			
				if(!CharString_parseDec(tmp, &lineId) || (lineId >> (16 + 7)))
					gotoIfError(clean, Error_invalidState(3, "Compiler_preprocess() couldn't parse U23 lineId from error"))
					
				//Parse line offset

				tmp = CharString_createNull();
				off = nextColon + 1;
				nextColon = CharString_findFirstSensitive(errs, ':', off);

				if(nextColon == U64_MAX || !CharString_cut(errs, off, nextColon - off, &tmp))
					gotoIfError(clean, Error_invalidState(2, "Compiler_preprocess() couldn't parse lineOff from error"))
			
				if(!CharString_parseDec(tmp, &lineOff) || lineOff >> 8)
					gotoIfError(clean, Error_invalidState(3, "Compiler_preprocess() couldn't parse U8 lineOff from error"))

				off = nextColon + 1;

				//Parse warning type

				Bool isFatalError = CharString_startsWithStringInsensitive(errs, fatalErrStr, off);

				if(CharString_startsWithStringInsensitive(errs, errStr, off) || isFatalError) {
					isError = true;
					hasErrors = true;
					off += isFatalError ? CharString_length(fatalErrStr) : CharString_length(errStr);
				}

				else if(CharString_startsWithStringInsensitive(errs, warning, off)) {
					isError = false;
					off += CharString_length(warning);
				}

				else gotoIfError(clean, Error_invalidState(4, "Compiler_preprocess() couldn't parse error type from error")) 

				prevOff = off;
			}

			if (prevOff != U64_MAX) {

				CharString errorStr = CharString_createRefSizedConst(
					errs.ptr + prevOff, CharString_length(errs) - prevOff, false
				);

				if(!Compiler_filterWarning(errorStr)) {
			
					gotoIfError(clean, CharString_createCopy(errorStr, alloc, &tempStr))
					gotoIfError(clean, CharString_createCopy(file, alloc, &tempStr2))

					CompileError cerr = CompileError{
						.lineId = (U16) lineId,
						.typeLineId = (U8) ((lineId >> 16) | ((U8)isError << 7)),
						.lineOffset = (U8) lineOff,
						.error = tempStr,
						.file = tempStr2
					};

					gotoIfError(clean, ListCompileError_pushBack(&result->compileErrors, cerr, alloc))

					tempStr = CharString_createNull();
					tempStr2 = CharString_createNull();
				}

				file = CharString_createNull();
			}
		}

		if(error) {
			error->Release();
			error = NULL;
		}

		if (settings.infoAboutIncludes) {

			gotoIfError(clean, ListIncludeInfo_resize(&result->includeInfo, interfaces->includeHandler->getCounter(), alloc))

			ListIncludedFile files = interfaces->includeHandler->getIncludedFiles();

			for(U64 i = 0, j = 0; i < files.length; ++i)
				if (files.ptr[i].includeInfo.counter) {		//Exclude inactive includes

					IncludeInfo copy = files.ptr[i].includeInfo;					
					gotoIfError(clean, CharString_createCopy(copy.file, alloc, &tempStr))

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
			gotoIfError(clean, Error_invalidState(2, "Compiler_preprocess() fetch hlsl failed"))

		gotoIfError(clean, CharString_createCopy(
			CharString_createRefSizedConst(error->GetStringPointer(), error->GetBufferSize(), false),
			alloc,
			&result->text
		))

		result->type = ECompileResultType_Text;
		result->isSuccess = true;

	} catch (std::exception) {
		gotoIfError(clean, Error_invalidState(1, "Compiler_preprocess() raised an internal exception"))
	}

clean:

	if(err.genericError)
		CompileResult_free(result, alloc);

	if(dxcResult)
		dxcResult->Release();

	if(error)
		error->Release();

	ListU16_free(&inputFile, alloc);
	ListU16_free(&includeDir, alloc);
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	return err;
}

//TODO: Real compile, check for [extension(I16, F16)] one of the two indicates we need to enable 16-bit types
