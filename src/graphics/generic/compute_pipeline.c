/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//graphics/generic/compute_pipeline.c

#include "graphics/generic/interface.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/device.h"
#include "types/container/ref_ptr.h"
#include "formats/oiSH/sh_file.h"

Bool GraphicsDeviceRef_createPipelineCompute(
	GraphicsDeviceRef *deviceRef,
	const SHFile *shaderBinary,
	const CharString *name,
	U32 entryId,
	const CharString *entryName,
	EPipelineFlags flags,
	PipelineLayoutRef *layout,
	PipelineRef **pipeline,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	U16 entrypointId = (U16) entryId;
	U16 binaryId = (U16) (entryId >> 16);
	Bool allocated = false;

	if(!deviceRef || !shaderBinary || entrypointId >= shaderBinary->entries.length || !pipeline)
		retError(clean, Error_nullPointer(
			!deviceRef ? 0 : (!pipeline ? 2 : 1),
			"GraphicsDeviceRef_createPipelineCompute()::deviceRef, shaderBinary and pipeline are required"
		));

	if(deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_createPipelineCompute()::deviceRef is an invalid type"
		));

	if(*pipeline)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineCompute()::*pipeline is non NULL, indicating a possible memleak"
		));

	if(!SHFile_isComplete(shaderBinary))
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createPipelineCompute()::shaderBinary is reflection-only (has no compiled binary)"
		));

	const SHEntry *entry = &shaderBinary->entries.ptr[entrypointId];

	if(entry->stage != EGfxPipelineStage_Compute)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineCompute() entry is not a compute shader"
		));

	if(binaryId >= entry->binaryIds.length)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineCompute() entry binaryId out of bounds"
		));

	if(layout && layout->refPtrType->typeId != (TypeId) EGraphicsTypeId_PipelineLayout)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineCompute() pipeline layout is invalid"
		));

	U32 finalBinaryId = entry->binaryIds.ptr[binaryId];
	const SHBinaryInfo *binary = &shaderBinary->binaries.ptr[finalBinaryId];

	gotoIfError3(clean, GraphicsDeviceRef_checkShaderFeatures(deviceRef, binary, entry, e_rr));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(deviceRef)->pipelineCompute, pipeline, e_rr));
	allocated = true;

	Pipeline *pipelinePtr = PipelineRef_ptr(*pipeline);

	if(!(flags & EPipelineFlags_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(deviceRef));

	*pipelinePtr = (Pipeline) {
		.device = deviceRef,
		.type = EPipelineType_Compute,
		.flags = flags,
		.extensions = (U32)(binary->identifier.extensions &~ binary->dormantExtensions)
	};

	if(!layout)
		layout = device->defaultPipelineLayout;

	//A device without bindless has no default pipeline layout, and the backends dereference it unconditionally.
	//EGraphicsDeviceFlags_DisableBindless puts any device in that state, so the pipeline has to bring its own.

	if(!layout)
		retError(clean, Error_nullPointer(
			6,
			"GraphicsDeviceRef_createPipelineCompute()::layout is required, "
			"the device has no default pipeline layout because it has no bindless"
		));

	if(!(flags & EPipelineFlags_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(layout));

	pipelinePtr->layout = layout;

	gotoIfError3(clean, ListPipelineStage_resize(&pipelinePtr->stages, 1, alloc, e_rr));
	pipelinePtr->stages.ptrNonConst[0] = (PipelineStage) { .stageType = EPipelineStage_Compute, .binaryId = entryId };

	gotoIfError3(clean, GraphicsDevice_createPipelineComputeExt(device, name, entryName, pipelinePtr, binary, e_rr));

	goto success;

clean:
	if(allocated)
		RefPtr_dec(pipeline);

success:
	return s_uccess;
}
