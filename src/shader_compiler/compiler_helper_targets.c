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

//shader_compiler/compiler_helper_targets.c

#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"
#include "types/container/ref_ptr.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "shader_compiler/compiler.h"

typedef struct ShaderFileRecursion {

	ListCharString *allShaders;
	ListCharString *allOutputs;
	ListU8 *allModes;

	CharString base, output;

	U64 compileModeU64;

	Bool hasMultipleModes;
	Bool hasCombineFlag;
	U8 padding[2];

	ECompileType compileType;

	const Allocator *alloc;

} ShaderFileRecursion;

const C8 *oiSHCombineSuffix = ".oiSH";        //Suffix when oiSH is combined

const C8 *oiSHSuffixes[] = {
	".spv.oiSH",
	".dxil.oiSH"
};

Bool registerFile(const FileInfo *file, void *shaderFilesGeneric, const Allocator *alloc, Error *e_rr) {

	ShaderFileRecursion *shaderFiles = (ShaderFileRecursion*) shaderFilesGeneric;

	Bool s_uccess = true;
	CharString copy = CharString_createNull();
	CharString tempStr = CharString_createNull();

	if (file->type == EFileType_File) {

		CharString hlsl = CharString_createRefCStrConst(".hlsl");

		if (CharString_endsWithStringInsensitive(&file->path, &hlsl, 0)) {

			gotoIfError3(clean, CharString_createCopy(file->path, alloc, &copy, e_rr));

			//Move to allShaders

			gotoIfError3(clean, ListCharString_pushBack(shaderFiles->allShaders, copy, alloc, e_rr));
			copy = CharString_createNull();

			//Grab subPath

			CharString subPath = CharString_createNull();

			if(!CharString_cut(&file->path, CharString_length(shaderFiles->base), 0, &subPath))
				retError(clean, Error_invalidState(0, "registerFile() couldn't get subPath"));

			//Copy subPath

			gotoIfError3(clean, CharString_createCopy(subPath, alloc, &copy, e_rr));

			//Move subPath into new folder

			gotoIfError3(clean, CharString_insertString(&copy, &shaderFiles->output, 0, alloc, e_rr));

			//Handle multiple modes by inserting .spv.hlsl at the end

			Bool foundFirstMode = false;

			for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

				if(!((shaderFiles->compileModeU64 >> i) & 1))
					continue;

				gotoIfError3(clean, ListU8_pushBack(shaderFiles->allModes, i, alloc, e_rr));

				//Add double reference to input, so we don't waste memory (besides 24 for CharString struct itself)
				//Because we want to compile it with two different modes
				//The first mode already added one

				if(foundFirstMode) {

					CharString input = *ListCharString_last(*shaderFiles->allShaders);
					input = CharString_createRefStrConst(input);

					gotoIfError3(clean, ListCharString_pushBack(shaderFiles->allShaders, input, alloc, e_rr));
				}

				//Append .oiSH/.spv.oiSH/etc.

				const CharString hlslSuffix = CharString_createRefCStrConst(".hlsl");

				gotoIfError3(clean, CharString_format(
					alloc, &tempStr, e_rr, "%.*s%s",
					(int)U64_min(
						CharString_length(copy),
						CharString_findLastStringInsensitive(&copy, &hlslSuffix, 0, 0)
					),
					copy.ptr,
					shaderFiles->hasCombineFlag ? oiSHCombineSuffix : oiSHSuffixes[i]
				));

				gotoIfError3(clean, File_add(&tempStr, EFileType_File, true, alloc, e_rr));
				gotoIfError3(clean, ListCharString_pushBack(shaderFiles->allOutputs, tempStr, alloc, e_rr));
				tempStr = CharString_createNull();
				foundFirstMode = true;
			}

			CharString_free(&copy, alloc);
		}
	}

clean:
	CharString_free(&copy, alloc);
	CharString_free(&tempStr, alloc);
	return s_uccess;
}

Bool Compiler_getTargetsFromFile(
	CharString input,
	ECompileType compileType,
	U64 compileModeU64,
	Bool multipleModes,
	Bool combineFlag,
	Bool enableLogging,
	const Allocator *alloc,
	Bool *isFolder,
	CharString *output,
	ListCharString *allFiles,
	ListCharString *allShaderText,
	ListCharString *allOutputs,
	ListU8 *allCompileModes
) {

	Bool s_uccess = true;

	if (!allCompileModes || !allFiles || !allShaderText || !allOutputs) {
		if(enableLogging) Log_debugLn(alloc, "Compiler_getTargetsFromFile one of outputs is missing");
		return false;
	}

	CharString resolved = CharString_createNull();
	CharString resolved2 = CharString_createNull();
	CharString tempStr = CharString_createNull();
	Buffer temp = Buffer_createNull();

	Error errTmp = Error_none(), *e_rr = &errTmp;

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	//Get all shaders

	if (File_hasFolder(&input, alloc)) {

		Bool isVirtual;
		gotoIfError3(clean, File_resolve(&input, &isVirtual, 128, &Platform_instance->defaultDir, alloc, &resolved, e_rr));
		gotoIfError3(clean, CharString_append(&resolved, '/', alloc, e_rr));

		//Foreach reports full virtual paths ("//section/...") while resolve strips the marker; re-add it
		// so the base cut in registerFile lines up and allShaders entries stay valid File_read inputs.

		if(isVirtual) {
			const CharString virtualPrefix = CharString_createRefCStrConst("//");
			gotoIfError3(clean, CharString_insertString(&resolved, &virtualPrefix, 0, alloc, e_rr));
		}

		//A separate flag on purpose: reusing isVirtual would clobber the input's virtualness with the
		// output's, and it's the input that decides how registerFile stores paths.

		if(output) {
			Bool isVirtualOut;
			gotoIfError3(clean, File_resolve(
				output, &isVirtualOut, 128, &Platform_instance->defaultDir, alloc, &resolved2, e_rr
			));
			gotoIfError3(clean, CharString_append(&resolved2, '/', alloc, e_rr));
		}

		ShaderFileRecursion shaderFileRecursion = (ShaderFileRecursion) {
			.allShaders = allFiles,
			.allOutputs = allOutputs,
			.allModes = allCompileModes,
			.base = resolved,
			.output = resolved2,
			.compileModeU64 = compileModeU64,
			.hasMultipleModes = multipleModes,
			.hasCombineFlag = combineFlag,
			.compileType = compileType,
			.alloc = alloc
		};

		gotoIfError3(clean, File_foreach(
			&input,
			false,
			registerFile,
			&shaderFileRecursion,
			true,
			alloc,
			e_rr
		));

		//Make sure we can have a folder at output

		if(output)
			gotoIfError3(clean, File_add(&resolved2, EFileType_Folder, false, alloc, e_rr));

		if(isFolder) *isFolder = true;
	}

	//We need to add multiple compile modes

	else for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(!((compileModeU64 >> i) & 1))
			continue;

		//Replace output's .hlsl by .spv.hlsl or .dxil.hlsl

		const CharString hlslSuffix = CharString_createRefCStrConst(".hlsl");

		gotoIfError3(clean, CharString_format(
			alloc, &tempStr, e_rr, "%.*s%s",
			output ? (int)U64_min(
				CharString_length(*output),
				CharString_findLastStringInsensitive(output, &hlslSuffix, 0, 0)
			) : (int)(sizeof("output") - 1),
			output ? output->ptr : "output",
			oiSHSuffixes[i]
		));

		//Register mode and input/output name

		gotoIfError3(clean, ListCharString_pushBack(allFiles, input, alloc, e_rr));

		gotoIfError3(clean, ListCharString_pushBack(allOutputs, tempStr, alloc, e_rr));        //Moved here
		tempStr = CharString_createNull();

		gotoIfError3(clean, ListU8_pushBack(allCompileModes, i, alloc, e_rr));
	}

	//Only continue if there are any files and then fetch all files

	if (!allFiles->length)
		goto clean;

	CharString prevStr = CharString_createNull();

	for (U64 i = 0; i < allFiles->length; ++i) {

		//Grab from cache if we're re-compiling the same file with a different mode

		if (CharString_equalsStringSensitive(&prevStr, &allFiles->ptr[i])) {

			CharString shader = *ListCharString_last(*allShaderText);
			shader = CharString_createRefStrConst(shader);

			gotoIfError3(clean, ListCharString_pushBack(allShaderText, shader, alloc, e_rr));
			continue;
		}

		//Otherwise grab from file

		gotoIfError3(clean, File_read(&allFiles->ptr[i], 10 * MS, 0, 0, &fileHandleType, &temp, e_rr));

		if(!Buffer_length(temp)) {
			gotoIfError3(clean, ListCharString_pushBack(allShaderText, CharString_createNull(), alloc, e_rr));
			continue;
		}

		gotoIfError3(clean, CharString_createCopy(
			CharString_createRefSizedConst((const C8*)temp.ptr, Buffer_length(temp), false), alloc, &tempStr, e_rr
		));

		if(!CharString_eraseAllSensitive(&tempStr, '\r', 0, 0))
			retError(clean, Error_invalidState(1, "Compiler_getTargetsFromFile couldn't erase \\rs"));

		gotoIfError3(clean, ListCharString_pushBack(allShaderText, tempStr, alloc, e_rr));
		tempStr = CharString_createNull();

		Buffer_free(&temp, alloc);

		prevStr = allFiles->ptr[i];
	}

clean:
	Error_print(alloc, &errTmp, ELogLevel_Error, ELogOptions_Default);
	CharString_free(&resolved, alloc);
	CharString_free(&resolved2, alloc);
	Buffer_free(&temp, alloc);
	CharString_free(&tempStr, alloc);
	return s_uccess;
}
