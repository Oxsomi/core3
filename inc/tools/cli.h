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

#pragma once
#include "operations.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct FileInfo FileInfo;

void CLI_showHelp(EOperationCategory category, EOperation op, EFormat f);

Bool CLI_convertToDL(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
);

Bool CLI_convertFromDL(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
);

Bool CLI_convertToCA(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
);

Bool CLI_convertFromCA(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
);

Bool CLI_convertTo(ParsedArgs args);
Bool CLI_convertFrom(ParsedArgs args);
Bool CLI_fileCombine(ParsedArgs args);

Bool CLI_encryptDo(ParsedArgs args);
Bool CLI_encryptUndo(ParsedArgs args);

Bool CLI_hashFile(ParsedArgs args);
Bool CLI_hashString(ParsedArgs args);

Bool CLI_randKey(ParsedArgs args);
Bool CLI_randChar(ParsedArgs args);
Bool CLI_randData(ParsedArgs args);
Bool CLI_randNum(ParsedArgs args);

Bool CLI_profileCast(ParsedArgs args);
Bool CLI_profileRNG(ParsedArgs args);
Bool CLI_profileCRC32C(ParsedArgs args);
Bool CLI_profileFNV1A64(ParsedArgs args);
Bool CLI_profileSHA256(ParsedArgs args);
Bool CLI_profileMD5(ParsedArgs args);
Bool CLI_profileAES256(ParsedArgs args);
Bool CLI_profileAES128(ParsedArgs args);

Bool CLI_helpOperation(ParsedArgs args);

Bool CLI_inspectHeader(ParsedArgs args);
Bool CLI_inspectData(ParsedArgs args);

Bool CLI_package(ParsedArgs args);

typedef enum ECompileType {
	ECompileType_Preprocess,		//Turns shader with includes & defines into an easily parsable string
	ECompileType_Includes,			//Turns shader with includes into a list of their dependencies (direct + indirect)
	ECompileType_Compile,			//Compile all shaders into an oiSH file for consumption
	ECompileType_Symbols			//List all symbols located in the shader or include as a text file
} ECompileType;

#ifdef CLI_SHADER_COMPILER

	Bool CLI_parseCompileTypes(ParsedArgs args, U64 *maskBinaryType, Bool *multipleModes);
	Bool CLI_parseThreads(ParsedArgs args, U32 *threadCount, U32 defaultThreadCount);
	ECompilerWarning CLI_getExtraWarnings(ParsedArgs args);
	Bool CLI_getCompileTargetsFromFile(
		CharString input,
		ECompileType compileType,
		U64 compileModeU64,
		Bool multipleModes,
		Bool combineFlag,
		Bool *isFolder,					//Optional (out); if the input is a folder or not
		CharString *output,				//Optional; the output directory. If NULL, will output file names only (relative to none)
		ListCharString *allFiles,		//Fully resolved file names (may contain duplicates per compile mode)
		ListCharString *allShaderText,	//Per file name: Input shader files
		ListCharString *allOutputs,		//Per file name: Output shader file names
		ListU8 *allCompileModes			//Per file name: ESHBinaryType
	);

	Bool CLI_compileShaders(
		ListCharString allFiles,
		ListCharString allShaderText,
		ListCharString allOutputs,
		ListU8 allCompileOutputs,
		U32 threadCount,
		Bool isDebug,
		ECompilerWarning extraWarnings,
		Bool ignoreEmptyFiles,
		ECompileType type,
		CharString includeDir,			//Optional
		CharString outputDir,			//Optional
		ListBuffer *allBuffers,			//Optional: buffer outputs (if NULL, outputs to file)
		Error *e_rr
	);

#endif

Bool CLI_compileShader(ParsedArgs args);

Bool CLI_graphicsDevices(ParsedArgs args);

Bool CLI_execute(ListCharString argList);

//Should be called on init and shutdown of program

void CLI_init();
void CLI_shutdown();

#ifdef __cplusplus
	}
#endif
