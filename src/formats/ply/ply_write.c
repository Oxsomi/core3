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

//formats/ply/ply_write.c

#include "mesh_internal.h"
#include "mesh_input.h"
#include "formats/ply/ply_file.h"
#include "types/container/buffer.h"
#include "types/math/type_cast.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/types.h"

//A 32 bit value in the target's byte order. The file's order is what the header declared, which is not the
// host's, so the bytes are laid out here rather than copied.

static Bool Ply_write32(MeshSink *sink, U32 raw, Bool bigEndian, Error *e_rr) {

	U8 bytes[4];

	for(U8 i = 0; i < 4; ++i)
		bytes[i] = (U8) (raw >> (bigEndian ? 24 - i * 8 : i * 8));

	return MeshSink_write(sink, bytes, sizeof(bytes), e_rr);
}

Bool Ply_write(
	const MeshInput *input,
	const MeshInfo *info,
	EMeshFlags layout,
	EPlyWriteFormat format,
	StreamRef *stream,
	U64 *off,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	MeshInputReader reader = (MeshInputReader) { 0 };
	MeshSink sink = (MeshSink) { 0 };

	if(!stream || !off || !info || !input)
		retError(clean, Error_nullPointer(0, "Ply_write()::stream, off, info and input are required"));

	if(format >= EPlyWriteFormat_Count)
		retError(clean, Error_invalidParameter(3, 0, "Ply_write()::format is not one the specification names"));

	gotoIfError3(clean, MeshInputReader_create(input, info, layout, alloc, &reader, e_rr));
	gotoIfError3(clean, MeshSink_create(stream, *off, alloc, &sink, e_rr));

	//A normal is written when the file named one or a read computed one; nothing ever computes a uv.

	const Bool writeNormals = reader.hasAttributes && (info->hasNormals || info->allNormals);
	const Bool writeUvs = reader.hasAttributes && info->hasUvs;

	const Bool isAscii = format == EPlyWriteFormat_Ascii;
	const Bool bigEndian = format == EPlyWriteFormat_BinaryBigEndian;

	const C8 *formatName =
		isAscii ? "ascii" : (bigEndian ? "binary_big_endian" : "binary_little_endian");

	gotoIfError3(clean, MeshSink_writeFormatted(
		&sink, e_rr, "ply\nformat %s 1.0\ncomment OxC%u.%u.%u\n",
		formatName, OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH
	));

	gotoIfError3(clean, MeshSink_writeFormatted(&sink, e_rr, "element vertex %u\n", info->vertexCount));
	gotoIfError3(clean, MeshSink_writeFormatted(&sink, e_rr, "property float x\nproperty float y\nproperty float z\n"));

	if(writeNormals)
		gotoIfError3(clean, MeshSink_writeFormatted(
			&sink, e_rr, "property float nx\nproperty float ny\nproperty float nz\n"
		));

	//s and t rather than u and v, both of which the reader takes, since s/t is what the specification's own
	// examples use for a texture coordinate

	if(writeUvs)
		gotoIfError3(clean, MeshSink_writeFormatted(&sink, e_rr, "property float s\nproperty float t\n"));

	gotoIfError3(clean, MeshSink_writeFormatted(
		&sink, e_rr,
		"element face %u\nproperty list uchar int vertex_indices\nend_header\n", info->indexCount / 3
	));

	F32 position[3] = { 0, 0, 0 }, normal[3] = { 0, 0, 1 }, uv[2] = { 0, 0 };

	for(U32 i = 0; i < info->vertexCount; ++i) {

		gotoIfError3(clean, MeshInputReader_vertex(&reader, i, position, normal, uv, e_rr));

		if(isAscii) {

			gotoIfError3(clean, MeshSink_writeFormatted(
				&sink, e_rr, "%.9g %.9g %.9g", (F64) position[0], (F64) position[1], (F64) position[2]
			));

			if(writeNormals)
				gotoIfError3(clean, MeshSink_writeFormatted(
					&sink, e_rr, " %.9g %.9g %.9g", (F64) normal[0], (F64) normal[1], (F64) normal[2]
				));

			if(writeUvs)
				gotoIfError3(clean, MeshSink_writeFormatted(&sink, e_rr, " %.9g %.9g", (F64) uv[0], (F64) uv[1]));

			gotoIfError3(clean, MeshSink_writeFormatted(&sink, e_rr, "\n"));
			continue;
		}

		for(U8 c = 0; c < 3; ++c)
			gotoIfError3(clean, Ply_write32(&sink, U32_fromF32Bits(position[c]), bigEndian, e_rr));

		if(writeNormals)
			for(U8 c = 0; c < 3; ++c)
				gotoIfError3(clean, Ply_write32(&sink, U32_fromF32Bits(normal[c]), bigEndian, e_rr));

		if(writeUvs)
			for(U8 c = 0; c < 2; ++c)
				gotoIfError3(clean, Ply_write32(&sink, U32_fromF32Bits(uv[c]), bigEndian, e_rr));
	}

	for(U32 t = 0; t < info->indexCount / 3; ++t) {

		U32 tri[3] = { 0, 0, 0 };
		gotoIfError3(clean, MeshInputReader_triangle(&reader, t, tri, e_rr));

		if(isAscii) {
			gotoIfError3(clean, MeshSink_writeFormatted(&sink, e_rr, "3 %u %u %u\n", tri[0], tri[1], tri[2]));
			continue;
		}

		//The corner count is a uchar, which has no byte order of its own

		const U8 corners = 3;
		gotoIfError3(clean, MeshSink_write(&sink, &corners, sizeof(corners), e_rr));

		for(U8 c = 0; c < 3; ++c)
			gotoIfError3(clean, Ply_write32(&sink, tri[c], bigEndian, e_rr));
	}

	gotoIfError3(clean, MeshSink_flush(&sink, e_rr));
	*off = sink.base + sink.written;

clean:

	MeshSink_free(&sink);
	MeshInputReader_free(&reader);
	return s_uccess;
}
