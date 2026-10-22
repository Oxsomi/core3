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

//shader_compiler/test/test_shader_compiler_spirv_layout.c

#include "test_shader_compiler_shared.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"

#include <inttypes.h>

//The opcodes, decorations and storage classes the check reads, numbered as the SPIR-V specification numbers them.

typedef enum ESpvLayoutOp {
	ESpvLayoutOp_MemberName         = 6,
	ESpvLayoutOp_TypeFirst          = 19,
	ESpvLayoutOp_TypeInt            = 21,
	ESpvLayoutOp_TypeFloat          = 22,
	ESpvLayoutOp_TypeVector         = 23,
	ESpvLayoutOp_TypeMatrix         = 24,
	ESpvLayoutOp_TypeArray          = 28,
	ESpvLayoutOp_TypeRuntimeArray   = 29,
	ESpvLayoutOp_TypeStruct         = 30,
	ESpvLayoutOp_TypePointer        = 32,
	ESpvLayoutOp_TypeLast           = 39,
	ESpvLayoutOp_Constant           = 43,
	ESpvLayoutOp_Variable           = 59,
	ESpvLayoutOp_Decorate           = 71,
	ESpvLayoutOp_MemberDecorate     = 72
} ESpvLayoutOp;

typedef enum ESpvLayoutDecoration {
	ESpvLayoutDecoration_ArrayStride    = 6,
	ESpvLayoutDecoration_MatrixStride   = 7,
	ESpvLayoutDecoration_Binding        = 33,
	ESpvLayoutDecoration_DescriptorSet  = 34,
	ESpvLayoutDecoration_Offset         = 35
} ESpvLayoutDecoration;

typedef enum ESpvLayoutStorage {
	ESpvLayoutStorage_Uniform           = 2,
	ESpvLayoutStorage_PushConstant      = 9,
	ESpvLayoutStorage_StorageBuffer     = 12
} ESpvLayoutStorage;

typedef struct SpvLayoutCheck {

	const Allocator *alloc;
	const C8 *label;

	const U32 *words;
	const U32 *declarations;        //Per id, the word index of the instruction declaring it, 0 when nothing does

	U64 wordCount;
	U64 declarationsStart;          //Names and decorations all precede the first type declaration

	const SBFile *sb;
	CharString registerName;

	U32 bound;
	Bool packed;
	Bool log;
	U16 padding;

} SpvLayoutCheck;

static const U32 *SpvLayoutCheck_declaration(const SpvLayoutCheck *c, U32 id, ESpvLayoutOp op) {

	if(id >= c->bound || !c->declarations[id])
		return NULL;

	const U32 *instruction = c->words + c->declarations[id];
	return (instruction[0] & 0xFFFF) == (U32) op ? instruction : NULL;
}

//The literal of OpDecorate target, or of OpMemberDecorate target member when member isn't U32_MAX.
static Bool SpvLayoutCheck_decoration(
	const SpvLayoutCheck *c, U32 target, U32 member, ESpvLayoutDecoration decoration, U32 *value
) {

	const Bool isMember = member != U32_MAX;
	const U32 op = isMember ? ESpvLayoutOp_MemberDecorate : ESpvLayoutOp_Decorate;
	const U32 decorationAt = isMember ? 3 : 2;

	for(U64 i = 5; i < c->declarationsStart; i += c->words[i] >> 16) {

		const U32 *instruction = c->words + i;

		if(
			(instruction[0] & 0xFFFF) == op && (instruction[0] >> 16) > decorationAt + 1 && instruction[1] == target &&
			(!isMember || instruction[2] == member) && instruction[decorationAt] == (U32) decoration
		) {
			*value = instruction[decorationAt + 1];
			return true;
		}
	}

	return false;
}

static CharString SpvLayoutCheck_memberName(const SpvLayoutCheck *c, U32 structType, U32 member) {

	for(U64 i = 5; i < c->declarationsStart; i += c->words[i] >> 16) {

		const U32 *instruction = c->words + i;
		const U32 length = instruction[0] >> 16;

		if(
			(instruction[0] & 0xFFFF) != ESpvLayoutOp_MemberName || length < 4 ||
			instruction[1] != structType || instruction[2] != member
		)
			continue;

		const C8 *name = (const C8*) (const void*) (instruction + 3);
		const U64 capacity = ((U64) length - 3) * sizeof(U32);
		U64 nameLength = 0;

		while(nameLength < capacity && name[nameLength])
			++nameLength;

		return CharString_createRefSizedConst(name, nameLength, nameLength < capacity);
	}

	return CharString_createNull();
}

//The ESBType a scalar, vector or matrix type is, where a SPIR-V matrix column is one of oiSB's vectors.
static Bool SpvLayoutCheck_leafType(const SpvLayoutCheck *c, U32 typeId, ESBType *type) {

	U32 vectors = 1, components = 1;
	const U32 *instruction = SpvLayoutCheck_declaration(c, typeId, ESpvLayoutOp_TypeMatrix);

	if(instruction) {
		vectors = instruction[3];
		typeId = instruction[2];
	}

	if((instruction = SpvLayoutCheck_declaration(c, typeId, ESpvLayoutOp_TypeVector)) != NULL) {
		components = instruction[3];
		typeId = instruction[2];
	}

	ESBPrimitive primitive = ESBPrimitive_Float;
	U32 width = 0;

	if((instruction = SpvLayoutCheck_declaration(c, typeId, ESpvLayoutOp_TypeFloat)) != NULL)
		width = instruction[2];

	else if((instruction = SpvLayoutCheck_declaration(c, typeId, ESpvLayoutOp_TypeInt)) != NULL) {
		primitive = instruction[3] ? ESBPrimitive_Int : ESBPrimitive_UInt;
		width = instruction[2];
	}

	if(!vectors || vectors > 4 || !components || components > 4)
		return false;

	ESBStride stride = ESBStride_X32;

	switch(width) {
		case 8:     stride = ESBStride_X8;      break;
		case 16:    stride = ESBStride_X16;     break;
		case 32:    stride = ESBStride_X32;     break;
		case 64:    stride = ESBStride_X64;     break;
		default:    return false;
	}

	*type = (ESBType) ESBType_create(stride, primitive, components - 1, vectors - 1);
	return true;
}

//Logs how a variable (U16_MAX for the buffer itself) differs from the SPIR-V, when logging, and fails.
static Bool SpvLayoutCheck_fail(const SpvLayoutCheck *c, U16 varId, const C8 *what, U64 recorded, U64 declared) {

	if(!c->log)
		return false;

	const U64 nameId = c->sb->structs.length + varId;

	const CharString name =
		varId != U16_MAX && nameId < c->sb->names.entryStrings.length ? c->sb->names.entryStrings.ptr[nameId] :
		CharString_createRefCStrConst("(buffer)");

	Log_errorLn(
		c->alloc, "%s: register %.*s variable %.*s %s (oiSB %" PRIu64 ", SPIR-V %" PRIu64 ")",
		c->label, (int) CharString_length(c->registerName), c->registerName.ptr,
		(int) CharString_length(name), name.ptr, what, recorded, declared
	);

	return false;
}

static U64 SpvLayoutCheck_recordedDims(const SBFile *sb, SBVar var, U32 *inlineDim, const U32 **dims) {

	*dims = NULL;

	if(!var.arrayDimOrArrayId)
		return 0;

	if(!(var.arrayDimOrArrayId & (U16) I16_MIN)) {
		*inlineDim = var.arrayDimOrArrayId;
		*dims = inlineDim;
		return 1;
	}

	const U16 arrayId = var.arrayDimOrArrayId & (U16) I16_MAX;

	if(arrayId >= sb->arrays.length)
		return U64_MAX;

	*dims = sb->arrays.ptr[arrayId].ptr;
	return sb->arrays.ptr[arrayId].length;
}

static Bool SpvLayoutCheck_members(SpvLayoutCheck *c, U32 structType, U16 parent, U32 base);

//One variable against the member that declares it: offset, array dimensions and strides, then either the members of
// its struct or its type and matrix row stride.
static Bool SpvLayoutCheck_variable(SpvLayoutCheck *c, U16 varId, U32 typeId, U32 parentType, U32 member, U32 offset) {

	const SBVar var = c->sb->vars.ptr[varId];

	if(var.offset != offset)
		return SpvLayoutCheck_fail(c, varId, "sits at a different offset", var.offset, offset);

	U32 dims[32], strides[32];
	U64 dimCount = 0;

	for(const U32 *array; (array = SpvLayoutCheck_declaration(c, typeId, ESpvLayoutOp_TypeArray)) != NULL; typeId = array[2]) {

		const U32 *length = SpvLayoutCheck_declaration(c, array[3], ESpvLayoutOp_Constant);

		if(dimCount == sizeof(dims) / sizeof(dims[0]) || !length)
			return SpvLayoutCheck_fail(c, varId, "is an array without a constant length", 0, dimCount);

		if(!SpvLayoutCheck_decoration(c, array[1], U32_MAX, ESpvLayoutDecoration_ArrayStride, &strides[dimCount]))
			return SpvLayoutCheck_fail(c, varId, "is an array without an ArrayStride", 0, dimCount);

		dims[dimCount++] = length[3];
	}

	U32 inlineDim = 0;
	const U32 *recordedDims = NULL;
	const U64 recordedCount = SpvLayoutCheck_recordedDims(c->sb, var, &inlineDim, &recordedDims);

	if(recordedCount != dimCount)
		return SpvLayoutCheck_fail(c, varId, "has a different number of array dimensions", recordedCount, dimCount);

	for(U64 i = 0; i < dimCount; ++i)
		if(recordedDims[i] != dims[i])
			return SpvLayoutCheck_fail(c, varId, "has a different array dimension", recordedDims[i], dims[i]);

	//oiSB spaces the elements of a multi dimensional array as one flat array, so an outer stride has to be exactly the
	// extent of the array inside it.

	for(U64 i = 0; i + 1 < dimCount; ++i)
		if((U64) strides[i] != (U64) strides[i + 1] * dims[i + 1])
			return SpvLayoutCheck_fail(
				c, varId, "has an outer ArrayStride that isn't its inner array's extent",
				(U64) strides[i + 1] * dims[i + 1], strides[i]
			);

	if(SpvLayoutCheck_declaration(c, typeId, ESpvLayoutOp_TypeStruct)) {

		if(var.structId >= c->sb->structs.length)
			return SpvLayoutCheck_fail(c, varId, "is a struct in SPIR-V but not in oiSB", var.structId, typeId);

		const U32 stride = c->sb->structs.ptr[var.structId].stride;

		if(dimCount && strides[dimCount - 1] != stride)
			return SpvLayoutCheck_fail(
				c, varId, "has a struct stride its ArrayStride disagrees with", stride, strides[dimCount - 1]
			);

		return SpvLayoutCheck_members(c, typeId, varId, offset);
	}

	ESBType type = (ESBType) 0;

	if(!SpvLayoutCheck_leafType(c, typeId, &type))
		return SpvLayoutCheck_fail(c, varId, "has a SPIR-V type oiSB can't describe", var.type, typeId);

	if(var.structId != U16_MAX || var.type != (U8) type)
		return SpvLayoutCheck_fail(c, varId, "has a different type (ESBType)", var.type, type);

	const U32 size = ESBType_getSize(type, c->packed);
	const U32 elementStride = c->packed ? size : (size + 15) & ~15;

	if(dimCount && strides[dimCount - 1] != elementStride)
		return SpvLayoutCheck_fail(
			c, varId, "has an ArrayStride other than the element stride its packing implies",
			elementStride, strides[dimCount - 1]
		);

	if(ESBType_getMatrix(type) == ESBMatrix_N1)
		return true;

	//MatrixStride decorates the member holding the matrix, through any arrays of it

	U32 matrixStride = 0;

	if(!SpvLayoutCheck_decoration(c, parentType, member, ESpvLayoutDecoration_MatrixStride, &matrixStride))
		return SpvLayoutCheck_fail(c, varId, "is a matrix without a MatrixStride", 0, 0);

	const U32 row = ((U32) ESBType_getVector(type) + 1) << (U32) ESBType_getStride(type);
	const U32 impliedRow = c->packed ? row : (row + 15) & ~15;

	if(matrixStride != impliedRow)
		return SpvLayoutCheck_fail(
			c, varId, "has a MatrixStride other than the row stride its packing implies", impliedRow, matrixStride
		);

	return true;
}

//The variables under parent against the members of structType, which the reflection adds in declaration order.
static Bool SpvLayoutCheck_members(SpvLayoutCheck *c, U32 structType, U16 parent, U32 base) {

	const U32 *strct = SpvLayoutCheck_declaration(c, structType, ESpvLayoutOp_TypeStruct);
	const U32 memberCount = (strct[0] >> 16) - 2;
	U32 member = 0;

	for(U64 i = 0; i < c->sb->vars.length; ++i) {

		if(c->sb->vars.ptr[i].parentId != parent)
			continue;

		if(member == memberCount)
			return SpvLayoutCheck_fail(c, (U16) i, "has no SPIR-V member left to match", member + 1, memberCount);

		U32 memberOffset = 0;

		if(!SpvLayoutCheck_decoration(c, structType, member, ESpvLayoutDecoration_Offset, &memberOffset))
			return SpvLayoutCheck_fail(c, (U16) i, "matches a member without an Offset", member, 0);

		//Names are compared when the binary still has them, so a skipped or reordered member can't pass on offsets alone

		const CharString declared = SpvLayoutCheck_memberName(c, structType, member);
		const U64 nameId = c->sb->structs.length + i;

		if(CharString_length(declared) && nameId < c->sb->names.entryStrings.length) {

			const CharString recorded = c->sb->names.entryStrings.ptr[nameId];

			if(!CharString_equalsStringSensitive(&recorded, &declared)) {

				if(c->log)
					Log_errorLn(
						c->alloc, "%s: register %.*s variable %.*s sits where the SPIR-V declares member %.*s",
						c->label, (int) CharString_length(c->registerName), c->registerName.ptr,
						(int) CharString_length(recorded), recorded.ptr, (int) CharString_length(declared), declared.ptr
					);

				return false;
			}
		}

		if(!SpvLayoutCheck_variable(c, (U16) i, strct[2 + member], structType, member, base + memberOffset))
			return false;

		++member;
	}

	if(member != memberCount)
		return SpvLayoutCheck_fail(c, parent, "has fewer variables than its SPIR-V struct has members", member, memberCount);

	return true;
}

//A buffer variable's pointer type against the whole oiSB.
static Bool SpvLayoutCheck_buffer(SpvLayoutCheck *c, U32 pointerType) {

	const U32 *pointer = SpvLayoutCheck_declaration(c, pointerType, ESpvLayoutOp_TypePointer);

	if(!pointer)
		return SpvLayoutCheck_fail(c, U16_MAX, "is bound to a variable that isn't a pointer", 0, pointerType);

	//An array of buffers is one register, every buffer in it laid out alike

	U32 blockType = pointer[3];
	const U32 *array = NULL;

	while(
		(array = SpvLayoutCheck_declaration(c, blockType, ESpvLayoutOp_TypeArray)) != NULL ||
		(array = SpvLayoutCheck_declaration(c, blockType, ESpvLayoutOp_TypeRuntimeArray)) != NULL
	)
		blockType = array[2];

	const U32 *block = SpvLayoutCheck_declaration(c, blockType, ESpvLayoutOp_TypeStruct);

	if(!block)
		return SpvLayoutCheck_fail(c, U16_MAX, "is bound to a block that isn't a struct", 0, blockType);

	const U32 *runtime =
		(block[0] >> 16) == 3 ? SpvLayoutCheck_declaration(c, block[2], ESpvLayoutOp_TypeRuntimeArray) : NULL;

	if(!runtime)
		return SpvLayoutCheck_members(c, blockType, U16_MAX, 0);

	//A structured buffer: oiSB describes a single element as its one root variable, and the buffer size is its stride

	U32 stride = 0;

	if(!SpvLayoutCheck_decoration(c, block[2], U32_MAX, ESpvLayoutDecoration_ArrayStride, &stride))
		return SpvLayoutCheck_fail(c, U16_MAX, "has a runtime array without an ArrayStride", c->sb->bufferSize, 0);

	if(c->sb->bufferSize != stride)
		return SpvLayoutCheck_fail(c, U16_MAX, "has a different element stride", c->sb->bufferSize, stride);

	U64 root = U64_MAX, roots = 0;

	for(U64 i = 0; i < c->sb->vars.length; ++i)
		if(c->sb->vars.ptr[i].parentId == U16_MAX && !roots++)
			root = i;

	if(roots != 1)
		return SpvLayoutCheck_fail(c, U16_MAX, "has a different number of element variables", roots, 1);

	const SBVar element = c->sb->vars.ptr[root];

	if(element.structId < c->sb->structs.length && c->sb->structs.ptr[element.structId].stride != stride)
		return SpvLayoutCheck_fail(
			c, (U16) root, "has a struct stride other than the element stride",
			c->sb->structs.ptr[element.structId].stride, stride
		);

	return SpvLayoutCheck_variable(c, (U16) root, runtime[2], blockType, 0, 0);
}

Bool oiSHFileDescribesSpirvLayout(const Allocator *alloc, const SHFile *file, const C8 *label, U64 *checkedBuffers) {

	Buffer words = Buffer_createNull(), declarations = Buffer_createNull();
	Error err = Error_none();
	Bool ok = true;

	for(U64 b = 0; b < file->binaries.length && ok; ++b) {

		const SHBinaryInfo *binary = &file->binaries.ptr[b];
		const Buffer spirv = binary->binaries[EGfxBinaryType_SPIRV];
		const U64 wordCount = Buffer_length(spirv) / sizeof(U32);

		if(!Buffer_length(spirv))
			continue;

		Buffer_free(&words, alloc);
		Buffer_free(&declarations, alloc);

		//A copy, because the binary sits wherever the oiSH put it and the words have to be aligned to be read in place

		ok = (Buffer_length(spirv) & 3) == 0 && wordCount >= 5 && Buffer_createCopy(spirv, alloc, &words, &err);

		const U32 *w = ok ? (const U32*) (const void*) words.ptr : NULL;
		const U32 bound = ok ? w[3] : 0;

		ok &= ok && w[0] == 0x07230203 && bound && !(bound >> 24);
		ok &= ok && Buffer_createUninitializedBytes((U64) bound * sizeof(U32), alloc, &declarations, &err);

		if(!ok) {

			if(label)
				Log_errorLn(alloc, "%s: binary %" PRIu64 " isn't a SPIR-V module the layout check can read", label, b);

			break;
		}

		U32 *declared = (U32*) (void*) declarations.ptrNonConst;

		for(U32 i = 0; i < bound; ++i)
			declared[i] = 0;

		U64 declarationsStart = wordCount;

		for(U64 i = 5; i < wordCount; ) {

			const U32 length = w[i] >> 16, op = w[i] & 0xFFFF;

			if(!length || length > wordCount - i) {

				if(label)
					Log_errorLn(alloc, "%s: binary %" PRIu64 " has a malformed instruction at word %" PRIu64, label, b, i);

				ok = false;
				break;
			}

			U32 minLength = 0, idAt = 1;

			switch(op) {

				case ESpvLayoutOp_TypeStruct:
					minLength = 2;
					break;

				case ESpvLayoutOp_TypeFloat:
				case ESpvLayoutOp_TypeRuntimeArray:
					minLength = 3;
					break;

				case ESpvLayoutOp_TypeInt:
				case ESpvLayoutOp_TypeVector:
				case ESpvLayoutOp_TypeMatrix:
				case ESpvLayoutOp_TypeArray:
				case ESpvLayoutOp_TypePointer:
					minLength = 4;
					break;

				case ESpvLayoutOp_Constant:
				case ESpvLayoutOp_Variable:
					minLength = 4;
					idAt = 2;
					break;

				default:
					break;
			}

			if(minLength && length >= minLength && w[i + idAt] < bound)
				declared[w[i + idAt]] = (U32) i;

			if(op >= ESpvLayoutOp_TypeFirst && op <= ESpvLayoutOp_TypeLast && declarationsStart == wordCount)
				declarationsStart = i;

			i += length;
		}

		SpvLayoutCheck check = (SpvLayoutCheck) {
			.alloc = alloc,
			.label = label,
			.words = w,
			.declarations = declared,
			.wordCount = wordCount,
			.declarationsStart = declarationsStart,
			.bound = bound
		};

		for(U64 r = 0; r < binary->registers.length && ok; ++r) {

			const SHRegisterRuntime *reg = &binary->registers.ptr[r];
			const U8 registerType = reg->reg.registerType & EGfxRegisterType_TypeMask;

			if(
				registerType < EGfxRegisterType_BufferStart || registerType > EGfxRegisterType_BufferEnd ||
				!reg->shaderBuffer.vars.length || !SHRegister_isPresentIn(&reg->reg, EGfxBinaryType_SPIRV)
			)
				continue;

			check.sb = &reg->shaderBuffer;
			check.registerName = reg->name;
			check.packed = (reg->shaderBuffer.flags & ESBSettingsFlags_IsTightlyPacked) != 0;

			const GfxBinding binding = reg->reg.bindings.arr[EGfxBinaryType_SPIRV];
			U32 firstPointer = 0;
			U64 candidates = 0;
			Bool matched = false;

			//A push constant is found by its storage class, since Vulkan never binds it, and the rest by set and binding.
			//A library module can hold several variables one register could be, so any of them matching is enough, and
			// the first one explains a mismatch.

			for(U64 i = declarationsStart; i < wordCount && !matched; i += w[i] >> 16) {

				if((w[i] & 0xFFFF) != ESpvLayoutOp_Variable || (w[i] >> 16) < 4)
					continue;

				const U32 storage = w[i + 3];
				U32 set = 0, bindingId = 0;

				const Bool isCandidate = registerType == EGfxRegisterType_PushConstants ?
					storage == ESpvLayoutStorage_PushConstant :
					(storage == ESpvLayoutStorage_Uniform || storage == ESpvLayoutStorage_StorageBuffer) &&
					SpvLayoutCheck_decoration(&check, w[i + 2], U32_MAX, ESpvLayoutDecoration_DescriptorSet, &set) &&
					SpvLayoutCheck_decoration(&check, w[i + 2], U32_MAX, ESpvLayoutDecoration_Binding, &bindingId) &&
					set == binding.space && bindingId == binding.binding;

				if(!isCandidate)
					continue;

				if(!candidates++)
					firstPointer = w[i + 1];

				check.log = false;
				matched = SpvLayoutCheck_buffer(&check, w[i + 1]);
			}

			if(matched) {

				if(checkedBuffers)
					++*checkedBuffers;

				continue;
			}

			ok = false;

			if(!label)
				continue;

			if(!candidates)
				Log_errorLn(
					alloc, "%s: register %.*s has no SPIR-V variable to compare its layout against",
					label, (int) CharString_length(reg->name), reg->name.ptr
				);

			else {
				check.log = true;
				SpvLayoutCheck_buffer(&check, firstPointer);
			}
		}
	}

	if(err.genericError && label)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	Buffer_free(&declarations, alloc);
	Buffer_free(&words, alloc);
	return ok;
}

Bool oiSHDescribesSpirvLayout(const Allocator *alloc, Buffer oiSH, const C8 *label, U64 *checkedBuffers) {

	SHFile file = (SHFile) { 0 };
	Error err = Error_none();

	const Bool ok = readOiSH(alloc, oiSH, &file, &err) && oiSHFileDescribesSpirvLayout(alloc, &file, label, checkedBuffers);

	if(err.genericError && label)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	SHFile_free(&file, alloc);
	return ok;
}
