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

//graphics/generic/pipeline_layout.c

#include "graphics/generic/interface.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/ref_ptr.h"
#include "platforms/logx.h"

void PipelineLayout_free(void *layoutGeneric, const Allocator *alloc) {

	PipelineLayout *layout = (PipelineLayout*) layoutGeneric;

	(void)alloc;

	PipelineLayout_freeExt(layout, alloc);

	if(!(layout->info.flags & EPipelineLayoutFlags_InternalWeakDeviceRef)) {
		RefPtr_dec(&layout->device);
		RefPtr_dec(&layout->info.bindings);
		RefPtr_dec(&layout->info.pushDescriptors);
	}
}

//OxC3's per frame globals are a constant buffer the RUNTIME fills. A pipeline layout that declares it gets
//them bound for it, rather than only the device's own default layout doing so: a custom layout is exactly
//what a shader needs the moment it wants push constants, and losing _frameId/_time in the process would make
//the two mutually exclusive.
//Identity rather than shape, because the shape differs per backend: the globals sit at b0 in the reserved
//space on DXIL but in their own SET 2 on Vulkan (see the binding in device.c), and set 2 is not reserved
//there, so a caller could legitimately own a binding that looks identical.
//Identity is exact and needs no per backend rule; the device's layout is the only one this can ever be.

Bool PipelineLayout_hasRuntimeGlobals(const PipelineLayout *layout) {

	if(!layout || !layout->info.pushDescriptors || !layout->device)
		return false;

	return layout->info.pushDescriptors == GraphicsDeviceRef_ptr(layout->device)->defaultCBufferLayout;
}

//The same idea for the bindless set: a layout whose bindings ARE the device's own layout is served by the
//device's own heap and table, so the caller neither has to bind one nor could, since createDescriptorLayout
//refuses caller bindings in the reserved space the whole bindless set lives in.

Bool PipelineLayout_usesRuntimeBindless(const PipelineLayout *layout) {

	if(!layout || !layout->info.bindings || !layout->device)
		return false;

	return layout->info.bindings == GraphicsDeviceRef_ptr(layout->device)->defaultDescLayout;
}

Bool GraphicsDeviceRef_createPipelineLayout(
	GraphicsDeviceRef *dev,
	const PipelineLayoutInfo *info,
	const CharString *name,
	PipelineLayoutRef **layoutRef,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createPipelineLayout()::dev is required"));

	if(!info)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_createPipelineLayout()::info is required"));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if(info->bindings && info->bindings->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorLayout)
		retError(clean, Error_nullPointer(
			1, "GraphicsDeviceRef_createPipelineLayout()::info->bindings must be DescriptorLayout if present"
		));

	if(info->pushDescriptors && info->pushDescriptors->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorLayout)
		retError(clean, Error_nullPointer(
			1, "GraphicsDeviceRef_createPipelineLayout()::info->pushDescriptors must be DescriptorLayout if present"
		));

	//Validate push constants

	if(info->pushConstants.count && (
		!info->pushConstants.constantBufferSize ||
		info->pushConstants.constantBufferSize > 128 ||
		(info->pushConstants.constantBufferSize & 3)
	))
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineLayout()::info->pushConstants must be 4-128 bytes (multiple of 4)"
		));

	if(info->pushConstants.count && !info->pushConstants.visibility)
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineLayout()::info->pushConstants must have at least one visibility"
		));

	Bool isVulkan = GraphicsInstanceRef_ptr(device->instance)->api == EGraphicsApi_Vulkan;
	EGfxBinaryType binaryType = isVulkan ? EGfxBinaryType_SPIRV : EGfxBinaryType_DXIL;

	Bool canBePushConstant =
		(binaryType == EGfxBinaryType_DXIL && info->pushConstants.registerType == EGfxRegisterType_ConstantBuffer) ||
		info->pushConstants.registerType == EGfxRegisterType_PushConstants;

	if(info->pushConstants.count && !canBePushConstant)
		retError(clean, Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineLayout()::info->pushConstants must be a constant buffer (DXIL) or "
			"push constants (SPV)"
		));

	if(info->pushConstants.count > 1)
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineLayout()::info->pushConstants.count must be 1 or 0"
		));

	//Validate DWORD root signature limit (DirectX).
	//At the same time; ensure the descriptor sets don't exceed 4 active sets if SPIRV is used.
	//Added here to ensure all backends behave relatively similar.

	U32 dwords = 0;

	U32 uniqueSets[4];
	U8 setCounter = 0;

	if(info->pushConstants.count)     //Push constants take 1 DWORD each
		dwords += info->pushConstants.constantBufferSize >> 2;

	if (info->pushDescriptors) {      //Push descriptors take 2 DWORDs each

		DescriptorLayout *layout = DescriptorLayoutRef_ptr(info->pushDescriptors);

		if(!(layout->info.flags & EDescriptorLayoutFlags_HasPushDescriptors))
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.pushDescriptors doesn't have the right flags"
			));

		dwords += (U32)(layout->info.bindings.length << 1);

		//Check overlap with push constant and push descriptors

		for (U64 i = 0; i < layout->info.bindings.length; ++i) {

			DescriptorBinding bind = layout->info.bindings.ptr[i];

			if(info->pushConstants.count && DescriptorBinding_overlaps(
				&info->pushConstants,
				bind.registerType,
				&bind.binding,
				bind.count,
				binaryType,
				true
			))
				retError(clean, Error_invalidParameter(
					1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.pushConstants overlaps with push descriptors"
				));

			if(isVulkan) {        //Grab sets of binding, DescriptorLayout already protects these to be <=4

				U8 j = 0;

				for(; j < setCounter; ++j)
					if(uniqueSets[j] == bind.binding.space)
						break;

				if(j == setCounter)
					uniqueSets[setCounter++] = bind.binding.space;
			}
		}
	}

	U8 setCounterPushDesc = setCounter;

	if (info->bindings) {          //Descriptor tables take 1 DWORD each

		DescriptorLayout *layout = DescriptorLayoutRef_ptr(info->bindings);

		if(layout->info.flags & EDescriptorLayoutFlags_HasPushDescriptors)
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.bindings doesn't have the right flags"
			));

		dwords += (U32)layout->anySampler + layout->anyResource;

		//Check overlap with bindings, push constants and push descriptors

		for (U64 i = 0; i < layout->info.bindings.length; ++i) {

			DescriptorBinding bind = layout->info.bindings.ptr[i];

			if(isVulkan) {        //If we run out of sets or have an overlapping, we have a problem

				U8 j = 0;

				for(; j < setCounterPushDesc; ++j)
					if(uniqueSets[j] == bind.binding.space)
						retError(clean, Error_invalidParameter(
							1, 0,
							"GraphicsDeviceRef_createPipelineLayout() push descriptors and descriptors can't share same sets"
						));

				for(; j < setCounter; ++j)
					if(uniqueSets[j] == bind.binding.space)
						break;

				if(j == setCounter) {

					if(setCounter == 4)
						retError(clean, Error_invalidParameter(
							1, 0,
							"GraphicsDeviceRef_createPipelineLayout() push descriptor + descriptors set count exceeds 4"
						));

					uniqueSets[setCounter++] = bind.binding.space;
				}
			}

			Bool match = false;

			if(info->pushDescriptors) {

				DescriptorLayout *pushLayout = DescriptorLayoutRef_ptr(info->pushDescriptors);

				for (U64 j = 0; j < pushLayout->info.bindings.length; ++j)
					if (DescriptorBinding_overlaps(
						&pushLayout->info.bindings.ptr[j],
						bind.registerType,
						&bind.binding,
						bind.count,
						binaryType,
						false
					)) {
						match = true;
						break;
					}
			}

			if(!match && info->pushConstants.count && DescriptorBinding_overlaps(
				&info->pushConstants,
				bind.registerType,
				&bind.binding,
				bind.count,
				binaryType,
				true
			))
				match = true;

			if(match)
				retError(clean, Error_invalidParameter(
					1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.bindings has an overlapping register"
				));
		}
	}

	if(dwords > 64)
		retError(clean, Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineLayout() "
			"root signature shouldn't exceed 64 DWORDs in case a DirectX backend is used, "
			"push constants bytes are 4:1, push descriptors 1:2 and "
			"bindings +1 if non static samplers are used, +1 if other resources are used"
		));

	if(dwords > 13 && GraphicsDevice_logOnce(device, EGraphicsDeviceMessage_RootSignature13Dwords))
		Log_performanceLnx(
			"GraphicsDeviceRef_createPipelineLayout() root signature would exceed 13 DWORDs if a DirectX backend "
			"is used (only logged once)"
		);

	//Create object

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->pipelineLayout, layoutRef, e_rr));

	PipelineLayout *layout = PipelineLayoutRef_ptr(*layoutRef);

	*layout = (PipelineLayout) { .device = dev, .info = *info };

	//The free decs device, bindings and pushDescriptors unconditionally for a strong layout, so all three have to
	// be inc'd together or the optional ones get over released.
	//Both layouts are optional, and RefPtr_inc reports a NULL as failure without setting an error, so a push
	// constant only layout used to fail here silently rather than be created.
	//None of these can fail (dev was validated and NULL is skipped), so a partial inc can't leak into the cleanup.

	if(!(info->flags & EPipelineLayoutFlags_InternalWeakDeviceRef)) {

		RefPtr_inc(dev);

		if(layout->info.bindings)
			RefPtr_inc(layout->info.bindings);

		if(layout->info.pushDescriptors)
			RefPtr_inc(layout->info.pushDescriptors);
	}

	gotoIfError3(clean, GraphicsDeviceRef_createPipelineLayoutExt(dev, layout, name, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(layoutRef);

	return s_uccess;
}
