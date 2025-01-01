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
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/texture.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/container/texture_format.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/error.h"

Error createShaderModule(
	Buffer buf,
	VkShaderModule *mod,
	VkGraphicsDevice *device,
	VkGraphicsInstance *instance,
	CharString name,
	EPipelineStage stage
);

TList(VkComputePipelineCreateInfo);
TListImpl(VkComputePipelineCreateInfo);

Bool VK_WRAP_FUNC(GraphicsDevice_createPipelineCompute)(
	GraphicsDevice *device,
	CharString name,
	Pipeline *pipeline,
	SHBinaryInfo buf,
	Error *e_rr
) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	Bool s_uccess = true;
	VkPipeline pipelineHandle = NULL;
	CharString temp = CharString_createNull();

	//TODO: Push constants

	VkComputePipelineCreateInfo pipelineInfo = (VkComputePipelineCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.pName = "main"
		},
		.layout = deviceExt->defaultLayout
	};

	gotoIfError2(clean, createShaderModule(
		buf.binaries[ESHBinaryType_SPIRV],
		&pipelineInfo.stage.module,
		deviceExt,
		instanceExt,
		name,
		EPipelineStage_Compute
	))

	gotoIfError2(clean, vkCheck(vkCreateComputePipelines(
		deviceExt->device,
		NULL,
		1, &pipelineInfo,
		NULL,
		&pipelineHandle
	)))

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName && CharString_length(name)) {

		if(!CharString_isNullTerminated(name))
			gotoIfError2(clean, CharString_createCopyx(name, &temp))

		VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_PIPELINE,
			.objectHandle = (U64) pipelineHandle,
			.pObjectName = temp.ptr ? temp.ptr : name.ptr
		};

		gotoIfError2(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)))
		CharString_freex(&temp);
	}

	*Pipeline_ext(pipeline, Vk) = pipelineHandle;
	pipelineHandle = NULL;

clean:

	if(pipelineHandle)
		vkDestroyPipeline(deviceExt->device, pipelineHandle, NULL);

	const VkShaderModule mod = pipelineInfo.stage.module;

	if(mod)
		vkDestroyShaderModule(deviceExt->device, mod, NULL);

	CharString_freex(&temp);
	return s_uccess;
}
