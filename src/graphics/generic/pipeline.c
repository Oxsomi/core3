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
#include "graphics/generic/device_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"

const C8 *EPipelineStage_names[] = {
	"vertex",
	"pixel",
	"compute",
	"geometry",
	"hull",
	"domain",
	"raygeneration",
	"callable",
	"miss",
	"closesthit",
	"anyhit",
	"intersection"
};

TListImpl(PipelineStage);
TListNamedImpl(ListPipelineRef);

Error PipelineRef_dec(PipelineRef **pipeline) {
	return !RefPtr_dec(pipeline) ?
		Error_invalidOperation(0, "PipelineRef_dec():: pipeline invalid") : Error_none();
}

Error PipelineRef_inc(PipelineRef *pipeline) {
	return !RefPtr_inc(pipeline) ?
		Error_invalidOperation(0, "PipelineRef_inc()::pipeline invalid") : Error_none();
}

Bool PipelineRef_decAll(ListPipelineRef *list) {

	if(!list)
		return true;

	Bool success = true;

	for(U64 i = 0; i < list->length; ++i)
		success &= !PipelineRef_dec(&list->ptrNonConst[i]).genericError;

	ListPipelineRef_freex(list);
	return success;
}

impl Bool Pipeline_freeExt(Pipeline *pipeline, Allocator alloc);

Bool Pipeline_free(Pipeline *pipeline, Allocator alloc) {

	Pipeline_freeExt(pipeline, alloc);

	if (pipeline->type == EPipelineType_RaytracingExt) {

		PipelineRaytracingInfo *info = Pipeline_info(pipeline, PipelineRaytracingInfo);

		for (U64 i = 0; i < info->binaries.length; ++i)
			Buffer_freex(&info->binaries.ptrNonConst[i]);

		ListBuffer_freex(&info->binaries);

		for (U64 i = 0; i < info->entrypoints.length; ++i)
			CharString_freex(&info->entrypoints.ptrNonConst[i]);

		ListCharString_freex(&info->entrypoints);

		ListPipelineRaytracingGroup_freex(&info->groups);

		DeviceBufferRef_dec(&info->shaderBindingTable);
	}

	for (U64 i = 0; i < pipeline->stages.length; ++i)
		Buffer_freex(&pipeline->stages.ptrNonConst[i].binary);

	ListPipelineStage_freex(&pipeline->stages);
	GraphicsDeviceRef_dec(&pipeline->device);
	return true;
}
