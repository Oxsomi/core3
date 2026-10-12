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
#include "types/container/job_queue.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/math/type_cast.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
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

//Every count comes from the header, so a reader that sizes buffers from them holds them to the body first.
//The bound is the least a record takes: its fixed properties and only the counts of its lists, or a byte a token.

static Bool Ply_checkCounts(const PlyHeader *header, const MeshSource *src, Error *e_rr) {

	Bool s_uccess = true;
	U64 least = 0;

	for(U64 e = 0; e < header->elements.length; ++e) {

		const PlyElement *el = header->elements.ptr + e;
		U64 record = 0;

		for(U32 p = 0; p < el->propertyCount; ++p) {

			const PlyProperty *prop = header->properties.ptr + el->firstProperty + p;

			if(header->format == EPlyFormat_Ascii)
				++record;

			else record += Ply_typeSize[prop->isList ? prop->countType : prop->type];
		}

		least += (U64) el->count * record;
	}

	if(least > src->size - src->pos)
		retError(clean, Error_invalidParameter(0, 20, "Ply_read() the header declares more than the file holds"));

clean:
	return s_uccess;
}

//One scalar out of bytes already in hand, as a double since that holds every type the format has.
//Assembled from the bytes in the FILE's order rather than loaded and swapped, so the host's byte order never
// enters into it. Unrolled per width because the width is not a constant here, and a byte at a time loop
// around a branch is what a binary body would otherwise pay on every scalar.

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
	F32 *dst;
	U16 offset;                   //Bytes into the record
	U8 type;                      //EPlyType
	U8 padding[5];
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

//A span's bounds, folded into whatever the caller already had.
//Shared by Ply_readRange, which calls this once for the one span it read,
// and by the parallel reader's vertex merge, which calls it once per chunk in order:
// both are "fold this span into the running total" and nothing tells the two apart from inside the fold.
//vertexCount is span's own, passed separately since a caller merging several chunks does not want it confused
// with info->vertexCount, which is the RUNNING total this adds to.

static void Ply_mergeSpanInfo(MeshInfo *info, const MeshInfo *span, U32 vertexCount) {

	//An empty span has no bounds to fold in.

	if(vertexCount && info->vertexCount) {
		info->aabbMin = F32x4_min(info->aabbMin, span->aabbMin);
		info->aabbMax = F32x4_max(info->aabbMax, span->aabbMax);
	}

	else if(vertexCount) {
		info->aabbMin = span->aabbMin;
		info->aabbMax = span->aabbMax;
	}

	info->vertexCount += vertexCount;
	info->maxUvError = F32_max(info->maxUvError, span->maxUvError);
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

	span.maxUvError = attrs.maxUvError;
	Ply_mergeSpanInfo(info, &span, vertexCount);

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

//Decodes faceCount consecutive records of a face element whose only property is a corner index list, fanning
//each polygon into triangles from its first corner. The serial reader calls this once for the whole element;
//a parallel read calls it once per chunk of faces, each on its own MeshSource positioned at that chunk's first
// record, and the two produce the same bytes because they are the same loop over a different range.
//
//heldPositions is read only when keepPositions is set, and then only at 3 floats per vertex index a face
// names, whatever that index is: the caller owns making every vertex a face in this range could name resolve
// into that array, which for a parallel chunk means every vertex has already been decoded, not just its own.

static Bool Ply_decodeFaceFast(
	MeshSource *src,
	U8 countType, U8 indexType, U8 format,
	U32 verticesRead,
	U32 faceCount,
	Bool keepPositions,
	const F32 *heldPositions,
	MeshTriangles *triangles,
	U32 *fannedFaces,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU32 face = (ListU32) { 0 };

	const U8 countSize = Ply_typeSize[countType];
	const U8 valueSize = Ply_typeSize[indexType];

	U8 recBuf[PLY_RECORD_CAP];
	U32 idx[PLY_FAN_CAP];

	for(U32 done = 0; done < faceCount; ++done) {

		U64 avail = 0;
		gotoIfError3(clean, MeshSource_available(src, &avail, e_rr));

		//The record is read out of the window when it sits there whole, and copied across the boundary when it
		//does not, which is once a window rather than once a record. Nothing here gives up and hands the
		// element back, since bailing at the first boundary would leave the path unused.

		const U8 *rec = NULL;
		U64 corners = 0;

		if(avail >= countSize) {

			rec = MeshSource_ptr(src);
			corners = (U64) Ply_decodeScalar(rec, countType, format);

			if(corners > PLY_FAN_CAP || avail < (U64) countSize + corners * valueSize)
				rec = NULL;
		}

		if(!rec) {

			//Across a boundary, or a polygon with more corners than the stack array holds. The list the
			//general path uses takes both, and neither is common enough to be worth more than this.

			gotoIfError3(clean, MeshSource_readBytes(src, recBuf, countSize, e_rr));
			corners = (U64) Ply_decodeScalar(recBuf, countType, format);

			if(corners < 3)
				retError(clean, Error_invalidParameter(0, 18, "Ply_read() a face has fewer than three corners"));

			gotoIfError3(clean, ListU32_clear(&face, e_rr));

			for(U64 i = 0; i < corners; ++i) {

				U8 one[8];
				gotoIfError3(clean, MeshSource_readBytes(src, one, valueSize, e_rr));

				const F64 v = Ply_decodeScalar(one, indexType, format);

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

				const F64 v = Ply_decodeScalar(rec + countSize + i * valueSize, indexType, format);

				if(v < 0 || v >= (F64) verticesRead)
					retError(clean, Error_outOfBounds(
						0, (U64) v, verticesRead, "Ply_read() a face names a vertex that doesn't exist"
					));

				idx[i] = (U32) v;
			}

			MeshSource_advance(src, (U64) countSize + corners * valueSize);
		}

		const U32 *corner = rec ? idx : face.ptr;

		if(corners > 3)
			++*fannedFaces;

		for(U64 k = 1; k + 1 < corners; ++k) {

			const U32 i0 = corner[0], i1 = corner[k], i2 = corner[k + 1];

			const F32 *p0 = keepPositions ? heldPositions + (U64) i0 * 3 : NULL;
			const F32 *p1 = keepPositions ? heldPositions + (U64) i1 * 3 : NULL;
			const F32 *p2 = keepPositions ? heldPositions + (U64) i2 * 3 : NULL;

			gotoIfError3(clean, MeshTriangles_emit(triangles, i0, i1, i2, 0, p0, p1, p2, alloc, e_rr));
		}
	}

clean:
	ListU32_free(&face, alloc);
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
	gotoIfError3(clean, Ply_checkCounts(&headerData, &src, e_rr));
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

			if(prop->isList && prop->semantic == EPlySemantic_Indices) {

				gotoIfError3(clean, Ply_decodeFaceFast(
					&src, prop->countType, prop->type, header->format, verticesRead, el->count,
					keepPositions, keepPositions ? held.ptr : NULL, &triangles, &info->fannedFaces, alloc, e_rr
				));

				done = el->count;
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

//---------------------------------------------------------------- Parallel read
//
//Binary only: an ascii body has no record boundary to split on. A vertex element has to be fixed stride
//(Ply_resolveStrides already says so through MeshHeader::vertexStride) and the face element has to be the one
// shape Ply_fillMeshHeader resolves an offset for, a single corner index list, which is what MeshHeader::
// faceOffset being nonzero already means. Anything else, a gaussian splat's extra lists included, falls back
// to Ply_read whole: those shapes exist, but they are not what a 500 MiB triangle soup looks like.
//
//ComputeNormals and QuantizePositions stay serial for the reason Ply_readRange already refuses them for: a
//normal sums over every face touching a vertex, and a quantized position needs the bounds of the whole mesh,
// neither of which a chunk can know on its own. Ply_read's own single pass already pays for both cheaply by
// holding what it must; splitting that holding across chunks would trade one serial pass for a merge step at
// least as expensive, for a file this reader is not slow on in the first place.

//Below this many combined bytes of vertex and face records, splitting costs more than it saves: a JobGroup
//per phase, a MemoryStream per chunk and the merge back into one MeshInfo are fixed costs a small file pays
//for nothing. Measured generating vertex only binary PLYs from 64 KiB to 128 MiB on the dev box (8 logical
//cores) and timing Ply_read against Ply_readParallel on each: parallel pulls ahead once the body passes a
//handful of MiB, and either side of that point costs low single digit milliseconds, so the exact line matters
// little. 4 MiB keeps every file this reader is actually slow on well inside the parallel side.

#define PLY_PARALLEL_MIN_BYTES (4 * MIBI)

//A few chunks per worker, so one that finishes early takes another rather than the whole read waiting on the
//slowest. Matches HDR_scanAndDecode's split (formats/hdr/hdr_read.c).

static U64 Ply_parallelChunkCount(U64 total, U64 threadCount) {

	const U64 want = U64_min(total, threadCount * 4);
	return want ? want : 1;
}

typedef struct PlyVertexJob {

	StreamRef *stream;
	const MeshHeader *header;
	const Allocator *alloc;
	JobGroup *group;

	StreamRef *positions;        //Owned: a disjoint region of the shared positions buffer
	StreamRef *attributes;       //Owned, or NULL when the caller asked for none
	MeshInfo *info;              //Not inline: a 16 byte aligned member would pad by Error's size, which varies per build

	EMeshFlags flags;
	U32 firstVertex, vertexCount;
	U8 padding[4];

	Error err;

} PlyVertexJob;

//Ply_readRange already produces exactly what Ply_read would for the same span (Test_plyRange checks this
//byte for byte), so a chunk is this and nothing more: the parallelism is entirely in how many of these run
// at once, not in a second decoder that has to be kept agreeing with the first by hand.

static Bool Ply_vertexJob(void *data, U64 threadId, JobQueue *queue) {

	(void) threadId;
	(void) queue;

	PlyVertexJob *job = (PlyVertexJob*) data;

	const MeshOutput output = (MeshOutput) { .positions = job->positions, .attributes = job->attributes };

	const Bool ok = Ply_readRange(
		job->stream, job->header, job->flags, job->firstVertex, job->vertexCount, job->info, &output,
		job->alloc, &job->err
	);

	if(!ok)
		JobGroup_fail(job->group, NULL);

	//The group and job->err carry a failure, so the queue, which may be the caller's, stays successful.

	JobGroup_leave(job->group, NULL);
	return true;
}

typedef struct PlyFaceJob {

	StreamRef *stream;
	StreamRef *indices;           //Owned: a disjoint region of the shared indices buffer
	StreamRef *words;             //Owned, or NULL when the caller asked for no triangle words
	const Allocator *alloc;
	JobGroup *group;
	const F32 *heldPositions;

	U64 byteStart;

	EMeshFlags flags;
	U32 verticesRead, faceCount, fannedFaces;

	U8 countType, indexType, format;
	Bool keepPositions;
	U8 padding[4];

	Error err;

} PlyFaceJob;

static Bool Ply_faceJob(void *data, U64 threadId, JobQueue *queue) {

	(void) threadId;
	(void) queue;

	PlyFaceJob *job = (PlyFaceJob*) data;

	MeshSource src = (MeshSource) { 0 };
	MeshSink indexSink = (MeshSink) { 0 }, wordSink = (MeshSink) { 0 };
	MeshTriangles triangles = (MeshTriangles) { 0 };

	Bool ok =
		MeshSource_create(job->stream, job->byteStart, job->alloc, &src, &job->err) &&
		MeshSink_create(job->indices, 0, job->alloc, &indexSink, &job->err) &&
		MeshSink_create(job->words, 0, job->alloc, &wordSink, &job->err);

	if(ok) {

		MeshTriangles_create(job->flags, &indexSink, job->words ? &wordSink : NULL, &triangles);

		ok = Ply_decodeFaceFast(
			&src, job->countType, job->indexType, job->format, job->verticesRead, job->faceCount,
			job->keepPositions, job->heldPositions, &triangles, &job->fannedFaces, job->alloc, &job->err
		);
	}

	if(ok)
		ok = MeshSink_flush(&indexSink, &job->err) && MeshSink_flush(&wordSink, &job->err);

	MeshTriangles_free(&triangles, job->alloc);
	MeshSink_free(&wordSink);
	MeshSink_free(&indexSink);
	MeshSource_free(&src);

	if(!ok)
		JobGroup_fail(job->group, NULL);

	//The group and job->err carry a failure, so the queue, which may be the caller's, stays successful.

	JobGroup_leave(job->group, NULL);
	return true;
}

//Where each face record of a single corner-index-list element begins, and how many triangles come before it,
//as plain arithmetic when every face declares the same corner count and as a table when it does not.
//corners and stride are only meaningful while uniform holds; the table is only populated otherwise.

typedef struct PlyFaceLayout {

	ListU64 byteOffset;          //faceCount + 1 entries, empty while uniform holds
	ListU64 triPrefix;           //Parallel to byteOffset, empty while uniform holds

	U64 corners, stride;
	U64 totalTriangles;
	Bool uniform;
	U8 padding[7];

} PlyFaceLayout;

static U64 PlyFaceLayout_byteOffset(const PlyFaceLayout *l, U64 faceOffset, U32 i) {
	return l->uniform ? faceOffset + (U64) i * l->stride : l->byteOffset.ptr[i];
}

static U64 PlyFaceLayout_triOffset(const PlyFaceLayout *l, U32 i) {
	return l->uniform ? (U64) i * (l->corners - 2) : l->triPrefix.ptr[i];
}

static U64 PlyFaceLayout_end(const PlyFaceLayout *l, U64 faceOffset, U32 faceCount) {
	return l->uniform ? faceOffset + (U64) faceCount * l->stride : l->byteOffset.ptr[faceCount];
}

static void PlyFaceLayout_free(PlyFaceLayout *l, const Allocator *alloc) {
	ListU64_free(&l->byteOffset, alloc);
	ListU64_free(&l->triPrefix, alloc);
}

//The general case: every face's byte offset and triangle prefix, from a second pass over the counts. Paid
//only once Ply_buildFaceLayout finds the file is not uniform, which real meshes rarely are not.

static Bool Ply_buildFaceLayoutTable(
	StreamRef *stream, U64 faceOffset, U8 countType, U8 indexType, U8 format, U32 faceCount,
	PlyFaceLayout *layout, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	MeshSource src = (MeshSource) { 0 };

	const U8 countSize = Ply_typeSize[countType];
	const U8 valueSize = Ply_typeSize[indexType];

	gotoIfError3(clean, MeshSource_create(stream, faceOffset, alloc, &src, e_rr));
	gotoIfError3(clean, ListU64_resize(&layout->byteOffset, (U64) faceCount + 1, alloc, e_rr));
	gotoIfError3(clean, ListU64_resize(&layout->triPrefix, (U64) faceCount + 1, alloc, e_rr));

	U64 tri = 0;

	for(U32 i = 0; i < faceCount; ++i) {

		layout->byteOffset.ptrNonConst[i] = MeshSource_offset(&src);
		layout->triPrefix.ptrNonConst[i] = tri;

		U8 countBuf[8];
		gotoIfError3(clean, MeshSource_readBytes(&src, countBuf, countSize, e_rr));

		const U64 corners = (U64) Ply_decodeScalar(countBuf, countType, format);

		if(corners < 3)
			retError(clean, Error_invalidParameter(0, 18, "Ply_read() a face has fewer than three corners"));

		tri += corners - 2;
		gotoIfError3(clean, MeshSource_skip(&src, corners * valueSize, e_rr));
	}

	layout->byteOffset.ptrNonConst[faceCount] = MeshSource_offset(&src);
	layout->triPrefix.ptrNonConst[faceCount] = tri;
	layout->totalTriangles = tri;

clean:
	MeshSource_free(&src);
	return s_uccess;
}

//One chunk of the hypothesis Ply_buildFaceLayout tests: every face in [byteStart, byteStart + faceCount
//records of corners0) declares exactly corners0 corners. Only the count field is read and only arithmetic
// says where the next one is, which is what a uniform file lets this skip paying for.
//
//uniform is the answer; err is set only by a genuine stream failure, which the caller treats the same as a
// mismatch. Either way this never fails the group: "the hypothesis does not hold here" is an outcome this
// chunk reports, not a reason to abandon the ones still running.

typedef struct PlyUniformCheckJob {

	StreamRef *stream;
	const Allocator *alloc;
	JobGroup *group;

	U64 byteStart;
	U64 corners0;
	U32 faceCount;
	U8 countType, indexType, format;
	U8 padding;

	Bool uniform;
	U8 padding2[7];
	Error err;

} PlyUniformCheckJob;

static Bool Ply_uniformCheckJob(void *data, U64 threadId, JobQueue *queue) {

	(void) threadId;
	(void) queue;

	PlyUniformCheckJob *job = (PlyUniformCheckJob*) data;
	MeshSource src = (MeshSource) { 0 };

	job->uniform = true;

	Bool ok = MeshSource_create(job->stream, job->byteStart, job->alloc, &src, &job->err);

	const U8 countSize = Ply_typeSize[job->countType];
	const U8 valueSize = Ply_typeSize[job->indexType];

	for(U32 i = 0; ok && job->uniform && i < job->faceCount; ++i) {

		U8 countBuf[8];
		ok = MeshSource_readBytes(&src, countBuf, countSize, &job->err);

		if(!ok)
			break;

		if((U64) Ply_decodeScalar(countBuf, job->countType, job->format) != job->corners0) {
			job->uniform = false;
			break;
		}

		ok = MeshSource_skip(&src, job->corners0 * valueSize, &job->err);
	}

	MeshSource_free(&src);

	if(!ok)
		job->uniform = false;

	JobGroup_leave(job->group, NULL);
	return true;
}

//Where every face record begins, found the fast way when the file lets it: read the first face's corner
//count once, then check IN PARALLEL that every other face agrees with it. The chunk that holds the first
//face that actually disagrees always starts from a byte position every chunk before it already confirmed, so
// it alone is enough to catch a real mismatch correctly; a chunk after it may by then be reading from the
// wrong place entirely, which is why ANY chunk saying no, a stream error included, is treated as the whole
// file disagreeing and falls back to Ply_buildFaceLayoutTable's serial, rigorous, always correct scan rather
// than trusting a "yes" that cannot tell a real one from a lucky one.

static Bool Ply_buildFaceLayout(
	StreamRef *stream, U64 faceOffset, U8 countType, U8 indexType, U8 format, U32 faceCount, JobQueue *jobs,
	PlyFaceLayout *layout, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	MeshSource first = (MeshSource) { 0 };
	Buffer jobBuf = Buffer_createNull();

	*layout = (PlyFaceLayout) { .uniform = true };

	if(!faceCount)
		goto clean;

	const U8 countSize = Ply_typeSize[countType];
	const U8 valueSize = Ply_typeSize[indexType];

	gotoIfError3(clean, MeshSource_create(stream, faceOffset, alloc, &first, e_rr));

	U8 countBuf[8];
	gotoIfError3(clean, MeshSource_readBytes(&first, countBuf, countSize, e_rr));
	MeshSource_free(&first);

	const U64 corners0 = (U64) Ply_decodeScalar(countBuf, countType, format);

	if(corners0 < 3)
		retError(clean, Error_invalidParameter(0, 18, "Ply_read() a face has fewer than three corners"));

	const U64 stride0 = countSize + corners0 * valueSize;

	const U64 jobCount = Ply_parallelChunkCount(faceCount, JobQueue_threadCount(jobs));
	const U32 perJob = (U32) (((U64) faceCount + jobCount - 1) / jobCount);
	const U64 realJobCount = ((U64) faceCount + perJob - 1) / perJob;

	//The checks are optional: if they can't run, the serial table below answers instead, so they take no e_rr.

	Bool ranChecks = Buffer_createEmptyBytes(realJobCount * sizeof(PlyUniformCheckJob), alloc, &jobBuf, NULL);
	Bool uniform = false;

	if(ranChecks) {

		PlyUniformCheckJob *jobArr = (PlyUniformCheckJob*) jobBuf.ptrNonConst;
		JobGroup group = (JobGroup) { 0 };

		ranChecks = JobGroup_create(&group, jobs, NULL, NULL, NULL, NULL) && JobGroup_enter(&group, realJobCount, NULL);

		U64 pushed = 0;

		if(ranChecks)
			for(; pushed < realJobCount; ++pushed) {

				const U32 chunkFirst = (U32) (pushed * perJob);
				const U32 chunkFaces = (U32) U64_min(perJob, (U64) faceCount - chunkFirst);

				jobArr[pushed] = (PlyUniformCheckJob) {
					.stream = stream, .alloc = alloc, .group = &group,
					.byteStart = faceOffset + (U64) chunkFirst * stride0, .corners0 = corners0, .faceCount = chunkFaces,
					.countType = countType, .indexType = indexType, .format = format
				};

				if(!JobQueue_pushGroup(jobs, Ply_uniformCheckJob, jobArr + pushed, NULL, &group, NULL))
					break;
			}

		for(U64 i = pushed; i < realJobCount; ++i)
			JobGroup_leave(&group, NULL);

		const Bool waited = JobGroup_wait(&group, NULL);
		ranChecks = ranChecks && pushed == realJobCount && waited;

		if(ranChecks) {

			uniform = true;

			for(U64 i = 0; i < realJobCount; ++i)
				if(!jobArr[i].uniform) {
					uniform = false;
					break;
				}
		}
	}

	if(ranChecks && uniform) {
		layout->corners = corners0;
		layout->stride = stride0;
		layout->totalTriangles = (U64) faceCount * (corners0 - 2);
		goto clean;
	}

	layout->uniform = false;

	gotoIfError3(clean, Ply_buildFaceLayoutTable(
		stream, faceOffset, countType, indexType, format, faceCount, layout, alloc, e_rr
	));

clean:
	MeshSource_free(&first);
	Buffer_free(&jobBuf, alloc);
	return s_uccess;
}

//Ply_read on a JobQueue: the vertex records, which are already fixed stride, split evenly across chunks that
//each run Ply_readRange; the face records, fanned into triangles the same way Ply_read's own fast path does,
// split the same way once Ply_buildFaceLayout says where each one begins. jobs == NULL, a shape the split
// above does not cover, or a stream that does not declare EStreamType_ConcurrentRead all fall back to Ply_read
// whole, which is always correct and is what this would have to do piece by piece otherwise.
//
//forceParallel bypasses PLY_PARALLEL_MIN_BYTES, nothing else: it still refuses the shapes above, since forcing
//those through would not test the split, only that Ply_read gets called. Tests use it to exercise chunk
// boundaries on inputs too small to ever pay for splitting on their own.

//One scratch buffer handed to the real output in one write: MeshSink_write still stages it through
//MeshSink_reserveFor's own geometric doubling, which sounds like it should lose to reserving the exact final
// size up front, and measured slower by about 60% on Lucy's 336 MiB index stream: a single allocation that
// big appears to cost more, in this allocator, than several smaller ones growing into it. Reserving ahead of
// time is for a caller who knows it is about to write many times, not once.

static Bool Ply_flushWhole(StreamRef *stream, U64 offset, Buffer data, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	MeshSink sink = (MeshSink) { 0 };

	gotoIfError3(clean, MeshSink_create(stream, offset, alloc, &sink, e_rr));
	gotoIfError3(clean, MeshSink_write(&sink, data.ptr, Buffer_length(data), e_rr));
	gotoIfError3(clean, MeshSink_flush(&sink, e_rr));

clean:
	MeshSink_free(&sink);
	return s_uccess;
}

Bool Ply_readParallel(
	StreamRef *stream,
	U64 *off,
	EMeshFlags flags,
	MeshInfo *info,
	const MeshOutput *output,
	JobQueue *jobs,
	Bool forceParallel,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	MeshSource headSrc = (MeshSource) { 0 };
	Buffer lineBuf = Buffer_createNull();
	PlyHeader headerData = (PlyHeader) { 0 };
	Bool headerFreed = false;

	Buffer positionsBuf = Buffer_createNull(), attributesBuf = Buffer_createNull();
	Buffer indicesBuf = Buffer_createNull(), wordsBuf = Buffer_createNull();
	Buffer vertexJobBuf = Buffer_createNull(), vertexInfoBuf = Buffer_createNull(), faceJobBuf = Buffer_createNull();
	PlyFaceLayout faceLayout = (PlyFaceLayout) { 0 };

	JobGroup vertexGroup = (JobGroup) { 0 }, faceGroup = (JobGroup) { 0 };
	U64 vertexJobCount = 0, faceJobCount = 0;

	//Nothing here can help: no queue to split across, a null the caller will get the usual error for, or
	//flags that need the whole mesh regardless of how the body is shaped.
	//forceParallel refuses instead of falling back, at each of the three places, so a test that forces it knows
	// the split ran.

	const Bool wholeMesh = !!(flags & (EMeshFlags_ComputeNormals | EMeshFlags_QuantizePositions));

	if(!jobs || !stream || !off || !info || !output || !output->positions || !output->indices || wholeMesh) {

		if(forceParallel && (!jobs || wholeMesh))
			retError(clean, Error_unsupportedOperation(
				0, "Ply_readParallel()::forceParallel can't split without a queue or with flags that need the whole mesh"
			));

		return Ply_read(stream, off, flags, info, output, alloc, e_rr);
	}

	const OxStream *streamData = RefPtr_data(stream, OxStream);

	if(!(streamData->streamType & EStreamType_ConcurrentRead)) {

		if(forceParallel)
			retError(clean, Error_unsupportedOperation(
				1, "Ply_readParallel()::forceParallel can't split a stream without EStreamType_ConcurrentRead"
			));

		return Ply_read(stream, off, flags, info, output, alloc, e_rr);
	}

	*info = (MeshInfo) { 0 };

	const U64 headOff = *off;
	gotoIfError3(clean, MeshSource_create(stream, headOff, alloc, &headSrc, e_rr));
	gotoIfError3(clean, Buffer_createUninitializedBytes(MESH_LINE_CAP, alloc, &lineBuf, e_rr));
	gotoIfError3(clean, Ply_parseHeader(&headSrc, (C8*) lineBuf.ptrNonConst, &headerData, info, alloc, e_rr));
	gotoIfError3(clean, Ply_checkCounts(&headerData, &headSrc, e_rr));

	info->allNormals = info->hasNormals;
	info->allUvs = info->hasUvs;

	Ply_resolveStrides(&headerData);

	MeshHeader header = (MeshHeader) { 0 };
	gotoIfError3(clean, Ply_fillMeshHeader(&headerData, MeshSource_offset(&headSrc), &header, e_rr));

	const Bool shapeOk =
		headerData.format != EPlyFormat_Ascii &&
		headerData.elements.length == 2 &&
		header.vertexStride != 0 &&
		header.faceOffset != 0 &&
		header.vertexCount != 0;

	const U64 bodyBytes = !shapeOk ? 0 :
		(U64) header.vertexCount * header.vertexStride +
		(U64) header.faceCount * (
			EMeshScalarType_bytes((EMeshScalarType) header.faceCountType) +
			3 * EMeshScalarType_bytes((EMeshScalarType) header.faceIndexType)
		);

	if(!shapeOk && forceParallel)
		retError(clean, Error_unsupportedOperation(
			2, "Ply_readParallel()::forceParallel can't split this file's elements"
		));

	if(!shapeOk || (!forceParallel && bodyBytes < PLY_PARALLEL_MIN_BYTES)) {

		ListPlyElement_free(&headerData.elements, alloc);
		ListPlyProperty_free(&headerData.properties, alloc);
		Buffer_free(&lineBuf, alloc);
		MeshSource_free(&headSrc);

		return Ply_read(stream, off, flags, info, output, alloc, e_rr);
	}

	//Past this point the header is fully resolved into header/headerData and nothing below reads the file's
	//text again, so the parse scratch leaves now rather than at the end.

	ListPlyElement_free(&headerData.elements, alloc);
	ListPlyProperty_free(&headerData.properties, alloc);
	Buffer_free(&lineBuf, alloc);
	MeshSource_free(&headSrc);
	headerFreed = true;

	const U8 format = header.bigEndian ? EPlyFormat_BigEndian : EPlyFormat_LittleEndian;
	const U64 threadCount = JobQueue_threadCount(jobs);
	const RefPtrType memType = MemoryStream_makeType(alloc);

	const U8 attrStride = output->attributes ? MeshAttributeLayout_fromFlags(flags).stride : 0;
	const Bool keepPositions = output->triangles != NULL;

	//---- vertices, split by the stride the header already resolved into chunks that each run Ply_readRange

	gotoIfError3(clean, Buffer_createUninitializedBytes(
		(U64) header.vertexCount * 3 * sizeof(F32), alloc, &positionsBuf, e_rr
	));

	if(attrStride)
		gotoIfError3(clean, Buffer_createUninitializedBytes(
			(U64) header.vertexCount * attrStride, alloc, &attributesBuf, e_rr
		));

	vertexJobCount = Ply_parallelChunkCount(header.vertexCount, threadCount);

	const U32 vertexPerJob = (U32) (((U64) header.vertexCount + vertexJobCount - 1) / vertexJobCount);
	vertexJobCount = ((U64) header.vertexCount + vertexPerJob - 1) / vertexPerJob;

	gotoIfError3(clean, Buffer_createEmptyBytes(vertexJobCount * sizeof(PlyVertexJob), alloc, &vertexJobBuf, e_rr));
	gotoIfError3(clean, Buffer_createEmptyBytes(vertexJobCount * sizeof(MeshInfo), alloc, &vertexInfoBuf, e_rr));
	gotoIfError3(clean, JobGroup_create(&vertexGroup, jobs, NULL, NULL, NULL, e_rr));
	gotoIfError3(clean, JobGroup_enter(&vertexGroup, vertexJobCount, e_rr));

	{
		PlyVertexJob *jobArr = (PlyVertexJob*) vertexJobBuf.ptrNonConst;
		Bool vertexOk = true;
		U64 pushed = 0;

		for(; pushed < vertexJobCount; ++pushed) {

			const U32 firstVertex = (U32) (pushed * vertexPerJob);
			const U32 vertexCount = (U32) U64_min(vertexPerJob, (U64) header.vertexCount - firstVertex);

			PlyVertexJob *job = jobArr + pushed;

			*job = (PlyVertexJob) {
				.stream = stream, .header = &header, .flags = flags, .alloc = alloc, .group = &vertexGroup,
				.info = (MeshInfo*) vertexInfoBuf.ptrNonConst + pushed,
				.firstVertex = firstVertex, .vertexCount = vertexCount
			};

			if(!MemoryStream_createFromBufferRegion(
				Buffer_createRef(positionsBuf.ptrNonConst, Buffer_length(positionsBuf)),
				(U64) firstVertex * 3 * sizeof(F32), (U64) vertexCount * 3 * sizeof(F32),
				EMemoryStreamFlags_IsWritable, &memType, (MemoryStreamRef**) &job->positions, e_rr
			)) {
				vertexOk = false;
				break;
			}

			if(attrStride && !MemoryStream_createFromBufferRegion(
				Buffer_createRef(attributesBuf.ptrNonConst, Buffer_length(attributesBuf)),
				(U64) firstVertex * attrStride, (U64) vertexCount * attrStride,
				EMemoryStreamFlags_IsWritable, &memType, (MemoryStreamRef**) &job->attributes, e_rr
			)) {
				vertexOk = false;
				break;
			}

			if(!JobQueue_pushGroup(jobs, Ply_vertexJob, job, NULL, &vertexGroup, e_rr)) {
				vertexOk = false;
				break;
			}
		}

		for(U64 i = pushed; i < vertexJobCount; ++i)
			JobGroup_leave(&vertexGroup, NULL);

		//Waited regardless of whether every chunk started: whatever did start is already running against this
		//call's own stack and buffers, and nothing below may touch those until every chunk has finished.

		if(!JobGroup_wait(&vertexGroup, vertexOk ? e_rr : NULL) && vertexOk)
			vertexOk = false;

		if(!vertexOk) {
			s_uccess = false;
			goto clean;
		}

		for(U64 i = 0; i < vertexJobCount; ++i) {

			if(jobArr[i].err.genericError) {
				if(e_rr) *e_rr = jobArr[i].err;
				s_uccess = false;
				goto clean;
			}

			Ply_mergeSpanInfo(info, jobArr[i].info, jobArr[i].info->vertexCount);
		}
	}

	//---- faces, split by Ply_buildFaceLayout and decoded the same way Ply_read's own fast path does

	U64 faceEnd = header.faceOffset;
	U32 fannedFaces = 0;
	U64 totalTriangles = 0;

	if(header.faceCount) {

		const U8 countType = Ply_fromScalarType[header.faceCountType];
		const U8 indexType = Ply_fromScalarType[header.faceIndexType];

		gotoIfError3(clean, Ply_buildFaceLayout(
			stream, header.faceOffset, countType, indexType, format, header.faceCount, jobs, &faceLayout, alloc, e_rr
		));

		totalTriangles = faceLayout.totalTriangles;
		faceEnd = PlyFaceLayout_end(&faceLayout, header.faceOffset, header.faceCount);

		const U8 indexWidth = (flags & EMeshFlags_NarrowIndices) ? sizeof(U16) : sizeof(U32);

		gotoIfError3(clean, Buffer_createUninitializedBytes(
			totalTriangles * 3 * indexWidth, alloc, &indicesBuf, e_rr
		));

		if(keepPositions)
			gotoIfError3(clean, Buffer_createUninitializedBytes(
				totalTriangles * sizeof(U32), alloc, &wordsBuf, e_rr
			));

		faceJobCount = Ply_parallelChunkCount(header.faceCount, threadCount);

		const U32 facePerJob = (U32) (((U64) header.faceCount + faceJobCount - 1) / faceJobCount);
		faceJobCount = ((U64) header.faceCount + facePerJob - 1) / facePerJob;

		gotoIfError3(clean, Buffer_createEmptyBytes(faceJobCount * sizeof(PlyFaceJob), alloc, &faceJobBuf, e_rr));
		gotoIfError3(clean, JobGroup_create(&faceGroup, jobs, NULL, NULL, NULL, e_rr));
		gotoIfError3(clean, JobGroup_enter(&faceGroup, faceJobCount, e_rr));

		PlyFaceJob *jobArr = (PlyFaceJob*) faceJobBuf.ptrNonConst;
		Bool faceOk = true;
		U64 pushed = 0;

		for(; pushed < faceJobCount; ++pushed) {

			const U32 firstFace = (U32) (pushed * facePerJob);
			const U32 faceCount = (U32) U64_min(facePerJob, (U64) header.faceCount - firstFace);

			const U64 triStart = PlyFaceLayout_triOffset(&faceLayout, firstFace);
			const U64 triCount = PlyFaceLayout_triOffset(&faceLayout, firstFace + faceCount) - triStart;

			PlyFaceJob *job = jobArr + pushed;

			*job = (PlyFaceJob) {
				.stream = stream, .alloc = alloc, .group = &faceGroup, .flags = flags,
				.countType = countType, .indexType = indexType, .format = format,
				.byteStart = PlyFaceLayout_byteOffset(&faceLayout, header.faceOffset, firstFace),
				.verticesRead = header.vertexCount, .faceCount = faceCount, .keepPositions = keepPositions,
				.heldPositions = keepPositions ? (const F32*) positionsBuf.ptr : NULL
			};

			if(!MemoryStream_createFromBufferRegion(
				Buffer_createRef(indicesBuf.ptrNonConst, Buffer_length(indicesBuf)),
				triStart * 3 * indexWidth, triCount * 3 * indexWidth,
				EMemoryStreamFlags_IsWritable, &memType, (MemoryStreamRef**) &job->indices, e_rr
			)) {
				faceOk = false;
				break;
			}

			if(keepPositions && !MemoryStream_createFromBufferRegion(
				Buffer_createRef(wordsBuf.ptrNonConst, Buffer_length(wordsBuf)),
				triStart * sizeof(U32), triCount * sizeof(U32),
				EMemoryStreamFlags_IsWritable, &memType, (MemoryStreamRef**) &job->words, e_rr
			)) {
				faceOk = false;
				break;
			}

			if(!JobQueue_pushGroup(jobs, Ply_faceJob, job, NULL, &faceGroup, e_rr)) {
				faceOk = false;
				break;
			}
		}

		for(U64 i = pushed; i < faceJobCount; ++i)
			JobGroup_leave(&faceGroup, NULL);

		if(!JobGroup_wait(&faceGroup, faceOk ? e_rr : NULL) && faceOk)
			faceOk = false;

		if(!faceOk) {
			s_uccess = false;
			goto clean;
		}

		for(U64 i = 0; i < faceJobCount; ++i) {

			if(jobArr[i].err.genericError) {
				if(e_rr) *e_rr = jobArr[i].err;
				s_uccess = false;
				goto clean;
			}

			fannedFaces += jobArr[i].fannedFaces;
		}
	}

	//---- flush: one write per stream, in the order the records occupy it, which is as cheap as this ever gets

	gotoIfError3(clean, Ply_flushWhole(output->positions, output->positionOffset, positionsBuf, alloc, e_rr));

	if(output->attributes)
		gotoIfError3(clean, Ply_flushWhole(output->attributes, output->attributeOffset, attributesBuf, alloc, e_rr));

	gotoIfError3(clean, Ply_flushWhole(output->indices, output->indexOffset, indicesBuf, alloc, e_rr));

	if(output->triangles)
		gotoIfError3(clean, Ply_flushWhole(output->triangles, output->triangleOffset, wordsBuf, alloc, e_rr));

	info->fannedFaces = fannedFaces;
	info->indexCount = (U32) (totalTriangles * 3);
	info->materialCount = 1;

	*off = faceEnd;

clean:

	if(vertexJobBuf.ptr) {
		PlyVertexJob *jobArr = (PlyVertexJob*) vertexJobBuf.ptrNonConst;

		for(U64 i = 0; i < vertexJobCount; ++i) {
			RefPtr_dec(&jobArr[i].attributes);
			RefPtr_dec(&jobArr[i].positions);
		}
	}

	if(faceJobBuf.ptr) {
		PlyFaceJob *jobArr = (PlyFaceJob*) faceJobBuf.ptrNonConst;

		for(U64 i = 0; i < faceJobCount; ++i) {
			RefPtr_dec(&jobArr[i].words);
			RefPtr_dec(&jobArr[i].indices);
		}
	}

	PlyFaceLayout_free(&faceLayout, alloc);
	Buffer_free(&faceJobBuf, alloc);
	Buffer_free(&vertexInfoBuf, alloc);
	Buffer_free(&vertexJobBuf, alloc);
	Buffer_free(&wordsBuf, alloc);
	Buffer_free(&indicesBuf, alloc);
	Buffer_free(&attributesBuf, alloc);
	Buffer_free(&positionsBuf, alloc);

	if(!headerFreed) {
		ListPlyElement_free(&headerData.elements, alloc);
		ListPlyProperty_free(&headerData.properties, alloc);
		Buffer_free(&lineBuf, alloc);
		MeshSource_free(&headSrc);
	}

	return s_uccess;
}
