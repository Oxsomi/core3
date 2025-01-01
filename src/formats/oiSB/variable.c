/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#ifndef DISALLOW_SB_OXC3_PLATFORMS
	#include "platforms/ext/listx_impl.h"
#else
	#include "types/container/list_impl.h"
#endif

#include "formats/oiSB/sb_file.h"

TListImpl(SBStruct);
TListImpl(SBVar);

const C8 *ESBType_names[] = {

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16", "F16x2", "F16x3", "F16x4",
	"I16", "I16x2", "I16x3", "I16x4",
	"U16", "U16x2", "U16x3", "U16x4",

	"", "", "", "",
	"F32", "F32x2", "F32x3", "F32x4",
	"I32", "I32x2", "I32x3", "I32x4",
	"U32", "U32x2", "U32x3", "U32x4",

	"", "", "", "",
	"F64", "F64x2", "F64x3", "F64x4",
	"I64", "I64x2", "I64x3", "I64x4",
	"U64", "U64x2", "U64x3", "U64x4",

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16x1x2", "F16x2x2", "F16x3x2", "F16x4x2",
	"I16x1x2", "I16x2x2", "I16x3x2", "I16x4x2",
	"U16x1x2", "U16x2x2", "U16x3x2", "U16x4x2",

	"", "", "", "",
	"F32x1x2", "F32x2x2", "F32x3x2", "F32x4x2",
	"I32x1x2", "I32x2x2", "I32x3x2", "I32x4x2",
	"U32x1x2", "U32x2x2", "U32x3x2", "U32x4x2",

	"", "", "", "",
	"F64x1x2", "F64x2x2", "F64x3x2", "F64x4x2",
	"I64x1x2", "I64x2x2", "I64x3x2", "I64x4x2",
	"U64x1x2", "U64x2x2", "U64x3x2", "U64x4x2",

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16x1x3", "F16x2x3", "F16x3x3", "F16x4x3",
	"I16x1x3", "I16x2x3", "I16x3x3", "I16x4x3",
	"U16x1x3", "U16x2x3", "U16x3x3", "U16x4x3",

	"", "", "", "",
	"F32x1x3", "F32x2x3", "F32x3x3", "F32x4x3",
	"I32x1x3", "I32x2x3", "I32x3x3", "I32x4x3",
	"U32x1x3", "U32x2x3", "U32x3x3", "U32x4x3",

	"", "", "", "",
	"F64x1x3", "F64x2x3", "F64x3x3", "F64x4x3",
	"I64x1x3", "I64x2x3", "I64x3x3", "I64x4x3",
	"U64x1x3", "U64x2x3", "U64x3x3", "U64x4x3",

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16x1x4", "F16x2x4", "F16x3x4", "F16x4x4",
	"I16x1x4", "I16x2x4", "I16x3x4", "I16x4x4",
	"U16x1x4", "U16x2x4", "U16x3x4", "U16x4x4",

	"", "", "", "",
	"F32x1x4", "F32x2x4", "F32x3x4", "F32x4x4",
	"I32x1x4", "I32x2x4", "I32x3x4", "I32x4x4",
	"U32x1x4", "U32x2x4", "U32x3x4", "U32x4x4",

	"", "", "", "",
	"F64x1x4", "F64x2x4", "F64x3x4", "F64x4x4",
	"I64x1x4", "I64x2x4", "I64x3x4", "I64x4x4",
	"U64x1x4", "U64x2x4", "U64x3x4", "U64x4x4"
};

ESBVector ESBType_getVector(ESBType type) {
	return type & 3;
}

ESBMatrix ESBType_getMatrix(ESBType type) {
	return (type >> 6) & 3;
}

ESBPrimitive ESBType_getPrimitive(ESBType type) {
	return (type >> 2) & 3;
}

ESBStride ESBType_getStride(ESBType type) {
	return (type >> 4) & 3;
}

U8 ESBType_getSize(ESBType type, Bool isPacked) {

	U8 primitiveSize = 1 << ESBType_getStride(type);
	U8 w = (U8) ESBType_getVector(type) + 1;
	U8 h = (U8) ESBType_getMatrix(type) + 1;

	if(isPacked)
		return primitiveSize * w * h;

	U8 realStride = w * primitiveSize;
	U8 stride = (realStride + 15) &~ 15;
	return stride * (h - 1) + realStride;
}

const C8 *ESBType_name(ESBType type) {
	return ESBType_names[type];
}

Bool SBFile_addStruct(SBFile *sbFile, CharString *name, SBStruct sbStruct, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	Bool pushedStruct = false;
	CharString tmp = CharString_createNull();

	if(!sbFile || !name)
		retError(clean, Error_nullPointer(!sbFile ? 0 : 1, "SBFile_addStruct()::sbFile and name are required"))

	if(!sbStruct.stride)
		retError(clean, Error_invalidParameter(
			2, 0, "SBFile_addStruct()::sbStruct.stride is required"
		))

	if(CharString_length(*name) >= U32_MAX)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_addStruct()::*name must be less than 4Gi of string"))

	if(sbFile->structs.length >= (U16)(U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->structs.length, U16_MAX, "SBFile_addStruct()::sbFile->structs.length limited to 65535"
		))

	U64 hash = sbFile->hash;
	hash = Buffer_fnv1a64Single(sbStruct.stride | ((U64)CharString_length(*name) << 32), hash);
	sbFile->hash = Buffer_fnv1a64(CharString_bufferConst(*name), hash);

	gotoIfError2(clean, ListSBStruct_pushBack(&sbFile->structs, sbStruct, alloc))
	pushedStruct = true;

	if(CharString_isRef(*name))
		gotoIfError2(clean, CharString_createCopy(*name, alloc, &tmp))

	if(!Buffer_isAscii(CharString_bufferConst(*name)))
		sbFile->flags |= ESBSettingsFlags_IsUTF8;

	gotoIfError2(clean, ListCharString_pushBack(&sbFile->structNames, tmp.ptr ? tmp : *name, alloc))
	pushedStruct = false;
	*name = tmp = CharString_createNull();

clean:

	CharString_free(&tmp, alloc);

	if(pushedStruct)
		ListSBStruct_popBack(&sbFile->structs, NULL);

	return s_uccess;
}

void SBVar_applyHash(U64 *hashRes, SBVar var, CharString name) {

	const void *structId = &var.structId;				//Interpreted as a U64 and U32
	const U64 *structIdU64 = (const U64*) structId;		//[0] = structId, arrayIndex, offset
	const U32 *structIdU32 = (const U32*) structId;		//[2] = type, flags, parentId

	U64 hash = *hashRes;
	hash = Buffer_fnv1a64Single(structIdU64[0], hash);
	hash = Buffer_fnv1a64Single(structIdU32[2] | ((U64)CharString_length(name) << 32), hash);
	*hashRes = Buffer_fnv1a64(CharString_bufferConst(name), hash);
}

void ListU32_applyHash(U64 *hashRes, ListU32 array) {

	U64 hash = Buffer_fnv1a64Single(array.length, *hashRes);

	const void *arrayPtr = array.ptr;
	const U64 *arrayU64 = (const U64*) arrayPtr;

	for(U64 i = 0, j = array.length >> 1; i < j; ++i)
		hash = Buffer_fnv1a64Single(arrayU64[j], hash);

	if (array.length & 1)
		hash = Buffer_fnv1a64Single(array.ptr[array.length - 1], hash);

	*hashRes = hash;
}

Bool SBFile_addVariableAsType(
	SBFile *sbFile,
	CharString *name,
	U32 offset,
	U16 parentId,
	ESBType type,
	ESBVarFlag flags,
	ListU32 *arrays,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool pushedName = false;
	Bool pushedVar = false;
	CharString tmp = CharString_createNull();
	ListU32 tmpArray = (ListU32) { 0 };

	if(!sbFile || !name)
		retError(clean, Error_nullPointer(!sbFile ? 0 : 1, "SBFile_addVariableAsType()::sbFile and name are required"))

	if(flags & ESBVarFlag_Invalid)
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsType()::flags is invalid"))

	if(arrays && arrays->length > 32)
		retError(clean, Error_outOfBounds(6, arrays->length, 32, "SBFile_addVariableAsType()::arrays.length limited to 32"))

	if(arrays && !arrays->length)
		retError(clean, Error_invalidState(0, "SBFile_addVariableAsType()::arrays should be NULL if the array is empty"))

	if(ESBType_getPrimitive(type) == ESBPrimitive_Invalid || (type >> 8) || (
		ESBType_getPrimitive(type) == ESBPrimitive_Float && ESBType_getStride(type) == ESBStride_X8
	))
		retError(clean, Error_invalidParameter(4, 0, "SBFile_addVariableAsType()::type is invalid"))

	if(sbFile->vars.length >= (U16)(U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->vars.length, U16_MAX, "SBFile_addVariableAsType()::sbFile->vars.length limited to 65535"
		))

	if(arrays && sbFile->arrays.length >= (U16)(U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->arrays.length, U16_MAX, "SBFile_addVariableAsType()::sbFile->arrays.length limited to 65535"
		))

	if(CharString_length(*name) >= U32_MAX)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_addVariableAsType()::*name must be less than 4Gi of string"))

	//Padding happens each element of an array that isn't 16-byte aligned.
	//The last element doesn't have padding

	Bool isTightlyPacked = sbFile->flags & ESBSettingsFlags_IsTightlyPacked;
	U32 size = ESBType_getSize(type, isTightlyPacked);
	U8 typeSize = 1 << ESBType_getStride(type);

	if(!isTightlyPacked && ((offset + size - 1) >> 4) != (offset >> 4) && (offset & 15))
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsType()::offset spans 16 bytes, not tightly packed"))

	if(isTightlyPacked && (offset & (typeSize - 1)))
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsType()::offset doesn't follow req type alignment"))

	U64 totalSizeBytes = isTightlyPacked ? size : (size + 15) &~ 15;

	for (U64 i = 0; i < (arrays ? arrays->length : 0); ++i) {

		U32 arrayi = arrays->ptr[i];

		if(!arrayi)
			retError(clean, Error_invalidParameter(0, 0, "SBFile_addVariableAsType()::arrays[i] is 0"))

		totalSizeBytes *= arrayi;

		if(totalSizeBytes > U32_MAX)
			retError(clean, Error_outOfBounds(
				0, totalSizeBytes, U32_MAX,
				"SBFile_addVariableAsType()::arrays.length bytes out of bounds (only 2^32 permitted)"
			))
	}

	if(!isTightlyPacked && (size & 15))
		totalSizeBytes -= 16 - (size & 15);

	size = (U32) totalSizeBytes;

	//Validate parent

	if (parentId == U16_MAX) {
		if(offset + size > sbFile->bufferSize)
			retError(clean, Error_outOfBounds(
				0, offset, sbFile->bufferSize, "SBFile_addVariableAsType()::offset + size is out of bounds"
			))
	}

	else {

		if(parentId >= sbFile->vars.length)
			retError(clean, Error_outOfBounds(
				0, parentId, sbFile->vars.length, "SBFile_addVariableAsType()::parentId is out of bounds"
			))

		SBVar var = sbFile->vars.ptr[parentId];

		if(var.type)
			retError(clean, Error_invalidState(0, "SBFile_addVariableAsType()::sbFile->vars.ptr[parentId] isn't a struct"))

		SBStruct strc = sbFile->structs.ptr[var.structId];

		if(isTightlyPacked && (var.offset & (typeSize - 1)) && ((var.offset + strc.stride) & (typeSize - 1)))
			retError(clean, Error_invalidState(0, "SBFile_addVariableAsType() parent struct doesn't respect alignment"))
	}

	for(U64 i = 0; i < sbFile->vars.length; ++i) {

		SBVar vari = sbFile->vars.ptr[i];

		if(vari.parentId != parentId)
			continue;

		if(CharString_equalsStringSensitive(sbFile->varNames.ptr[i], *name))
			retError(clean, Error_invalidState(0, "SBFile_addVariableAsType() parent already contains this member name"))
	}

	Bool reuseArray = false;
	U16 arrayId = arrays ? (U16) sbFile->arrays.length : U16_MAX;

	if (arrays)
		for(U64 i = 0; i < sbFile->arrays.length; ++i)
			if (ListU32_eq(*arrays, sbFile->arrays.ptr[i])) {
				arrayId = (U16) i;
				reuseArray = true;
				ListU32_free(arrays, alloc);
				break;
			}

	//Add to array & vars

	SBVar var = (SBVar) {
		.structId = U16_MAX,
		.arrayIndex = arrayId,
		.offset = offset,
		.type = type,
		.flags = flags,
		.parentId = parentId
	};

	SBVar_applyHash(&sbFile->hash, var, *name);

	gotoIfError2(clean, ListSBVar_pushBack(&sbFile->vars, var, alloc))
	pushedVar = true;

	if(CharString_isRef(*name))
		gotoIfError2(clean, CharString_createCopy(*name, alloc, &tmp))

	if(!Buffer_isAscii(CharString_bufferConst(*name)))
		sbFile->flags |= ESBSettingsFlags_IsUTF8;

	gotoIfError2(clean, ListCharString_pushBack(&sbFile->varNames, tmp.ptr ? tmp : *name, alloc))
	*name = tmp = CharString_createNull();
	pushedVar = true;

	if(!reuseArray) {

		if(arrays && ListU32_isRef(*arrays))
			gotoIfError2(clean, ListU32_createCopy(*arrays, alloc, &tmpArray))

		if (arrays) {
			ListU32_applyHash(&sbFile->hash, tmpArray.ptr ? tmpArray : *arrays);
			gotoIfError2(clean, ListListU32_pushBack(&sbFile->arrays, tmpArray.ptr ? tmpArray : *arrays, alloc))
			*arrays = tmpArray = (ListU32) { 0 };
		}
	}

	pushedName = false;
	pushedVar = false;

clean:

	CharString_free(&tmp, alloc);
	ListU32_free(&tmpArray, alloc);

	if(pushedVar)
		ListSBVar_popBack(&sbFile->vars, NULL);

	if(pushedName)
		ListCharString_popBack(&sbFile->varNames, NULL);

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
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool pushedVar = false;
	Bool pushedName = true;
	CharString tmp = CharString_createNull();
	ListU32 tmpArray = (ListU32) { 0 };

	if(!sbFile || !name)
		retError(clean, Error_nullPointer(!sbFile ? 0 : 1, "SBFile_addVariableAsStruct()::sbFile and name are required"))

	if(flags & ESBVarFlag_Invalid)
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsStruct()::flags is invalid"))

	if(arrays && arrays->length > 32)
		retError(clean, Error_outOfBounds(6, arrays->length, 32, "SBFile_addVariableAsStruct()::arrays.length limited to 32"))

	if(arrays && !arrays->length)
		retError(clean, Error_invalidState(0, "SBFile_addVariableAsStruct()::arrays should be NULL if the array is empty"))

	if(structId >= sbFile->structs.length)
		retError(clean, Error_outOfBounds(
			0, structId, sbFile->structs.length, "SBFile_addVariableAsStruct()::structId out of bounds"
		))

	if(sbFile->vars.length >= (U16)(U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->vars.length, U16_MAX, "SBFile_addVariableAsStruct()::vars.length limited to 65535"
		))

	if(arrays && sbFile->arrays.length >= (U16)(U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->arrays.length, U16_MAX, "SBFile_addVariableAsStruct()::arrays.length limited to 65535"
		))

	if(CharString_length(*name) >= U32_MAX)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_addVariableAsStruct()::*name must be less than 4Gi of string"))

	//Alignment: starts at 16 byte boundary if CBuffer

	Bool isTightlyPacked = sbFile->flags & ESBSettingsFlags_IsTightlyPacked;

	if(!isTightlyPacked && (offset & 15))
		retError(clean, Error_invalidParameter(5, 0, "SBFile_addVariableAsStruct()::offset needs 16-byte alignment"))

	SBStruct strc = sbFile->structs.ptr[structId];

	U64 size = strc.stride;

	for (U64 i = 0; i < (arrays ? arrays->length : 0); ++i) {

		U32 arrayi = arrays->ptr[i];

		if(!arrayi)
			retError(clean, Error_invalidParameter(0, 0, "SBFile_addVariableAsStruct()::arrays[i] is 0"))

		size *= arrayi;

		if(size > U32_MAX)
			retError(clean, Error_outOfBounds(
				0, size, U32_MAX,
				"SBFile_addVariableAsStruct()::arrays.length bytes out of bounds (only 2^32 permitted)"
			))
	}

	//Validate parent

	if (parentId == U16_MAX) {
		if(offset + size > sbFile->bufferSize)
			retError(clean, Error_outOfBounds(
				0, offset, sbFile->bufferSize, "SBFile_addVariableAsStruct()::offset + size is out of bounds"
			))
	}

	else {

		if(parentId >= sbFile->vars.length)
			retError(clean, Error_outOfBounds(
				0, parentId, sbFile->vars.length, "SBFile_addVariableAsStruct()::parentId is out of bounds"
			))

		SBVar var = sbFile->vars.ptr[parentId];

		if(var.type)
			retError(clean, Error_invalidState(0, "SBFile_addVariableAsStruct()::sbFile->vars.ptr[parentId] isn't a struct"))
	}

	for(U64 i = 0; i < sbFile->vars.length; ++i) {

		SBVar vari = sbFile->vars.ptr[i];

		if(vari.parentId != parentId)
			continue;

		if(CharString_equalsStringSensitive(sbFile->varNames.ptr[i], *name))
			retError(clean, Error_invalidState(0, "SBFile_addVariableAsStruct() parent already contains this member name"))
	}

	Bool reuseArray = false;
	U16 arrayId = arrays ? (U16) sbFile->arrays.length : U16_MAX;

	if (arrays)
		for(U64 i = 0; i < sbFile->arrays.length; ++i)
			if (ListU32_eq(*arrays, sbFile->arrays.ptr[i])) {
				arrayId = (U16) i;
				reuseArray = true;
				ListU32_free(arrays, alloc);
				break;
			}

	//Add to array & vars

	SBVar var = (SBVar) {
		.structId = structId,
		.arrayIndex = arrayId,
		.offset = offset,
		.type = 0,
		.flags = flags,
		.parentId = parentId
	};

	SBVar_applyHash(&sbFile->hash, var, *name);

	gotoIfError2(clean, ListSBVar_pushBack(&sbFile->vars, var, alloc))
	pushedVar = true;

	if(CharString_isRef(*name))
		gotoIfError2(clean, CharString_createCopy(*name, alloc, &tmp))

	if(!Buffer_isAscii(CharString_bufferConst(*name)))
		sbFile->flags |= ESBSettingsFlags_IsUTF8;

	gotoIfError2(clean, ListCharString_pushBack(&sbFile->varNames, tmp.ptr ? tmp : *name, alloc))
	*name = tmp = CharString_createNull();
	pushedVar = true;

	if(!reuseArray) {

		if(arrays && ListU32_isRef(*arrays))
			gotoIfError2(clean, ListU32_createCopy(*arrays, alloc, &tmpArray))

		if (arrays) {
			ListU32_applyHash(&sbFile->hash, tmpArray.ptr ? tmpArray : *arrays);
			gotoIfError2(clean, ListListU32_pushBack(&sbFile->arrays, tmpArray.ptr ? tmpArray : *arrays, alloc))
			*arrays = tmpArray = (ListU32) { 0 };
		}
	}

	pushedName = false;
	pushedVar = false;

clean:

	CharString_free(&tmp, alloc);
	ListU32_free(&tmpArray, alloc);

	if(pushedVar)
		ListSBVar_popBack(&sbFile->vars, NULL);

	if(pushedName)
		ListCharString_popBack(&sbFile->varNames, NULL);

	return s_uccess;
}
