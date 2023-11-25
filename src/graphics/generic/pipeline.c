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
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"

Error PipelineRef_dec(PipelineRef **pipeline) {
	return !RefPtr_dec(pipeline) ? Error_invalidOperation(0) : Error_none();
}

Error PipelineRef_inc(PipelineRef *pipeline) {
	return !RefPtr_inc(pipeline) ? Error_invalidOperation(0) : Error_none();
}

Bool PipelineRef_decAll(List *list) {

	if(!list)
		return true;

	if(list->stride != sizeof(PipelineRef*))
		return false;

	Bool success = true;

	for(U64 i = 0; i < list->length; ++i)
		success &= !PipelineRef_dec(&((PipelineRef**) list->ptr)[i]).genericError;

	List_freex(list);
	return success;
}

PipelineRef *PipelineRef_at(List list, U64 index) {

	Buffer buf = List_at(list, index);

	if(Buffer_length(buf) != sizeof(PipelineRef*))
		return NULL;

	return *(PipelineRef* const*) buf.ptr;
}

impl Bool Pipeline_freeExt(Pipeline *pipeline, Allocator alloc);
impl extern const U64 PipelineExt_size;

Bool Pipeline_free(Pipeline *pipeline, Allocator alloc) {

	Pipeline_freeExt(pipeline, alloc);

	for (U64 i = 0; i < pipeline->stages.length; ++i)
		Buffer_freex(&((PipelineStage*)pipeline->stages.ptr)[i].shaderBinary);

	List_freex(&pipeline->stages);
	GraphicsDeviceRef_dec(&pipeline->device);
	return true;
}

impl Error GraphicsDevice_createPipelinesComputeExt(GraphicsDevice *device, List names, List *pipelines);

Error GraphicsDeviceRef_createPipelinesCompute(
	GraphicsDeviceRef *deviceRef,
	List *shaderBinaries,			//<Buffer>
	List names,						//Temporary names for debugging. Can be empty too, else match shaderBinaries->length
	List *pipelines					//<PipelineRef*>
) {

	if(!deviceRef || !shaderBinaries || !pipelines)
		return Error_nullPointer(!deviceRef ? 0 : (!shaderBinaries ? 1 : 2));

	if(!shaderBinaries->length || shaderBinaries->stride != sizeof(Buffer))
		return Error_invalidParameter(1, 0);

	if(names.length && (names.length != shaderBinaries->length || names.stride != sizeof(CharString)))
		return Error_invalidParameter(2, 0);

	if(shaderBinaries->length >> 32)
		return Error_outOfBounds(1, shaderBinaries->length, U32_MAX);

	if(pipelines->ptr)
		return Error_invalidParameter(3, 0);

	*pipelines = List_createEmpty(sizeof(PipelineRef*));
	Error err = Error_none();
	_gotoIfError(clean, List_resizex(pipelines, shaderBinaries->length));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	for (U64 i = 0; i < pipelines->length; ++i) {

		RefPtr **refPtr = &((RefPtr**)pipelines->ptr)[i];

		_gotoIfError(clean, RefPtr_createx(
			(U32)(sizeof(Pipeline) + PipelineExt_size), 
			(ObjectFreeFunc) Pipeline_free, 
			EGraphicsTypeId_Pipeline, 
			refPtr
		));

		Pipeline *pipeline = PipelineRef_ptr(*refPtr);

		GraphicsDeviceRef_inc(deviceRef);

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
	}

	List_freex(shaderBinaries);

	_gotoIfError(clean, GraphicsDevice_createPipelinesComputeExt(device, names, pipelines));

	goto success;

clean:

	for (U64 i = 0; i < pipelines->length; ++i)
		RefPtr_dec(&((RefPtr**)pipelines->ptr)[i]);

	List_freex(pipelines);

success:
	return err;
}

Error validateBlend(EBlend blend, Bool hasDualSrc) {

	if(blend >= EBlend_Count)
		return Error_invalidEnum(0, (U64)blend, EBlend_Count);

	if(!hasDualSrc && blend >= EBlend_Src1ColorExt && blend <= EBlend_InvSrc1AlphaExt)
		return Error_unsupportedOperation(0);

	return Error_none();
}

impl Error GraphicsDevice_createPipelinesGraphicsExt(GraphicsDevice *device, List names, List *pipelines);

Error GraphicsDeviceRef_createPipelinesGraphics(
	GraphicsDeviceRef *deviceRef,
	List *stages,					//<PipelineStage>
	List *infos,					//<PipelineGraphicsInfo>
	List names,						//Temporary names for debugging. Can be empty too, else match infos->length 
	List *pipelines					//<PipelineRef*>
) {

	//Validate

	if(!deviceRef || !stages || !infos || !pipelines)
		return Error_nullPointer(!deviceRef ? 0 : (!stages ? 1 : (!infos ? 2 : 3)));

	if(!stages->length || stages->stride != sizeof(PipelineStage))
		return Error_invalidParameter(1, 0);

	if(!infos->length || infos->stride != sizeof(PipelineGraphicsInfo) || infos->length >> 32)
		return Error_invalidParameter(2, 0);

	if(names.length && (names.length != infos->length || names.stride != sizeof(CharString)))
		return Error_invalidParameter(2, 0);

	if(pipelines->ptr)
		return Error_invalidParameter(4, 0);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	U64 totalStageCount = 0;

	for(U64 i = 0; i < infos->length; ++i) {

		const PipelineGraphicsInfo *info = &((const PipelineGraphicsInfo*) infos->ptr)[i];

		//Validate some basics

		if(info->attachmentCountExt > 8)
			return Error_outOfBounds((U32)i, info->attachmentCountExt, 8);

		if(!info->stageCount || totalStageCount > stages->length)
			return Error_invalidOperation(0);

		U64 countStart = totalStageCount;
		totalStageCount += stages->length;

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

			if((!!info->renderPass) == (info->attachmentCountExt || info->depthFormatExt))
				return Error_invalidOperation(4);
		}

		//Validate that stages are unique, are compatible.

		U64 stageFlags = 0;

		for (U64 j = countStart; j < countStart + info->stageCount; ++j) {

			PipelineStage stage = ((const PipelineStage*)stages->ptr)[j];

			if(stage.stageType >= EPipelineStage_Count)
				return Error_invalidOperation(21);

			if((stageFlags >> stage.stageType) & 1)
				return Error_alreadyDefined(0);

			stageFlags |= (U64)1 << stage.stageType;
		}

		//Invalidate compute or raytracing stages

		if((stageFlags >> EPipelineStage_Compute) & 1)
			return Error_invalidOperation(5);

		for(U32 j = EPipelineStage_RtStart; j < EPipelineStage_RtEnd; ++j)
			if((stageFlags >> j) & 1)
				return Error_invalidOperation(6);

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

		//TODO: Implement renderPass here! Also don't forgor to properly handle check if this is the same device

		if(info->renderPass)
			return Error_unsupportedOperation(3);

		//Vertex input

		for(U32 j = 0; j < 16; ++j) {

			//Validate buffer strides

			U16 stride = info->vertexLayout.bufferStrides12_isInstance1[j] & 4095;

			if(stride > 2048)
				return Error_invalidOperation(7);

			//Validate format for attribute

			VertexAttribute attrib = info->vertexLayout.attributes[j];

			if(attrib.format >= ETextureFormatId_Count)
				return Error_invalidOperation(8);

			ETextureFormat format = ETextureFormatId_unpack[attrib.format];

			if(!attrib.format)
				continue;

			if(ETextureFormat_getIsCompressed(format))
				return Error_invalidOperation(9);

			//Validate bounds for attribute

			U64 size = ETextureFormat_getSize(format, 1, 1);
			U64 offset = attrib.offset11 & 2047;

			if(offset + size > stride)
				return Error_outOfBounds(0, offset + size, stride);
		}

		if(
			!(info->rasterizer.flags & ERasterizerFlags_EnableDepthBias) && (
				info->rasterizer.depthBiasConstantFactor ||
				info->rasterizer.depthBiasClamp ||
				info->rasterizer.depthBiasSlopeFactor
			)
		)
			return Error_unsupportedOperation(5);

		if(
			(info->rasterizer.flags & ERasterizerFlags_IsWireframeExt) &&
			!(device->info.capabilities.features & EGraphicsFeatures_Wireframe)
		)
			return Error_unsupportedOperation(4);

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
			return Error_unsupportedOperation(6);

		if(info->depthStencil.depthCompare && !(info->depthStencil.flags & EDepthStencilFlags_DepthTest))
			return Error_unsupportedOperation(7);

		if(info->blendState.logicOpExt && !(device->info.capabilities.features & EGraphicsFeatures_LogicOp))
			return Error_unsupportedOperation(8);

		//Validate enums

		if(info->topologyMode >= EToplogyMode_Count)
			return Error_invalidOperation(10);

		if(info->rasterizer.cullMode >= ECullMode_Count)
			return Error_invalidOperation(11);

		if(info->depthStencil.stencilCompare >= ECompareOp_Count)
			return Error_invalidOperation(12);

		if(info->depthStencil.depthCompare >= ECompareOp_Count)
			return Error_invalidOperation(13);

		if(info->depthStencil.stencilFail >= EStencilOp_Count)
			return Error_invalidOperation(14);

		if(info->depthStencil.stencilPass >= EStencilOp_Count)
			return Error_invalidOperation(15);

		if(info->depthStencil.stencilDepthFail >= EStencilOp_Count)
			return Error_invalidOperation(16);

		Bool dualBlend = device->info.capabilities.features & EGraphicsFeatures_DualSrcBlend;

		for(U64 j = 0; j < 8; ++j) {

			BlendStateAttachment attachj = info->blendState.attachments[j];

			if(attachj.blendOp >= EBlendOp_Count)
				return Error_invalidOperation(17);

			if(attachj.blendOpAlpha >= EBlendOp_Count)
				return Error_invalidOperation(18);

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
			return Error_invalidOperation(19);

		if(info->depthFormatExt >= EDepthStencilFormat_Count)
			return Error_invalidOperation(20);

		for(U32 j = 0; j < 8; ++j)
			if(info->attachmentFormatsExt[j] >= ETextureFormatId_Count)
				return Error_invalidOperation(22);
	}

	//Create ref ptrs

	Error err = Error_none();
	
	*pipelines = List_createEmpty(sizeof(PipelineRef*));
	_gotoIfError(clean, List_resizex(pipelines, infos->length));

	totalStageCount = 0;

	for (U64 i = 0; i < pipelines->length; ++i) {

		PipelineGraphicsInfo *info = &((PipelineGraphicsInfo*) infos->ptr)[i];
		PipelineRef **refPtr = &((PipelineRef**)pipelines->ptr)[i];

		_gotoIfError(clean, RefPtr_createx(
			(U32)(sizeof(Pipeline) + PipelineExt_size + sizeof(PipelineGraphicsInfo)), 
			(ObjectFreeFunc) Pipeline_free, 
			EGraphicsTypeId_Pipeline, 
			refPtr
		));

		Pipeline *pipeline = PipelineRef_ptr(*refPtr);

		GraphicsDeviceRef_inc(deviceRef);

		*pipeline = (Pipeline) {
			.device = deviceRef,
			.type = EPipelineType_Graphics,
			.stages = List_createEmpty(sizeof(PipelineStage))
		};

		_gotoIfError(clean, List_resizex(&pipeline->stages, info->stageCount));

		List tempList = (List) { 0 };
		_gotoIfError(clean, List_createSubset(*stages, totalStageCount, info->stageCount, &tempList));

		Buffer_copy(List_buffer(pipeline->stages), List_bufferConst(tempList));

		for(U64 k = totalStageCount, j = k; j < k + info->stageCount; ++j)
			((PipelineStage*)stages->ptr)[j].shaderBinary = Buffer_createNull();			//Moved

		void *pipelineExt = Pipeline_ext(pipeline, );
		pipeline->extraInfo = (U8*)pipelineExt + PipelineExt_size;
		*(PipelineGraphicsInfo*)pipeline->extraInfo = ((const PipelineGraphicsInfo*) infos->ptr)[i];

		totalStageCount += info->stageCount;
	}

	//Free stages and info since they moved

	List_freex(stages);
	List_freex(infos);

	//Create API objects

	_gotoIfError(clean, GraphicsDevice_createPipelinesGraphicsExt(device, names, pipelines));
	goto success;

	//Clean RefPtrs if failed

clean:

	for (U64 i = 0; i < pipelines->length; ++i)
		RefPtr_dec(&((RefPtr**)pipelines->ptr)[i]);

	List_freex(pipelines);

success:
	return err;
}
