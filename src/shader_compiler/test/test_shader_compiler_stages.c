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

//shader_compiler/test/test_shader_compiler_stages.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_entries.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/log.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"

//One shader per pipeline stage (from test/stages/), each a minimal-but-valid entrypoint for that stage.
//The case compiles it and asserts the reflected SHEntry carries the expected stage (SHEntry_stageName).
//Graphics + compute + mesh/task + raytracing (raygen/closesthit/anyhit/miss/intersection/callable) stages are covered.
//
//Most stages run on BOTH backends.
//The only remaining parked stage is inheritance
// (an upstream DXC SPIR-V assert on multi-base-class structs, unrelated to a specific stage).

#define ST_SPIRV (1 << ESHBinaryType_SPIRV)
#define ST_DXIL  (1 << ESHBinaryType_DXIL)
#define ST_BOTH  (ST_SPIRV | ST_DXIL)

typedef struct StageCase {
	const C8 *file;
	const C8 *stageName;    //Expected SHEntry_stageName (identical on both backends)
	U8 backends;            //Bitmask of (1 << ESHBinaryType_*) this stage is expected to compile on
} StageCase;

void Test_shaderCompilerStages(Test *t) {

	Test_setModule(t, "Compiler stages");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();

	static const StageCase stages[] = {
		{ "stages/vertex.hlsl",        "vertex",        ST_BOTH },
		{ "stages/pixel.hlsl",         "pixel",         ST_BOTH },
		{ "stages/geometry.hlsl",      "geometry",      ST_BOTH },
		{ "stages/hull.hlsl",          "hull",          ST_BOTH },
		{ "stages/domain.hlsl",        "domain",        ST_BOTH },
		{ "stages/mesh.hlsl",          "mesh",          ST_BOTH },
		{ "stages/task.hlsl",          "task",          ST_BOTH },
		{ "stages/raygeneration.hlsl", "raygeneration", ST_BOTH },
		{ "stages/closesthit.hlsl",    "closesthit",    ST_BOTH },
		{ "stages/anyhit.hlsl",        "anyhit",        ST_BOTH },
		{ "stages/miss.hlsl",          "miss",          ST_BOTH },
		{ "stages/intersection.hlsl",  "intersection",  ST_BOTH },
		{ "stages/callable.hlsl",      "callable",      ST_BOTH }
	};

	//Each stage is exercised on BOTH backends: the reflected stage must be identical on SPIRV and DXIL.
	static const struct { U8 mode; const C8 *name; } targets[] = {
		{ ESHBinaryType_SPIRV, "spirv" },
		{ ESHBinaryType_DXIL,  "dxil"  }
	};

	for (U64 i = 0; i < sizeof(stages) / sizeof(stages[0]); ++i)
		for (U64 tg = 0; tg < sizeof(targets) / sizeof(targets[0]); ++tg) {

			if(!(stages[i].backends & (1 << targets[tg].mode)))        //Stage not expected to compile on this backend
				continue;

			const C8 *file = stages[i].file;
			ListBuffer out = (ListBuffer) { 0 };
			SHFile shFile = (SHFile) { 0 };
			CharString label = CharString_createNull();

			Bool compiled = compileFileShader(alloc, file, targets[tg].mode, true, false, &out, &err);

			Bool hasEntry =
				compiled && out.length == 1 && Buffer_length(out.ptr[0]) &&
				readOiSH(alloc, out.ptr[0], &shFile, &err) && shFile.entries.length >= 1;

			Bool stageOk = false;

			if (hasEntry) {

				const CharString expected = CharString_createRefCStrConst(stages[i].stageName);

				for (U64 j = 0; j < shFile.entries.length; ++j) {

					const C8 *nm = SHEntry_stageName(&shFile.entries.ptr[j]);

					if (nm) {
						const CharString got = CharString_createRefCStrConst(nm);
						if (CharString_equalsStringSensitive(&got, &expected)) { stageOk = true; break; }
					}
				}
			}

			//The produced oiSH must also round-trip (read -> write -> byte-identical) on this backend.
			stageOk = stageOk && oiSHRoundtrips(alloc, out.ptr[0], &err);

			if (!stageOk)
				Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

			//Label includes the backend so a failure reads "stages/vertex.hlsl (dxil)"
			if (!CharString_format(alloc, &label, &err, "%s (%s)", file, targets[tg].name))
				label = CharString_createRefCStrConst(file);

			Test_assert(t, label.ptr, stageOk);

			CharString_free(&label, alloc);
			SHFile_free(&shFile, alloc);
			ListBuffer_freeUnderlying(&out, alloc);
			err = Error_none();
		}

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
