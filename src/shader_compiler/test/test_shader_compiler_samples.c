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

//shader_compiler/test/test_shader_compiler_samples.c
//
//The web frontend's sample project (web/samples/) compiled for both backends.
//
//The samples are the first thing a visitor edits and the page's own regression nets only exercise them
//through the wasm build, so this is the desktop guard: every sample has to compile for SPIRV and DXIL on
//its own, except broken.hlsl, which exists to fail and has to keep failing. Backends are compiled
//separately because that is how the page proves them too (coop_vec's buffer layout genuinely differs per
//backend, so the combined form is refused by design).
//
//Each sample's output is also pinned against a committed oiSH under test/samples/, one per backend, so a
//sample that still compiles but compiles to something else is caught. The samples are what a visitor reads
//to learn the annotations, so a silent change in what they produce is a documentation bug. A missing
//reference is written once and fails, the corpus convention, and an exact match is required before the
//version/include tolerance gets a say. coop_vec's DXIL is the one exception and says why at the skip.
//
//Desktop only, as its own executable running from the repository root (see samples/); a missing folder
//is a logged skip rather than a failure so a differently rooted run degrades loudly but green.

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/time.h"
#include "types/base/string_read_helper.h"

//Relative to the repository root: this test's executable runs from there (see samples/), because the
//file API jails a process to its working directory and the main suite's is the corpus folder.

static const C8 *samplesRoot = "web/samples";

//The pinned outputs, beside this test rather than beside the samples: web/samples is what the page serves and
//what a visitor browses, so nothing that is only a test fixture belongs in it.

static const C8 *goldenRoot = "src/shader_compiler/test/samples";

void Test_shaderCompilerSamples(Test *t) {

	Test_setModule(t, "Web sample project");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	const CharString root = CharString_createRefCStrConst(samplesRoot);
	const CharString oiSHExt = CharString_createRefCStrConst(".oiSH");

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileModes = (ListU8) { 0 };
	ListCharString includeDirs = (ListCharString) { 0 };
	ListBuffer buffers = (ListBuffer) { 0 };
	CharString refPath = CharString_createNull();
	CharString newPath = CharString_createNull();
	Buffer golden = Buffer_createNull();

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	if (!File_has(&root, alloc)) {
		Log_debugLn(alloc, "web/samples isn't reachable (bundled run), skipping the sample sweep");
		goto clean;
	}

	for (U64 backend = 0; backend < 2; ++backend) {

		const ESHBinaryType type = backend ? ESHBinaryType_DXIL : ESHBinaryType_SPIRV;
		Bool isFolder = false;

		ListCharString_freeUnderlying(&allFiles, alloc);
		ListCharString_freeUnderlying(&allShaderText, alloc);
		ListCharString_freeUnderlying(&allOutputs, alloc);
		ListU8_free(&allCompileModes, alloc);
		ListBuffer_freeUnderlying(&buffers, alloc);

		const Bool enumerated = Compiler_getTargetsFromFile(
			root, ECompileType_Compile, (U64)1 << type, false, true, true, alloc,
			&isFolder, NULL, &allFiles, &allShaderText, &allOutputs, &allCompileModes
		);

		Test_assert(t, "sample folder enumerated shaders", enumerated && isFolder && allFiles.length >= 10);

		if(!enumerated)
			goto clean;

		//Not asserted as a whole: broken.hlsl is in the batch and failing is its job.

		Compiler_compileShaders(
			&allFiles, &allShaderText, &allOutputs, &allCompileModes,
			1, false, false, false, (ECompilerWarning) 0, false, ECompileType_Compile,
			&includeDirs, true, alloc, &buffers, e_rr
		);

		err = Error_none();

		Test_assert(t, "one result per sample", buffers.length == allFiles.length);

		const CharString brokenName = CharString_createRefCStrConst("broken.hlsl");
		const CharString coopVecName = CharString_createRefCStrConst("coop_vec.hlsl");

		for (U64 i = 0; i < buffers.length && i < allFiles.length; ++i) {

			const Bool isBroken =
				CharString_findFirstStringSensitive(&allFiles.ptr[i], &brokenName, 0, 0) != U64_MAX;

			const Bool produced = Buffer_length(buffers.ptr[i]) > 0;

			//The name in the assert so a failure says which sample regressed on which backend.

			Log_debugLn(
				alloc, "sample %.*s (%s): %s",
				(int) CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr,
				type == ESHBinaryType_DXIL ? "dxil" : "spirv",
				produced ? "compiled" : "no binary"
			);

			Test_assert(t, allFiles.ptr[i].ptr, isBroken ? !produced : produced);

			if(!produced)                    //broken.hlsl compiles to nothing, so there is nothing to pin
				continue;

			//coop_vec's DXIL is not reproducible: the same source compiles to different bitcode from run to
			//run, moving the container hash and scattered metadata bytes, so pinning it would only make this
			//suite flaky. Its SPIR-V is stable and stays pinned, as does every other sample on both backends.

			const Bool unstable =
				type == ESHBinaryType_DXIL &&
				CharString_findFirstStringSensitive(&allFiles.ptr[i], &coopVecName, 0, 0) != U64_MAX;

			if(unstable)
				continue;

			//<sample>.<backend>.oiSH: the two backends are compiled separately and differ, so they pin separately.

			const CharString out = allOutputs.ptr[i];
			const U64 baseLen = CharString_endsWithStringInsensitive(&out, &oiSHExt, 0)
				? CharString_length(out) - CharString_length(oiSHExt) : CharString_length(out);

			CharString_free(&refPath, alloc);

			gotoIfError3(clean, CharString_format(
				alloc, &refPath, e_rr, "%s/%.*s.%s.oiSH",
				goldenRoot, (int) baseLen, out.ptr, type == ESHBinaryType_DXIL ? "dxil" : "spirv"
			));

			if (File_has(&refPath, alloc)) {

				Buffer_free(&golden, alloc);
				gotoIfError3(clean, File_read(&refPath, 1 * SECOND, 0, 0, &fileHandleType, &golden, e_rr));

				//Exact first, so an untouched reference is still compared byte for byte.

				Bool matches = Buffer_eq(buffers.ptr[i], golden);

				if(!matches && oiSHContentMatches(alloc, buffers.ptr[i], golden)) {
					Log_warnLn(
						alloc, "\t%.*s differs only in version/include metadata, content is identical",
						(int) CharString_length(refPath), refPath.ptr
					);
					matches = true;
				}

				//A mismatch is written out beside the reference so `git diff --no-index` (or OxC3 file data) can
				//show what moved, rather than leaving a byte comparison that only says no.

				if (!matches) {

					CharString_free(&newPath, alloc);

					gotoIfError3(clean, CharString_format(
						alloc, &newPath, e_rr, "%.*s.new", (int) CharString_length(refPath), refPath.ptr
					));

					gotoIfError3(clean, File_write(
						&buffers.ptr[i], &newPath, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr
					));

					Log_warnLn(
						alloc, "\tproduced oiSH written to %.*s",
						(int) CharString_length(newPath), newPath.ptr
					);
				}

				Test_assert(t, refPath.ptr, matches);
			}

			else {

				gotoIfError3(clean, File_write(
					&buffers.ptr[i], &refPath, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr
				));

				Log_warnLn(
					alloc, "Generated missing reference %.*s (review & commit)",
					(int) CharString_length(refPath), refPath.ptr
				);

				Test_assert(t, refPath.ptr, false);     //Red until the new reference is reviewed & committed
			}
		}
	}

clean:

	Test_assert(t, "sample sweep produced no error", s_uccess);

	ListCharString_freeUnderlying(&allFiles, alloc);
	ListCharString_freeUnderlying(&allShaderText, alloc);
	ListCharString_freeUnderlying(&allOutputs, alloc);
	ListCharString_free(&includeDirs, alloc);
	ListU8_free(&allCompileModes, alloc);
	ListBuffer_freeUnderlying(&buffers, alloc);
	CharString_free(&refPath, alloc);
	CharString_free(&newPath, alloc);
	Buffer_free(&golden, alloc);

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
	Test_setModule(t, NULL);
}
