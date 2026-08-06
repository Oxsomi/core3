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

//shader_compiler/test/test_shader_compiler_parse.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "platforms/platform.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_base.h"

//Parse a trivial compute shader with an oxc:: annotation and verify the reflection round-trips:
// one entrypoint is discovered and marked as a compute stage.
// This is the smallest end-to-end check of the DXC-backed parser and our annotation handling.

void Test_shaderCompilerParse(Test *t) {

	Test_setModule(t, "Compiler parse");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	Compiler comp = (Compiler) { 0 };
	CompileResult result = (CompileResult) { 0 };
	Bool created = false;

	static const C8 *src =
		"RWByteAddressBuffer buf;\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main(uint id : SV_DispatchThreadID) { buf.Store<uint>(id * 4, 1); }\n";

	gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));
	created = true;

	CompilerSettings settings = (CompilerSettings) {
		.string = CharString_createRefCStrConst(src),
		.path = CharString_createRefCStrConst("test_parse.hlsl"),
		.format = ECompilerFormat_HLSL,
		.outputType = ESHBinaryType_SPIRV
	};

	gotoIfError3(clean, Compiler_parse(&comp, &settings, alloc, &result, e_rr));

	Test_assert(t, "parse reports success", result.isSuccess);
	Test_assert(t, "parse yields entrypoint reflection", result.type == ECompileResultType_SHEntryRuntime);
	Test_assert(t, "exactly one entrypoint discovered", result.shEntriesRuntime.length == 1);

clean:

	Test_assert(t, "parse produced no error", s_uccess);

	CompileResult_free(&result, alloc);

	if(created)
		Compiler_free(&comp, alloc);

	Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
}

//Built-in includes are enumerable so tools don't have to hardcode the list; verify the table and the
//by-name lookup agree, since LoadSource resolves #include "@x.hlsli" through the same lookup.

void Test_shaderCompilerBuiltInIncludes(Test *t) {

	Test_setModule(t, "BuiltInIncludes");

	const U64 count = Compiler_builtInIncludeCount();
	Test_assert(t, "there are built-ins", count > 0);
	Test_assert(t, "out of range returns NULL", !Compiler_builtInIncludeAt(count));

	Bool allNamed = true, allSourced = true, allFindable = true, noAtInName = true;

	for(U64 i = 0; i < count; ++i) {

		const CompilerBuiltInInclude *entry = Compiler_builtInIncludeAt(i);

		if(!entry) {
			allNamed = allSourced = allFindable = false;
			break;
		}

		allNamed &= entry->name && entry->name[0];
		allSourced &= entry->source && entry->source[0];
		noAtInName &= entry->name[0] != '@';

		//Iteration and lookup must reach the same entry, or a shader could include something no tool lists

		allFindable &= Compiler_findBuiltInInclude(CharString_createRefCStrConst(entry->name)) == entry;
	}

	Test_assert(t, "every entry is named", allNamed);
	Test_assert(t, "every entry has source", allSourced);
	Test_assert(t, "names carry no @ prefix", noAtInName);
	Test_assert(t, "every entry is findable by name", allFindable);

	//The @ prefix is how a shader spells it, and resolution is case insensitive

	Test_assert(t, "found with @", Compiler_findBuiltInInclude(CharString_createRefCStrConst("@types.hlsli")));
	Test_assert(t, "found without @", Compiler_findBuiltInInclude(CharString_createRefCStrConst("types.hlsli")));
	Test_assert(t, "case insensitive", Compiler_findBuiltInInclude(CharString_createRefCStrConst("@TYPES.HLSLI")));

	Test_assert(t, "@ and bare agree", (
		Compiler_findBuiltInInclude(CharString_createRefCStrConst("@mat.hlsli")) ==
		Compiler_findBuiltInInclude(CharString_createRefCStrConst("mat.hlsli"))
	));

	Test_assert(t, "unknown is NULL", !Compiler_findBuiltInInclude(CharString_createRefCStrConst("@nope.hlsli")));
	Test_assert(t, "empty is NULL", !Compiler_findBuiltInInclude(CharString_createRefCStrConst("")));
	Test_assert(t, "bare @ is NULL", !Compiler_findBuiltInInclude(CharString_createRefCStrConst("@")));

	Test_setModule(t, NULL);
}
