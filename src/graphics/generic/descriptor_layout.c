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

//graphics/generic/descriptor_layout.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/base/string_read.h"
#include "types/base/constants.h"

TListImpl(DescriptorBinding);

static inline U8 getDxilRegisterType(ESHRegisterType type) {

	U8 regType = type & ESHRegisterType_TypeMask;

	if(regType == ESHRegisterType_ConstantBuffer || regType == ESHRegisterType_PushConstants)
		return 2;

	if(regType == ESHRegisterType_Sampler || regType == ESHRegisterType_SamplerComparisonState)
		return 3;

	return type & ESHRegisterType_IsWrite;
}

Bool DescriptorBinding_overlaps(
	const DescriptorBinding *binding,
	ESHRegisterType regType,
	const SHBinding *b,
	U32 bcount,
	ESHBinaryType type,
	Bool isPushConstant
) {

	if(!binding || !b)
		return false;

	SHBinding a = binding->binding;

	switch (type) {

		//SPIRV; register intersection only happens if they're identical

		case ESHBinaryType_SPIRV:
			return a.space == b->space && a.binding == b->binding && !isPushConstant;

		//DXIL; register intersection happens when the range overlaps

		case ESHBinaryType_DXIL:

			if (a.space == b->space && regType == getDxilRegisterType(binding->registerType))
				return a.binding + binding->count > b->binding && a.binding < b->binding + bcount;

			return false;

		default:
			return false;
	}
}

Bool GraphicsDeviceRef_detectLayoutFromEntries(
	GraphicsDeviceRef *dev,
	const SHFile *binary,
	const ListU32 *entrypoints,
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	const ListCharString *pushDescriptors,
	const CharString *pushConstantName,
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	Bool init = false;

	if(!info || !entrypoints || !binary || !entrypoints->length)
		retError(clean, Error_nullPointer(
			!entrypoints || !entrypoints->length ? 2 : (!binary ? 1 : 4),
			"DescriptorLayoutInfo_detect()::info, binary and entrypoints are required"
		));

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "DescriptorLayoutInfo_detect()::dev is required"));

	if(info->bindings.ptr)
		retError(clean, Error_invalidParameter(
			4, 0, "DescriptorLayoutInfo_detect()::info was already defined, possible memleak"
		));

	if(
		(
			(pushConstantName && CharString_length(*pushConstantName)) ||
			(detectFlags & EDetectDescriptorLayoutFlags_AssumePushConstants)
		) != !!pushConstantOut
	)
		retError(clean, Error_invalidParameter(
			4, 0,
			"DescriptorLayoutInfo_detect()::pushConstantName/AssumePushConstants can't be present without pushConstantOut"
		));

	if(
		((pushDescriptors && pushDescriptors->length) || (detectFlags & EDetectDescriptorLayoutFlags_AssumePushDescriptors)) !=
		!!pushDescriptorInfo
	)
		retError(clean, Error_invalidParameter(
			4, 0,
			"DescriptorLayoutInfo_detect()::pushDescriptors/AssumePushDescriptors can't be present without pushDescriptorInfo"
		));

	if(flags & EDescriptorLayoutFlags_HasPushDescriptors)
		retError(clean, Error_invalidParameter(
			4, 0,
			"DescriptorLayoutInfo_detect() hasPushDescriptors is not an input value"
		));

	if(pushDescriptorInfo)
		pushDescriptorInfo->flags =
			(flags &~ EDescriptorLayoutFlags_AllowBindlessAny) | EDescriptorLayoutFlags_HasPushDescriptors;

	if(pushDescriptorInfo && pushDescriptorInfo->bindings.length)
		retError(clean, Error_invalidParameter(
			4, 0, "DescriptorLayoutInfo_detect()::pushDescriptorInfo must be empty"
		));

	info->flags = flags;
	init = true;

	gotoIfError3(clean, ListDescriptorBinding_reserve(&info->bindings, 24, alloc, e_rr));
	gotoIfError3(clean, ListCharString_reserve(&info->bindingNames, 24, alloc, e_rr));

	ESHBinaryType binaryType =
		GraphicsInstanceRef_ptr(GraphicsDeviceRef_ptr(dev)->instance)->api == EGraphicsApi_Direct3D12 ?
		ESHBinaryType_DXIL : ESHBinaryType_SPIRV;

	CharString tmp = CharString_createNull();

	for (U64 i = 0; i < entrypoints->length; ++i) {

		U32 epPacked = entrypoints->ptr[i];
		U16 entrypointId = (U16) epPacked;
		U16 binaryId = epPacked >> 16;

		if(binaryId >= binary->binaries.length || entrypointId >= binary->entries.length)
			retError(clean, Error_invalidParameter(
				3, 0, "DescriptorLayoutInfo_detect()::entrypoints binary or entry index out of bounds"
			));

		const SHBinaryInfo *bin = &binary->binaries.ptr[binaryId];

		Bool hasPushConstants = false;

		for(U64 j = 0; j < bin->registers.length; ++j)
			if(bin->registers.ptr[j].reg.registerType == ESHRegisterType_PushConstants) {

				if(hasPushConstants)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorLayoutInfo_detect() already has a push constant, two aren't allowed at once"
					));

				hasPushConstants = true;
			}

		for(U64 j = 0; j < bin->registers.length; ++j) {

			const SHRegisterRuntime *reg = &bin->registers.ptr[j];
			U64 registerNameMatch = U64_MAX;
			U64 registerBindingsMatch = U64_MAX;
			const SHBinding *regMatch = &reg->reg.bindings.arr[binaryType];

			U32 count = 1;

			for(U64 k = 0; k < reg->arrays.length; ++k)
				count *= reg->arrays.ptr[k];

			//PushConstants are the only one where the bindings can be invalid for SPIRV
			//This is because they don't map to a register, while in DXIL they do

			Bool validBinding = !(regMatch->binding == U32_MAX && regMatch->space == U32_MAX);
			Bool anyBinding = validBinding;

			if(reg->reg.registerType == ESHRegisterType_PushConstants && binaryType == ESHBinaryType_SPIRV)
				validBinding = (reg->reg.isUsedFlag >> binaryType) & 1;

			if(!validBinding)    //Doesn't exist in current binary type
				continue;

			U8 regType = getDxilRegisterType(reg->reg.registerType);

			//Find matching register by name or binding

			for(U64 k = 0; k < info->bindingNames.length; ++k) {

				if (CharString_equalsString(&info->bindingNames.ptr[k], &reg->name, EStringCase_Sensitive))
					registerNameMatch = k;

				if(
					anyBinding &&
					DescriptorBinding_overlaps(&info->bindings.ptr[k], regType, regMatch, count, binaryType, false)
				) {

					if(
						info->bindings.ptr[k].binding.binding != regMatch->binding ||
						info->bindings.ptr[k].count != count
					)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorLayoutInfo_detect() dxil register range and/or count conflicts"
						));

					registerBindingsMatch = k;
				}
			}

			if(registerNameMatch != registerBindingsMatch)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorLayoutInfo_detect() detected mismatching register names with same binding"
				));

			//Find visibility; e.g. by checking if it's unused and by finding all entries

			U32 visibility = 0;

			if ((reg->reg.isUsedFlag >> binaryType) & 1)
				visibility |= (U32)1 << binary->entries.ptr[i].stage;

			//Unique register, create another

			ESHRegisterType regType4 = reg->reg.registerType & ESHRegisterType_TypeMask;
			Bool isCBuffer = regType4 == ESHRegisterType_ConstantBuffer;

			Bool hasStrideOrLen =
				isCBuffer ||
				reg->reg.registerType == ESHRegisterType_PushConstants ||
				regType4 == ESHRegisterType_StructuredBuffer ||
				regType4 == ESHRegisterType_StructuredBufferAtomic;

			Bool isTexture = regType4 >= ESHRegisterType_TextureStart && regType4 <= ESHRegisterType_TextureEnd;
			Bool isSampler = regType4 == ESHRegisterType_Sampler || regType4 == ESHRegisterType_SamplerComparisonState;

			DescriptorBinding binding = (DescriptorBinding) {
				.registerType = reg->reg.registerType,
				.count = count,
				.binding = *regMatch,
				.visibility = visibility
			};

			if(hasStrideOrLen)
				binding.data = reg->shaderBuffer.bufferSize;    //Same as setting structuredBufferSize/constantBufferSize

			else if(isTexture)
				binding.textureFormat = reg->reg.texture;

			//Push constants don't generate any descriptor bindings to allow sharing DescriptorLayout

			Bool isPushConstants = regType4 == ESHRegisterType_PushConstants;

			if ((!hasPushConstants && isCBuffer) || isPushConstants) {

				if(
					isPushConstants ||
					(
						(detectFlags & EDetectDescriptorLayoutFlags_AssumePushConstants) &&
						(!pushConstantName || !CharString_length(*pushConstantName)) &&
						reg->shaderBuffer.bufferSize <= 128
					) ||
					CharString_equalsString(&reg->name, pushConstantName, EStringCase_Sensitive)
				) {

					if(reg->shaderBuffer.bufferSize > 128)
						retError(clean, Error_invalidParameter(
							3, 0,
							"DescriptorLayoutInfo_detect() pushConstant referenced a constant buffer >128 "
							"which is illegal as push constants"
						));

					*pushConstantOut = binding;
					hasPushConstants = true;
					continue;
				}
			}

			Bool isPushDescriptor = false;

			if((detectFlags & EDetectDescriptorLayoutFlags_AssumePushDescriptors) && count == 1 && !isSampler)
				isPushDescriptor = true;

			else for(U64 k = 0; k < (!pushDescriptors ? 0 : pushDescriptors->length); ++k)        //TODO: Hash
				if (CharString_equalsString(&reg->name, &pushDescriptors->ptr[k], EStringCase_Sensitive)) {

					if(isSampler)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorLayoutInfo_detect() pushDescriptors contained a sampler, which is not allowed"
						));

					isPushDescriptor = true;
					break;
				}

			if(isPushDescriptor) {

				if(CharString_length(reg->name)) {
					gotoIfError3(clean, CharString_createCopy(reg->name, alloc, &tmp, e_rr));
					gotoIfError3(clean, ListCharString_pushBack(&pushDescriptorInfo->bindingNames, tmp, alloc, e_rr));
					tmp = CharString_createNull();
				}

				gotoIfError3(clean, ListDescriptorBinding_pushBack(&pushDescriptorInfo->bindings, binding, alloc, e_rr));
				continue;
			}

			if (registerNameMatch == U64_MAX) {

				gotoIfError3(clean, CharString_createCopy(reg->name, alloc, &tmp, e_rr));
				gotoIfError3(clean, ListCharString_pushBack(&info->bindingNames, tmp, alloc, e_rr));
				tmp = CharString_createNull();

				gotoIfError3(clean, ListDescriptorBinding_pushBack(&info->bindings, binding, alloc, e_rr));
				continue;
			}

			//Validate bindings

			const DescriptorBinding *dk = &info->bindings.ptr[registerNameMatch];

			if(reg->reg.registerType != dk->registerType || count != dk->count)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorLayoutInfo_detect() mismatching register count or register type"
				));

			if (hasStrideOrLen) {
				if(dk->data != reg->shaderBuffer.bufferSize)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorLayoutInfo_detect() mismatching stride/buffer length"
					));
			}

			//Validate compatibility of primitive and format

			else if (isTexture && (
				dk->textureFormat.primitive != reg->reg.texture.primitive ||
				dk->textureFormat.formatId != reg->reg.texture.formatId
			))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorLayoutInfo_detect() mismatching texture primitive or format id"
				));

			info->bindings.ptrNonConst[registerNameMatch].visibility |= visibility;
		}
	}

	init = false;

clean:

	if(init) {

		DescriptorLayoutInfo_free(info, alloc);

		if(pushDescriptorInfo)
			DescriptorLayoutInfo_free(pushDescriptorInfo, alloc);
	}

	CharString_free(&tmp, alloc);
	return s_uccess;
}

Bool GraphicsDeviceRef_detectLayoutFromEntry(
	GraphicsDeviceRef *dev,
	const SHFile *binary,
	U32 entrypoint,
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	const ListCharString *pushDescriptors,
	const CharString *pushConstantName,
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
) {

	Bool s_uccess = true;

	ListU32 entrypoints = (ListU32) { 0 };
	gotoIfError3(clean, ListU32_createRefConst(&entrypoint, 1, &entrypoints, e_rr));
	gotoIfError3(clean, GraphicsDeviceRef_detectLayoutFromEntries(
		dev,
		binary,
		&entrypoints,
		flags,
		detectFlags,
		pushDescriptors,
		pushConstantName,
		pushConstantOut,
		info,
		pushDescriptorInfo,
		e_rr
	));

clean:
	return s_uccess;
}

void DescriptorLayoutInfo_free(DescriptorLayoutInfo *info, const Allocator *alloc) {

	if(!info)
		return;

	ListDescriptorBinding_free(&info->bindings, alloc);
	ListCharString_freeUnderlying(&info->bindingNames, alloc);
}

void DescriptorLayout_free(DescriptorLayout *layout, const Allocator *alloc) {

	DescriptorLayout_freeExt(layout, alloc);

	if(!(layout->info.flags & EDescriptorLayoutFlags_InternalWeakDeviceRef))
		RefPtr_dec(&layout->device);

	ListU16_free(&layout->bindlessTypeToBinding, alloc);
	ListU8_free(&layout->bindingToBindlessType, alloc);
	DescriptorLayoutInfo_free(&layout->info, alloc);
}

Bool DescriptorBinding_validate(GraphicsDevice *device, DescriptorBinding b, Bool isPushDescriptor, Error *e_rr) {

	Bool s_uccess = true;

	ESHRegisterType type = b.registerType & ESHRegisterType_TypeMask;

	if(
		type == ESHRegisterType_AccelerationStructure &&
		!(device->info.capabilities.features & EGraphicsFeatures_Raytracing)
	)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.bindings has an RTAS, but device doesn't have RT"
		));

	Bool isCBV = type == ESHRegisterType_ConstantBuffer || type == ESHRegisterType_PushConstants;

	if(isCBV && (!b.constantBufferSize || b.constantBufferSize > 64 * KIBI))
		retError(clean, Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() requires strideOrLength to be equal to the constant buffer size "
			"(0 < x < 64KiB)"
		));

	if(
		(type == ESHRegisterType_StructuredBuffer || type == ESHRegisterType_StructuredBufferAtomic) &&
		!b.structedBufferStride
	)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout() requires strideOrLength to be equal to the structured buffer size"
		));

	if(isPushDescriptor)
		if(type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState)
			retError(clean, Error_invalidOperation(
				0, "GraphicsDeviceRef_createDescriptorLayout() can't create a push descriptor for a sampler"
			));

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_createDescriptorLayout(
	GraphicsDeviceRef *dev,
	DescriptorLayoutInfo *info,
	const CharString *name,
	DescriptorLayoutRef **layoutRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);
	Bool allocated = false;

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorLayout()::dev is required"));

	if(!info)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorLayout()::info is required"));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if(
		(info->flags & EDescriptorLayoutFlags_AllowBindlessAny) &&
		!(device->info.capabilities.features & EGraphicsFeatures_Bindless)
	)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.flags can't include bindless if bindless feature is missing"
		));

	if(info->bindings.length >= U16_MAX)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.bindings.length is limited to U16_MAX"
		));

	Bool isPushDescriptor = info->flags & EDescriptorLayoutFlags_HasPushDescriptors;

	if((info->flags & EDescriptorLayoutFlags_AllowBindlessAny) && isPushDescriptor)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout() bindless is not allowed with push descriptors"
		));

	U64 bindlessTypes = 0;
	U64 bindingCount = 0;

	Bool anySampler = false, anyResource = false;

	for(U64 i = 0; i < info->bindings.length; ++i) {

		DescriptorBinding b = info->bindings.ptr[i];
		bindingCount += b.count;

		if(b.registerType == ESHRegisterType_PushConstants)
			retError(clean, Error_invalidOperation(
				0,
				"GraphicsDeviceRef_createDescriptorLayout() can't contain push constants as a descriptor layout, "
				"provide them using a pipeline layout instead"
			));

		if(b.registerType == ESHRegisterType_Sampler || b.registerType == ESHRegisterType_SamplerComparisonState)
			anySampler = true;

		else anyResource = true;

			gotoIfError3(clean, DescriptorBinding_validate(device, b, isPushDescriptor, e_rr));

		if ((info->flags & EDescriptorLayoutFlags_AllowBindlessAny) && b.count > 1)
			++bindlessTypes;
	}

	if(bindlessTypes > 15)
		retError(clean, Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() more than 15 bindless arrays are present, this is unsupported "
			"due to BindlessDescriptor's limits"
		));

	if(bindingCount > 32 && isPushDescriptor)
		retError(clean, Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() push descriptors are limited to 32"
		));

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->descriptorLayout, layoutRef, e_rr));
	allocated = true;

	if(!(info->flags & EDescriptorLayoutFlags_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(dev));

	DescriptorLayout *layout = DescriptorLayoutRef_ptr(*layoutRef);

	//Log_debugLnx("Create: DescriptorLayout %.*s (%p)", (int) CharString_length(name), name.ptr, layout);

	*layout = (DescriptorLayout) {
		.device = dev,
		.info = *info,
		.anySampler = anySampler,
		.anyResource = anyResource
	};

	layout->info.bindingNames = (ListCharString) { 0 };
	layout->info.bindings = (ListDescriptorBinding) { 0 };

	gotoIfError3(clean, ListCharString_move(&info->bindingNames, alloc, &layout->info.bindingNames, e_rr));

	if(ListDescriptorBinding_isRef(info->bindings)) {
		gotoIfError3(clean, ListDescriptorBinding_createCopy(info->bindings, alloc, &layout->info.bindings, e_rr));
	}

	else layout->info.bindings = info->bindings;

	*info = (DescriptorLayoutInfo) { 0 };

	if (bindlessTypes) {

		gotoIfError3(clean, ListU16_reserve(&layout->bindlessTypeToBinding, bindlessTypes, alloc, e_rr));
		gotoIfError3(clean, ListU8_resize(&layout->bindingToBindlessType, layout->info.bindings.length, alloc, e_rr));

		bindlessTypes = 0;

		for(U16 i = 0; i < (U16) layout->info.bindings.length; ++i) {

			DescriptorBinding b = layout->info.bindings.ptr[i];

			U8 bindlessType = U8_MAX;

			if ((layout->info.flags & EDescriptorLayoutFlags_AllowBindlessAny) && b.count > 1) {
				bindlessType = (U8) bindlessTypes;
				gotoIfError3(clean, ListU16_pushBack(&layout->bindlessTypeToBinding, i, alloc, e_rr));
				++bindlessTypes;
			}

			layout->bindingToBindlessType.ptrNonConst[i] = bindlessType;
		}
	}

	gotoIfError3(clean, GraphicsDeviceRef_createDescriptorLayoutExt(dev, layout, name, e_rr));

clean:

	if(!s_uccess && allocated)
		RefPtr_dec(layoutRef);

	return s_uccess;
}
