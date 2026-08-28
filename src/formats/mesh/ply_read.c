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

//formats/mesh/ply_read.c

#include "formats/ply/ply_file.h"
#include "mesh_internal.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/string_read.h"
#include "types/math/type_cast.h"

//The scalar types the specification names, in both spellings, and the width of each.

typedef enum EPLYType {
	EPLYType_Char, EPLYType_UChar,
	EPLYType_Short, EPLYType_UShort,
	EPLYType_Int, EPLYType_UInt,
	EPLYType_Float, EPLYType_Double,
	EPLYType_Count
} EPLYType;

static const U8 PLY_typeSize[EPLYType_Count] = { 1, 1, 2, 2, 4, 4, 4, 8 };

static const C8 *const PLY_typeNames[EPLYType_Count][2] = {
	{ "char", "int8" }, { "uchar", "uint8" },
	{ "short", "int16" }, { "ushort", "uint16" },
	{ "int", "int32" }, { "uint", "uint32" },
	{ "float", "float32" }, { "double", "float64" }
};

static Bool PLY_parseType(CharString token, U8 *type) {

	for(U8 i = 0; i < EPLYType_Count; ++i)
		for(U8 j = 0; j < 2; ++j)
			if(CharString_equalsCString(&token, PLY_typeNames[i][j], EStringCase_Sensitive)) {
				*type = i;
				return true;
			}

	return false;
}

//What a property means to this reader. Everything it does not recognize is read and dropped.

typedef enum EPLYSemantic {
	EPLYSemantic_None,
	EPLYSemantic_X, EPLYSemantic_Y, EPLYSemantic_Z,
	EPLYSemantic_NX, EPLYSemantic_NY, EPLYSemantic_NZ,
	EPLYSemantic_U, EPLYSemantic_V,
	EPLYSemantic_Indices
} EPLYSemantic;

typedef struct PLYProperty {
	U8 type;                      //EPLYType of the value, or of the list's items
	U8 countType;                 //EPLYType of the list's count, when isList
	U8 semantic;                  //EPLYSemantic
	Bool isList;
} PLYProperty;

//Bounded rather than listed: no file in the wild comes near either limit, and a fixed table keeps the header
// parse free of allocation.

#define PLY_MAX_PROPERTIES 32
#define PLY_MAX_ELEMENTS 16

typedef struct PLYElement {
	U32 count;
	U8 propertyCount;
	Bool isVertex, isFace;
	U8 padding;
	PLYProperty properties[PLY_MAX_PROPERTIES];
} PLYElement;

typedef enum EPLYFormat { EPLYFormat_Ascii, EPLYFormat_LittleEndian, EPLYFormat_BigEndian } EPLYFormat;

typedef struct PLYHeader {
	U8 format;                    //EPLYFormat
	U8 elementCount;
	U8 padding[2];
	U32 vertexCount;
	PLYElement elements[PLY_MAX_ELEMENTS];
} PLYHeader;

static Bool PLY_tokenIs(CharString token, const C8 *literal) {
	return CharString_equalsCString(&token, literal, EStringCase_Sensitive);
}

//Which of a vertex's properties this reader wants. The first uv pair wins, whichever of the three spellings
// it comes in, so a file carrying two sets keeps the one it listed first.

static U8 PLY_vertexSemantic(CharString name, Bool *sawU, Bool *sawV) {

	if(PLY_tokenIs(name, "x")) return EPLYSemantic_X;
	if(PLY_tokenIs(name, "y")) return EPLYSemantic_Y;
	if(PLY_tokenIs(name, "z")) return EPLYSemantic_Z;
	if(PLY_tokenIs(name, "nx")) return EPLYSemantic_NX;
	if(PLY_tokenIs(name, "ny")) return EPLYSemantic_NY;
	if(PLY_tokenIs(name, "nz")) return EPLYSemantic_NZ;

	if(!*sawU && (PLY_tokenIs(name, "s") || PLY_tokenIs(name, "u") || PLY_tokenIs(name, "texture_u"))) {
		*sawU = true;
		return EPLYSemantic_U;
	}

	if(!*sawV && (PLY_tokenIs(name, "t") || PLY_tokenIs(name, "v") || PLY_tokenIs(name, "texture_v"))) {
		*sawV = true;
		return EPLYSemantic_V;
	}

	return EPLYSemantic_None;
}

static Bool PLY_readHeader(MeshSource *src, C8 *line, PLYHeader *header, MeshInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	U64 len = 0;
	Bool got = false;

	gotoIfError3(clean, MeshSource_readLine(src, line, MESH_LINE_CAP, &len, &got, e_rr));

	if(!got || len != 3 || line[0] != 'p' || line[1] != 'l' || line[2] != 'y')
		retError(clean, Error_invalidParameter(0, 0, "PLY_read() the file doesn't start with ply"));

	Bool sawFormat = false, sawEnd = false;
	Bool sawU = false, sawV = false;
	PLYElement *current = NULL;

	for(;;) {

		gotoIfError3(clean, MeshSource_readLine(src, line, MESH_LINE_CAP, &len, &got, e_rr));

		if(!got)
			retError(clean, Error_invalidParameter(0, 1, "PLY_read() the header never ends"));

		U64 pos = 0;
		CharString keyword, a, b, c;

		if(!Mesh_nextToken(line, len, &pos, &keyword))
			continue;

		if(PLY_tokenIs(keyword, "comment") || PLY_tokenIs(keyword, "obj_info"))
			continue;

		if(PLY_tokenIs(keyword, "end_header")) {
			sawEnd = true;
			break;
		}

		if(PLY_tokenIs(keyword, "format")) {

			if(!Mesh_nextToken(line, len, &pos, &a))
				retError(clean, Error_invalidParameter(0, 2, "PLY_read() the format line names no format"));

			if(PLY_tokenIs(a, "ascii")) header->format = EPLYFormat_Ascii;
			else if(PLY_tokenIs(a, "binary_little_endian")) header->format = EPLYFormat_LittleEndian;
			else if(PLY_tokenIs(a, "binary_big_endian")) header->format = EPLYFormat_BigEndian;
			else retError(clean, Error_unsupportedOperation(0, "PLY_read() the format is not one the specification names"));

			sawFormat = true;
			continue;
		}

		if(PLY_tokenIs(keyword, "element")) {

			if(header->elementCount == PLY_MAX_ELEMENTS)
				retError(clean, Error_outOfBounds(0, header->elementCount, PLY_MAX_ELEMENTS, "PLY_read() too many elements"));

			if(!Mesh_nextToken(line, len, &pos, &a) || !Mesh_nextToken(line, len, &pos, &b))
				retError(clean, Error_invalidParameter(0, 3, "PLY_read() an element line needs a name and a count"));

			U64 count = 0;

			if(!CharString_parseDec(b, &count) || count > U32_MAX)
				retError(clean, Error_invalidParameter(0, 4, "PLY_read() an element count doesn't parse"));

			current = header->elements + header->elementCount++;
			*current = (PLYElement) { .count = (U32) count };

			if(PLY_tokenIs(a, "vertex")) {
				current->isVertex = true;
				header->vertexCount = (U32) count;
			}

			else if(PLY_tokenIs(a, "face"))
				current->isFace = true;

			continue;
		}

		if(PLY_tokenIs(keyword, "property")) {

			if(!current)
				retError(clean, Error_invalidParameter(0, 5, "PLY_read() a property before any element"));

			if(current->propertyCount == PLY_MAX_PROPERTIES)
				retError(clean, Error_outOfBounds(
					0, current->propertyCount, PLY_MAX_PROPERTIES, "PLY_read() too many properties on one element"
				));

			PLYProperty prop = (PLYProperty) { 0 };

			if(!Mesh_nextToken(line, len, &pos, &a))
				retError(clean, Error_invalidParameter(0, 6, "PLY_read() a property line names no type"));

			if(PLY_tokenIs(a, "list")) {

				if(!Mesh_nextToken(line, len, &pos, &b) || !Mesh_nextToken(line, len, &pos, &c))
					retError(clean, Error_invalidParameter(0, 7, "PLY_read() a list property needs two types"));

				if(!PLY_parseType(b, &prop.countType) || !PLY_parseType(c, &prop.type))
					retError(clean, Error_unsupportedOperation(
						1, "PLY_read() a list property uses a type the specification doesn't name"
					));

				if(prop.countType >= EPLYType_Float)
					retError(clean, Error_invalidParameter(0, 8, "PLY_read() a list's count has to be an integer"));

				prop.isList = true;

				if(!Mesh_nextToken(line, len, &pos, &a))
					retError(clean, Error_invalidParameter(0, 9, "PLY_read() a list property has no name"));

				//The first list on the face element is its corners, whatever it is called, and those have to be
				// integers. A later list, per face uvs say, can hold anything and is read and dropped.

				Bool hasIndices = false;

				for(U8 i = 0; i < current->propertyCount; ++i)
					hasIndices |= current->properties[i].semantic == EPLYSemantic_Indices;

				if(current->isFace && !hasIndices) {

					if(prop.type >= EPLYType_Float)
						retError(clean, Error_invalidParameter(0, 19, "PLY_read() a face's corner list has to be integers"));

					prop.semantic = EPLYSemantic_Indices;
				}
			}

			else {

				if(!PLY_parseType(a, &prop.type))
					retError(clean, Error_unsupportedOperation(
						2, "PLY_read() a property uses a type the specification doesn't name"
					));

				if(!Mesh_nextToken(line, len, &pos, &a))
					retError(clean, Error_invalidParameter(0, 10, "PLY_read() a property has no name"));

				if(current->isVertex)
					prop.semantic = PLY_vertexSemantic(a, &sawU, &sawV);
			}

			current->properties[current->propertyCount++] = prop;
			continue;
		}

		retError(clean, Error_invalidParameter(
			0, 11, "PLY_read() a header line starts with a keyword the specification doesn't name"
		));
	}

	if(!sawFormat || !sawEnd)
		retError(clean, Error_invalidParameter(0, 12, "PLY_read() the header lacks a format line"));

	//A vertex element with all of x, y and z, before any face: faces name vertices, so the vertices have to exist
	// by the time a face is read.

	Bool sawVertex = false, sawXYZ[3] = { false, false, false };

	for(U8 e = 0; e < header->elementCount; ++e) {

		const PLYElement *el = header->elements + e;

		if(el->isFace && !sawVertex)
			retError(clean, Error_invalidParameter(0, 13, "PLY_read() the face element comes before the vertex element"));

		if(!el->isVertex)
			continue;

		sawVertex = true;

		for(U8 p = 0; p < el->propertyCount; ++p) {

			const U8 s = el->properties[p].semantic;

			if(s >= EPLYSemantic_X && s <= EPLYSemantic_Z)
				sawXYZ[s - EPLYSemantic_X] = true;

			if(s >= EPLYSemantic_NX && s <= EPLYSemantic_NZ)
				info->hasNormals = true;

			if(s == EPLYSemantic_U || s == EPLYSemantic_V)
				info->hasUvs = true;
		}
	}

	if(!sawVertex || !sawXYZ[0] || !sawXYZ[1] || !sawXYZ[2])
		retError(clean, Error_invalidParameter(0, 14, "PLY_read() the file has no vertex element with x, y and z"));

clean:
	return s_uccess;
}

//One scalar off the body, as a double since that holds every type the format has.
//Binary values are assembled from bytes in the file's own order rather than loaded and swapped,
// so the host's byte order never enters into it.

static Bool PLY_readScalar(MeshSource *src, U8 type, U8 format, F64 *result, Error *e_rr) {

	Bool s_uccess = true;

	const U8 size = PLY_typeSize[type];
	U8 bytes[8];

	gotoIfError3(clean, MeshSource_readBytes(src, bytes, size, e_rr));

	U64 raw = 0;

	for(U8 i = 0; i < size; ++i) {
		const U8 b = format == EPLYFormat_BigEndian ? bytes[i] : bytes[size - 1 - i];
		raw = (raw << 8) | b;
	}

	switch(type) {

		case EPLYType_Char: *result = (F64) (I8) raw; break;
		case EPLYType_UChar: *result = (F64) (U8) raw; break;
		case EPLYType_Short: *result = (F64) (I16) raw; break;
		case EPLYType_UShort: *result = (F64) (U16) raw; break;
		case EPLYType_Int: *result = (F64) (I32) raw; break;
		case EPLYType_UInt: *result = (F64) (U32) raw; break;
		case EPLYType_Float: *result = (F64) F32_fromU32Bits((U32) raw); break;
		default: *result = F64_fromU64Bits(raw); break;
	}

clean:
	return s_uccess;
}

//Text and binary meet here: a property's values come out as doubles either way, so the element loop below
// has one body. For ascii the line was already read and pos walks its tokens.

typedef struct PLYCursor {
	MeshSource *src;
	U8 format;
	const C8 *line;
	U64 len, pos;
} PLYCursor;

static Bool PLYCursor_value(PLYCursor *c, U8 type, F64 *result, Error *e_rr) {

	Bool s_uccess = true;

	if(c->format != EPLYFormat_Ascii) {
		gotoIfError3(clean, PLY_readScalar(c->src, type, c->format, result, e_rr));
		goto clean;
	}

	CharString token;

	if(!Mesh_nextToken(c->line, c->len, &c->pos, &token))
		retError(clean, Error_invalidParameter(0, 15, "PLY_read() a line has fewer values than the header declares"));

	if(type >= EPLYType_Float) {

		F32 f = 0;

		if(!Mesh_parseF32(token, &f))
			retError(clean, Error_invalidParameter(0, 16, "PLY_read() a value doesn't parse as a number"));

		*result = f;
	}

	else {

		I64 i = 0;

		if(!Mesh_parseI64(token, &i))
			retError(clean, Error_invalidParameter(0, 17, "PLY_read() a value doesn't parse as an integer"));

		*result = (F64) i;
	}

clean:
	return s_uccess;
}

Bool PLY_read(
	StreamRef *stream,
	U64 *off,
	EMeshReadFlags flags,
	MeshInfo *info,
	const MeshOutput *output,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	MeshSource src = (MeshSource) { 0 };
	MeshSink positionSink = (MeshSink) { 0 }, attributeSink = (MeshSink) { 0 };
	MeshSink indexSink = (MeshSink) { 0 }, wordSink = (MeshSink) { 0 };
	MeshPositions positions = (MeshPositions) { 0 };
	MeshTriangles triangles = (MeshTriangles) { 0 };
	MeshAttributes attrs = (MeshAttributes) { 0 };

	//Held only when a triangle word or a smooth normal will need them, since a PLY vertex otherwise leaves as
	// soon as it is read and nothing has to look it up again.

	ListF32 held = (ListF32) { 0 };
	ListU32 face = (ListU32) { 0 };

	Buffer lineBuf = Buffer_createNull();
	Buffer headerBuf = Buffer_createNull();

	if(!stream || !off || !info || !output)
		retError(clean, Error_nullPointer(0, "PLY_read()::stream, off, info and output are required"));

	if(!output->positions || !output->indices)
		retError(clean, Error_nullPointer(4, "PLY_read()::output->positions and indices are required"));

	*info = (MeshInfo) { 0 };

	gotoIfError3(clean, MeshSource_create(stream, *off, alloc, &src, e_rr));
	gotoIfError3(clean, MeshSink_create(output->positions, output->positionOffset, alloc, &positionSink, e_rr));
	gotoIfError3(clean, MeshSink_create(output->attributes, output->attributeOffset, alloc, &attributeSink, e_rr));
	gotoIfError3(clean, MeshSink_create(output->indices, output->indexOffset, alloc, &indexSink, e_rr));
	gotoIfError3(clean, MeshSink_create(output->triangles, output->triangleOffset, alloc, &wordSink, e_rr));

	MeshPositions_create(flags, &positionSink, &positions);
	MeshTriangles_create(flags, &indexSink, &wordSink, &triangles);
	MeshAttributes_create(flags, &attributeSink, &attrs);

	gotoIfError3(clean, Buffer_createUninitializedBytes(MESH_LINE_CAP, alloc, &lineBuf, e_rr));
	C8 *line = (C8*) lineBuf.ptrNonConst;

	//The header is a few KiB of tables; off the stack so a deep stack is never needed for it.

	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(PLYHeader), alloc, &headerBuf, e_rr));
	PLYHeader *header = (PLYHeader*) headerBuf.ptrNonConst;

	gotoIfError3(clean, PLY_readHeader(&src, line, header, info, e_rr));

	const Bool keepPositions = MeshTriangles_needsPositions(&triangles);

	if(keepPositions)
		gotoIfError3(clean, ListF32_reserve(&held, (U64) header->vertexCount * 3, alloc, e_rr));

	PLYCursor cursor = (PLYCursor) { .src = &src, .format = header->format, .line = line };
	U32 verticesRead = 0;

	for(U8 e = 0; e < header->elementCount; ++e) {

		const PLYElement *el = header->elements + e;

		for(U32 elId = 0; elId < el->count; ++elId) {

			if(header->format == EPLYFormat_Ascii) {

				Bool got = false;
				gotoIfError3(clean, MeshSource_readLine(&src, line, MESH_LINE_CAP, &cursor.len, &got, e_rr));

				if(!got)
					retError(clean, Error_outOfBounds(
						0, elId, el->count, "PLY_read() the body ends before the header's element counts do"
					));

				cursor.pos = 0;
			}

			F32 p[3] = { 0, 0, 0 }, n[3] = { 0, 0, 0 }, uv[2] = { 0, 0 };

			for(U8 pi = 0; pi < el->propertyCount; ++pi) {

				const PLYProperty *prop = el->properties + pi;

				if(!prop->isList) {

					F64 value = 0;
					gotoIfError3(clean, PLYCursor_value(&cursor, prop->type, &value, e_rr));

					switch(prop->semantic) {
						case EPLYSemantic_X: p[0] = (F32) value; break;
						case EPLYSemantic_Y: p[1] = (F32) value; break;
						case EPLYSemantic_Z: p[2] = (F32) value; break;
						case EPLYSemantic_NX: n[0] = (F32) value; break;
						case EPLYSemantic_NY: n[1] = (F32) value; break;
						case EPLYSemantic_NZ: n[2] = (F32) value; break;
						case EPLYSemantic_U: uv[0] = (F32) value; break;
						case EPLYSemantic_V: uv[1] = (F32) value; break;
						default: break;
					}

					continue;
				}

				F64 countValue = 0;
				gotoIfError3(clean, PLYCursor_value(&cursor, prop->countType, &countValue, e_rr));

				const U64 count = (U64) countValue;

				if(prop->semantic != EPLYSemantic_Indices) {

					//A list this reader has no use for, per face uvs for one, is read and dropped item by item.
					//Binary could skip by size, but the text form can't, and one loop serves both.

					for(U64 i = 0; i < count; ++i) {
						F64 dropped = 0;
						gotoIfError3(clean, PLYCursor_value(&cursor, prop->type, &dropped, e_rr));
					}

					continue;
				}

				if(count < 3)
					retError(clean, Error_invalidParameter(0, 18, "PLY_read() a face has fewer than three corners"));

				gotoIfError3(clean, ListU32_clear(&face, e_rr));

				for(U64 i = 0; i < count; ++i) {

					F64 indexValue = 0;
					gotoIfError3(clean, PLYCursor_value(&cursor, prop->type, &indexValue, e_rr));

					if(indexValue < 0 || indexValue >= (F64) verticesRead)
						retError(clean, Error_outOfBounds(
							0, (U64) indexValue, verticesRead, "PLY_read() a face names a vertex that doesn't exist"
						));

					gotoIfError3(clean, ListU32_pushBack(&face, (U32) indexValue, alloc, e_rr));
				}

				if(count > 3)
					++info->fannedFaces;

				for(U64 k = 1; k + 1 < face.length; ++k) {

					const U32 i0 = face.ptr[0], i1 = face.ptr[k], i2 = face.ptr[k + 1];

					const F32 *p0 = keepPositions ? held.ptr + (U64) i0 * 3 : NULL;
					const F32 *p1 = keepPositions ? held.ptr + (U64) i1 * 3 : NULL;
					const F32 *p2 = keepPositions ? held.ptr + (U64) i2 * 3 : NULL;

					gotoIfError3(clean, MeshTriangles_emit(&triangles, i0, i1, i2, 0, p0, p1, p2, alloc, e_rr));
				}
			}

			if(!el->isVertex)
				continue;

			gotoIfError3(clean, MeshPositions_push(&positions, p, alloc, e_rr));
			gotoIfError3(clean, MeshAttributes_push(&attrs, n, uv, alloc, e_rr));

			if(keepPositions)
				for(U8 i = 0; i < 3; ++i)
					gotoIfError3(clean, ListF32_pushBack(&held, p[i], alloc, e_rr));

			++verticesRead;
		}
	}

	gotoIfError3(clean, MeshPositions_finish(&positions, info, e_rr));
	gotoIfError3(clean, MeshAttributes_finish(&attrs, &triangles, verticesRead, e_rr));

	gotoIfError3(clean, MeshSink_flush(&positionSink, e_rr));
	gotoIfError3(clean, MeshSink_flush(&attributeSink, e_rr));
	gotoIfError3(clean, MeshSink_flush(&indexSink, e_rr));
	gotoIfError3(clean, MeshSink_flush(&wordSink, e_rr));

	info->vertexCount = verticesRead;
	info->indexCount = triangles.count * 3;
	info->materialCount = 1;

	*off = MeshSource_offset(&src);

clean:

	ListF32_free(&held, alloc);
	ListU32_free(&face, alloc);
	MeshAttributes_free(&attrs, alloc);
	MeshTriangles_free(&triangles, alloc);
	MeshPositions_free(&positions, alloc);
	MeshSink_free(&positionSink);
	MeshSink_free(&attributeSink);
	MeshSink_free(&indexSink);
	MeshSink_free(&wordSink);
	MeshSource_free(&src);
	Buffer_free(&lineBuf, alloc);
	Buffer_free(&headerBuf, alloc);

	return s_uccess;
}
