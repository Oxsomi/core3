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
#include "types/container/buffer.h"
#include "types/base/thread.h"
#include "types/math/math.h"
#include "types/base/time.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/threadx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/errorx.h"
#include "shader_compiler/compiler.h"
#include "tools/oxc3_cli/cli.h"

#ifdef CLI_SHADER_COMPILER

	Bool CLI_parseThreads(ParsedArgs args, U64 *threadCount, U64 defaultThreadCount) {

		if(!threadCount)
			return false;

		U64 maxThreads = Platform_getThreads();

		if(!(args.parameters & EOperationHasParameter_ThreadCount)) {
			*threadCount = !defaultThreadCount ? maxThreads : defaultThreadCount;
			return true;
		}

		CharString str = (CharString) { 0 };
		if(ParsedArgs_getArg(args, EOperationHasParameter_ThreadCountShift, &str).genericError)
			return false;

		if(CharString_endsWithSensitive(str, '%', 0)) {					//-threads 50%

			CharString number = CharString_createRefSizedConst(str.ptr, CharString_length(str) - 1, false);
			F64 num = 0;

			if (!CharString_parseDouble(number, &num) || num < 0 || num > 100) {
				Log_errorLnx("Couldn't parse -threads x%, x is expected to be a F64 between (0-100)% or 0 -> threadCount - 1");
				return false;
			}

			*threadCount = (U32) F64_max(1, maxThreads * num / 100);
			return true;
		}

		//-threads x

		U64 num = 0;
		if (!CharString_parseU64(str, &num) || num > maxThreads) {
			Log_errorLnx("Couldn't parse -threads x, where x is expected to be a F64 of (0-100)% or 0 -> threadCount - 1");
			return false;
		}

		*threadCount = (U32)num == 0 ? maxThreads : (U32)num;
		return true;
	}

	Bool CLI_parseCompileTypes(ParsedArgs args, U64 *maskBinaryType, Bool *multipleModes) {

		if(!maskBinaryType || !multipleModes)
			return false;

		if(!(args.parameters & EOperationHasParameter_ShaderOutputMode)) {
			*multipleModes = true;
			*maskBinaryType = (1 << ESHBinaryType_Count) - 1;
			return true;
		}

		CharString compileMode = CharString_createNull();
		if(ParsedArgs_getArg(args, EOperationHasParameter_ShaderOutputModeShift, &compileMode).genericError)
			return false;

		ListCharString splits = (ListCharString) { 0 };

		if(CharString_splitSensitivex(compileMode, ',', &splits).genericError)
			return false;

		CharString modes[] = {
			CharString_createRefCStrConst("spv"),
			CharString_createRefCStrConst("dxil"),
			CharString_createRefCStrConst("all")
		};

		static const U64 modeCount = sizeof(modes) / sizeof(modes[0]);
		*multipleModes = splits.length > 1;
		U64 compileModeU64 = 0;

		for (U64 i = 0; i < splits.length; ++i) {

			Bool match = false;

			for(U64 j = 0; j < modeCount; ++j)
				if (CharString_equalsStringInsensitive(splits.ptr[i], modes[j])) {

					if(j == modeCount - 1) {
						compileModeU64 = (1 << ESHBinaryType_Count) - 1;
						*multipleModes = true;
					}

					else compileModeU64 |= (U64)1 << j;

					match = true;
					break;
				}

			if(!match) {
				compileModeU64 = U64_MAX;
				Log_errorLnx("Couldn't parse -m x, where x is spv, dxil or all (or for example spv,dxil)");
				goto clean;
			}
		}

		*maskBinaryType = compileModeU64;

	clean:
		ListCharString_freex(&splits);
		return compileModeU64 != U64_MAX;
	}

	ECompilerWarning CLI_getExtraWarnings(ParsedArgs args) {

		ECompilerWarning extraWarnings = ECompilerWarning_None;

		if(args.flags & EOperationFlags_CompilerWarnings) {

			if(args.flags & EOperationFlags_WarnUnusedRegisters)
				extraWarnings |= ECompilerWarning_UnusedRegisters;

			if(args.flags & EOperationFlags_WarnUnusedConstants)
				extraWarnings |= ECompilerWarning_UnusedConstants;

			if(args.flags & EOperationFlags_WarnBufferPadding)
				extraWarnings |= ECompilerWarning_BufferPadding;
		}

		return extraWarnings;
	}

	Bool CLI_compileShader(ParsedArgs args) {

		//Get input

		ListCharString allFiles = (ListCharString) { 0 };
		ListCharString allShaderText = (ListCharString) { 0 };
		ListCharString allOutputs = (ListCharString) { 0 };
		ListU8 allCompileModes = (ListU8) { 0 };

		Error errTemp = Error_none(), *e_rr = &errTemp;
		Bool s_uccess = true;

		Ns start = Time_now();

		CharString input = (CharString) { 0 };
		gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input))

		CharString output = (CharString) { 0 };
		gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output))

		//Get compile types

		Bool multipleModes = false;
		U64 compileModeU64 = 0;
		gotoIfError3(clean, CLI_parseCompileTypes(args, &compileModeU64, &multipleModes))

		//Check thread count

		U64 threadCount = 0;
		gotoIfError3(clean, CLI_parseThreads(args, &threadCount, 0))

		//Compile type

		CharString compileTypeStr = (CharString) { 0 };
		ECompileType compileType = ECompileType_Compile;

		if(args.parameters & EOperationHasParameter_ShaderCompileMode) {

			gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_ShaderCompileModeShift, &compileTypeStr))

			if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("preprocess")))
				compileType = ECompileType_Preprocess;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("includes")))
				compileType = ECompileType_Includes;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("symbols")))
				compileType = ECompileType_Symbols;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("compile")))
				compileType = ECompileType_Compile;

			else {
				Log_errorLnx("Unknown shader compile mode passed %s", compileTypeStr.ptr);
				s_uccess = false;
				goto clean;
			}
		}

		//Additional includeDir

		CharString includeDir = (CharString) { 0 };

		if (args.parameters & EOperationHasParameter_IncludeDir)
			gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_IncludeDirShift, &includeDir))

		//Grab all files that need compilation

		Bool isFolder = false;
		gotoIfError3(clean, Compiler_getTargetsFromFile(
			input,
			compileType,
			compileModeU64,
			multipleModes,
			!(args.flags & EOperationFlags_Split),
			true,
			Platform_instance->alloc,
			&isFolder,
			&output,
			&allFiles,
			&allShaderText,
			&allOutputs,
			&allCompileModes
		))

		//Grab info about extra detailed compiler warnings

		ECompilerWarning extraWarnings = CLI_getExtraWarnings(args);

		//Compile

		gotoIfError3(clean, Compiler_compileShaders(
			allFiles, allShaderText, allOutputs, allCompileModes,
			threadCount,
			args.flags & EOperationFlags_Debug,
			extraWarnings,
			args.flags & EOperationFlags_IgnoreEmptyFiles,
			ECompileType_Compile,
			includeDir,
			isFolder ? output : CharString_createNull(),
			true,
			Platform_instance->alloc,
			NULL,
			e_rr
		))

	clean:

		F64 dt = (F64)(Time_now() - start) / SECOND;

		if(s_uccess)
			Log_debugLnx("-- Compile %.*s success in %fs", (int)CharString_length(input), input.ptr, dt);

		else Log_errorLnx("-- Compile %.*s failed in %fs!", (int)CharString_length(input), input.ptr, dt);

		Error_printx(errTemp, ELogLevel_Error, ELogOptions_Default);

		ListCharString_freeUnderlyingx(&allFiles);
		ListCharString_freeUnderlyingx(&allShaderText);
		ListCharString_freeUnderlyingx(&allOutputs);
		ListU8_freex(&allCompileModes);

		return s_uccess;
	}
#else
	Bool CLI_compileShader(ParsedArgs args) { (void) args; return false; }
#endif
