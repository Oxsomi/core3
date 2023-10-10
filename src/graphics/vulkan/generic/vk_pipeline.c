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
#include "formats/texture.h"
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

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

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
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
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

Error mapVkCompareOp(ECompareOp op, VkCompareOp *result) {

	switch (op) {
			
		case ECompareOp_gt:			*result = VK_COMPARE_OP_GREATER;			break;
		case ECompareOp_geq:		*result = VK_COMPARE_OP_GREATER_OR_EQUAL;	break;
		case ECompareOp_eq:			*result = VK_COMPARE_OP_EQUAL;				break;
		case ECompareOp_neq:		*result = VK_COMPARE_OP_NOT_EQUAL;			break;
		case ECompareOp_leq:		*result = VK_COMPARE_OP_LESS_OR_EQUAL;		break;
		case ECompareOp_lt:			*result = VK_COMPARE_OP_LESS;				break;
		case ECompareOp_always:		*result = VK_COMPARE_OP_ALWAYS;				break;
		case ECompareOp_never:		*result = VK_COMPARE_OP_NEVER;				break;

		default:
			return Error_invalidOperation(0);
	}

	return Error_none();
}

Error mapVkStencilOp(EStencilOp op, VkStencilOp *result) {

	switch (op) {
			
		case EStencilOp_keep:		*result = VK_STENCIL_OP_KEEP;					break;
		case EStencilOp_zero:		*result = VK_STENCIL_OP_ZERO;					break;
		case EStencilOp_replace:	*result = VK_STENCIL_OP_REPLACE;				break;
		case EStencilOp_incClamp:	*result = VK_STENCIL_OP_INCREMENT_AND_CLAMP;	break;
		case EStencilOp_decClamp:	*result = VK_STENCIL_OP_DECREMENT_AND_CLAMP;	break;
		case EStencilOp_invert:		*result = VK_STENCIL_OP_INVERT;					break;
		case EStencilOp_incWrap:	*result = VK_STENCIL_OP_INCREMENT_AND_WRAP;		break;
		case EStencilOp_decWrap:	*result = VK_STENCIL_OP_DECREMENT_AND_WRAP;		break;

		default:
			return Error_invalidOperation(0);
	}

	return Error_none();
}

Error mapVkBlendOp(EBlendOp op, VkBlendOp *result) {

	switch (op) {

		case EBlendOp_add:					*result = VK_BLEND_OP_ADD;				break;
		case EBlendOp_subtract:				*result = VK_BLEND_OP_SUBTRACT;			break;
		case EBlendOp_reverseSubtract:		*result = VK_BLEND_OP_REVERSE_SUBTRACT;	break;
		case EBlendOp_min:					*result = VK_BLEND_OP_MIN;				break;
		case EBlendOp_max:					*result = VK_BLEND_OP_MAX;				break;

		default:
			return Error_invalidOperation(0);
	}

	return Error_none();
}

Error mapVkBlend(Bool dualSrc, EBlend op, VkBlendFactor *result) {

	Bool wasDualSrc = false;

	switch (op) {
																								break;
		case EBlend_zero:				*result = VK_BLEND_FACTOR_ZERO;							break;
		case EBlend_one:				*result = VK_BLEND_FACTOR_ONE;							break;
		case EBlend_srcColor:			*result = VK_BLEND_FACTOR_SRC_COLOR;					break;
		case EBlend_invSrcColor:		*result = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;			break;
		case EBlend_dstColor:			*result = VK_BLEND_FACTOR_DST_COLOR;					break;
		case EBlend_invDstColor:		*result = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;			break;
		case EBlend_srcAlpha:			*result = VK_BLEND_FACTOR_SRC_ALPHA;					break;
		case EBlend_invSrcAlpha:		*result = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;			break;
		case EBlend_dstAlpha:			*result = VK_BLEND_FACTOR_DST_ALPHA;					break;
		case EBlend_invDstAlpha:		*result = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;			break;
		case EBlend_blendFactor:		*result = VK_BLEND_FACTOR_CONSTANT_COLOR;				break;
		case EBlend_invBlendFactor:		*result = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;		break;
		case EBlend_alphaFactor:		*result = VK_BLEND_FACTOR_CONSTANT_ALPHA;				break;
		case EBlend_invAlphaFactor:		*result = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;		break;
		case EBlend_srcAlphaSat:		*result = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;			break;

		case EBlend_src1ColorExt:		wasDualSrc = true; *result = VK_BLEND_FACTOR_ZERO;		break;
		case EBlend_src1AlphaExt:		wasDualSrc = true; *result = VK_BLEND_FACTOR_ZERO;		break;
		case EBlend_invSrc1ColorExt:	wasDualSrc = true; *result = VK_BLEND_FACTOR_ZERO;		break;
		case EBlend_invSrc1AlphaExt:	wasDualSrc = true; *result = VK_BLEND_FACTOR_ZERO;		break;

		default:
			return Error_invalidOperation(0);
	}

	if(wasDualSrc && !dualSrc)
		return Error_invalidOperation(1);

	return Error_none();
}

Error GraphicsDeviceRef_createPipelinesGraphics(GraphicsDeviceRef *deviceRef, List *stages, List *infos, List *pipelines) {

	if(!deviceRef || !stages || !infos || !pipelines)
		return Error_nullPointer(!deviceRef ? 0 : (!stages ? 1 : (!infos ? 2 : 3)));

	if(!stages->length || stages->stride != sizeof(PipelineStage))
		return Error_invalidParameter(1, 0);

	if(!infos->length || infos->stride != sizeof(PipelineGraphicsInfo))
		return Error_invalidParameter(2, 0);

	if(pipelines->ptr)
		return Error_invalidParameter(3, 0);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	typedef enum EPipelineStateType {

		EPipelineStateType_MSAA,
		EPipelineStateType_VertexInput,
		EPipelineStateType_InputAssembly,
		EPipelineStateType_Tessellation,
		EPipelineStateType_Rasterizer,
		EPipelineStateType_DepthStencil,
		EPipelineStateType_BlendState,
		EPipelineStateType_DirectRendering,
		EPipelineStateType_PipelineCreateInfo,
		EPipelineStateType_VkPipeline,

		EPipelineStateType_PerPipelineStart = EPipelineStateType_MSAA,
		EPipelineStateType_PerPipelineEnd = EPipelineStateType_VkPipeline,

		//Multiple per pipeline. Needs to calculate size manually

		EPipelineStateType_Stage,
		EPipelineStateType_BlendAttachment,
		EPipelineStateType_DirectRenderingAttachments,
		EPipelineStateType_InputBuffers,
		EPipelineStateType_InputAttributes,

		EPipelineStateType_Count,

		EPipelineStateType_PerPipelinePropertyStart = EPipelineStateType_Stage,
		EPipelineStateType_PerPipelinePropertyEnd = EPipelineStateType_InputAttributes,

		EPipelineStageType_PerPipelineCount = 
			EPipelineStateType_PerPipelinePropertyStart - EPipelineStateType_PerPipelineStart,

		EPipelineStageType_PerPipelinePropertyCount = 
			EPipelineStateType_Count - EPipelineStateType_PerPipelinePropertyStart,

	} EPipelineStateType;

	U64 counts[EPipelineStateType_Count] = { 0 };
	Error err = Error_none();

	counts[EPipelineStateType_PipelineCreateInfo] = infos->length;
	counts[EPipelineStateType_VkPipeline] = infos->length;

	for(U64 i = 0; i < infos->length; ++i) {

		const PipelineGraphicsInfo *info = &((const PipelineGraphicsInfo*) infos->ptr)[i];

		//Count if available

		if (info->msaa)
			++counts[EPipelineStateType_MSAA];

		if (info->topologyMode)
			++counts[EPipelineStateType_InputAssembly];

		if (info->patchControlPointsExt)
			++counts[EPipelineStateType_Tessellation];

		if(info->rasterizer.flags || info->rasterizer.cullMode)
			++counts[EPipelineStateType_Rasterizer];

		if(info->depthStencil.flags)
			++counts[EPipelineStateType_DepthStencil];

		if(
			info->blendState.logicOpExt || 
			info->blendState.renderTargetMask || 
			info->blendState.renderTargetsCount ||
			info->blendState.allowIndependentBlend
		)
			++counts[EPipelineStateType_BlendState];

		if(info->attachmentCountExt || info->depthFormatExt)
			++counts[EPipelineStateType_DirectRendering];

		//Count variable

		U64 countStart = counts[EPipelineStateType_Stage];

		counts[EPipelineStateType_Stage] += info->stageCount;
		counts[EPipelineStateType_BlendAttachment] += info->attachmentCountExt;		//TODO: if renderPass

		Bool anyAttrib = false;

		for(U64 j = 0; j < 16; ++j) {

			if(info->vertexLayout.bufferStrides12_isInstance1[j] & 4095)
				++counts[EPipelineStateType_InputBuffers];

			if(info->vertexLayout.attributes[j].format) {
				++counts[EPipelineStateType_InputAttributes];
				anyAttrib = true;
			}
		}

		if(anyAttrib)
			++counts[EPipelineStateType_VertexInput];

		//Validate info struct

		//Validate some basics

		if(info->attachmentCountExt > 8)
			return Error_outOfBounds(i, info->attachmentCountExt, 8);

		if(!info->stageCount || counts[EPipelineStateType_Stage] > stages->length || info->stageCount >> 32)
			return Error_invalidOperation(0);

		if(!info->renderPass && info->subPass)
			return Error_invalidOperation(1);

		//Validate tesselation and msaa extensions

		if(info->patchControlPointsExt && !(device->info.capabilities.features & EGraphicsFeatures_TessellationShader))
			return Error_unsupportedOperation(0);

		U32 dataTypeCheck = 0;

		switch (info->msaa) {

			case EMSAASamples_x2Ext:
				dataTypeCheck = EGraphicsDataTypes_MSAA2x;
				break;

			case EMSAASamples_x8Ext:
				dataTypeCheck = EGraphicsDataTypes_MSAA8x;
				break;

			case EMSAASamples_x16Ext:
				dataTypeCheck = EGraphicsDataTypes_MSAA16x;
				break;
		}

		if(dataTypeCheck && !(device->info.capabilities.dataTypes & dataTypeCheck))
			return Error_unsupportedOperation(1);

		//Validate render pass / attachments

		if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {

			//Force renderPass definition and no direct rendering stuff

			if(!info->renderPass || info->attachmentCountExt || info->depthFormatExt)
				return Error_unsupportedOperation(2);
		}

		else {

			//Force one of render pass or dynamic rendering.

			if((Bool)info->renderPass == (info->attachmentCountExt || info->depthFormatExt))
				return Error_invalidOperation(4);
		}

		//Validate that stages are unique, are compatible.

		U64 stageFlags = 0;

		for (U64 j = countStart; j < countStart + info->stageCount; ++j) {

			PipelineStage stage = ((const PipelineStage*)stages->ptr)[j - countStart];

			if((stageFlags >> stage.stageType) & 1)
				return Error_alreadyDefined(0);

			if(stageFlags && stage.stageType == EPipelineStage_Compute)
				return Error_invalidState(0);
		}

		//Validate if stages are allowed due to TesselationShader, GeometryShader

		if(
			stageFlags & (((U64)1 << EPipelineStage_HullExt) | ((U64)1 << EPipelineStage_DomainExt)) && 
			!(device->info.capabilities.features & EGraphicsFeatures_TessellationShader)
		)
			return Error_unsupportedOperation(10);

		if(
			((stageFlags >> EPipelineStage_GeometryExt) & 1) && 
			!(device->info.capabilities.features & EGraphicsFeatures_GeometryShader)
		)
			return Error_unsupportedOperation(11);

		//TODO: Implement renderPass here!

		if(info->renderPass)
			return Error_unsupportedOperation(3);
	}

	counts[EPipelineStateType_DirectRenderingAttachments] = counts[EPipelineStateType_BlendAttachment];

	//Create temp data to store these
	//But not every part is properly filled.
	//If the default values are used, it will use a default one instead to avoid having to re-create.

	U64 strides[EPipelineStateType_Count] = {
		sizeof(VkPipelineMultisampleStateCreateInfo),
		sizeof(VkPipelineVertexInputStateCreateInfo),
		sizeof(VkPipelineInputAssemblyStateCreateInfo),
		sizeof(VkPipelineTessellationStateCreateInfo),
		sizeof(VkPipelineRasterizationStateCreateInfo),
		sizeof(VkPipelineDepthStencilStateCreateInfo),
		sizeof(VkPipelineColorBlendStateCreateInfo),
		sizeof(VkPipelineRenderingCreateInfo),
		sizeof(VkGraphicsPipelineCreateInfo),
		sizeof(VkPipeline),
		sizeof(VkPipelineShaderStageCreateInfo),
		sizeof(VkPipelineColorBlendAttachmentState),
		sizeof(VkFormat),
		sizeof(VkVertexInputBindingDescription),
		sizeof(VkVertexInputAttributeDescription)
	};

	List states[EPipelineStateType_Count] = { 0 };

	for(U64 i = EPipelineStateType_PerPipelineStart; i < EPipelineStateType_Count; ++i) {
		states[i] = List_createEmpty(strides[i]);
		_gotoIfError(clean, List_resizex(&states[i], counts[i]));
	}

	*pipelines = List_createEmpty(sizeof(PipelineRef*));
	_gotoIfError(clean, List_resizex(pipelines, infos->length));

	//TODO: Push constants

	VkDynamicState dynamicStates[4] = {
		VK_DYNAMIC_STATE_VIEWPORT, 
		VK_DYNAMIC_STATE_SCISSOR, 
		VK_DYNAMIC_STATE_STENCIL_REFERENCE, 
		VK_DYNAMIC_STATE_BLEND_CONSTANTS
	};

	VkPipelineDynamicStateCreateInfo dynamicState = (VkPipelineDynamicStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]),
		.pDynamicStates = dynamicStates
	};

	VkPipelineViewportStateCreateInfo viewport = (VkPipelineViewportStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkPipelineMultisampleStateCreateInfo msaaState = (VkPipelineMultisampleStateCreateInfo) { 
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineTessellationStateCreateInfo tessState = (VkPipelineTessellationStateCreateInfo) { 
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState = (VkPipelineDepthStencilStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.minDepthBounds = 1,
		.maxDepthBounds = 0
	};

	VkPipelineRasterizationStateCreateInfo rasterState = (VkPipelineRasterizationStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = (VkPipelineInputAssemblyStateCreateInfo) {
		.flags = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	VkPipelineVertexInputStateCreateInfo vertexInput = (VkPipelineVertexInputStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	VkPipelineColorBlendStateCreateInfo blendState = (VkPipelineColorBlendStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
	};

	for(U64 i = EPipelineStateType_PerPipelinePropertyStart; i < EPipelineStateType_Count; ++i)
		counts[i] = 0;

	for(U64 i = 0; i < infos->length; ++i) {

		PipelineGraphicsInfo *info = &((const PipelineGraphicsInfo*) infos->ptr)[i];

		//Convert info struct to vulkan struct

		VkGraphicsPipelineCreateInfo *currentInfo = 
			&((VkGraphicsPipelineCreateInfo*)states[EPipelineStateType_PipelineCreateInfo].ptr)[i];

		*currentInfo = (VkGraphicsPipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = info->stageCount,
			//.pStages = stages,
			.pVertexInputState = &vertexInput,
			.pInputAssemblyState = &inputAssembly,
			.pTessellationState = &tessState,
			.pViewportState = &viewport,
			.pRasterizationState = &rasterState,
			.pMultisampleState = &msaaState,
			.pDepthStencilState = &depthStencilState,
			.pColorBlendState = &blendState,
			.pDynamicState = &dynamicState,
			.layout = deviceExt->defaultLayout
		};

		//Vertex input

		U64 vertexBuffers = 0, vertexAttribs = 0;

		for(U32 j = 0; j < 16; ++j) {

			//Validate buffer strides

			U16 stride = info->vertexLayout.bufferStrides12_isInstance1[j] & 4095;

			if(stride > 2048)
				_gotoIfError(clean, Error_invalidOperation(7));

			if(stride)
				++vertexBuffers;

			//Validate format for attribute

			GraphicsBufferAttribute attrib = info->vertexLayout.attributes[j];

			if(attrib.format >= ETextureFormatId_Count)
				_gotoIfError(clean, Error_invalidOperation(8));

			ETextureFormat format = ETextureFormatId_unpack[attrib.format];

			if(!format)
				continue;

			++vertexAttribs;

			if(ETextureFormat_getIsCompressed(format))
				_gotoIfError(clean, Error_invalidOperation(9));

			//Validate bounds for attribute

			U64 size = ETextureFormat_getSize(format, 1, 1);
			U64 offset = attrib.offset11_bufferId4 & 2047;
			U64 bufferId = attrib.offset11_bufferId4 >> 11;

			if(offset + size > stride)
				_gotoIfError(clean, Error_outOfBounds(0, offset + size, stride));
		}

		//Transform into description

		if (vertexAttribs) {

			VkVertexInputBindingDescription *bindings = 
				&((VkVertexInputBindingDescription*)states[EPipelineStateType_InputBuffers].ptr)[
					counts[EPipelineStateType_InputBuffers]
				];

			VkVertexInputAttributeDescription *attributes = 
				&((VkVertexInputAttributeDescription*)states[EPipelineStateType_InputAttributes].ptr)[
					counts[EPipelineStateType_InputAttributes]
				];
			
			U32 bindingCount = 0;

			for (U32 j = 0; j < 16; ++j) {

				U16 bufferStrides12_isInstance1 = info->vertexLayout.bufferStrides12_isInstance1[j];
				U16 stride = bufferStrides12_isInstance1 & 4095;

				if(!stride)
					continue;

				bindings[bindingCount++] = (VkVertexInputBindingDescription) {
					.binding = j,
					.inputRate = 
						(bufferStrides12_isInstance1 >> 12) & 1 ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
					.stride = stride
				};
			}

			counts[EPipelineStateType_InputBuffers] += bindingCount;

			U32 attribCount = 0;

			for (U32 j = 0; j < 16; ++j) {

				GraphicsBufferAttribute attrib = info->vertexLayout.attributes[j];

				if(!attrib.format)
					continue;

				ETextureFormat format = ETextureFormatId_unpack[attrib.format];
				VkFormat formatExt = 0;

				_gotoIfError(clean, mapVkFormat(NULL, format, &formatExt, true, false));

				attributes[attribCount++] = (VkVertexInputAttributeDescription) {
					.location = j,
					.binding = (attrib.offset11_bufferId4 >> 11) & 0xF,
					.format = formatExt,
					.offset = attrib.offset11_bufferId4 & 2047
				};
			}

			counts[EPipelineStateType_InputAttributes] += attribCount;

			VkPipelineVertexInputStateCreateInfo *infoi = 
				&((VkPipelineVertexInputStateCreateInfo*)states[EPipelineStateType_VertexInput].ptr)[i];

			*infoi = (VkPipelineVertexInputStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.vertexBindingDescriptionCount = bindingCount,
				.vertexAttributeDescriptionCount = attribCount,
				.pVertexAttributeDescriptions = attributes,
				.pVertexBindingDescriptions = bindings
			};

			currentInfo->pVertexInputState = infoi;
		}

		//IA

		if (info->topologyMode) {

			VkPrimitiveTopology topology = 0;

			switch (info->topologyMode) {
				
				case ETopologyMode_TriangleList:		topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;		break;
				case ETopologyMode_TriangleStrip:		topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;	break;

				case ETopologyMode_LineList:			topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;			break;
				case ETopologyMode_LineStrip:			topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;		break;

				case ETopologyMode_PointList:			topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;		break;

				case ETopologyMode_TriangleListAdj:
					topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
					break;

				case ETopologyMode_TriangleStripAdj:
					topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
					break;

				case ETopologyMode_LineListAdj:
					topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
					break;

				case ETopologyMode_LineStripAdj:
					topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
					break;

				default:
					_gotoIfError(clean, Error_unsupportedOperation(9));
			}

			VkPipelineInputAssemblyStateCreateInfo *infoi = 
				&((const VkPipelineInputAssemblyStateCreateInfo*)states[EPipelineStateType_InputAssembly].ptr)[i];

			*infoi = (VkPipelineInputAssemblyStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = topology
			};

			currentInfo->pInputAssemblyState = infoi;
		}

		//Rasterizer

		if(
			!(info->rasterizer.flags & ERasterizerFlags_enableDepthBias) && (
				info->rasterizer.depthBiasConstantFactor ||
				info->rasterizer.depthBiasClamp ||
				info->rasterizer.depthBiasSlopeFactor
			)
		)
			_gotoIfError(clean, Error_unsupportedOperation(5));

		if(info->rasterizer.flags || info->rasterizer.cullMode) {

			Bool wireframe = info->rasterizer.flags & ERasterizerFlags_isWireframeExt;

			if(
				wireframe && 
				!(device->info.capabilities.features & EGraphicsFeatures_Wireframe)
			)
				_gotoIfError(clean, Error_unsupportedOperation(4));

			VkCullModeFlags cullMode = 0;

			switch (info->rasterizer.cullMode) {

				case ECullMode_back:	cullMode = VK_CULL_MODE_BACK_BIT;	break;
				case ECullMode_front:	cullMode = VK_CULL_MODE_FRONT_BIT;	break;
				case ECullMode_none:	cullMode = VK_CULL_MODE_NONE;		break;

				default:
					_gotoIfError(clean, Error_invalidOperation(0));
			}

			VkFrontFace windingOrder = 
				info->rasterizer.flags & ERasterizerFlags_isClockWise ? VK_FRONT_FACE_CLOCKWISE : 
				VK_FRONT_FACE_COUNTER_CLOCKWISE;
			
			VkPipelineRasterizationStateCreateInfo *infoi = 
				&((const VkPipelineRasterizationStateCreateInfo*)states[EPipelineStateType_Rasterizer].ptr)[i];

			*infoi = (VkPipelineRasterizationStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.cullMode = cullMode,
				.frontFace = windingOrder,
				.depthClampEnable = info->rasterizer.flags & ERasterizerFlags_enableDepthClamp,
				.polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
				.depthBiasEnable = info->rasterizer.flags & ERasterizerFlags_enableDepthBias,
				.depthBiasConstantFactor = info->rasterizer.depthBiasConstantFactor,
				.depthBiasClamp = info->rasterizer.depthBiasClamp,
				.depthBiasSlopeFactor = info->rasterizer.depthBiasSlopeFactor,
				.lineWidth = 1
			};

			currentInfo->pRasterizationState = infoi;
		}

		//Depth stencil

		VkStencilOpState stencil = (VkStencilOpState) { 0 };

		if (info->depthStencil.flags & EDepthStencilFlags_stencilTest) {

			VkCompareOp stencilCompareOp = 0;
			_gotoIfError(clean, mapVkCompareOp(info->depthStencil.stencilCompare, &stencilCompareOp));

			VkStencilOp failOp = 0, passOp = 0, depthFailOp = 0;
			_gotoIfError(clean, mapVkStencilOp(info->depthStencil.stencilFail, &failOp));
			_gotoIfError(clean, mapVkStencilOp(info->depthStencil.stencilPass, &passOp));
			_gotoIfError(clean, mapVkStencilOp(info->depthStencil.stencilDepthFail, &depthFailOp));

			stencil = (VkStencilOpState) {
				.failOp = failOp,
				.passOp = passOp,
				.depthFailOp = depthFailOp,
				.compareOp = stencilCompareOp,
				.compareMask = info->depthStencil.stencilReadMask,
				.writeMask = info->depthStencil.stencilWriteMask
			};
		}

		else if(
			info->depthStencil.stencilCompare ||
			info->depthStencil.stencilFail ||
			info->depthStencil.stencilPass ||
			info->depthStencil.stencilDepthFail ||
			info->depthStencil.stencilReadMask ||
			info->depthStencil.stencilWriteMask
		)
			_gotoIfError(clean, Error_unsupportedOperation(6));

		if(info->depthStencil.depthCompare && !(info->depthStencil.flags & EDepthStencilFlags_depthTest))
			_gotoIfError(clean, Error_unsupportedOperation(7));

		if(info->depthStencil.flags) {

			VkCompareOp depthCompareOp = 0;
			_gotoIfError(clean, mapVkCompareOp(info->depthStencil.depthCompare, &depthCompareOp));

			VkPipelineDepthStencilStateCreateInfo *infoi = 
				&((const VkPipelineDepthStencilStateCreateInfo*)states[EPipelineStateType_DepthStencil].ptr)[i];

			*infoi = (VkPipelineDepthStencilStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = info->depthStencil.flags & EDepthStencilFlags_depthTest,
				.depthWriteEnable = info->depthStencil.flags & EDepthStencilFlags_depthWriteBit,
				.depthCompareOp = depthCompareOp,
				.stencilTestEnable = info->depthStencil.flags & EDepthStencilFlags_stencilTest,
				.front = stencil,
				.back = stencil,
				.minDepthBounds = 1,
				.maxDepthBounds = 0
			};

			currentInfo->pDepthStencilState = infoi;
		}

		//Blend state

		if(
			info->blendState.logicOpExt || 
			info->blendState.renderTargetMask || 
			info->blendState.renderTargetsCount ||
			info->blendState.allowIndependentBlend
		) {

			if(info->blendState.logicOpExt && !(device->info.capabilities.features & EGraphicsFeatures_LogicOp))
				_gotoIfError(clean, Error_unsupportedOperation(8));

			if(info->attachmentCountExt && info->attachmentCountExt != info->blendState.renderTargetsCount)
				_gotoIfError(clean, Error_invalidOperation(3));

			//TODO: Validate render target count with renderPass

			//Validate if attachments are the same if independent blending isn't enabled.

			if(!info->blendState.allowIndependentBlend) {

				Bool hadFirst = false;
				BlendStateAttachment first = { 0 };

				//Blend is either all the way off or all the way on.

				if(
					info->blendState.renderTargetMask && 
					info->blendState.renderTargetMask != (U8)(((U16)1 << info->blendState.renderTargetsCount) - 1)
				)
					_gotoIfError(clean, Error_invalidOperation(5));

				for(U64 i = 0; i < 8; ++i)
					if ((info->blendState.renderTargetMask >> i) & 1) {

						if(!hadFirst)
							first = info->blendState.attachments[i];

						//Ensure blend state is the same.

						else if(*(const U64*)&first != *(const U64*)&info->blendState.attachments[i])
							_gotoIfError(clean, Error_invalidOperation(6));
					}
			}

			//Find attachments

			VkPipelineColorBlendAttachmentState *attachments = 
				&((VkPipelineColorBlendAttachmentState*)states[EPipelineStateType_BlendAttachment].ptr)[
					counts[EPipelineStateType_BlendAttachment]
				];

			Bool dualSrc = device->info.capabilities.features & EGraphicsFeatures_DualSrcBlend;

			for(U64 j = 0; j < info->blendState.renderTargetsCount; ++j) {

				BlendStateAttachment attachment = info->blendState.attachments[j];

				attachments[j] = (VkPipelineColorBlendAttachmentState) {
					.blendEnable = (info->blendState.renderTargetMask >> j) & 1,
					.colorWriteMask = (VkColorComponentFlags) attachment.writeMask
				};

				_gotoIfError(clean, mapVkBlendOp(attachment.blendOp, &attachments[j].colorBlendOp));
				_gotoIfError(clean, mapVkBlendOp(attachment.blendOpAlpha, &attachments[j].alphaBlendOp));

				_gotoIfError(clean, mapVkBlend(dualSrc, attachment.srcBlend, &attachments[j].srcColorBlendFactor));
				_gotoIfError(clean, mapVkBlend(dualSrc, attachment.dstBlend, &attachments[j].dstColorBlendFactor));

				_gotoIfError(clean, mapVkBlend(dualSrc, attachment.srcBlendAlpha, &attachments[j].srcAlphaBlendFactor));
				_gotoIfError(clean, mapVkBlend(dualSrc, attachment.dstBlendAlpha, &attachments[j].dstAlphaBlendFactor));
			}

			//Turn into blend state

			VkLogicOp logicOp = 0;

			switch (info->blendState.logicOpExt) {
				case ELogicOpExt_off:			break;
				case ELogicOpExt_clear:			logicOp = VK_LOGIC_OP_CLEAR;			break;
				case ELogicOpExt_set:			logicOp = VK_LOGIC_OP_SET;			break;
				case ELogicOpExt_copy:			logicOp = VK_LOGIC_OP_COPY;			break;
				case ELogicOpExt_copyInvert:	logicOp = VK_LOGIC_OP_COPY_INVERTED;	break;
				case ELogicOpExt_none:			logicOp = VK_LOGIC_OP_NO_OP;			break;
				case ELogicOpExt_invert:		logicOp = VK_LOGIC_OP_INVERT;			break;
				case ELogicOpExt_and:			logicOp = VK_LOGIC_OP_AND;			break;
				case ELogicOpExt_nand:			logicOp = VK_LOGIC_OP_NAND;			break;
				case ELogicOpExt_or:			logicOp = VK_LOGIC_OP_OR;				break;
				case ELogicOpExt_nor:			logicOp = VK_LOGIC_OP_NOR;			break;
				case ELogicOpExt_xor:			logicOp = VK_LOGIC_OP_XOR;			break;
				case ELogicOpExt_equiv:			logicOp = VK_LOGIC_OP_EQUIVALENT;		break;
				case ELogicOpExt_andReverse:	logicOp = VK_LOGIC_OP_AND_REVERSE;	break;
				case ELogicOpExt_andInvert:		logicOp = VK_LOGIC_OP_AND_INVERTED;	break;
				case ELogicOpExt_orReverse:		logicOp = VK_LOGIC_OP_OR_REVERSE;		break;
				case ELogicOpExt_orInvert:		logicOp = VK_LOGIC_OP_OR_INVERTED;	break;
				default:
					_gotoIfError(clean, Error_invalidOperation(1));
			}

			VkPipelineColorBlendStateCreateInfo *infoi = 
				&((const VkPipelineColorBlendStateCreateInfo*)states[EPipelineStateType_BlendState].ptr)[i];

			*infoi = (VkPipelineColorBlendStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.logicOpEnable = info->blendState.logicOpExt != ELogicOpExt_off,
				.logicOp = logicOp,
				.attachmentCount = info->blendState.renderTargetsCount,
				.pAttachments = attachments
			};

			currentInfo->pDepthStencilState = infoi;
		}

		//Tessellation

		if (info->patchControlPointsExt) {

			VkPipelineTessellationStateCreateInfo *infoi = 
				&((const VkPipelineTessellationStateCreateInfo*)states[EPipelineStateType_Tessellation].ptr)[i];

			*infoi = (VkPipelineTessellationStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
				.patchControlPoints = info->patchControlPointsExt
			};

			currentInfo->pTessellationState = infoi;
		}

		//MSAA

		if (info->msaa) {

			VkPipelineMultisampleStateCreateInfo *infoi = 
				&((const VkPipelineMultisampleStateCreateInfo*)states[EPipelineStateType_MSAA].ptr)[i];

			*infoi = (VkPipelineMultisampleStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = (VkSampleCountFlagBits)(1 << info->msaa)
			};

			currentInfo->pMultisampleState = infoi;
		}

		//Direct rendering

		if (!info->renderPass) {

			VkFormat stencilFormat = 0, depthFormat = 0;

			switch (info->depthFormatExt) {

				case EDepthStencilFormat_none:	break;
				case EDepthStencilFormat_D16:	depthFormat = VK_FORMAT_D16_UNORM;		break;
				case EDepthStencilFormat_D32:	depthFormat = VK_FORMAT_D32_SFLOAT;		break;

				case EDepthStencilFormat_D24S8:	
					depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
					stencilFormat = VK_FORMAT_S8_UINT;
					break;

				case EDepthStencilFormat_D32S8:	
					depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
					stencilFormat = VK_FORMAT_S8_UINT;
					break;
			}

			VkFormat *formats = 
				&((VkFormat*)states[EPipelineStateType_DirectRenderingAttachments].ptr)[
					counts[EPipelineStateType_DirectRenderingAttachments]
				];

			for(U64 j = 0; j < info->attachmentCountExt; ++j)
				_gotoIfError(clean, mapVkFormat(NULL, info->attachmentFormatsExt[j], &formats[j], true, true));

			VkPipelineRenderingCreateInfo *infoi = 
				&((VkPipelineRenderingCreateInfo*)states[EPipelineStateType_DirectRendering].ptr)[i];

			*infoi = (VkPipelineRenderingCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = info->attachmentCountExt,
				.pColorAttachmentFormats = formats,
				.depthAttachmentFormat = depthFormat,
				.stencilAttachmentFormat = stencilFormat
			};

			currentInfo->pNext = infoi;
		}

		//Stages

		//TODO:
		//List_createEmpty(sizeof(VkPipelineShaderStageCreateInfo))
		//const VkPipelineShaderStageCreateInfo*         pStages;

		/*.stage = (VkPipelineShaderStageCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.pName = "main"
		},*/

		_gotoIfError(clean, createShaderModule(
			((Buffer*)shaderBinaries->ptr)[i], 
			&((VkPipelineShaderStageCreateInfo*) pipelineInfos.ptr)[i].stage.module, 
			deviceExt
		));

		//Continue to next stages

		counts[EPipelineStateType_Stage] += info->stageCount;

		counts[EPipelineStateType_BlendAttachment] += info->attachmentCountExt;					//TODO: !renderPass
		counts[EPipelineStateType_DirectRenderingAttachments] += info->attachmentCountExt;		//TODO: !renderPass
	}

	_gotoIfError(clean, vkCheck(vkCreateGraphicsPipelines(
		deviceExt->device,
		NULL,
		(U32) infos->length,
		(const VkGraphicsPipelineCreateInfo*) states[EPipelineStateType_PipelineCreateInfo].ptr,
		NULL,
		(VkPipeline*) states[EPipelineStateType_VkPipeline].ptr
	)));

	//Create RefPtrs for OxC3 usage.

	counts[EPipelineStateType_Stage] = 0;

	for (U64 i = 0; i < pipelines->length; ++i) {

		PipelineGraphicsInfo *info = &((const PipelineGraphicsInfo*) infos->ptr)[i];
		RefPtr **refPtr = &((RefPtr**)pipelines->ptr)[i];

		_gotoIfError(clean, RefPtr_createx(
			(U32)(sizeof(Pipeline) + sizeof(VkPipeline) + sizeof(PipelineGraphicsInfo)), 
			(ObjectFreeFunc) Pipeline_free, 
			EGraphicsTypeId_Pipeline, 
			refPtr
		));

		Pipeline *pipeline = PipelineRef_ptr(*refPtr);

		RefPtr_inc(deviceRef);

		*pipeline = (Pipeline) {
			.device = deviceRef,
			.type = EPipelineType_Graphics,
			.stages = List_createEmpty(sizeof(PipelineStage))
		};

		_gotoIfError(clean, List_resizex(&pipeline->stages, info->stageCount));

		List tempList = (List) { 0 };
		_gotoIfError(clean, List_createSubset(*stages, counts[EPipelineStateType_Stage], info->stageCount, &tempList));

		Buffer_copy(List_buffer(pipeline->stages), List_bufferConst(tempList));

		for(U64 k = counts[EPipelineStateType_Stage], j = k; j < k + info->stageCount; ++j)
			((PipelineStage*)stages->ptr)[j].shaderBinary = Buffer_createNull();			//Moved

		VkPipeline *pipelineExt = Pipeline_ext(pipeline, Vk);
		*pipelineExt = ((VkPipeline*) states[EPipelineStateType_VkPipeline].ptr)[i];
		*(PipelineGraphicsInfo*)(pipelineExt + 1) = ((const PipelineGraphicsInfo*) infos->ptr)[i];
		pipeline->extraInfo = pipelineExt + 1;

		counts[EPipelineStateType_Stage] += info->stageCount;
	}

	List_freex(stages);
	List_freex(infos);
	goto success;

clean:

	for (U64 i = 0; i < pipelines->length; ++i)
		RefPtr_dec(&((RefPtr**)pipelines->ptr)[i]);

	List_freex(pipelines);

success:

	for (U64 i = 0; i < infos->length; ++i) {

		VkGraphicsPipelineCreateInfo *graphicsInfo = 
			&((VkGraphicsPipelineCreateInfo*) states[EPipelineStateType_PipelineCreateInfo].ptr)[i];

		for(U64 j = 0; j < graphicsInfo->stageCount; ++j) {

			VkShaderModule mod = graphicsInfo->pStages[j].module;

			if(mod)
				vkDestroyShaderModule(deviceExt->device, mod, NULL);
		}
	}

	for(U64 i = 0; i < sizeof(states) / sizeof(states[0]); ++i)
		List_freex(&states[i]);

	return Error_none();
}
