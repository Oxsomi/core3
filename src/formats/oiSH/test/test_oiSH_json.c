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

//formats/oiSH/test/test_oiSH_json.c

#include "test_oiSH_shared.h"
#include "formats/json/json_writer.h"
#include "types/container/string.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"

//How many times a fragment occurs in the document, which is how the tests pin a shape without pinning the
// whole file, and how they see a register listed exactly once per place it belongs.

static U64 jsonCount(const CharString *doc, const C8 *needle) {

	const U64 length = CharString_length(*doc);
	U64 needleLength = 0;

	while(needle[needleLength])
		++needleLength;

	U64 count = 0;

	for (U64 i = 0; needleLength && i + needleLength <= length; ++i) {

		U64 j = 0;

		while(j < needleLength && doc->ptr[i + j] == needle[j])
			++j;

		if(j == needleLength)
			++count;
	}

	return count;
}

//SHDisassemble stubs: the formats test proves the plumbing without owning a compiler.

static Bool stubDisassemble(
	void *ctx, EGfxBinaryType type, Buffer binary, const Allocator *alloc, CharString *text, Error *e_rr
) {
	(void) ctx; (void) binary;
	return CharString_format(
		alloc, text, e_rr, "STUB DISASSEMBLY (%s)", type == EGfxBinaryType_SPIRV ? "spirv" : "dxil"
	);
}

static Bool hugeDisassemble(
	void *ctx, EGfxBinaryType type, Buffer binary, const Allocator *alloc, CharString *text, Error *e_rr
) {
	(void) ctx; (void) type; (void) binary;
	return CharString_create('x', 4 * 1024 * 1024 + 1, alloc, text, e_rr);
}

void Test_SHFileWriteJson(Test *t) {

	Test_setModule(t, "SHFile: JSON view");

	SHFile sh = (SHFile) { 0 };
	CharString whole = CharString_createNull();
	CharString wrapped = CharString_createNull();

	if (!Test_assert(t, "create", Test_SHFileCreate(t, &sh)))
		goto clean;

	//A compute binary carrying a sampler and a texture, the shape the register view is built from

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	CharString sampName = CharString_createRefCStrConst("gSampler");
	Test_assert(t, "add sampler", ListSHRegisterRuntime_addSampler(
		&info.registers, 0x3, false, &sampName, NULL, makeDualBinding(0, 0, 0), t->alloc, &t->err
	));

	CharString texName = CharString_createRefCStrConst("gTexture");
	Test_assert(t, "add texture", ListSHRegisterRuntime_addTexture(
		&info.registers, ESHTextureType_Texture2D, false, false, 0x3,
		EGfxTexturePrimitive_Float | EGfxTexturePrimitive_Component4,
		&texName, NULL, makeDualBinding(0, 1, 0), t->alloc, &t->err
	));

	Test_assert(t, "add binary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst("cs");
	e.stage = EGfxPipelineStage_Compute;
	e.groupX = e.groupY = e.groupZ = 8;
	U16 bid = 0;
	Test_assert(t, "binary ref", ListU16_createRefConst(&bid, 1, &e.binaryIds, &t->err));
	Test_assert(t, "add entry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	JsonWriter w = JsonWriter_create(&whole, false, t->alloc);

	Test_assert(
		t, "writes a complete document",
		SHFile_writeJson(&sh, NULL, NULL, &w, t->alloc, &t->err) && JsonWriter_isComplete(&w)
	);

	Test_assert(t, "no disassembler means no disassembly section", !jsonCount(&whole, "\"disassembly\""));

	Test_assert(t, "the entry", jsonCount(
		&whole,
		"\"entries\":[{\"name\":\"cs\",\"stage\":\"compute\",\"lib\":false,\"binaryIds\":[0],\"inputs\":[],\"outputs\":[],"
		"\"group\":[8,8,8],\"waveSize\":null}]"
	) == 1);

	Test_assert(t, "the binary", jsonCount(
		&whole, "\"binaries\":[{\"entrypoint\":\"cs\",\"stage\":\"compute\",\"lib\":false,\"entryNames\":[\"cs\"]"
	) == 1);

	//Every register is listed twice: under its binary and in the file's union

	Test_assert(t, "the sampler", jsonCount(
		&whole,
		"{\"name\":\"gSampler\",\"arrays\":[],\"typeStr\":\"SamplerState\",\"cls\":\"SMP\","
		"\"flags\":{\"write\":false,\"array\":false,\"combined\":false},"
		"\"bindings\":{\"spirv\":{\"set\":0,\"binding\":0},\"dxil\":{\"letter\":\"s\",\"binding\":0,\"space\":0}},"
		"\"used\":{\"spirv\":true,\"dxil\":true},\"push\":false,\"texture\":null,\"inputAttachment\":null,\"buffer\":null}"
	) == 2);

	Test_assert(t, "the texture", jsonCount(
		&whole,
		"{\"name\":\"gTexture\",\"arrays\":[],\"typeStr\":\"Texture2D\",\"cls\":\"SRV\","
		"\"flags\":{\"write\":false,\"array\":false,\"combined\":false},"
		"\"bindings\":{\"spirv\":{\"set\":0,\"binding\":1},\"dxil\":{\"letter\":\"t\",\"binding\":0,\"space\":0}}"
	) == 2);

	Test_assert(t, "a texture register carries no buffer", jsonCount(&whole, "\"buffer\":null}") == 4);

	//The members form lands inside an object the embedder opened, which is how a frame puts a name before them

	JsonWriter wm = JsonWriter_create(&wrapped, false, t->alloc);

	Test_assert(t, "the members write into an open object",
		JsonWriter_beginObject(&wm, &t->err) &&
		JsonWriter_keyCstr(&wm, "name", "x", &t->err) &&
		SHFile_writeJsonMembers(&sh, NULL, NULL, &wm, t->alloc, &t->err) &&
		JsonWriter_endObject(&wm, &t->err) && JsonWriter_isComplete(&wm)
	);

	Test_assert(t, "after the embedder's own keys", CharString_startsWithCStringSensitive(
		&wrapped, "{\"name\":\"x\",\"compilerVersion\":{", 0
	));

	//An injected disassembler renders each backend's code as text; a backend without code stays null,
	//and text past what a document carries becomes a stated omission with its size, oiDL's rule.

	{
		SHBinaryInfo di = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

		const C8 spvBytes[8] = { 3, 2, 35, 7, 0, 0, 0, 0 };
		di.binaries[EGfxBinaryType_SPIRV] = Buffer_createRefConst(spvBytes, sizeof(spvBytes));

		SHFile dsh = (SHFile) { 0 };
		CharString dj = CharString_createNull();

		Bool wrote =
			Test_SHFileCreate(t, &dsh) &&
			SHFile_addBinary(&dsh, &di, t->alloc, &t->err);

		JsonWriter dw = JsonWriter_create(&dj, false, t->alloc);

		wrote =
			wrote && SHFile_writeJson(&dsh, stubDisassemble, NULL, &dw, t->alloc, &t->err) &&
			JsonWriter_isComplete(&dw);

		Test_assert(t, "an injected disassembler renders the held backend as text", wrote && jsonCount(
			&dj, "\"disassembly\":{\"spirv\":\"STUB DISASSEMBLY (spirv)\",\"dxil\":null}"
		));

		CharString_free(&dj, t->alloc);
		SHFile_free(&dsh, t->alloc);

		//The oversized form: the stub hands back more text than a document carries.

		SHBinaryInfo bi = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
		bi.binaries[EGfxBinaryType_SPIRV] = Buffer_createRefConst(spvBytes, sizeof(spvBytes));

		SHFile bsh = (SHFile) { 0 };
		CharString bj = CharString_createNull();

		Bool omitted =
			Test_SHFileCreate(t, &bsh) &&
			SHFile_addBinary(&bsh, &bi, t->alloc, &t->err);

		JsonWriter bw = JsonWriter_create(&bj, false, t->alloc);

		omitted =
			omitted && SHFile_writeJson(&bsh, hugeDisassemble, NULL, &bw, t->alloc, &t->err) &&
			JsonWriter_isComplete(&bw);

		Test_assert(t, "text past the cap becomes a stated omission with its size", omitted && jsonCount(
			&bj, "\"disassembly\":{\"spirv\":{\"omitted\":4194305},\"dxil\":null}"
		));

		CharString_free(&bj, t->alloc);
		SHFile_free(&bsh, t->alloc);
	}

	Test_assert(t, "a missing file is refused", !SHFile_writeJson(NULL, NULL, NULL, &w, t->alloc, NULL));

clean:
	CharString_free(&whole, t->alloc);
	CharString_free(&wrapped, t->alloc);
	SHFile_free(&sh, t->alloc);
}
