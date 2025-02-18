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
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "types/container/string.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "formats/oiSH/entries.h"

Bool VK_WRAP_FUNC(PipelineLayout_free)(PipelineLayout *layout) {

	VkGraphicsDevice *device = GraphicsDevice_ext(GraphicsDeviceRef_ptr(layout->device), Vk);
	VkPipelineLayout *layoutExt = PipelineLayout_ext(layout, Vk);

	if(*layoutExt)
		device->destroyPipelineLayout(device->device, *layoutExt, NULL);

	return true;
}

Error VK_WRAP_FUNC(GraphicsDeviceRef_createPipelineLayout)(
	GraphicsDeviceRef *dev,
	PipelineLayout *layout,
	CharString name
) {

	Error err = Error_none();
	CharString tmpName = CharString_createNull();
	VkPipelineLayout *layoutExt = PipelineLayout_ext(layout, Vk);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	DescriptorLayout *desc = DescriptorLayoutRef_ptr(layout->info.bindings);
	VkDescriptorLayout *descExt = DescriptorLayout_ext(desc, Vk);

	U32 count = 0;

	for(; count < 4 && descExt->layouts[count]; ++count)
		;

	VkPipelineLayoutCreateInfo create = (VkPipelineLayoutCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = count,
		.pSetLayouts = descExt->layouts
	};

	gotoIfError(clean, checkVkError(deviceExt->createPipelineLayout(deviceExt->device, &create, NULL, layoutExt)))
	
	if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name) && instanceExt->debugSetName) {

		if(!CharString_isNullTerminated(name))
			gotoIfError(clean, CharString_createCopyx(name, &tmpName))

		const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT,
			.pObjectName = tmpName.ptr ? tmpName.ptr : name.ptr,
			.objectHandle = (U64) *layoutExt
		};

		gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
	}

clean:
	CharString_freex(&tmpName);
	return err;
}
