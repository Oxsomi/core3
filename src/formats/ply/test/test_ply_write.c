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

//formats/ply/test/test_ply_write.c

#include "test_ply_shared.h"
#include "formats/ply/ply_file.h"
#include "types/container/buffer.h"
#include "types/container/memory_stream.h"
#include "types/base/error.h"
#include <locale.h>

//The format is the one thing a writer takes beyond the shared signature, so each one gets a wrapper and the
// round trip below drives all three the same way.

static Bool Ply_writeAscii(
	const MeshInput *in, const MeshInfo *info, EMeshFlags layout,
	StreamRef *stream, U64 *off, const Allocator *alloc, Error *e_rr
) {
	return Ply_write(in, info, layout, EPlyWriteFormat_Ascii, stream, off, alloc, e_rr);
}

static Bool Ply_writeLittle(
	const MeshInput *in, const MeshInfo *info, EMeshFlags layout,
	StreamRef *stream, U64 *off, const Allocator *alloc, Error *e_rr
) {
	return Ply_write(in, info, layout, EPlyWriteFormat_BinaryLittleEndian, stream, off, alloc, e_rr);
}

static Bool Ply_writeBig(
	const MeshInput *in, const MeshInfo *info, EMeshFlags layout,
	StreamRef *stream, U64 *off, const Allocator *alloc, Error *e_rr
) {
	return Ply_write(in, info, layout, EPlyWriteFormat_BinaryBigEndian, stream, off, alloc, e_rr);
}

//The first corner sits at x = 1 so the first float of the body is 0x3F800000, whose byte order is visible.

static const C8 *const quadPly =
	"ply\nformat ascii 1.0\n"
	"element vertex 4\n"
	"property float x\nproperty float y\nproperty float z\n"
	"property float nx\nproperty float ny\nproperty float nz\n"
	"property float s\nproperty float t\n"
	"element face 2\nproperty list uchar int vertex_indices\nend_header\n"
	"1 0 0 0 0 1 0 0\n"
	"2 0 0 0 0 1 1 0\n"
	"2 1 0 0 0 1 1 1\n"
	"1 1 0 0 0 1 0 1\n"
	"3 0 1 2\n3 0 2 3\n";

static void Test_plyRoundTrip(Test *t, const C8 *name, MeshWriteFunc write) {

	MeshResult first = Test_meshRead(
		t, Ply_read, quadPly, CharString_calcStrLen(quadPly, U64_MAX), EMeshFlags_None, true, false, &t->err
	);

	if(!Test_assert(t, name, first.ok))
		return;

	Buffer written = Test_meshWrite(t, write, &first, EMeshFlags_None);

	if(!Test_assert(t, name, Buffer_length(written) != 0)) {
		MeshResult_free(t, &first);
		return;
	}

	MeshResult second = Test_meshRead(
		t, Ply_read, written.ptr, Buffer_length(written), EMeshFlags_None, true, false, &t->err
	);

	Buffer_free(&written, t->alloc);

	if(Test_assert(t, name, second.ok)) {
		Test_assert(t, name, second.info.vertexCount == first.info.vertexCount);
		Test_assert(t, name, second.info.hasNormals && second.info.hasUvs);
		Test_meshTrianglesMatch(t, name, &first, &second);
	}

	MeshResult_free(t, &second);
	MeshResult_free(t, &first);
}

void Test_plyWrite(Test *t) {

	Test_setModule(t, "PLY/write");

	Test_plyRoundTrip(t, "ascii", Ply_writeAscii);
	Test_plyRoundTrip(t, "littleEndian", Ply_writeLittle);
	Test_plyRoundTrip(t, "bigEndian", Ply_writeBig);
}

//A round trip alone cannot tell a correct writer from one whose byte order is wrong in the same way the reader's
// is, so the bytes themselves are read: 1.0f is 0x3F800000, which big endian lays out 3F 80 00 00 and little
// endian the other way around.

void Test_plyWriteByteOrder(Test *t) {

	Test_setModule(t, "PLY/writeByteOrder");

	MeshResult first = Test_meshRead(
		t, Ply_read, quadPly, CharString_calcStrLen(quadPly, U64_MAX), EMeshFlags_None, true, false, &t->err
	);

	if(!Test_assert(t, "read", first.ok))
		return;

	const C8 *marker = "end_header\n";
	const U64 markerLen = CharString_calcStrLen(marker, U64_MAX);

	for(U8 pass = 0; pass < 2; ++pass) {

		const Bool big = pass != 0;
		Buffer written = Test_meshWrite(t, big ? Ply_writeBig : Ply_writeLittle, &first, EMeshFlags_None);

		if(!Test_assert(t, "written", Buffer_length(written) != 0))
			break;

		//The body starts right after the header's last line

		const C8 *bytes = (const C8*) written.ptr;
		U64 body = 0;

		for(U64 i = 0; i + markerLen <= Buffer_length(written); ++i) {

			Bool hit = true;

			for(U64 c = 0; c < markerLen && hit; ++c)
				hit = bytes[i + c] == marker[c];

			if(hit) {
				body = i + markerLen;
				break;
			}
		}

		if(Test_assert(t, "bodyFound", body && body + 4 <= Buffer_length(written))) {

			const U8 *x = (const U8*) written.ptr + body;

			if(big)
				Test_assert(t, "bigEndianBytes", x[0] == 0x3F && x[1] == 0x80 && !x[2] && !x[3]);

			else Test_assert(t, "littleEndianBytes", !x[0] && !x[1] && x[2] == 0x80 && x[3] == 0x3F);
		}

		Buffer_free(&written, t->alloc);
	}

	MeshResult_free(t, &first);
}

//The same with no attributes, which for PLY decides the header itself: no nx/ny/nz and no s/t are declared, so
// a vertex record is three floats wide and the reader has to agree on that to find the next one.

void Test_plyWriteGeometryOnly(Test *t) {

	Test_setModule(t, "PLY/writeGeometryOnly");

	MeshResult first = Test_meshRead(
		t, Ply_read, quadPly, CharString_calcStrLen(quadPly, U64_MAX), EMeshFlags_None, false, false, &t->err
	);

	if(!Test_assert(t, "read", first.ok))
		return;

	for(U8 pass = 0; pass < 2; ++pass) {

		Buffer written = Test_meshWrite(
			t, pass ? Ply_writeBig : Ply_writeAscii, &first, EMeshFlags_None
		);

		if(!Test_assert(t, "written", Buffer_length(written) != 0))
			break;

		MeshResult second = Test_meshRead(
			t, Ply_read, written.ptr, Buffer_length(written), EMeshFlags_None, true, false, &t->err
		);

		Buffer_free(&written, t->alloc);

		if(Test_assert(t, "reread", second.ok)) {
			Test_assert(t, "noNormalsBack", !second.info.hasNormals);
			Test_assert(t, "noUvsBack", !second.info.hasUvs);
			Test_meshTrianglesMatch(t, "surface", &first, &second);
		}

		MeshResult_free(t, &second);
	}

	MeshResult_free(t, &first);
}

//The text form is read back by parsers that expect a '.', so a locale whose decimal separator is ',' must not
// change a single byte of it.

void Test_plyWriteLocale(Test *t) {

	Test_setModule(t, "PLY/writeLocale");

	static const C8 *const fractions =
		"ply\nformat ascii 1.0\n"
		"element vertex 3\n"
		"property float x\nproperty float y\nproperty float z\n"
		"element face 1\nproperty list uchar int vertex_indices\nend_header\n"
		"0.5 1.25 -2.5\n3.75 0.125 1.5\n-0.5 2.5 0.0625\n"
		"3 0 1 2\n";

	MeshResult first = Test_meshRead(
		t, Ply_read, fractions, CharString_calcStrLen(fractions, U64_MAX), EMeshFlags_None, false, false, &t->err
	);

	if(!Test_assert(t, "read", first.ok))
		return;

	static const C8 *locales[] = { "de-DE", "de_DE.UTF-8", "de_DE" };

	Bool commaLocale = false;

	for(U64 l = 0; l < sizeof(locales) / sizeof(locales[0]); ++l) {

		if(!setlocale(LC_NUMERIC, locales[l]))
			continue;

		commaLocale = true;

		Buffer written = Test_meshWrite(t, Ply_writeAscii, &first, EMeshFlags_None);

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
		Test_print(t, "PLY write locale test ran in the C locale only (no decimal comma locale installed)");

	MeshResult_free(t, &first);
}

void Test_plyWriteNonFinite(Test *t) {

	Test_setModule(t, "PLY/writeNonFinite");

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

			Test_assert(t, "non-finite position or uv rejected", !Ply_write(
				&input, &info, EMeshFlags_WideUvs, EPlyWriteFormat_Ascii, output, &off, t->alloc, NULL
			));

			Test_assert(t, "failed write preserves offset", off == 16);

			RefPtr_dec(&output);
			words[components[c]] = 0;
		}

clean:
	RefPtr_dec(&output);
	RefPtr_dec(&source);
}
