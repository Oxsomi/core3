/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/list_impl.h"
#include "formats/oiSB.h"
#include "formats/oiDL.h"

TListImpl(SBStruct);
TListImpl(SBVar);

const C8 *ESBType_names[] = {

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16",
	"F16x2",
	"F16x3",
	"F16x4",
	"I16",
	"I16x2",
	"I16x3",
	"I16x4",
	"U16",
	"U16x2",
	"U16x3",
	"U16x4",

	"", "", "", "",
	"F32",
	"F32x2",
	"F32x3",
	"F32x4",
	"I32",
	"I32x2",
	"I32x3",
	"I32x4",
	"U32",
	"U32x2",
	"U32x3",
	"U32x4",

	"", "", "", "",
	"F64",
	"F64x2",
	"F64x3",
	"F64x4",
	"I64",
	"I64x2",
	"I64x3",
	"I64x4",
	"U64",
	"U64x2",
	"U64x3",
	"U64x4",

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16x1x2",
	"F16x2x2",
	"F16x3x2",
	"F16x4x2",
	"I16x1x2",
	"I16x2x2",
	"I16x3x2",
	"I16x4x2",
	"U16x1x2",
	"U16x2x2",
	"U16x3x2",
	"U16x4x2",

	"", "", "", "",
	"F32x1x2",
	"F32x2x2",
	"F32x3x2",
	"F32x4x2",
	"I32x1x2",
	"I32x2x2",
	"I32x3x2",
	"I32x4x2",
	"U32x1x2",
	"U32x2x2",
	"U32x3x2",
	"U32x4x2",

	"", "", "", "",
	"F64x1x2",
	"F64x2x2",
	"F64x3x2",
	"F64x4x2",
	"I64x1x2",
	"I64x2x2",
	"I64x3x2",
	"I64x4x2",
	"U64x1x2",
	"U64x2x2",
	"U64x3x2",
	"U64x4x2",

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16x1x3",
	"F16x2x3",
	"F16x3x3",
	"F16x4x3",
	"I16x1x3",
	"I16x2x3",
	"I16x3x3",
	"I16x4x3",
	"U16x1x3",
	"U16x2x3",
	"U16x3x3",
	"U16x4x3",

	"", "", "", "",
	"F32x1x3",
	"F32x2x3",
	"F32x3x3",
	"F32x4x3",
	"I32x1x3",
	"I32x2x3",
	"I32x3x3",
	"I32x4x3",
	"U32x1x3",
	"U32x2x3",
	"U32x3x3",
	"U32x4x3",

	"", "", "", "",
	"F64x1x3",
	"F64x2x3",
	"F64x3x3",
	"F64x4x3",
	"I64x1x3",
	"I64x2x3",
	"I64x3x3",
	"I64x4x3",
	"U64x1x3",
	"U64x2x3",
	"U64x3x3",
	"U64x4x3",

	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",

	"", "", "", "",
	"F16x1x4",
	"F16x2x4",
	"F16x3x4",
	"F16x4x4",
	"I16x1x4",
	"I16x2x4",
	"I16x3x4",
	"I16x4x4",
	"U16x1x4",
	"U16x2x4",
	"U16x3x4",
	"U16x4x4",

	"", "", "", "",
	"F32x1x4",
	"F32x2x4",
	"F32x3x4",
	"F32x4x4",
	"I32x1x4",
	"I32x2x4",
	"I32x3x4",
	"I32x4x4",
	"U32x1x4",
	"U32x2x4",
	"U32x3x4",
	"U32x4x4",

	"", "", "", "",
	"F64x1x4",
	"F64x2x4",
	"F64x3x4",
	"F64x4x4",
	"I64x1x4",
	"I64x2x4",
	"I64x3x4",
	"I64x4x4",
	"U64x1x4",
	"U64x2x4",
	"U64x3x4",
	"U64x4x4"
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

TListImpl(SBFile);

Bool SBFile_create(
	ESBSettingsFlags flags,
	U32 bufferSize,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;

	if(!sbFile)
		retError(clean, Error_nullPointer(0, "SBFile_create()::sbFile is required"))

	if(sbFile->vars.length)
		retError(clean, Error_invalidOperation(0, "SBFile_create()::sbFile isn't empty, might indicate memleak"))

	if(flags & ESBSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "SBFile_create()::flags contained unsupported flag"))

	if(!bufferSize)
		retError(clean, Error_invalidParameter(1, 0, "SBFile_create()::bufferSize is required"))

	gotoIfError2(clean, ListSBStruct_reserve(&sbFile->structs, 16, alloc))
	gotoIfError2(clean, ListSBVar_reserve(&sbFile->vars, 16, alloc))
	gotoIfError2(clean, ListCharString_reserve(&sbFile->structNames, 16, alloc))
	gotoIfError2(clean, ListCharString_reserve(&sbFile->varNames, 16, alloc))
	gotoIfError2(clean, ListListU32_reserve(&sbFile->arrays, 16, alloc))

	sbFile->flags = flags;
	sbFile->bufferSize = bufferSize;

clean:
	return s_uccess;
}

void SBFile_free(SBFile *sbFile, Allocator alloc) {

	if(!sbFile)
		return;

	ListSBStruct_free(&sbFile->structs, alloc);
	ListSBVar_free(&sbFile->vars, alloc);
	ListCharString_freeUnderlying(&sbFile->structNames, alloc);
	ListCharString_freeUnderlying(&sbFile->varNames, alloc);
	ListListU32_freeUnderlying(&sbFile->arrays, alloc);
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

	if(sbFile->structs.length >= (U16)(U16_MAX - 1))
		retError(clean, Error_outOfBounds(
			0, sbFile->structs.length, U16_MAX, "SBFile_addStruct()::sbFile->structs.length limited to 65535"
		))

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

	//Add to array & vars

	SBVar var = (SBVar) {
		.structId = U16_MAX,
		.arrayIndex = arrays ? (U16) sbFile->arrays.length : U16_MAX,
		.offset = offset,
		.type = type,
		.flags = flags,
		.parentId = parentId
	};

	gotoIfError2(clean, ListSBVar_pushBack(&sbFile->vars, var, alloc))
	pushedVar = true;

	if(CharString_isRef(*name))
		gotoIfError2(clean, CharString_createCopy(*name, alloc, &tmp))

	if(!Buffer_isAscii(CharString_bufferConst(*name)))
		sbFile->flags |= ESBSettingsFlags_IsUTF8;
		
	gotoIfError2(clean, ListCharString_pushBack(&sbFile->varNames, tmp.ptr ? tmp : *name, alloc))
	*name = tmp = CharString_createNull();
	pushedVar = true;

	if(arrays && ListU32_isRef(*arrays))
		gotoIfError2(clean, ListU32_createCopy(*arrays, alloc, &tmpArray))

	if (arrays) {
		gotoIfError2(clean, ListListU32_pushBack(&sbFile->arrays, tmpArray.ptr ? tmpArray : *arrays, alloc))
		*arrays = tmpArray = (ListU32) { 0 };
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

	//Add to array & vars

	SBVar var = (SBVar) {
		.structId = structId,
		.arrayIndex = arrays ? (U16) sbFile->arrays.length : U16_MAX,
		.offset = offset,
		.type = 0,
		.flags = flags,
		.parentId = parentId
	};

	gotoIfError2(clean, ListSBVar_pushBack(&sbFile->vars, var, alloc))
	pushedVar = true;

	if(CharString_isRef(*name))
		gotoIfError2(clean, CharString_createCopy(*name, alloc, &tmp))
		
	if(!Buffer_isAscii(CharString_bufferConst(*name)))
		sbFile->flags |= ESBSettingsFlags_IsUTF8;

	gotoIfError2(clean, ListCharString_pushBack(&sbFile->varNames, tmp.ptr ? tmp : *name, alloc))
	*name = tmp = CharString_createNull();
	pushedVar = true;

	if(arrays && ListU32_isRef(*arrays))
		gotoIfError2(clean, ListU32_createCopy(*arrays, alloc, &tmpArray))

	if (arrays) {
		gotoIfError2(clean, ListListU32_pushBack(&sbFile->arrays, tmpArray.ptr ? tmpArray : *arrays, alloc))
		*arrays = tmpArray = (ListU32) { 0 };
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

static const U8 SBHeader_V1_2 = 2;

Bool SBFile_write(SBFile sbFile, Allocator alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!sbFile.vars.ptr)
		retError(clean, Error_invalidOperation(0, "SBFile_write()::sbFile is undefined"))

	if(!result)
		retError(clean, Error_nullPointer(2, "SBFile_write()::result is required"))

	if(result->ptr)
		retError(clean, Error_invalidOperation(0, "SBFile_write()::result wasn't empty, might indicate memleak"))
		
	//Get header size

	U64 headerSize = sizeof(SBHeader);

	if(!(sbFile.flags & ESBSettingsFlags_HideMagicNumber))		//Magic number (can be hidden by parent; such as oiSC)
		headerSize += sizeof(U32);

	headerSize += ListSBStruct_bytes(sbFile.structs);
	headerSize += ListSBVar_bytes(sbFile.vars);
	headerSize += sbFile.arrays.length;

	for(U64 i = 0; i < sbFile.arrays.length; ++i)
		headerSize += sbFile.arrays.ptr[i].length * sizeof(U32);

	Bool isUTF8 = sbFile.flags & ESBSettingsFlags_IsUTF8;

	DLSettings settings = (DLSettings) {
		.dataType = isUTF8 ? EDLDataType_UTF8 : EDLDataType_Ascii,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	DLFile dlFile = (DLFile) { 0 };
	Buffer dlFileBuf = Buffer_createNull();
	gotoIfError3(clean, DLFile_create(settings, alloc, &dlFile, e_rr))

	for (U64 i = 0; i < sbFile.structNames.length; ++i) {

		if(isUTF8)
			gotoIfError3(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(sbFile.structNames.ptr[i]), alloc, e_rr))

		else gotoIfError3(clean, DLFile_addEntryAscii(
			&dlFile, CharString_createRefStrConst(sbFile.structNames.ptr[i]), alloc, e_rr
		))
	}

	for (U64 i = 0; i < sbFile.varNames.length; ++i) {

		if(isUTF8)
			gotoIfError3(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(sbFile.varNames.ptr[i]), alloc, e_rr))

		else gotoIfError3(clean, DLFile_addEntryAscii(
			&dlFile, CharString_createRefStrConst(sbFile.varNames.ptr[i]), alloc, e_rr
		))
	}

	gotoIfError3(clean, DLFile_write(dlFile, alloc, &dlFileBuf, e_rr))

	U64 len = Buffer_length(dlFileBuf) + headerSize;

	//Create buffer

	gotoIfError2(clean, Buffer_createUninitializedBytes(len, alloc, result))

	//Prepend header

	U8 *headerIt = (U8*)result->ptr;

	if (!(sbFile.flags & ESBSettingsFlags_HideMagicNumber)) {
		*(U32*)headerIt = SBHeader_MAGIC;
		headerIt += sizeof(U32);
	}

	SBHeader *sbHeader = (SBHeader*) headerIt;

	*sbHeader = (SBHeader) {

		.version = SBHeader_V1_2,
		.flags = sbFile.flags & ESBSettingsFlags_IsTightlyPacked ? ESBFlag_IsTightlyPacked : ESBFlag_None,
		.arrays = (U16) sbFile.arrays.length,

		.structs = (U16) sbFile.structs.length,
		.vars = (U16) sbFile.vars.length,

		.bufferSize = sbFile.bufferSize
	};

	headerIt += sizeof(SBHeader);

	Buffer_copy(Buffer_createRef(headerIt, Buffer_length(dlFileBuf)), dlFileBuf);
	headerIt += Buffer_length(dlFileBuf);

	SBStruct *structs = (SBStruct*) headerIt;

	Buffer_copy(Buffer_createRef(structs, ListSBStruct_bytes(sbFile.structs)), ListSBStruct_bufferConst(sbFile.structs));
	SBVar *vars = (SBVar*) (structs + sbFile.structs.length);

	Buffer_copy(Buffer_createRef(vars, ListSBVar_bytes(sbFile.vars)), ListSBVar_bufferConst(sbFile.vars));
	U8 *arrayDimCount = (U8*) (vars + sbFile.vars.length);
	U32 *arrayDims = (U32*) (arrayDimCount + sbFile.arrays.length);

	for(U64 i = 0, j = 0; i < sbFile.arrays.length; ++i) {

		arrayDimCount[i] = (U8)sbFile.arrays.ptr[i].length;

		for(U64 k = 0; k < arrayDimCount[i]; ++k, ++j)
			arrayDims[j] = sbFile.arrays.ptr[i].ptr[k];
	}

clean:
	Buffer_free(&dlFileBuf, alloc);
	DLFile_free(&dlFile, alloc);
	return s_uccess;
}

Bool SBFile_read(Buffer file, Bool isSubFile, Allocator alloc, SBFile *result, Error *e_rr) {
	
	Bool s_uccess = true;
	Bool didAllocate = false;
	DLFile strings = (DLFile) { 0 };

	if(!result)
		retError(clean, Error_nullPointer(0, "SBFile_read()::result is required"))

	if(result->vars.ptr)
		retError(clean, Error_invalidParameter(4, 0, "SBFile_read()::result is already present, possible memleak"))

	Buffer tmpFile = Buffer_createRefFromBuffer(file, true);

	if(!isSubFile) {

		U32 magic = 0;
		gotoIfError2(clean, Buffer_consumeU32(&tmpFile, &magic))

		if(magic != SBHeader_MAGIC)
			retError(clean, Error_invalidState(0, "SBFile_read()::file didn't start with oiSB"))
	}

	SBHeader header;
	gotoIfError2(clean, Buffer_consume(&tmpFile, &header, sizeof(header)))

	if(header.version != SBHeader_V1_2 || (header.flags & ESBFlag_Unsupported) || !header.vars)
		retError(clean, Error_invalidState(0, "SBFile_read()::file didn't have right version, flags or was empty"))

	ESBSettingsFlags flags = 
		(isSubFile ? ESBSettingsFlags_HideMagicNumber : ESBSettingsFlags_None) |
		((header.flags & ESBFlag_IsTightlyPacked) ? ESBSettingsFlags_IsTightlyPacked : ESBSettingsFlags_None);

	gotoIfError3(clean, SBFile_create(flags, header.bufferSize, alloc, result, e_rr))
	didAllocate = true;

	gotoIfError3(clean, DLFile_read(tmpFile, NULL, true, alloc, &strings, e_rr))
	
	U64 stringCount = (U64)header.structs + header.vars;

	if(
		strings.entryBuffers.length != stringCount ||
		strings.settings.flags & EDLSettingsFlags_UseSHA256 ||
		strings.settings.dataType == EDLDataType_Data ||
		strings.settings.encryptionType ||
		strings.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SBFile_read() dlFile didn't match expectations"))

	gotoIfError2(clean, Buffer_offset(&tmpFile, strings.readLength))

	const SBStruct *structs = (const SBStruct*) tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, sizeof(SBStruct) * header.structs))

	for(U64 i = 0; i < header.structs; ++i) {

		CharString tmpName = CharString_createNull();
		CharString *name = &tmpName;

		if(strings.settings.dataType == EDLDataType_Ascii)
			name = &strings.entryStrings.ptrNonConst[i];

		else {
			Buffer buf = strings.entryBuffers.ptr[i];
			tmpName = CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false);
		}

		gotoIfError3(clean, SBFile_addStruct(result, name, structs[i], alloc, e_rr))
	}

	const SBVar *vars = (const SBVar*) tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, sizeof(SBVar) * header.vars))
	
	const U8 *arraySizeCount = tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, header.arrays))

	U64 arrayCounters = 0;
	for(U64 i = 0; i < header.arrays; ++i)
		arrayCounters += arraySizeCount[i];

	const U32 *arraySizes = (const U32*) tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, arrayCounters * sizeof(U32)))

	for(U64 i = 0; i < header.vars; ++i) {

		SBVar vari = vars[i];

		if(vari.arrayIndex != U16_MAX && vari.arrayIndex >= header.arrays)
			retError(clean, Error_outOfBounds(
				0, vari.arrayIndex, header.arrays, "SBFile_read() arrayIndex out of bounds"
			))

		U8 arrayCount = 0;
		U32 tmp[32];	//Worst case array

		if(vari.arrayIndex != U16_MAX) {

			U64 off = 0;

			for(U64 j = 0; j < vari.arrayIndex; ++j)
				off += arraySizeCount[j];

			arrayCount = arraySizeCount[vari.arrayIndex];

			if(!arrayCount || arrayCount > 32)
				retError(clean, Error_outOfBounds(
					0, arrayCount, 32, "SBFile_read() arrayCount out of bounds [1, 32]"
				))

			for(U8 j = 0; j < arrayCount; ++j)
				tmp[j] = arraySizes[off + j];
		}

		ListU32 listArray = (ListU32) { 0 };

		if(arrayCount)
			gotoIfError2(clean, ListU32_createRefConst(tmp, arrayCount, &listArray))

		if(vari.type && vari.structId != U16_MAX)
			retError(clean, Error_invalidState(0, "SBFile_read()::file had a variable with both a type and a structId"))

		if(!vari.type && vari.structId == U16_MAX)
			retError(clean, Error_invalidState(0, "SBFile_read()::file had a variable with neither a type and a structId"))

		CharString tmpName = CharString_createNull();
		CharString *name = &tmpName;

		if(strings.settings.dataType == EDLDataType_Ascii)
			name = &strings.entryStrings.ptrNonConst[i - header.structs];

		else {
			Buffer buf = strings.entryBuffers.ptr[i - header.structs];
			tmpName = CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false);
		}

		if(vari.type)
			gotoIfError3(clean, SBFile_addVariableAsType(
				result,
				name,
				vari.offset, vari.parentId, vari.type, vari.flags,
				arrayCount ? &listArray : NULL,
				alloc, e_rr
			))

		else gotoIfError3(clean, SBFile_addVariableAsStruct(
			result,
			name,
			vari.offset, vari.parentId, vari.structId, vari.flags,
			arrayCount ? &listArray : NULL,
			alloc, e_rr
		))
	}

	if(!isSubFile && Buffer_length(tmpFile))
		retError(clean, Error_invalidState(0, "SBFile_read() file had unrecognized data at the end"))

clean:

	DLFile_free(&strings, alloc);

	if(didAllocate && !s_uccess)
		SBFile_free(result, alloc);

	return s_uccess;
}

//Bool SBFile_combine(SBFile a, SBFile b, Allocator alloc, SBFile *combined, Error *e_rr);	TODO:

void ListSBFile_freeUnderlying(ListSBFile *files, Allocator alloc) {

	if(!files)
		return;

	for(U64 i = 0; i < files->length; ++i)
		SBFile_free(&files->ptrNonConst[i], alloc);

	ListSBFile_free(files, alloc);
}

void SBFile_print(SBFile sbFile, U64 indenting, U16 parent, Bool isRecursive, Allocator alloc) {

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SBFile_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';
	
	for (U64 i = 0; i < sbFile.vars.length; ++i) {

		SBVar var = sbFile.vars.ptr[i];

		if(var.parentId != parent)
			continue;

		CharString varName = sbFile.varNames.ptr[i];
		Bool isArray = var.arrayIndex != U16_MAX;

		CharString typeName = 
			var.structId == U16_MAX ? CharString_createRefCStrConst(ESBType_name((ESBType)var.type)) :
			sbFile.structNames.ptr[var.structId];

		SBStruct strct = (SBStruct) { 0 };

		if(var.structId != U16_MAX)
			strct = sbFile.structs.ptr[var.structId];

		Log_debug(
			alloc,
			!isArray ? ELogOptions_NewLine : ELogOptions_None,
			!strct.stride ? "%s0x%08"PRIx32": %.*s (%s): %.*s" : "%s0x%08"PRIx32": %.*s (%s): %.*s (Stride: %"PRIu32")",
			indent,
			var.offset,
			(int) CharString_length(varName),
			varName.ptr,
			(var.flags & ESBVarFlag_IsUsedVar) ? "Used" : "Unused",
			(int) CharString_length(typeName),
			typeName.ptr,
			strct.stride
		);

		if (isArray) {

			ListU32 array = sbFile.arrays.ptr[var.arrayIndex];

			for(U64 j = 0; j < array.length; ++j)
				Log_debug(
					alloc,
					j + 1 == array.length ? ELogOptions_NewLine : ELogOptions_None,
					"[%"PRIu32"]", array.ptr[j]
				);
		}

		if(isRecursive && strct.stride)
			SBFile_print(sbFile, indenting + 1, (U16) i, true, alloc);
	}
}
