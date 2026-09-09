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

//shader_compiler/test/test_shader_compiler_reflect_errors.c
//
//Compiler_reflect with settings->reflectAllowErrors over sources that do not compile.
//
//This is the editor path: an outline is wanted for a file mid-edit, so the reflection describes whatever
//survived clang's error recovery instead of refusing outright. The exact tree is the compiler's business and
//shifts with it, so what this asserts is the contract that makes the feature safe to build on:
//
//It returns rather than faulting, for every shape of broken input. The declarations a case names still reach
// the tree, so recovery does something rather than refusing everything quietly, which is what the feature is
// for. It serializes and reads back, because the page writes these to .oiSR. And without the flag the same
// source is still refused, so a build cannot consume a partial reflection.
//
//Structural soundness is not asserted here. SRFile_finalize judges it for every producer, so a reflect that
//returns at all has already passed it.

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "platforms/platform.h"
#include "formats/oiSR/sr_file.h"
#include "types/container/log.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/base/error.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

typedef struct BrokenCase {
	const C8 *name;
	const C8 *src;
	const C8 *recovers;     //Declaration the tree must still carry, or NULL when nothing is promised
} BrokenCase;

//A source whose errors sit inside a function body, or in one member of an otherwise complete file, keeps its
//declarations through clang's recovery, so those cases name one. A file that breaks before any declaration is
//complete promises nothing, and says so with NULL rather than by asserting whatever today happens to survive.

//Each one is a way HLSL breaks while it is being typed, not a synthetic parse fuzz: a half-written line, a
//name that isn't declared yet, a type whose header hasn't been included, a brace that hasn't been closed.

static const BrokenCase brokenCases[] = {

	{ "undeclared identifier in a body",
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = missingValue; }\n",
		"main" },

	{ "undefined type on a member",
		"struct Holder { NotAType member; float valid; };\n"
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = 1; }\n",
		"Holder" },

	{ "undefined type on a parameter",
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main(NotAType x) { buf[0] = 1; }\n",
		"main" },

	{ "unterminated function body",
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = 1;\n",
		NULL },

	{ "unterminated struct",
		"struct Half { float a;\n"
		"RWStructuredBuffer<float> buf;\n",
		NULL },

	{ "half-typed member access",
		"struct Light { float3 pos; };\n"
		"StructuredBuffer<Light> lights;\n"
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = lights[0]. }\n",
		"Light" },

	{ "call to an undeclared function",
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = notDeclared(1, 2); }\n",
		"main" },

	//An include that does not resolve is fatal to preprocessing, so nothing after it is ever parsed and the
	//tree comes back with its root and nothing else. That is the one broken shape an outline cannot describe.

	{ "missing include",
		"#include \"@doesNotExist.hlsli\"\n"
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = 1; }\n",
		NULL },

	{ "unterminated string in an annotation",
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute)]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = 1; }\n",
		NULL },

	{ "duplicate definition",
		"struct Dup { float a; };\n"
		"struct Dup { float b; };\n"
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = 1; }\n",
		"Dup" },

	{ "recursive type",
		"struct Node { Node next; float v; };\n"
		"RWStructuredBuffer<float> buf;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
		"void main() { buf[0] = 1; }\n",
		"Node" },

	{ "empty source",
		"",
		NULL },

	{ "only a comment",
		"//nothing here yet\n",
		NULL },

	{ "garbage tokens",
		"}}} ;;; ][ struct\n",
		NULL }
};

//Any node of any kind carrying this name. The kind a recovered declaration comes back as is the compiler's
//business (a function whose body did not parse may not stay a Function), so only the name is asserted.

static Bool srHasName(const SRFile *sr, const C8 *name) {

	CharString want = CharString_createRefCStrConst(name);

	for (U64 i = 0; i < sr->nodes.length; ++i) {

		U32 nameId = sr->nodes.ptr[i].nameId;

		if(nameId == U32_MAX || nameId >= sr->names.entryStrings.length)
			continue;

		if(CharString_equalsStringSensitive(&sr->names.entryStrings.ptr[nameId], &want))
			return true;
	}

	return false;
}

//Writes and reads back, which is what the page does when it downloads an .oiSR.

static Bool srRoundTrips(const SRFile *sr, const Allocator *alloc) {

	Bool ok = false;
	const RefPtrType streamType = MemoryStream_makeType(alloc);
	StreamRef *stream = NULL;
	SRFile back = (SRFile) { 0 };
	U64 offset = 0;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &streamType, &stream, NULL))
		goto clean;

	if(!SRFile_write(sr, alloc, stream, &offset, NULL))
		goto clean;

	offset = 0;

	if(!SRFile_read(stream, &offset, false, alloc, &back, NULL))
		goto clean;

	ok = back.nodes.length == sr->nodes.length;

clean:
	SRFile_free(&back, alloc);
	RefPtr_dec(&stream);
	return ok;
}

void Test_shaderCompilerReflectErrors(Test *t) {

	Test_setModule(t, "Compiler reflect (oiSR) over broken sources");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	Compiler comp = (Compiler) { 0 };
	Bool created = false;
	U64 recoveredCases = 0;

	gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));
	created = true;

	for (U64 i = 0; i < sizeof(brokenCases) / sizeof(brokenCases[0]); ++i) {

		const BrokenCase *test = &brokenCases[i];
		SRFile reflection = (SRFile) { 0 };
		Error caseErr = Error_none();

		CompilerSettings settings = (CompilerSettings) {
			.string = CharString_createRefCStrConst(test->src),
			.path = CharString_createRefCStrConst("broken.hlsl"),
			.format = ECompilerFormat_HLSL,
			.outputType = EGfxBinaryType_DXIL,
			.reflectAllowErrors = true
		};

		//Not asserted as success: whether anything survives is the compiler's decision, and either answer is
		//legitimate. What is asserted is that it returned at all, and that what it returned is usable.

		//Compiler_reflect finalizes what it builds, and SRFile_finalize rejects a tree whose references do not
		//land in the pools they name, so a reflect that succeeds is already sound.

		Bool reflected = Compiler_reflect(&comp, &settings, alloc, &reflection, &caseErr);

		if (reflected && reflection.nodes.length) {
			++recoveredCases;
			Test_assert(t, test->name, srRoundTrips(&reflection, alloc));
		}

		//The declaration the case names has to survive. Without this the suite passes on a reflect that
		//refuses everything, which is the state the flag exists to change and is exactly what it is worth
		//failing on.

		if(test->recovers)
			Test_assert(t, test->name, reflected && srHasName(&reflection, test->recovers));

		SRFile_free(&reflection, alloc);
	}

	Test_assert(t, "broken sources still recover a tree", recoveredCases > 0);

	//Without the flag the same sources are refused, so nothing that builds an oiSH can pick up a partial
	//reflection by accident.

	{
		SRFile reflection = (SRFile) { 0 };
		Error caseErr = Error_none();

		CompilerSettings strict = (CompilerSettings) {
			.string = CharString_createRefCStrConst(brokenCases[0].src),
			.path = CharString_createRefCStrConst("broken.hlsl"),
			.format = ECompilerFormat_HLSL,
			.outputType = EGfxBinaryType_DXIL
		};

		Test_assert(t, "without reflectAllowErrors a broken source is refused",
			!Compiler_reflect(&comp, &strict, alloc, &reflection, &caseErr
		));

		SRFile_free(&reflection, alloc);
	}

	//A source that does compile is unaffected by the flag: it is a relaxation, not a different walk.

	{
		SRFile reflection = (SRFile) { 0 };

		static const C8 *valid =
			"struct Light { float3 pos; };\n"
			"StructuredBuffer<Light> lights;\n"
			"RWStructuredBuffer<float> buf;\n"
			"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) { buf[id] = lights[0].pos.x; }\n";

		CompilerSettings settings = (CompilerSettings) {
			.string = CharString_createRefCStrConst(valid),
			.path = CharString_createRefCStrConst("valid.hlsl"),
			.format = ECompilerFormat_HLSL,
			.outputType = EGfxBinaryType_DXIL,
			.reflectAllowErrors = true
		};

		Bool ok = Compiler_reflect(&comp, &settings, alloc, &reflection, e_rr);

		Test_assert(t, "a valid source still reflects with the flag on", ok && reflection.nodes.length);
		Test_assert(t, "and it survives a round trip", srRoundTrips(&reflection, alloc));

		SRFile_free(&reflection, alloc);
	}

clean:

	Test_assert(t, "reflect errors module produced no error", s_uccess);

	if(created)
		Compiler_free(&comp, alloc);

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
