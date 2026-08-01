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

//tools/oxc3_cli/package.c

#include "types/container/list_basic_types.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/base/c8.h"
#include "types/base/error.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/container/buffer.h"
#include "types/container/string_helper.h"
#include "tools/oxc3_cli/cli.h"

#ifdef CLI_SHADER_COMPILER

	#include "tools/package_cli/packager.h"
	#include "shader_compiler/compiler.h"

	Bool CLI_package(const ParsedArgs *args) {

		if(!args) return false;

		//Parse encryption key

		U32 encryptionKeyV[8] = { 0 };
		U32 *encryptionKey = NULL;            //Only if we have aes should encryption key be set.
		Bool hasKey = false;
		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;

		//Parse encryption key (-aes / -aes-file / -aes-stdin)

		gotoIfError3(clean, CLI_getAesKey(args, encryptionKeyV, &hasKey, e_rr));

		if(hasKey)
			encryptionKey = encryptionKeyV;

		//Get input

		CharString input = (CharString) { 0 };
		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, e_rr));

		//Check if output is valid

		CharString output = (CharString) { 0 };
		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output, e_rr));

		//Get compile settings

		Bool multipleModes = false;
		U64 compileModeU64 = 0;
		U64 threadCount = 0;
		CharString includeDir = (CharString) { 0 };
		ECompilerWarning extraWarnings = (ECompilerWarning) 0;
		Bool merge = !(args->flags & EOperationFlags_Split);

		gotoIfError3(clean, CLI_parseCompileTypes(args, &compileModeU64, &multipleModes));
		gotoIfError3(clean, CLI_parseThreads(args, &threadCount, 1));

		if (args->parameters & EOperationHasParameter_IncludeDir)
			gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_IncludeDirShift, &includeDir, e_rr));

		extraWarnings = CLI_getExtraWarnings(args);

		{
			const PackageSettings packageSettings = {
				.input = input,
				.output = output,
				.encryptionKey = (const U32 (*)[8]) encryptionKey,
				.compileMode = (U32) compileModeU64,
				.threadCount = (U32) threadCount,
				.includeDir = includeDir,
				.extraWarnings = (CompilerWarning) extraWarnings,
				.merge = merge,
				.enableLogging = true,
				.isDebug = !!(args->flags & EOperationFlags_Debug),
				.ignoreEmptyFiles = !!(args->flags & EOperationFlags_IgnoreEmptyFiles),
				.multipleModes = multipleModes
			};

			gotoIfError3(clean, Packager_package(&packageSettings, Platform_instance->alloc, &err));
		}

	clean:

		if(encryptionKey)
			Buffer_clearAllSecure(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

		if(err.genericError)
			Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

		return s_uccess;
	}

#else
	Bool CLI_package(const ParsedArgs *args) {
		if(!args) return false;
		(void) args;
		return false;
	}
#endif
