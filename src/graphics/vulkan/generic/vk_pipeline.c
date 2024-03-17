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

	(void)instance;
	(void)stage;

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

	if(CharString_length(name)) {

		#ifndef NDEBUG

			if(instance->debugSetName && CharString_length(name)) {

				Bool isRt = stage >= EPipelineStage_RtStart && stage <= EPipelineStage_RtEnd;

				gotoIfError(clean, CharString_formatx(
					&temp, "Shader module (\"%.*s\": %s)",
					CharString_length(name), name.ptr, isRt ? "Raytracing" : EPipelineStage_names[stage]
				));

				VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_SHADER_MODULE,
					.objectHandle = (U64) *mod,
					.pObjectName = temp.ptr
				};

				gotoIfError(clean, vkCheck(instance->debugSetName(device->device, &debugName2)));
			}

		#endif
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

	GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	vkDestroyPipeline(deviceExt->device, *Pipeline_ext(pipeline, Vk), NULL);
	return true;
}

TList(VkComputePipelineCreateInfo);
TList(VkPipeline);
TListImpl(VkComputePipelineCreateInfo);
TListImpl(VkPipeline);

Error GraphicsDevice_createPipelinesComputeExt(GraphicsDevice *device, ListCharString names, ListPipelineRef *pipelines) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	ListVkComputePipelineCreateInfo pipelineInfos = (ListVkComputePipelineCreateInfo) { 0 };
	ListVkPipeline pipelineHandles = (ListVkPipeline) { 0 };

	Error err = Error_none();
	gotoIfError(clean, ListVkComputePipelineCreateInfo_resizex(&pipelineInfos, pipelines->length));
	gotoIfError(clean, ListVkPipeline_resizex(&pipelineHandles, pipelines->length));

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

		Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[i]);

		gotoIfError(clean, createShaderModule(
			pipeline->stages.ptr[0].binary,
			&pipelineInfos.ptrNonConst[i].stage.module,
			deviceExt,
			instanceExt,
			!names.length ? CharString_createNull() : names.ptr[i],
			EPipelineStage_Compute
		));
	}

	gotoIfError(clean, vkCheck(vkCreateComputePipelines(
		deviceExt->device,
		NULL,
		(U32) pipelineInfos.length,
		pipelineInfos.ptr,
		NULL,
		pipelineHandles.ptrNonConst
	)));

	for (U64 i = 0; i < pipelines->length; ++i) {

		#ifndef NDEBUG

			if(instanceExt->debugSetName && names.length) {

				VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_PIPELINE,
					.objectHandle = (U64) pipelineHandles.ptr[i],
					.pObjectName = names.ptr[i].ptr
				};

				gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));
			}

		#endif

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

		VkShaderModule mod = pipelineInfos.ptr[i].stage.module;

		if(mod)
			vkDestroyShaderModule(deviceExt->device, mod, NULL);
	}

	ListVkComputePipelineCreateInfo_freex(&pipelineInfos);
	return err;
}

VkCompareOp mapVkCompareOp(ECompareOp op) {
	switch (op) {
		case ECompareOp_Gt:						return VK_COMPARE_OP_GREATER;
		case ECompareOp_Geq:					return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case ECompareOp_Eq:						return VK_COMPARE_OP_EQUAL;
		case ECompareOp_Neq:					return VK_COMPARE_OP_NOT_EQUAL;
		case ECompareOp_Leq:					return VK_COMPARE_OP_LESS_OR_EQUAL;
		case ECompareOp_Lt:						return VK_COMPARE_OP_LESS;
		case ECompareOp_Always:					return VK_COMPARE_OP_ALWAYS;
		default:								return VK_COMPARE_OP_NEVER;
	}
}

VkStencilOp mapVkStencilOp(EStencilOp op) {
	switch (op) {
		default:								return VK_STENCIL_OP_KEEP;
		case EStencilOp_Zero:					return VK_STENCIL_OP_ZERO;
		case EStencilOp_Replace:				return VK_STENCIL_OP_REPLACE;
		case EStencilOp_IncClamp:				return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case EStencilOp_DecClamp:				return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case EStencilOp_Invert:					return VK_STENCIL_OP_INVERT;
		case EStencilOp_IncWrap:				return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case EStencilOp_DecWrap:				return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	}
}

VkBlendOp mapVkBlendOp(EBlendOp op) {
	switch (op) {
		default:								return VK_BLEND_OP_ADD;
		case EBlendOp_Subtract:					return VK_BLEND_OP_SUBTRACT;
		case EBlendOp_ReverseSubtract:			return VK_BLEND_OP_REVERSE_SUBTRACT;
		case EBlendOp_Min:						return VK_BLEND_OP_MIN;
		case EBlendOp_Max:						return VK_BLEND_OP_MAX;
	}
}

VkBlendFactor mapVkBlend(EBlend op) {

	switch (op) {

		default:								return VK_BLEND_FACTOR_ZERO;
		case EBlend_One:						return VK_BLEND_FACTOR_ONE;
		case EBlend_SrcColor:					return VK_BLEND_FACTOR_SRC_COLOR;
		case EBlend_InvSrcColor:				return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case EBlend_DstColor:					return VK_BLEND_FACTOR_DST_COLOR;
		case EBlend_InvDstColor:				return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case EBlend_SrcAlpha:					return VK_BLEND_FACTOR_SRC_ALPHA;
		case EBlend_InvSrcAlpha:				return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case EBlend_DstAlpha:					return VK_BLEND_FACTOR_DST_ALPHA;
		case EBlend_InvDstAlpha:				return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case EBlend_BlendFactor:				return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case EBlend_InvBlendFactor:				return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case EBlend_AlphaFactor:				return VK_BLEND_FACTOR_CONSTANT_ALPHA;
		case EBlend_InvAlphaFactor:				return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		case EBlend_SrcAlphaSat:				return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;

		case EBlend_Src1ColorExt:				return VK_BLEND_FACTOR_ZERO;
		case EBlend_Src1AlphaExt:				return VK_BLEND_FACTOR_ZERO;
		case EBlend_InvSrc1ColorExt:			return VK_BLEND_FACTOR_ZERO;
		case EBlend_InvSrc1AlphaExt:			return VK_BLEND_FACTOR_ZERO;
	}
}

Error GraphicsDevice_createPipelinesGraphicsExt(GraphicsDevice *device, ListCharString names, ListPipelineRef *pipelines) {

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

	EPipelineStateType type;
	(void)type;

	U64 counts[EPipelineStateType_Count] = { 0 };
	Error err = Error_none();
	U64 total = 0;

	counts[EPipelineStateType_PipelineCreateInfo] = pipelines->length;
	counts[EPipelineStateType_VkPipeline] = pipelines->length;

	for(U64 i = 0; i < pipelines->length; ++i) {

		const PipelineGraphicsInfo *info = Pipeline_info(PipelineRef_ptr(pipelines->ptr[i]), PipelineGraphicsInfo);

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

	GenericList states[EPipelineStateType_Count] = { 0 };

	for(U64 i = EPipelineStateType_PerPipelineStart; i < EPipelineStateType_Count; ++i) {
		states[i] = GenericList_createEmpty(strides[i]);
		gotoIfError(clean, GenericList_resizex(&states[i], counts[i]));
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

	for(U64 i = EPipelineStateType_MSAA; i < EPipelineStateType_Count; ++i)
		counts[i] = 0;

	for(; total < pipelines->length; ++total) {

		const PipelineGraphicsInfo *info = Pipeline_info(PipelineRef_ptr(pipelines->ptr[total]), PipelineGraphicsInfo);

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
				&((VkPipelineVertexInputStateCreateInfo*)states[EPipelineStateType_VertexInput].ptr)[
					counts[EPipelineStateType_VertexInput]++
				];

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
				&((VkPipelineInputAssemblyStateCreateInfo*)states[EPipelineStateType_InputAssembly].ptr)[
					counts[EPipelineStateType_InputAssembly]++
				];

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
				&((VkPipelineRasterizationStateCreateInfo*)states[EPipelineStateType_Rasterizer].ptr)[
					counts[EPipelineStateType_Rasterizer]++
				];

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
				&((VkPipelineDepthStencilStateCreateInfo*)states[EPipelineStateType_DepthStencil].ptr)[
					counts[EPipelineStateType_DepthStencil]++
				];

			*infoi = (VkPipelineDepthStencilStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = (Bool)(info->depthStencil.flags & EDepthStencilFlags_DepthTest),
				.depthWriteEnable = (Bool)(info->depthStencil.flags & EDepthStencilFlags_DepthWriteBit),
				.depthCompareOp = depthCompareOp,
				.stencilTestEnable = (Bool)(info->depthStencil.flags & EDepthStencilFlags_StencilTest),
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
				&((VkPipelineTessellationStateCreateInfo*)states[EPipelineStateType_Tessellation].ptr)[
					counts[EPipelineStateType_Tessellation]++
				];

			*infoi = (VkPipelineTessellationStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
				.patchControlPoints = info->patchControlPointsExt
			};

			currentInfo->pTessellationState = infoi;
		}

		//MSAA

		if (info->msaa) {

			VkPipelineMultisampleStateCreateInfo *infoi =
				&((VkPipelineMultisampleStateCreateInfo*)states[EPipelineStateType_MSAA].ptr)[
					counts[EPipelineStateType_MSAA]++
				];

			*infoi = (VkPipelineMultisampleStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = (VkSampleCountFlagBits)(1 << info->msaa),
				.sampleShadingEnable = info->msaaMinSampleShading > 0,
				.minSampleShading = info->msaaMinSampleShading
			};

			currentInfo->pMultisampleState = infoi;
		}

		//Direct rendering

		if (!info->renderPass) {

			VkFormat stencilFormat = 0, depthFormat = 0;

			switch (info->depthFormatExt) {

				case EDepthStencilFormat_D16:
					depthFormat = VK_FORMAT_D16_UNORM;
					break;

				case EDepthStencilFormat_D32:
					depthFormat = VK_FORMAT_D32_SFLOAT;
					break;

				case EDepthStencilFormat_D24S8Ext:
					depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
					stencilFormat = VK_FORMAT_S8_UINT;
					break;

				case EDepthStencilFormat_D32S8:
					depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
					stencilFormat = VK_FORMAT_S8_UINT;
					break;

				case EDepthStencilFormat_S8Ext:
					stencilFormat = VK_FORMAT_S8_UINT;
					break;

				default:
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

		Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[total]);

		for(U64 j = 0; j < info->stageCount; ++j) {

			VkShaderStageFlagBits stageBit = 0;
			PipelineStage stage = pipeline->stages.ptr[j];

			switch (stage.stageType) {
				default:							stageBit = VK_SHADER_STAGE_VERTEX_BIT;						break;
				case EPipelineStage_Pixel:			stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;					break;
				case EPipelineStage_GeometryExt:	stageBit = VK_SHADER_STAGE_GEOMETRY_BIT;					break;
				case EPipelineStage_Hull:			stageBit = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;		break;
				case EPipelineStage_Domain:			stageBit = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;		break;
			}

			VkShaderModule module = NULL;

			gotoIfError(clean, createShaderModule(
				stage.binary,
				&module,
				deviceExt,
				instanceExt,
				!names.length ? CharString_createNull() : names.ptr[total],
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

	gotoIfError(clean, vkCheck(vkCreateGraphicsPipelines(
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
					.pObjectName = names.ptr[i].ptr
				};

				gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));
			}

		#endif

		VkPipeline *pipelineExt = Pipeline_ext(PipelineRef_ptr(pipelines->ptr[i]), Vk);
		*pipelineExt = ((VkPipeline*) states[EPipelineStateType_VkPipeline].ptr)[i];
		((VkPipeline*) states[EPipelineStateType_VkPipeline].ptr)[i] = NULL;
	}

	GenericList_freex(&states[EPipelineStateType_VkPipeline]);

clean:

	for(U64 i = 0; i < states[EPipelineStateType_VkPipeline].length; ++i)
		if(((VkPipeline*)states[EPipelineStateType_VkPipeline].ptr)[i])
			vkDestroyPipeline(deviceExt->device, ((VkPipeline*)states[EPipelineStateType_VkPipeline].ptr)[i], NULL);

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
		GenericList_freex(&states[i]);

	return err;
}

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

	gotoIfError(clean, ListVkRayTracingPipelineCreateInfoKHR_resizex(&createInfos, pipelines->length));
	gotoIfError(clean, ListVkPipeline_resizex(&pipelinesExt, pipelines->length));
	gotoIfError(clean, ListVkShaderModule_resizex(&modules, binaryCounter));
	gotoIfError(clean, ListVkPipelineShaderStageCreateInfo_resizex(&stages, stageCounter));
	gotoIfError(clean, GenericList_resizex(&shaderHandles, stageCounter));
	gotoIfError(clean, Buffer_createEmptyBytesx(stageCounter * raytracingShaderAlignment, &shaderBindings));
	gotoIfError(clean, ListVkRayTracingShaderGroupCreateInfoKHR_resizex(&groups, groupCounter + stageCounter));

	//Convert

	binaryCounter = stageCounter = groupCounter = 0;

	for(U64 i = 0; i < pipelines->length; ++i) {

		Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[i]);
		PipelineRaytracingInfo *rtPipeline = Pipeline_info(pipeline, PipelineRaytracingInfo);

		CharString name = CharString_createNull();

		if (names.length) {

			if(!CharString_isNullTerminated(names.ptr[i])) {
				gotoIfError(clean, CharString_createCopyx(names.ptr[i], &temp1));
				name = temp1;
			}

			else name = names.ptr[i];
		}

		//Create binaries

		for(U64 j = 0; j < rtPipeline->binaryCount; ++j) {

			if(names.length)
				gotoIfError(clean, CharString_formatx(&temp, "%s binary %"PRIu64, name.ptr, j));

			gotoIfError(clean, createShaderModule(
				rtPipeline->binaries.ptr[j], &modules.ptrNonConst[binaryCounter + j],
				deviceExt, instanceExt, name, EPipelineStage_RtStart
			));

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
	)));

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

				gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));
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
		)));

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
	));

	for (U64 i = 0; i < pipelines->length; ++i) {
		gotoIfError(clean, DeviceBufferRef_inc(sbt));
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
