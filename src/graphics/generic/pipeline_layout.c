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
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "platforms/ext/ref_ptrx.h"
#include "platforms/log.h"
#include "types/container/string.h"

Error PipelineLayoutRef_dec(PipelineLayoutRef **layout) {
	return !RefPtr_dec(layout) ?
		Error_invalidOperation(0, "PipelineLayoutRef_dec()::layout is required") : Error_none();
}

Error PipelineLayoutRef_inc(PipelineLayoutRef *layout) {
	return !RefPtr_inc(layout) ?
		Error_invalidOperation(0, "PipelineLayoutRef_inc()::layout is required") : Error_none();
}

Bool PipelineLayout_free(PipelineLayout *layout, Allocator alloc) {

	(void)alloc;

	//Log_debugLnx("Destroy: %p", layout);

	Bool success = PipelineLayout_freeExt(layout, alloc);

	if(!(layout->info.flags & EPipelineLayoutFlags_InternalWeakDeviceRef)) {
		success &= !GraphicsDeviceRef_dec(&layout->device).genericError;
		DescriptorLayoutRef_dec(&layout->info.bindings);
		DescriptorLayoutRef_dec(&layout->info.pushDescriptors);
	}

	return success;
}

Error GraphicsDeviceRef_createPipelineLayout(
	GraphicsDeviceRef *dev,
	PipelineLayoutInfo info,
	CharString name,
	PipelineLayoutRef **layoutRef
) {

	if(!dev || dev->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createPipelineLayout()::dev is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if(info.bindings && info.bindings->typeId != (ETypeId) EGraphicsTypeId_DescriptorLayout)
		return Error_nullPointer(
			1, "GraphicsDeviceRef_createPipelineLayout()::info.bindings must be DescriptorLayout if present"
		);

	if(info.pushDescriptors && info.pushDescriptors->typeId != (ETypeId) EGraphicsTypeId_DescriptorLayout)
		return Error_nullPointer(
			1, "GraphicsDeviceRef_createPipelineLayout()::info.pushDescriptors must be DescriptorLayout if present"
		);

	//Validate push constants

	if(info.pushConstants.count && (
		!info.pushConstants.constantBufferSize ||
		info.pushConstants.constantBufferSize > 128 ||
		(info.pushConstants.constantBufferSize & 3)
	))
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.pushConstants must be 4-128 bytes (multiple of 4)"
		);

	if(info.pushConstants.count && !info.pushConstants.visibility)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.pushConstants must have at least one visibility"
		);

	Bool isVulkan = GraphicsInstanceRef_ptr(device->instance)->api == EGraphicsApi_Vulkan;
	ESHBinaryType binaryType = isVulkan ? ESHBinaryType_SPIRV : ESHBinaryType_DXIL;

	Bool canBePushConstant =
		(binaryType == ESHBinaryType_DXIL && info.pushConstants.registerType == ESHRegisterType_ConstantBuffer) ||
		info.pushConstants.registerType == ESHRegisterType_PushConstants;

	if(info.pushConstants.count && !canBePushConstant)
		return Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineLayout()::info.pushConstants must be a constant buffer (DXIL) or "
			"push constants (SPV)"
		);

	if(info.pushConstants.count > 1)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.pushConstants.count must be 1 or 0"
		);

	//Validate DWORD root signature limit (DirectX).
	//At the same time; ensure the descriptor sets don't exceed 4 active sets if SPIRV is used.
	//Added here to ensure all backends behave relatively similar.

	U32 dwords = 0;

	U32 uniqueSets[4];
	U8 setCounter = 0;

	if(info.pushConstants.count)	//Push constants take 1 DWORD each
		dwords += info.pushConstants.constantBufferSize >> 2;

	if (info.pushDescriptors) {		//Push descriptors take 2 DWORDs each

		DescriptorLayout *layout = DescriptorLayoutRef_ptr(info.pushDescriptors);

		if(!(layout->info.flags & EDescriptorLayoutFlags_HasPushDescriptors))
			return Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.pushDescriptors doesn't have the right flags"
			);

		dwords += (U32)(layout->info.bindings.length << 1);

		//Check overlap with push constant and push descriptors

		for (U64 i = 0; i < layout->info.bindings.length; ++i) {

			DescriptorBinding bind = layout->info.bindings.ptr[i];

			if(info.pushConstants.count && DescriptorBinding_overlaps(
				info.pushConstants,
				bind.registerType,
				bind.binding,
				bind.count,
				binaryType,
				false
			))
				return Error_invalidParameter(
					1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.pushConstants overlaps with push descriptors"
				);

			if(isVulkan) {		//Grab sets of binding, DescriptorLayout already protects these to be <=4

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

	if (info.bindings) {			//Descriptor tables take 1 DWORD each

		DescriptorLayout *layout = DescriptorLayoutRef_ptr(info.bindings);

		if(layout->info.flags & EDescriptorLayoutFlags_HasPushDescriptors)
			return Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.bindings doesn't have the right flags"
			);

		dwords += (U32)layout->anySampler + layout->anyResource;

		//Check overlap with bindings, push constants and push descriptors

		for (U64 i = 0; i < layout->info.bindings.length; ++i) {

			DescriptorBinding bind = layout->info.bindings.ptr[i];

			if(isVulkan) {		//If we run out of sets or have an overlapping, we have a problem

				U8 j = 0;

				for(; j < setCounterPushDesc; ++j)
					if(uniqueSets[j] == bind.binding.space)
						return Error_invalidParameter(
							1, 0,
							"GraphicsDeviceRef_createPipelineLayout() push descriptors and descriptors can't share same sets"
						);

				for(; j < setCounter; ++j)
					if(uniqueSets[j] == bind.binding.space)
						break;

				if(j == setCounter) {

					if(setCounter == 4)
						return Error_invalidParameter(
							1, 0,
							"GraphicsDeviceRef_createPipelineLayout() push descriptor + descriptors set count exceeds 4"
						);

					uniqueSets[setCounter++] = bind.binding.space;
				}
			}

			Bool match = false;

			if(info.pushDescriptors) {

				DescriptorLayout *pushLayout = DescriptorLayoutRef_ptr(info.pushDescriptors);

				for (U64 j = 0; j < pushLayout->info.bindings.length; ++j)
					if (DescriptorBinding_overlaps(
						pushLayout->info.bindings.ptr[j],
						bind.registerType,
						bind.binding,
						bind.count,
						binaryType,
						false
					)) {
						match = true;
						break;
					}
			}

			if(!match && info.pushConstants.count && DescriptorBinding_overlaps(
				info.pushConstants,
				bind.registerType,
				bind.binding,
				bind.count,
				binaryType,
				true
			))
				match = true;

			if(match)
				return Error_invalidParameter(
					1, 0, "GraphicsDeviceRef_createPipelineLayout()::info.bindings has an overlapping register"
				);
		}
	}

	if(dwords > 64)
		return Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineLayout() "
			"root signature shouldn't exceed 64 DWORDs in case a DirectX backend is used, "
			"push constants bytes are 4:1, push descriptors 1:2 and "
			"bindings +1 if non static samplers are used, +1 if other resources are used"
		);

	if(dwords > 13)
		Log_performanceLnx(
			"GraphicsDeviceRef_createPipelineLayout() root signature would exceed 13 DWORDs if a DirectX backend is used"
		);

	//Create object

	Error err = RefPtr_createx(
		(U32)(sizeof(PipelineLayout) + GraphicsDeviceRef_getObjectSizes(dev)->pipelineLayout),
		(ObjectFreeFunc) PipelineLayout_free,
		(ETypeId) EGraphicsTypeId_PipelineLayout,
		layoutRef
	);

	if(err.genericError)
		return err;

	if(!(info.flags & EPipelineLayoutFlags_InternalWeakDeviceRef))
		gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	PipelineLayout *layout = PipelineLayoutRef_ptr(*layoutRef);

	//Log_debugLnx("Create: PipelineLayout %.*s (%p)", (int) CharString_length(name), name.ptr, layout);

	*layout = (PipelineLayout) { .device = dev, .info = info };

	if(!(info.flags & EPipelineLayoutFlags_InternalWeakDeviceRef))
		gotoIfError(clean, DescriptorLayoutRef_inc(layout->info.bindings))

	gotoIfError(clean, GraphicsDeviceRef_createPipelineLayoutExt(dev, layout, name))

clean:

	if(err.genericError)
		PipelineLayoutRef_dec(layoutRef);

	return err;
}
