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

//formats/oiSH/sh_registers.c

#include "types/container/list_impl.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/log.h"
#include "types/container/list_basic_types.h"
#include "types/base/constants.h"
#include "types/base/string_read_helper.h"

TListImpl(SHRegister);
TListImpl(SHRegisterRuntime);

EGfxTexturePrimitive EGfxTexturePrimitive_fromTextureFormat(ETextureFormat format) {

	ETexturePrimitive prim = ETextureFormat_getPrimitive(format);

	U8 channels = ETextureFormat_getChannels(format);
	EGfxTexturePrimitive res = (channels - 1) << EGfxTexturePrimitive_ComponentShift;

	switch(prim) {

		default:                         return 0xFF;
		case ETexturePrimitive_UInt:     return res | EGfxTexturePrimitive_UInt;
		case ETexturePrimitive_SInt:     return res | EGfxTexturePrimitive_SInt;
		case ETexturePrimitive_SNorm:    return res | EGfxTexturePrimitive_SNorm;

		case ETexturePrimitive_Compressed:
		case ETexturePrimitive_Float:
			return res | EGfxTexturePrimitive_Float;

		case ETexturePrimitive_UNorm:
		case ETexturePrimitive_UNormBGR:
			return res | EGfxTexturePrimitive_UNorm;
	}
}

U64 SHRegister_arrayCount(const ListU32 *arrays) {

	U64 count = 1;

	for(U64 i = 0; i < (arrays ? arrays->length : 0); ++i) {

		const U32 dim = arrays->ptr[i];

		if(!dim)
			return 0;

		if(count > U64_MAX / dim)
			return U64_MAX;

		count *= dim;
	}

	return count;
}

Bool SHFile_detectDuplicate(
	const ListSHRegisterRuntime *info,
	CharString name,
	const ListU32 *arrays,
	GfxBindings bindings,
	EGfxRegisterType type,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool anyBinding = false;

	Bool anyDxilBinding = false;
	Bool anySpvBinding = false;

	for(U8 i = 0; i < EGfxBinaryType_Count; ++i)
		if(bindings.arr[i].space != U32_MAX || bindings.arr[i].binding != U32_MAX) {

			anyBinding = true;

			if(i == EGfxBinaryType_DXIL)
				anyDxilBinding = true;

			if(i == EGfxBinaryType_SPIRV)
				anySpvBinding = true;
		}

	if (!anyBinding && type != EGfxRegisterType_PushConstants)
		retError(clean, Error_invalidState(0, "SHFile_detectDuplicate()::bindings contained no valid bindings"));

	if(!CharString_length(name))
		retError(clean, Error_invalidParameter(1, 0, "SHFile_detectDuplicate()::name is required"));

	const U64 count = SHRegister_arrayCount(arrays);

	for(U64 i = 0; i < info->length; ++i) {

		SHRegisterRuntime reg = info->ptr[i];

		if (CharString_equalsStringSensitive(&reg.name, &name)) {
			retError(clean, Error_invalidState(0, "SHFile_detectDuplicate()::name was already found in SHFile"));
		}

		else {

			const U64 regCount = SHRegister_arrayCount(&reg.arrays);

			if(anySpvBinding) {

				GfxBinding dstBinding = reg.reg.bindings.arr[EGfxBinaryType_SPIRV];
				GfxBinding srcBinding = bindings.arr[EGfxBinaryType_SPIRV];

				if(
					GfxBinding_overlaps(
						dstBinding, reg.reg.registerType, regCount, srcBinding, type, count, EGfxBinaryType_SPIRV
					)
				)
					retError(clean, Error_invalidState(
						0, "SHFile_detectDuplicate() SPIRV space & binding combo was already found in SHFile"
					));
			}

			if (anyDxilBinding) {

				GfxBinding dstBinding = reg.reg.bindings.arr[EGfxBinaryType_DXIL];
				GfxBinding srcBinding = bindings.arr[EGfxBinaryType_DXIL];

				if(
					GfxBinding_overlaps(
						dstBinding, reg.reg.registerType, regCount, srcBinding, type, count, EGfxBinaryType_DXIL
					)
				)
					retError(clean, Error_invalidState(
						0, "SHFile_detectDuplicate() DXIL register range overlaps one already in SHFile"
					));
			}
		}
	}

clean:
	return s_uccess;
}

Bool SHFile_validateRegister(
	const ListSHRegisterRuntime *info,
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	EGfxRegisterType type,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!name || !CharString_length(*name))
		retError(clean, Error_nullPointer(0, "SHFile_validateRegister()::name is required"));

	if(arrays && (!arrays->length || arrays->length > 32))
		retError(clean, Error_outOfBounds(
			1, arrays->length, 32, "SHFile_validateRegister()::arrays.length should be [1, 32]"
		));

	gotoIfError3(clean, SHFile_detectDuplicate(info, *name, arrays, bindings, type, e_rr));

clean:
	return s_uccess;
}

Bool SHRegisterRuntime_hash(
	const SHRegister *registr,
	const CharString *name,
	ListU32 *arrays,
	SBFile *sbFile,
	U64 *res,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!registr || !name)
		retError(clean, Error_nullPointer(!registr ? 0 : 1, "SHRegisterRuntime_hash()::registr and name are required"));

	if(CharString_length(*name) > U32_MAX)
		retError(clean, Error_outOfBounds(
			0, CharString_length(*name), U32_MAX, "SHRegisterRuntime_hash() name->length out of bounds"
		));

	if(arrays && arrays->length > U32_MAX)
		retError(clean, Error_outOfBounds(
			0, arrays->length, U32_MAX, "SHRegisterRuntime_hash() arrays->length out of bounds"
		));

	if(!res)
		retError(clean, Error_nullPointer(4, "SHRegisterRuntime_hash()::res is required"));

	//Compute hash to find register

	static_assert(sizeof(SHRegister) == sizeof(U64) * (EGfxBinaryType_Count + 1), "Expected SHRegister as U64[N + 1]");

	U64 hash = sbFile ? sbFile->hash : Buffer_fnv1a64Offset;

	//SHRegister only guarantees U32 alignment, so reading it in place as U64s can be a misaligned load.
	//The bytes go through a properly aligned local instead.
	//The static_assert above keeps the sizes in sync.

	U64 regU64[EGfxBinaryType_Count + 1];
	Buffer_memcpy(Buffer_createRef(regU64, sizeof(regU64)), Buffer_createRefConst(registr, sizeof(SHRegister)));

	for(U64 i = 0; i < EGfxBinaryType_Count + 1; ++i)
		hash = Buffer_fnv1a64Single(regU64[i], hash);

	hash = Buffer_fnv1a64Single(CharString_length(*name) | ((arrays ? arrays->length : 0) << 32), hash);
	hash = Buffer_fnv1a64(CharString_bufferConst(*name), hash);

	if (arrays) {

		for (U64 i = 0; i + 1 < arrays->length; i += 2)
			hash = Buffer_fnv1a64Single(arrays->ptr[i] | ((U64)arrays->ptr[i + 1] << 32), hash);

		if (arrays->length & 1)
			hash = Buffer_fnv1a64Single(arrays->ptr[arrays->length - 1], hash);
	}

	*res = hash;

clean:
	return s_uccess;
}

Bool SHRegisterRuntime_createCopy(const SHRegisterRuntime *reg, const Allocator *alloc, SHRegisterRuntime *res, Error *e_rr) {

	Bool s_uccess = true;

	if (!res || !reg)
		retError(clean, Error_nullPointer(!reg ? 0 : 2, "SHRegisterRuntime_createCopy()::reg and res are required"));

	if(res->name.ptr)
		retError(clean, Error_nullPointer(2, "SHRegisterRuntime_createCopy()::res already defined, could indicate memleak"));

	res->reg = reg->reg;
	res->nameHash = reg->nameHash;
	gotoIfError3(clean, CharString_createCopy(reg->name, alloc, &res->name, e_rr));
	gotoIfError3(clean, ListU32_createCopy(reg->arrays, alloc, &res->arrays, e_rr));
	gotoIfError3(clean, SBFile_createCopy(&reg->shaderBuffer, alloc, &res->shaderBuffer, e_rr));

clean:
	return s_uccess;
}

Bool SHBinaryInfo_addRegisterBase(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	SHRegister registr,
	SBFile *sbFile,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	SHRegisterRuntime reg = (SHRegisterRuntime) { 0 };

	if (!registers)
		retError(clean, Error_nullPointer(0, "SHBinaryInfo_addRegisterBase()::registers is required"));

	U64 hash = 0;
	gotoIfError3(clean, SHRegisterRuntime_hash(&registr, name, arrays, sbFile, &hash, e_rr));

	//Find duplicate register (that is legal to add, without conflicting info).
	//The caller hands over the buffer either way, so a duplicate has to consume it too rather than leave it
	//for the caller to trip over: two entrypoints declaring the same push constant block hash identically,
	//and the second one's SBFile_create then refuses a buffer that still holds the first one's variables.

	for(U64 i = 0; i < registers->length; ++i)
		if(registers->ptr[i].hash == hash) {

			if(sbFile) {
				SBFile_free(sbFile, alloc);
				*sbFile = (SBFile) { 0 };
			}

			goto clean;
		}

	//Ensure there's no registers with duplicate name or binding

	gotoIfError3(clean, SHFile_validateRegister(registers, name, arrays, bindings, registr.registerType, e_rr));

	reg.reg = registr;

	if(CharString_isRef(*name))
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &reg.name, e_rr));

	if(arrays && ListU32_isRef(*arrays))
		gotoIfError3(clean, ListU32_createCopy(*arrays, alloc, &reg.arrays, e_rr));

	if(registers->length >= U16_MAX)
		retError(clean, Error_outOfBounds(
			0, registers->length, U16_MAX, "SHBinaryInfo_addRegisterBase() registers out of bounds"
		));

	SHRegisterRuntime tmp = reg;

	if(!CharString_isRef(*name))
		tmp.name = *name;

	if(arrays && !ListU32_isRef(*arrays))
		tmp.arrays = *arrays;

	if(sbFile)
		tmp.shaderBuffer = *sbFile;

	tmp.hash = hash;

	//Name-only hash lets combine pre-filter name matches with a U64 compare instead of a string compare
	tmp.nameHash = Buffer_fnv1a64(CharString_bufferConst(*name), Buffer_fnv1a64Offset);

	gotoIfError3(clean, ListSHRegisterRuntime_pushBack(registers, tmp, alloc, e_rr));

	*name = CharString_createNull();

	if(arrays)
		*arrays = (ListU32) { 0 };

	if(sbFile)
		*sbFile = (SBFile) { 0 };

clean:
	if (!s_uccess)
		SHRegisterRuntime_free(&reg, alloc);

	return s_uccess;
}

Bool ListSHRegisterRuntime_createCopyUnderlying(
	const ListSHRegisterRuntime *orig,
	const Allocator *alloc,
	ListSHRegisterRuntime *dst,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool didAlloc = false;

	if (!orig || !dst)
		retError(clean, Error_nullPointer(
			!orig ? 0 : 2, "ListSHRegisterRuntime_createCopyUnderlying()::orig and dst are required"
		));

	if(dst->ptr)
		retError(clean, Error_invalidParameter(
			1, 0, "ListSHRegisterRuntime_createCopyUnderlying()::dst is non zero, could indicate memleak"
		));

	gotoIfError3(clean, ListSHRegisterRuntime_createCopy(*orig, alloc, dst, e_rr));
	didAlloc = true;

	for(U64 i = 0; i < orig->length; ++i) {            //Ensure we don't accidentally free something we shouldn't
		dst->ptrNonConst[i].arrays = (ListU32) { 0 };
		dst->ptrNonConst[i].name = CharString_createNull();
		dst->ptrNonConst[i].shaderBuffer = (SBFile) { 0 };
	}

	for(U64 i = 0; i < orig->length; ++i) {            //Copy
		gotoIfError3(clean, ListU32_createCopy(orig->ptr[i].arrays, alloc, &dst->ptrNonConst[i].arrays, e_rr));
		gotoIfError3(clean, CharString_createCopy(orig->ptr[i].name, alloc, &dst->ptrNonConst[i].name, e_rr));
		gotoIfError3(clean, SBFile_createCopy(&orig->ptr[i].shaderBuffer, alloc, &dst->ptrNonConst[i].shaderBuffer, e_rr));
	}

clean:

	if(didAlloc && !s_uccess)
		ListSHRegisterRuntime_freeUnderlying(dst, alloc);

	return s_uccess;
}

Bool ListSHRegisterRuntime_addSampler(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	Bool isSamplerComparisonState,
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
) {
	return SHBinaryInfo_addRegisterBase(
		registers,
		name,
		arrays,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.registerType = (U8)(
				isSamplerComparisonState ? EGfxRegisterType_SamplerComparisonState : EGfxRegisterType_Sampler
			),
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	);
}

Bool ListSHRegisterRuntime_addBuffer(
	ListSHRegisterRuntime *registers,
	ESHBufferType registerType,
	Bool isWrite,
	U8 isUsedFlag,
	CharString *name,
	ListU32 *arrays,
	SBFile *sbFile,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool isCBV = registerType == ESHBufferType_ConstantBuffer || registerType == ESHBufferType_PushConstants;

	if (registerType >= ESHBufferType_Count)
		retError(clean, Error_outOfBounds(
			1, registerType, ESHBufferType_Count, "ListSHRegisterRuntime_addBuffer()::registerType was invalid"
		));

	if(registerType == ESHBufferType_AccelerationStructure || registerType == ESHBufferType_ByteAddressBuffer) {
		if(sbFile)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile should be NULL if the type is acceleration structure or BAB"
			));
	}

	else {

		if(!sbFile || !sbFile->bufferSize)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile is required"
			));

		if(!(sbFile->flags & ESBSettingsFlags_IsTightlyPacked) != isCBV)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile needs to packing to match CBV/SRV type"
			));

		if(isCBV && sbFile->bufferSize >= 64 * KIBI)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile is limited to 64KiB if it's a constant buffer"
			));
	}

	switch (registerType) {

		case ESHBufferType_StructuredBufferAtomic:
		case ESHBufferType_StorageBufferAtomic:

			if(!isWrite)
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addBuffer()::registerType needs write flag to always be enabled"
				));

			break;

		case ESHBufferType_ConstantBuffer:
		case ESHBufferType_AccelerationStructure:
		case ESHBufferType_PushConstants:

			if(isWrite)
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addBuffer()::registerType was incompatible with write flag"
				));

			break;

		default:
			break;
	}

	gotoIfError3(clean, SHBinaryInfo_addRegisterBase(
		registers,
		name,
		arrays,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.registerType = (U8)(
				(EGfxRegisterType_BufferStart + registerType) |
				(isWrite ? EGfxRegisterType_IsWrite : 0)
			),
			.isUsedFlag = isUsedFlag
		},
		sbFile,
		alloc,
		e_rr
	));

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addTextureBase(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	Bool isCombinedSampler,
	Bool isWrite,
	U8 isUsedFlag,
	EGfxTexturePrimitive textureFormatPrimitive,
	ETextureFormatId textureFormatId,
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (registerType >= ESHTextureType_Count)
		retError(clean, Error_outOfBounds(
			1, registerType, ESHTextureType_Count, "ListSHRegisterRuntime_addRWTexture()::registerType was invalid"
		));

	if(textureFormatId >= ETextureFormatId_Count)
		retError(clean, Error_outOfBounds(
			5, textureFormatId, ETextureFormatId_Count, "ListSHRegisterRuntime_addRWTexture()::textureFormatId out of bounds"
		));

	if(
		(textureFormatPrimitive & EGfxTexturePrimitive_TypeMask) > EGfxTexturePrimitive_Count ||
		(textureFormatPrimitive & EGfxTexturePrimitive_Unused)
	)
		retError(clean, Error_outOfBounds(
			5, textureFormatPrimitive, EGfxTexturePrimitive_Count,
			"ListSHRegisterRuntime_addRWTexture()::textureFormatPrimitive out of bounds"
		));

	ETextureFormat format = ETextureFormatId_unpack[textureFormatId];
	EGfxTexturePrimitive primitive = EGfxTexturePrimitive_Count;

	if(textureFormatId) {

		ETexturePrimitive texPrim = ETextureFormat_getPrimitive(format);
		U8 channels = ETextureFormat_getChannels(format);

		switch (texPrim) {

			case ETexturePrimitive_UInt:     primitive = EGfxTexturePrimitive_UInt;         break;
			case ETexturePrimitive_SInt:     primitive = EGfxTexturePrimitive_SInt;         break;
			case ETexturePrimitive_UNorm:    primitive = EGfxTexturePrimitive_UNorm;        break;
			case ETexturePrimitive_SNorm:    primitive = EGfxTexturePrimitive_SNorm;        break;

			case ETexturePrimitive_Float:
				primitive = EGfxTexturePrimitive_Float;
				//TODO: 64 bit texture formats
				//primitive =
				// ETextureFormat_getRedBits(format) == 64 ? EGfxTexturePrimitive_Double : EGfxTexturePrimitive_Float;
				break;

			default:
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addRWTexture() texture format is incompatible"
				));
		}

		switch (channels) {
			case 1:        primitive |= EGfxTexturePrimitive_Component1;    break;
			case 2:        primitive |= EGfxTexturePrimitive_Component2;    break;
			case 3:        primitive |= EGfxTexturePrimitive_Component3;    break;
			case 4:        primitive |= EGfxTexturePrimitive_Component4;    break;
			default:
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addRWTexture() texture format is incompatible"
				));
		}

		//Norm formats want norm templates (unorm/snorm float): DXIL reflection keeps the norm primitive, so
		// requiring agreement here catches a template/format mismatch that would read garbage at runtime
		// (a plain float template makes DXC's SPIRV claim a 32-bit float image format).

		if(
			textureFormatPrimitive != primitive &&
			(textureFormatPrimitive & EGfxTexturePrimitive_TypeMask) != EGfxTexturePrimitive_Count
		)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addRWTexture() texture primitive is incompatible"
			));
	}

	else primitive = textureFormatPrimitive;

	gotoIfError3(clean, SHBinaryInfo_addRegisterBase(
		registers,
		name,
		arrays,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.registerType = (U8)(
				(EGfxRegisterType_TextureStart + registerType) |
				(isWrite ? EGfxRegisterType_IsWrite : 0) |
				(isCombinedSampler ? EGfxRegisterType_IsCombinedSampler : 0) |
				(isLayeredTexture ? EGfxRegisterType_IsArray : 0)
			),
			.texture = (GfxTextureFormat) {
				.formatId = textureFormatId,
				.primitive = textureFormatPrimitive
			},
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	));

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	Bool isCombinedSampler,
	U8 isUsedFlag,
	EGfxTexturePrimitive textureFormatPrimitive,
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
) {
	return ListSHRegisterRuntime_addTextureBase(
		registers,
		registerType,
		isLayeredTexture,
		isCombinedSampler,
		false,
		isUsedFlag,
		textureFormatPrimitive,
		ETextureFormatId_Undefined,
		name,
		arrays,
		bindings,
		alloc,
		e_rr
	);
}

Bool ListSHRegisterRuntime_addRWTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	U8 isUsedFlag,
	EGfxTexturePrimitive textureFormatPrimitive,
	ETextureFormatId textureFormatId,
	CharString *name,
	ListU32 *arrays,
	GfxBindings bindings,
	const Allocator *alloc,
	Error *e_rr
) {
	return ListSHRegisterRuntime_addTextureBase(
		registers,
		registerType,
		isLayeredTexture,
		false,
		true,
		isUsedFlag,
		textureFormatPrimitive,
		textureFormatId,
		name,
		arrays,
		bindings,
		alloc,
		e_rr
	);
}

Bool ListSHRegisterRuntime_addSubpassInput(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	CharString *name,
	GfxBindings bindings,
	U16 inputAttachmentId,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	if (inputAttachmentId >= 8)
		retError(clean, Error_outOfBounds(
			4, inputAttachmentId, 8, "ListSHRegisterRuntime_addSubpassInput()::inputAttachmentId out of bounds"
		));

	for(U8 i = 0; i < EGfxBinaryType_Count; ++i)
		if(i != EGfxBinaryType_SPIRV && (bindings.arr[i].space != U32_MAX || bindings.arr[i].binding != U32_MAX))
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addSubpassInput() can only have bindings for SPIRV"
			));

	gotoIfError3(clean, SHBinaryInfo_addRegisterBase(
		registers,
		name,
		NULL,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.inputAttachmentId = inputAttachmentId,
			.registerType = (U8)EGfxRegisterType_SubpassInput,
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	));

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addRegister(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	SHRegister reg,
	SBFile *sbFile,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	U32 baseRegType = reg.registerType & EGfxRegisterType_TypeMask;

	switch (baseRegType) {

		case EGfxRegisterType_ConstantBuffer:
		case EGfxRegisterType_PushConstants:
		case EGfxRegisterType_ByteAddressBuffer:
		case EGfxRegisterType_StructuredBuffer:
		case EGfxRegisterType_StructuredBufferAtomic:
		case EGfxRegisterType_StorageBuffer:
		case EGfxRegisterType_StorageBufferAtomic:
		case EGfxRegisterType_AccelerationStructure:

			if (reg.registerType & (EGfxRegisterType_Masks & ~EGfxRegisterType_IsWrite))
				retError(clean, Error_invalidParameter(
					2, 4,
					"ListSHRegisterRuntime_addRegister()::registerType buffer needs to be R/W only "
					"(not array or combined sampler)"
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				(ESHBufferType)(baseRegType - EGfxRegisterType_BufferStart),
				reg.registerType & EGfxRegisterType_IsWrite,
				reg.isUsedFlag,
				name,
				arrays,
				sbFile,
				reg.bindings,
				alloc,
				e_rr
			));

			break;

		case EGfxRegisterType_SamplerComparisonState:
		case EGfxRegisterType_Sampler: {

			U32 regType = reg.registerType;
			Bool isComparisonState = regType == EGfxRegisterType_SamplerComparisonState;

			if(reg.padding)
				retError(clean, Error_invalidParameter(
					2, 5, "ListSHRegisterRuntime_addRegister()::padding is invalid (non zero)"
				));

			if(sbFile)
				retError(clean, Error_invalidParameter(
					2, 7, "ListSHRegisterRuntime_addRegister()::sbFile on sampler not allowed"
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				reg.isUsedFlag,
				isComparisonState,
				name,
				arrays,
				reg.bindings,
				alloc,
				e_rr
			));

			break;
		}

		case EGfxRegisterType_SubpassInput:

			if(reg.registerType != EGfxRegisterType_SubpassInput)
				retError(clean, Error_invalidParameter(2, 0, "ListSHRegisterRuntime_addRegister()::registerType is invalid"));

			if(arrays)
				retError(clean, Error_invalidParameter(
					2, 2, "ListSHRegisterRuntime_addRegister()::arrays on subpassInput not allowed"
				));

			if(sbFile)
				retError(clean, Error_invalidParameter(
					2, 3, "ListSHRegisterRuntime_addRegister()::sbFile on subpassInput not allowed"
				));

			gotoIfError3(clean, ListSHRegisterRuntime_addSubpassInput(
				registers,
				reg.isUsedFlag,
				name,
				reg.bindings,
				reg.inputAttachmentId,
				alloc,
				e_rr
			));

			break;

		default:

			if(baseRegType < EGfxRegisterType_TextureStart || baseRegType >= EGfxRegisterType_TextureEnd)
				retError(clean, Error_invalidParameter(2, 0, "ListSHRegisterRuntime_addRegister()::registerType is invalid"));

			if(sbFile)
				retError(clean, Error_invalidParameter(
					2, 3, "ListSHRegisterRuntime_addRegister()::sbFile on texture not allowed"
				));

			if (reg.registerType & EGfxRegisterType_IsWrite) {

				if(reg.registerType & EGfxRegisterType_IsCombinedSampler)
					retError(clean, Error_invalidParameter(
						2, 0, "ListSHRegisterRuntime_addRegister() RWTexture can't contain combined sampler"
					));

				gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
					registers,
					(ESHTextureType)(baseRegType - EGfxRegisterType_TextureStart),
					reg.registerType & EGfxRegisterType_IsArray,
					reg.isUsedFlag,
					(EGfxTexturePrimitive) reg.texture.primitive,
					(ETextureFormatId) reg.texture.formatId,
					name,
					arrays,
					reg.bindings,
					alloc,
					e_rr
				));
			}

			else {

				if(reg.texture.formatId)
					retError(clean, Error_invalidParameter(
						3, 5, "ListSHRegisterRuntime_addRegister()::texture.formatId isn't allowed only readonly texture"
					));

				gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
					registers,
					(ESHTextureType)(baseRegType - EGfxRegisterType_TextureStart),
					reg.registerType & EGfxRegisterType_IsArray,
					reg.registerType & EGfxRegisterType_IsCombinedSampler,
					reg.isUsedFlag,
					(EGfxTexturePrimitive) reg.texture.primitive,
					name,
					arrays,
					reg.bindings,
					alloc,
					e_rr
				));
			}

			break;
	}

clean:
	return s_uccess;
}

const C8 *EGfxTexturePrimitive_name[EGfxTexturePrimitive_CountAll] = {
	"uint",  "int",  "unorm float",  "snorm float",  "float",  "double",  "", "", "", "", "", "", "", "", "", "",
	"uint2", "int2", "unorm float2", "snorm float2", "float2", "double2", "", "", "", "", "", "", "", "", "", "",
	"uint3", "int3", "unorm float3", "snorm float3", "float3", "double3", "", "", "", "", "", "", "", "", "", "",
	"uint4", "int4", "unorm float4", "snorm float4", "float4", "double4", "", "", "", "", "", "", "", "", "", ""
};

void SHRegister_printBindings(
	EGfxRegisterType type,
	GfxBindings bindings,
	const Allocator *alloc,
	const C8 *prefix,
	const C8 *indent
) {

	GfxBinding spirvBinding = bindings.arr[EGfxBinaryType_SPIRV];

	if(spirvBinding.space != U32_MAX || spirvBinding.binding != U32_MAX)
		Log_debugLn(
			alloc,
			"%s%s[[vk::binding(%"PRIu32", %"PRIu32")]]",
			indent,
			prefix,
			spirvBinding.binding,
			spirvBinding.space
		);

	GfxBinding dxilBinding = bindings.arr[EGfxBinaryType_DXIL];

	if(dxilBinding.space != U32_MAX || dxilBinding.binding != U32_MAX) {

		Bool isCBV = type == EGfxRegisterType_ConstantBuffer || type == EGfxRegisterType_PushConstants;

		C8 letter = isCBV ? 'b' : (
			type == EGfxRegisterType_Sampler || type == EGfxRegisterType_SamplerComparisonState ? 's' : (
				type & EGfxRegisterType_IsWrite ? 'u' : 't'
			)
		);

		Log_debugLn(
			alloc,
			"%s: register(%c%"PRIu32", space%"PRIu32")",
			indent,
			letter,
			dxilBinding.binding,
			dxilBinding.space
		);
	}
}

void SHRegister_print(const SHRegister *reg, U64 indenting, Bool isVerbose, const Allocator *alloc) {

	if (!reg)
		return;

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SHRegister_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	switch (reg->registerType & EGfxRegisterType_TypeMask) {

		case EGfxRegisterType_SubpassInput:
			Log_debugLn(alloc, "%sinput_attachment_index = %"PRIu8, indent, reg->inputAttachmentId);
			break;

		case EGfxRegisterType_Sampler:                   Log_debugLn(alloc, "%sSamplerState", indent);                    break;
		case EGfxRegisterType_SamplerComparisonState:    Log_debugLn(alloc, "%sSamplerComparisonState", indent);          break;
		case EGfxRegisterType_ConstantBuffer:            Log_debugLn(alloc, "%sConstantBuffer", indent);                  break;
		case EGfxRegisterType_PushConstants:             Log_debugLn(alloc, "%sPushConstants", indent);                   break;
		case EGfxRegisterType_AccelerationStructure:     Log_debugLn(alloc, "%sRaytracingAccelerationStructure", indent); break;

		case EGfxRegisterType_ByteAddressBuffer:
			Log_debugLn(alloc, "%s%sByteAddressBuffer", indent, reg->registerType & EGfxRegisterType_IsWrite ? "RW" : "");
			break;

		case EGfxRegisterType_StructuredBuffer:
			Log_debugLn(alloc, "%s%sStructuredBuffer", indent, reg->registerType & EGfxRegisterType_IsWrite ? "RW" : "");
			break;

		case EGfxRegisterType_StorageBuffer:
			Log_debugLn(alloc, "%s%sStorageBuffer", indent, reg->registerType & EGfxRegisterType_IsWrite ? "RW" : "");
			break;

		case EGfxRegisterType_StorageBufferAtomic:
			Log_debugLn(alloc, "%s%sStorageBufferAtomic", indent, reg->registerType & EGfxRegisterType_IsWrite ? "RW" : "");
			break;

		case EGfxRegisterType_StructuredBufferAtomic:
			Log_debugLn(alloc, "%sAppend/ConsumeBuffer", indent);
			break;

		default: {

			const C8 *dim = "2D";

			switch(reg->registerType & EGfxRegisterType_TypeMask) {

				case EGfxRegisterType_Texture2D:                           break;
				case EGfxRegisterType_Texture1D:        dim = "1D";        break;
				case EGfxRegisterType_Texture3D:        dim = "3D";        break;
				case EGfxRegisterType_TextureCube:      dim = "Cube";      break;
				case EGfxRegisterType_Texture2DMS:      dim = "2DMS";      break;
			}

			Log_debugLn(
				alloc, "%s%s%s%s%s",
				indent,
				reg->registerType & EGfxRegisterType_IsWrite ? "RW" : "",
				reg->registerType & EGfxRegisterType_IsCombinedSampler ? "sampler" : "Texture",
				dim,
				reg->registerType & EGfxRegisterType_IsArray ? "Array" : ""
			);

			if(reg->texture.formatId)
				Log_debugLn(alloc, "%s%s", indent, ETextureFormatId_name[reg->texture.formatId]);

			if(reg->texture.primitive != EGfxTexturePrimitive_Count)
				Log_debugLn(alloc, "%s%s", indent, EGfxTexturePrimitive_name[reg->texture.primitive]);

			break;
		}
	}

	if(isVerbose)
		SHRegister_printBindings(reg->registerType, reg->bindings, alloc, "", indent);
}

Bool SHRegister_isPresentIn(const SHRegister *reg, EGfxBinaryType type) {

	if(!reg || type >= EGfxBinaryType_Count)
		return false;

	//A SPIRV push constant lives in no descriptor set, so it has no binding to go by

	if(reg->registerType == EGfxRegisterType_PushConstants && type == EGfxBinaryType_SPIRV)
		return (reg->isUsedFlag >> type) & 1;

	const GfxBinding binding = reg->bindings.arr[type];
	return !(binding.binding == U32_MAX && binding.space == U32_MAX);
}

void SHRegisterRuntime_print(const SHRegisterRuntime *reg, U64 indenting, Bool isVerbose, const Allocator *alloc) {

	if (!reg)
		return;

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SHRegisterRuntime_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	Log_debug(
		alloc,
		ELogOptions_None,
		"%s%.*s",
		indent,
		(int)CharString_length(reg->name), reg->name.ptr
	);

	for(U64 i = 0; i < reg->arrays.length; ++i)
		Log_debug(
			alloc,
			ELogOptions_None,
			"[%"PRIu32"]",
			reg->arrays.ptr[i]
		);

	for(U8 i = 0; i < EGfxBinaryType_Count; ++i) {

		if(!SHRegister_isPresentIn(&reg->reg, (EGfxBinaryType) i))
			continue;

		Log_debug(
			alloc, ELogOptions_None,
			(reg->reg.isUsedFlag >> i) & 1 ? " (%s: Used)" : " (%s: Unused)",
			EGfxBinaryType_names[i]
		);
	}

	Log_debugLn(alloc, "");

	SHRegister_print(&reg->reg, indenting + 1, isVerbose, alloc);

	if(reg->shaderBuffer.vars.ptr && isVerbose)
		SBFile_print(&reg->shaderBuffer, indenting + 1, U16_MAX, true, alloc);
}

void ListSHRegisterRuntime_print(const ListSHRegisterRuntime *reg, U64 indenting, Bool isVerbose, const Allocator *alloc) {

	if (!reg)
		return;

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "ListSHRegisterRuntime_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	Log_debugLn(alloc, "%sRegisters:", indent);

	for(U64 i = 0; i < reg->length; ++i)
		SHRegisterRuntime_print(&reg->ptr[i], indenting + 1, isVerbose, alloc);
}

void SHRegisterRuntime_free(SHRegisterRuntime *reg, const Allocator *alloc) {

	if(!reg)
		return;

	CharString_free(&reg->name, alloc);
	ListU32_free(&reg->arrays, alloc);
	SBFile_free(&reg->shaderBuffer, alloc);
}

void ListSHRegisterRuntime_freeUnderlying(ListSHRegisterRuntime *reg, const Allocator *alloc) {

	if(!reg)
		return;

	for(U64 i = 0; i < reg->length; ++i)
		SHRegisterRuntime_free(&reg->ptrNonConst[i], alloc);

	ListSHRegisterRuntime_free(reg, alloc);
}
