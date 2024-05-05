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
#include "graphics/generic/instance.h"
#include "graphics/generic/texture.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "formats/texture.h"
#include "types/buffer.h"
#include "types/string.h"
#include "types/error.h"

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

Error GraphicsDevice_createPipelinesComputeExt(GraphicsDevice *device, ListCharString names, ListPipelineRef *pipelines) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	ListVkComputePipelineCreateInfo pipelineInfos = (ListVkComputePipelineCreateInfo) { 0 };
	ListVkPipeline pipelineHandles = (ListVkPipeline) { 0 };

	Error err;
	gotoIfError(clean, ListVkComputePipelineCreateInfo_resizex(&pipelineInfos, pipelines->length))
	gotoIfError(clean, ListVkPipeline_resizex(&pipelineHandles, pipelines->length))

	//TODO: Push constants

	for(U64 i = 0; i < pipelines->length; ++i) {

		pipelineInfos.ptrNonConst[i] = (VkComputePipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = (VkPipelineShaderStageCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
				.pName = "main"
			},
			.layout = deviceExt->defaultLayout
		};

		const Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[i]);

		gotoIfError(clean, createShaderModule(
			pipeline->stages.ptr[0].binary,
			&pipelineInfos.ptrNonConst[i].stage.module,
			deviceExt,
			instanceExt,
			!names.length ? CharString_createNull() : names.ptr[i],
			EPipelineStage_Compute
		))
	}

	gotoIfError(clean, vkCheck(vkCreateComputePipelines(
		deviceExt->device,
		NULL,
		(U32) pipelineInfos.length,
		pipelineInfos.ptr,
		NULL,
		pipelineHandles.ptrNonConst
	)))

	for (U64 i = 0; i < pipelines->length; ++i) {

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName && names.length) {

			VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_PIPELINE,
				.objectHandle = (U64) pipelineHandles.ptr[i],
				.pObjectName = names.ptr[i].ptr
			};

			gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)))
		}

		Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[i]);
		*Pipeline_ext(pipeline, Vk) = pipelineHandles.ptr[i];
		pipelineHandles.ptrNonConst[i] = NULL;
	}

	ListVkPipeline_freex(&pipelineHandles);

clean:

	for(U64 i = 0; i < pipelineHandles.length; ++i)
		if(pipelineHandles.ptr[i])
			vkDestroyPipeline(deviceExt->device, pipelineHandles.ptr[i], NULL);

	ListVkPipeline_freex(&pipelineHandles);

	for (U64 i = 0; i < pipelineInfos.length; ++i) {

		const VkShaderModule mod = pipelineInfos.ptr[i].stage.module;

		if(mod)
			vkDestroyShaderModule(deviceExt->device, mod, NULL);
	}

	ListVkComputePipelineCreateInfo_freex(&pipelineInfos);
	return err;
}
