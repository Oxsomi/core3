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

TList(VkRayTracingPipelineCreateInfoKHR);
TList(VkShaderModule);
TList(VkPipelineShaderStageCreateInfo);
TList(VkRayTracingShaderGroupCreateInfoKHR);

TListImpl(VkRayTracingPipelineCreateInfoKHR);
TListImpl(VkShaderModule);
TListImpl(VkPipelineShaderStageCreateInfo);
TListImpl(VkRayTracingShaderGroupCreateInfoKHR);

Error GraphicsDevice_createPipelinesRaytracingInternalExt(
	GraphicsDeviceRef *deviceRef,
	ListCharString names,
	ListPipelineRef *pipelines,
	U64 stageCounter,
	U64 binaryCounter,
	U64 groupCounter
) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	Error err = Error_none();
	ListVkRayTracingPipelineCreateInfoKHR createInfos = (ListVkRayTracingPipelineCreateInfoKHR) { 0 };
	ListVkPipeline pipelinesExt = (ListVkPipeline) { 0 };
	ListVkShaderModule modules = (ListVkShaderModule) { 0 };
	ListVkPipelineShaderStageCreateInfo stages = (ListVkPipelineShaderStageCreateInfo) { 0 };
	ListVkRayTracingShaderGroupCreateInfoKHR groups = (ListVkRayTracingShaderGroupCreateInfoKHR) { 0 };
	GenericList shaderHandles = (GenericList) { .stride = raytracingShaderIdSize };
	Buffer shaderBindings = Buffer_createNull();
	DeviceBufferRef *sbt = NULL;
	CharString temp = CharString_createNull();
	CharString temp1 = CharString_createNull();

	//Reserve mem

	gotoIfError(clean, ListVkRayTracingPipelineCreateInfoKHR_resizex(&createInfos, pipelines->length))
	gotoIfError(clean, ListVkPipeline_resizex(&pipelinesExt, pipelines->length))
	gotoIfError(clean, ListVkShaderModule_resizex(&modules, binaryCounter))
	gotoIfError(clean, ListVkPipelineShaderStageCreateInfo_resizex(&stages, stageCounter))
	gotoIfError(clean, GenericList_resizex(&shaderHandles, stageCounter))
	gotoIfError(clean, Buffer_createEmptyBytesx(stageCounter * raytracingShaderAlignment, &shaderBindings))
	gotoIfError(clean, ListVkRayTracingShaderGroupCreateInfoKHR_resizex(&groups, groupCounter + stageCounter))

	//Convert

	binaryCounter = stageCounter = groupCounter = 0;

	for(U64 i = 0; i < pipelines->length; ++i) {

		Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[i]);
		PipelineRaytracingInfo *rtPipeline = Pipeline_info(pipeline, PipelineRaytracingInfo);

		CharString name = CharString_createNull();

		if (names.length) {

			if(!CharString_isNullTerminated(names.ptr[i])) {
				gotoIfError(clean, CharString_createCopyx(names.ptr[i], &temp1))
				name = temp1;
			}

			else name = names.ptr[i];
		}

		//Create binaries

		for(U64 j = 0; j < rtPipeline->binaryCount; ++j) {

			if(names.length)
				gotoIfError(clean, CharString_formatx(&temp, "%s binary %"PRIu64, name.ptr, j))

			gotoIfError(clean, createShaderModule(
				rtPipeline->binaries.ptr[j], &modules.ptrNonConst[binaryCounter + j],
				deviceExt, instanceExt, name, EPipelineStage_RtStart
			))

			CharString_freex(&temp);
		}

		//Create stages

		U64 startGroupCounter = groupCounter;

		//Create groups

		for (U64 j = 0; j < rtPipeline->groupCount; ++j) {

			PipelineRaytracingGroup group = rtPipeline->groups.ptr[j];

			groups.ptrNonConst[groupCounter + j] = (VkRayTracingShaderGroupCreateInfoKHR) {

				.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,

				.type =
					group.intersection == U32_MAX ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR :
					VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,

				.closestHitShader = group.closestHit,
				.anyHitShader = group.anyHit,
				.intersectionShader = group.intersection
			};
		}

		groupCounter += rtPipeline->groupCount;

		U64 stageStart = stageCounter;

		for (U64 j = 0; j < rtPipeline->stageCount; ++j) {

			PipelineStage *stage = &pipeline->stages.ptrNonConst[j];

			VkShaderStageFlagBits shaderStage = 0;

			switch (stage->stageType) {

				default:								shaderStage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;			break;
				case EPipelineStage_ClosestHitExt:		shaderStage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;		break;
				case EPipelineStage_IntersectionExt:	shaderStage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;		break;

				case EPipelineStage_MissExt:
					shaderStage = VK_SHADER_STAGE_MISS_BIT_KHR;
					stage->localShaderId = rtPipeline->missCount;
					stage->groupId = (U32)(groupCounter - startGroupCounter);
					++rtPipeline->missCount;
					break;

				case EPipelineStage_RaygenExt:
					shaderStage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
					stage->localShaderId = rtPipeline->raygenCount;
					stage->groupId = (U32)(groupCounter - startGroupCounter);
					++rtPipeline->raygenCount;
					break;

				case EPipelineStage_CallableExt:
					shaderStage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
					stage->localShaderId = rtPipeline->callableCount;
					stage->groupId = (U32)(groupCounter - startGroupCounter);
					++rtPipeline->callableCount;
					break;
			}

			//Generic shader

			if (!(stage->stageType >= EPipelineStage_RtHitStart && stage->stageType <= EPipelineStage_RtHitEnd))
				groups.ptrNonConst[groupCounter++] = (VkRayTracingShaderGroupCreateInfoKHR) {
					.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
					.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					.generalShader = stage->binaryId == U32_MAX ? VK_SHADER_UNUSED_KHR : (U32) (stageCounter - stageStart),
					.closestHitShader = VK_SHADER_UNUSED_KHR,
					.anyHitShader = VK_SHADER_UNUSED_KHR,
					.intersectionShader = VK_SHADER_UNUSED_KHR
				};

			if(stage->binaryId == U32_MAX)		//Invalid shaders get skipped
				continue;

			CharString entrypoint = !rtPipeline->entrypoints.ptr ? CharString_createNull() : rtPipeline->entrypoints.ptr[j];

			stages.ptrNonConst[stageCounter++] = (VkPipelineShaderStageCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = shaderStage,
				.module = modules.ptrNonConst[binaryCounter + stage->binaryId],
				.pName = entrypoint.ptr ? entrypoint.ptr : "main"
			};
		}

		//Init create info

		VkPipelineCreateFlags flags = 0;

		if(rtPipeline->flags & EPipelineRaytracingFlags_SkipAABBs)
			flags |= VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR;

		if(rtPipeline->flags & EPipelineRaytracingFlags_SkipTriangles)
			flags |= VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR;

		if(rtPipeline->flags & EPipelineRaytracingFlags_AllowMotionBlurExt)
			flags |= VK_PIPELINE_CREATE_RAY_TRACING_ALLOW_MOTION_BIT_NV;

		if(rtPipeline->flags & EPipelineRaytracingFlags_NoNullAnyHit)
			flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_BIT_KHR;

		if(rtPipeline->flags & EPipelineRaytracingFlags_NoNullClosestHit)
			flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR;

		if(rtPipeline->flags & EPipelineRaytracingFlags_NoNullMiss)
			flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR;

		if(rtPipeline->flags & EPipelineRaytracingFlags_NoNullIntersection)
			flags |= VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR;

		createInfos.ptrNonConst[i] = (VkRayTracingPipelineCreateInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
			.flags = flags,
			.stageCount = rtPipeline->stageCount,
			.pStages = stages.ptr + stageStart,
			.groupCount = (U32)(groupCounter - startGroupCounter),
			.pGroups = groups.ptr + startGroupCounter,
			.maxPipelineRayRecursionDepth = rtPipeline->maxRecursionDepth,
			.layout = deviceExt->defaultLayout
		};

		//Continue

		CharString_freex(&temp1);

		binaryCounter += rtPipeline->binaryCount;
	}

	//Create vulkan pipelines

	gotoIfError(clean, vkCheck(instanceExt->createRaytracingPipelines(
		deviceExt->device,
		NULL,						//deferredOperation
		NULL,						//pipelineCache
		(U32) pipelines->length,
		createInfos.ptr,
		NULL,						//allocator
		pipelinesExt.ptrNonConst
	)))

	//Create RefPtrs for OxC3 usage.

	groupCounter = 0;

	for (U64 i = 0; i < pipelines->length; ++i) {

		#ifndef NDEBUG

			if(instanceExt->debugSetName && names.length) {

				VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_PIPELINE,
					.objectHandle = (U64) pipelinesExt.ptr[i],
					.pObjectName = names.ptr[i].ptr
				};

				gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)))
			}

		#endif

		VkPipeline vkPipeline = pipelinesExt.ptrNonConst[i];
		*Pipeline_ext(PipelineRef_ptr(pipelines->ptr[i]), Vk) = vkPipeline;
		pipelinesExt.ptrNonConst[i] = NULL;

		//Fetch all shader handles

		PipelineRaytracingInfo *rtPipeline = Pipeline_info(PipelineRef_ptr(pipelines->ptr[i]), PipelineRaytracingInfo);

		U32 groupCount = rtPipeline->missCount + rtPipeline->raygenCount + rtPipeline->callableCount + rtPipeline->groupCount;

		gotoIfError(clean, vkCheck(instanceExt->getRayTracingShaderGroupHandles(
			deviceExt->device,
			vkPipeline,
			0,
			groupCount,
			raytracingShaderIdSize * groupCount,
			shaderHandles.ptrNonConst
		)))

		//Fix SBT alignment

		Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[i]);

		for(U64 j = 0; j < groupCount; ++j) {

			//Remap raygen, miss and callable shaders to be next to eachother

			U64 groupId = j;

			if (j >= rtPipeline->groupCount)
				for (U64 k = 0; k < rtPipeline->stageCount; ++k) {

					PipelineStage stage = pipeline->stages.ptr[k];

					if (stage.groupId != groupId)		//TODO: Better search
						continue;

					groupId = rtPipeline->groupCount;

					if (stage.stageType != EPipelineStage_MissExt) {

						groupId += rtPipeline->missCount;

						if (stage.stageType != EPipelineStage_RaygenExt)
							groupId += rtPipeline->raygenCount;
					}

					groupId += stage.localShaderId;
					break;
				}

			Buffer_copy(
				Buffer_createRef(
					(U8*)shaderBindings.ptr + raytracingShaderAlignment * (groupCounter + groupId), raytracingShaderIdSize
				),
				Buffer_createRefConst((const U8*)shaderHandles.ptr + raytracingShaderIdSize * j, raytracingShaderIdSize)
			);
		}

		rtPipeline->sbtOffset = groupCounter * raytracingShaderAlignment;
		groupCounter += groupCount;
	}

	ListVkPipeline_freex(&pipelinesExt);

	//Create one big SBT with all handles in it.
	//The SBT has a stride of 64 as well (since that's expected).
	//Then we link the SBT per pipeline.

	gotoIfError(clean, GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_SBTExt, EGraphicsResourceFlag_None,
		CharString_createRefCStrConst("Shader binding table"),
		&shaderBindings,
		&sbt
	))

	for (U64 i = 0; i < pipelines->length; ++i) {
		gotoIfError(clean, DeviceBufferRef_inc(sbt))
		Pipeline_info(PipelineRef_ptr(pipelines->ptr[i]), PipelineRaytracingInfo)->shaderBindingTable = sbt;
	}

clean:

	for(U64 i = 0; i < pipelinesExt.length; ++i)
		if(pipelinesExt.ptr[i])
			vkDestroyPipeline(deviceExt->device, pipelinesExt.ptr[i], NULL);

	ListVkRayTracingPipelineCreateInfoKHR_freex(&createInfos);
	ListVkPipeline_freex(&pipelinesExt);
	ListVkPipelineShaderStageCreateInfo_freex(&stages);
	ListVkRayTracingShaderGroupCreateInfoKHR_freex(&groups);

	for(U64 i = 0; i < modules.length; ++i)
		vkDestroyShaderModule(deviceExt->device, modules.ptr[i], NULL);

	ListVkShaderModule_freex(&modules);
	CharString_freex(&temp);
	CharString_freex(&temp1);
	GenericList_freex(&shaderHandles);
	Buffer_freex(&shaderBindings);
	DeviceBufferRef_dec(&sbt);

	return err;
}
