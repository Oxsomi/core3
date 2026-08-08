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

//tools/oxc3_cli/executable/oxc3_main.c

#include "tools/oxc3_cli/cli.h"
#include "platforms/platform.h"

#ifdef CLI_SHADER_COMPILER
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

	CLI_init();

	if (!CLI_execute(Platform_instance->args)) {
		status = -1;
		goto clean;
	}

clean:
	CLI_shutdown();
	Platform_cleanup();
	Platform_return(status);
}
