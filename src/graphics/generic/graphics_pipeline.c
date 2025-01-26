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
#include "graphics/generic/device.h"
#include "graphics/generic/texture.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/ref_ptrx.h"
#include "platforms/log.h"
#include "types/container/texture_format.h"
#include "formats/oiSH/sh_file.h"

TListImpl(PipelineGraphicsInfo);

Bool validateBlend(EBlend blend, Bool hasDualSrcBlend, Error *e_rr) {

	Bool s_uccess = true;

	if(blend >= EBlend_Count)
		retError(clean, Error_invalidEnum(
			0, (U64)blend, EBlend_Count, "validateBlend()::blend is out of bounds"
		))

	if(!hasDualSrcBlend && blend >= EBlend_Src1ColorExt && blend <= EBlend_InvSrc1AlphaExt)
		retError(clean, Error_unsupportedOperation(
			0, "validateBlend()::blend is unsupported since hasDualSrcBlend is false"
		))

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_createPipelineGraphics(
	GraphicsDeviceRef *deviceRef,
	ListSHFile binaries,
	ListPipelineStage *stages,
	PipelineGraphicsInfo info,
	CharString name,
	PipelineRef **pipeline,
	Error *e_rr
) {

	Bool s_uccess = true;

	//Validate

	if(!deviceRef || !stages || !stages->length || !pipeline)
		retError(clean, Error_nullPointer(
			!deviceRef ? 0 : (!stages || !stages->length ? 1 : 4),
			"GraphicsDeviceRef_createPipelineGraphics()::deviceRef, stages and pipeline are required"
		))

	if(deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_createPipelineGraphics()::deviceRef is an invalid type"
		))

	if(*pipeline)
		retError(clean, Error_invalidParameter(
			4, 0,
			"GraphicsDeviceRef_createPipelineGraphics()::*pipeline is non NULL, indicating a possible memleak"
		))

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	//Validate some basics

	if(info.attachmentCountExt > 8)
		retError(clean, Error_outOfBounds(
			0, info.attachmentCountExt, 8,
			"GraphicsDeviceRef_createPipelineGraphics()::info.attachmentCountExt out of bounds"
		))

	if(!info.renderPass && info.subPass)
		retError(clean, Error_invalidOperation(
			1,
			"GraphicsDeviceRef_createPipelineGraphics()::info.subPass is specified while renderPass is NULL"
		))

	//Validate msaa extensions

	U32 dataTypeCheck = 0;

	switch (info.msaa) {
		case EMSAASamples_x2Ext:	dataTypeCheck = EGraphicsDataTypes_MSAA2x;	break;
		case EMSAASamples_x8Ext:	dataTypeCheck = EGraphicsDataTypes_MSAA8x;	break;
		default:																break;
	}

	if(dataTypeCheck && !(device->info.capabilities.dataTypes & dataTypeCheck))
		retError(clean, Error_unsupportedOperation(
			1,
			"GraphicsDeviceRef_createPipelineGraphics()::info uses an MSAA variant that's not supported "
			"(1 and 4 samples are always supported)"
		))

	//Validate render pass / attachments

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {

		//Force renderPass definition and no direct rendering stuff

		if(!info.renderPass || info.attachmentCountExt || info.depthFormatExt)
			retError(clean, Error_unsupportedOperation(
				2,
				"GraphicsDeviceRef_createPipelineGraphics()::info.renderPass is required when "
				"directRendering is unsupported and attachmentCountExt and depthFormatExt should be 0 and None"
			))
	}

	else {

		//Force one of render pass or dynamic rendering.

		if((info.renderPass != NULL) == (info.attachmentCountExt || info.depthFormatExt))
			retError(clean, Error_invalidOperation(
				4,
				"GraphicsDeviceRef_createPipelineGraphics()::info should pick either "
				"directRendering or renderPass"
			))
	}

	//Validate that stages are unique, are compatible.

	U64 stageFlags = 0;

	for (U64 j = 0; j < stages->length; ++j) {

		PipelineStage stage = stages->ptr[j];

		if(stage.shFileId >= binaries.length)
			retError(clean, Error_invalidOperation(
				21,
				"GraphicsDeviceRef_createPipelineGraphics()::stages[j] points to invalid shFile"
			))

		SHFile binary = binaries.ptr[stage.shFileId];

		U32 shaderId = stage.binaryId;

		U16 entrypointId = (U16) shaderId;
		U16 binaryId = (U16) (shaderId >> 16);

		if(entrypointId >= binary.entries.length)
			retError(clean, Error_invalidOperation(
				21,
				"GraphicsDeviceRef_createPipelineGraphics()::stages[j] points to invalid entry"
			))

		SHEntry entry = binary.entries.ptr[entrypointId];

		if((stageFlags >> entry.stage) & 1)
			retError(clean, Error_alreadyDefined(
				0,
				"GraphicsDeviceRef_createPipelineGraphics()::stages[j].stageType is duplicate for info"
			))

		if(binaryId >= entry.binaryIds.length)
			retError(clean, Error_invalidOperation(
				22,
				"GraphicsDeviceRef_createPipelineGraphics()::stages[j] points to invalid binary"
			))

		U32 finalBinaryId = entry.binaryIds.ptr[binaryId];
		SHBinaryInfo bin = binary.binaries.ptr[finalBinaryId];

		gotoIfError3(clean, GraphicsDeviceRef_checkShaderFeatures(deviceRef, bin, entry, e_rr))

		stageFlags |= (U64)1 << entry.stage;
	}

	//Invalidate compute or raytracing stages

	if(stageFlags & ((1 << ESHPipelineStage_Compute) | (1 << ESHPipelineStage_WorkgraphExt)))
		retError(clean, Error_invalidOperation(
			5, "GraphicsDeviceRef_createPipelinesGraphics()::info contains compute or workgraph stage"
		))

	if(stageFlags & (((1 << ESHPipelineStage_RtEndExt) - 1) &~ ((1 << ESHPipelineStage_RtStartExt) - 1)))
		retError(clean, Error_invalidOperation(6, "GraphicsDeviceRef_createPipelineGraphics()::info contains RT stage(s)"))

	//Validate if stages are allowed due to GeometryShader

	if(
		((stageFlags >> EPipelineStage_GeometryExt) & 1) &&
		!(device->info.capabilities.features & EGraphicsFeatures_GeometryShader)
	)
		retError(clean, Error_unsupportedOperation(
			11,
			"GraphicsDeviceRef_createPipelineGraphics()::info contains geometry shader, "
			"but geometry shaders aren't supported"
		))

	//TODO: Implement renderPass here! Also don't forgor to properly handle check if this is the same device

	if(info.renderPass)
		retError(clean, Error_unsupportedOperation(
			3, "GraphicsDeviceRef_createPipelineGraphics()::info contains renderPass but this isn't supported yet"
		))

	//Vertex input

	for(U32 j = 0; j < 16; ++j) {

		//Validate buffer strides

		U16 stride = info.vertexLayout.bufferStrides12_isInstance1[j] & 4095;

		if(stride > 2048)
			retError(clean, Error_invalidOperation(
				7,
				"GraphicsDeviceRef_createPipelineGraphics()::info.vertexLayout.bufferStrides12_isInstance1[j] "
				"contains stride that's bigger than 2048, which is illegal."
			))

		//Validate format for attribute

		VertexAttribute attrib = info.vertexLayout.attributes[j];

		if(attrib.format >= ETextureFormatId_Count)
			retError(clean, Error_invalidOperation(
				8,
				"GraphicsDeviceRef_createPipelineGraphics()::info.vertexLayout.attributes[j].format "
				"is invalid"
			))

		ETextureFormat format = ETextureFormatId_unpack[attrib.format];

		if(!attrib.format)
			continue;

		if(!GraphicsDeviceInfo_supportsFormatVertexAttribute(format))
			retError(clean, Error_invalidOperation(
				9,
				"GraphicsDeviceRef_createPipelineGraphics()::info.vertexLayout.attributes[j].format is "
				"unsupported as a vertex input attribute"
			))

		//Validate bounds for attribute

		U64 size = ETextureFormat_getSize(format, 1, 1, 1);
		U64 offset = attrib.offset11 & 2047;

		if(offset + size > stride)
			retError(clean, Error_outOfBounds(
				0, offset + size, stride,
				"GraphicsDeviceRef_createPipelineGraphics()::info.vertexLayout.attributes[j].offset "
				"is out of bounds"
			))
	}

	if(
		!(info.rasterizer.flags & ERasterizerFlags_EnableDepthBias) && (
			info.rasterizer.depthBiasConstantFactor ||
			info.rasterizer.depthBiasClamp ||
			info.rasterizer.depthBiasSlopeFactor
		)
	)
		retError(clean, Error_unsupportedOperation(
			5,
			"GraphicsDeviceRef_createPipelineGraphics()::info rasterizer uses depth bias "
			"but depthBias is disabled"
		))

	if(
		(info.rasterizer.flags & ERasterizerFlags_IsWireframeExt) &&
		!(device->info.capabilities.features & EGraphicsFeatures_Wireframe)
	)
		retError(clean, Error_unsupportedOperation(
			4,
			"GraphicsDeviceRef_createPipelineGraphics()::info requires wireframe extension, "
			"which isn't supported"
		))

	if (
		!(info.depthStencil.flags & EDepthStencilFlags_StencilTest) && (
			info.depthStencil.stencilCompare ||
			info.depthStencil.stencilFail ||
			info.depthStencil.stencilPass ||
			info.depthStencil.stencilDepthFail ||
			info.depthStencil.stencilReadMask ||
			info.depthStencil.stencilWriteMask
		)
	)
		retError(clean, Error_unsupportedOperation(
			6,
			"GraphicsDeviceRef_createPipelineGraphics()::info stencilTest is off, "
			"but stencil params were set"
		))

	if(info.depthStencil.depthCompare && !(info.depthStencil.flags & EDepthStencilFlags_DepthTest))
		retError(clean, Error_unsupportedOperation(
			7,
			"GraphicsDeviceRef_createPipelineGraphics()::info depthTest is off but depthCompare was set"
		))

	if(info.blendState.logicOpExt && !(device->info.capabilities.features & EGraphicsFeatures_LogicOp))
		retError(clean, Error_unsupportedOperation(
			8,
			"GraphicsDeviceRef_createPipelineGraphics()::info.blendState.logicOpExt "
			"was set but logicOp wasn't supported"
		))

	//Validate enums

	if(info.topologyMode >= EToplogyMode_Count)
		retError(clean, Error_invalidOperation(
			10, "GraphicsDeviceRef_createPipelineGraphics()::info.topologyMode is out of bounds"
		))

	if(info.patchControlPoints > 32)
		retError(clean, Error_invalidOperation(
			24,
			"GraphicsDeviceRef_createPipelineGraphics()::info.pathControlPoints is out of bounds (max 32)"
		))

	if(info.rasterizer.cullMode >= ECullMode_Count)
		retError(clean, Error_invalidOperation(
			11, "GraphicsDeviceRef_createPipelineGraphics()::info.rasterizer.cullMode is out of bounds"
		))

	if(info.depthStencil.stencilCompare >= ECompareOp_Count)
		retError(clean, Error_invalidOperation(
			12,
			"GraphicsDeviceRef_createPipelineGraphics()::info.depthStencil.stencilCompare is out of bounds"
		))

	if(info.depthStencil.depthCompare >= ECompareOp_Count)
		retError(clean, Error_invalidOperation(
			13,
			"GraphicsDeviceRef_createPipelineGraphics()::info.depthStencil.depthCompare is out of bounds"
		))

	if(info.depthStencil.stencilFail >= EStencilOp_Count)
		retError(clean, Error_invalidOperation(
			14,
			"GraphicsDeviceRef_createPipelineGraphics()::info.depthStencil.stencilFail is out of bounds"
		))

	if(info.depthStencil.stencilPass >= EStencilOp_Count)
		retError(clean, Error_invalidOperation(
			15,
			"GraphicsDeviceRef_createPipelineGraphics()::info.depthStencil.stencilPass is out of bounds"
		))

	if(info.depthStencil.stencilDepthFail >= EStencilOp_Count)
		retError(clean, Error_invalidOperation(
			16,
			"GraphicsDeviceRef_createPipelineGraphics()::info.depthStencil.stencilDepthFail is out of bounds"
		))

	Bool dualBlend = device->info.capabilities.features & EGraphicsFeatures_DualSrcBlend;

	for(U64 j = 0; j < 8; ++j) {

		BlendStateAttachment attachj = info.blendState.attachments[j];

		if(attachj.blendOp >= EBlendOp_Count)
			retError(clean, Error_invalidOperation(
				17,
				"GraphicsDeviceRef_createPipelineGraphics()::info.blendState.attachments[j].blendOp "
				"out of bounds"
			))

		if(attachj.blendOpAlpha >= EBlendOp_Count)
			retError(clean, Error_invalidOperation(
				18,
				"GraphicsDeviceRef_createPipelineGraphics()::info.blendState.attachments[j].blendOpAlpha "
				"out of bounds"
			))

		gotoIfError3(clean, validateBlend(attachj.srcBlend, dualBlend, e_rr))
		gotoIfError3(clean, validateBlend(attachj.dstBlend, dualBlend, e_rr))
		gotoIfError3(clean, validateBlend(attachj.srcBlendAlpha, dualBlend, e_rr))
		gotoIfError3(clean, validateBlend(attachj.dstBlendAlpha, dualBlend, e_rr))
	}

	if(info.blendState.logicOpExt >= ELogicOpExt_Count)
		retError(clean, Error_invalidOperation(
			19, "GraphicsDeviceRef_createPipelineGraphics()::info.blendState.logicOpExt out of bounds"
		))

	if(info.blendState.renderTargetMask && info.blendState.logicOpExt)
		retError(clean, Error_invalidOperation(
			23,
			"GraphicsDeviceRef_createPipelineGraphics()::info.blendState.logicOpExt can't be on if blending is used"
		))

	if(info.depthFormatExt >= EDepthStencilFormat_Count)
		retError(clean, Error_invalidOperation(
			20, "GraphicsDeviceRef_createPipelineGraphics()::info.depthFormatExt out of bounds"
		))

	for(U32 j = 0; j < 8; ++j)
		if(info.attachmentFormatsExt[j] >= ETextureFormatId_Count)
			retError(clean, Error_invalidOperation(
				22,
				"GraphicsDeviceRef_createPipelineGraphics()::info.attachmentFormatsExt[j] out of bounds"
			))

	//Create ref ptrs

	gotoIfError2(clean, RefPtr_createx(
		(U32)(sizeof(Pipeline) + GraphicsDeviceRef_getObjectSizes(deviceRef)->pipeline + sizeof(PipelineGraphicsInfo)),
		(ObjectFreeFunc) Pipeline_free,
		(ETypeId) EGraphicsTypeId_Pipeline,
		pipeline
	))

	Pipeline *pipelinePtr = PipelineRef_ptr(*pipeline);

	//Log_debugLnx("Create: GraphicsPipeline %.*s (%p)", (int) CharString_length(name), name.ptr, pipelinePtr);

	GraphicsDeviceRef_inc(deviceRef);

	*pipelinePtr = (Pipeline) { .device = deviceRef, .type = EPipelineType_Graphics };

	*Pipeline_info(pipelinePtr, PipelineGraphicsInfo) = info;

	if(ListPipelineStage_isRef(*stages))
		gotoIfError2(clean, ListPipelineStage_createCopyx(*stages, &pipelinePtr->stages))

	else pipelinePtr->stages = *stages;

	*stages = (ListPipelineStage) { 0 };

	for (U64 i = 0; i < pipelinePtr->stages.length; ++i) {

		PipelineStage *stage = &pipelinePtr->stages.ptrNonConst[i];
		SHFile binary = binaries.ptr[stage->shFileId];
		U32 shaderId = stage->binaryId;

		U16 entrypointId = (U16) shaderId;

		SHEntry entry = binary.entries.ptr[entrypointId];

		switch(entry.stage) {

			case ESHPipelineStage_Vertex:		stage->stageType = EPipelineStage_Vertex;			break;
			case ESHPipelineStage_Pixel:		stage->stageType = EPipelineStage_Pixel;			break;
			case ESHPipelineStage_GeometryExt:	stage->stageType = EPipelineStage_GeometryExt;		break;
			case ESHPipelineStage_Hull:			stage->stageType = EPipelineStage_Hull;				break;
			case ESHPipelineStage_Domain:		stage->stageType = EPipelineStage_Domain;			break;

			//TODO: Mesh and task shaders

			default:
				retError(clean, Error_invalidParameter(
					0, 0, "GraphicsDeviceRef_createPipelineGraphics()::stages[i] stageType isn't supported (yet)"
				))
		}
	}

	//Create API objects

	gotoIfError3(clean, GraphicsDevice_createPipelineGraphicsExt(device, binaries, name, pipelinePtr, e_rr))
	goto success;

	//Clean RefPtrs if failed

clean:
	RefPtr_dec(pipeline);

success:
	return s_uccess;
}
