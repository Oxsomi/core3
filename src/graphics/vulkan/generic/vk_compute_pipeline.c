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

//graphics/vulkan/generic/vk_compute_pipeline.c

#include "types/container/list_impl.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "formats/oiSH/sh_file.h"
#include "types/base/error.h"

Bool createShaderModule(
	Buffer buf,
	VkShaderModule *mod,
	VkGraphicsDevice *device,
	VkGraphicsInstance *instance,
	const CharString *name,
	EPipelineStage stage,
	const Allocator *alloc,
	Error *e_rr
);

TList(VkComputePipelineCreateInfo);
TListImpl(VkComputePipelineCreateInfo);

Bool VK_WRAP_FUNC(GraphicsDevice_createPipelineCompute)(
	GraphicsDevice *device,
	const CharString *name,
	Pipeline *pipeline,
	const SHBinaryInfo *buf,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDevice_getAlloc(device);

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

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
		.layout = *PipelineLayout_ext(PipelineLayoutRef_ptr(pipeline->layout), Vk)
	};

	gotoIfError3(clean, createShaderModule(
		buf->binaries[ESHBinaryType_SPIRV],
		&pipelineInfo.stage.module,
		deviceExt,
		instanceExt,
		name,
		EPipelineStage_Compute, alloc, e_rr
	));

	gotoIfError3(clean, checkVkError(deviceExt->createComputePipelines(
		deviceExt->device,
		NULL,
		1, &pipelineInfo,
		NULL,
		&pipelineHandle
	), e_rr));

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName && name && CharString_length(*name)) {

		if(!CharString_isNullTerminated(*name))
			gotoIfError3(clean, CharString_createCopy(*name, alloc, &temp, e_rr));

		VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_PIPELINE,
			.objectHandle = (U64) pipelineHandle,
			.pObjectName = temp.ptr ? temp.ptr : name->ptr
		};

		gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName2), e_rr));
		CharString_free(&temp, alloc);
	}

	*Pipeline_ext(pipeline, Vk) = pipelineHandle;
	pipelineHandle = NULL;

clean:

	if(pipelineHandle)
		deviceExt->destroyPipeline(deviceExt->device, pipelineHandle, NULL);

	const VkShaderModule mod = pipelineInfo.stage.module;

	if(mod)
		deviceExt->destroyShaderModule(deviceExt->device, mod, NULL);

	CharString_free(&temp, alloc);
	return s_uccess;
}
