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

//formats/oiSB/test/test_oiSB_json.c

#include "test_oiSB_shared.h"
#include "formats/oiSB/sb_file.h"
#include "formats/json/json_writer.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"

//The bytes are pinned: the document is what a reader and the web frontend's recording consume, so a moved key is
// a contract change rather than a style choice.

void Test_SBFileWriteJson(Test *t) {

	Test_setModule(t, "SBFile: JSON view");

	SBFile sb = (SBFile) { 0 };
	CharString out = CharString_createNull();

	//MyStruct { F32x4 pos; F32x4 col; } as myVar at offset 0 of a 48 byte buffer, so padding reports the 16 bytes
	// the layout left spare, and one used flag per backend so both halves of `used` are seen to travel

	if (!Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 48, t->alloc, &sb, &t->err)))
		goto clean;

	SBStruct myStruct = { .stride = 32 };
	CharString sName = CharString_createRefCStrConst("MyStruct");
	CharString vName = CharString_createRefCStrConst("myVar");
	CharString posName = CharString_createRefCStrConst("pos");
	CharString colName = CharString_createRefCStrConst("col");

	Test_assert(t, "add struct", SBFile_addStruct(&sb, &sName, myStruct, t->alloc, &t->err));

	Test_assert(t, "add struct var", SBFile_addVariableAsStruct(
		&sb, &vName, 0, U16_MAX, 0, ESBVarFlag_IsUsedVarSPIRV, NULL, t->alloc, &t->err
	));

	Test_assert(t, "add pos", SBFile_addVariableAsType(
		&sb, &posName, 0, 0, ESBType_F32x4, ESBVarFlag_None, NULL, t->alloc, &t->err
	));

	Test_assert(t, "add col", SBFile_addVariableAsType(
		&sb, &colName, 16, 0, ESBType_F32x4, ESBVarFlag_IsUsedVarDXIL, NULL, t->alloc, &t->err
	));

	JsonWriter w = JsonWriter_create(&out, false, t->alloc);

	Test_assert(t, "writes a complete document", SBFile_writeJson(&sb, &w, &t->err) && JsonWriter_isComplete(&w));

	CharString expected = CharString_createRefCStrConst(
		"{\"size\":48,\"padding\":16,\"packed\":false,\"vars\":["
		"{\"name\":\"myVar\",\"offset\":0,\"type\":\"MyStruct\",\"stride\":32,\"arrays\":[],"
		"\"used\":{\"spirv\":true,\"dxil\":false},\"children\":["
		"{\"name\":\"pos\",\"offset\":0,\"type\":\"F32x4\",\"stride\":16,\"arrays\":[],"
		"\"used\":{\"spirv\":false,\"dxil\":false},\"children\":[]},"
		"{\"name\":\"col\",\"offset\":16,\"type\":\"F32x4\",\"stride\":16,\"arrays\":[],"
		"\"used\":{\"spirv\":false,\"dxil\":true},\"children\":[]}]}]}"
	);

	Test_assert(t, "its bytes are exact", CharString_equalsStringSensitive(&out, &expected));
	Test_assert(t, "a missing file is refused", !SBFile_writeJson(NULL, &w, NULL));

clean:
	CharString_free(&out, t->alloc);
	SBFile_free(&sb, t->alloc);
}
