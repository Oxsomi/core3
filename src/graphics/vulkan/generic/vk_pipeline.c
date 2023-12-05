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
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "formats/texture.h"
#include "types/buffer.h"
#include "types/string.h"
#include "types/error.h"

const C8 *EPipelineStage_names[] = {
	"vertex",
	"pixel",
	"compute",
	"geometry",
	"hull",
	"domain"
};

Error createShaderModule(
	Buffer buf, 
	VkShaderModule *mod, 
	VkGraphicsDevice *device, 
	VkGraphicsInstance *instance, 
	CharString name, 
	EPipelineStage stage
) {

	instance;
	name;
	stage;

	if(Buffer_length(buf) >> 32)
		return Error_outOfBounds(0, Buffer_length(buf), U32_MAX, "createShaderModule()::buf.length is limited to U32_MAX");

	if(!Buffer_length(buf) || Buffer_length(buf) % sizeof(U32))
		return Error_invalidParameter(0, 0, "createShaderModule()::buf.length must be in U32s when SPIR-V is used");

	VkShaderModuleCreateInfo info = (VkShaderModuleCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (U32) Buffer_length(buf),
		.pCode = (const U32*) buf.ptr
	};

	Error err = vkCheck(vkCreateShaderModule(device->device, &info, NULL, mod));
	CharString temp = CharString_createNull();

	if(err.genericError)
		return err;

	#ifndef NDEBUG

		if(instance->debugSetName && CharString_length(name)) {

			_gotoIfError(clean, CharString_formatx(
				&temp, "Shader module (\"%.*s\": %s)", 
				CharString_length(name), name.ptr, EPipelineStage_names[stage]
			));

			VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_SHADER_MODULE,
				.objectHandle = (U64) *mod,
				.pObjectName = temp.ptr
			};

			_gotoIfError(clean, vkCheck(instance->debugSetName(device->device, &debugName2)));
		}

	#endif

	goto clean;
	
clean:

	CharString_freex(&temp);

	if (err.genericError)
		vkDestroyShaderModule(device->device, *mod, NULL);
	
	return err;
}

const U64 PipelineExt_size = sizeof(VkPipeline);

Bool Pipeline_freeExt(Pipeline *pipeline, Allocator allocator) {

	allocator;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	vkDestroyPipeline(deviceExt->device, *Pipeline_ext(pipeline, Vk), NULL);
	return true;
}

Error GraphicsDevice_createPipelinesComputeExt(GraphicsDevice *device, List names, List *pipelines) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	List pipelineInfos = List_createEmpty(sizeof(VkComputePipelineCreateInfo));
	List pipelineHandles = List_createEmpty(sizeof(VkPipeline));

	Error err = Error_none();
	_gotoIfError(clean, List_resizex(&pipelineInfos, pipelines->length));
	_gotoIfError(clean, List_resizex(&pipelineHandles, pipelines->length));

	//TODO: Push constants

	for(U64 i = 0; i < pipelines->length; ++i) {

		((VkComputePipelineCreateInfo*)pipelineInfos.ptr)[i] = (VkComputePipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = (VkPipelineShaderStageCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
				.pName = "main"
			},
			.layout = deviceExt->defaultLayout
		};

		Pipeline *pipeline = PipelineRef_ptr(((PipelineRef**)pipelines->ptr)[i]);

		_gotoIfError(clean, createShaderModule(
			((const PipelineStage*) pipeline->stages.ptr)[0].shaderBinary, 
			&((VkComputePipelineCreateInfo*) pipelineInfos.ptr)[i].stage.module, 
			deviceExt,
			instanceExt,
			!names.length ? CharString_createNull() : ((const CharString*)names.ptr)[i],
			EPipelineStage_Compute
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

		#ifndef NDEBUG

			if(instanceExt->debugSetName && names.length) {

				VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_PIPELINE,
					.objectHandle = (U64) ((const VkPipeline*) pipelineHandles.ptr)[i],
					.pObjectName = ((const CharString*)names.ptr)[i].ptr
				};

				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));
			}

		#endif

		Pipeline *pipeline = PipelineRef_ptr(((PipelineRef**)pipelines->ptr)[i]);
		*Pipeline_ext(pipeline, Vk) = ((const VkPipeline*) pipelineHandles.ptr)[i];
	}

clean:

	List_freex(&pipelineHandles);

	for (U64 i = 0; i < pipelineInfos.length; ++i) {

		VkShaderModule mod = ((VkComputePipelineCreateInfo*) pipelineInfos.ptr)[i].stage.module;

		if(mod)
			vkDestroyShaderModule(deviceExt->device, mod, NULL);
	}

	List_freex(&pipelineInfos);
	return err;
}

VkCompareOp mapVkCompareOp(ECompareOp op) {
	switch (op) {
		case ECompareOp_Gt:			return VK_COMPARE_OP_GREATER;
		case ECompareOp_Geq:		return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case ECompareOp_Eq:			return VK_COMPARE_OP_EQUAL;
		case ECompareOp_Neq:		return VK_COMPARE_OP_NOT_EQUAL;
		case ECompareOp_Leq:		return VK_COMPARE_OP_LESS_OR_EQUAL;
		case ECompareOp_Lt:			return VK_COMPARE_OP_LESS;
		case ECompareOp_Always:		return VK_COMPARE_OP_ALWAYS;
		default:					return VK_COMPARE_OP_NEVER;
	}
}

VkStencilOp mapVkStencilOp(EStencilOp op) {
	switch (op) {
		default:					return VK_STENCIL_OP_KEEP;
		case EStencilOp_Zero:		return VK_STENCIL_OP_ZERO;
		case EStencilOp_Replace:	return VK_STENCIL_OP_REPLACE;
		case EStencilOp_IncClamp:	return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case EStencilOp_DecClamp:	return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case EStencilOp_Invert:		return VK_STENCIL_OP_INVERT;
		case EStencilOp_IncWrap:	return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case EStencilOp_DecWrap:	return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	}
}

VkBlendOp mapVkBlendOp(EBlendOp op) {
	switch (op) {
		default:							return VK_BLEND_OP_ADD;
		case EBlendOp_Subtract:				return VK_BLEND_OP_SUBTRACT;
		case EBlendOp_ReverseSubtract:		return VK_BLEND_OP_REVERSE_SUBTRACT;
		case EBlendOp_Min:					return VK_BLEND_OP_MIN;
		case EBlendOp_Max:					return VK_BLEND_OP_MAX;
	}
}

VkBlendFactor mapVkBlend(EBlend op) {

	switch (op) {

		default:						return VK_BLEND_FACTOR_ZERO;
		case EBlend_One:				return VK_BLEND_FACTOR_ONE;
		case EBlend_SrcColor:			return VK_BLEND_FACTOR_SRC_COLOR;
		case EBlend_InvSrcColor:		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case EBlend_DstColor:			return VK_BLEND_FACTOR_DST_COLOR;
		case EBlend_InvDstColor:		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case EBlend_SrcAlpha:			return VK_BLEND_FACTOR_SRC_ALPHA;
		case EBlend_InvSrcAlpha:		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case EBlend_DstAlpha:			return VK_BLEND_FACTOR_DST_ALPHA;
		case EBlend_InvDstAlpha:		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case EBlend_BlendFactor:		return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case EBlend_InvBlendFactor:		return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case EBlend_AlphaFactor:		return VK_BLEND_FACTOR_CONSTANT_ALPHA;
		case EBlend_InvAlphaFactor:		return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		case EBlend_SrcAlphaSat:		return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;

		case EBlend_Src1ColorExt:		return VK_BLEND_FACTOR_ZERO;
		case EBlend_Src1AlphaExt:		return VK_BLEND_FACTOR_ZERO;
		case EBlend_InvSrc1ColorExt:	return VK_BLEND_FACTOR_ZERO;
		case EBlend_InvSrc1AlphaExt:	return VK_BLEND_FACTOR_ZERO;
	}
}

Error GraphicsDevice_createPipelinesGraphicsExt(GraphicsDevice *device, List names, List *pipelines) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

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
	U64 total = 0;

	counts[EPipelineStateType_PipelineCreateInfo] = pipelines->length;
	counts[EPipelineStateType_VkPipeline] = pipelines->length;

	for(U64 i = 0; i < pipelines->length; ++i) {

		const PipelineGraphicsInfo *info = 
			(const PipelineGraphicsInfo*) PipelineRef_ptr(((const PipelineRef**)pipelines->ptr)[i])->extraInfo;

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

		++counts[EPipelineStateType_BlendState];
		counts[EPipelineStateType_BlendAttachment] += info->attachmentCountExt;		//TODO: if renderPass

		if(info->attachmentCountExt || info->depthFormatExt)
			++counts[EPipelineStateType_DirectRendering];

		//Count variable

		counts[EPipelineStateType_Stage] += info->stageCount;

		if(info->attachmentCountExt)
			counts[EPipelineStateType_DirectRenderingAttachments] += info->attachmentCountExt;

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
	}

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
		.maxDepthBounds = 0,
		.depthCompareOp = VK_COMPARE_OP_ALWAYS
	};

	VkPipelineRasterizationStateCreateInfo rasterState = (VkPipelineRasterizationStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.lineWidth = 1
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = (VkPipelineInputAssemblyStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	VkPipelineVertexInputStateCreateInfo vertexInput = (VkPipelineVertexInputStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	for(U64 i = EPipelineStateType_PerPipelinePropertyStart; i < EPipelineStateType_Count; ++i)
		counts[i] = 0;

	for(; total < pipelines->length; ++total) {

		const PipelineGraphicsInfo *info =
			(const PipelineGraphicsInfo*) PipelineRef_ptr(((const PipelineRef**)pipelines->ptr)[total])->extraInfo;

		//Convert info struct to vulkan struct

		VkGraphicsPipelineCreateInfo *currentInfo = 
			&((VkGraphicsPipelineCreateInfo*)states[EPipelineStateType_PipelineCreateInfo].ptr)[total];

		*currentInfo = (VkGraphicsPipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			//.pStages = stages,
			.pVertexInputState = &vertexInput,
			.pInputAssemblyState = &inputAssembly,
			.pTessellationState = &tessState,
			.pViewportState = &viewport,
			.pRasterizationState = &rasterState,
			.pMultisampleState = &msaaState,
			.pDepthStencilState = &depthStencilState,
			//.pColorBlendState = &blendState,
			.pDynamicState = &dynamicState,
			.layout = deviceExt->defaultLayout
		};

		//Vertex input

		Bool hasAttrib = false;

		for(U32 j = 0; j < 16; ++j)
			if(info->vertexLayout.attributes[j].format) {
				hasAttrib = true;
				break;
			}

		//Transform into description

		if (hasAttrib) {

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

				VertexAttribute attrib = info->vertexLayout.attributes[j];

				if(!attrib.format)
					continue;

				VkFormat formatExt =  mapVkFormat(ETextureFormatId_unpack[attrib.format]);

				attributes[attribCount++] = (VkVertexInputAttributeDescription) {
					.location = j,
					.binding = attrib.bufferId4 & 0xF,
					.format = formatExt,
					.offset = attrib.offset11 & 2047
				};
			}

			counts[EPipelineStateType_InputAttributes] += attribCount;

			VkPipelineVertexInputStateCreateInfo *infoi = 
				&((VkPipelineVertexInputStateCreateInfo*)states[EPipelineStateType_VertexInput].ptr)[total];

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
				
				default:								topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;		break;
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
			}

			VkPipelineInputAssemblyStateCreateInfo *infoi = 
				&((VkPipelineInputAssemblyStateCreateInfo*)states[EPipelineStateType_InputAssembly].ptr)[total];

			*infoi = (VkPipelineInputAssemblyStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = topology
			};

			currentInfo->pInputAssemblyState = infoi;
		}

		//Rasterizer

		if(info->rasterizer.flags || info->rasterizer.cullMode) {

			VkCullModeFlags cullMode = 0;

			switch (info->rasterizer.cullMode) {

				case ECullMode_Back:	cullMode = VK_CULL_MODE_BACK_BIT;	break;
				case ECullMode_Front:	cullMode = VK_CULL_MODE_FRONT_BIT;	break;
				default:				cullMode = VK_CULL_MODE_NONE;		break;
			}

			VkFrontFace windingOrder = 
				info->rasterizer.flags & ERasterizerFlags_IsClockWise ? VK_FRONT_FACE_CLOCKWISE : 
				VK_FRONT_FACE_COUNTER_CLOCKWISE;

			VkPolygonMode polygonMode = 
				info->rasterizer.flags & ERasterizerFlags_IsWireframeExt ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
			
			VkPipelineRasterizationStateCreateInfo *infoi = 
				&((VkPipelineRasterizationStateCreateInfo*)states[EPipelineStateType_Rasterizer].ptr)[total];

			*infoi = (VkPipelineRasterizationStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.cullMode = cullMode,
				.frontFace = windingOrder,
				.depthClampEnable = info->rasterizer.flags & ERasterizerFlags_EnableDepthClamp,
				.polygonMode = polygonMode,
				.depthBiasEnable = info->rasterizer.flags & ERasterizerFlags_EnableDepthBias,
				.depthBiasConstantFactor = info->rasterizer.depthBiasConstantFactor,
				.depthBiasClamp = info->rasterizer.depthBiasClamp,
				.depthBiasSlopeFactor = info->rasterizer.depthBiasSlopeFactor,
				.lineWidth = 1
			};

			currentInfo->pRasterizationState = infoi;
		}

		//Depth stencil

		VkStencilOpState stencil = (VkStencilOpState) { 0 };

		if (info->depthStencil.flags & EDepthStencilFlags_StencilTest) {

			VkCompareOp stencilCompareOp = mapVkCompareOp(info->depthStencil.stencilCompare);

			VkStencilOp failOp = mapVkStencilOp(info->depthStencil.stencilFail);
			VkStencilOp passOp = mapVkStencilOp(info->depthStencil.stencilPass);
			VkStencilOp depthFailOp = mapVkStencilOp(info->depthStencil.stencilDepthFail);

			stencil = (VkStencilOpState) {
				.failOp = failOp,
				.passOp = passOp,
				.depthFailOp = depthFailOp,
				.compareOp = stencilCompareOp,
				.compareMask = info->depthStencil.stencilReadMask,
				.writeMask = info->depthStencil.stencilWriteMask
			};
		}

		if(info->depthStencil.flags) {

			VkCompareOp depthCompareOp = mapVkCompareOp(info->depthStencil.depthCompare);

			VkPipelineDepthStencilStateCreateInfo *infoi = 
				&((VkPipelineDepthStencilStateCreateInfo*)states[EPipelineStateType_DepthStencil].ptr)[total];

			*infoi = (VkPipelineDepthStencilStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = info->depthStencil.flags & EDepthStencilFlags_DepthTest,
				.depthWriteEnable = info->depthStencil.flags & EDepthStencilFlags_DepthWriteBit,
				.depthCompareOp = depthCompareOp,
				.stencilTestEnable = info->depthStencil.flags & EDepthStencilFlags_StencilTest,
				.front = stencil,
				.back = stencil,
				.minDepthBounds = 1,
				.maxDepthBounds = 0
			};

			currentInfo->pDepthStencilState = infoi;
		}

		//Blend state

		{
			//TODO: Validate render target count with renderPass

			//Find attachments

			VkPipelineColorBlendAttachmentState *attachments = 
				&((VkPipelineColorBlendAttachmentState*)states[EPipelineStateType_BlendAttachment].ptr)[
					counts[EPipelineStateType_BlendAttachment]
				];

			for(U64 j = 0; j < info->attachmentCountExt; ++j) {

				BlendStateAttachment attachment = info->blendState.attachments[j];

				if (!info->blendState.enable) {

					attachments[j] = (VkPipelineColorBlendAttachmentState) {
						.blendEnable = false,
						.colorWriteMask = (VkColorComponentFlags) EWriteMask_All
					};

					continue;
				}

				U64 index = info->blendState.allowIndependentBlend ? j : 0;

				attachments[j] = (VkPipelineColorBlendAttachmentState) {
					.blendEnable = (info->blendState.renderTargetMask >> index) & 1,
					.colorWriteMask = (VkColorComponentFlags) info->blendState.writeMask[index]
				};

				attachments[j].colorBlendOp = mapVkBlendOp(attachment.blendOp);
				attachments[j].alphaBlendOp = mapVkBlendOp(attachment.blendOpAlpha);

				attachments[j].srcColorBlendFactor = mapVkBlend(attachment.srcBlend);
				attachments[j].dstColorBlendFactor = mapVkBlend(attachment.dstBlend);

				attachments[j].srcAlphaBlendFactor = mapVkBlend(attachment.srcBlendAlpha);
				attachments[j].dstAlphaBlendFactor = mapVkBlend(attachment.dstBlendAlpha);
			}

			//Turn into blend state

			VkLogicOp logicOp = 0;

			switch (info->blendState.logicOpExt) {
				default:						break;
				case ELogicOpExt_Clear:			logicOp = VK_LOGIC_OP_CLEAR;			break;
				case ELogicOpExt_Set:			logicOp = VK_LOGIC_OP_SET;				break;
				case ELogicOpExt_Copy:			logicOp = VK_LOGIC_OP_COPY;				break;
				case ELogicOpExt_CopyInvert:	logicOp = VK_LOGIC_OP_COPY_INVERTED;	break;
				case ELogicOpExt_None:			logicOp = VK_LOGIC_OP_NO_OP;			break;
				case ELogicOpExt_Invert:		logicOp = VK_LOGIC_OP_INVERT;			break;
				case ELogicOpExt_And:			logicOp = VK_LOGIC_OP_AND;				break;
				case ELogicOpExt_Nand:			logicOp = VK_LOGIC_OP_NAND;				break;
				case ELogicOpExt_Or:			logicOp = VK_LOGIC_OP_OR;				break;
				case ELogicOpExt_Nor:			logicOp = VK_LOGIC_OP_NOR;				break;
				case ELogicOpExt_Xor:			logicOp = VK_LOGIC_OP_XOR;				break;
				case ELogicOpExt_Equiv:			logicOp = VK_LOGIC_OP_EQUIVALENT;		break;
				case ELogicOpExt_AndReverse:	logicOp = VK_LOGIC_OP_AND_REVERSE;		break;
				case ELogicOpExt_AndInvert:		logicOp = VK_LOGIC_OP_AND_INVERTED;		break;
				case ELogicOpExt_OrReverse:		logicOp = VK_LOGIC_OP_OR_REVERSE;		break;
				case ELogicOpExt_OrInvert:		logicOp = VK_LOGIC_OP_OR_INVERTED;		break;
			}

			VkPipelineColorBlendStateCreateInfo *infoi = 
				&((VkPipelineColorBlendStateCreateInfo*)states[EPipelineStateType_BlendState].ptr)[total];

			*infoi = (VkPipelineColorBlendStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.logicOpEnable = info->blendState.logicOpExt != ELogicOpExt_Off,
				.logicOp = logicOp,
				.attachmentCount = info->attachmentCountExt,
				.pAttachments = attachments
			};

			currentInfo->pColorBlendState = infoi;
		}

		//Tessellation

		if (info->patchControlPointsExt) {

			VkPipelineTessellationStateCreateInfo *infoi = 
				&((VkPipelineTessellationStateCreateInfo*)states[EPipelineStateType_Tessellation].ptr)[total];

			*infoi = (VkPipelineTessellationStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
				.patchControlPoints = info->patchControlPointsExt
			};

			currentInfo->pTessellationState = infoi;
		}

		//MSAA

		if (info->msaa) {

			VkPipelineMultisampleStateCreateInfo *infoi = 
				&((VkPipelineMultisampleStateCreateInfo*)states[EPipelineStateType_MSAA].ptr)[total];

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

				default:						break;
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
				formats[j] = mapVkFormat(ETextureFormatId_unpack[info->attachmentFormatsExt[j]]);

			VkPipelineRenderingCreateInfo *infoi = 
				&((VkPipelineRenderingCreateInfo*)states[EPipelineStateType_DirectRendering].ptr)[total];

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

		VkPipelineShaderStageCreateInfo *vkStages = 
			&((VkPipelineShaderStageCreateInfo*)states[EPipelineStateType_Stage].ptr)[
				counts[EPipelineStateType_Stage]
			];

		Pipeline *pipeline = PipelineRef_ptr(((const PipelineRef**)pipelines->ptr)[total]);

		for(U64 j = 0; j < info->stageCount; ++j) {

			VkShaderStageFlagBits stageBit = 0;
			PipelineStage stage = ((const PipelineStage*) pipeline->stages.ptr)[j];

			switch (stage.stageType) {
				default:							stageBit = VK_SHADER_STAGE_VERTEX_BIT;						break;
				case EPipelineStage_Pixel:			stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;					break;
				case EPipelineStage_GeometryExt:	stageBit = VK_SHADER_STAGE_GEOMETRY_BIT;					break;
				case EPipelineStage_HullExt:		stageBit = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;		break;
				case EPipelineStage_DomainExt:		stageBit = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;		break;
			}

			VkShaderModule module = NULL;

			_gotoIfError(clean, createShaderModule(
				stage.shaderBinary, 
				&module, 
				deviceExt,
				instanceExt,
				!names.length ? CharString_createNull() : ((const CharString*)names.ptr)[total],
				stage.stageType
			));

			vkStages[j] = (VkPipelineShaderStageCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = stageBit,
				.module = module,
				.pName = "main"
			};

			++currentInfo->stageCount;
		}

		currentInfo->pStages = vkStages;

		//Continue to next stages

		counts[EPipelineStateType_Stage] += info->stageCount;
		counts[EPipelineStateType_BlendAttachment] += info->attachmentCountExt;					//TODO: !renderPass
		counts[EPipelineStateType_DirectRenderingAttachments] += info->attachmentCountExt;
	}

	_gotoIfError(clean, vkCheck(vkCreateGraphicsPipelines(
		deviceExt->device,
		NULL,
		(U32) pipelines->length,
		(const VkGraphicsPipelineCreateInfo*) states[EPipelineStateType_PipelineCreateInfo].ptr,
		NULL,
		(VkPipeline*) states[EPipelineStateType_VkPipeline].ptr
	)));

	//Create RefPtrs for OxC3 usage.

	for (U64 i = 0; i < pipelines->length; ++i) {

		#ifndef NDEBUG

			if(instanceExt->debugSetName && names.length) {

				VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_PIPELINE,
					.objectHandle = (U64) ((const VkPipeline*) states[EPipelineStateType_VkPipeline].ptr)[i],
					.pObjectName = ((const CharString*)names.ptr)[i].ptr
				};

				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));
			}

		#endif

		VkPipeline *pipelineExt = Pipeline_ext(PipelineRef_ptr(((const PipelineRef**)pipelines->ptr)[i]), Vk);
		*pipelineExt = ((VkPipeline*) states[EPipelineStateType_VkPipeline].ptr)[i];
	}

clean:

	for (U64 i = 0; i < total; ++i) {

		VkGraphicsPipelineCreateInfo *graphicsInfo = 
			&((VkGraphicsPipelineCreateInfo*) states[EPipelineStateType_PipelineCreateInfo].ptr)[i];

		if(graphicsInfo->pStages)
			for(U64 j = 0; j < graphicsInfo->stageCount; ++j) {

				VkShaderModule mod = graphicsInfo->pStages[j].module;

				if(mod)
					vkDestroyShaderModule(deviceExt->device, mod, NULL);
			}
	}

	for(U64 i = 0; i < sizeof(states) / sizeof(states[0]); ++i)
		List_freex(&states[i]);

	return err;
}
