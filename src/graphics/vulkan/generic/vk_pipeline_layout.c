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

//graphics/vulkan/generic/vk_pipeline_layout.c

#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"

void VK_WRAP_FUNC(PipelineLayout_free)(PipelineLayout *layout, const Allocator *alloc) {

	(void) alloc;

	VkGraphicsDevice *device = GraphicsDevice_ext(GraphicsDeviceRef_ptr(layout->device), Vk);
	VkPipelineLayout *layoutExt = PipelineLayout_ext(layout, Vk);

	if(*layoutExt)
		device->destroyPipelineLayout(device->device, *layoutExt, NULL);
}

Bool VK_WRAP_FUNC(GraphicsDeviceRef_createPipelineLayout)(
	GraphicsDeviceRef *dev,
	PipelineLayout *layout,
	const CharString *name,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	CharString tmpName = CharString_createNull();
	VkPipelineLayout *layoutExt = PipelineLayout_ext(layout, Vk);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkDescriptorSetLayout layouts[4];
	U32 count = 0;

	if(layout->info.bindings) {

		DescriptorLayout *desc = DescriptorLayoutRef_ptr(layout->info.bindings);
		VkDescriptorLayout *descExt = DescriptorLayout_ext(desc, Vk);

		for(; count < 4 && descExt->layouts[count]; ++count)
			layouts[count] = descExt->layouts[count];
	}

	if(layout->info.pushDescriptors) {

		DescriptorLayout *desc = DescriptorLayoutRef_ptr(layout->info.pushDescriptors);
		VkDescriptorLayout *descExt = DescriptorLayout_ext(desc, Vk);

		U32 startCount = count;

		for(; count < 4 && descExt->layouts[count - startCount]; ++count)
			layouts[count] = descExt->layouts[count - startCount];
	}

	VkPipelineLayoutCreateInfo create = (VkPipelineLayoutCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = count,
		.pSetLayouts = count ? layouts : NULL
	};

	VkPushConstantRange pushConstants = (VkPushConstantRange) { 0 };

	if (layout->info.pushConstants.count) {

		pushConstants = (VkPushConstantRange) {
			.stageFlags = vkGetShaderStages(layout->info.pushConstants.visibility),
			.size = layout->info.pushConstants.constantBufferSize
		};

		create.pushConstantRangeCount = 1;
		create.pPushConstantRanges = &pushConstants;
	}

	gotoIfError3(clean, checkVkError(deviceExt->createPipelineLayout(deviceExt->device, &create, NULL, layoutExt), e_rr));

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name) && instanceExt->debugSetName) {

		if(!CharString_isNullTerminated(*name))
			gotoIfError3(clean, CharString_createCopy(*name, alloc, &tmpName, e_rr));

		const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT,
			.pObjectName = tmpName.ptr ? tmpName.ptr : name->ptr,
			.objectHandle = (U64) *layoutExt
		};

		gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
	}

clean:
	CharString_free(&tmpName, alloc);
	return s_uccess;
}
