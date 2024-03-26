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
#include "platforms/ext/ref_ptrx.h"

impl Error GraphicsDevice_createPipelinesComputeExt(GraphicsDevice *device, ListCharString names, ListPipelineRef *pipelines);

Error GraphicsDeviceRef_createPipelinesCompute(
	GraphicsDeviceRef *deviceRef,
	ListBuffer *shaderBinaries,
	ListCharString names,			//Temporary names for debugging. Can be empty too, else match shaderBinaries->length
	ListPipelineRef *pipelines
) {

	if(!deviceRef || !shaderBinaries || !pipelines)
		return Error_nullPointer(
			!deviceRef ? 0 : (!shaderBinaries ? 1 : 2),
			"GraphicsDeviceRef_createPipelinesCompute()::deviceRef, shaderBinaries and pipelines are required"
		);

	if(deviceRef->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_createPipelinesCompute()::deviceRef is an invalid type"
		);

	if(!shaderBinaries->length)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_createPipelinesCompute()::shaderBinaries should be of length > 0"
		);

	if(names.length && names.length != shaderBinaries->length)
		return Error_invalidParameter(
			2, 0,
			"GraphicsDeviceRef_createPipelinesCompute()::names should have the same length as shaderBinaries"
		);

	if(shaderBinaries->length >> 32)
		return Error_outOfBounds(
			1, shaderBinaries->length, U32_MAX,
			"GraphicsDeviceRef_createPipelinesCompute()::shaderBinaries.length should be less than U32_MAX"
		);

	if(pipelines->ptr)
		return Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelinesCompute()::pipelines->ptr is non zero, indicating a possible memleak"
		);

	Error err = Error_none();
	gotoIfError(clean, ListPipelineRef_resizex(pipelines, shaderBinaries->length))

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	for (U64 i = 0; i < pipelines->length; ++i) {

		RefPtr **refPtr = &pipelines->ptrNonConst[i];

		gotoIfError(clean, RefPtr_createx(
			(U32)(sizeof(Pipeline) + PipelineExt_size),
			(ObjectFreeFunc) Pipeline_free,
			(ETypeId) EGraphicsTypeId_Pipeline,
			refPtr
		))

		Pipeline *pipeline = PipelineRef_ptr(*refPtr);

		GraphicsDeviceRef_inc(deviceRef);

		*pipeline = (Pipeline) { .device = deviceRef, .type = EPipelineType_Compute };
		gotoIfError(clean, ListPipelineStage_resizex(&pipeline->stages, 1))

		pipeline->stages.ptrNonConst[0] = (PipelineStage) {
			.stageType = EPipelineStage_Compute,
			.binary = shaderBinaries->ptr[i]
		};

		shaderBinaries->ptrNonConst[i] = Buffer_createNull();		//Moved
	}

	ListBuffer_freex(shaderBinaries);

	gotoIfError(clean, GraphicsDevice_createPipelinesComputeExt(device, names, pipelines))

	goto success;

clean:

	for (U64 i = 0; i < pipelines->length; ++i)
		RefPtr_dec((RefPtr**)&pipelines->ptr[i]);

	ListPipelineRef_freex(pipelines);

success:
	return err;
}
