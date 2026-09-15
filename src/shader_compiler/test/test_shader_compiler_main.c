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

#include "types/test/test.h"
#include "test_shader_compiler_shared.h"
#include "platforms/platform.h"
#include "types/base/error.h"
#include "shader_compiler/compiler.h"
#include "platforms/file.h"
#include "types/container/memory_stream.h"
#include "test_shader_compiler_shared.h"

OXC3_TEST_ENTRY(shader_compiler) {

	Error err = Error_none();
	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &err)) {
		Test_printPlatformCreateFail(&err);
		Platform_return(1);
	}

	//No-op unless the shader compiler is a shared module, which has its own Platform_instance
	Compiler_setPlatform(Platform_instance);

	Test t = (Test) { 0 };
	t.alloc = Platform_instance->alloc;

	//Bundled builds read their HLSL out of the apk rather than a working directory,
	// so the section CMake attached has to be mounted first.
	//See TEST_SHADER_ROOT in test_shader_compiler_shared.h.

	#ifdef TEST_SHADER_SECTION
	{
		const RefPtrType memStreamType = MemoryStream_makeType(t.alloc);
		CharString section = CharString_createRefCStrConst(TEST_SHADER_SECTION);

		if(!File_loadVirtual(&section, &memStreamType, NULL, NULL, t.alloc, &err))
			Test_print(&t, "WARN: couldn't mount the shader test data, file backed shaders will fail");
	}
	#endif

	Test_shaderCompilerParse(&t);
	Test_shaderCompilerReflectSR(&t);
	Test_shaderCompilerBuiltInIncludes(&t);
	Test_shaderCompilerAnnotations(&t);
	Test_shaderCompilerFeatures(&t);
	Test_shaderCompilerStages(&t);
	Test_shaderCompilerReflection(&t);
	Test_shaderCompilerDriver(&t);
	Test_shaderCompilerPermutations(&t);
	//Runs bundled too: the corpus and its committed reference oiSHs ship in the virtual file system
	// (see TEST_SHADER_ROOT), where the test compares byte for byte but never regenerates,
	// since the bundle is read only.
	//A byte level difference between desktop and device DXC is exactly the kind of thing worth failing loudly on.

	Test_shaderCompilerCorpus(&t);

	int status = Test_end(&t);

	Platform_cleanup();
	Platform_return(status);
}
