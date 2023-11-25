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
