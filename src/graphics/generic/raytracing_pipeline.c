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

TListImpl(PipelineRaytracingGroup);
TListImpl(PipelineRaytracingInfo);

impl Error GraphicsDevice_createPipelinesRaytracingInternalExt(
	GraphicsDeviceRef *deviceRef,
	ListCharString names,
	ListPipelineRef *pipelines,
	U64 stageCounter,
	U64 binaryCounter,
	U64 groupCounter
);

Error GraphicsDeviceRef_createPipelineRaytracingExt(
	GraphicsDeviceRef *device,
	ListPipelineStage stages,
	ListBuffer *binaries,
	ListPipelineRaytracingGroup groups,
	ListPipelineRaytracingInfo infos,
	ListCharString *entrypoints,
	ListCharString names,					//Temporary names for debugging. Can be empty, else match infos->length
	ListPipelineRef *pipelines
) {

	//Validate sizes

	if(!device || !binaries || !pipelines)
		return Error_nullPointer(
			!device ? 0 : (!binaries ? 2 : 7),
			"GraphicsDeviceRef_createPipelinesRaytracing()::device, binaries, entrypoints and pipelines are required"
		);

	if(device->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_createPipelinesRaytracing()::deviceRef is an invalid type"
		);

	if(!binaries->length || !stages.length || !infos.length)
		return Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelinesRaytracing()::binaries, stages and infos should be of length > 0"
		);

	if(names.length && names.length != infos.length)
		return Error_invalidParameter(
			2, 0,
			"GraphicsDeviceRef_createPipelinesRaytracing()::names should have the same length as infos"
		);

	if(entrypoints && entrypoints->length != stages.length)
		return Error_invalidParameter(
			2, 0,
			"GraphicsDeviceRef_createPipelinesRaytracing()::entrypoints should have the same length as infos"
		);

	if(infos.length >> 32)
		return Error_outOfBounds(
			1, infos.length, U32_MAX,
			"GraphicsDeviceRef_createPipelinesRaytracing()::infos.length should be less than U32_MAX"
		);

	if(pipelines->ptr)
		return Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelinesRaytracing()::pipelines->ptr is non zero, indicating a possible memleak"
		);

	//Validate infos

	for(U64 i = 0; i < binaries->length; ++i)
		if(!Buffer_length(binaries->ptr[i]))
			return Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelinesRaytracing()::binaries[i] is required"
			);

	for(U64 i = 0; i < stages.length; ++i)
		if(stages.ptr[i].stageType < EPipelineStage_RtStart || stages.ptr[i].stageType > EPipelineStage_RtEnd)
			return Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelinesRaytracing()::stageType is not RT capable"
			);

	U64 binaryCounter = 0, stageCounter = 0, groupCounter = 0;
	Bool anyMotionBlurExt = false;

	for(U64 j = 0; j < infos.length; ++j) {

		PipelineRaytracingInfo info = infos.ptr[j];

		anyMotionBlurExt |= info.flags & EPipelineRaytracingFlags_AllowMotionBlurExt;

		if(binaryCounter + info.binaryCount > binaries->length)
			return Error_outOfBounds(
				1, binaryCounter + info.binaryCount, binaries->length,
				"GraphicsDeviceRef_createPipelinesRaytracing()::binaries[i] out of bounds"
			);

		if(stageCounter + info.stageCount > stages.length)
			return Error_outOfBounds(
				1, stageCounter + info.stageCount, stages.length,
				"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i] out of bounds"
			);

		if(groupCounter + info.groupCount > groups.length)
			return Error_outOfBounds(
				1, groupCounter + info.groupCount, groups.length,
				"GraphicsDeviceRef_createPipelinesRaytracing()::groups[i] out of bounds"
			);

		if(((U64)info.groupCount + info.stageCount) >> 32)
			return Error_outOfBounds(
				1, info.groupCount + info.stageCount, U32_MAX,
				"GraphicsDeviceRef_createPipelinesRaytracing()::stageCount + groupCount out of bounds"
			);

		if(info.flags >> EPipelineRaytracingFlags_Count)
			return Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createPipelinesRaytracing()::info[i].flags is invalid"
			);

		if((info.maxPayloadSize & 3) || (info.maxAttributeSize & 3))
			return Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelinesRaytracing()::info[i].maxPayloadSize and maxAttributeSize "
				"need to be 4 byte aligned"
			);

		if(info.maxAttributeSize > 32 || info.maxRecursionDepth > 2 || info.maxPayloadSize > 32)
			return Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelinesRaytracing()::info[i]."
				"maxAttributeSize, maxRecursionDepth and maxRayHitAttributeSize need to be <=32, <=2 and <=32 respectively"
			);

		if(info.maxAttributeSize < 8 || info.maxPayloadSize < 4 || !info.maxRecursionDepth)
			return Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelinesRaytracing()::info[i].maxAttributeSize, "
				"maxPayloadSize and maxRecursionDepth need to be >=8, >=4 and >=1 respectively"
			);

		if(info.binaries.ptr || info.groups.length)
			return Error_invalidParameter(
				1, 0,
				"GraphicsDeviceRef_createPipelinesRaytracing()::info[i].binaries and groups should be NULL"
			);

		//Validate stages

		for (U64 i = 0; i < info.stageCount; ++i) {

			PipelineStage stage = stages.ptr[stageCounter + i];

			if (Buffer_length(stage.binary))
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].shaderBinary "
					"is not allowed for raytracing shaders"
				);

			if(
				stage.binaryId == U32_MAX && stage.stageType == EPipelineStage_MissExt &&
				!(info.flags & EPipelineRaytracingFlags_NoNullMiss)
			)
				continue;

			if(stage.binaryId == U32_MAX)
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::info[i].binaryId is U32_MAX (unused) "
					"but it's not allowed"
				);

			if(stage.binaryId >= info.binaryCount)
				return Error_outOfBounds(
					1, stage.binaryId, info.binaryCount,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].binaryId out of bounds"
				);
		}

		//Validate stages

		for (U64 i = 0; i < info.groupCount; ++i) {

			PipelineRaytracingGroup group = groups.ptr[groupCounter + i];

			//Validate with flags

			if(group.intersection != U32_MAX && (info.flags & EPipelineRaytracingFlags_SkipAABBs))
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::groups[i].intersection is "
					"disallowed if aabbs are skipped"
				);

			if(group.anyHit == U32_MAX && (info.flags & EPipelineRaytracingFlags_NoNullAnyHit))
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::groups[i].anyHit is null, but "
					"NoNullAnyHit is used"
				);

			if(group.closestHit == U32_MAX && (info.flags & EPipelineRaytracingFlags_NoNullClosestHit))
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::groups[i].closestHit is null, but "
					"NoNullClosestHit is used"
				);

			//Validate bounds

			if(group.anyHit != U32_MAX && group.anyHit >= info.stageCount)
				return Error_outOfBounds(
					1, group.anyHit, info.stageCount,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].anyHit out of bounds"
				);

			if(group.closestHit != U32_MAX && group.closestHit >= info.stageCount)
				return Error_outOfBounds(
					1, group.closestHit, info.stageCount,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].closestHit out of bounds"
				);

			if(group.intersection != U32_MAX && group.intersection >= info.stageCount)
				return Error_outOfBounds(
					1, group.intersection, info.stageCount,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].intersection out of bounds"
				);

			//Validate pipeline type

			if(group.anyHit != U32_MAX && stages.ptr[group.anyHit + i].stageType != EPipelineStage_AnyHitExt)
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].anyHit pointed to "
					"the wrong pipeline stage"
				);

			if(group.closestHit != U32_MAX && stages.ptr[group.closestHit + i].stageType != EPipelineStage_ClosestHitExt)
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].closestHit pointed to "
					"the wrong pipeline stage"
				);

			if(group.intersection != U32_MAX && stages.ptr[group.intersection + i].stageType != EPipelineStage_IntersectionExt)
				return Error_invalidParameter(
					1, 0,
					"GraphicsDeviceRef_createPipelinesRaytracing()::stages[i].intersection pointed to "
					"the wrong pipeline stage"
				);
		}

		stageCounter += info.stageCount;
		binaryCounter += info.binaryCount;
		groupCounter += info.groupCount;
	}

	//Compare counters with data

	if(binaries->length != binaryCounter)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelinesRaytracing()::binaries has leftover elements"
		);

	if(stages.length != stageCounter)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelinesRaytracing()::stages has leftover elements"
		);

	if(groups.length != groupCounter)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelinesRaytracing()::groups has leftover elements"
		);

	stageCounter = binaryCounter = groupCounter = 0;

	//Check for raytracing extension(s)

	GraphicsDevice *dev = GraphicsDeviceRef_ptr(device);

	if(!(dev->info.capabilities.features & EGraphicsFeatures_RayPipeline))
		return Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelinesRaytracing() can't be called if RayPipeline isn't supported"
		);

	if(anyMotionBlurExt && !(dev->info.capabilities.features & EGraphicsFeatures_RayMotionBlur))
		return Error_invalidParameter(
			1, 0,
			"GraphicsDeviceRef_createPipelinesRaytracing() can't enable motion blur if the feature isn't supported"
		);

	Error err = Error_none();
	gotoIfError(clean, ListPipelineRef_resizex(pipelines, infos.length))

	for (U64 i = 0; i < pipelines->length; ++i) {

		PipelineRaytracingInfo info = infos.ptr[i];
		RefPtr **refPtr = &pipelines->ptrNonConst[i];

		gotoIfError(clean, RefPtr_createx(
			(U32)(sizeof(Pipeline) + PipelineExt_size + sizeof(PipelineRaytracingInfo)),
			(ObjectFreeFunc) Pipeline_free,
			(ETypeId) EGraphicsTypeId_Pipeline,
			refPtr
		))

		Pipeline *pipeline = PipelineRef_ptr(*refPtr);

		GraphicsDeviceRef_inc(device);

		*pipeline = (Pipeline) { .device = device, .type = EPipelineType_RaytracingExt };

		PipelineRaytracingInfo *dstInfo = Pipeline_info(pipeline, PipelineRaytracingInfo);
		*dstInfo = info;

		gotoIfError(clean, ListPipelineStage_createCopySubsetx(stages, stageCounter, info.stageCount, &pipeline->stages))
		gotoIfError(clean, ListBuffer_createx(info.binaryCount, &dstInfo->binaries))
		gotoIfError(clean, ListCharString_createx(info.stageCount, &dstInfo->entrypoints))
		gotoIfError(clean, ListPipelineRaytracingGroup_createCopySubsetx(
			groups, groupCounter, info.groupCount, &dstInfo->groups
		))

		for(U64 j = 0; j < info.binaryCount; ++j) {

			Buffer *buf = &binaries->ptrNonConst[binaryCounter + j];

			//Move or copy, depending on what's relevant

			if(Buffer_isRef(*buf))
				gotoIfError(clean, Buffer_createCopyx(*buf, &dstInfo->binaries.ptrNonConst[j]))

			else dstInfo->binaries.ptrNonConst[j] = *buf;

			*buf = Buffer_createNull();		//Moved
		}

		if(entrypoints)
			for (U64 j = 0; j < info.stageCount; ++j) {

				CharString *name = &entrypoints->ptrNonConst[stageCounter + j];

				if(name->ptr && (CharString_isRef(*name) || !CharString_isNullTerminated(*name)))
					gotoIfError(clean, CharString_createCopyx(*name, &dstInfo->entrypoints.ptrNonConst[j]))

				else dstInfo->entrypoints.ptrNonConst[j] = *name;

				*name = CharString_createNull();
			}

		stageCounter += info.stageCount;
		binaryCounter += info.binaryCount;
		groupCounter += info.groupCount;
	}

	ListBuffer_freex(binaries);
	ListCharString_freex(entrypoints);

	gotoIfError(clean, GraphicsDevice_createPipelinesRaytracingInternalExt(
		device, names, pipelines, stageCounter, binaryCounter, groupCounter
	))

	goto success;

clean:

	for (U64 i = 0; i < pipelines->length; ++i)
		RefPtr_dec((RefPtr**)&pipelines->ptr[i]);

	ListPipelineRef_freex(pipelines);

success:
	return err;
}
