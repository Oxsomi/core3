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

//formats/oiSB/sb_variable.c

#include "formats/oiDL/dl_entry.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/list_impl.h"
#include "types/container/list_basic_types.h"
#include "types/base/constants.h"
#include "types/base/string_read_helper.h"

TListImpl(SBStruct);
TListImpl(SBVar);

#define EMPTY4 "", "", "", ""
#define EXPAND4(a, x, x1) #a #x1 #x, #a "x2" #x, #a "x3" #x, #a "x4" #x

#define EXPAND4x4(...)                              \
	EMPTY4,                                         \
	EMPTY4,                                         \
	EMPTY4,                                         \
	EMPTY4,                                         \
													\
	EMPTY4,                                         \
	EXPAND4(F16, __VA_ARGS__),                      \
	EXPAND4(I16, __VA_ARGS__),                      \
	EXPAND4(U16, __VA_ARGS__),                      \
													\
	EMPTY4,                                         \
	EXPAND4(F32, __VA_ARGS__),                      \
	EXPAND4(I32, __VA_ARGS__),                      \
	EXPAND4(U32, __VA_ARGS__),                      \
													\
	EMPTY4,                                         \
	EXPAND4(F64, __VA_ARGS__),                      \
	EXPAND4(I64, __VA_ARGS__),                      \
	EXPAND4(U64, __VA_ARGS__)

const C8 *ESBType_names[] = {
	EXPAND4x4(,),
	EXPAND4x4(x2, x1),
	EXPAND4x4(x3, x1),
	EXPAND4x4(x4, x1)
};

const C8 *ESBType_name(ESBType type) {
	return ESBType_names[type];
}

Bool SBFile_addStruct(SBFile *sbFile, CharString *name, SBStruct sbStruct, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	Bool pushedStruct = false;
	CharString tmp = CharString_createNull();

	if (!sbFile || !name)
		retError(clean, Error_nullPointer(!sbFile ? 0 : 1, "SBFile_addStruct()::sbFile and name are required"));

	if (!sbStruct.stride)
		retError(clean, Error_invalidParameter(
			2, 0, "SBFile_addStruct()::sbStruct.stride is required"
		));

	if (CharString_length(*name) >= 128)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_addStruct()::*name must be leq 128 bytes"));

	if (sbFile->structs.length >= (U16) (U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->structs.length, U16_MAX, "SBFile_addStruct()::sbFile->structs.length limited to 65535"
		));

	U64 hash = sbFile->hash;

	SBStruct_applyHash(&hash, sbStruct, *name, sbFile->structs.length);

	gotoIfError3(clean, ListSBStruct_pushBack(&sbFile->structs, sbStruct, alloc, e_rr));
	pushedStruct = true;

	if (CharString_isRef(*name))
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &tmp, e_rr));

	gotoIfError3(clean, DLFile_insertEntryString(
		&sbFile->names,
		sbFile->structs.length - 1,
		tmp.ptr ? &tmp : name,
		alloc,
		e_rr
	));

	sbFile->hash = hash;        //Only once nothing can fail anymore

clean:

	CharString_free(&tmp, alloc);

	if(!s_uccess && pushedStruct)
		ListSBStruct_popBack(&sbFile->structs, NULL, NULL);

	return s_uccess;
}

void SBStruct_applyHash(U64 *hashRes, SBStruct strct, CharString name, U64 index) {

	U64 hash = Buffer_fnv1a64Single(index << 2, Buffer_fnv1a64Offset);

	hash = Buffer_fnv1a64Single(strct.stride | ((U64) CharString_length(name) << 32), hash);
	*hashRes ^= Buffer_fnv1a64(CharString_bufferConst(name), hash);
}

void SBVar_applyHash(U64 *hashRes, SBVar var, CharString name, U64 index) {

	const void *structId = &var.structId;                  //Interpreted as a U64 and U32
	const U32 *structIdU32 = (const U32*) structId;        //[] = (structId, arrayIndex), offset, (type, flags, parentId)

	U64 hash = Buffer_fnv1a64Single((index << 2) | 1, Buffer_fnv1a64Offset);

	hash = Buffer_fnv1a64Single(structIdU32[0] | ((U64) structIdU32[1] << 32), hash);
	hash = Buffer_fnv1a64Single(structIdU32[2] | ((U64) CharString_length(name) << 32), hash);
	*hashRes ^= Buffer_fnv1a64(CharString_bufferConst(name), hash);
}

void ListU32_applyHash(U64 *hashRes, ListU32 array, U64 index) {

	U64 hash = Buffer_fnv1a64Single((index << 2) | 2, Buffer_fnv1a64Offset);

	hash = Buffer_fnv1a64Single(array.length, hash);

	for(U64 i = 0; i + 1 < array.length; i += 2)
		hash = Buffer_fnv1a64Single(array.ptr[i] | ((U64) array.ptr[i + 1] << 32), hash);

	if(array.length & 1)
		hash = Buffer_fnv1a64Single(array.ptr[array.length - 1], hash);

	*hashRes ^= hash;
}

static Bool SBFile_validateVariableExtent(
	const SBFile *sbFile, U32 offset, U64 size, U16 parentId, Bool checkAncestors, Error *e_rr
) {

	Bool s_uccess = true;
	U64 end = (U64) offset + size;

	while(parentId != U16_MAX) {

		if(parentId >= sbFile->vars.length)
			retError(clean, Error_outOfBounds(
				0, parentId, sbFile->vars.length, "SBFile_validateVariableExtent()::parentId is out of bounds"
			));

		SBVar parent = sbFile->vars.ptr[parentId];

		if(parent.type)
			retError(clean, Error_invalidState(0, "SBFile_validateVariableExtent()::parent isn't a struct"));

		U32 stride = sbFile->structs.ptr[parent.structId].stride;

		if(offset < parent.offset || end - parent.offset > stride)
			retError(clean, Error_outOfBounds(
				0, end, (U64) parent.offset + stride, "SBFile_validateVariableExtent()::member exceeds its parent"
			));

		//Struct metadata can include padding. Its occupied extent becomes known when primitive children are added.

		if(!checkAncestors)
			goto clean;

		//A child's occupied bytes must fit in the last instance of every enclosing struct array too.

		U64 count = parent.arrayDimOrArrayId;

		if(count >> 15) {

			ListU32 dimensions = sbFile->arrays.ptr[count & (U16) I16_MAX];

			count = 1;

			for(U64 i = 0; i < dimensions.length; ++i)
				count *= dimensions.ptr[i];
		}

		if(count)
			end += (count - 1) * stride;

		parentId = parent.parentId;
	}

	if(end > sbFile->bufferSize)
		retError(clean, Error_outOfBounds(
			0, end, sbFile->bufferSize, "SBFile_validateVariableExtent()::member exceeds the buffer"
		));

clean:
	return s_uccess;
}

Bool SBFile_addVariableAsType(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,
	ESBType type,
	ESBVarFlag flags,
	ListU32 *arrays,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool pushedName = false;
	Bool pushedVar = false;
	CharString tmp = CharString_createNull();
	ListU32 tmpArray = (ListU32) { 0 };

	if (!sbFile || !name)
		retError(clean, Error_nullPointer(!sbFile ? 0 : 1, "SBFile_addVariableAsType()::sbFile and name are required"));

	if (flags & ESBVarFlag_Invalid)
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsType()::flags is invalid"));

	if(arrays && arrays->length > 32)
		retError(clean, Error_outOfBounds(6, arrays->length, 32, "SBFile_addVariableAsType()::arrays.length limited to 32"));

	if (arrays && !arrays->length)
		retError(clean, Error_invalidState(0, "SBFile_addVariableAsType()::arrays should be NULL if the array is empty"));

	if (
		ESBType_getPrimitive(type) == ESBPrimitive_Invalid || (type >> 8) || (
		ESBType_getPrimitive(type) == ESBPrimitive_Float && ESBType_getStride(type) == ESBStride_X8
	))
		retError(clean, Error_invalidParameter(4, 0, "SBFile_addVariableAsType()::type is invalid"));

	if (sbFile->vars.length >= (U16) (U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->vars.length, U16_MAX, "SBFile_addVariableAsType()::sbFile->vars.length limited to 65535"
		));

	if (arrays && sbFile->arrays.length >= (U16) (U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->arrays.length, U16_MAX, "SBFile_addVariableAsType()::sbFile->arrays.length limited to 65535"
		));

	if (CharString_length(*name) > 128)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_addVariableAsType()::*name must be leq 128 bytes"));

	//Padding happens each element of an array that isn't 16-byte aligned.
	//The last element doesn't have padding.
	//Offsets and sizes are U64 from here on: a U32 offset plus a U32 size can't wrap there.

	Bool isTightlyPacked = sbFile->flags & ESBSettingsFlags_IsTightlyPacked;
	U32 size = ESBType_getSize(type, isTightlyPacked);
	U8 typeSize = 1 << ESBType_getStride(type);

	//Every type is self aligned, in both packings

	if (offset & (typeSize - 1))
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsType()::offset doesn't follow req type alignment"));

	U64 totalSizeBytes = isTightlyPacked ? size : ((size + 15) & ~15);

	if(arrays)
		for (U64 i = 0; i < arrays->length; ++i) {

			U32 arrayi = arrays->ptr[i];

			if(!arrayi)
				retError(clean, Error_invalidParameter(0, 0, "SBFile_addVariableAsType()::arrays[i] is 0"));

			totalSizeBytes *= arrayi;        //totalSizeBytes <= U32_MAX before this, so U64 can't wrap

			if (totalSizeBytes > U32_MAX)
				retError(clean, Error_outOfBounds(
					0, totalSizeBytes, U32_MAX,
					"SBFile_addVariableAsType()::arrays.length bytes out of bounds (only 2^32 permitted)"
				));
		}

	if(!isTightlyPacked && (size & 15))
		totalSizeBytes -= 16 - (size & 15);

	U64 sizeU64 = totalSizeBytes;

	//CBuffer packing: something that spans a 16-byte boundary starts on one (this makes arrays start aligned too)

	if (!isTightlyPacked && ((((U64) offset + sizeU64 - 1) >> 4) != ((U64) offset >> 4)) && (offset & 15))
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsType()::offset spans 16 bytes, needs alignment"));

	gotoIfError3(clean, SBFile_validateVariableExtent(sbFile, offset, sizeU64, parentId, true, e_rr));

	for(U64 i = 0; i < sbFile->vars.length; ++i) {

		SBVar vari = sbFile->vars.ptr[i];

		if(vari.parentId != parentId)
			continue;

		if (CharString_equalsStringSensitive(&sbFile->names.entryStrings.ptr[sbFile->structs.length + i], name))
			retError(clean, Error_invalidState(0, "SBFile_addVariableAsType() parent already contains this member name"));
	}

	Bool reuseArray = false;
	U16 arrayDimOrArrayId = 0;

	if (arrays) {

		Bool useInlineArray = arrays->length == 1 && arrays->ptr[0] <= (U16) I16_MAX;

		if (useInlineArray) {
			reuseArray = true;
			arrayDimOrArrayId = (U16) arrays->ptr[0];
			ListU32_free(arrays, alloc);
		}

		else {

			for (U64 i = 0; i < sbFile->arrays.length; ++i)
				if (ListU32_eq(*arrays, sbFile->arrays.ptr[i])) {
					arrayDimOrArrayId = (U16) i | (U16) I16_MIN;
					reuseArray = true;
					ListU32_free(arrays, alloc);
					break;
				}

			if (!reuseArray) {

				if(sbFile->arrays.length >= (U16) I16_MAX)
					retError(clean, Error_invalidState(0, "SBFile_addVariableAsStruct() no arrays left"));

				arrayDimOrArrayId = (U16) sbFile->arrays.length | (U16) I16_MIN;
			}
		}
	}

	//Add to array & vars

	SBVar var = (SBVar) {
		.structId = U16_MAX,
		.arrayDimOrArrayId = arrayDimOrArrayId,
		.offset = offset,
		.type = type,
		.flags = flags,
		.parentId = parentId
	};

	U64 hash = sbFile->hash;        //Committed once nothing can fail anymore

	SBVar_applyHash(&hash, var, *name, sbFile->vars.length);

	gotoIfError3(clean, ListSBVar_pushBack(&sbFile->vars, var, alloc, e_rr));
	pushedVar = true;

	if (CharString_isRef(*name))
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &tmp, e_rr));

	gotoIfError3(clean, DLFile_addEntryString(&sbFile->names, tmp.ptr ? &tmp : name, alloc, e_rr));
	pushedName = true;

	if(!reuseArray) {

		if (arrays && ListU32_isRef(*arrays))
			gotoIfError3(clean, ListU32_createCopy(*arrays, alloc, &tmpArray, e_rr));

		if (arrays) {

			ListU32_applyHash(&hash, tmpArray.ptr ? tmpArray : *arrays, sbFile->arrays.length);
			gotoIfError3(clean, ListListU32_pushBack(&sbFile->arrays, tmpArray.ptr ? tmpArray : *arrays, alloc, e_rr));
			*arrays = tmpArray = (ListU32) { 0 };
		}
	}

	sbFile->hash = hash;

clean:

	CharString_free(&tmp, alloc);
	ListU32_free(&tmpArray, alloc);

	if(!s_uccess && pushedVar)
		ListSBVar_popBack(&sbFile->vars, NULL, NULL);

	if(!s_uccess && pushedName)
		DLFile_remove(&sbFile->names, sbFile->names.entryStrings.length - 1, alloc, NULL);

	return s_uccess;
}

Bool SBFile_addVariableAsStruct(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,
	U16 structId,
	ESBVarFlag flags,
	ListU32 *arrays,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool pushedVar = false;
	Bool pushedName = false;
	CharString tmp = CharString_createNull();
	ListU32 tmpArray = (ListU32) { 0 };

	if (!sbFile || !name)
		retError(clean, Error_nullPointer(!sbFile ? 0 : 1, "SBFile_addVariableAsStruct()::sbFile and name are required"));

	if(flags & ESBVarFlag_Invalid)
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsStruct()::flags is invalid"));

	if(arrays && arrays->length > 32)
		retError(clean, Error_outOfBounds(6, arrays->length, 32, "SBFile_addVariableAsStruct()::arrays.length limited to 32"));

	if(arrays && !arrays->length)
		retError(clean, Error_invalidState(0, "SBFile_addVariableAsStruct()::arrays should be NULL if the array is empty"));

	if(structId >= sbFile->structs.length)
		retError(clean, Error_outOfBounds(
			0, structId, sbFile->structs.length, "SBFile_addVariableAsStruct()::structId out of bounds"
		));

	if(sbFile->vars.length >= (U16) (U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->vars.length, U16_MAX, "SBFile_addVariableAsStruct()::vars.length limited to 65535"
		));

	if(arrays && sbFile->arrays.length >= (U16) (U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->arrays.length, U16_MAX, "SBFile_addVariableAsStruct()::arrays.length limited to 65535"
		));

	if(CharString_length(*name) > 128)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_addVariableAsStruct()::*name must be leq 128 bytes"));

	//Alignment: starts at 16 byte boundary if CBuffer

	Bool isTightlyPacked = sbFile->flags & ESBSettingsFlags_IsTightlyPacked;

	if(!isTightlyPacked && (offset & 15))
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsStruct()::offset needs 16-byte alignment"));

	SBStruct strc = sbFile->structs.ptr[structId];

	U64 size = strc.stride;

	if(arrays)
		for (U64 i = 0; i < arrays->length; ++i) {

			U32 arrayi = arrays->ptr[i];

			if(!arrayi)
				retError(clean, Error_invalidParameter(0, 0, "SBFile_addVariableAsStruct()::arrays[i] is 0"));

			size *= arrayi;

			if(size > U32_MAX)
				retError(clean, Error_outOfBounds(
					0, size, U32_MAX,
					"SBFile_addVariableAsStruct()::arrays.length bytes out of bounds (only 2^32 permitted)"
				));
		}

	//Cbuffer array strides include row padding; the final instance may leave that padding unoccupied.
	//Its exact occupied size is checked as children are appended, through every enclosing array instance.

	if(arrays && !isTightlyPacked && !(strc.stride & 15))
		size -= 15;

	gotoIfError3(clean, SBFile_validateVariableExtent(sbFile, offset, size, parentId, false, e_rr));

	for(U64 i = 0; i < sbFile->vars.length; ++i) {

		SBVar vari = sbFile->vars.ptr[i];

		if(vari.parentId != parentId)
			continue;

		if (CharString_equalsStringSensitive(&sbFile->names.entryStrings.ptr[sbFile->structs.length + i], name))
			retError(clean, Error_invalidState(0, "SBFile_addVariableAsStruct() parent already contains this member name"));
	}

	Bool reuseArray = false;
	U16 arrayDimOrArrayId = 0;

	if (arrays) {

		Bool useInlineArray = arrays->length == 1 && arrays->ptr[0] <= (U16) I16_MAX;

		if (useInlineArray) {
			reuseArray = true;
			arrayDimOrArrayId = (U16) arrays->ptr[0];
			ListU32_free(arrays, alloc);
		}

		else {

			for (U64 i = 0; i < sbFile->arrays.length; ++i)
				if (ListU32_eq(*arrays, sbFile->arrays.ptr[i])) {
					arrayDimOrArrayId = (U16) i | (U16) I16_MIN;
					reuseArray = true;
					ListU32_free(arrays, alloc);
					break;
				}

			if (!reuseArray) {

				if(sbFile->arrays.length >= (U16) I16_MAX)
					retError(clean, Error_invalidState(0, "SBFile_addVariableAsStruct() no arrays left"));

				arrayDimOrArrayId = (U16) sbFile->arrays.length | (U16) I16_MIN;
			}
		}
	}

	//Add to array & vars

	SBVar var = (SBVar) {
		.structId = structId,
		.arrayDimOrArrayId = arrayDimOrArrayId,
		.offset = offset,
		.type = 0,
		.flags = flags,
		.parentId = parentId
	};

	U64 hash = sbFile->hash;        //Committed once nothing can fail anymore

	SBVar_applyHash(&hash, var, *name, sbFile->vars.length);

	gotoIfError3(clean, ListSBVar_pushBack(&sbFile->vars, var, alloc, e_rr));
	pushedVar = true;

	if(CharString_isRef(*name))
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &tmp, e_rr));

	gotoIfError3(clean, DLFile_addEntryString(&sbFile->names, tmp.ptr ? &tmp : name, alloc, e_rr));
	pushedName = true;

	if(!reuseArray) {

		if(arrays && ListU32_isRef(*arrays))
			gotoIfError3(clean, ListU32_createCopy(*arrays, alloc, &tmpArray, e_rr));

		if (arrays) {

			ListU32_applyHash(&hash, tmpArray.ptr ? tmpArray : *arrays, sbFile->arrays.length);
			gotoIfError3(clean, ListListU32_pushBack(&sbFile->arrays, tmpArray.ptr ? tmpArray : *arrays, alloc, e_rr));
			*arrays = tmpArray = (ListU32) { 0 };
		}
	}

	sbFile->hash = hash;

clean:

	CharString_free(&tmp, alloc);
	ListU32_free(&tmpArray, alloc);

	if(!s_uccess && pushedVar)
		ListSBVar_popBack(&sbFile->vars, NULL, NULL);

	if(!s_uccess && pushedName)
		DLFile_remove(&sbFile->names, sbFile->names.entryStrings.length - 1, alloc, NULL);

	return s_uccess;
}
