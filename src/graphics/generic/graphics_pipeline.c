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
#include "graphics/generic/texture.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/ref_ptrx.h"
#include "formats/texture.h"

TListImpl(PipelineGraphicsInfo);

Error validateBlend(EBlend blend, Bool hasDualSrcBlend) {

	if(blend >= EBlend_Count)
		return Error_invalidEnum(
			0, (U64)blend, EBlend_Count, "validateBlend()::blend is out of bounds"
		);

	if(!hasDualSrcBlend && blend >= EBlend_Src1ColorExt && blend <= EBlend_InvSrc1AlphaExt)
		return Error_unsupportedOperation(
			0, "validateBlend()::blend is unsupported since hasDualSrcBlend is false"
		);

	return Error_none();
}

impl Error GraphicsDevice_createPipelinesGraphicsExt(
	GraphicsDevice *device, ListCharString names, ListPipelineRef *pipelines
);

Error GraphicsDeviceRef_createPipelinesGraphics(
	GraphicsDeviceRef *deviceRef,
	ListPipelineStage *stages,
	ListPipelineGraphicsInfo *infos,
	ListCharString names,					//Temporary names for debugging. Can be empty too, else match infos->length
	ListPipelineRef *pipelines
) {

	//Validate

	if(!deviceRef || !stages || !infos || !pipelines)
		return Error_nullPointer(
			!deviceRef ? 0 : (!stages ? 1 : (!infos ? 2 : 3)),
			"GraphicsDeviceRef_createPipelinesGraphics()::deviceRef, stages, infos and pipelines are required"
		);

	if(deviceRef->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_createPipelinesGraphics()::deviceRef is an invalid type"
		);

	if(!stages->length)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelinesGraphics()::stages should be of length > 0"
		);

	if(!infos->length || infos->length >> 32)
		return Error_invalidParameter(
			2, 0,
			"GraphicsDeviceRef_createPipelinesGraphics()::infos should be length = <0, U32_MAX>"
		);

	if(names.length && (names.length != infos->length))
		return Error_invalidParameter(
			2, 0, "GraphicsDeviceRef_createPipelinesGraphics()::names should be the same length as infos"
		);

	if(pipelines->ptr)
		return Error_invalidParameter(
			4, 0, 
			"GraphicsDeviceRef_createPipelinesGraphics()::pipelines->ptr is non zero, indicating a possible memleak"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	U64 totalStageCount = 0;

	for(U64 i = 0; i < infos->length; ++i) {

		const PipelineGraphicsInfo *info = &infos->ptr[i];

		//Validate some basics

		if(info->attachmentCountExt > 8)
			return Error_outOfBounds(
				(U32)i, info->attachmentCountExt, 8,
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].attachmentCountExt out of bounds"
			);

		if(!info->stageCount || totalStageCount + info->stageCount > stages->length)
			return Error_invalidOperation(
				0, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i] stage out of bounds"
			);

		U64 countStart = totalStageCount;
		totalStageCount += info->stageCount;

		if(!info->renderPass && info->subPass)
			return Error_invalidOperation(
				1, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].subPass is specified while renderPass is NULL"
			);

		//Validate msaa extensions

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

			default:
				break;
		}

		if(dataTypeCheck && !(device->info.capabilities.dataTypes & dataTypeCheck))
			return Error_unsupportedOperation(
				1,
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i] uses an MSAA variant that's not supported "
				"(1 and 4 samples are always supported)"
			);

		//Validate render pass / attachments

		if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {

			//Force renderPass definition and no direct rendering stuff

			if(!info->renderPass || info->attachmentCountExt || info->depthFormatExt)
				return Error_unsupportedOperation(
					2,
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].renderPass is required when "
					"directRendering is unsupported and attachmentCountExt and depthFormatExt should be 0 and None"
				);
		}

		else {

			//Force one of render pass or dynamic rendering.

			if((info->renderPass != NULL) == (info->attachmentCountExt || info->depthFormatExt))
				return Error_invalidOperation(
					4, 
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i] should pick either "
					"directRendering or renderPass"
				);
		}

		//Validate that stages are unique, are compatible.

		U64 stageFlags = 0;

		for (U64 j = countStart; j < countStart + info->stageCount; ++j) {

			PipelineStage stage = stages->ptr[j];

			if(stage.stageType >= EPipelineStage_Count)
				return Error_invalidOperation(
					21, 
					"GraphicsDeviceRef_createPipelinesGraphics()::stages[j].stageType is invalid"
				);

			if((stageFlags >> stage.stageType) & 1)
				return Error_alreadyDefined(
					0, 
					"GraphicsDeviceRef_createPipelinesGraphics()::stages[j].stageType is duplicate for infos[i]"
				);

			stageFlags |= (U64)1 << stage.stageType;
		}

		//Invalidate compute or raytracing stages

		if((stageFlags >> EPipelineStage_Compute) & 1)
			return Error_invalidOperation(
				5, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i] contains compute stage"
			);

		//TODO:
		//for(U32 j = EPipelineStage_RtStart; j < EPipelineStage_RtEnd; ++j)
		//	if((stageFlags >> j) & 1)
		//		return Error_invalidOperation(6, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i] contains RT stage(s)");

		//Validate if stages are allowed due to GeometryShader

		if(
			((stageFlags >> EPipelineStage_GeometryExt) & 1) &&
			!(device->info.capabilities.features & EGraphicsFeatures_GeometryShader)
		)
			return Error_unsupportedOperation(
				11,
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i] contains geometry shader, "
				"but geometry shaders aren't supported"
			);

		//TODO: Implement renderPass here! Also don't forgor to properly handle check if this is the same device

		if(info->renderPass)
			return Error_unsupportedOperation(
				3, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i] contains renderPass but this isn't supported yet"
			);

		//Vertex input

		for(U32 j = 0; j < 16; ++j) {

			//Validate buffer strides

			U16 stride = info->vertexLayout.bufferStrides12_isInstance1[j] & 4095;

			if(stride > 2048)
				return Error_invalidOperation(
					7,
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].vertexLayout.bufferStrides12_isInstance1[j] "
					"contains stride that's bigger than 2048, which is illegal."
				);

			//Validate format for attribute

			VertexAttribute attrib = info->vertexLayout.attributes[j];

			if(attrib.format >= ETextureFormatId_Count)
				return Error_invalidOperation(
					8, 
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].vertexLayout.attributes[j].format "
					"is invalid"
				);

			ETextureFormat format = ETextureFormatId_unpack[attrib.format];

			if(!attrib.format)
				continue;

			if(!GraphicsDeviceInfo_supportsFormatVertexAttribute(format))
				return Error_invalidOperation(
					9,
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].vertexLayout.attributes[j].format is "
					"unsupported as a vertex input attribute"
				);

			//Validate bounds for attribute

			U64 size = ETextureFormat_getSize(format, 1, 1, 1);
			U64 offset = attrib.offset11 & 2047;

			if(offset + size > stride)
				return Error_outOfBounds(
					0, offset + size, stride,
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].vertexLayout.attributes[j].offset "
					"is out of bounds"
				);
		}

		if(
			!(info->rasterizer.flags & ERasterizerFlags_EnableDepthBias) && (
				info->rasterizer.depthBiasConstantFactor ||
				info->rasterizer.depthBiasClamp ||
				info->rasterizer.depthBiasSlopeFactor
			)
		)
			return Error_unsupportedOperation(
				5, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i] rasterizer uses depth bias "
				"but depthBias is disabled"
			);

		if(
			(info->rasterizer.flags & ERasterizerFlags_IsWireframeExt) &&
			!(device->info.capabilities.features & EGraphicsFeatures_Wireframe)
		)
			return Error_unsupportedOperation(
				4, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i] requires wireframe extension, "
				"which isn't supported"
			);

		if (
			!(info->depthStencil.flags & EDepthStencilFlags_StencilTest) && (
				info->depthStencil.stencilCompare ||
				info->depthStencil.stencilFail ||
				info->depthStencil.stencilPass ||
				info->depthStencil.stencilDepthFail ||
				info->depthStencil.stencilReadMask ||
				info->depthStencil.stencilWriteMask
			)
		)
			return Error_unsupportedOperation(
				6, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i] stencilTest is off, "
				"but stencil params were set"
			);

		if(info->depthStencil.depthCompare && !(info->depthStencil.flags & EDepthStencilFlags_DepthTest))
			return Error_unsupportedOperation(
				7, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i] depthTest is off but depthCompare was set"
			);

		if(info->blendState.logicOpExt && !(device->info.capabilities.features & EGraphicsFeatures_LogicOp))
			return Error_unsupportedOperation(
				8,
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].blendState.logicOpExt "
				"was set but logicOp wasn't supported"
			);

		//Validate enums

		if(info->topologyMode >= EToplogyMode_Count)
			return Error_invalidOperation(
				10, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i].topologyMode is out of bounds"
			);

		if(info->rasterizer.cullMode >= ECullMode_Count)
			return Error_invalidOperation(
				11, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i].rasterizer.cullMode is out of bounds"
			);

		if(info->depthStencil.stencilCompare >= ECompareOp_Count)
			return Error_invalidOperation(
				12, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].depthStencil.stencilCompare is out of bounds"
			);

		if(info->depthStencil.depthCompare >= ECompareOp_Count)
			return Error_invalidOperation(
				13, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].depthStencil.depthCompare is out of bounds"
			);

		if(info->depthStencil.stencilFail >= EStencilOp_Count)
			return Error_invalidOperation(
				14, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].depthStencil.stencilFail is out of bounds"
			);

		if(info->depthStencil.stencilPass >= EStencilOp_Count)
			return Error_invalidOperation(
				15, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].depthStencil.stencilPass is out of bounds"
			);

		if(info->depthStencil.stencilDepthFail >= EStencilOp_Count)
			return Error_invalidOperation(
				16, 
				"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].depthStencil.stencilDepthFail is out of bounds"
			);

		Bool dualBlend = device->info.capabilities.features & EGraphicsFeatures_DualSrcBlend;

		for(U64 j = 0; j < 8; ++j) {

			BlendStateAttachment attachj = info->blendState.attachments[j];

			if(attachj.blendOp >= EBlendOp_Count)
				return Error_invalidOperation(
					17, 
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].blendState.attachments[j].blendOp "
					"out of bounds"
				);

			if(attachj.blendOpAlpha >= EBlendOp_Count)
				return Error_invalidOperation(
					18,
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].blendState.attachments[j].blendOpAlpha "
					"out of bounds"
				);

			Error err = validateBlend(attachj.srcBlend, dualBlend);

			if(err.genericError)
				return err;

			if((err = validateBlend(attachj.dstBlend, dualBlend)).genericError)
				return err;

			if((err = validateBlend(attachj.srcBlendAlpha, dualBlend)).genericError)
				return err;

			if((err = validateBlend(attachj.dstBlendAlpha, dualBlend)).genericError)
				return err;
		}

		if(info->blendState.logicOpExt >= ELogicOpExt_Count)
			return Error_invalidOperation(
				19, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i].blendState.logicOpExt out of bounds"
			);

		if(info->depthFormatExt >= EDepthStencilFormat_Count)
			return Error_invalidOperation(
				20, "GraphicsDeviceRef_createPipelinesGraphics()::infos[i].depthFormatExt out of bounds"
			);

		for(U32 j = 0; j < 8; ++j)
			if(info->attachmentFormatsExt[j] >= ETextureFormatId_Count)
				return Error_invalidOperation(
					22, 
					"GraphicsDeviceRef_createPipelinesGraphics()::infos[i].attachmentFormatsExt[j] out of bounds"
				);
	}

	//Create ref ptrs

	Error err = Error_none();

	gotoIfError(clean, ListPipelineRef_resizex(pipelines, infos->length))

	totalStageCount = 0;

	for (U64 i = 0; i < pipelines->length; ++i) {

		PipelineGraphicsInfo *info = &infos->ptrNonConst[i];
		PipelineRef **refPtr = &pipelines->ptrNonConst[i];

		gotoIfError(clean, RefPtr_createx(
			(U32)(sizeof(Pipeline) + PipelineExt_size + sizeof(PipelineGraphicsInfo)),
			(ObjectFreeFunc) Pipeline_free,
			(ETypeId) EGraphicsTypeId_Pipeline,
			refPtr
		))

		Pipeline *pipeline = PipelineRef_ptr(*refPtr);

		GraphicsDeviceRef_inc(deviceRef);

		*pipeline = (Pipeline) { .device = deviceRef, .type = EPipelineType_Graphics };
		gotoIfError(clean, ListPipelineStage_resizex(&pipeline->stages, info->stageCount))

		ListPipelineStage tempList = (ListPipelineStage) { 0 };
		gotoIfError(clean, ListPipelineStage_createSubset(*stages, totalStageCount, info->stageCount, &tempList))

		Buffer_copy(ListPipelineStage_buffer(pipeline->stages), ListPipelineStage_bufferConst(tempList));

		for(U64 k = totalStageCount, j = k; j < k + info->stageCount; ++j)
			stages->ptrNonConst[j].binary = Buffer_createNull();			//Moved

		*Pipeline_info(pipeline, PipelineGraphicsInfo) = infos->ptr[i];

		totalStageCount += info->stageCount;
	}

	//Free stages and info since they moved

	ListPipelineStage_freex(stages);
	ListPipelineGraphicsInfo_freex(infos);

	//Create API objects

	gotoIfError(clean, GraphicsDevice_createPipelinesGraphicsExt(device, names, pipelines))
	goto success;

	//Clean RefPtrs if failed

clean:

	for (U64 i = 0; i < pipelines->length; ++i)
		RefPtr_dec(&pipelines->ptrNonConst[i]);

	ListPipelineRef_freex(pipelines);

success:
	return err;
}
