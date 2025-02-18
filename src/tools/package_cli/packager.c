/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "tools/package_cli/packager.h"
#include "types/base/time.h"
#include "types/base/allocator.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/archive.h"
#include "formats/oiCA/ca_file.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/platform.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

typedef struct CAFileRecursion {
	Archive *archive;
	CharString root;
	Allocator alloc;
} CAFileRecursion;

Bool packageFile(FileInfo file, CAFileRecursion *caFile, Error *e_rr) {

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();
	Allocator alloc = caFile->alloc;

	if(!CharString_cut(file.path, CharString_length(caFile->root), 0, &subPath))
		retError(clean, Error_invalidState(0, "packageFile()::file.path cut failed"))

	ArchiveEntry entry = (ArchiveEntry) {
		.path = subPath,
		.type = file.type
	};

	if (entry.type == EFileType_File)
		gotoIfError3(clean, File_read(file.path, 100 * MS, 0, 0, alloc, &entry.data, e_rr))

	if (file.type == EFileType_File) {

		#ifdef CLI_SHADER_COMPILER
			if (
				CharString_endsWithStringSensitive(entry.path, CharString_createRefCStrConst(".hlsl"), 0) ||
				CharString_endsWithStringSensitive(entry.path, CharString_createRefCStrConst(".hlsli"), 0)
			)
				goto clean;
		#endif

		//We have to detect file type and process it here to a custom type.
		//We don't have a custom file yet (besides oiSH), so for now
		//this will just be identical to addFileToCAFile.

		gotoIfError3(clean, Archive_addFile(caFile->archive, entry.path, &entry.data, 0, alloc, e_rr))
	}

	else gotoIfError3(clean, Archive_addDirectory(caFile->archive, entry.path, alloc, e_rr))

clean:
	Buffer_free(&entry.data, alloc);
	return s_uccess;
}

Bool Packager_package(
	CharString input,
	CharString output,
	const U32 encryptionKey[8],
	Bool multipleModes,
	U64 compileModeU64,
	U64 threadCount,
	CharString includeDir,
	Bool merge,
	ECompilerWarning extraWarnings,
	Bool enableLogging,
	Bool isDebug,
	Bool ignoreEmptyFiles,
	Allocator alloc,
	Error *e_rr
) {
	Archive archive = (Archive) { 0 };
	CharString resolved = CharString_createNull();
	CAFile file = (CAFile) { 0 };
	Buffer res = Buffer_createNull();
	Bool isVirtual = false;
	Bool s_uccess = true;

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileOutputs = (ListU8) { 0 };
	ListBuffer allBuffers = (ListBuffer) { 0 };

	Ns start = Time_now();

	CASettings settings = (CASettings) { .compressionType = EXXCompressionType_None };

	if(encryptionKey)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_memcpy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRefConst(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Grab all files that need compilation

	#ifdef CLI_SHADER_COMPILER
		gotoIfError3(clean, Compiler_getTargetsFromFile(
			input,
			ECompileType_Compile,
			compileModeU64,
			multipleModes,
			merge,
			enableLogging,
			alloc,
			NULL,
			NULL,		//Don't write to output, write to Buffer[] instead
			&allFiles,
			&allShaderText,
			&allOutputs,
			&allCompileOutputs
		))
	#endif

	//Make archive

	gotoIfError3(clean, Archive_create(alloc, &archive, e_rr))
	gotoIfError3(clean, File_resolve(input, &isVirtual, 128, Platform_instance->defaultDir, alloc, &resolved, e_rr))

	gotoIfError2(clean, CharString_append(&resolved, '/', alloc))

	CAFileRecursion caFileRecursion = (CAFileRecursion) {
		.archive = &archive,
		.root = resolved,
		.alloc = alloc
	};

	gotoIfError3(clean, File_foreach(
		caFileRecursion.root,
		false,
		(FileCallback) packageFile,
		&caFileRecursion,
		true,
		e_rr
	))

	//Convert shaders

	#ifdef CLI_SHADER_COMPILER

		if(allFiles.length)
			gotoIfError3(clean, Compiler_compileShaders(
				allFiles, allShaderText, allOutputs, allCompileOutputs,
				threadCount,
				isDebug,
				extraWarnings,
				ignoreEmptyFiles,
				ECompileType_Compile,
				includeDir,
				CharString_createNull(),
				true,
				alloc,
				&allBuffers,
				e_rr
			))

		for(U64 i = 0; i < allOutputs.length; ++i)

			if(Buffer_length(allBuffers.ptrNonConst[i]))
				gotoIfError3(clean, Archive_addFile(&archive, allOutputs.ptr[i], &allBuffers.ptrNonConst[i], 0, alloc, e_rr))

			else {

				if(															//Merged binaries contain empty buffers
					merge &&
					i + 1 != allOutputs.length &&
					CharString_equalsStringSensitive(allOutputs.ptr[i], allOutputs.ptr[i + 1])
				)
					continue;

				retError(clean, Error_invalidState(0, "Packager_package() one of the shaders didn't compile, aborting packaging"))
			}

	#endif

	//Convert to CAFile and write to file

	gotoIfError3(clean, CAFile_create(settings, &archive, &file, e_rr))
	gotoIfError3(clean, CAFile_write(file, alloc, &res, e_rr))
	gotoIfError3(clean, File_write(res, output, 0, 0, 1 * SECOND, true, alloc, e_rr))

clean:
	if(settings.encryptionType)
		Buffer_unsetAllBits(Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)));

	F64 dt = (F64)(Time_now() - start) / SECOND;

	if(enableLogging) {

		if(s_uccess)
			Log_debugLn(alloc, "-- Packaging %s success in %fs!", resolved.ptr, dt);

		else Log_errorLn(alloc, "-- Packaging %s failed in %fs!!", resolved.ptr, dt);

		if(e_rr)
			Error_print(alloc, *e_rr, ELogLevel_Error, ELogOptions_NewLine);
	}

	ListBuffer_freeUnderlying(&allBuffers, alloc);
	ListCharString_freeUnderlying(&allFiles, alloc);
	ListCharString_freeUnderlying(&allShaderText, alloc);
	ListCharString_freeUnderlying(&allOutputs, alloc);
	ListU8_free(&allCompileOutputs, alloc);

	Buffer_free(&res, alloc);
	CAFile_free(&file, alloc);
	Archive_free(&archive, alloc);
	CharString_free(&resolved, alloc);

	return s_uccess;
}
