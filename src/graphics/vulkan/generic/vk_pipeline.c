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

//graphics/vulkan/generic/vk_pipeline.c

#include "types/container/list_impl.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "types/base/error.h"

TListImpl(VkPipelineShaderStageCreateInfo);

Bool createShaderModule(
	Buffer buf,
	VkShaderModule *mod,
	VkGraphicsDevice *device,
	VkGraphicsInstance *instanceExt,
	const CharString *name,
	EPipelineStage stage,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	(void)stage;

	if(Buffer_length(buf) >> 32)
		retError(clean, Error_outOfBounds(
			0, Buffer_length(buf), U32_MAX, "createShaderModule()::buf.length is limited to U32_MAX"
		));

	if(!Buffer_length(buf) || Buffer_length(buf) % sizeof(U32))
		retError(clean, Error_invalidParameter(
			0, 0, "createShaderModule()::buf.length must be in U32s when SPIR-V is used"
		));

	VkShaderModuleCreateInfo info = (VkShaderModuleCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (U32) Buffer_length(buf),
		.pCode = (const U32*) buf.ptr
	};

	CharString temp = CharString_createNull();

	gotoIfError3(clean, checkVkError(device->createShaderModule(device->device, &info, NULL, mod), e_rr));

	const GraphicsDevice *baseDevice = (const GraphicsDevice*)device - 1;

	if((baseDevice->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name) && instanceExt->debugSetName) {

		const Bool isRt = stage >= EPipelineStage_RtStart && stage <= EPipelineStage_RtEnd;

		gotoIfError3(clean, CharString_format(
			alloc,
			&temp,
			e_rr,
			"Shader module (\"%.*s\": %s)",
			(int) CharString_length(*name),
			name->ptr,
			isRt ? "Raytracing" : EPipelineStage_names[stage]
		));

		const VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_SHADER_MODULE,
			.objectHandle = (U64) *mod,
			.pObjectName = temp.ptr
		};

		gotoIfError3(clean, checkVkError(instanceExt->debugSetName(device->device, &debugName2), e_rr));
	}

	goto clean;

clean:

	CharString_free(&temp, alloc);

	if (!s_uccess && *mod)
		device->destroyShaderModule(device->device, *mod, NULL);

	return s_uccess;
}

void VK_WRAP_FUNC(Pipeline_free)(Pipeline *pipeline, const Allocator *alloc) {

	(void)alloc;

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	deviceExt->destroyPipeline(deviceExt->device, *Pipeline_ext(pipeline, Vk), NULL);
}

Bool VK_WRAP_FUNC(Pipeline_getExecutables)(
	Pipeline *pipeline,
	const Allocator *alloc,
	ListPipelineExecutable *result,
	Error *e_rr
) {
	Bool s_uccess = true;

	Buffer propsBuf = Buffer_createNull();
	Buffer statsBuf = Buffer_createNull();
	Buffer irsBuf = Buffer_createNull();
	Buffer irData = Buffer_createNull();
	ListPipelineExecutable executables = (ListPipelineExecutable) { 0 };

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkPipeline pipelineHandle = *Pipeline_ext(pipeline, Vk);

	if(!deviceExt->getPipelineExecutableProperties)
		retError(clean, Error_unsupportedOperation(0, "VkPipeline_getExecutables() query functions weren't loaded"));

	VkPipelineInfoKHR pipelineInfo = (VkPipelineInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR, .pipeline = pipelineHandle
	};

	//Executable properties (name / description / stages / subgroup size)

	U32 execCount = 0;
	gotoIfError3(clean, checkVkError(deviceExt->getPipelineExecutableProperties(
		deviceExt->device, &pipelineInfo, &execCount, NULL
	), e_rr));

	if(!execCount)
		goto clean;        //Nothing captured (e.g. pipeline wasn't created with the capture flags)

	gotoIfError3(clean, Buffer_createUninitializedBytes(
		sizeof(VkPipelineExecutablePropertiesKHR) * execCount, alloc, &propsBuf, e_rr
	));
	VkPipelineExecutablePropertiesKHR *props = (VkPipelineExecutablePropertiesKHR*) propsBuf.ptrNonConst;

	for(U32 i = 0; i < execCount; ++i)
		props[i] = (VkPipelineExecutablePropertiesKHR) { .sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR };

	gotoIfError3(clean, checkVkError(deviceExt->getPipelineExecutableProperties(
		deviceExt->device, &pipelineInfo, &execCount, props
	), e_rr));

	gotoIfError3(clean, ListPipelineExecutable_resize(&executables, execCount, alloc, e_rr));

	for(U32 i = 0; i < execCount; ++i) {

		PipelineExecutable *exec = &executables.ptrNonConst[i];
		exec->stages = (U32) props[i].stages;
		exec->subgroupSize = props[i].subgroupSize;

		CharString name = CharString_createRefCStrConst(props[i].name);
		CharString desc = CharString_createRefCStrConst(props[i].description);
		gotoIfError3(clean, CharString_createCopy(name, alloc, &exec->name, e_rr));
		gotoIfError3(clean, CharString_createCopy(desc, alloc, &exec->description, e_rr));

		VkPipelineExecutableInfoKHR execInfo = (VkPipelineExecutableInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR, .pipeline = pipelineHandle, .executableIndex = i
		};

		//Numeric statistics (VGPRs, SGPRs, occupancy, ...)

		U32 statCount = 0;
		gotoIfError3(clean, checkVkError(deviceExt->getPipelineExecutableStatistics(
			deviceExt->device, &execInfo, &statCount, NULL
		), e_rr));

		if(statCount) {

			Buffer_free(&statsBuf, alloc);
			gotoIfError3(clean, Buffer_createUninitializedBytes(
				sizeof(VkPipelineExecutableStatisticKHR) * statCount, alloc, &statsBuf, e_rr
			));
			VkPipelineExecutableStatisticKHR *stats = (VkPipelineExecutableStatisticKHR*) statsBuf.ptrNonConst;

			for(U32 j = 0; j < statCount; ++j)
				stats[j] = (VkPipelineExecutableStatisticKHR) { .sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR };

			gotoIfError3(clean, checkVkError(deviceExt->getPipelineExecutableStatistics(
				deviceExt->device, &execInfo, &statCount, stats
			), e_rr));

			gotoIfError3(clean, ListPipelineStatistic_resize(&exec->statistics, statCount, alloc, e_rr));

			for(U32 j = 0; j < statCount; ++j) {

				PipelineStatistic *st = &exec->statistics.ptrNonConst[j];
				gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(stats[j].name), alloc, &st->name, e_rr));
				gotoIfError3(clean, CharString_createCopy(
					CharString_createRefCStrConst(stats[j].description), alloc, &st->description, e_rr
				));

				switch(stats[j].format) {

					default:
					case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:
						st->format = EPipelineStatisticFormat_Bool; st->value = stats[j].value.b32; break;

					case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:
						st->format = EPipelineStatisticFormat_I64; st->value = (U64) stats[j].value.i64; break;

					case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:
						st->format = EPipelineStatisticFormat_U64; st->value = stats[j].value.u64; break;

					case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR: {
						st->format = EPipelineStatisticFormat_F64;
						F64 d = stats[j].value.f64;
						st->value = *(const U64*) &d;
						break;
					}
				}
			}
		}

		//Internal representations: first query the count + sizes/isText (pData = NULL), then fetch the first text one

		U32 irCount = 0;
		gotoIfError3(clean, checkVkError(deviceExt->getPipelineExecutableInternalRepresentations(
			deviceExt->device, &execInfo, &irCount, NULL
		), e_rr));

		if(irCount) {

			Buffer_free(&irsBuf, alloc);
			gotoIfError3(clean, Buffer_createUninitializedBytes(
				sizeof(VkPipelineExecutableInternalRepresentationKHR) * irCount, alloc, &irsBuf, e_rr
			));
			VkPipelineExecutableInternalRepresentationKHR *irs = (VkPipelineExecutableInternalRepresentationKHR*) irsBuf.ptrNonConst;

			for(U32 j = 0; j < irCount; ++j)
				irs[j] = (VkPipelineExecutableInternalRepresentationKHR) {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR
				};

			gotoIfError3(clean, checkVkError(deviceExt->getPipelineExecutableInternalRepresentations(
				deviceExt->device, &execInfo, &irCount, irs
			), e_rr));

			//Pick the first text representation (the human-readable ISA disassembly)

			U32 chosen = irCount;

			for(U32 j = 0; j < irCount; ++j)
				if(irs[j].isText && irs[j].dataSize) { chosen = j; break; }

			if(chosen < irCount) {

				Buffer_free(&irData, alloc);
				gotoIfError3(clean, Buffer_createEmptyBytes(irs[chosen].dataSize, alloc, &irData, e_rr));
				irs[chosen].pData = irData.ptrNonConst;

				gotoIfError3(clean, checkVkError(deviceExt->getPipelineExecutableInternalRepresentations(
					deviceExt->device, &execInfo, &irCount, irs
				), e_rr));

				//Driver text is NUL-terminated; wrap without the trailing NUL then copy into the executable

				U64 len = irs[chosen].dataSize;
				while(len && ((const C8*) irData.ptr)[len - 1] == '\0')
					--len;

				CharString isa = CharString_createRefSizedConst((const C8*) irData.ptr, len, false);
				gotoIfError3(clean, CharString_createCopy(isa, alloc, &exec->disassembly, e_rr));
			}
		}
	}

	*result = executables;
	executables = (ListPipelineExecutable) { 0 };

clean:
	ListPipelineExecutable_freeUnderlying(&executables, alloc);
	Buffer_free(&propsBuf, alloc);
	Buffer_free(&statsBuf, alloc);
	Buffer_free(&irsBuf, alloc);
	Buffer_free(&irData, alloc);
	return s_uccess;
}
