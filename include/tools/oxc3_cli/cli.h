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

//tools/oxc3_cli/cli.h

#pragma once
#include "tools/oxc3_cli/operations.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct FileInfo FileInfo;

void CLI_showHelp(EOperationCategory category, EOperation op, EFormat f);

typedef struct CLIConvert {
	const ParsedArgs *args;
	const CharString *input;
	const FileInfo *inputInfo;
	const CharString *output;
	U32 encKey[8];
} CLIConvert;

Bool CLI_convertToDL(const CLIConvert *convert, Error *e_rr);
Bool CLI_convertFromDL(const CLIConvert *convert, Error *e_rr);
Bool CLI_convertToCA(const CLIConvert *convert, Error *e_rr);
Bool CLI_convertFromCA(const CLIConvert *convert, Error *e_rr);

Bool CLI_convertTo(const ParsedArgs *args);
Bool CLI_convertFrom(const ParsedArgs *args);
Bool CLI_fileCombine(const ParsedArgs *args);

Bool CLI_encryptDo(const ParsedArgs *args);
Bool CLI_encryptUndo(const ParsedArgs *args);

Bool CLI_hashFile(const ParsedArgs *args);
Bool CLI_hashString(const ParsedArgs *args);

Bool CLI_randKey(const ParsedArgs *args);
Bool CLI_randChar(const ParsedArgs *args);
Bool CLI_randData(const ParsedArgs *args);
Bool CLI_randNum(const ParsedArgs *args);

Bool CLI_profileCast(const ParsedArgs *args);
Bool CLI_profileRNG(const ParsedArgs *args);
Bool CLI_profileCRC32C(const ParsedArgs *args);
Bool CLI_profileFNV1A64(const ParsedArgs *args);
Bool CLI_profileSHA256(const ParsedArgs *args);
Bool CLI_profileMD5(const ParsedArgs *args);
Bool CLI_profileAES256(const ParsedArgs *args);
Bool CLI_profileAES128(const ParsedArgs *args);
Bool CLI_profileMemcpy(const ParsedArgs *args);
Bool CLI_profileMemset(const ParsedArgs *args);
Bool CLI_profileVec(const ParsedArgs *args);

Bool CLI_helpOperation(const ParsedArgs *args);

Bool CLI_inspectHeader(const ParsedArgs *args);
Bool CLI_inspectData(const ParsedArgs *args);

Bool CLI_cpuDevices(const ParsedArgs *args);
Bool CLI_infoAll(const ParsedArgs *args);        //CPU + graphics + audio in one dump (support diagnostics)

U64 CLI_parseGraphicsAPIs(const ParsedArgs *args);        //U64_MAX indicates invalid, U32_MAX means all, otherwise bitmask

#ifdef CLI_SHADER_COMPILER

	Bool CLI_package(const ParsedArgs *args);

	Bool CLI_parseCompileTypes(const ParsedArgs *args, U64 *maskBinaryType, Bool *multipleModes);
	Bool CLI_parseThreads(const ParsedArgs *args, U64 *threadCount, U64 defaultThreadCount);

	ECompilerWarning CLI_getExtraWarnings(const ParsedArgs *args);

#endif

Bool CLI_compileShader(const ParsedArgs *args);

Bool CLI_graphicsDevices(const ParsedArgs *args);
Bool CLI_graphicsCreate(const ParsedArgs *args);

Bool CLI_audioDevices(const ParsedArgs *args);
Bool CLI_audioConvert(const ParsedArgs *args);

Bool CLI_execute(ListCharString argList);

//Should be called on init and shutdown of program

void CLI_init();
void CLI_shutdown();

#ifdef __cplusplus
	}
#endif
