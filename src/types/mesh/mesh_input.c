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

//types/mesh/mesh_input.c

#include "mesh_input.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/math/vec4f.h"
#include "types/math/pack.h"
#include "types/math/flp.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//A cursor per stream, since the three are walked in parallel and one cache would thrash between them.

static Bool MeshInputReader_open(
	StreamRef *stream, U64 base, U64 needed, U8 which, const Allocator *alloc, StreamCursor *cursor, Error *e_rr
) {

	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(which, "MeshInputReader_create()::input stream is required"));

	const OxStream *s = RefPtr_data(stream, OxStream);

	if(!s->read)
		retError(clean, Error_unsupportedOperation(which, "MeshInputReader_create()::input stream is not readable"));

	if(base > s->size || s->size - base < needed)
		retError(clean, Error_outOfBounds(
			which, base + needed, s->size, "MeshInputReader_create()::input stream is shorter than MeshInfo names"
		));

	gotoIfError3(clean, StreamCursor_create(stream, 0, false, alloc, cursor, e_rr));

clean:
	return s_uccess;
}

Bool MeshInputReader_create(
	const MeshInput *input,
	const MeshInfo *info,
	EMeshFlags layout,
	const Allocator *alloc,
	MeshInputReader *reader,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!input || !info || !reader)
		retError(clean, Error_nullPointer(0, "MeshInputReader_create()::input, info and reader are required"));

	if(info->indexCount % 3)
		retError(clean, Error_invalidParameter(1, 0, "MeshInputReader_create()::info->indexCount must be a multiple of 3"));

	*reader = (MeshInputReader) {

		.alloc = alloc,

		.positionBase = input->positionOffset,
		.attributeBase = input->attributeOffset,
		.indexBase = input->indexOffset,

		.vertexCount = info->vertexCount,
		.indexCount = info->indexCount,

		.attributeLayout = MeshAttributeLayout_fromFlags(layout),
		.hasAttributes = input->attributes != NULL,
		.quantized = !!(layout & EMeshFlags_QuantizePositions)
	};

	reader->positionStride = reader->quantized ? (U8)(sizeof(I16) * 4) : (U8)(sizeof(F32) * 3);
	reader->attributeStride = reader->attributeLayout.stride;
	reader->indexStride = (layout & EMeshFlags_NarrowIndices) ? (U8) sizeof(U16) : (U8) sizeof(U32);

	//The bounds are the space a quantized position lives in, so they are decoded once here rather than per vertex

	for(U8 i = 0; i < 3; ++i) {
		reader->center[i] = (info->aabbMax[i] - info->aabbMin[i]) * 0.5f + info->aabbMin[i];
		reader->halfExtent[i] = (info->aabbMax[i] - info->aabbMin[i]) * 0.5f;
	}

	gotoIfError3(clean, MeshInputReader_open(
		input->positions, reader->positionBase, (U64) info->vertexCount * reader->positionStride, 0, alloc,
		&reader->positions, e_rr
	));

	gotoIfError3(clean, MeshInputReader_open(
		input->indices, reader->indexBase, (U64) info->indexCount * reader->indexStride, 2, alloc,
		&reader->indices, e_rr
	));

	if(reader->hasAttributes)
		gotoIfError3(clean, MeshInputReader_open(
			input->attributes, reader->attributeBase, (U64) info->vertexCount * reader->attributeStride, 1, alloc,
			&reader->attributes, e_rr
		));

clean:

	if(!s_uccess)
		MeshInputReader_free(reader);

	return s_uccess;
}

void MeshInputReader_free(MeshInputReader *reader) {

	if(!reader)
		return;

	StreamCursor_close(&reader->positions, reader->alloc);
	StreamCursor_close(&reader->attributes, reader->alloc);
	StreamCursor_close(&reader->indices, reader->alloc);

	*reader = (MeshInputReader) { 0 };
}

Bool MeshInputReader_vertex(MeshInputReader *reader, U32 i, F32 position[3], F32 normal[3], F32 uv[2], Error *e_rr) {

	Bool s_uccess = true;

	if(!reader || !position)
		retError(clean, Error_nullPointer(0, "MeshInputReader_vertex()::reader and position are required"));

	if(i >= reader->vertexCount)
		retError(clean, Error_outOfBounds(1, i, reader->vertexCount, "MeshInputReader_vertex()::i out of bounds"));

	if(reader->quantized) {

		I16 q[4] = { 0, 0, 0, 0 };

		gotoIfError3(clean, StreamCursor_read(
			&reader->positions, Buffer_createRef(q, sizeof(q)),
			reader->positionBase + (U64) i * reader->positionStride, 0, sizeof(q), false, reader->alloc, e_rr
		));

		for(U8 c = 0; c < 3; ++c)
			position[c] = reader->center[c] + (F32) q[c] / 32767.f * reader->halfExtent[c];
	}

	else gotoIfError3(clean, StreamCursor_read(
		&reader->positions, Buffer_createRef(position, sizeof(F32) * 3),
		reader->positionBase + (U64) i * reader->positionStride, 0, sizeof(F32) * 3, false, reader->alloc, e_rr
	));

	if(!reader->hasAttributes)
		goto clean;

	//The record comes in as bytes and the layout says what is where, so no struct is reinterpreted as another
	//and an attribute the layout does not name simply never gets read.

	U8 record[MeshAttributeLayout_maxEntries * 8] = { 0 };

	gotoIfError3(clean, StreamCursor_read(
		&reader->attributes, Buffer_createRef(record, reader->attributeStride),
		reader->attributeBase + (U64) i * reader->attributeStride, 0, reader->attributeStride, false,
		reader->alloc, e_rr
	));

	if(normal) {

		const MeshAttributeEntry *ch = MeshAttributeLayout_find(&reader->attributeLayout, EMeshAttribute_Normal);

		normal[0] = normal[1] = normal[2] = 0;

		if(ch && ch->encoding == EMeshAttributeEncoding_Oct) {

			U32 packed = 0;

			Buffer_memcpy(
				Buffer_createRef(&packed, sizeof(packed)),
				Buffer_createRefConst(record + ch->offset, sizeof(packed))
			);

			F32x4_store3(normal, F32x4_unpackOct32(packed));
		}

		else if(ch) ETextureFormatId_decode(record + ch->offset, ch->format, normal, 3);
	}

	if(uv) {

		const MeshAttributeEntry *ch = MeshAttributeLayout_find(&reader->attributeLayout, EMeshAttribute_Uv0);

		uv[0] = uv[1] = 0;

		if(ch)
			ETextureFormatId_decode(record + ch->offset, ch->format, uv, 2);
	}

clean:
	return s_uccess;
}

Bool MeshInputReader_triangle(MeshInputReader *reader, U32 t, U32 indices[3], Error *e_rr) {

	Bool s_uccess = true;

	if(!reader || !indices)
		retError(clean, Error_nullPointer(0, "MeshInputReader_triangle()::reader and indices are required"));

	if((U64) t * 3 + 3 > reader->indexCount)
		retError(clean, Error_outOfBounds(1, t, reader->indexCount / 3, "MeshInputReader_triangle()::t out of bounds"));

	const U64 off = reader->indexBase + (U64) t * 3 * reader->indexStride;

	if(reader->indexStride == sizeof(U16)) {

		U16 narrow[3] = { 0, 0, 0 };

		gotoIfError3(clean, StreamCursor_read(
			&reader->indices, Buffer_createRef(narrow, sizeof(narrow)), off, 0, sizeof(narrow), false,
			reader->alloc, e_rr
		));

		for(U8 c = 0; c < 3; ++c)
			indices[c] = narrow[c];
	}

	else gotoIfError3(clean, StreamCursor_read(
		&reader->indices, Buffer_createRef(indices, sizeof(U32) * 3), off, 0, sizeof(U32) * 3, false,
		reader->alloc, e_rr
	));

	for(U8 c = 0; c < 3; ++c)
		if(indices[c] >= reader->vertexCount)
			retError(clean, Error_outOfBounds(
				1, indices[c], reader->vertexCount, "MeshInputReader_triangle() an index names no vertex"
			));

clean:
	return s_uccess;
}
