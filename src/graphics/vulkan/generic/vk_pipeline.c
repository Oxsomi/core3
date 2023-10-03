/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/vulkan/vk_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "types/buffer.h"
#include "types/error.h"

Error createShaderModule(Buffer buf, VkShaderModule *mod, VkGraphicsDevice *device) {

	if(Buffer_length(buf) >> 32)
		return Error_outOfBounds(0, Buffer_length(buf), U32_MAX);

	if(!Buffer_length(buf) || Buffer_length(buf) % sizeof(U32))
		return Error_invalidParameter(0, 0);

	VkShaderModuleCreateInfo info = (VkShaderModuleCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (U32) Buffer_length(buf),
		.pCode = (const U32*) buf.ptr
	};

	return vkCheck(vkCreateShaderModule(device->device, &info, NULL, mod));
}

Bool Pipeline_free(Pipeline *pipeline, Allocator allocator) {

	allocator;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	for (U64 i = 0; i < pipeline->stages.length; ++i)
		Buffer_freex(&((PipelineStage*)pipeline->stages.ptr)[i].shaderBinary);

	List_freex(&pipeline->stages);
	vkDestroyPipeline(deviceExt->device, *Pipeline_ext(pipeline, Vk), NULL);

	RefPtr_dec(&pipeline->device);
	return true;
}

Error GraphicsDeviceRef_createPipelinesCompute(GraphicsDeviceRef *deviceRef, List *shaderBinaries, List *pipelines) {

	if(!deviceRef || !shaderBinaries || !pipelines)
		return Error_nullPointer(!deviceRef ? 0 : (!shaderBinaries ? 1 : 2));

	if(!shaderBinaries->length || shaderBinaries->stride != sizeof(Buffer))
		return Error_invalidParameter(1, 0);

	if(shaderBinaries->length >> 32)
		return Error_outOfBounds(1, shaderBinaries->length, U32_MAX);

	if(pipelines->ptr)
		return Error_invalidParameter(2, 0);

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(deviceRef, Vk);

	List pipelineInfos = List_createEmpty(sizeof(VkComputePipelineCreateInfo));
	List pipelineHandles = List_createEmpty(sizeof(VkPipeline));

	*pipelines = List_createEmpty(sizeof(PipelineRef*));

	Error err = List_resizex(&pipelineInfos, shaderBinaries->length);

	if(err.genericError)
		return err;

	_gotoIfError(clean, List_resizex(&pipelineHandles, shaderBinaries->length));
	_gotoIfError(clean, List_resizex(pipelines, shaderBinaries->length));

	//TODO: Push constants

	for(U64 i = 0; i < shaderBinaries->length; ++i) {

		((VkComputePipelineCreateInfo*)pipelineInfos.ptr)[i] = (VkComputePipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = (VkPipelineShaderStageCreateInfo) {
			
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.flags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pName = "main"
			},
			.layout = deviceExt->defaultLayout
		};

		_gotoIfError(clean, createShaderModule(
			((Buffer*)shaderBinaries->ptr)[i], 
			&((VkComputePipelineCreateInfo*) pipelineInfos.ptr)[i].stage.module, 
			deviceExt
		));
	}

	_gotoIfError(clean, vkCheck(vkCreateComputePipelines(
		deviceExt->device,
		NULL,
		(U32) pipelineInfos.length,
		(const VkComputePipelineCreateInfo*) pipelineInfos.ptr,
		NULL,
		(VkPipeline*) pipelineHandles.ptr
	)));

	for (U64 i = 0; i < pipelines->length; ++i) {

		RefPtr **refPtr = &((RefPtr**)pipelines->ptr)[i];

		_gotoIfError(clean, RefPtr_createx(
			(U32)(sizeof(Pipeline) + sizeof(VkPipeline)), 
			(ObjectFreeFunc) Pipeline_free, 
			EGraphicsTypeId_Pipeline, 
			refPtr
		));

		Pipeline *pipeline = PipelineRef_ptr(*refPtr);

		RefPtr_inc(deviceRef);

		*pipeline = (Pipeline) {
			.device = deviceRef,
			.type = EPipelineType_Compute,
			.stages = List_createEmpty(sizeof(PipelineStage))
		};

		_gotoIfError(clean, List_resizex(&pipeline->stages, 1));

		*(PipelineStage*) pipeline->stages.ptr = (PipelineStage) {
			.stageType = EPipelineStage_Compute,
			.shaderBinary = ((Buffer*)shaderBinaries->ptr)[i]
		};

		((Buffer*)shaderBinaries->ptr)[i] = Buffer_createNull();		//Moved

		*Pipeline_ext(pipeline, Vk) = ((VkPipeline*) pipelineHandles.ptr)[i];
	}

	List_freex(shaderBinaries);
	goto success;

clean:

	for (U64 i = 0; i < pipelines->length; ++i)
		RefPtr_dec(&((RefPtr**)pipelines->ptr)[i]);

	List_freex(pipelines);

success:

	List_freex(&pipelineHandles);

	for (U64 i = 0; i < pipelineInfos.length; ++i) {

		VkShaderModule mod = ((VkComputePipelineCreateInfo*) pipelineInfos.ptr)[i].stage.module;

		if(mod)
			vkDestroyShaderModule(deviceExt->device, mod, NULL);
	}

	List_freex(&pipelineInfos);
	return Error_none();
}

//List<PipelineStage> stages, List<PipelineGraphicsInfo> infos, List<PipelineRef*> *pipelines
//Error GraphicsDeviceRef_createPipelinesGraphics(GraphicsDeviceRef *deviceRef, List stages, List info, List *pipelines);