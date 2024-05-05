/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/texture.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/string.h"
#include "types/error.h"

Error createShaderModule(
	Buffer buf,
	VkShaderModule *mod,
	VkGraphicsDevice *device,
	VkGraphicsInstance *instance,
	CharString name,
	EPipelineStage stage
) {

	(void)instance;
	(void)stage;

	if(Buffer_length(buf) >> 32)
		return Error_outOfBounds(
			0, Buffer_length(buf), U32_MAX, "createShaderModule()::buf.length is limited to U32_MAX"
		);

	if(!Buffer_length(buf) || Buffer_length(buf) % sizeof(U32))
		return Error_invalidParameter(
			0, 0, "createShaderModule()::buf.length must be in U32s when SPIR-V is used"
		);

	VkShaderModuleCreateInfo info = (VkShaderModuleCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (U32) Buffer_length(buf),
		.pCode = (const U32*) buf.ptr
	};

	Error err = vkCheck(vkCreateShaderModule(device->device, &info, NULL, mod));
	CharString temp = CharString_createNull();

	if(err.genericError)
		return err;

	const GraphicsDevice *baseDevice = (const GraphicsDevice*)device - 1;

	if((baseDevice->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name) && instance->debugSetName) {

		const Bool isRt = stage >= EPipelineStage_RtStart && stage <= EPipelineStage_RtEnd;

		gotoIfError(clean, CharString_formatx(
			&temp, "Shader module (\"%.*s\": %s)",
			CharString_length(name), name.ptr, isRt ? "Raytracing" : EPipelineStage_names[stage]
		))

		const VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_SHADER_MODULE,
			.objectHandle = (U64) *mod,
			.pObjectName = temp.ptr
		};

		gotoIfError(clean, vkCheck(instance->debugSetName(device->device, &debugName2)))
	}

	goto clean;

clean:

	CharString_freex(&temp);

	if (err.genericError)
		vkDestroyShaderModule(device->device, *mod, NULL);

	return err;
}

const U64 PipelineExt_size = sizeof(VkPipeline);

Bool Pipeline_freeExt(Pipeline *pipeline, Allocator allocator) {

	(void)allocator;

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	vkDestroyPipeline(deviceExt->device, *Pipeline_ext(pipeline, Vk), NULL);
	return true;
}
