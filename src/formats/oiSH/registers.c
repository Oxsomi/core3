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

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	#include "platforms/ext/listx_impl.h"
#else
	#include "types/container/list_impl.h"
#endif

#include "types/container/log.h"
#include "formats/oiSH/sh_file.h"

TListImpl(SHRegister);
TListImpl(SHRegisterRuntime);

#ifndef DISALLOW_SH_OXC3_PLATFORMS

	#include "platforms/platform.h"

	void SHRegister_printx(SHRegister reg, U64 indenting) { SHRegister_print(reg, indenting, Platform_instance->alloc); }

	void SHRegisterRuntime_printx(SHRegisterRuntime reg, U64 indenting) {
		SHRegisterRuntime_print(reg, indenting, Platform_instance->alloc);
	}

	void ListSHRegisterRuntime_printx(ListSHRegisterRuntime reg, U64 indenting) {
		ListSHRegisterRuntime_print(reg, indenting, Platform_instance->alloc);
	}

	Bool ListSHRegisterRuntime_createCopyUnderlyingx(ListSHRegisterRuntime orig, ListSHRegisterRuntime *dst, Error *e_rr) {
		return ListSHRegisterRuntime_createCopyUnderlying(orig, Platform_instance->alloc, dst, e_rr);
	}

	Bool ListSHRegisterRuntime_addBufferx(
		ListSHRegisterRuntime *registers,
		ESHBufferType registerType,
		Bool isWrite,
		U8 isUsedFlag,
		CharString *name,
		ListU32 *arrays,
		SBFile *sbFile,
		SHBindings bindings,
		Error *e_rr
	) {
		return ListSHRegisterRuntime_addBuffer(
			registers, registerType, isWrite, isUsedFlag, name, arrays, sbFile, bindings, Platform_instance->alloc, e_rr
		);
	}

	Bool ListSHRegisterRuntime_addTexturex(
		ListSHRegisterRuntime *registers,
		ESHTextureType registerType,
		Bool isLayeredTexture,
		Bool isCombinedSampler,
		U8 isUsedFlag,
		ESHTexturePrimitive textureFormatPrimitive,
		CharString *name,
		ListU32 *arrays,
		SHBindings bindings,
		Error *e_rr
	) {
		return ListSHRegisterRuntime_addTexture(
			registers, registerType, isLayeredTexture, isCombinedSampler, isUsedFlag, textureFormatPrimitive,
			name, arrays, bindings, Platform_instance->alloc, e_rr
		);
	}

	Bool ListSHRegisterRuntime_addRWTexturex(
		ListSHRegisterRuntime *registers,
		ESHTextureType registerType,
		Bool isLayeredTexture,
		U8 isUsedFlag,
		ESHTexturePrimitive textureFormatPrimitive,		//ESHTexturePrimitive_Count = auto detect from formatId
		ETextureFormatId textureFormatId,				//!textureFormatId = only allowed if primitive is set
		CharString *name,
		ListU32 *arrays,
		SHBindings bindings,
		Error *e_rr
	) {
		return ListSHRegisterRuntime_addRWTexture(
			registers, registerType, isLayeredTexture, isUsedFlag, textureFormatPrimitive, textureFormatId,
			name, arrays, bindings, Platform_instance->alloc, e_rr
		);
	}

	Bool ListSHRegisterRuntime_addSubpassInputx(
		ListSHRegisterRuntime *registers,
		U8 isUsedFlag,
		CharString *name,
		SHBindings bindings,
		U16 attachmentId,
		Error *e_rr
	) {
		return ListSHRegisterRuntime_addSubpassInput(
			registers, isUsedFlag,
			name, bindings, attachmentId, Platform_instance->alloc, e_rr
		);
	}

	Bool ListSHRegisterRuntime_addSamplerx(
		ListSHRegisterRuntime *registers,
		U8 isUsedFlag,
		Bool isSamplerComparisonState,
		CharString *name,
		ListU32 *arrays,
		SHBindings bindings,
		Error *e_rr
	) {
		return ListSHRegisterRuntime_addSampler(
			registers, isUsedFlag, isSamplerComparisonState,
			name, arrays, bindings, Platform_instance->alloc, e_rr
		);
	}

	Bool ListSHRegisterRuntime_addRegisterx(
		ListSHRegisterRuntime *registers,
		CharString *name,
		ListU32 *arrays,
		SHRegister reg,
		SBFile *sbFile,
		Error *e_rr
	) {
		return ListSHRegisterRuntime_addRegister(registers, name, arrays, reg, sbFile, Platform_instance->alloc, e_rr);
	}

#endif

Bool SHFile_detectDuplicate(
	const ListSHRegisterRuntime *info,
	CharString name,
	SHBindings bindings,
	ESHRegisterType type,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool anyBinding = false;

	Bool anyDxilBinding = false;
	Bool anySpvBinding = false;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(bindings.arr[i].space != U32_MAX || bindings.arr[i].binding != U32_MAX) {

			anyBinding = true;

			if(i == ESHBinaryType_DXIL)
				anyDxilBinding = true;

			if(i == ESHBinaryType_SPIRV)
				anySpvBinding = true;
		}

	if(!anyBinding)
		retError(clean, Error_invalidState(0, "SHFile_detectDuplicate()::bindings contained no valid bindings"))

	if(!CharString_length(name))
		retError(clean, Error_invalidParameter(1, 0, "SHFile_detectDuplicate()::name is required"))

	//u, s, b, t registers in DXIL

	U8 registerBindingType = 0;

	switch (type & ESHRegisterType_TypeMask) {

		case ESHRegisterType_Sampler:
		case ESHRegisterType_SamplerComparisonState:
			registerBindingType = 2;
			break;

		case ESHRegisterType_ConstantBuffer:
			registerBindingType = 3;
			break;

		default:
			registerBindingType = type & ESHRegisterType_IsWrite ? 1 : 0;
			break;
	}

	for(U64 i = 0; i < info->length; ++i) {

		SHRegisterRuntime reg = info->ptr[i];

		if(CharString_equalsStringSensitive(reg.name, name))
			retError(clean, Error_invalidState(0, "SHFile_detectDuplicate()::name was already found in SHFile"))

		else {

			if(anySpvBinding) {

				SHBinding dstBinding = reg.reg.bindings.arr[ESHBinaryType_SPIRV];
				SHBinding srcBinding = bindings.arr[ESHBinaryType_SPIRV];

				if(dstBinding.binding == srcBinding.binding && dstBinding.space == srcBinding.space)
					retError(clean, Error_invalidState(
						0, "SHFile_detectDuplicate() SPIRV space & binding combo was already found in SHFile"
					))
			}

			if (anyDxilBinding) {

				SHBinding dstBinding = reg.reg.bindings.arr[ESHBinaryType_DXIL];
				SHBinding srcBinding = bindings.arr[ESHBinaryType_DXIL];

				U8 registerBindingTypei = 0;

				switch (reg.reg.registerType & ESHRegisterType_TypeMask) {

					case ESHRegisterType_Sampler:
					case ESHRegisterType_SamplerComparisonState:
						registerBindingTypei = 2;
						break;

					case ESHRegisterType_ConstantBuffer:
						registerBindingTypei = 3;
						break;

					default:
						registerBindingTypei = reg.reg.registerType & ESHRegisterType_IsWrite ? 1 : 0;
						break;
				}

				if(
					registerBindingType == registerBindingTypei &&
					dstBinding.binding == srcBinding.binding && dstBinding.space == srcBinding.space
				)
					retError(clean, Error_invalidState(
						0, "SHFile_detectDuplicate() DXIL space & binding combo was already found in SHFile"
					))
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
	SHBindings bindings,
	ESHRegisterType type,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!name || !CharString_length(*name))
		retError(clean, Error_nullPointer(0, "SHFile_validateRegister()::name is required"))

	if(arrays && (!arrays->length || arrays->length > 32))
		retError(clean, Error_outOfBounds(1, arrays->length, 32, "SHFile_validateRegister()::arrays.length should be [1, 32]"))

	gotoIfError3(clean, SHFile_detectDuplicate(info, *name, bindings, type, e_rr))

clean:
	return s_uccess;
}

Bool SHRegisterRuntime_hash(SHRegister registr, CharString name, ListU32 *arrays, SBFile *sbFile, U64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if(CharString_length(name) > U32_MAX)
		retError(clean, Error_outOfBounds(
			0, CharString_length(name), U32_MAX, "SHRegisterRuntime_hash() name->length out of bounds"
		))

	if(arrays && arrays->length > U32_MAX)
		retError(clean, Error_outOfBounds(
			0, CharString_length(name), U32_MAX, "SHRegisterRuntime_hash() arrays->length out of bounds"
		))

	if(!res)
		retError(clean, Error_nullPointer(4, "SHRegisterRuntime_hash()::res is required"))

	//Compute hash to find register

	static_assert(sizeof(SHRegister) == sizeof(U64) * (ESHBinaryType_Count + 1), "Expected SHRegister as U64[N + 1]");

	U64 hash = sbFile ? sbFile->hash : Buffer_fnv1a64Offset;
	const U64 *regU64 = (const U64*) &registr;

	for(U64 i = 0; i < ESHBinaryType_Count + 1; ++i)
		hash = Buffer_fnv1a64Single(regU64[i], hash);

	hash = Buffer_fnv1a64Single(CharString_length(name) | ((arrays ? arrays->length : 0) << 32), hash);
	hash = Buffer_fnv1a64(CharString_bufferConst(name), hash);

	if (arrays) {

		const U64 *arraysU64 = (const U64*) &arrays->ptr;

		for(U64 i = 0; i < arrays->length >> 1; ++i)
			hash = Buffer_fnv1a64Single(arraysU64[i], hash);

		if(arrays->length & 1)
			hash = Buffer_fnv1a64Single(arrays->ptr[arrays->length - 1], hash);
	}

	*res = hash;

clean:
	return s_uccess;
}

Bool SHRegisterRuntime_createCopy(SHRegisterRuntime reg, Allocator alloc, SHRegisterRuntime *res, Error *e_rr) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(2, "SHRegisterRuntime_createCopy()::res is required"))

	if(res->name.ptr)
		retError(clean, Error_nullPointer(2, "SHRegisterRuntime_createCopy()::res already defined, could indicate memleak"))

	res->reg = reg.reg;
	gotoIfError2(clean, CharString_createCopy(reg.name, alloc, &res->name))
	gotoIfError2(clean, ListU32_createCopy(reg.arrays, alloc, &res->arrays))
	gotoIfError3(clean, SBFile_createCopy(reg.shaderBuffer, alloc, &res->shaderBuffer, e_rr))

clean:
	return s_uccess;
}

Bool SHBinaryInfo_addRegisterBase(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	SHRegister registr,
	SBFile *sbFile,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	SHRegisterRuntime reg = (SHRegisterRuntime) { 0 };

	if(!registers || !name)
		retError(clean, Error_nullPointer(
			!registers ? 0 : 1, "SHBinaryInfo_addRegisterBase()::registers and name are required"
		))

	U64 hash = 0;
	gotoIfError3(clean, SHRegisterRuntime_hash(registr, *name, arrays, sbFile, &hash, e_rr))

	//Find duplicate register (that is legal to add, though duplicates aren't)

	for(U64 i = 0; i < registers->length; ++i)
		if(registers->ptr[i].hash == hash)
			goto clean;

	//Ensure there's no registers with duplicate name or binding

	gotoIfError3(clean, SHFile_validateRegister(registers, name, arrays, bindings, registr.registerType, e_rr))

	reg.reg = registr;

	if(CharString_isRef(*name))
		gotoIfError2(clean, CharString_createCopy(*name, alloc, &reg.name))

	if(arrays && ListU32_isRef(*arrays))
		gotoIfError2(clean, ListU32_createCopy(*arrays, alloc, &reg.arrays))

	if(registers->length >= U16_MAX)
		retError(clean, Error_outOfBounds(
			0, registers->length, U16_MAX, "SHBinaryInfo_addRegisterBase() registers out of bounds"
		))

	SHRegisterRuntime tmp = reg;

	if(!CharString_isRef(*name))
		tmp.name = *name;

	if(arrays && !ListU32_isRef(*arrays))
		tmp.arrays = *arrays;

	if(sbFile)
		tmp.shaderBuffer = *sbFile;

	tmp.hash = hash;

	gotoIfError2(clean, ListSHRegisterRuntime_pushBack(registers, tmp, alloc))

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

SHBindings SHBindings_dummy() {

	SHBindings bindings = (SHBindings) { 0 };

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		bindings.arr[i] = (SHBinding) { .binding = U32_MAX, .space = U32_MAX };

	return bindings;
}

Bool ListSHRegisterRuntime_createCopyUnderlying(
	ListSHRegisterRuntime orig,
	Allocator alloc,
	ListSHRegisterRuntime *dst,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool didAlloc = false;

	if(!dst)
		retError(clean, Error_nullPointer(1, "ListSHRegisterRuntime_createCopyUnderlying()::dst is required"))

	if(dst->ptr)
		retError(clean, Error_invalidParameter(
			1, 0, "ListSHRegisterRuntime_createCopyUnderlying()::dst is non zero, could indicate memleak"
		))

	gotoIfError2(clean, ListSHRegisterRuntime_createCopy(orig, alloc, dst))
	didAlloc = true;

	for(U64 i = 0; i < orig.length; ++i) {			//Ensure we don't accidentally free something we shouldn't
		dst->ptrNonConst[i].arrays = (ListU32) { 0 };
		dst->ptrNonConst[i].name = CharString_createNull();
		dst->ptrNonConst[i].shaderBuffer = (SBFile) { 0 };
	}

	for(U64 i = 0; i < orig.length; ++i) {			//Copy
		gotoIfError2(clean, ListU32_createCopy(orig.ptr[i].arrays, alloc, &dst->ptrNonConst[i].arrays))
		gotoIfError2(clean, CharString_createCopy(orig.ptr[i].name, alloc, &dst->ptrNonConst[i].name))
		gotoIfError3(clean, SBFile_createCopy(orig.ptr[i].shaderBuffer, alloc, &dst->ptrNonConst[i].shaderBuffer, e_rr))
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
	SHBindings bindings,
	Allocator alloc,
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
				isSamplerComparisonState ? ESHRegisterType_SamplerComparisonState : ESHRegisterType_Sampler
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
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool isCBV = registerType == ESHBufferType_ConstantBuffer;

	if(registerType >= ESHBufferType_Count)
		retError(clean, Error_outOfBounds(
			1, registerType, ESHBufferType_Count, "ListSHRegisterRuntime_addBuffer()::registerType was invalid"
		))

	if(registerType == ESHBufferType_AccelerationStructure || registerType == ESHBufferType_ByteAddressBuffer) {
		if(sbFile)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile should be NULL if the type is acceleration structure or BAB"
			))
	}

	else {

		if(!sbFile || !sbFile->bufferSize)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile is required"
			))

		if(!(sbFile->flags & ESBSettingsFlags_IsTightlyPacked) != isCBV)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile needs to be tightly packed for non CBV and loosely packed for CBV"
			))

		if(isCBV && sbFile->bufferSize >= 64 * KIBI)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile is limited to 64KiB if it's a constant buffer"
			))
	}

	switch (registerType) {

		case ESHBufferType_StructuredBufferAtomic:
		case ESHBufferType_StorageBufferAtomic:

			if(!isWrite)
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addBuffer()::registerType needs write flag to always be enabled"
				))

			break;

		case ESHBufferType_ConstantBuffer:
		case ESHBufferType_AccelerationStructure:

			if(isWrite)
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addBuffer()::registerType was incompatible with write flag"
				))

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
				(ESHRegisterType_BufferStart + registerType) |
				(isWrite ? ESHRegisterType_IsWrite : 0)
			),
			.isUsedFlag = isUsedFlag
		},
		sbFile,
		alloc,
		e_rr
	))

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
	ESHTexturePrimitive textureFormatPrimitive,
	ETextureFormatId textureFormatId,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(registerType >= ESHTextureType_Count)
		retError(clean, Error_outOfBounds(
			1, registerType, ESHTextureType_Count, "ListSHRegisterRuntime_addRWTexture()::registerType was invalid"
		))

	if(textureFormatId >= ETextureFormatId_Count)
		retError(clean, Error_outOfBounds(
			5, textureFormatId, ETextureFormatId_Count, "ListSHRegisterRuntime_addRWTexture()::textureFormatId out of bounds"
		))

	if(
		(textureFormatPrimitive & ESHTexturePrimitive_TypeMask) > ESHTexturePrimitive_Count ||
		(textureFormatPrimitive & ESHTexturePrimitive_Unused)
	)
		retError(clean, Error_outOfBounds(
			5, textureFormatPrimitive, ESHTexturePrimitive_Count,
			"ListSHRegisterRuntime_addRWTexture()::textureFormatPrimitive out of bounds"
		))

	if(textureFormatPrimitive == ESHTexturePrimitive_Count && !textureFormatId && isWrite)
		retError(clean, Error_invalidState(
			0, "ListSHRegisterRuntime_addRWTexture() either texture format primitive or texture format id has to be set for RW"
		))

	ETextureFormat format = ETextureFormatId_unpack[textureFormatId];
	ESHTexturePrimitive primitive = ESHTexturePrimitive_Count;

	if(textureFormatId) {

		ETexturePrimitive texPrim = ETextureFormat_getPrimitive(format);
		U8 channels = ETextureFormat_getChannels(format);

		switch (texPrim) {

			case ETexturePrimitive_UInt:	primitive = ESHTexturePrimitive_UInt;		break;
			case ETexturePrimitive_SInt:	primitive = ESHTexturePrimitive_SInt;		break;
			case ETexturePrimitive_UNorm:	primitive = ESHTexturePrimitive_UNorm;		break;
			case ETexturePrimitive_SNorm:	primitive = ESHTexturePrimitive_SNorm;		break;

			case ETexturePrimitive_Float:
				primitive = ESHTexturePrimitive_Float;
				//TODO: 64 bit texture formats
				//primitive = ETextureFormat_getRedBits(format) == 64 ? ESHTexturePrimitive_Double : ESHTexturePrimitive_Float;
				break;

			default:
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addRWTexture() texture format is incompatible"
				))
		}

		switch (channels) {
			case 1:		primitive |= ESHTexturePrimitive_Component1;	break;
			case 2:		primitive |= ESHTexturePrimitive_Component2;	break;
			case 3:		primitive |= ESHTexturePrimitive_Component3;	break;
			case 4:		primitive |= ESHTexturePrimitive_Component4;	break;
			default:
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addRWTexture() texture format is incompatible"
				))
		}

		if(
			textureFormatPrimitive != primitive &&
			(textureFormatPrimitive & ESHTexturePrimitive_TypeMask) != ESHTexturePrimitive_Count
		)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addRWTexture() texture primitive is incompatible"
			))
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
				(ESHRegisterType_TextureStart + registerType) |
				(isWrite ? ESHRegisterType_IsWrite : 0) |
				(isCombinedSampler ? ESHRegisterType_IsCombinedSampler : 0) |
				(isLayeredTexture ? ESHRegisterType_IsArray : 0)
			),
			.texture = (SHTextureFormat) {
				.formatId = textureFormatId,
				.primitive = textureFormatPrimitive
			},
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	))

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	Bool isCombinedSampler,
	U8 isUsedFlag,
	ESHTexturePrimitive textureFormatPrimitive,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	gotoIfError3(clean, ListSHRegisterRuntime_addTextureBase(
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
	))

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addRWTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	U8 isUsedFlag,
	ESHTexturePrimitive textureFormatPrimitive,
	ETextureFormatId textureFormatId,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
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
	SHBindings bindings,
	U16 inputAttachmentId,
	Allocator alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	if(inputAttachmentId >= 7)
		retError(clean, Error_outOfBounds(
			4, inputAttachmentId, 7, "ListSHRegisterRuntime_addSubpassInput()::inputAttachmentId out of bounds"
		))

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(i != ESHBinaryType_SPIRV && (bindings.arr[i].space != U32_MAX || bindings.arr[i].binding != U32_MAX))
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addSubpassInput() can only have bindings for SPIRV"
			))

	gotoIfError3(clean, SHBinaryInfo_addRegisterBase(
		registers,
		name,
		NULL,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.inputAttachmentId = inputAttachmentId,
			.registerType = (U8)ESHRegisterType_SubpassInput,
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	))

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addRegister(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	SHRegister reg,
	SBFile *sbFile,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	U32 baseRegType = reg.registerType & ESHRegisterType_TypeMask;

	switch (baseRegType) {

		case ESHRegisterType_ConstantBuffer:
		case ESHRegisterType_ByteAddressBuffer:
		case ESHRegisterType_StructuredBuffer:
		case ESHRegisterType_StructuredBufferAtomic:
		case ESHRegisterType_StorageBuffer:
		case ESHRegisterType_StorageBufferAtomic:
		case ESHRegisterType_AccelerationStructure:

			if(reg.registerType & (ESHRegisterType_Masks &~ ESHRegisterType_IsWrite))
				retError(clean, Error_invalidParameter(
					2, 4,
					"ListSHRegisterRuntime_addRegister()::registerType buffer needs to be R/W only (not array or combined sampler)"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				(ESHBufferType)(baseRegType - ESHRegisterType_BufferStart),
				reg.registerType & ESHRegisterType_IsWrite,
				reg.isUsedFlag,
				name,
				arrays,
				sbFile,
				reg.bindings,
				alloc,
				e_rr
			))

			break;

		case ESHRegisterType_SamplerComparisonState:
		case ESHRegisterType_Sampler: {

			U32 regType = reg.registerType;
			Bool isComparisonState = regType == ESHRegisterType_SamplerComparisonState;

			if(regType != ESHRegisterType_Sampler && isComparisonState)
				retError(clean, Error_invalidParameter(2, 4, "ListSHRegisterRuntime_addRegister()::registerType is invalid"))

			if(reg.padding)
				retError(clean, Error_invalidParameter(
					2, 5, "ListSHRegisterRuntime_addRegister()::padding is invalid (non zero)"
				))

			if(sbFile)
				retError(clean, Error_invalidParameter(2, 7, "ListSHRegisterRuntime_addRegister()::sbFile on subpassInput not allowed"))

			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				reg.isUsedFlag,
				isComparisonState,
				name,
				arrays,
				reg.bindings,
				alloc,
				e_rr
			))

			break;
		}

		case ESHRegisterType_SubpassInput:

			if(reg.registerType != ESHRegisterType_SubpassInput)
				retError(clean, Error_invalidParameter(2, 0, "ListSHRegisterRuntime_addRegister()::registerType is invalid"))

			if(arrays)
				retError(clean, Error_invalidParameter(
					2, 2, "ListSHRegisterRuntime_addRegister()::arrays on subpassInput not allowed"
				))

			if(sbFile)
				retError(clean, Error_invalidParameter(
					2, 3, "ListSHRegisterRuntime_addRegister()::sbFile on subpassInput not allowed"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addSubpassInput(
				registers,
				reg.isUsedFlag,
				name,
				reg.bindings,
				reg.inputAttachmentId,
				alloc,
				e_rr
			))

			break;

		default:

			if(baseRegType < ESHRegisterType_TextureStart || baseRegType >= ESHRegisterType_TextureEnd)
				retError(clean, Error_invalidParameter(2, 0, "ListSHRegisterRuntime_addRegister()::registerType is invalid"))

			if(sbFile)
				retError(clean, Error_invalidParameter(2, 3, "ListSHRegisterRuntime_addRegister()::sbFile on subpassInput not allowed"))

			if (reg.registerType & ESHRegisterType_IsWrite) {

				if(reg.registerType & ESHRegisterType_IsCombinedSampler)
					retError(clean, Error_invalidParameter(
						2, 0, "ListSHRegisterRuntime_addRegister() RWTexture can't contain combined sampler"
					))

				gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
					registers,
					(ESHTextureType)(baseRegType - ESHRegisterType_TextureStart),
					reg.registerType & ESHRegisterType_IsArray,
					reg.isUsedFlag,
					(ESHTexturePrimitive) reg.texture.primitive,
					(ETextureFormatId) reg.texture.formatId,
					name,
					arrays,
					reg.bindings,
					alloc,
					e_rr
				))
			}

			else {

				if(reg.texture.formatId)
					retError(clean, Error_invalidParameter(
						3, 5, "ListSHRegisterRuntime_addRegister()::texture.formatId isn't allowed only readonly texture"
					))

				gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
					registers,
					(ESHTextureType)(baseRegType - ESHRegisterType_TextureStart),
					reg.registerType & ESHRegisterType_IsArray,
					reg.registerType & ESHRegisterType_IsCombinedSampler,
					reg.isUsedFlag,
					(ESHTexturePrimitive) reg.texture.primitive,
					name,
					arrays,
					reg.bindings,
					alloc,
					e_rr
				))
			}

			break;
	}

clean:
	return s_uccess;
}

const C8 *ESHTexturePrimitive_name[ESHTexturePrimitive_CountAll] = {
	"uint",  "int",  "unorm float",  "snorm float",  "float",  "double",  "", "", "", "", "", "", "", "", "", "",
	"uint2", "int2", "unorm float2", "snorm float2", "float2", "double2", "", "", "", "", "", "", "", "", "", "",
	"uint3", "int3", "unorm float3", "snorm float3", "float3", "double3", "", "", "", "", "", "", "", "", "", "",
	"uint4", "int4", "unorm float4", "snorm float4", "float4", "double4", "", "", "", "", "", "", "", "", "", ""
};

void SHRegister_printBindings(ESHRegisterType type, SHBindings bindings, Allocator alloc, const C8 *prefix, const C8 *indent) {

	SHBinding spirvBinding = bindings.arr[ESHBinaryType_SPIRV];

	if(spirvBinding.space != U32_MAX || spirvBinding.binding != U32_MAX)
		Log_debugLn(
			alloc,
			"%s%s[[vk::binding(%"PRIu32", %"PRIu32")]]",
			indent,
			prefix,
			spirvBinding.binding,
			spirvBinding.space
		);

	SHBinding dxilBinding = bindings.arr[ESHBinaryType_DXIL];

	if(dxilBinding.space != U32_MAX || dxilBinding.binding != U32_MAX) {

		C8 letter = type == ESHRegisterType_ConstantBuffer ? 'b' : (
			type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState ? 's' : (
				type & ESHRegisterType_IsWrite ? 'u' : 't'
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

void SHRegister_print(SHRegister reg, U64 indenting, Allocator alloc) {

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SHRegister_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	switch (reg.registerType & ESHRegisterType_TypeMask) {

		case ESHRegisterType_SubpassInput:
			Log_debugLn(alloc, "%sinput_attachment_index = %"PRIu8, indent, reg.inputAttachmentId);
			break;

		case ESHRegisterType_Sampler:					Log_debugLn(alloc, "%sSamplerState", indent);					 break;
		case ESHRegisterType_SamplerComparisonState:	Log_debugLn(alloc, "%sSamplerComparisonState", indent);			 break;
		case ESHRegisterType_ConstantBuffer:			Log_debugLn(alloc, "%sConstantBuffer", indent);					 break;
		case ESHRegisterType_AccelerationStructure:		Log_debugLn(alloc, "%sRaytracingAccelerationStructure", indent); break;

		case ESHRegisterType_ByteAddressBuffer:
			Log_debugLn(alloc, "%s%sByteAddressBuffer", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StructuredBuffer:
			Log_debugLn(alloc, "%s%sStructuredBuffer", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StorageBuffer:
			Log_debugLn(alloc, "%s%sStorageBuffer", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StorageBufferAtomic:
			Log_debugLn(alloc, "%s%sStorageBufferAtomic", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StructuredBufferAtomic:
			Log_debugLn(alloc, "%sAppend/ConsumeBuffer", indent);
			break;

		default: {

			const C8 *dim = "2D";

			switch(reg.registerType & ESHRegisterType_TypeMask) {

				case ESHRegisterType_Texture2D:						break;
				case ESHRegisterType_Texture1D:		dim = "1D";		break;
				case ESHRegisterType_Texture3D:		dim = "3D";		break;
				case ESHRegisterType_TextureCube:	dim = "Cube";	break;
				case ESHRegisterType_Texture2DMS:	dim = "2DMS";	break;
			}

			Log_debugLn(
				alloc, "%s%s%s%s%s",
				indent,
				reg.registerType & ESHRegisterType_IsWrite ? "RW" : "",
				reg.registerType & ESHRegisterType_IsCombinedSampler ? "sampler" : "Texture",
				dim,
				reg.registerType & ESHRegisterType_IsArray ? "Array" : ""
			);

			if(reg.texture.formatId)
				Log_debugLn(alloc, "%s%s", indent, ETextureFormatId_name[reg.texture.formatId]);

			if(reg.texture.primitive != ESHTexturePrimitive_Count)
				Log_debugLn(alloc, "%s%s", indent, ESHTexturePrimitive_name[reg.texture.primitive]);

			break;
		}
	}

	SHRegister_printBindings(reg.registerType, reg.bindings, alloc, "", indent);
}

void SHRegisterRuntime_print(SHRegisterRuntime reg, U64 indenting, Allocator alloc) {

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
		(int)CharString_length(reg.name), reg.name.ptr
	);

	for(U64 i = 0; i < reg.arrays.length; ++i)
		Log_debug(
			alloc,
			ELogOptions_None,
			"[%"PRIu32"]",
			reg.arrays.ptr[i]
		);

	for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(reg.reg.bindings.arr[i].space == U32_MAX && reg.reg.bindings.arr[i].binding == U32_MAX)
			continue;

		Log_debug(
			alloc, ELogOptions_None,
			(reg.reg.isUsedFlag >> i) & 1 ? " (%s: Used)" : " (%s: Unused)",
			ESHBinaryType_names[i]
		);
	}

	Log_debugLn(alloc, "");

	SHRegister_print(reg.reg, indenting + 1, alloc);

	if(reg.shaderBuffer.vars.ptr)
		SBFile_print(reg.shaderBuffer, indenting + 1, U16_MAX, true, alloc);
}

void ListSHRegisterRuntime_print(ListSHRegisterRuntime reg, U64 indenting, Allocator alloc) {

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "ListSHRegisterRuntime_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	Log_debugLn(alloc, "%sRegisters:", indent);

	for(U64 i = 0; i < reg.length; ++i)
		SHRegisterRuntime_print(reg.ptr[i], indenting + 1, alloc);
}

void SHRegisterRuntime_free(SHRegisterRuntime *reg, Allocator alloc) {

	if(!reg)
		return;

	CharString_free(&reg->name, alloc);
	ListU32_free(&reg->arrays, alloc);
	SBFile_free(&reg->shaderBuffer, alloc);
}

void ListSHRegisterRuntime_freeUnderlying(ListSHRegisterRuntime *reg, Allocator alloc) {

	if(!reg)
		return;

	for(U64 i = 0; i < reg->length; ++i)
		SHRegisterRuntime_free(&reg->ptrNonConst[i], alloc);

	ListSHRegisterRuntime_free(reg, alloc);
}