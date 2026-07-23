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

//shader_compiler/test/test_shader_compiler_main.c

#include "test_shader_compiler_shared.h"
#include "platforms/platform.h"
#include "types/base/error.h"

Platform_defineEntrypoint() {

	Error err = Error_none();
	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &err))
		Platform_return(1);

	Test t = (Test) { 0 };
	t.alloc = Platform_instance->alloc;

	Test_shaderCompilerParse(&t);
	Test_shaderCompilerAnnotations(&t);
	Test_shaderCompilerFeatures(&t);
	Test_shaderCompilerStages(&t);
	Test_shaderCompilerDriver(&t);
	Test_shaderCompilerCorpus(&t);

	int status = Test_end(&t);

	Platform_cleanup();
	Platform_return(status);
}
