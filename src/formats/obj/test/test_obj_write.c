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

//formats/obj/test/test_obj_write.c

#include "test_obj_shared.h"
#include "formats/obj/obj_file.h"
#include "types/container/buffer.h"
#include "types/container/memory_stream.h"
#include "types/base/error.h"
#include <locale.h>

//A quad with a normal and a uv per corner, so every slot an OBJ corner can name is exercised.

static const C8 *const quadObj =
	"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
	"vn 0 0 1\n"
	"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
	"f 1/1/1 2/2/1 3/3/1\n"
	"f 1/1/1 3/3/1 4/4/1\n";

//Written and read back, the surface has to be the same one. Vertices may be renumbered on the way through,
// since a reader shares them in first use order, so the comparison walks triangles.

void Test_objWriteRoundTrip(Test *t) {

	Test_setModule(t, "OBJ/writeRoundTrip");

	MeshResult first = Test_meshRead(
		t, Obj_read, quadObj, CharString_calcStrLen(quadObj, U64_MAX), EMeshFlags_None, true, false, &t->err
	);

	if(!Test_assert(t, "read", first.ok))
		return;

	Test_assert(t, "vertexCount", first.info.vertexCount == 4);
	Test_assert(t, "indexCount", first.info.indexCount == 6);

	Buffer written = Test_meshWrite(t, Obj_write, &first, EMeshFlags_None);

	if(!Test_assert(t, "written", Buffer_length(written) != 0)) {
		MeshResult_free(t, &first);
		return;
	}

	MeshResult second = Test_meshRead(
		t, Obj_read, written.ptr, Buffer_length(written), EMeshFlags_None, true, false, &t->err
	);

	Buffer_free(&written, t->alloc);

	if(Test_assert(t, "reread", second.ok)) {

		Test_assert(t, "vertexCountBack", second.info.vertexCount == first.info.vertexCount);
		Test_assert(t, "normalsBack", second.info.hasNormals);
		Test_assert(t, "uvsBack", second.info.hasUvs);
		Test_meshTrianglesMatch(t, "surface", &first, &second);
	}

	MeshResult_free(t, &second);
	MeshResult_free(t, &first);
}

//Quantized positions decode back through the bounds MeshInfo carries, so the surface survives a lossy layout
// to within the step the quantizer took.

void Test_objWriteQuantized(Test *t) {

	Test_setModule(t, "OBJ/writeQuantized");

	MeshResult first = Test_meshRead(
		t, Obj_read, quadObj, CharString_calcStrLen(quadObj, U64_MAX),
		EMeshFlags_QuantizePositions, true, false, &t->err
	);

	if(!Test_assert(t, "read", first.ok))
		return;

	Buffer written = Test_meshWrite(t, Obj_write, &first, EMeshFlags_QuantizePositions);

	if(!Test_assert(t, "written", Buffer_length(written) != 0)) {
		MeshResult_free(t, &first);
		return;
	}

	MeshResult second = Test_meshRead(
		t, Obj_read, written.ptr, Buffer_length(written), EMeshFlags_None, true, false, &t->err
	);

	Buffer_free(&written, t->alloc);

	if(Test_assert(t, "reread", second.ok)) {

		Test_assert(t, "vertexCountBack", second.info.vertexCount == first.info.vertexCount);

		//The quad spans 0..1 on x and y and is flat on z, so every corner lands on a bound or the center and
		// comes back exactly; a flat axis quantizes to its only value.

		Bool exact = true;

		for(U32 i = 0; i < second.info.vertexCount; ++i) {

			const F32 *p = MeshResult_position(&second, i);

			for(U8 c = 0; c < 3; ++c)
				exact = exact && (Test_near(p[c], 0) || Test_near(p[c], 1));
		}

		Test_assert(t, "corners", exact);
	}

	MeshResult_free(t, &second);
	MeshResult_free(t, &first);
}

//An input carrying no attributes at all: the reader leaves normal and uv alone, and the writer declares neither,
// so a corner names its position and nothing else.

void Test_objWriteGeometryOnly(Test *t) {

	Test_setModule(t, "OBJ/writeGeometryOnly");

	MeshResult first = Test_meshRead(
		t, Obj_read, quadObj, CharString_calcStrLen(quadObj, U64_MAX), EMeshFlags_None, false, false, &t->err
	);

	if(!Test_assert(t, "read", first.ok))
		return;

	if(!Test_assert(t, "noAttributes", !Buffer_length(first.attributes))) {
		MeshResult_free(t, &first);
		return;
	}

	Buffer written = Test_meshWrite(t, Obj_write, &first, EMeshFlags_None);

	if(!Test_assert(t, "written", Buffer_length(written) != 0)) {
		MeshResult_free(t, &first);
		return;
	}

	MeshResult second = Test_meshRead(
		t, Obj_read, written.ptr, Buffer_length(written), EMeshFlags_None, true, false, &t->err
	);

	Buffer_free(&written, t->alloc);

	if(Test_assert(t, "reread", second.ok)) {
		Test_assert(t, "noNormalsBack", !second.info.hasNormals);
		Test_assert(t, "noUvsBack", !second.info.hasUvs);
		Test_meshTrianglesMatch(t, "surface", &first, &second);
	}

	MeshResult_free(t, &second);
	MeshResult_free(t, &first);
}

//A locale whose decimal separator is ',' must not change a byte of the text: "v 0,5 1,5 2,5" would read back
// as garbage or not at all.

void Test_objWriteLocale(Test *t) {

	Test_setModule(t, "OBJ/writeLocale");

	static const C8 *const fractions =
		"v 0.5 1.25 -2.5\nv 3.75 0.125 1.5\nv -0.5 2.5 0.0625\n"
		"vn 0.5 0.5 0.5\nvt 0.25 0.75\n"
		"f 1/1/1 2/1/1 3/1/1\n";

	MeshResult first = Test_meshRead(
		t, Obj_read, fractions, CharString_calcStrLen(fractions, U64_MAX), EMeshFlags_None, true, false, &t->err
	);

	if(!Test_assert(t, "read", first.ok))
		return;

	static const C8 *locales[] = { "de-DE", "de_DE.UTF-8", "de_DE" };

	Bool commaLocale = false;

	for(U64 l = 0; l < sizeof(locales) / sizeof(locales[0]); ++l) {

		if(!setlocale(LC_NUMERIC, locales[l]))
			continue;

		commaLocale = true;

		Buffer written = Test_meshWrite(t, Obj_write, &first, EMeshFlags_None);

		setlocale(LC_NUMERIC, "C");

		Bool comma = false, dot = false;

		for(U64 i = 0; i < Buffer_length(written); ++i) {
			comma |= written.ptr[i] == ',';
			dot |= written.ptr[i] == '.';
		}

		Test_assert(t, "dotNotComma", Buffer_length(written) && dot && !comma);
		Buffer_free(&written, t->alloc);
		break;
	}

	setlocale(LC_NUMERIC, "C");

	if(!commaLocale)
		Test_print(t, "OBJ write locale test ran in the C locale only (no decimal comma locale installed)");

	MeshResult_free(t, &first);
}

void Test_objWriteNonFinite(Test *t) {

	Test_setModule(t, "OBJ/writeNonFinite");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *source = NULL, *output = NULL;
	U32 words[9] = { 0 };
	const U32 values[] = { 0x7FC00000, 0x7F800000, 0xFF800000 };
	const U8 components[] = { 0, 1, 2, 7, 8 };

	//Three position floats, three triangle indices, then an oct normal and two full precision uv floats.

	if (!Test_assert(t, "source stream", MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(words, sizeof(words)), 0, 0, EMemoryStreamFlags_None, &type, &source, &t->err
	)))
		goto clean;

	const MeshInput input = {
		.positions = source, .indices = source, .indexOffset = 3 * sizeof(U32),
		.attributes = source, .attributeOffset = 6 * sizeof(U32)
	};

	const MeshInfo info = { .vertexCount = 1, .indexCount = 3, .hasNormals = true, .hasUvs = true };

	for(U8 i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
		for(U8 c = 0; c < sizeof(components) / sizeof(components[0]); ++c) {

			words[components[c]] = values[i];

			if (!Test_assert(t, "output stream", MemoryStream_create(
				16, EMemoryStreamFlags_WriteResize, &type, &output, &t->err
			)))
				goto clean;

			U64 off = 16;

			Test_assert(t, "non-finite position or uv rejected", !Obj_write(
				&input, &info, EMeshFlags_WideUvs, output, &off, t->alloc, NULL
			));

			Test_assert(t, "failed write preserves offset", off == 16);

			RefPtr_dec(&output);
			words[components[c]] = 0;
		}

clean:
	RefPtr_dec(&output);
	RefPtr_dec(&source);
}
