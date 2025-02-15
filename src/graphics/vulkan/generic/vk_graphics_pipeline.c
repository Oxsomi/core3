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
#include "graphics/generic/interface.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/texture.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/container/texture_format.h"
#include "formats/oiSH/sh_file.h"
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

TList(VkPipelineShaderStageCreateInfo);

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

		case EBlend_Src1ColorExt:				return VK_BLEND_FACTOR_SRC1_COLOR;
		case EBlend_Src1AlphaExt:				return VK_BLEND_FACTOR_SRC1_ALPHA;
		case EBlend_InvSrc1ColorExt:			return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		case EBlend_InvSrc1AlphaExt:			return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
	}
}

Bool VK_WRAP_FUNC(GraphicsDevice_createPipelineGraphics)(
	GraphicsDevice *device,
	ListSHFile binaries,
	CharString name,
	Pipeline *pipeline,
	Error *e_rr
) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);
	VkPipeline pipelineHandle = NULL;
	Bool s_uccess = true;
	CharString temp = CharString_createNull();
	ListVkPipelineShaderStageCreateInfo stages = (ListVkPipelineShaderStageCreateInfo) { 0 };
	VkGraphicsPipelineCreateInfo currentInfo = (VkGraphicsPipelineCreateInfo) { 0 };

	gotoIfError2(clean, ListVkPipelineShaderStageCreateInfo_resizex(&stages, pipeline->stages.length))

	const PipelineGraphicsInfo *info = Pipeline_info(pipeline, PipelineGraphicsInfo);

	//TODO: Validate render target count with renderPass

	//Find attachments

	VkPipelineColorBlendAttachmentState attachments[8];

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

	//Blend state

	VkLogicOp logicOp = 0;

	switch (info->blendState.logicOpExt) {
		default:																break;
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

	VkPipelineColorBlendStateCreateInfo blendState = (VkPipelineColorBlendStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = info->blendState.logicOpExt != ELogicOpExt_Off,
		.logicOp = logicOp,
		.attachmentCount = info->attachmentCountExt,
		.pAttachments = attachments
	};

	//Topology

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

	VkPipelineInputAssemblyStateCreateInfo assembly = (VkPipelineInputAssemblyStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = topology
	};

	VkPipelineTessellationStateCreateInfo tessellation = (VkPipelineTessellationStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.patchControlPoints = info->patchControlPoints
	};

	//Stages

	for(U64 j = 0; j < pipeline->stages.length; ++j) {

		VkShaderStageFlagBits stageBit = 0;
		PipelineStage stage = pipeline->stages.ptr[j];

		switch (stage.stageType) {
			default:							stageBit = VK_SHADER_STAGE_VERTEX_BIT;						break;
			case EPipelineStage_Pixel:			stageBit = VK_SHADER_STAGE_FRAGMENT_BIT;					break;
			case EPipelineStage_GeometryExt:	stageBit = VK_SHADER_STAGE_GEOMETRY_BIT;					break;
			case EPipelineStage_Hull:			stageBit = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;		break;
			case EPipelineStage_Domain:			stageBit = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;		break;
		}

		if(CharString_length(name))
			gotoIfError2(clean, CharString_formatx(&temp, "%.*s binary %"PRIu64, (int) CharString_length(name), name.ptr, j))

		U16 entrypointId = (U16) stage.binaryId;
		U16 binaryId = (U16) (stage.binaryId >> 16);

		SHFile bin = binaries.ptr[stage.shFileId];
		SHEntry entry = bin.entries.ptr[entrypointId];
		SHBinaryInfo buf = bin.binaries.ptr[entry.binaryIds.ptr[binaryId]];

		VkShaderModule module = NULL;

		gotoIfError2(clean, createShaderModule(
			buf.binaries[ESHBinaryType_SPIRV],
			&module,
			deviceExt,
			instanceExt,
			temp,
			stage.stageType
		))

		CharString_freex(&temp);

		stages.ptrNonConst[j] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = stageBit,
			.module = module,
			.pName = "main"
		};
	}

	//Direct rendering

	VkFormat dynamicFormats[8];
	VkPipelineRenderingCreateInfo dynamicRendering;
	dynamicRendering.sType = 0;

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

			case EDepthStencilFormat_D32S8X24Ext:
				depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
				stencilFormat = VK_FORMAT_S8_UINT;
				break;

			case EDepthStencilFormat_S8X24Ext:
				stencilFormat = VK_FORMAT_S8_UINT;
				break;

			default:
				break;
		}

		for(U64 j = 0; j < info->attachmentCountExt; ++j)
			dynamicFormats[j] = mapVkFormat(ETextureFormatId_unpack[info->attachmentFormatsExt[j]]);

		dynamicRendering = (VkPipelineRenderingCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = info->attachmentCountExt,
			.pColorAttachmentFormats = dynamicFormats,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = stencilFormat
		};
	}

	//MSAA

	VkPipelineMultisampleStateCreateInfo msaaState = (VkPipelineMultisampleStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	if (info->msaa) {
		msaaState.rasterizationSamples = (VkSampleCountFlagBits)(1 << info->msaa);
		msaaState.sampleShadingEnable = info->msaaMinSampleShading > 0;
		msaaState.minSampleShading = info->msaaMinSampleShading;
	}

	//Depth stencil

	VkPipelineDepthStencilStateCreateInfo depthStencilState = (VkPipelineDepthStencilStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.minDepthBounds = 1,
		.maxDepthBounds = 0,
		.depthCompareOp = VK_COMPARE_OP_ALWAYS
	};

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

	if(info->depthStencil.flags)
		depthStencilState = (VkPipelineDepthStencilStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = !!(info->depthStencil.flags & EDepthStencilFlags_DepthTest),
			.depthWriteEnable = !!(info->depthStencil.flags & EDepthStencilFlags_DepthWriteBit),
			.depthCompareOp = mapVkCompareOp(info->depthStencil.depthCompare),
			.stencilTestEnable = !!(info->depthStencil.flags & EDepthStencilFlags_StencilTest),
			.front = stencil,
			.back = stencil,
			.minDepthBounds = 1,
			.maxDepthBounds = 0
		};


	//Rasterizer

	VkPipelineRasterizationStateCreateInfo rasterState = (VkPipelineRasterizationStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.lineWidth = 1
	};

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

		rasterState = (VkPipelineRasterizationStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.cullMode = cullMode,
			.frontFace = windingOrder,
			.depthClampEnable = !!(info->rasterizer.flags & ERasterizerFlags_EnableDepthClamp),
			.polygonMode = polygonMode,
			.depthBiasEnable = !!(info->rasterizer.flags & ERasterizerFlags_EnableDepthBias),
			.depthBiasConstantFactor = (F32) info->rasterizer.depthBiasConstantFactor,
			.depthBiasClamp = info->rasterizer.depthBiasClamp,
			.depthBiasSlopeFactor = info->rasterizer.depthBiasSlopeFactor,
			.lineWidth = 1
		};
	}

	//Vertex input

	Bool hasAttrib = false;

	for(U32 j = 0; j < 16; ++j)
		if(info->vertexLayout.attributes[j].format) {
			hasAttrib = true;
			break;
		}

	//Transform into description

	VkPipelineVertexInputStateCreateInfo vertexInput = (VkPipelineVertexInputStateCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	VkVertexInputAttributeDescription attributes[16];
	VkVertexInputBindingDescription bindings[16];

	if (hasAttrib) {

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

		vertexInput = (VkPipelineVertexInputStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = bindingCount,
			.vertexAttributeDescriptionCount = attribCount,
			.pVertexAttributeDescriptions = attributes,
			.pVertexBindingDescriptions = bindings
		};
	}

	//Finalize

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

	currentInfo = (VkGraphicsPipelineCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = dynamicRendering.sType ? &dynamicRendering : NULL,
		.stageCount = (U32) stages.length,
		.pStages = stages.ptr,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &assembly,
		.pTessellationState = &tessellation,
		.pViewportState = &viewport,
		.pRasterizationState = &rasterState,
		.pMultisampleState = &msaaState,
		.pDepthStencilState = &depthStencilState,
		.pColorBlendState = &blendState,
		.pDynamicState = &dynamicState,
		.layout = *PipelineLayout_ext(PipelineLayoutRef_ptr(pipeline->layout), Vk)
	};

	gotoIfError2(clean, checkVkError(deviceExt->createGraphicsPipelines(
		deviceExt->device,
		NULL,
		1,
		&currentInfo,
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

		gotoIfError2(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName2)))
		CharString_freex(&temp);
	}

	*Pipeline_ext(pipeline, Vk) = pipelineHandle;
	pipelineHandle = NULL;

clean:

	if(pipelineHandle)
		deviceExt->destroyPipeline(deviceExt->device, pipelineHandle, NULL);

	if(currentInfo.pStages)
		for(U64 j = 0; j < currentInfo.stageCount; ++j) {

			VkShaderModule mod = currentInfo.pStages[j].module;

			if(mod)
				deviceExt->destroyShaderModule(deviceExt->device, mod, NULL);
		}

	CharString_freex(&temp);
	ListVkPipelineShaderStageCreateInfo_freex(&stages);

	return s_uccess;
}
