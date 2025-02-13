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
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "tools/package_cli/packager.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

Platform_defineEntrypoint() {

	int status = 0;
	Error err = Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true);

	if(err.genericError)		//Can't print
		Platform_return(-2);

	ListCharString args = Platform_instance->args;

	if (args.length < 2 || args.length > 3) {
		Log_debugLnx("Invalid arguments: Expected OxC3_package input output (optional: includeDir)");
		goto clean;
	}

	ECompilerWarning warnings = (ECompilerWarning) 0;

	#ifdef CLI_SHADER_COMPILER
		warnings = ECompilerWarning_BufferPadding | ECompilerWarning_UnusedRegisters;
	#endif

	Bool multipleModes = false;
	U64 compileModeU64 = 1 << ESHBinaryType_SPIRV;

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		#ifdef GRAPHICS_API_DYNAMIC		//Both DXIL and SPIRV
			multipleModes = true;
			compileModeU64 |= 1 << ESHBinaryType_DXIL;
		#elif !defined(FORCE_VULKAN)	//DXIL only
			compileModeU64 = 1 << ESHBinaryType_DXIL;
		#endif
	#endif

	if (!Packager_package(
		args.ptr[0],
		args.ptr[1],
		NULL,
		multipleModes,
		compileModeU64,
		Platform_getThreads(),
		args.length == 3 ? args.ptr[2] : CharString_createNull(),
		true,
		warnings,
		true,
		false,
		false,
		Platform_instance->alloc,
		&err
	)) {
		status = -1;
		goto clean;
	}

clean:
	Platform_cleanup();
	Platform_return(status);
}
