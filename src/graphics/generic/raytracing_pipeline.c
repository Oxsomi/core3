/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/math.h"
#include "formats/oiSH.h"

TListImpl(PipelineRaytracingGroup);
TListImpl(PipelineRaytracingInfo);

impl Bool GraphicsDevice_createPipelineRaytracingInternalExt(
	GraphicsDeviceRef *deviceRef,
	ListSHFile binaries,
	CharString name,
	U8 maxPayloadSize,
	U8 maxAttributeSize,
	ListU32 binaryIndices,
	Pipeline *pipeline,
	Error *e_rr
);

Bool GraphicsDeviceRef_createPipelineRaytracingExt(
	GraphicsDeviceRef *deviceRef,
	ListPipelineStage *stages,
	ListSHFile binaries,
	ListPipelineRaytracingGroup *groups,
	PipelineRaytracingInfo info,
	CharString name,					//Temporary names for debugging. Can be empty, else match infos->length
	PipelineRef **pipelineRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool madePipeline = false;
	U64 totalBinaryCount = 0;
	ListU32 binaryIndices = (ListU32) { 0 };
	ListPipelineStage tmpStages = (ListPipelineStage) { 0 };

	//Validate sizes

	if(!deviceRef || !stages || !stages->length || !binaries.length || !groups || !pipelineRef)
		retError(clean, Error_nullPointer(
			!deviceRef ? 0 : (!stages || !stages->length ? 1 : (!binaries.length ? 2 : (!groups ? 3 : 6))),
			"GraphicsDeviceRef_createPipelineRaytracing()::device, binaries, stages, groups and pipelineRef are required"
		))

	if(deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_createPipelineRaytracing()::deviceRef is an invalid type"
		))

	if(*pipelineRef)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineRaytracing()::*pipelineRef is non NULL, indicating a possible memleak"
		))

	if(binaries.length >> 16)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineRaytracing()::binaries is limited to a size of 65535"
		))

	for(U64 i = 0; i < binaries.length; ++i)
		if(!binaries.ptr[i].entries.length)
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineRaytracing()::binaries[i] is required"
			))

	for(U64 i = 0; i < binaries.length; ++i)
		totalBinaryCount += binaries.ptr[i].binaries.length;

	gotoIfError2(clean, ListU32_reservex(&binaryIndices, totalBinaryCount))

	U8 maxPayloadSize = 0;
	U8 maxAttributeSize = 0;

	if(ListPipelineStage_isRef(*stages))
		gotoIfError2(clean, ListPipelineStage_createCopyx(*stages, &tmpStages))

	else tmpStages = *stages;

	*stages = (ListPipelineStage) { 0 };

	//Validate stages

	for(U64 i = 0; i < tmpStages.length; ++i) {

		PipelineStage *stagePtr = &tmpStages.ptrNonConst[i], stage = *stagePtr;

		U16 entrypointId = (U16) stage.binaryId;
		U16 binaryId = (U16) (stage.binaryId >> 16);
		U16 shFileId = stage.shFileId;

		if(shFileId >= binaries.length)
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineRaytracing()::shFileId is out of bounds"
			))

		SHFile file = binaries.ptr[shFileId];

		if(entrypointId >= file.entries.length)
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineRaytracing()::entrypointId is out of bounds"
			))

		SHEntry entry = file.entries.ptr[entrypointId];

		if(entry.stage < ESHPipelineStage_RtStartExt || entry.stage > ESHPipelineStage_RtEndExt)
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineRaytracing()::stageType is not RT capable"
			))

		switch (entry.stage) {
			case ESHPipelineStage_AnyHitExt:		stagePtr->stageType = EPipelineStage_AnyHitExt;			break;
			case ESHPipelineStage_ClosestHitExt:	stagePtr->stageType = EPipelineStage_ClosestHitExt;		break;
			case ESHPipelineStage_MissExt:			stagePtr->stageType = EPipelineStage_MissExt;			break;
			case ESHPipelineStage_IntersectionExt:	stagePtr->stageType = EPipelineStage_IntersectionExt;	break;
			case ESHPipelineStage_CallableExt:		stagePtr->stageType = EPipelineStage_CallableExt;		break;
			default:								stagePtr->stageType = EPipelineStage_RaygenExt;			break;
		}

		if(
			stage.binaryId == U32_MAX && entry.stage == ESHPipelineStage_MissExt &&
			!(info.flags & EPipelineRaytracingFlags_NoNullMiss)
		)
			continue;

		if(binaryId >= entry.binaryIds.length)
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelineRaytracing()::binaryId is out of bounds"
			))
			
		U16 realBinaryId = entry.binaryIds.ptr[binaryId];
		SHBinaryInfo bin = file.binaries.ptr[realBinaryId];

		gotoIfError3(clean, GraphicsDeviceRef_checkShaderFeatures(deviceRef, bin, entry, e_rr))

		if(stage.binaryId == U32_MAX)
			retError(clean, Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelineRaytracing()::info.binaryId is U32_MAX (unused) "
				"but it's not allowed"
			))

		U32 id = realBinaryId | ((U32)shFileId << 16);

		if(!ListU32_contains(binaryIndices, id, 0, NULL))
			gotoIfError2(clean, ListU32_pushBackx(&binaryIndices, id))

		maxPayloadSize = U8_max(maxPayloadSize, entry.payloadSize);
		maxAttributeSize = U8_max(maxAttributeSize, entry.intersectionSize);
	}

	Bool anyMotionBlurExt = info.flags & EPipelineRaytracingFlags_AllowMotionBlurExt;

	if((((U64)groups->length + tmpStages.length) >> 32) || ((tmpStages.length) >> 32))
		retError(clean, Error_outOfBounds(
			1, groups->length + tmpStages.length, U32_MAX,
			"GraphicsDeviceRef_createPipelineRaytracing() tmpStages.length + groups.length out of bounds"
		))

	if(info.flags >> EPipelineRaytracingFlags_Count)
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineRaytracing()::info.flags is invalid"
		))

	if((maxPayloadSize & 1) || (maxAttributeSize & 1))
		retError(clean, Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineRaytracing()::info.maxPayloadSize and maxAttributeSize need to be 2 byte aligned"
		))

	if(maxAttributeSize > 32 || info.maxRecursionDepth > 2 || maxPayloadSize > 32)
		retError(clean, Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineRaytracing()::info."
			"maxAttributeSize, maxRecursionDepth and maxRayHitAttributeSize need to be <=32, <=2 and <=32 respectively"
		))

	if(maxAttributeSize < 8 || maxPayloadSize < 2 || !info.maxRecursionDepth)
		retError(clean, Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineRaytracing()::info.maxAttributeSize, "
			"maxPayloadSize and maxRecursionDepth need to be >=8, >=2 and >=1 respectively"
		))

	if(info.groups.length)
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelineRaytracing()::info.groups should be NULL"
		))

	//Validate stages

	for (U64 i = 0; i < groups->length; ++i) {

		PipelineRaytracingGroup group = groups->ptr[i];

		//Validate with flags

		if(group.intersection != U32_MAX && (info.flags & EPipelineRaytracingFlags_SkipAABBs))
			retError(clean, Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].intersection is "
				"disallowed if aabbs are skipped"
			))

		if(group.anyHit == U32_MAX && (info.flags & EPipelineRaytracingFlags_NoNullAnyHit))
			retError(clean, Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].anyHit is null, but "
				"NoNullAnyHit is used"
			))

		if(group.closestHit == U32_MAX && (info.flags & EPipelineRaytracingFlags_NoNullClosestHit))
			retError(clean, Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].closestHit is null, but "
				"NoNullClosestHit is used"
			))

		//Validate bounds

		if(group.anyHit != U32_MAX && group.anyHit >= tmpStages.length)
			retError(clean, Error_outOfBounds(
				1, group.anyHit, tmpStages.length,
				"GraphicsDeviceRef_createPipelineRaytracing() groups[i].anyHit out of bounds"
			))

		if(group.closestHit != U32_MAX && group.closestHit >= tmpStages.length)
			retError(clean, Error_outOfBounds(
				1, group.closestHit, tmpStages.length,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].closestHit out of bounds"
			))

		if(group.intersection != U32_MAX && group.intersection >= tmpStages.length)
			retError(clean, Error_outOfBounds(
				1, group.intersection, tmpStages.length,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].intersection out of bounds"
			))

		//Validate pipeline type

		if(group.anyHit != U32_MAX && tmpStages.ptr[group.anyHit + i].stageType != EPipelineStage_AnyHitExt)
			retError(clean, Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].anyHit pointed to "
				"the wrong pipeline stage"
			))

		if(group.closestHit != U32_MAX && tmpStages.ptr[group.closestHit + i].stageType != EPipelineStage_ClosestHitExt)
			retError(clean, Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].closestHit pointed to "
				"the wrong pipeline stage"
			))

		if(group.intersection != U32_MAX && tmpStages.ptr[group.intersection + i].stageType != EPipelineStage_IntersectionExt)
			retError(clean, Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelineRaytracing()::groups[i].intersection pointed to "
				"the wrong pipeline stage"
			))
	}

	//Check for raytracing extension(s)

	GraphicsDevice *dev = GraphicsDeviceRef_ptr(deviceRef);

	if(!(dev->info.capabilities.features & EGraphicsFeatures_RayPipeline))
		retError(clean, Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineRaytracing() can't be called if RayPipeline isn't supported"
		))

	if(anyMotionBlurExt && !(dev->info.capabilities.features & EGraphicsFeatures_RayMotionBlur))
		retError(clean, Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelineRaytracing() can't enable motion blur if the feature isn't supported"
		))

	gotoIfError2(clean, RefPtr_createx(
		(U32)(sizeof(Pipeline) + PipelineExt_size + sizeof(PipelineRaytracingInfo)),
		(ObjectFreeFunc) Pipeline_free,
		(ETypeId) EGraphicsTypeId_Pipeline,
		pipelineRef
	))

	madePipeline = true;
	Pipeline *pipeline = PipelineRef_ptr(*pipelineRef);

	GraphicsDeviceRef_inc(deviceRef);

	*pipeline = (Pipeline) { .device = deviceRef, .type = EPipelineType_RaytracingExt };

	PipelineRaytracingInfo *dstInfo = Pipeline_info(pipeline, PipelineRaytracingInfo);
	*dstInfo = info;

	pipeline->stages = tmpStages;
	tmpStages = (ListPipelineStage) { 0 };

	if(ListPipelineRaytracingGroup_isRef(*groups))
		gotoIfError2(clean, ListPipelineRaytracingGroup_createCopyx(*groups, &dstInfo->groups))

	else dstInfo->groups = *groups;

	*groups = (ListPipelineRaytracingGroup) { 0 };

	gotoIfError3(clean, GraphicsDevice_createPipelineRaytracingInternalExt(
		deviceRef,
		binaries,
		name,
		maxPayloadSize,
		maxAttributeSize,
		binaryIndices,
		pipeline,
		e_rr
	))

clean:

	ListPipelineStage_freex(&tmpStages);
	ListU32_freex(&binaryIndices);

	if(!s_uccess && madePipeline)
		RefPtr_dec(pipelineRef);

	return s_uccess;
}
