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

//tools/package_cli/executable/oxc3_package_main.c

#include "tools/package_cli/packager.h"
#include "platforms/platform.h"
#include "types/base/string_read_helper.h"
#include "platforms/logx.h"

#ifdef CLI_SHADER_COMPILER
	#include "formats/oiSH/sh_file.h"
	#include "shader_compiler/compiler.h"
#endif

Platform_defineEntrypoint() {

	int status = 0;
	(void) status;
	Bool s_uccess = Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, NULL);

	if(!s_uccess)        //Can't print
		Platform_return(-2);

	#ifdef CLI_SHADER_COMPILER
		//No-op unless the shader compiler is a shared module, which has its own Platform_instance
		Compiler_setPlatform(Platform_instance);
	#endif

	ListCharString args = Platform_instance->args;

	//-raw keeps .hlsl/.hlsli as source instead of compiling them, for test data that gets read back as text.
	//Stripped here so the positional arguments below still line up.

	Bool keepShaderSource = false;

	for (U64 i = 0; i < args.length; ++i) {

		const CharString raw = CharString_createRefCStrConst("-raw");

		if(!CharString_equalsStringSensitive(&args.ptr[i], &raw))
			continue;

		keepShaderSource = true;
		ListCharString_popLocation(&args, i, NULL, NULL);
		break;
	}

	#ifdef PACKAGE_SIMPLE
		if (args.length != 2) {
			Log_debugLnx("Invalid arguments: Expected OxC3_package_simple input output");
			goto clean;
		}
	#else
		if (args.length < 2 || args.length > 3) {
			Log_debugLnx("Invalid arguments: Expected OxC3_package input output (optional: includeDir)");
			goto clean;
		}
	#endif

	CompilerWarning warnings = (CompilerWarning) 0;
	U32 compileMode = 0;
	Bool multipleModes = false;

	#ifdef CLI_SHADER_COMPILER

		warnings = ECompilerWarning_BufferPadding | ECompilerWarning_UnusedRegisters;

		compileMode = 1 << ESHBinaryType_SPIRV;

		//Always both formats on Windows rather than whichever the packager's own graphics config prefers.
		//The packaged oiCA outlives one configuration: a tree's four configs (and its bin/) share these
		// outputs, so a DXIL-only package written by a static D3D12 packager eventually meets a runtime
		// that can see Vulkan, which then dies at device create with "SPIRV binary is missing".
		//Compiling the prebuilt shaders twice costs seconds; diagnosing that mismatch didn't.

		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			multipleModes = true;
			compileMode |= 1 << ESHBinaryType_DXIL;
		#endif

	#endif

	PackageSettings packageSettings = (PackageSettings) {
		.input = args.ptr[0],
		.output = args.ptr[1],
		.encryptionKey = NULL,
		.multipleModes = multipleModes,
		.compileMode = compileMode,
		.threadCount = (U32) Platform_getThreads(),
		.includeDir = args.length == 3 ? args.ptr[2] : CharString_createNull(),
		.merge = true,
		.extraWarnings = warnings,
		.enableLogging = true,
		.isDebug = false,
		.ignoreEmptyFiles = false,
		.keepShaderSource = keepShaderSource
	};

	Error err = Error_none();

	if (!Packager_package(&packageSettings, Platform_instance->alloc, &err)) {
		status = -1;
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
		goto clean;
	}

clean:
	Platform_cleanup();
	Platform_return(status);
}
