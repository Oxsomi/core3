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

//graphics/vulkan/generic/vk_raytracing_pipeline.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/texture.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/constants.h"

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

TList(VkRayTracingPipelineCreateInfoKHR);
TList(VkShaderModule);
TList(VkRayTracingShaderGroupCreateInfoKHR);

TListImpl(VkRayTracingPipelineCreateInfoKHR);
TListImpl(VkShaderModule);
TListImpl(VkRayTracingShaderGroupCreateInfoKHR);

Bool VK_WRAP_FUNC(GraphicsDevice_createPipelineRaytracingInternal)(
	GraphicsDeviceRef *deviceRef,
	const ListSHFile *binaries,
	const CharString *name,
	U8 maxPayloadSize,
	U8 maxAttributeSize,
	const ListU32 *binaryIndices,
	Pipeline *pipeline,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	(void) maxPayloadSize;
	(void) maxAttributeSize;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	PipelineRaytracingInfo *rtPipeline = Pipeline_info(pipeline, PipelineRaytracingInfo);

	U64 stageCount = pipeline->stages.length;
	U64 hitGroupCount = rtPipeline->groups.length;
	U64 binaryCount = binaryIndices->length;

	VkPipeline pipelineHandle = NULL;
	ListVkShaderModule modules = (ListVkShaderModule) { 0 };
	ListVkPipelineShaderStageCreateInfo stages = (ListVkPipelineShaderStageCreateInfo) { 0 };
	ListVkRayTracingShaderGroupCreateInfoKHR groups = (ListVkRayTracingShaderGroupCreateInfoKHR) { 0 };
	GenericList shaderHandles = (GenericList) { .stride = raytracingShaderIdSize };
	Buffer shaderBindings = Buffer_createNull();
	CharString temp = CharString_createNull();

	gotoIfError3(clean, ListVkShaderModule_resize(&modules, binaryCount, alloc, e_rr));
	gotoIfError3(clean, ListVkPipelineShaderStageCreateInfo_resize(&stages, stageCount, alloc, e_rr));
	gotoIfError3(clean, GenericList_resize(&shaderHandles, stageCount, alloc, e_rr));

	gotoIfError3(clean, Buffer_createEmptyBytes(
		(hitGroupCount + stageCount) * raytracingShaderAlignment,
		alloc,
		&shaderBindings,
		e_rr
	));

	gotoIfError3(clean, ListVkRayTracingShaderGroupCreateInfoKHR_resize(&groups, hitGroupCount + stageCount, alloc, e_rr));

	//Create binaries

	for(U64 j = 0; j < binaryCount; ++j) {

		if(name && CharString_length(*name))
			gotoIfError3(clean, CharString_format(
				alloc,
				&temp,
				e_rr,
				"%.*s binary %"PRIu64,
				(int) CharString_length(*name),
				name->ptr,
				j
			));

		U32 binId = binaryIndices->ptr[j];
		U16 realBinaryId = (U16) binId;
		U16 shFileId = (U16)(binId >> 16);

		const SHBinaryInfo *info = &binaries->ptr[shFileId].binaries.ptr[realBinaryId];

		gotoIfError3(clean, createShaderModule(
			info->binaries[ESHBinaryType_SPIRV], &modules.ptrNonConst[j],
			deviceExt, instanceExt, &temp, EPipelineStage_RtStart, alloc, e_rr
		));

		CharString_free(&temp, alloc);
	}

	//Create groups

	for (U64 j = 0; j < hitGroupCount; ++j) {

		PipelineRaytracingGroup group = rtPipeline->groups.ptr[j];

		groups.ptrNonConst[j] = (VkRayTracingShaderGroupCreateInfoKHR) {

			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,

			.type =
				group.intersection == U32_MAX ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR :
				VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,

			.closestHitShader = group.closestHit,
			.anyHitShader = group.anyHit,
			.intersectionShader = group.intersection
		};
	}

	U32 groupCounter = (U32) hitGroupCount;
	U32 stageCounter = 0;

	for (U64 j = 0; j < stageCount; ++j) {

		PipelineStage *stage = &pipeline->stages.ptrNonConst[j];

		VkShaderStageFlagBits shaderStage = 0;

		switch (stage->stageType) {

			default:                                shaderStage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;            break;
			case EPipelineStage_ClosestHitExt:      shaderStage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;        break;
			case EPipelineStage_IntersectionExt:    shaderStage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;        break;

			case EPipelineStage_MissExt:
				shaderStage = VK_SHADER_STAGE_MISS_BIT_KHR;
				stage->localShaderId = rtPipeline->missCount++;
				stage->groupId = (U32) groupCounter;
				break;

			case EPipelineStage_RaygenExt:
				shaderStage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
				stage->localShaderId = rtPipeline->raygenCount++;
				stage->groupId = (U32) groupCounter;
				break;

			case EPipelineStage_CallableExt:
				shaderStage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
				stage->localShaderId = rtPipeline->callableCount++;
				stage->groupId = (U32) groupCounter;
				break;
		}

		//Generic shader

		if (!(stage->stageType >= EPipelineStage_RtHitStart && stage->stageType <= EPipelineStage_RtHitEnd))
			groups.ptrNonConst[groupCounter++] = (VkRayTracingShaderGroupCreateInfoKHR) {
				.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
				.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
				.generalShader = stage->binaryId == U32_MAX ? VK_SHADER_UNUSED_KHR : (U32) stageCounter,
				.closestHitShader = VK_SHADER_UNUSED_KHR,
				.anyHitShader = VK_SHADER_UNUSED_KHR,
				.intersectionShader = VK_SHADER_UNUSED_KHR
			};

		if(stage->binaryId == U32_MAX)        //Invalid shaders get skipped
			continue;

		U16 entrypointId = (U16) stage->binaryId;
		U16 binaryId = (U16) (stage->binaryId >> 16);

		SHEntry entry = binaries->ptr[stage->shFileId].entries.ptr[entrypointId];
		U16 realBinId = entry.binaryIds.ptr[binaryId];
		U32 binId = ((U32)stage->shFileId << 16) | realBinId;

		U64 libId = ListU32_findFirst(*binaryIndices, binId, 0, NULL);

		if(!CharString_isNullTerminated(entry.name))
			gotoIfError3(clean, CharString_createCopy(entry.name, alloc, &temp, e_rr));

		stages.ptrNonConst[stageCounter++] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = shaderStage,
			.module = modules.ptrNonConst[libId],
			.pName = temp.ptr ? temp.ptr : entry.name.ptr
		};

		CharString_free(&temp, alloc);
	}

	//Init create info

	VkPipelineCreateFlags flags = 0;

	//Capture ISA + statistics for live disassembly when requested and supported (VK_KHR_pipeline_executable_properties)

	if(
		(pipeline->flags & EPipelineFlags_CaptureISA) &&
		(device->info.capabilities.features2 & EGraphicsFeatures2_PipelineExecutableInfo)
	)
		flags |=
			VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR |
			VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR;

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

	VkRayTracingPipelineCreateInfoKHR info = (VkRayTracingPipelineCreateInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
		.flags = flags,
		.stageCount = (U32) stageCount,
		.pStages = stages.ptr,
		.groupCount = (U32) groupCounter,
		.pGroups = groups.ptr,
		.maxPipelineRayRecursionDepth = rtPipeline->maxRecursionDepth,
		.layout = *PipelineLayout_ext(PipelineLayoutRef_ptr(pipeline->layout), Vk)
	};

	//Create vulkan pipelines

	gotoIfError3(clean, checkVkError(deviceExt->createRaytracingPipelines(
		deviceExt->device,
		NULL,
		NULL,
		1, &info,
		NULL,
		&pipelineHandle
	), e_rr));

	//Create RefPtrs for OxC3 usage.

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

	//Fetch all shader handles

	gotoIfError3(clean, checkVkError(deviceExt->getRayTracingShaderGroupHandles(
		deviceExt->device,
		*Pipeline_ext(pipeline, Vk),
		0,
		groupCounter,
		raytracingShaderIdSize * groupCounter,
		shaderHandles.ptrNonConst
	), e_rr));

	//Fix SBT alignment

	for(U64 j = 0; j < groupCounter; ++j) {

		//Remap raygen, miss and callable shaders to be next to eachother

		U64 groupId = j;

		if (j >= hitGroupCount)
			for (U64 k = 0; k < stageCounter; ++k) {

				PipelineStage stage = pipeline->stages.ptr[k];

				if (stage.groupId != groupId)        //TODO: Better search
					continue;

				groupId = hitGroupCount;

				if (stage.stageType != EPipelineStage_MissExt) {

					groupId += rtPipeline->missCount;

					if (stage.stageType != EPipelineStage_RaygenExt)
						groupId += rtPipeline->raygenCount;
				}

				groupId += stage.localShaderId;
				break;
			}

		Buffer_memcpy(
			Buffer_createRef(shaderBindings.ptrNonConst + raytracingShaderAlignment * groupId,  raytracingShaderIdSize),
			Buffer_createRefConst((const U8*)shaderHandles.ptr + raytracingShaderIdSize * j, raytracingShaderIdSize)
		);
	}

	CharString sbtName = CharString_createRefCStrConst("Shader binding table");

	gotoIfError3(clean, GraphicsDeviceRef_createBufferData(
		deviceRef,
		EDeviceBufferUsage_SBTExt,
		EGraphicsResourceFlag_None,
		NULL,
		&sbtName,
		&shaderBindings,
		&Pipeline_info(pipeline, PipelineRaytracingInfo)->shaderBindingTable, e_rr
	));

clean:

	if(pipelineHandle)
		deviceExt->destroyPipeline(deviceExt->device, pipelineHandle, NULL);

	ListVkPipelineShaderStageCreateInfo_free(&stages, alloc);
	ListVkRayTracingShaderGroupCreateInfoKHR_free(&groups, alloc);

	for(U64 i = 0; i < modules.length; ++i)
		deviceExt->destroyShaderModule(deviceExt->device, modules.ptr[i], NULL);

	ListVkShaderModule_free(&modules, alloc);
	CharString_free(&temp, alloc);
	GenericList_free(&shaderHandles, alloc);
	Buffer_free(&shaderBindings, alloc);

	return s_uccess;
}
