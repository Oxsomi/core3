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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "platforms/ext/ref_ptrx.h"
#include "platforms/ext/stringx.h"
#include "types/container/string.h"
#include "formats/oiSH/sh_file.h"

TListImpl(DescriptorBinding);

Error DescriptorLayoutRef_dec(DescriptorLayoutRef **layout) {
	return !RefPtr_dec(layout) ?
		Error_invalidOperation(0, "DescriptorLayoutRef_dec()::layout is required") : Error_none();
}

Error DescriptorLayoutRef_inc(DescriptorLayoutRef *layout) {
	return !RefPtr_inc(layout) ?
		Error_invalidOperation(0, "DescriptorLayoutRef_inc()::layout is required") : Error_none();
}

U8 getDxilRegisterType(ESHRegisterType type) {

	U8 regType = type & ESHRegisterType_TypeMask;

	if(regType == ESHRegisterType_ConstantBuffer || regType == ESHRegisterType_PushConstants)
		return 2;

	if(regType == ESHRegisterType_Sampler || regType == ESHRegisterType_SamplerComparisonState)
		return 3;

	return type & ESHRegisterType_IsWrite;
}

Bool DescriptorBinding_overlaps(
	DescriptorBinding binding,
	ESHRegisterType regType,
	SHBinding b,
	U32 bcount,
	ESHBinaryType type,
	Bool isPushConstant
) {

	SHBinding a = binding.binding;

	switch (type) {

		//SPIRV; register intersection only happens if they're identical

		case ESHBinaryType_SPIRV:
			return a.space == b.space && a.binding == b.binding && !isPushConstant;

		//DXIL; register intersection happens when the range overlaps

		case ESHBinaryType_DXIL:
						
			if (a.space == b.space && regType == getDxilRegisterType(binding.registerType))
				return a.binding + binding.count > b.binding && a.binding < b.binding + bcount;

			return false;

		default:
			return false;
	}
}

Bool GraphicsDeviceRef_detectLayoutFromEntries(
	GraphicsDeviceRef *dev,
	SHFile binary,
	ListU32 entrypoints,
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	ListCharString pushDescriptors,
	CharString pushConstantName,
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool init = false;

	if(!info || !entrypoints.length)
		retError(clean, Error_nullPointer(
			!entrypoints.length ? 2 : 4, "DescriptorLayoutInfo_detect()::info and entrypoints are required"
		))

	if(!dev || dev->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "DescriptorLayoutInfo_detect()::dev is required"))

	if(info->bindings.ptr)
		retError(clean, Error_invalidParameter(
			4, 0, "DescriptorLayoutInfo_detect()::info was already defined, possible memleak"
		))

	if(
		(CharString_length(pushConstantName) || (detectFlags & EDetectDescriptorLayoutFlags_AssumePushConstants)) !=
		!!pushConstantOut
	)
		retError(clean, Error_invalidParameter(
			4, 0,
			"DescriptorLayoutInfo_detect()::pushConstantName/AssumePushConstants can't be present without pushConstantOut"
		))

	if(
		(pushDescriptors.length || (detectFlags & EDetectDescriptorLayoutFlags_AssumePushDescriptors)) !=
		!!pushDescriptorInfo
	)
		retError(clean, Error_invalidParameter(
			4, 0,
			"DescriptorLayoutInfo_detect()::pushDescriptors/AssumePushDescriptors can't be present without pushDescriptorInfo"
		))

	if(flags & EDescriptorLayoutFlags_HasPushDescriptors)
		retError(clean, Error_invalidParameter(
			4, 0,
			"DescriptorLayoutInfo_detect() hasPushDescriptors is not an input value"
		))

	if(pushDescriptorInfo)
		pushDescriptorInfo->flags =
			(flags &~ EDescriptorLayoutFlags_AllowBindlessAny) | EDescriptorLayoutFlags_HasPushDescriptors;

	if(pushDescriptorInfo && pushDescriptorInfo->bindings.length)
		retError(clean, Error_invalidParameter(
			4, 0,
			"DescriptorLayoutInfo_detect()::pushDescriptorInfo must be empty"
		))

	info->flags = flags;
	init = true;

	gotoIfError2(clean, ListDescriptorBinding_reservex(&info->bindings, 24))
	gotoIfError2(clean, ListCharString_reservex(&info->bindingNames, 24))

	ESHBinaryType binaryType =
		GraphicsInstanceRef_ptr(GraphicsDeviceRef_ptr(dev)->instance)->api == EGraphicsApi_Direct3D12 ?
		ESHBinaryType_DXIL : ESHBinaryType_SPIRV;

	CharString tmp = CharString_createNull();

	for (U64 i = 0; i < entrypoints.length; ++i) {

		U16 entrypointId = (U16) entrypoints.ptr[i];
		U16 binaryId = entrypoints.ptr[i] >> 16;

		if(binaryId >= binary.binaries.length || entrypointId >= binary.entries.length)
			retError(clean, Error_invalidParameter(
				3, 0, "DescriptorLayoutInfo_detect()::entrypoints binary or entry index out of bounds"
			))

		SHBinaryInfo bin = binary.binaries.ptr[binaryId];

		Bool hasPushConstants = false;

		for(U64 j = 0; j < bin.registers.length; ++j)
			if(bin.registers.ptr[j].reg.registerType == ESHRegisterType_PushConstants) {

				if(hasPushConstants)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorLayoutInfo_detect() already has a push constant, two aren't allowed at once"
					))

				hasPushConstants = true;
			}

		for(U64 j = 0; j < bin.registers.length; ++j) {

			SHRegisterRuntime reg = bin.registers.ptr[j];
			U64 registerNameMatch = U64_MAX;
			U64 registerBindingsMatch = U64_MAX;
			SHBinding regMatch = reg.reg.bindings.arr[binaryType];

			U32 count = 1;

			for(U64 k = 0; k < reg.arrays.length; ++k)
				count *= reg.arrays.ptr[k];

			//PushConstants are the only one where the bindings can be invalid for SPIRV
			//This is because they don't map to a register, while in DXIL they do

			Bool validBinding = !(regMatch.binding == U32_MAX && regMatch.space == U32_MAX);
			Bool anyBinding = validBinding;

			if(reg.reg.registerType == ESHRegisterType_PushConstants && binaryType == ESHBinaryType_SPIRV)
				validBinding = (reg.reg.isUsedFlag >> binaryType) & 1;

			if(!validBinding)	//Doesn't exist in current binary type
				continue;

			U8 regType = getDxilRegisterType(reg.reg.registerType);

			//Find matching register by name or binding

			for(U64 k = 0; k < info->bindingNames.length; ++k) {

				if (CharString_equalsStringSensitive(info->bindingNames.ptr[k], reg.name))
					registerNameMatch = k;

				if(anyBinding && DescriptorBinding_overlaps(info->bindings.ptr[k], regType, regMatch, count, binaryType, false)) {

					if(
						info->bindings.ptr[k].binding.binding != regMatch.binding ||
						info->bindings.ptr[k].count != count
					)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorLayoutInfo_detect() dxil register range and/or count conflicts"
						))

					registerBindingsMatch = k;
				}
			}

			if(registerNameMatch != registerBindingsMatch)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorLayoutInfo_detect() detected mismatching register names with same binding"
				))

			//Find visibility; e.g. by checking if it's unused and by finding all entries

			U32 visibility = 0;

			if ((reg.reg.isUsedFlag >> binaryType) & 1)
				visibility |= (U32)1 << binary.entries.ptr[i].stage;

			//Unique register, create another

			ESHRegisterType regType4 = reg.reg.registerType & ESHRegisterType_TypeMask;
			Bool isCBuffer = regType4 == ESHRegisterType_ConstantBuffer;

			Bool hasStrideOrLen = 
				isCBuffer ||
				reg.reg.registerType == ESHRegisterType_PushConstants ||
				regType4 == ESHRegisterType_StructuredBuffer ||
				regType4 == ESHRegisterType_StructuredBufferAtomic;

			Bool isTexture = regType4 >= ESHRegisterType_TextureStart && regType4 <= ESHRegisterType_TextureEnd;
			Bool isSampler = regType4 == ESHRegisterType_Sampler || regType4 == ESHRegisterType_SamplerComparisonState;

			DescriptorBinding binding = (DescriptorBinding) {
				.registerType = reg.reg.registerType,
				.count = count,
				.binding = regMatch,
				.visibility = visibility
			};

			if(hasStrideOrLen)
				binding.data = reg.shaderBuffer.bufferSize;		//Same as setting structuredBufferSize/constantBufferSize

			else if(isTexture)
				binding.textureFormat = reg.reg.texture;

			//Push constants don't generate any descriptor bindings to allow sharing DescriptorLayout

			Bool isPushConstants = regType4 == ESHRegisterType_PushConstants;

			if ((!hasPushConstants && isCBuffer) || isPushConstants) {

				if(
					isPushConstants ||
					((detectFlags & EDetectDescriptorLayoutFlags_AssumePushConstants) && !CharString_length(pushConstantName) && reg.shaderBuffer.bufferSize <= 128) ||
					CharString_equalsStringSensitive(reg.name, pushConstantName)
				) {
					
					if(reg.shaderBuffer.bufferSize > 128)
						retError(clean, Error_invalidParameter(
							3, 0,
							"DescriptorLayoutInfo_detect() pushConstant referenced a constant buffer >128 "
							"which is illegal as push constants"
						))

					*pushConstantOut = binding;
					hasPushConstants = true;
					continue;
				}
			}

			Bool isPushDescriptor = false;

			if((detectFlags & EDetectDescriptorLayoutFlags_AssumePushDescriptors) && count == 1 && !isSampler)
				isPushDescriptor = true;

			else for(U64 k = 0; k < pushDescriptors.length; ++k)		//TODO: Hash
				if (CharString_equalsStringSensitive(reg.name, pushDescriptors.ptr[k])) {

					if(isSampler)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorLayoutInfo_detect() pushDescriptors contained a sampler, which is not allowed"
						))

					isPushDescriptor = true;
					break;
				}


			if(isPushDescriptor) {

				if(CharString_length(reg.name)) {
					gotoIfError2(clean, CharString_createCopyx(reg.name, &tmp))
					gotoIfError2(clean, ListCharString_pushBackx(&pushDescriptorInfo->bindingNames, tmp))
					tmp = CharString_createNull();
				}

				gotoIfError2(clean, ListDescriptorBinding_pushBackx(&pushDescriptorInfo->bindings, binding))
				continue;
			}

			if (registerNameMatch == U64_MAX) {

				gotoIfError2(clean, CharString_createCopyx(reg.name, &tmp))
				gotoIfError2(clean, ListCharString_pushBackx(&info->bindingNames, tmp))
				tmp = CharString_createNull();

				gotoIfError2(clean, ListDescriptorBinding_pushBackx(&info->bindings, binding))
				continue;
			}

			//Validate bindings

			DescriptorBinding dk = info->bindings.ptr[registerNameMatch];

			if(reg.reg.registerType != dk.registerType || count != dk.count)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorLayoutInfo_detect() mismatching register count or register type"
				))

			if (hasStrideOrLen) {
				if(dk.data != reg.shaderBuffer.bufferSize)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorLayoutInfo_detect() mismatching stride/buffer length"
					))
			}

			//Validate compatibility of primitive and format

			else if (isTexture && (
				dk.textureFormat.primitive != reg.reg.texture.primitive ||
				dk.textureFormat.formatId != reg.reg.texture.formatId
			))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorLayoutInfo_detect() mismatching texture primitive or format id"
				))

			info->bindings.ptrNonConst[registerNameMatch].visibility |= visibility;
		}
	}

	init = false;

clean:

	if(init) {

		DescriptorLayoutInfo_free(info, Platform_instance->alloc);

		if(pushDescriptorInfo)
			DescriptorLayoutInfo_free(pushDescriptorInfo, Platform_instance->alloc);
	}

	CharString_freex(&tmp);
	return s_uccess;
}

Bool GraphicsDeviceRef_detectLayoutFromEntry(
	GraphicsDeviceRef *dev,
	SHFile binary,
	U32 entrypoint,
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	ListCharString pushDescriptors,
	CharString pushConstantName,
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU32 entrypoints = (ListU32) { 0 };
	gotoIfError2(clean, ListU32_createRefConst(&entrypoint, 1, &entrypoints))
	gotoIfError3(clean, GraphicsDeviceRef_detectLayoutFromEntries(
		dev,
		binary,
		entrypoints,
		flags,
		detectFlags,
		pushDescriptors,
		pushConstantName,
		pushConstantOut,
		info,
		pushDescriptorInfo,
		e_rr
	))

clean:
	return s_uccess;
}

void DescriptorLayoutInfo_free(DescriptorLayoutInfo *info, Allocator alloc) {

	if(!info)
		return;

	ListDescriptorBinding_free(&info->bindings, alloc);
	ListCharString_freeUnderlying(&info->bindingNames, alloc);
}

Bool DescriptorLayout_free(DescriptorLayout *layout, Allocator alloc) {

	//Log_debugLnx("Destroy: %p", layout);

	Bool success = DescriptorLayout_freeExt(layout, alloc);

	if(!(layout->info.flags & EDescriptorLayoutFlags_InternalWeakDeviceRef))
		success &= !GraphicsDeviceRef_dec(&layout->device).genericError;

	ListU16_freex(&layout->bindlessTypeToBinding);
	ListU8_freex(&layout->bindingToBindlessType);
	DescriptorLayoutInfo_free(&layout->info, alloc);
	return success;
}

Error DescriptorBinding_validate(GraphicsDevice *device, DescriptorBinding b, Bool isPushDescriptor) {
	
	ESHRegisterType type = b.registerType & ESHRegisterType_TypeMask;

	if(
		type == ESHRegisterType_AccelerationStructure &&
		!(device->info.capabilities.features & EGraphicsFeatures_Raytracing)
	)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.bindings has an RTAS, but device doesn't have RT"
		);

	Bool isCBV = type == ESHRegisterType_ConstantBuffer || type == ESHRegisterType_PushConstants;

	if(isCBV && (!b.constantBufferSize || b.constantBufferSize > 64 * KIBI))
		return Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() requires strideOrLength to be equal to the constant buffer size "
			"(0 < x < 64KiB)"
		);

	if(
		(type == ESHRegisterType_StructuredBuffer || type == ESHRegisterType_StructuredBufferAtomic) &&
		!b.structedBufferStride
	)
		return Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() requires strideOrLength to be equal to the structured buffer size"
		);

	if(isPushDescriptor)
		if(type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState)
			return Error_invalidOperation(
				0,
				"GraphicsDeviceRef_createDescriptorLayout() can't create a push descriptor for a sampler"
			);

	return Error_none();
}

Error GraphicsDeviceRef_createDescriptorLayout(
	GraphicsDeviceRef *dev,
	DescriptorLayoutInfo *info,
	CharString name,
	DescriptorLayoutRef **layoutRef
) {

	if(!dev || dev->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorLayout()::dev is required");

	if(!info)
		return Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorLayout()::info is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if((info->flags & EDescriptorLayoutFlags_AllowBindlessAny) && !(device->info.capabilities.features & EGraphicsFeatures_Bindless))
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.flags can't include bindless if bindless feature is missing"
		);

	if(info->bindings.length >= U16_MAX)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.bindings.length is limited to U16_MAX"
		);

	Bool isPushDescriptor = info->flags & EDescriptorLayoutFlags_HasPushDescriptors;

	if((info->flags & EDescriptorLayoutFlags_AllowBindlessAny) && isPushDescriptor)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout() bindless is not allowed with push descriptors"
		);

	U64 bindlessTypes = 0;
	U64 bindingCount = 0;

	Bool anySampler = false, anyResource = false;

	for(U64 i = 0; i < info->bindings.length; ++i) {

		DescriptorBinding b = info->bindings.ptr[i];
		bindingCount += b.count;

		if(b.registerType == ESHRegisterType_PushConstants)
			return Error_invalidOperation(
				0,
				"GraphicsDeviceRef_createDescriptorLayout() can't contain push constants as a descriptor layout, "
				"provide them using a pipeline layout instead"
			);

		if(b.registerType == ESHRegisterType_Sampler || b.registerType == ESHRegisterType_SamplerComparisonState)
			anySampler = true;

		else anyResource = true;
		
		Error err = Error_none();
		if((err = DescriptorBinding_validate(device, b, isPushDescriptor)).genericError)
			return err;

		if ((info->flags & EDescriptorLayoutFlags_AllowBindlessAny) && b.count > 1)
			++bindlessTypes;
	}

	if(bindlessTypes > 15)
		return Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() more than 15 bindless arrays are present, this is unsupported "
			"due to BindlessDescriptor's limits"
		);

	if(bindingCount > 32 && isPushDescriptor)
		return Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() push descriptors are limited to 32"
		);

	Error err = RefPtr_createx(
		(U32)(sizeof(DescriptorLayout) + GraphicsDeviceRef_getObjectSizes(dev)->descriptorLayout),
		(ObjectFreeFunc) DescriptorLayout_free,
		(ETypeId) EGraphicsTypeId_DescriptorLayout,
		layoutRef
	);

	if(err.genericError)
		return err;

	if(!(info->flags & EDescriptorLayoutFlags_InternalWeakDeviceRef))
		gotoIfError(clean, GraphicsDeviceRef_inc(dev))

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

	if(!ListCharString_movex(&info->bindingNames, &layout->info.bindingNames, &err))
		goto clean;

	if(ListDescriptorBinding_isRef(info->bindings))
		gotoIfError(clean, ListDescriptorBinding_createCopyx(info->bindings, &layout->info.bindings))

	else layout->info.bindings = info->bindings;

	*info = (DescriptorLayoutInfo) { 0 };

	if (bindlessTypes) {

		gotoIfError(clean, ListU16_reservex(&layout->bindlessTypeToBinding, bindlessTypes))
		gotoIfError(clean, ListU8_resizex(&layout->bindingToBindlessType, layout->info.bindings.length))
		
		bindlessTypes = 0;

		for(U16 i = 0; i < (U16) layout->info.bindings.length; ++i) {

			DescriptorBinding b = layout->info.bindings.ptr[i];

			U8 bindlessType = U8_MAX;

			if ((layout->info.flags & EDescriptorLayoutFlags_AllowBindlessAny) && b.count > 1) {
				bindlessType = (U8) bindlessTypes;
				gotoIfError(clean, ListU16_pushBackx(&layout->bindlessTypeToBinding, i))
				++bindlessTypes;
			}

			layout->bindingToBindlessType.ptrNonConst[i] = bindlessType;
		}
	}

	gotoIfError(clean, GraphicsDeviceRef_createDescriptorLayoutExt(dev, layout, name))

clean:

	if(err.genericError)
		DescriptorLayoutRef_dec(layoutRef);

	return err;
}
