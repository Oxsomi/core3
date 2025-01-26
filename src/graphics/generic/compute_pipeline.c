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
#include "platforms/ext/ref_ptrx.h"
#include "platforms/log.h"
#include "formats/oiSH/sh_file.h"

Bool GraphicsDeviceRef_createPipelineCompute(
	GraphicsDeviceRef *deviceRef,
	SHFile shaderBinary,
	CharString name,			//Temporary name for debugging
	U32 entryId,
	PipelineRef **pipeline,
	Error *e_rr
) {

	Bool s_uccess = true;
	U16 entrypointId = (U16) entryId;
	U16 binaryId = (U16) (entryId >> 16);

	if(!deviceRef || entrypointId >= shaderBinary.entries.length || !pipeline)
		retError(clean, Error_nullPointer(
			!deviceRef ? 0 : (!pipeline ? 2 : 1),
			"GraphicsDeviceRef_createPipelineCompute()::deviceRef, shaderBinary and pipeline are required"
		))

	if(deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_createPipelineCompute()::deviceRef is an invalid type"
		))

	if(*pipeline)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineCompute()::*pipeline is non NULL, indicating a possible memleak"
		))

	SHEntry entry = shaderBinary.entries.ptr[entrypointId];

	if(entry.stage != ESHPipelineStage_Compute)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineCompute() entry is not a compute shader"
		))

	if(binaryId >= entry.binaryIds.length)
		retError(clean, Error_invalidParameter(
			3, 0,
			"GraphicsDeviceRef_createPipelineCompute() entry binaryId out of bounds"
		))

	U32 finalBinaryId = entry.binaryIds.ptr[binaryId];
	SHBinaryInfo binary = shaderBinary.binaries.ptr[finalBinaryId];

	gotoIfError3(clean, GraphicsDeviceRef_checkShaderFeatures(deviceRef, binary, entry, e_rr))

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	gotoIfError2(clean, RefPtr_createx(
		(U32)(sizeof(Pipeline) + GraphicsDeviceRef_getObjectSizes(deviceRef)->pipeline),
		(ObjectFreeFunc) Pipeline_free,
		(ETypeId) EGraphicsTypeId_Pipeline,
		pipeline
	))

	Pipeline *pipelinePtr = PipelineRef_ptr(*pipeline);

	//Log_debugLnx("Create: ComputePipeline %.*s (%p)", (int) CharString_length(name), name.ptr, pipelinePtr);

	GraphicsDeviceRef_inc(deviceRef);

	*pipelinePtr = (Pipeline) { .device = deviceRef, .type = EPipelineType_Compute };

	gotoIfError2(clean, ListPipelineStage_resizex(&pipelinePtr->stages, 1))
	pipelinePtr->stages.ptrNonConst[0] = (PipelineStage) { .stageType = EPipelineStage_Compute, .binaryId = entryId };

	gotoIfError3(clean, GraphicsDevice_createPipelineComputeExt(device, name, pipelinePtr, binary, e_rr))

	goto success;

clean:
	RefPtr_dec(pipeline);

success:
	return s_uccess;
}
