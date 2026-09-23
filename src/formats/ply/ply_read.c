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

//An element whose properties are all fixed width has a fixed record, so a whole record comes off the stream in
//one read and its properties decode out of it by offset. stride is 0 when the element declares a list, whose
// width is not known until the count itself is read, or when the record is wider than the buffer.

#define PLY_RECORD_CAP 256

//A face with more corners than this falls to the general path, which has a resizable list for it.

#define PLY_FAN_CAP 32

typedef struct PlyElement {
	U32 count;
	U32 firstProperty;            //Into the header's flat property table
	U32 propertyCount;
	U16 stride;                   //0 when the record is not fixed width
	Bool isVertex, isFace;
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

static Bool Ply_parseHeader(
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

//One scalar out of bytes already in hand, as a double since that holds every type the format has.
//Assembled from the bytes in the FILE's order rather than loaded and swapped, so the host's byte order never
// enters into it. Unrolled per width because the width is not a constant here and a byte at a time loop
// around a branch is most of what reading a binary body used to cost.

static F64 Ply_decodeScalar(const U8 *bytes, U8 type, U8 format) {

	const Bool be = format == EPlyFormat_BigEndian;
	U64 raw = 0;

	switch(Ply_typeSize[type]) {

		case 1:
			raw = bytes[0];
			break;

		case 2:
			raw = be
				? ((U64) bytes[0] << 8) | bytes[1]
				: ((U64) bytes[1] << 8) | bytes[0];
			break;

		case 4:
			raw = be
				? ((U64) bytes[0] << 24) | ((U64) bytes[1] << 16) | ((U64) bytes[2] << 8) | bytes[3]
				: ((U64) bytes[3] << 24) | ((U64) bytes[2] << 16) | ((U64) bytes[1] << 8) | bytes[0];
			break;

		default:

			for(U8 i = 0; i < 8; ++i)
				raw = (raw << 8) | bytes[be ? i : 7 - i];

			break;
	}

	switch(type) {

		case EPlyType_Char: return (F64) (I8) raw;
		case EPlyType_UChar: return (F64) (U8) raw;
		case EPlyType_Short: return (F64) (I16) raw;
		case EPlyType_UShort: return (F64) (U16) raw;
		case EPlyType_Int: return (F64) (I32) raw;
		case EPlyType_UInt: return (F64) (U32) raw;
		case EPlyType_Float: return (F64) F32_fromU32Bits((U32) raw);
		default: return F64_fromU64Bits(raw);
	}
}

static Bool Ply_readScalar(MeshSource *src, U8 type, U8 format, F64 *result, Error *e_rr) {

	Bool s_uccess = true;
	U8 bytes[8];

	gotoIfError3(clean, MeshSource_readBytes(src, bytes, Ply_typeSize[type], e_rr));
	*result = Ply_decodeScalar(bytes, type, format);

clean:
	return s_uccess;
}

//Text and binary meet here: a property's values come out as doubles either way, so the element loop below
// has one body. For ascii the line was already read and pos walks its tokens.

//What one property of a fixed width record contributes, worked out once for the element rather than dispatched
//per scalar. The destination is a pointer into the caller's own locals, so decoding a record is a walk down
//this table with no switch in it at all.

typedef struct PlyPlanEntry {
	U16 offset;                   //Bytes into the record
	U8 type;                      //EPlyType
	U8 padding;
	F32 *dst;
} PlyPlanEntry;

typedef struct PlyCursor {
	MeshSource *src;
	U8 format;
	U8 padding[7];
	CharString line;
	U64 pos;
	const U8 *rec;                //When set, binary values come out of this record rather than off the stream
	U64 recPos;
} PlyCursor;

static Bool PlyCursor_value(PlyCursor *c, U8 type, F64 *result, Error *e_rr) {

	Bool s_uccess = true;

	if(c->format != EPlyFormat_Ascii) {

		if(c->rec) {
			*result = Ply_decodeScalar(c->rec + c->recPos, type, c->format);
			c->recPos += Ply_typeSize[type];
			goto clean;
		}

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

//A fixed width record is the common binary case and the only one worth a bulk read, so the width is worked
//out once per element rather than per record. Zero where the element declares a list, whose width is not known
// until the count itself is read, or where the record is wider than the buffer that would hold it.

static void Ply_resolveStrides(PlyHeader *header) {

	for(U64 e = 0; e < header->elements.length; ++e) {

		PlyElement *el = header->elements.ptrNonConst + e;
		U64 stride = 0;

		for(U32 pi = 0; pi < el->propertyCount && stride <= PLY_RECORD_CAP; ++pi) {

			const PlyProperty *prop = header->properties.ptr + el->firstProperty + pi;

			if(prop->isList) {
				stride = 0;
				break;
			}

			stride += Ply_typeSize[prop->type];
		}

		el->stride = (U16) (stride && stride <= PLY_RECORD_CAP ? stride : 0);
	}
}

//EPlyType and EMeshScalarType name the same eight scalars in the same order, so the map is a cast. Spelled as
// a table anyway, because two enums agreeing by coincidence is exactly what a later edit breaks silently.

static const U8 Ply_toScalarType[EPlyType_Count] = {
	EMeshScalarType_I8,  EMeshScalarType_U8,
	EMeshScalarType_I16, EMeshScalarType_U16,
	EMeshScalarType_I32, EMeshScalarType_U32,
	EMeshScalarType_F32, EMeshScalarType_F64
};

static const U8 Ply_fromScalarType[EMeshScalarType_Count] = {
	EPlyType_Char,  EPlyType_UChar,
	EPlyType_Short, EPlyType_UShort,
	EPlyType_Int,   EPlyType_UInt,
	EPlyType_Float, EPlyType_Double
};

//What the PLY header says, as the format independent description in types/mesh. Only the properties a reader
//has a use for get a row: a gaussian splat declares 62 of them and 54 are spherical harmonics nothing here
// reads, which cost only the stride they occupy.

static Bool Ply_fillMeshHeader(const PlyHeader *header, U64 bodyOffset, MeshHeader *out, Error *e_rr) {

	Bool s_uccess = true;

	*out = (MeshHeader) { .bodyOffset = bodyOffset, .bigEndian = header->format == EPlyFormat_BigEndian };

	//Walked in DECLARATION order with the offset accumulating, because that is the order the body is in: an
	//element of unknown width makes every offset after it unknown too, which is what `at` going stale means.

	U64 at = bodyOffset;
	Bool offsetKnown = header->format != EPlyFormat_Ascii;

	for(U64 e = 0; e < header->elements.length; ++e) {

		const PlyElement *el = header->elements.ptr + e;

		if(el->isFace) {

			out->faceCount = el->count;

			//One list property is the shape a face has to be for any of this to mean anything.

			if(offsetKnown && el->propertyCount == 1) {

				const PlyProperty *prop = header->properties.ptr + el->firstProperty;

				if(prop->isList && prop->semantic == EPlySemantic_Indices) {
					out->faceOffset = at;
					out->faceCountType = Ply_toScalarType[prop->countType];
					out->faceIndexType = Ply_toScalarType[prop->type];
				}
			}

			//A face element's width depends on every count in it, so nothing after it can be addressed.

			offsetKnown = false;
			continue;
		}

		if(!el->isVertex) {

			//Skipped, but its bytes still shift everything after it.

			if(!el->stride)
				offsetKnown = false;

			else at += (U64) el->count * el->stride;

			continue;
		}

		out->vertexCount = el->count;

		//Ascii has no record to seek to, whatever the properties say, so the stride stays zero there.

		out->vertexStride = header->format == EPlyFormat_Ascii ? 0 : el->stride;

		if(out->vertexStride)
			at += (U64) el->count * out->vertexStride;

		else offsetKnown = false;

		U16 fieldAt = 0;

		for(U32 pi = 0; pi < el->propertyCount; ++pi) {

			const PlyProperty *prop = header->properties.ptr + el->firstProperty + pi;
			U8 semantic = EMeshVertexSemantic_Count, component = 0;

			switch(prop->semantic) {

				case EPlySemantic_X: case EPlySemantic_Y: case EPlySemantic_Z:
					semantic = EMeshVertexSemantic_Position;
					component = (U8) (prop->semantic - EPlySemantic_X);
					break;

				case EPlySemantic_NX: case EPlySemantic_NY: case EPlySemantic_NZ:
					semantic = EMeshVertexSemantic_Normal;
					component = (U8) (prop->semantic - EPlySemantic_NX);
					break;

				case EPlySemantic_U: case EPlySemantic_V:
					semantic = EMeshVertexSemantic_Uv0;
					component = (U8) (prop->semantic - EPlySemantic_U);
					break;

				default: break;
			}

			if(semantic != EMeshVertexSemantic_Count) {

				if(out->planCount >= MeshHeader_maxPlan)
					retError(clean, Error_outOfBounds(
						0, out->planCount, MeshHeader_maxPlan, "Ply_readHeader() more wanted properties than the plan holds"
					));

				out->plan[out->planCount++] = (MeshPlanEntry) {
					.semantic = semantic,
					.component = component,
					.type = Ply_toScalarType[prop->type],
					.offset = fieldAt
				};
			}

			fieldAt = (U16) (fieldAt + Ply_typeSize[prop->type]);
		}
	}

clean:
	return s_uccess;
}

//---------------------------------------------------------------- Header and range

Bool Ply_readHeader(
	StreamRef *stream, U64 *off, MeshInfo *info, MeshHeader *header, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	MeshSource src = (MeshSource) { 0 };
	Buffer lineBuf = Buffer_createNull();
	PlyHeader headerData = (PlyHeader) { 0 };

	if(!stream || !off || !info || !header)
		retError(clean, Error_nullPointer(0, "Ply_readHeader()::stream, off, info and header are required"));

	*info = (MeshInfo) { 0 };
	*header = (MeshHeader) { 0 };

	gotoIfError3(clean, MeshSource_create(stream, *off, alloc, &src, e_rr));
	gotoIfError3(clean, Buffer_createUninitializedBytes(MESH_LINE_CAP, alloc, &lineBuf, e_rr));

	gotoIfError3(clean, Ply_parseHeader(&src, (C8*) lineBuf.ptrNonConst, &headerData, info, alloc, e_rr));

	Ply_resolveStrides(&headerData);
	gotoIfError3(clean, Ply_fillMeshHeader(&headerData, MeshSource_offset(&src), header, e_rr));

	info->allNormals = info->hasNormals;
	info->allUvs = info->hasUvs;

	*off = header->bodyOffset;

clean:
	ListPlyElement_free(&headerData.elements, alloc);
	ListPlyProperty_free(&headerData.properties, alloc);
	Buffer_free(&lineBuf, alloc);
	MeshSource_free(&src);
	return s_uccess;
}

//One span of the VERTEX element, decoded through the plan the header resolved. Faces are not spanned: a face
//names arbitrary vertices, so a range of them is only meaningful once every vertex it could name is in hand,
// which is the whole file.
//
//MERGES into info rather than overwriting it, so a caller zeroes it before the first span and reads the
// totals after the last. vertexCount is what says whether a bound has been seen yet.
//
//ComputeNormals and QuantizePositions are refused: a normal sums over faces this span cannot see, and a
// quantized position needs bounds no span knows before the last one is read. Derive both afterwards.

Bool Ply_readRange(
	StreamRef *stream,
	const MeshHeader *header,
	EMeshFlags flags,
	U32 firstVertex,
	U32 vertexCount,
	MeshInfo *info,
	const MeshOutput *output,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	MeshSource src = (MeshSource) { 0 };
	MeshSink positionSink = (MeshSink) { 0 }, attributeSink = (MeshSink) { 0 };
	MeshPositions positions = (MeshPositions) { 0 };
	MeshAttributes attrs = (MeshAttributes) { 0 };

	U8 recBuf[PLY_RECORD_CAP];

	if(!stream || !header || !info || !output)
		retError(clean, Error_nullPointer(0, "Ply_readRange()::stream, header, info and output are required"));

	if(!output->positions)
		retError(clean, Error_nullPointer(6, "Ply_readRange()::output->positions is required"));

	if(!header->vertexStride)
		retError(clean, Error_invalidState(
			0, "Ply_readRange() the body is not fixed stride, so no record can be seeked to"
		));

	if(flags & (EMeshFlags_ComputeNormals | EMeshFlags_QuantizePositions))
		retError(clean, Error_unsupportedOperation(
			2, "Ply_readRange() ComputeNormals and QuantizePositions both need the whole mesh"
		));

	if((U64) firstVertex + vertexCount > header->vertexCount)
		retError(clean, Error_outOfBounds(
			3, (U64) firstVertex + vertexCount, header->vertexCount, "Ply_readRange() span past the vertex count"
		));

	gotoIfError3(clean, MeshSource_create(
		stream, header->bodyOffset + (U64) firstVertex * header->vertexStride, alloc, &src, e_rr
	));

	gotoIfError3(clean, MeshSink_create(output->positions, output->positionOffset, alloc, &positionSink, e_rr));
	gotoIfError3(clean, MeshSink_create(output->attributes, output->attributeOffset, alloc, &attributeSink, e_rr));

	MeshPositions_create(flags, &positionSink, &positions);
	MeshAttributes_create(flags, &attributeSink, &attrs);

	const U8 format = header->bigEndian ? EPlyFormat_BigEndian : EPlyFormat_LittleEndian;

	for(U32 v = 0; v < vertexCount; ++v) {

		//In the window where the whole record sits there, copied across the boundary where it does not, which
		// is once a window rather than once a record.

		U64 avail = 0;
		gotoIfError3(clean, MeshSource_available(&src, &avail, e_rr));

		const U8 *rec;

		if(avail >= header->vertexStride) {
			rec = MeshSource_ptr(&src);
			MeshSource_advance(&src, header->vertexStride);
		}

		else {
			gotoIfError3(clean, MeshSource_readBytes(&src, recBuf, header->vertexStride, e_rr));
			rec = recBuf;
		}

		F32 p[3] = { 0, 0, 0 }, n[3] = { 0, 0, 0 }, uv[2] = { 0, 0 };
		F32 *const dst[EMeshVertexSemantic_Count] = { p, n, uv };

		for(U8 k = 0; k < header->planCount; ++k) {

			const MeshPlanEntry e = header->plan[k];

			dst[e.semantic][e.component] =
				(F32) Ply_decodeScalar(rec + e.offset, (U8) Ply_fromScalarType[e.type], format);
		}

		gotoIfError3(clean, MeshPositions_push(&positions, p, alloc, e_rr));
		gotoIfError3(clean, MeshAttributes_push(&attrs, n, uv, alloc, e_rr));
	}

	//The span's own bounds, folded into whatever the caller already had.

	MeshInfo span = (MeshInfo) { 0 };
	gotoIfError3(clean, MeshPositions_finish(&positions, &span, e_rr));
	gotoIfError3(clean, MeshAttributes_finish(&attrs, NULL, vertexCount, e_rr));

	for(U8 i = 0; i < 3; ++i) {
		info->aabbMin[i] = !info->vertexCount ? span.aabbMin[i] : F32_min(info->aabbMin[i], span.aabbMin[i]);
		info->aabbMax[i] = !info->vertexCount ? span.aabbMax[i] : F32_max(info->aabbMax[i], span.aabbMax[i]);
	}

	info->vertexCount += vertexCount;
	info->maxUvError = F32_max(info->maxUvError, attrs.maxUvError);

	gotoIfError3(clean, MeshSink_flush(&positionSink, e_rr));
	gotoIfError3(clean, MeshSink_flush(&attributeSink, e_rr));

clean:
	MeshAttributes_free(&attrs, alloc);
	MeshPositions_free(&positions, alloc);
	MeshSink_free(&attributeSink);
	MeshSink_free(&positionSink);
	MeshSource_free(&src);
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

	U8 recBuf[PLY_RECORD_CAP];
	U8 listBuf[PLY_RECORD_CAP];

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

	gotoIfError3(clean, Buffer_createUninitializedBytes(MESH_LINE_CAP, alloc, &lineBuf, e_rr));
	C8 *line = (C8*) lineBuf.ptrNonConst;

	gotoIfError3(clean, Ply_parseHeader(&src, line, &headerData, info, alloc, e_rr));
	PlyHeader *header = &headerData;

	//A PLY property is declared on the ELEMENT, so either every vertex carries one or none does. Partial
	// coverage, which is what these two exist to report, is an OBJ-only situation.

	info->allNormals = info->hasNormals;
	info->allUvs = info->hasUvs;

	//A header naming normals leaves nothing to compute, and it says so before a single vertex is read. Dropping
	//the flag here is what keeps a consumer from having to read the file twice to find out: it can ask for
	// computed normals unconditionally and a file that has them pays neither the holding nor the sums.
	//The three below are created from this rather than from the caller's flags for that reason.

	const EMeshFlags bodyFlags = info->hasNormals
		? (EMeshFlags) (flags & ~(EMeshFlags) EMeshFlags_ComputeNormals)
		: flags;

	MeshPositions_create(bodyFlags, &positionSink, &positions);
	MeshTriangles_create(bodyFlags, &indexSink, &wordSink, &triangles);
	MeshAttributes_create(bodyFlags, &attributeSink, &attrs);

	Ply_resolveStrides(header);

	const Bool keepPositions = MeshTriangles_needsPositions(&triangles);

	if(keepPositions)
		gotoIfError3(clean, ListF32_reserve(&held, (U64) header->vertexCount * 3, alloc, e_rr));

	PlyCursor cursor = (PlyCursor) { .src = &src, .format = header->format };
	U32 verticesRead = 0;

	for(U64 e = 0; e < header->elements.length; ++e) {

		const PlyElement *el = header->elements.ptr + e;

		//A fixed width binary vertex record decodes IN PLACE out of the cursor's window, with the properties
		//this reader wants resolved once for the element. Only a record straddling the window's end is copied,
		// and everything this does not cover falls to the loop below, which starts wherever this left off.

		U32 done = 0;

		if(header->format != EPlyFormat_Ascii && el->stride && el->isVertex) {

			F32 p[3] = { 0, 0, 0 }, n[3] = { 0, 0, 0 }, uv[2] = { 0, 0 };

			PlyPlanEntry plan[8];
			U8 planCount = 0;
			U16 at = 0;

			for(U32 pi = 0; pi < el->propertyCount; ++pi) {

				const PlyProperty *prop = header->properties.ptr + el->firstProperty + pi;
				F32 *dst = NULL;

				switch(prop->semantic) {
					case EPlySemantic_X: dst = p + 0; break;
					case EPlySemantic_Y: dst = p + 1; break;
					case EPlySemantic_Z: dst = p + 2; break;
					case EPlySemantic_NX: dst = n + 0; break;
					case EPlySemantic_NY: dst = n + 1; break;
					case EPlySemantic_NZ: dst = n + 2; break;
					case EPlySemantic_U: dst = uv + 0; break;
					case EPlySemantic_V: dst = uv + 1; break;
					default: break;
				}

				if(dst && planCount < (U8)(sizeof(plan) / sizeof(plan[0])))
					plan[planCount++] = (PlyPlanEntry) { .offset = at, .type = prop->type, .dst = dst };

				at = (U16) (at + Ply_typeSize[prop->type]);
			}

			while(done < el->count) {

				U64 avail = 0;
				gotoIfError3(clean, MeshSource_available(&src, &avail, e_rr));

				U64 batch = avail / el->stride;

				if(batch > el->count - done)
					batch = el->count - done;

				const U8 *rec = batch ? MeshSource_ptr(&src) : recBuf;

				if(!batch) {
					gotoIfError3(clean, MeshSource_readBytes(&src, recBuf, el->stride, e_rr));
					batch = 1;
				}

				else MeshSource_advance(&src, batch * el->stride);

				for(U64 r = 0; r < batch; ++r, rec += el->stride) {

					for(U8 k = 0; k < planCount; ++k)
						*plan[k].dst = (F32) Ply_decodeScalar(rec + plan[k].offset, plan[k].type, header->format);

					gotoIfError3(clean, MeshPositions_push(&positions, p, alloc, e_rr));
					gotoIfError3(clean, MeshAttributes_push(&attrs, n, uv, alloc, e_rr));

					if(keepPositions)
						gotoIfError3(clean, Mesh_appendF32(&held, p, 3, alloc, e_rr));
				}

				done += (U32) batch;
				verticesRead += (U32) batch;
			}
		}

		//The face element's width is not fixed, since its list's length is the first thing in the record, but a
		//record whose corner count has been read IS of known width, so it too decodes in place. A count this
		// does not cover stops it and the loop below reports whatever was wrong.

		else if(header->format != EPlyFormat_Ascii && el->isFace && el->propertyCount == 1) {

			const PlyProperty *prop = header->properties.ptr + el->firstProperty;

			const U8 countSize = Ply_typeSize[prop->countType];
			const U8 valueSize = Ply_typeSize[prop->type];

			U32 idx[PLY_FAN_CAP];

			while(prop->isList && prop->semantic == EPlySemantic_Indices && done < el->count) {

				U64 avail = 0;
				gotoIfError3(clean, MeshSource_available(&src, &avail, e_rr));

				//The record is read out of the window when it sits there whole, and copied across the boundary
				//when it does not, which is once a window rather than once a record. Nothing here gives up and
				// hands the element back, since bailing at the first boundary would leave the path unused.

				const U8 *rec = NULL;
				U64 corners = 0;

				if(avail >= countSize) {

					rec = MeshSource_ptr(&src);
					corners = (U64) Ply_decodeScalar(rec, prop->countType, header->format);

					if(corners > PLY_FAN_CAP || avail < (U64) countSize + corners * valueSize)
						rec = NULL;
				}

				if(!rec) {

					//Across a boundary, or a polygon with more corners than the stack array holds. The list the
					//general path uses takes both, and neither is common enough to be worth more than this.

					gotoIfError3(clean, MeshSource_readBytes(&src, recBuf, countSize, e_rr));
					corners = (U64) Ply_decodeScalar(recBuf, prop->countType, header->format);

					if(corners < 3)
						retError(clean, Error_invalidParameter(0, 18, "Ply_read() a face has fewer than three corners"));

					gotoIfError3(clean, ListU32_clear(&face, e_rr));

					for(U64 i = 0; i < corners; ++i) {

						U8 one[8];
						gotoIfError3(clean, MeshSource_readBytes(&src, one, valueSize, e_rr));

						const F64 v = Ply_decodeScalar(one, prop->type, header->format);

						if(v < 0 || v >= (F64) verticesRead)
							retError(clean, Error_outOfBounds(
								0, (U64) v, verticesRead, "Ply_read() a face names a vertex that doesn't exist"
							));

						gotoIfError3(clean, ListU32_pushBack(&face, (U32) v, alloc, e_rr));
					}
				}

				else {

					if(corners < 3)
						retError(clean, Error_invalidParameter(0, 18, "Ply_read() a face has fewer than three corners"));

					for(U64 i = 0; i < corners; ++i) {

						const F64 v = Ply_decodeScalar(rec + countSize + i * valueSize, prop->type, header->format);

						if(v < 0 || v >= (F64) verticesRead)
							retError(clean, Error_outOfBounds(
								0, (U64) v, verticesRead, "Ply_read() a face names a vertex that doesn't exist"
							));

						idx[i] = (U32) v;
					}

					MeshSource_advance(&src, (U64) countSize + corners * valueSize);
				}

				const U32 *corner = rec ? idx : face.ptr;

				if(corners > 3)
					++info->fannedFaces;

				for(U64 k = 1; k + 1 < corners; ++k) {

					const U32 i0 = corner[0], i1 = corner[k], i2 = corner[k + 1];

					const F32 *p0 = keepPositions ? held.ptr + (U64) i0 * 3 : NULL;
					const F32 *p1 = keepPositions ? held.ptr + (U64) i1 * 3 : NULL;
					const F32 *p2 = keepPositions ? held.ptr + (U64) i2 * 3 : NULL;

					gotoIfError3(clean, MeshTriangles_emit(&triangles, i0, i1, i2, 0, p0, p1, p2, alloc, e_rr));
				}

				++done;
			}
		}

		for(U32 elId = done; elId < el->count; ++elId) {

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

			//One read a record rather than one a scalar, which is what the fixed width buys.

			cursor.rec = NULL;
			cursor.recPos = 0;

			if(header->format != EPlyFormat_Ascii && el->stride) {
				gotoIfError3(clean, MeshSource_readBytes(&src, recBuf, el->stride, e_rr));
				cursor.rec = recBuf;
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

				//A binary list's width is known the moment its count is, so its values come off the stream in
				//one read and decode out of the buffer. Ascii keeps the token at a time path, since a text
				// value has no width until it is parsed, and so does a list too long for the buffer.

				const U64 valueSize = Ply_typeSize[prop->type];
				const Bool bulk = header->format != EPlyFormat_Ascii && count <= PLY_RECORD_CAP / valueSize;

				if(bulk)
					gotoIfError3(clean, MeshSource_readBytes(&src, listBuf, count * valueSize, e_rr));

				for(U64 i = 0; i < count; ++i) {

					F64 indexValue = 0;

					if(bulk)
						indexValue = Ply_decodeScalar(listBuf + i * valueSize, prop->type, header->format);

					else gotoIfError3(clean, PlyCursor_value(&cursor, prop->type, &indexValue, e_rr));

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
				gotoIfError3(clean, Mesh_appendF32(&held, p, 3, alloc, e_rr));

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
