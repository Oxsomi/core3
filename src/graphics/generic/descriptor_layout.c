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

	if(regType == ESHRegisterType_ConstantBuffer)
		return 2;

	if(regType == ESHRegisterType_Sampler || regType == ESHRegisterType_SamplerComparisonState)
		return 3;

	return type & ESHRegisterType_IsWrite;
}

Bool GraphicsDeviceRef_detectLayoutFromEntries(
	GraphicsDeviceRef *dev,
	SHFile binary,
	ListU32 entrypoints,
	EDescriptorLayoutFlags flags,
	DescriptorLayoutInfo *info,
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

		for(U64 j = 0; j < bin.registers.length; ++j) {

			SHRegisterRuntime reg = bin.registers.ptr[j];
			U64 registerNameMatch = U64_MAX;
			U64 registerBindingsMatch = U64_MAX;
			SHBinding regMatch = reg.reg.bindings.arr[binaryType];

			if(regMatch.binding == U32_MAX && regMatch.space == U32_MAX)	//Doesn't exist in current binary type
				continue;

			U8 regType = getDxilRegisterType(reg.reg.registerType);

			//Find matching register by name or binding

			for(U64 k = 0; k < info->bindingNames.length; ++k) {

				if (CharString_equalsStringSensitive(info->bindingNames.ptr[k], reg.name))
					registerNameMatch = k;

				DescriptorBinding dk = info->bindings.ptr[k];
				SHBinding bindk = dk.binding;

				switch (binaryType) {

					//SPIRV; register intersection only happens if they're identical

					case ESHBinaryType_SPIRV:
						
						if(bindk.space == regMatch.space && bindk.binding == regMatch.binding)
							registerBindingsMatch = k;

						break;

					//DXIL; register intersection happens when the range overlaps

					case ESHBinaryType_DXIL:
						
						if (bindk.space == regMatch.space && regType == getDxilRegisterType(dk.registerType)) {

							if(bindk.binding == regMatch.binding)
								registerBindingsMatch = k;

							else if(bindk.binding + dk.count > regMatch.binding && bindk.binding <= regMatch.binding)
								retError(clean, Error_invalidParameter(
									3, 0, "DescriptorLayoutInfo_detect() dxil register range conflicts"
								))
						}

						break;
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

			//Grab count

			U32 count = 1;

			for(U64 k = 0; k < reg.arrays.length; ++k)
				count *= reg.arrays.ptr[k];

			//Unique register, create another

			ESHRegisterType regType4 = reg.reg.registerType & ESHRegisterType_TypeMask;

			Bool hasStrideOrLen = 
				regType4 == ESHRegisterType_ConstantBuffer ||
				regType4 == ESHRegisterType_StructuredBuffer ||
				regType4 == ESHRegisterType_StructuredBufferAtomic;

			Bool isTexture = regType4 >= ESHRegisterType_TextureStart && regType4 <= ESHRegisterType_TextureEnd;

			if (registerNameMatch == U64_MAX) {

				gotoIfError2(clean, CharString_createCopyx(reg.name, &tmp))
				gotoIfError2(clean, ListCharString_pushBackx(&info->bindingNames, tmp))
				tmp = CharString_createNull();

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

	if(init)
		DescriptorLayoutInfo_free(info, Platform_instance->alloc);

	CharString_freex(&tmp);
	return s_uccess;
}

Bool GraphicsDeviceRef_detectLayoutFromEntry(
	GraphicsDeviceRef *dev,
	SHFile binary,
	U32 entrypoint,
	EDescriptorLayoutFlags flags,
	DescriptorLayoutInfo *info,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU32 entrypoints = (ListU32) { 0 };
	gotoIfError2(clean, ListU32_createRefConst(&entrypoint, 1, &entrypoints))
	gotoIfError3(clean, GraphicsDeviceRef_detectLayoutFromEntries(dev, binary, entrypoints, flags, info, e_rr))

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

	(void)alloc;

	//Log_debugLnx("Destroy: %p", layout);

	Bool success = DescriptorLayout_freeExt(layout, alloc);

	if(!(layout->info.flags & EDescriptorLayoutFlags_InternalWeakDeviceRef))
		success &= !GraphicsDeviceRef_dec(&layout->device).genericError;

	ListU16_freex(&layout->bindlessTypeToBinding);
	ListU8_freex(&layout->bindingToBindlessType);
	DescriptorLayoutInfo_free(&layout->info, alloc);
	return success;
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

	U64 bindlessTypes = 0;

	for(U64 i = 0; i < info->bindings.length; ++i) {

		DescriptorBinding b = info->bindings.ptr[i];
		ESHRegisterType type = b.registerType & ESHRegisterType_TypeMask;

		if(
			type == ESHRegisterType_AccelerationStructure &&
			!(device->info.capabilities.features & EGraphicsFeatures_Raytracing)
		)
			return Error_invalidOperation(
				0, "GraphicsDeviceRef_createDescriptorLayout()::info.bindings has an RTAS, but device doesn't have RT"
			);

		if(type == ESHRegisterType_ConstantBuffer && (!b.constantBufferSize || b.constantBufferSize > 64 * KIBI))
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

		if ((info->flags & EDescriptorLayoutFlags_AllowBindlessAny) && b.count > 1)
			++bindlessTypes;
	}

	if(bindlessTypes > 15)
		return Error_invalidOperation(
			0,
			"GraphicsDeviceRef_createDescriptorLayout() more than 15 bindless arrays are present, this is unsupported "
			"due to BindlessDescriptor's limits"
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

	*layout = (DescriptorLayout) { .device = dev, .info = *info };

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
