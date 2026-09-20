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

//formats/ply/ply_read.c

#include "mesh_internal.h"
#include "formats/ply/ply_file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/list_impl.h"
#include "types/math/type_cast.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/string_read_helper.h"

//The scalar types the specification names, in both spellings, and the width of each.

typedef enum EPlyType {
	EPlyType_Char,  EPlyType_UChar,
	EPlyType_Short, EPlyType_UShort,
	EPlyType_Int,   EPlyType_UInt,
	EPlyType_Float, EPlyType_Double,
	EPlyType_Count
} EPlyType;

static const U8 Ply_typeSize[EPlyType_Count] = { 1, 1, 2, 2, 4, 4, 4, 8 };

static const C8 *const Ply_typeNames[EPlyType_Count][2] = {
	{ "char", "int8" }, { "uchar", "uint8" },
	{ "short", "int16" }, { "ushort", "uint16" },
	{ "int", "int32" }, { "uint", "uint32" },
	{ "float", "float32" }, { "double", "float64" }
};

static Bool Ply_parseType(CharString token, U8 *type) {

	for(U8 i = 0; i < EPlyType_Count; ++i)
		for(U8 j = 0; j < 2; ++j)
			if(CharString_equalsCStringSensitive(&token, Ply_typeNames[i][j])) {
				*type = i;
				return true;
			}

	return false;
}

//What a property means to this reader. Everything it does not recognize is read and dropped.

typedef enum EPlySemantic {
	EPlySemantic_None,
	EPlySemantic_X, EPlySemantic_Y, EPlySemantic_Z,
	EPlySemantic_NX, EPlySemantic_NY, EPlySemantic_NZ,
	EPlySemantic_U, EPlySemantic_V,
	EPlySemantic_Indices
} EPlySemantic;

typedef struct PlyProperty {
	U8 type;                      //EPlyType of the value, or of the list's items
	U8 countType;                 //EPlyType of the list's count, when isList
	U8 semantic;                  //EPlySemantic
	Bool isList;
} PlyProperty;

//Both tables grow, so neither imposes a shape on the file. A gaussian splat's vertex element alone declares 62
// properties: a position, a normal, 3 f_dc, 45 f_rest for spherical harmonics of degree 3, opacity, 3 scale and
// 4 rot, and every one has to be kept to know the widths between the ones that are wanted.
//The caps that remain only refuse absurd input, since a header is attacker supplied and each line of it costs
// memory that the file itself never has to contain.

#define PLY_MAX_PROPERTIES 65536
#define PLY_MAX_ELEMENTS 1024

typedef struct PlyElement {
	U32 count;
	U32 firstProperty;            //Into the header's flat property table
	U32 propertyCount;
	Bool isVertex, isFace;
	U8 padding[2];
} PlyElement;

TList(PlyProperty);
TList(PlyElement);

typedef enum EPlyFormat { EPlyFormat_Ascii, EPlyFormat_LittleEndian, EPlyFormat_BigEndian } EPlyFormat;

typedef struct PlyHeader {
	U8 format;                    //EPlyFormat
	U8 padding[3];
	U32 vertexCount;
	ListPlyElement elements;
	ListPlyProperty properties;   //Every element's properties, back to back
} PlyHeader;

TListImpl(PlyProperty);
TListImpl(PlyElement);

static Bool Ply_tokenIs(CharString token, const C8 *literal) {
	return CharString_equalsCStringSensitive(&token, literal);
}

//Which of a vertex's properties this reader wants. The first uv pair wins, whichever of the three spellings
// it comes in, so a file carrying two sets keeps the one it listed first.

static U8 Ply_vertexSemantic(CharString name, Bool *sawU, Bool *sawV) {

	if(Ply_tokenIs(name, "x")) return EPlySemantic_X;
	if(Ply_tokenIs(name, "y")) return EPlySemantic_Y;
	if(Ply_tokenIs(name, "z")) return EPlySemantic_Z;
	if(Ply_tokenIs(name, "nx")) return EPlySemantic_NX;
	if(Ply_tokenIs(name, "ny")) return EPlySemantic_NY;
	if(Ply_tokenIs(name, "nz")) return EPlySemantic_NZ;

	if(!*sawU && (Ply_tokenIs(name, "s") || Ply_tokenIs(name, "u") || Ply_tokenIs(name, "texture_u"))) {
		*sawU = true;
		return EPlySemantic_U;
	}

	if(!*sawV && (Ply_tokenIs(name, "t") || Ply_tokenIs(name, "v") || Ply_tokenIs(name, "texture_v"))) {
		*sawV = true;
		return EPlySemantic_V;
	}

	return EPlySemantic_None;
}

static Bool Ply_readHeader(
	MeshSource *src, C8 *line, PlyHeader *header, MeshInfo *info, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	U64 len = 0;
	Bool got = false;

	gotoIfError3(clean, MeshSource_readLine(src, line, MESH_LINE_CAP, &len, &got, e_rr));

	if(!got || len != 3 || line[0] != 'p' || line[1] != 'l' || line[2] != 'y')
		retError(clean, Error_invalidParameter(0, 0, "Ply_read() the file doesn't start with ply"));

	Bool sawFormat = false, sawEnd = false;
	Bool sawU = false, sawV = false;
	PlyElement *current = NULL;

	for(;;) {

		gotoIfError3(clean, MeshSource_readLine(src, line, MESH_LINE_CAP, &len, &got, e_rr));

		if(!got)
			retError(clean, Error_invalidParameter(0, 1, "Ply_read() the header never ends"));

		const CharString lineStr = CharString_createRefSizedConst(line, len, false);
		U64 pos = 0;
		CharString keyword, a, b, c;

		if(!CharString_nextToken(lineStr, &pos, &keyword))
			continue;

		if(Ply_tokenIs(keyword, "comment") || Ply_tokenIs(keyword, "obj_info"))
			continue;

		if(Ply_tokenIs(keyword, "end_header")) {
			sawEnd = true;
			break;
		}

		if(Ply_tokenIs(keyword, "format")) {

			if(!CharString_nextToken(lineStr, &pos, &a))
				retError(clean, Error_invalidParameter(0, 2, "Ply_read() the format line names no format"));

			if(Ply_tokenIs(a, "ascii")) header->format = EPlyFormat_Ascii;
			else if(Ply_tokenIs(a, "binary_little_endian")) header->format = EPlyFormat_LittleEndian;
			else if(Ply_tokenIs(a, "binary_big_endian")) header->format = EPlyFormat_BigEndian;
			else retError(clean, Error_unsupportedOperation(0, "Ply_read() the format is not one the specification names"));

			sawFormat = true;
			continue;
		}

		if(Ply_tokenIs(keyword, "element")) {

			if(header->elements.length == PLY_MAX_ELEMENTS)
				retError(clean, Error_outOfBounds(
					0, header->elements.length, PLY_MAX_ELEMENTS, "Ply_read() too many elements"
				));

			if(!CharString_nextToken(lineStr, &pos, &a) || !CharString_nextToken(lineStr, &pos, &b))
				retError(clean, Error_invalidParameter(0, 3, "Ply_read() an element line needs a name and a count"));

			U64 count = 0;

			if(!CharString_parseDec(b, &count) || count > U32_MAX)
				retError(clean, Error_invalidParameter(0, 4, "Ply_read() an element count doesn't parse"));

			//Pushing may move the table, so current is taken after the push and lives only until the next one

			gotoIfError3(clean, ListPlyElement_pushBack(&header->elements, (PlyElement) {
				.count = (U32) count,
				.firstProperty = (U32) header->properties.length
			}, alloc, e_rr));

			current = header->elements.ptrNonConst + header->elements.length - 1;

			if(Ply_tokenIs(a, "vertex")) {
				current->isVertex = true;
				header->vertexCount = (U32) count;
			}

			else if(Ply_tokenIs(a, "face"))
				current->isFace = true;

			continue;
		}

		if(Ply_tokenIs(keyword, "property")) {

			if(!current)
				retError(clean, Error_invalidParameter(0, 5, "Ply_read() a property before any element"));

			if(header->properties.length == PLY_MAX_PROPERTIES)
				retError(clean, Error_outOfBounds(
					0, header->properties.length, PLY_MAX_PROPERTIES, "Ply_read() too many properties"
				));

			PlyProperty prop = (PlyProperty) { 0 };

			if(!CharString_nextToken(lineStr, &pos, &a))
				retError(clean, Error_invalidParameter(0, 6, "Ply_read() a property line names no type"));

			if(Ply_tokenIs(a, "list")) {

				if(!CharString_nextToken(lineStr, &pos, &b) || !CharString_nextToken(lineStr, &pos, &c))
					retError(clean, Error_invalidParameter(0, 7, "Ply_read() a list property needs two types"));

				if(!Ply_parseType(b, &prop.countType) || !Ply_parseType(c, &prop.type))
					retError(clean, Error_unsupportedOperation(
						1, "Ply_read() a list property uses a type the specification doesn't name"
					));

				if(prop.countType >= EPlyType_Float)
					retError(clean, Error_invalidParameter(0, 8, "Ply_read() a list's count has to be an integer"));

				prop.isList = true;

				if(!CharString_nextToken(lineStr, &pos, &a))
					retError(clean, Error_invalidParameter(0, 9, "Ply_read() a list property has no name"));

				//The first list on the face element is its corners, whatever it is called, and those have to be
				// integers. A later list, per face uvs say, can hold anything and is read and dropped.

				Bool hasIndices = false;

				for(U32 i = 0; i < current->propertyCount; ++i)
					hasIndices |= header->properties.ptr[current->firstProperty + i].semantic == EPlySemantic_Indices;

				if(current->isFace && !hasIndices) {

					if(prop.type >= EPlyType_Float)
						retError(clean, Error_invalidParameter(0, 19, "Ply_read() a face's corner list has to be integers"));

					prop.semantic = EPlySemantic_Indices;
				}
			}

			else {

				if(!Ply_parseType(a, &prop.type))
					retError(clean, Error_unsupportedOperation(
						2, "Ply_read() a property uses a type the specification doesn't name"
					));

				if(!CharString_nextToken(lineStr, &pos, &a))
					retError(clean, Error_invalidParameter(0, 10, "Ply_read() a property has no name"));

				if(current->isVertex)
					prop.semantic = Ply_vertexSemantic(a, &sawU, &sawV);
			}

			gotoIfError3(clean, ListPlyProperty_pushBack(&header->properties, prop, alloc, e_rr));
			++current->propertyCount;
			continue;
		}

		retError(clean, Error_invalidParameter(
			0, 11, "Ply_read() a header line starts with a keyword the specification doesn't name"
		));
	}

	if(!sawFormat || !sawEnd)
		retError(clean, Error_invalidParameter(0, 12, "Ply_read() the header lacks a format line"));

	//A vertex element with all of x, y and z, before any face: faces name vertices, so the vertices have to exist
	// by the time a face is read.

	//Bit 0 is a vertex element and bits 1 to 3 its x, y and z, so an axis is one shift off its semantic and the
	// check at the end is a single compare.

	const U8 sawVertex = 1 << 0, sawEverything = 0xF;
	U8 saw = 0;

	for(U64 e = 0; e < header->elements.length; ++e) {

		const PlyElement *el = header->elements.ptr + e;

		if(el->isFace && !(saw & sawVertex))
			retError(clean, Error_invalidParameter(0, 13, "Ply_read() the face element comes before the vertex element"));

		if(!el->isVertex)
			continue;

		saw |= sawVertex;

		for(U32 p = 0; p < el->propertyCount; ++p) {

			const U8 s = header->properties.ptr[el->firstProperty + p].semantic;

			if(s >= EPlySemantic_X && s <= EPlySemantic_Z)
				saw |= (U8) (2 << (s - EPlySemantic_X));

			if(s >= EPlySemantic_NX && s <= EPlySemantic_NZ)
				info->hasNormals = true;

			if(s == EPlySemantic_U || s == EPlySemantic_V)
				info->hasUvs = true;
		}
	}

	if(saw != sawEverything)
		retError(clean, Error_invalidParameter(0, 14, "Ply_read() the file has no vertex element with x, y and z"));

clean:
	return s_uccess;
}

//One scalar off the body, as a double since that holds every type the format has.
//Binary values are assembled from bytes in the file's own order rather than loaded and swapped,
// so the host's byte order never enters into it.

static Bool Ply_readScalar(MeshSource *src, U8 type, U8 format, F64 *result, Error *e_rr) {

	Bool s_uccess = true;

	const U8 size = Ply_typeSize[type];
	U8 bytes[8];

	gotoIfError3(clean, MeshSource_readBytes(src, bytes, size, e_rr));

	U64 raw = 0;

	for(U8 i = 0; i < size; ++i) {
		const U8 b = format == EPlyFormat_BigEndian ? bytes[i] : bytes[size - 1 - i];
		raw = (raw << 8) | b;
	}

	switch(type) {

		case EPlyType_Char: *result = (F64) (I8) raw; break;
		case EPlyType_UChar: *result = (F64) (U8) raw; break;
		case EPlyType_Short: *result = (F64) (I16) raw; break;
		case EPlyType_UShort: *result = (F64) (U16) raw; break;
		case EPlyType_Int: *result = (F64) (I32) raw; break;
		case EPlyType_UInt: *result = (F64) (U32) raw; break;
		case EPlyType_Float: *result = (F64) F32_fromU32Bits((U32) raw); break;
		default: *result = F64_fromU64Bits(raw); break;
	}

clean:
	return s_uccess;
}

//Text and binary meet here: a property's values come out as doubles either way, so the element loop below
// has one body. For ascii the line was already read and pos walks its tokens.

typedef struct PlyCursor {
	MeshSource *src;
	U8 format;
	U8 padding[7];
	CharString line;
	U64 pos;
} PlyCursor;

static Bool PlyCursor_value(PlyCursor *c, U8 type, F64 *result, Error *e_rr) {

	Bool s_uccess = true;

	if(c->format != EPlyFormat_Ascii) {
		gotoIfError3(clean, Ply_readScalar(c->src, type, c->format, result, e_rr));
		goto clean;
	}

	CharString token;

	if(!CharString_nextToken(c->line, &c->pos, &token))
		retError(clean, Error_invalidParameter(0, 15, "Ply_read() a line has fewer values than the header declares"));

	if(type >= EPlyType_Float) {

		F32 f = 0;

		if(!CharString_parseFloat(token, &f))
			retError(clean, Error_invalidParameter(0, 16, "Ply_read() a value doesn't parse as a number"));

		*result = f;
	}

	else {

		I64 i = 0;

		if(!CharString_parseDecSigned(token, &i))
			retError(clean, Error_invalidParameter(0, 17, "Ply_read() a value doesn't parse as an integer"));

		*result = (F64) i;
	}

clean:
	return s_uccess;
}

Bool Ply_read(
	StreamRef *stream,
	U64 *off,
	EMeshFlags flags,
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
	PlyHeader headerData = (PlyHeader) { 0 };

	if(!stream || !off || !info || !output)
		retError(clean, Error_nullPointer(0, "Ply_read()::stream, off, info and output are required"));

	if(!output->positions || !output->indices)
		retError(clean, Error_nullPointer(4, "Ply_read()::output->positions and indices are required"));

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

	gotoIfError3(clean, Ply_readHeader(&src, line, &headerData, info, alloc, e_rr));
	PlyHeader *header = &headerData;

	//A PLY property is declared on the ELEMENT, so either every vertex carries one or none does. Partial
	// coverage, which is what these two exist to report, is an OBJ-only situation.

	info->allNormals = info->hasNormals;
	info->allUvs = info->hasUvs;

	const Bool keepPositions = MeshTriangles_needsPositions(&triangles);

	if(keepPositions)
		gotoIfError3(clean, ListF32_reserve(&held, (U64) header->vertexCount * 3, alloc, e_rr));

	PlyCursor cursor = (PlyCursor) { .src = &src, .format = header->format };
	U32 verticesRead = 0;

	for(U64 e = 0; e < header->elements.length; ++e) {

		const PlyElement *el = header->elements.ptr + e;

		for(U32 elId = 0; elId < el->count; ++elId) {

			if(header->format == EPlyFormat_Ascii) {

				Bool got = false;
				U64 len = 0;
				gotoIfError3(clean, MeshSource_readLine(&src, line, MESH_LINE_CAP, &len, &got, e_rr));

				if(!got)
					retError(clean, Error_outOfBounds(
						0, elId, el->count, "Ply_read() the body ends before the header's element counts do"
					));

				cursor.pos = 0;
				cursor.line = CharString_createRefSizedConst(line, len, false);
			}

			F32 p[3] = { 0, 0, 0 }, n[3] = { 0, 0, 0 }, uv[2] = { 0, 0 };

			for(U32 pi = 0; pi < el->propertyCount; ++pi) {

				const PlyProperty *prop = header->properties.ptr + el->firstProperty + pi;

				if(!prop->isList) {

					F64 value = 0;
					gotoIfError3(clean, PlyCursor_value(&cursor, prop->type, &value, e_rr));

					switch(prop->semantic) {
						case EPlySemantic_X: p[0] = (F32) value; break;
						case EPlySemantic_Y: p[1] = (F32) value; break;
						case EPlySemantic_Z: p[2] = (F32) value; break;
						case EPlySemantic_NX: n[0] = (F32) value; break;
						case EPlySemantic_NY: n[1] = (F32) value; break;
						case EPlySemantic_NZ: n[2] = (F32) value; break;
						case EPlySemantic_U: uv[0] = (F32) value; break;
						case EPlySemantic_V: uv[1] = (F32) value; break;
						default: break;
					}

					continue;
				}

				F64 countValue = 0;
				gotoIfError3(clean, PlyCursor_value(&cursor, prop->countType, &countValue, e_rr));

				const U64 count = (U64) countValue;

				if(prop->semantic != EPlySemantic_Indices) {

					//A list this reader has no use for, per face uvs for one, is read and dropped item by item.
					//Binary could skip by size, but the text form can't, and one loop serves both.

					for(U64 i = 0; i < count; ++i) {
						F64 dropped = 0;
						gotoIfError3(clean, PlyCursor_value(&cursor, prop->type, &dropped, e_rr));
					}

					continue;
				}

				if(count < 3)
					retError(clean, Error_invalidParameter(0, 18, "Ply_read() a face has fewer than three corners"));

				gotoIfError3(clean, ListU32_clear(&face, e_rr));

				for(U64 i = 0; i < count; ++i) {

					F64 indexValue = 0;
					gotoIfError3(clean, PlyCursor_value(&cursor, prop->type, &indexValue, e_rr));

					if(indexValue < 0 || indexValue >= (F64) verticesRead)
						retError(clean, Error_outOfBounds(
							0, (U64) indexValue, verticesRead, "Ply_read() a face names a vertex that doesn't exist"
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

	//See Obj_read: a computed normal replaces whatever the file named, so only the vertices no triangle with
	// area touched are left holding a placeholder.

	if(flags & EMeshFlags_ComputeNormals)
		info->allNormals = !attrs.placeholderNormal;

	info->maxUvError = attrs.maxUvError;

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
	ListPlyElement_free(&headerData.elements, alloc);
	ListPlyProperty_free(&headerData.properties, alloc);

	return s_uccess;
}
