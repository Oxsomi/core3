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

//tools/package_cli/test/test_package_main.c
//
//The shader cache of Packager_package: the previous package plus one stamp file.
//A run has to leave those two files as the only ones there, however stale the cache is, since rewriting a file per shader
// in place is what anti-ransomware software kills a process for (see the top of packager.c).

#include "types/test/test.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "formats/gfx_util/gfx_util.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "shader_compiler/compiler.h"
#include "tools/package_cli/packager.h"

static const C8 *const PackageTest_root = "package_cache";
static const C8 *const PackageTest_a = "package_cache/input/a.hlsl";
static const C8 *const PackageTest_b = "package_cache/input/b.hlsl";
static const C8 *const PackageTest_outputDir = "package_cache/output";
static const C8 *const PackageTest_output = "package_cache/output/shaders.oiCA";
static const C8 *const PackageTest_stamp = "package_cache/output/shaders.oiCA.stamp";
static const C8 *const PackageTest_oldCache = "package_cache/output/shaders.oiCA.cache";

static const C8 *const PackageTest_shader = "[shader(\"compute\")]\n[numthreads(1, 1, 1)]\nvoid main() { }\n";
static const C8 *const PackageTest_shaderEdited = "//Edited\n[shader(\"compute\")]\n[numthreads(1, 1, 1)]\nvoid main() { }\n";

static void PackageTest_write(Test *t, const C8 *path, Buffer data) {
	const CharString loc = CharString_createRefCStrConst(path);
	Test_assert(t, "write", File_write(&data, &loc, 0, 0, SECOND, true, NULL, &t->err));
}

static void PackageTest_writeText(Test *t, const C8 *path, const C8 *text) {
	PackageTest_write(t, path, Buffer_createRefConst(text, CharString_calcStrLen(text, U64_MAX)));
}

//The caller owns it
static Buffer PackageTest_read(Test *t, const C8 *path) {
	const CharString loc = CharString_createRefCStrConst(path);
	Buffer result = Buffer_createNull();
	Test_assert(t, "read", File_read(&loc, SECOND, 0, 0, NULL, &result, &t->err));
	return result;
}

//Whether a file holds exactly these bytes
static Bool PackageTest_holds(Test *t, const C8 *path, Buffer expected) {
	Buffer data = PackageTest_read(t, path);
	const Bool equal = Buffer_eq(data, expected);
	Buffer_free(&data, t->alloc);
	return equal;
}

static Ns PackageTest_time(Test *t, const C8 *path) {

	const CharString loc = CharString_createRefCStrConst(path);
	FileInfo info = (FileInfo) { 0 };

	Test_assert(t, "time", File_getInfo(&loc, &info, t->alloc, &t->err));

	const Ns time = info.timestamp;
	FileInfo_free(&info, t->alloc);
	return time;
}

static Bool PackageTest_countFile(const FileInfo *file, void *count, const Allocator *alloc, Error *e_rr) {
	(void) file;
	(void) alloc;
	(void) e_rr;
	++*(U64*) count;
	return true;
}

//Everything in the output folder, folders included
static U64 PackageTest_outputCount(Test *t) {

	const CharString loc = CharString_createRefCStrConst(PackageTest_outputDir);
	U64 count = 0;

	Test_assert(t, "count", File_foreach(&loc, false, PackageTest_countFile, &count, true, t->alloc, &t->err));
	return count;
}

//How many shaders came out of the previous package.
//ignoreEmptyFiles is just a setting that goes into the stamp without changing what compiles.

static U64 PackageTest_run(Test *t, Bool ignoreEmptyFiles) {

	U64 reused = U64_MAX;

	const PackageSettings settings = (PackageSettings) {
		.input = CharString_createRefCStrConst("package_cache/input"),
		.output = CharString_createRefCStrConst(PackageTest_output),
		.compileMode = 1 << EGfxBinaryType_SPIRV,
		.threadCount = (U32) Platform_getThreads(),
		.merge = true,
		.enableLogging = true,
		.ignoreEmptyFiles = ignoreEmptyFiles,
		.shadersReused = &reused
	};

	Test_assert(t, "package", Packager_package(&settings, t->alloc, &t->err));
	return reused;
}

//The offset of the 8 hex digits behind a stamp's field
static U64 PackageTest_fieldOffset(Buffer stamp, const C8 *field) {

	const CharString text = CharString_createRefSizedConst((const C8*) stamp.ptr, Buffer_length(stamp), false);
	const CharString name = CharString_createRefCStrConst(field);

	return CharString_findFirstStringSensitive(&text, &name, 0, 0) + CharString_length(name);
}

static void Test_packageCache(Test *t) {

	Test_setModule(t, "Package/cache");

	const CharString root = CharString_createRefCStrConst(PackageTest_root);
	const CharString oldCache = CharString_createRefCStrConst(PackageTest_oldCache);

	Buffer package = Buffer_createNull(), stamp = Buffer_createNull(), stale = Buffer_createNull();

	if(File_hasFolder(&root, t->alloc))
		Test_assert(t, "removeOld", File_remove(&root, SECOND, t->alloc, &t->err));

	PackageTest_writeText(t, PackageTest_a, PackageTest_shader);
	PackageTest_writeText(t, PackageTest_b, PackageTest_shader);

	//A first run makes the package and its stamp, and nothing else

	Test_assert(t, "firstRunCompiles", PackageTest_run(t, false) == 0);
	Test_assert(t, "firstRunFiles", PackageTest_outputCount(t) == 2);
	Test_assert(t, "noCacheFolder", !File_hasFolder(&oldCache, t->alloc));

	package = PackageTest_read(t, PackageTest_output);
	stamp = PackageTest_read(t, PackageTest_stamp);

	//With nothing moved every shader comes out of the package, and a run writes nothing

	const Ns packageTime = PackageTest_time(t, PackageTest_output);
	const Ns stampTime = PackageTest_time(t, PackageTest_stamp);

	Test_assert(t, "cleanReuses", PackageTest_run(t, false) == 2);
	Test_assert(t, "cleanPackageUntouched", PackageTest_time(t, PackageTest_output) == packageTime);
	Test_assert(t, "cleanStampUntouched", PackageTest_time(t, PackageTest_stamp) == stampTime);

	//A moved toolchain compiles everything again.
	//That comes out the same, so only the stamp is written, which is the case that used to rewrite every cached file.

	stale = PackageTest_read(t, PackageTest_stamp);

	U8 *digit = stale.ptrNonConst + PackageTest_fieldOffset(stale, "toolchain ");
	*digit = *digit == '0' ? '1' : '0';

	PackageTest_write(t, PackageTest_stamp, stale);

	Test_assert(t, "staleCompiles", PackageTest_run(t, false) == 0);
	Test_assert(t, "stalePackageUntouched", PackageTest_time(t, PackageTest_output) == packageTime);
	Test_assert(t, "staleStampRewritten", PackageTest_holds(t, PackageTest_stamp, stamp));
	Test_assert(t, "staleFiles", PackageTest_outputCount(t) == 2);

	//The same goes for other settings

	Test_assert(t, "settingsCompile", PackageTest_run(t, true) == 0);
	Test_assert(t, "settingsPackageUntouched", PackageTest_time(t, PackageTest_output) == packageTime);
	Test_assert(t, "settingsStampRewritten", !PackageTest_holds(t, PackageTest_stamp, stamp));

	Test_assert(t, "settingsBackCompile", PackageTest_run(t, false) == 0);
	Test_assert(t, "settingsBackStamp", PackageTest_holds(t, PackageTest_stamp, stamp));

	//A package the stamp was not written for is not trusted, or a run that died between the two writes could hand out
	// binaries from another toolchain

	PackageTest_writeText(t, PackageTest_output, "Not the package the stamp was written for");

	Test_assert(t, "foreignCompiles", PackageTest_run(t, false) == 0);
	Test_assert(t, "foreignReplaced", PackageTest_holds(t, PackageTest_output, package));

	//An edited shader compiles alone and changes the package, still without any file besides these two

	PackageTest_writeText(t, PackageTest_a, PackageTest_shaderEdited);

	Test_assert(t, "editReusesOthers", PackageTest_run(t, false) == 1);
	Test_assert(t, "editChangesPackage", !PackageTest_holds(t, PackageTest_output, package));
	Test_assert(t, "editFiles", PackageTest_outputCount(t) == 2);

	Buffer_free(&stale, t->alloc);
	Buffer_free(&stamp, t->alloc);
	Buffer_free(&package, t->alloc);

	Test_assert(t, "remove", File_remove(&root, SECOND, t->alloc, &t->err));
}

OXC3_TEST_ENTRY(package) {

	Error err = Error_none();

	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &err)) {
		Test_printPlatformCreateFail(&err);
		Platform_return(1);
	}

	//No-op unless the shader compiler is a shared module, which has its own Platform_instance
	Compiler_setPlatform(Platform_instance);

	Test t = (Test) { 0 };
	t.alloc = Platform_instance->alloc;

	Test_packageCache(&t);

	int status = Test_end(&t);

	Platform_cleanup();
	Platform_return(status);
}
