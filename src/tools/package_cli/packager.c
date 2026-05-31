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

#include "tools/package_cli/packager.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"
#include "types/container/list_basic_types.h"
#include "types/container/file.h"
#include "types/container/buffer.h"
#include "types/container/encryption_stream.h"
#include "types/container/log.h"
#include "types/base/string_read_helper.h"
#include "types/base/time.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

typedef struct CAFileRecursion {
	CAFile *archive;
	CharString root;
	const RefPtrType *fileHandleType;
} CAFileRecursion;

Bool packageFile(const FileInfo *file, CAFileRecursion *pkgFile, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();
	Buffer data = Buffer_createNull();

	if(!CharString_cut(&file->path, CharString_length(pkgFile->root), 0, &subPath))
		retError(clean, Error_invalidState(0, "packageFile()::file.path cut failed"));

	CharString parentPath = CharString_createNull();
	CharString_cutAfterLastSensitive(&subPath, '/', &parentPath);

	CAHandle parent = CAHandle_Root;

	if (CharString_length(parentPath)) {

		parent = CAFile_resolve(pkgFile->archive, parentPath);

		if(parent == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "packageFile()::file.path parent lookup failed"));
	}

	CharString tmp = CharString_createNull();
	CharString_cutBeforeLastSensitive(&subPath, '/', &tmp);

	if(!tmp.ptr)
		tmp = subPath;

	subPath = CharString_createNull();
	gotoIfError3(clean, CharString_createCopy(tmp, alloc, &subPath, e_rr));

	if (file->type == EFileType_File) {

		gotoIfError3(clean, File_read(&file->path, 100 * MS, 0, 0, pkgFile->fileHandleType, &data, e_rr));

		//If shader compilation is used, skip hlsl files, they'll be turned into oiSH files later.

		#ifdef CLI_SHADER_COMPILER

			const CharString hlsl = CharString_createRefCStrConst(".hlsl");
			const CharString hlsli = CharString_createRefCStrConst(".hlsli");

			if (
				CharString_endsWithStringSensitive(&file->path, &hlsl, 0) ||
				CharString_endsWithStringSensitive(&file->path, &hlsli, 0)
			)
				goto clean;
		#endif

		//We have to detect file type and process it here to a custom type.
		//We don't have a custom file yet (besides oiSH), so for now
		//this will just be identical to addFileToCAFile.

		CAHandle handle = CAFile_addFile(pkgFile->archive, parent, &subPath, 0, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "packageFile() couldn't add file"));

		gotoIfError3(clean, CAFile_setData(pkgFile->archive, handle, alloc, &data, e_rr));
	}

	else {
		CAHandle handle = CAFile_addFolder(pkgFile->archive, parent, &subPath, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "packageFile() couldn't add folder"));
	}

clean:
	CharString_free(&subPath, alloc);
	Buffer_free(&data, alloc);
	return s_uccess;
}

Bool Packager_package(const PackageSettings *settings, const Allocator *alloc, Error *e_rr) {

	CAFile archive = (CAFile) { 0 };
	CharString resolved = CharString_createNull();
	Bool isVirtual = false;
	Bool s_uccess = true;
	StreamRef *stream = NULL;

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileOutputs = (ListU8) { 0 };
	ListBuffer allBuffers = (ListBuffer) { 0 };

	Ns start = Time_now();

	CASettings caSettings = (CASettings) { .compressionType = EXXCompressionType_None };

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType streamType = FileStream_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);

	if(!settings)
		retError(clean, Error_nullPointer(0, "Packager_package()::settings is required"));

	if(settings->encryptionKey)
		caSettings.encryptionType = EXXEncryptionType_AES256GCM;

	//Copying encryption key

	if(caSettings.encryptionType)
		Buffer_memcpy(
			Buffer_createRef(caSettings.encryptionKey, sizeof(caSettings.encryptionKey)),
			Buffer_createRefConst(settings->encryptionKey, sizeof(caSettings.encryptionKey))
		);

	//Grab all files that need compilation

	#ifdef CLI_SHADER_COMPILER
		gotoIfError3(clean, Compiler_getTargetsFromFile(
			settings->input,
			ECompileType_Compile,
			settings->compileMode,
			settings->multipleModes,
			settings->merge,
			settings->enableLogging,
			alloc,
			NULL,
			NULL,        //Don't write to output, write to Buffer[] instead
			&allFiles,
			&allShaderText,
			&allOutputs,
			&allCompileOutputs
		));
	#endif

	//Make archive

	gotoIfError3(clean, CAFile_create(&caSettings, 16, 8, alloc, &archive, e_rr));
	gotoIfError3(clean, File_resolve(&settings->input, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	gotoIfError3(clean, CharString_append(&resolved, '/', alloc, e_rr));

	CAFileRecursion caFileRecursion = (CAFileRecursion) {
		.archive = &archive,
		.root = resolved,
		.fileHandleType = &fileHandleType
	};

	gotoIfError3(clean, File_foreach(
		&caFileRecursion.root,
		false,
		(FileCallback) packageFile,
		&caFileRecursion,
		true,
		alloc,
		e_rr
	));

	//Convert shaders

	#ifdef CLI_SHADER_COMPILER

		if(allFiles.length)
			gotoIfError3(clean, Compiler_compileShaders(
				allFiles, allShaderText, allOutputs, allCompileOutputs,
				settings->threadCount,
				settings->isDebug,
				settings->extraWarnings,
				settings->ignoreEmptyFiles,
				ECompileType_Compile,
				settings->includeDir,
				true,
				alloc,
				&allBuffers,
				e_rr
			));

		for(U64 i = 0; i < allOutputs.length; ++i)

			if(Buffer_length(allBuffers.ptrNonConst[i])) {
				gotoIfError3(clean, CAFile_addFile(&archive, allOutputs.ptr[i], &allBuffers.ptrNonConst[i], 0, alloc, e_rr));
			} else {

				if(                                                            //Merged binaries contain empty buffers
					settings->merge &&
					i + 1 != allOutputs.length &&
					CharString_equalsStringSensitive(&allOutputs.ptr[i], &allOutputs.ptr[i + 1])
				)
					continue;

				retError(clean, Error_invalidState(0, "Packager_package() one of the shaders didn't compile, aborting packaging"));
			}

	#endif

	//Convert to CAFile and write to file

	gotoIfError3(clean, File_openStream(&settings->output, 50 * MS, EFileOpenType_Write, true, &fileHandleType, &streamType, &stream, e_rr));

	U64 startOffset = 0;
	gotoIfError3(clean, CAFile_write(&archive, &encStreamType, stream, &startOffset, alloc, e_rr));

clean:
	if(caSettings.encryptionType)
		Buffer_clearAllSecure(Buffer_createRef(caSettings.encryptionKey, sizeof(caSettings.encryptionKey)));

	F64 dt = (F64)(Time_now() - start) / SECOND;

	if(settings && settings->enableLogging) {

		if(s_uccess)
			Log_debugLn(alloc, "-- Packaging %s success in %fs!", resolved.ptr, dt);

		else Log_errorLn(alloc, "-- Packaging %s failed in %fs!", resolved.ptr, dt);

		if(e_rr)
			Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_NewLine);
	}

	ListBuffer_freeUnderlying(&allBuffers, alloc);
	ListCharString_freeUnderlying(&allFiles, alloc);
	ListCharString_freeUnderlying(&allShaderText, alloc);
	ListCharString_freeUnderlying(&allOutputs, alloc);
	ListU8_free(&allCompileOutputs, alloc);

	RefPtr_dec(&stream);
	CAFile_free(&archive, alloc);
	CharString_free(&resolved, alloc);

	return s_uccess;
}
