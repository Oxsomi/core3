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

//shader_compiler/compiler_spv_reflect.cpp

#include "shader_compiler/compiler.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/string_read_helper.h"
#include "types/base/allocator.h"

#include "SPIRV-Reflect/spirv_reflect.h"
#include "compiler_spv_internal.hpp"

Bool spvTypeToESBType(SpvReflectTypeDescription *desc, ESBType *type, Error *e_rr) {

	Bool s_uccess = true;
	SpvReflectNumericTraits numeric = desc->traits.numeric;

	ESBPrimitive prim = ESBPrimitive_Invalid;
	ESBStride stride = ESBStride_X8;
	ESBVector vector = ESBVector_N1;
	ESBMatrix matrix = ESBMatrix_N1;

	if(!desc || !type)
		retError(clean, Error_nullPointer(!desc ? 0 : 1, "spvTypeToESBType()::desc and type are required"));

	switch (desc->type_flags) {

		case SPV_REFLECT_TYPE_FLAG_BOOL:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:

			if(numeric.scalar.signedness || numeric.scalar.width != 32)
				retError(clean, Error_unsupportedOperation(
					0, "spvTypeToESBType()::desc has an unrecognized type (signed bool or size != 32)"
				));

			prim = ESBPrimitive_UInt;
			break;

		case SPV_REFLECT_TYPE_FLAG_INT:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:
			prim = numeric.scalar.signedness ? ESBPrimitive_Int : ESBPrimitive_UInt;
			break;

		case SPV_REFLECT_TYPE_FLAG_FLOAT:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:

			prim = ESBPrimitive_Float;

			if(numeric.scalar.signedness || (
				numeric.scalar.width != 16 &&
				numeric.scalar.width != 32 &&
				numeric.scalar.width != 64
			))
				retError(clean, Error_unsupportedOperation(
					0, "spvTypeToESBType()::desc has an unrecognized type (signed floatXX or size != [16, 32, 64])"
				));

			break;

		default:
			retError(clean, Error_unsupportedOperation(0, "spvTypeToESBType()::desc has an unrecognized type"));
	}

	switch(numeric.scalar.width) {

		case  8:  stride = ESBStride_X8;   break;
		case 16:  stride = ESBStride_X16;  break;
		case 32:  stride = ESBStride_X32;  break;
		case 64:  stride = ESBStride_X64;  break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (8, 16, 32, 64)"
			));
	}

	switch(numeric.matrix.column_count ? numeric.matrix.column_count : numeric.vector.component_count) {

		case 0:
		case 1:  vector = ESBVector_N1;  break;

		case 2:  vector = ESBVector_N2;  break;
		case 3:  vector = ESBVector_N3;  break;
		case 4:  vector = ESBVector_N4;  break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (vecN)"
			));
	}

	switch(numeric.matrix.row_count) {

		case 0:
		case 1:  matrix = ESBMatrix_N1;  break;

		case 2:  matrix = ESBMatrix_N2;  break;
		case 3:  matrix = ESBMatrix_N3;  break;
		case 4:  matrix = ESBMatrix_N4;  break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (matWxH)"
			));
	}

	if(numeric.matrix.stride && numeric.matrix.stride != 0x10)
		retError(clean, Error_unsupportedOperation(0, "spvTypeToESBType()::desc has matrix with stride != 16"));

	*type = (ESBType) ESBType_create(stride, prim, vector, matrix);

clean:
	return s_uccess;
}

Bool SpvReflectFormatToESBType(SpvReflectFormat format, ESBType *type, Error *e_rr) {

	Bool s_uccess = true;

	switch (format) {

		case SPV_REFLECT_FORMAT_R16_UINT:             *type = ESBType_U16;    break;
		case SPV_REFLECT_FORMAT_R16_SINT:             *type = ESBType_I16;    break;
		case SPV_REFLECT_FORMAT_R16_SFLOAT:           *type = ESBType_F16;    break;

		case SPV_REFLECT_FORMAT_R16G16_UINT:          *type = ESBType_U16x2;  break;
		case SPV_REFLECT_FORMAT_R16G16_SINT:          *type = ESBType_I16x2;  break;
		case SPV_REFLECT_FORMAT_R16G16_SFLOAT:        *type = ESBType_F16x2;  break;

		case SPV_REFLECT_FORMAT_R16G16B16_UINT:       *type = ESBType_U16x3;  break;
		case SPV_REFLECT_FORMAT_R16G16B16_SINT:       *type = ESBType_I16x3;  break;
		case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:     *type = ESBType_F16x3;  break;

		case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:    *type = ESBType_U16x4;  break;
		case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:    *type = ESBType_I16x4;  break;
		case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:  *type = ESBType_F16x4;  break;

		case SPV_REFLECT_FORMAT_R32_UINT:             *type = ESBType_U32;    break;
		case SPV_REFLECT_FORMAT_R32_SINT:             *type = ESBType_I32;    break;
		case SPV_REFLECT_FORMAT_R32_SFLOAT:           *type = ESBType_F32;    break;

		case SPV_REFLECT_FORMAT_R32G32_UINT:          *type = ESBType_U32x2;  break;
		case SPV_REFLECT_FORMAT_R32G32_SINT:          *type = ESBType_I32x2;  break;
		case SPV_REFLECT_FORMAT_R32G32_SFLOAT:        *type = ESBType_F32x2;  break;

		case SPV_REFLECT_FORMAT_R32G32B32_UINT:       *type = ESBType_U32x3;  break;
		case SPV_REFLECT_FORMAT_R32G32B32_SINT:       *type = ESBType_I32x3;  break;
		case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:     *type = ESBType_F32x3;  break;

		case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:    *type = ESBType_U32x4;  break;
		case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:    *type = ESBType_I32x4;  break;
		case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:  *type = ESBType_F32x4;  break;

		case SPV_REFLECT_FORMAT_R64_UINT:             *type = ESBType_U64;    break;
		case SPV_REFLECT_FORMAT_R64_SINT:             *type = ESBType_I64;    break;
		case SPV_REFLECT_FORMAT_R64_SFLOAT:           *type = ESBType_F64;    break;

		case SPV_REFLECT_FORMAT_R64G64_UINT:          *type = ESBType_U64x2;  break;
		case SPV_REFLECT_FORMAT_R64G64_SINT:          *type = ESBType_I64x2;  break;
		case SPV_REFLECT_FORMAT_R64G64_SFLOAT:        *type = ESBType_F64x2;  break;

		case SPV_REFLECT_FORMAT_R64G64B64_UINT:       *type = ESBType_U64x3;  break;
		case SPV_REFLECT_FORMAT_R64G64B64_SINT:       *type = ESBType_I64x3;  break;
		case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:     *type = ESBType_F64x3;  break;

		case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:    *type = ESBType_U64x4;  break;
		case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:    *type = ESBType_I64x4;  break;
		case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:  *type = ESBType_F64x4;  break;

		default:
			retError(clean, Error_invalidState(
				0, "SpvReflectFormatToESBType() couldn't map SPV_REFLECT_FORMAT to ESBType"
			));
	}

clean:
	return s_uccess;
}

Bool SpvCalculateStructLength(const SpvReflectTypeDescription *typeDesc, U64 *result, Error *e_rr) {

	U64 len = 0;
	Bool s_uccess = true;

	for (U64 i = 0; i < typeDesc->member_count; ++i) {

		SpvReflectTypeDescription typeDesck = typeDesc->members[i];

		SpvReflectTypeFlags disallowed =
			SPV_REFLECT_TYPE_FLAG_EXTERNAL_MASK | SPV_REFLECT_TYPE_FLAG_REF | SPV_REFLECT_TYPE_FLAG_VOID;

		if(typeDesck.type_flags & disallowed)
			retError(clean, Error_invalidState(
				0, "SpvCalculateStructLength() can't be called on a struct that contains external data or a ref or void"
			));

		U64 currLen = 0;

		//Array specifies stride, so that's easy

		if (typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY) {

			U64 arrayLen = typeDesck.traits.array.stride;

			for (U64 j = 0; j < typeDesck.traits.array.dims_count; ++j) {

				U64 prevArrayLen = arrayLen;
				arrayLen *= typeDesck.traits.array.dims[j];

				if(arrayLen < prevArrayLen)
					retError(clean, Error_overflow(
						0, arrayLen, prevArrayLen, "SpvCalculateStructLength() arrayLen overflow"
					));
			}

			currLen = arrayLen;
		}

		//Struct causes recursion

		else if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) {
			gotoIfError3(clean, SpvCalculateStructLength(typeDesck.struct_type_description, &currLen, e_rr));
		}

		//Otherwise, we can easily calculate it via SpvReflectNumericTraits

		else {

			const SpvReflectNumericTraits *numeric = &typeDesck.traits.numeric;

			currLen = numeric->scalar.width >> 3;

			if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
				currLen =
					!(typeDesck.decoration_flags & SPV_REFLECT_DECORATION_ROW_MAJOR) ?
					numeric->matrix.stride * numeric->matrix.column_count :
					numeric->matrix.stride * numeric->matrix.row_count;

			else if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
				currLen *= numeric->vector.component_count;
		}

		U64 prevLen = len;
		len += currLen;

		if(len < prevLen)
			retError(clean, Error_overflow(0, len, prevLen, "SpvCalculateStructLength() len overflow"));
	}

clean:

	if(len)
		*result = len;

	return s_uccess;
}

Bool Compiler_convertMemberSPIRV(
	SBFile *sbFile,
	const SpvReflectBlockVariable *var,
	U16 parent,
	U32 offset,
	Bool isPacked,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	ESBType shType = (ESBType) 0;
	U64 perElementStride = 0;
	U16 structId = U16_MAX;
	U64 expectedSize = perElementStride;
	CharString str = CharString_createRefCStrConst(var->name);
	ListU32 arrays = ListU32{};

	if(var->array.dims_count > SPV_REFLECT_MAX_ARRAY_DIMS)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() array dimensions out of bounds"));

	if(var->array.dims_count && !var->array.stride)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() array stride unset"));

	for(U64 m = 0; m < var->array.dims_count; ++m)
		if(!var->array.dims[m] || var->array.spec_constant_op_ids[m] != U32_MAX)
			retError(clean, Error_invalidState(
				0, "Compiler_convertMemberSPIRV() invalid array data (0 or has spec constant op)"
			));

	if(var->flags && var->flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() unsupported value in cbuffer member"));

	if(!(var->type_description->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)) {
		gotoIfError3(clean, spvTypeToESBType(var->type_description, &shType, e_rr));
		perElementStride = var->array.dims_count ? var->array.stride : ESBType_getSize(shType, isPacked);
	}

	else {

		perElementStride = var->array.dims_count ? var->array.stride : var->size;

		U32 stride = var->array.dims_count ? var->array.stride : var->size;

		CharString structName = CharString_createRefCStrConst(var->type_description->type_name);

		U64 j = 0;

		for (; j < sbFile->structs.length; ++j) {

			SBStruct strct = sbFile->structs.ptr[j];
			CharString structNamej = sbFile->names.entryStrings.ptr[j];

			if(CharString_equalsStringSensitive(&structName, &structNamej) && strct.stride == stride)
				break;
		}

		//Insert type

		if (j == sbFile->structs.length)
			gotoIfError3(clean, SBFile_addStruct(sbFile, &structName, SBStruct{ .stride = stride }, alloc, e_rr));

		structId = (U16) j;
	}

	expectedSize = perElementStride;

	for(U64 m = 0; m < var->array.dims_count; ++m)
		expectedSize *= var->array.dims[m];

	if(var->size > expectedSize)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() var had mismatching size"));

	if(var->array.dims_count)
		gotoIfError3(clean, ListU32_createRefConst(var->array.dims, var->array.dims_count, &arrays, e_rr));

	if(shType != (ESBType) 0) {
		gotoIfError3(clean, SBFile_addVariableAsType(
			sbFile,
			&str,
			offset + var->offset, parent, shType,
			var->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		));
	}

	else {

		U16 newParent = (U16) sbFile->vars.length;

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			&str,
			offset + var->offset, parent, structId,
			var->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		));

		if(!var->member_count || !var->members)
			retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() missing member_count or members"));

		for (U64 j = 0; j < var->member_count; ++j)
			gotoIfError3(clean, Compiler_convertMemberSPIRV(
				sbFile, &var->members[j], newParent, offset + var->offset, isPacked, alloc, e_rr
			));
	}

clean:
	return s_uccess;
}

Bool Compiler_convertShaderBufferSPIRV(
	SpvReflectBlockVariable *block,
	Bool isPacked,
	const Allocator *alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;

	//StructuredBuffer; the inner element represents the whole buffer

	ESBSettingsFlags packedFlags = isPacked ? ESBSettingsFlags_IsTightlyPacked : (ESBSettingsFlags) 0;

	if (!block->padded_size) {

		if(block->member_count != 1 || !block->members)
			retError(clean, Error_invalidState(
				0, "Compiler_convertShaderBufferSPIRV()::block is missing member count or members"
			));

		SpvReflectBlockVariable *innerStruct = block->members;

		if(!innerStruct->member_count || !innerStruct->members || !innerStruct->padded_size) {

			ESBType type{};
			gotoIfError3(clean, spvTypeToESBType(innerStruct->type_description, &type, e_rr));

			if(type && (innerStruct->members || innerStruct->member_count || innerStruct->padded_size))
				retError(clean, Error_invalidState(
					0, "Compiler_convertShaderBufferSPIRV() inner struct is assumed to be a type, but has invalid members"
				));

			U32 paddedSize = ESBType_getSize(type, isPacked);

			if(!isPacked)
				paddedSize = (paddedSize + 15) &~ 15;

			gotoIfError3(clean, SBFile_create(packedFlags, paddedSize, alloc, sbFile, e_rr));
			CharString elementName = CharString_createRefCStrConst("$Element");
			ListU32 arrays{};

			if(innerStruct->array.dims_count)
				gotoIfError3(clean, ListU32_createRefConst(
					innerStruct->array.dims, innerStruct->array.dims_count, &arrays, e_rr
				));

			gotoIfError3(clean, SBFile_addVariableAsType(
				sbFile,
				&elementName,
				0, U16_MAX, type,
				block->flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
				arrays.length ? &arrays : NULL,
				alloc, e_rr
			));

			goto clean;
		}

		const C8 *structNameC = innerStruct->type_description->type_name;
		CharString structName = CharString_createRefCStrConst(structNameC);

		gotoIfError3(clean, SBFile_create(packedFlags, innerStruct->padded_size, alloc, sbFile, e_rr));
		gotoIfError3(clean, SBFile_addStruct(sbFile, &structName, SBStruct{ .stride = innerStruct->padded_size }, alloc, e_rr));

		CharString element = CharString_createRefCStrConst("$Element");

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			&element,
			0, U16_MAX, 0,
			innerStruct->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			NULL,
			alloc,
			e_rr
		));

		for (U64 l = 0; l < innerStruct->member_count; ++l) {
			SpvReflectBlockVariable var = innerStruct->members[l];
			gotoIfError3(clean, Compiler_convertMemberSPIRV(sbFile, &var, 0, 0, isPacked, alloc, e_rr));
		}

		goto clean;
	}

	//CBuffer or storage buffer (without dynamic entries)

	gotoIfError3(clean, SBFile_create(ESBSettingsFlags_None, block->padded_size, alloc, sbFile, e_rr));

	for (U64 l = 0; l < block->member_count; ++l) {
		SpvReflectBlockVariable var = block->members[l];
		gotoIfError3(clean, Compiler_convertMemberSPIRV(sbFile, &var, U16_MAX, 0, isPacked, alloc, e_rr));
	}

clean:
	if(!s_uccess && sbFile)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

Bool Compiler_convertRegisterSPIRV(
	ListSHRegisterRuntime *registers,
	SpvReflectDescriptorBinding *binding,
	U32 expectedSet,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	Bool isUnused = !binding->accessed;
	CharString name = CharString_createRefCStrConst(binding->name);

	ListU32 arrays{};
	SBFile sbFile{};

	U64 flatLen = 1;

	ESHBufferType bufferType = ESHBufferType_Count;
	Bool shouldBeBufferWrite = false;

	//Copy image desc to U64[3] for easier compares (copy is for alignment)

	static_assert(
		sizeof(binding->image) == sizeof(U64) * 3,
		"Compiler_convertRegisterSPIRV() does compares in U64[3] of binding->image, but size changed"
	);

	U64 imagePtrU64[3];
	Buffer_memcpy(
		Buffer_createRef(imagePtrU64, sizeof(imagePtrU64)),
		Buffer_createRefConst(&binding->image, sizeof(binding->image))
	);

	//Copy numeric desc to U64[3] for easier compares (copy is for alignment)

	static_assert(
		sizeof(binding->block.numeric) == sizeof(U64) * 3,
		"Compiler_convertShaderBufferSPIRV() does compares in U64[3] of binding->block.numeric, but size changed"
	);

	U64 numericTraitsU64[3];        //Fixes alignment
	Buffer_memcpy(
		Buffer_createRef(numericTraitsU64, sizeof(numericTraitsU64)),
		Buffer_createRefConst(&binding->block.numeric, sizeof(binding->block.numeric))
	);

	//Variable info

	const void *blockPtr = &binding->block.name;
	const U64 *blockPtrU64 = (const U64*) blockPtr;

	constexpr U8 blockSize = 40;

	static_assert(
		!(offsetof(SpvReflectBlockVariable, name) & 7) &&
		offsetof(SpvReflectBlockVariable, member_count) - offsetof(SpvReflectBlockVariable, name) == sizeof(U64) * blockSize,
		"Compiler_convertRegisterSPIRV() expected SpvReflectBlockVariable to be made of 43 U64s + 2x U32 and be aligned"
	);

	SHBindings bindings;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		bindings.arr[i] = SHBinding{ .space = U32_MAX, .binding = U32_MAX };

	bindings.arr[ESHBinaryType_SPIRV] = SHBinding{ .space = binding->set, .binding = binding->binding };

	if(expectedSet != binding->set)
		retError(clean, Error_invalidState(1, "Compiler_convertRegisterSPIRV() binding->set != parent->set"));

	if(binding->binding == U32_MAX && binding->set == U32_MAX)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() binding = U32_MAX, set = U32_MAX is reserved"
		));

	if(binding->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT && binding->input_attachment_index)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() input attachment index is invalid on non input attachment"
		));

	if(binding->byte_address_buffer_offset_count || binding->byte_address_buffer_offsets || binding->user_type)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() unsupported BAB offsets/count and user_type"
		));

	if(binding->array.dims_count) {

		gotoIfError3(clean, ListU32_createRefConst(binding->array.dims, binding->array.dims_count, &arrays, e_rr));

		for(U64 i = 0; i < binding->array.dims_count; ++i) {

			flatLen *= binding->array.dims[i];

			if(!flatLen || flatLen >> 32)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid flat length (out of bounds or 0)"
				));
		}

		if(flatLen != binding->count)
			retError(clean, Error_invalidState(
				1, "Compiler_convertRegisterSPIRV() register flat length mismatches binding count"
			));
	}

	switch (binding->descriptor_type) {

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {

			SpvReflectDecorationFlags sbufferFlags =
				SPV_REFLECT_DECORATION_ROW_MAJOR | SPV_REFLECT_DECORATION_COLUMN_MAJOR | SPV_REFLECT_DECORATION_NON_WRITABLE;

			if(
				!binding->block.member_count ||
				(binding->block.decoration_flags && binding->block.decoration_flags != SPV_REFLECT_DECORATION_NON_WRITABLE) ||
				(binding->block.flags && binding->block.flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED) ||
				numericTraitsU64[0] ||
				numericTraitsU64[1] ||
				numericTraitsU64[2] ||
				imagePtrU64[0] ||
				imagePtrU64[1] ||
				imagePtrU64[2] ||
				!binding->block.members ||
				(binding->decoration_flags &~ sbufferFlags)
			)
				retError(clean, Error_invalidState(0, "Compiler_convertRegisterSPIRV() invalid constant buffer data"));

			CharString typeName = CharString_createRefCStrConst(binding->type_description->type_name);
			Bool isAtomic = binding->uav_counter_id != U32_MAX || binding->uav_counter_binding;

			bufferType = ESHBufferType_StorageBuffer;

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				bufferType = ESHBufferType_ConstantBuffer;

			else if(CharString_startsWithCStringSensitive(&typeName, "type.", 0)) {

				typeName.ptr += 5;
				typeName.lenAndNullTerminated -= 5;

				Bool shouldBeWrite = false;

				if (CharString_startsWithCStringSensitive(&typeName, "RW", 0)) {
					typeName.ptr += 2;
					typeName.lenAndNullTerminated -= 2;
					shouldBeWrite = true;
				}

				if (CharString_equalsCStringSensitive(&typeName, "ByteAddressBuffer"))
					bufferType = ESHBufferType_ByteAddressBuffer;

				else {

					CharString appendBuffer = CharString_createRefCStrConst("AppendStructuredBuffer.");
					CharString consumeBuffer = CharString_createRefCStrConst("ConsumeStructuredBuffer.");
					CharString structuredBuffer = CharString_createRefCStrConst("StructuredBuffer.");

					if (
						CharString_startsWithStringSensitive(&typeName, &appendBuffer, 0) ||
						CharString_startsWithStringSensitive(&typeName, &consumeBuffer, 0)
					) {

						if(shouldBeWrite)
							retError(clean, Error_invalidState(
								0, "Compiler_convertRegisterSPIRV() invalid RW prefix for append/consume buffer"
							));

						bufferType = ESHBufferType_StructuredBufferAtomic;
						shouldBeWrite = true;
					}

					//TODO: Remember counter binding for SPIRV.

					else if(CharString_equalsCStringSensitive(&typeName, "ACSBuffer.counter"))
						goto clean;

					else if(CharString_startsWithStringSensitive(&typeName, &structuredBuffer, 0))
						bufferType = ESHBufferType_StructuredBuffer;

					else retError(clean, Error_invalidState(
						0, "Compiler_convertRegisterSPIRV() invalid RW prefix for append/consume buffer"
					));
				}

				shouldBeBufferWrite = shouldBeWrite;
			}

			if(bufferType == ESHBufferType_StorageBuffer && isAtomic)
				bufferType = ESHBufferType_StorageBufferAtomic;

			if(
				bufferType != ESHBufferType_StorageBufferAtomic &&
				bufferType != ESHBufferType_StructuredBufferAtomic &&
				isAtomic
			)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (has atomic, but invalid type)"
				));

			if(bufferType != ESHBufferType_ByteAddressBuffer && bufferType != ESHBufferType_AccelerationStructure) {

				//TODO: Storage buffer can have "static" part that's constant sized

				if (
					(
						bufferType == ESHBufferType_StructuredBuffer ||
						bufferType == ESHBufferType_StructuredBufferAtomic ||
						bufferType == ESHBufferType_StorageBuffer ||
						bufferType == ESHBufferType_StorageBufferAtomic
					) != (!binding->block.size || !binding->block.padded_size)
				)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() buffer requires size and/or padded size to be set/unset"
					));

				gotoIfError3(clean, Compiler_convertShaderBufferSPIRV(
					&binding->block,
					binding->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					alloc,
					&sbFile,
					e_rr
				));
			}

			break;
		}

		default:
			break;
	}

	if (bufferType == ESHBufferType_Count) {

		for(U8 i = 0; i < blockSize; ++i)
			if(blockPtrU64[i])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid register had buffer decorations but wasn't one"
				));

		if(
			binding->uav_counter_binding ||
			binding->uav_counter_id != U32_MAX ||
			binding->block.spirv_id ||
			binding->block.members ||
			binding->block.type_description ||
			binding->block.word_offset.offset
		)
			retError(clean, Error_invalidState(
				1, "Compiler_convertRegisterSPIRV() invalid register had buffer decorations but wasn't one"
			));
	}

	switch (binding->descriptor_type) {

		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_CBV)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not cbv)"
				));

			if(arrays.ptr)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() arrays aren't allowed on uniform buffers"
				));

			if(binding->decoration_flags)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected decoration flags on resource"
				));

			if(binding->uav_counter_id != U32_MAX || binding->uav_counter_binding)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() uav_counter_id or uav_counter_binding can't be set on cbv"
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_ConstantBuffer,
				false,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			));

			goto clean;

		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SAMPLER)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not sampler)"
				));

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid sampler register"
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				false,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			));

			goto clean;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			Log_errorLn(alloc, "Unsupported combined image sampler");        //TODO:
			retError(clean, Error_invalidState(1, "Compiler_convertRegisterSPIRV() combined image samplers not supported yet"));
			break;

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {

				if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() sampled image didn't have SRV resource flag"
					));

				if(binding->image.image_format || binding->image.sampled != 1)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unexpected image data on sampled image"
					));
			}
			else {

				if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_UAV)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() storage image didn't have UAV resource flag"
					));

				if(binding->image.sampled != 2)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unexpected image data on storage image"
					));
			}

			if(binding->decoration_flags)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected decoration flags on image"
				));

			if(binding->image.ms && (binding->image.dim != SpvDim2D || binding->image.depth != 2))
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected multi sample image"
				));

			ESHTextureType type = ESHTextureType_Texture2D;
			Bool isArray = binding->image.arrayed;

			if(binding->image.ms)
				type = ESHTextureType_Texture2DMS;

			else switch(binding->image.dim) {

				case SpvDim1D:    type = ESHTextureType_Texture1D;    break;
				case SpvDim2D:    type = ESHTextureType_Texture2D;    break;
				case SpvDim3D:    type = ESHTextureType_Texture3D;    break;
				case SpvDimCube:  type = ESHTextureType_TextureCube;  break;

				case SpvDimRect:
				case SpvDimBuffer:
				case SpvDimSubpassData:
				default:
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unsupported image type"
					));
			}

			//The SPIRV image "Depth" operand (0 = non-depth, 1 = depth/comparison, 2 = no indication) describes
			// how the image is sampled, not the register type:
			// DXC emits 2 for a regular sampled image and 1 for one used with SampleCmp (a SamplerComparisonState).
			//It doesn't change the reflected texture type, the comparison is carried by the separate sampler register,
			// so accept any of the three valid values.
			//(Previously this compared depth against a per-dimension constant,
			// which rejected all of Texture1D / Texture3D / TextureCube and every comparison-sampled texture on SPIRV.)
			if(binding->image.depth > 2)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() Unexpected image depth"
				));

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				{ gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
					registers,
					type,
					isArray,
					false,
					(U8)((!isUnused) << ESHBinaryType_SPIRV),
					ESHTexturePrimitive_Count,
					&name,
					arrays.length ? &arrays : NULL,
					bindings,
					alloc,
					e_rr
				)); }

			else {

				ETextureFormatId formatId = ETextureFormatId_Undefined;

				switch (binding->image.image_format) {

					case SpvImageFormatRgba32f:      formatId = ETextureFormatId_RGBA32f;  break;
					case SpvImageFormatRgba16f:      formatId = ETextureFormatId_RGBA16f;  break;
					case SpvImageFormatR32f:         formatId = ETextureFormatId_R32f;     break;
					case SpvImageFormatRgba8:        formatId = ETextureFormatId_RGBA8;    break;
					case SpvImageFormatRgba8Snorm:   formatId = ETextureFormatId_RGBA8s;   break;
					case SpvImageFormatRg32f:        formatId = ETextureFormatId_RG32f;    break;
					case SpvImageFormatRg16f:        formatId = ETextureFormatId_RG16f;    break;
					case SpvImageFormatR16f:         formatId = ETextureFormatId_R16f;     break;
					case SpvImageFormatRgba16:       formatId = ETextureFormatId_RGBA16;   break;
					case SpvImageFormatRgb10A2:      formatId = ETextureFormatId_BGR10A2;  break;
					case SpvImageFormatRg16:         formatId = ETextureFormatId_RG16;     break;
					case SpvImageFormatRg8:          formatId = ETextureFormatId_RG8;      break;
					case SpvImageFormatR16:          formatId = ETextureFormatId_R16;      break;
					case SpvImageFormatR8:           formatId = ETextureFormatId_R8;       break;
					case SpvImageFormatRgba16Snorm:  formatId = ETextureFormatId_RGBA16s;  break;
					case SpvImageFormatRg16Snorm:    formatId = ETextureFormatId_RG16s;    break;
					case SpvImageFormatRg8Snorm:     formatId = ETextureFormatId_RG8s;     break;
					case SpvImageFormatR16Snorm:     formatId = ETextureFormatId_R16s;     break;
					case SpvImageFormatR8Snorm:      formatId = ETextureFormatId_R8s;      break;
					case SpvImageFormatRgba32i:      formatId = ETextureFormatId_RGBA32i;  break;
					case SpvImageFormatRgba16i:      formatId = ETextureFormatId_RGBA16i;  break;
					case SpvImageFormatRgba8i:       formatId = ETextureFormatId_RGBA8i;   break;
					case SpvImageFormatR32i:         formatId = ETextureFormatId_R32i;     break;
					case SpvImageFormatRg32i:        formatId = ETextureFormatId_RG32i;    break;
					case SpvImageFormatRg16i:        formatId = ETextureFormatId_RG16i;    break;
					case SpvImageFormatRg8i:         formatId = ETextureFormatId_RG8i;     break;
					case SpvImageFormatR16i:         formatId = ETextureFormatId_R16i;     break;
					case SpvImageFormatR8i:          formatId = ETextureFormatId_R8i;      break;
					case SpvImageFormatRgba32ui:     formatId = ETextureFormatId_RGBA32u;  break;
					case SpvImageFormatRgba16ui:     formatId = ETextureFormatId_RGBA16u;  break;
					case SpvImageFormatRgba8ui:      formatId = ETextureFormatId_RGBA8u;   break;
					case SpvImageFormatR32ui:        formatId = ETextureFormatId_R32u;     break;
					case SpvImageFormatRg32ui:       formatId = ETextureFormatId_RG32u;    break;
					case SpvImageFormatRg16ui:       formatId = ETextureFormatId_RG16u;    break;
					case SpvImageFormatRg8ui:        formatId = ETextureFormatId_RG8u;     break;
					case SpvImageFormatR16ui:        formatId = ETextureFormatId_R16u;     break;
					case SpvImageFormatR8ui:         formatId = ETextureFormatId_R8u;      break;
					case SpvImageFormatUnknown:                                            break;

					default:
					case SpvImageFormatRgb10a2ui:
					case SpvImageFormatR64ui:
					case SpvImageFormatR64i:
					case SpvImageFormatR11fG11fB10f:
						retError(clean, Error_invalidState(
							1, "Compiler_convertRegisterSPIRV() unsupported image format: rg11fb10f, r64i, r64ui, rgb10a2ui"
						));
				}

				gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
					registers,
					type,
					isArray,
					(U8)((!isUnused) << ESHBinaryType_SPIRV),
					ESHTexturePrimitive_Count,
					formatId,
					&name,
					arrays.ptr ? &arrays : NULL,
					bindings,
					alloc,
					e_rr
				));
			}

			break;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {

			if(
				binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV &&
				binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_UAV
			)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not uav/srv)"
				));

			Bool isWrite = !(binding->block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);

			if((binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_UAV) != isWrite)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (expected uav or srv but got the opposite)"
				));

			if(shouldBeBufferWrite != isWrite)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (type had RW, but buffer didn't)"
				));

			Bool hasSBFile =
				bufferType != ESHBufferType_ByteAddressBuffer &&
				bufferType != ESHBufferType_AccelerationStructure;

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				bufferType,
				isWrite,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				arrays.length ? &arrays : NULL,
				hasSBFile ? &sbFile : NULL,
				bindings,
				alloc,
				e_rr
			));

			goto clean;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not SRV)"
				));

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(1, "Compiler_convertRegisterSPIRV() invalid RTAS register"));

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_AccelerationStructure,
				false,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				arrays.length ? &arrays : NULL,
				NULL,
				bindings,
				alloc,
				e_rr
			));

			break;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
				retError(clean, Error_invalidState(
					2, "Compiler_convertRegisterSPIRV() mismatching resource_type (not SRV)"
				));

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() invalid input attachment register"
				));

			if(binding->input_attachment_index >> 16)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() input attachment register out of bounds"
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addSubpassInput(
				registers,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				bindings,
				(U16) binding->input_attachment_index,
				alloc,
				e_rr
			));

			break;

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		default:
			retError(clean, Error_invalidState(
				1,
				"Compiler_convertRegisterSPIRV() unsupported descriptor type "
				"(uniform/storage buffer dynamic or storage/uniform texel buffer)"
			));
	}

clean:
	SBFile_free(&sbFile, alloc);
	return s_uccess;
}
