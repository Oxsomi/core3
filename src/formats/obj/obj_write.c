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

//formats/obj/obj_write.c

#include "mesh_internal.h"
#include "mesh_input.h"
#include "formats/obj/obj_file.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/types.h"

Bool Obj_write(
	const MeshInput *input,
	const MeshInfo *info,
	EMeshFlags layout,
	StreamRef *stream,
	U64 *off,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	MeshInputReader reader = (MeshInputReader) { 0 };
	MeshSink sink = (MeshSink) { 0 };

	if(!stream || !off || !info || !input)
		retError(clean, Error_nullPointer(0, "Obj_write()::stream, off, info and input are required"));

	gotoIfError3(clean, MeshInputReader_create(input, info, layout, alloc, &reader, e_rr));
	gotoIfError3(clean, MeshSink_create(stream, *off, alloc, &sink, e_rr));

	//A normal is written when the file named one or a read computed one; nothing ever computes a uv.

	const Bool writeNormals = reader.hasAttributes && (info->hasNormals || info->allNormals);
	const Bool writeUvs = reader.hasAttributes && info->hasUvs;

	gotoIfError3(clean, MeshSink_writeFormatted(
		&sink, e_rr, "# OxC%u.%u.%u\n", OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH
	));

	F32 position[3] = { 0, 0, 0 }, normal[3] = { 0, 0, 1 }, uv[2] = { 0, 0 };

	for(U32 i = 0; i < info->vertexCount; ++i) {
		gotoIfError3(clean, MeshInputReader_vertex(&reader, i, position, normal, uv, e_rr));
		gotoIfError3(clean, MeshSink_writeFormatted(
			&sink, e_rr, "v %.9g %.9g %.9g\n", (F64) position[0], (F64) position[1], (F64) position[2]
		));
	}

	if(writeUvs)
		for(U32 i = 0; i < info->vertexCount; ++i) {
			gotoIfError3(clean, MeshInputReader_vertex(&reader, i, position, normal, uv, e_rr));
			gotoIfError3(clean, MeshSink_writeFormatted(&sink, e_rr, "vt %.9g %.9g\n", (F64) uv[0], (F64) uv[1]));
		}

	if(writeNormals)
		for(U32 i = 0; i < info->vertexCount; ++i) {
			gotoIfError3(clean, MeshInputReader_vertex(&reader, i, position, normal, uv, e_rr));
			gotoIfError3(clean, MeshSink_writeFormatted(
				&sink, e_rr, "vn %.9g %.9g %.9g\n", (F64) normal[0], (F64) normal[1], (F64) normal[2]
			));
		}

	//OBJ corners are 1 based, and the three arrays are parallel, so a corner names one index in every slot it
	// has. Which slots it has is what the file declared: v, v/vt, v//vn or v/vt/vn.

	for(U32 t = 0; t < info->indexCount / 3; ++t) {

		U32 tri[3] = { 0, 0, 0 };
		gotoIfError3(clean, MeshInputReader_triangle(&reader, t, tri, e_rr));

		if(writeUvs && writeNormals) {
			gotoIfError3(clean, MeshSink_writeFormatted(
				&sink, e_rr, "f %u/%u/%u %u/%u/%u %u/%u/%u\n",
				tri[0] + 1, tri[0] + 1, tri[0] + 1,
				tri[1] + 1, tri[1] + 1, tri[1] + 1,
				tri[2] + 1, tri[2] + 1, tri[2] + 1
			));
		}

		else if(writeNormals) {
			gotoIfError3(clean, MeshSink_writeFormatted(
				&sink, e_rr, "f %u//%u %u//%u %u//%u\n",
				tri[0] + 1, tri[0] + 1, tri[1] + 1, tri[1] + 1, tri[2] + 1, tri[2] + 1
			));
		}

		else if(writeUvs) {
			gotoIfError3(clean, MeshSink_writeFormatted(
				&sink, e_rr, "f %u/%u %u/%u %u/%u\n",
				tri[0] + 1, tri[0] + 1, tri[1] + 1, tri[1] + 1, tri[2] + 1, tri[2] + 1
			));
		}

		else {
			gotoIfError3(clean, MeshSink_writeFormatted(
				&sink, e_rr, "f %u %u %u\n", tri[0] + 1, tri[1] + 1, tri[2] + 1
			));
		}
	}

	gotoIfError3(clean, MeshSink_flush(&sink, e_rr));
	*off = sink.base + sink.written;

clean:

	MeshSink_free(&sink);
	MeshInputReader_free(&reader);
	return s_uccess;
}
