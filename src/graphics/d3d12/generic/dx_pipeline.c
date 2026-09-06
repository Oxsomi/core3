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

//graphics/d3d12/generic/dx_pipeline.c

#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_amd_shader_analyzer.h"
#include "types/base/string_read_helper.h"

void DX_WRAP_FUNC(Pipeline_free)(Pipeline *pipeline, const Allocator *alloc) {

	(void)alloc;

	if(!pipeline)
		return;

	const DxPipeline *dxPipeline = Pipeline_ext(pipeline, Dx);

	if(!dxPipeline->pso)
		return;

	if(pipeline->type == EPipelineType_RaytracingExt) {

		if(dxPipeline->stateObjectProps)
			dxPipeline->stateObjectProps->lpVtbl->Release(dxPipeline->stateObjectProps);

		dxPipeline->stateObject->lpVtbl->Release(dxPipeline->stateObject);
	}

	else dxPipeline->pso->lpVtbl->Release(dxPipeline->pso);
}

//D3D12 exposes nothing of its own here, so this is AMD's extension or nothing; the pipeline had to be created
// with EPipelineFlags_CaptureISA for a handle to exist.

Bool DX_WRAP_FUNC(Pipeline_getExecutables)(
	Pipeline *pipeline,
	const Allocator *alloc,
	ListPipelineExecutable *result,
	Error *e_rr
) {

	Bool s_uccess = true;

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);
	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	const DxPipeline *dxPipeline = Pipeline_ext(pipeline, Dx);

	if(!deviceExt->amdAnalyzer.analyzer)
		retError(clean, Error_unsupportedOperation(
			0, "DxPipeline_getExecutables() needs AMD's D3D12 shader analyzer extension, which this driver lacks"
		));

	if(!dxPipeline->amdAnalyzerHandle)
		retError(clean, Error_invalidOperation(
			0, "DxPipeline_getExecutables() the pipeline wasn't created with EPipelineFlags_CaptureISA"
		));

	gotoIfError3(clean, DxAmdShaderAnalyzer_getExecutables(
		&deviceExt->amdAnalyzer, dxPipeline->amdAnalyzerHandle,
		pipeline->type, alloc, result, e_rr
	));

clean:
	return s_uccess;
}

//AMD's driver compiles for a whole generation, not only the installed ASIC, and reports those as virtual GPUs.

Bool DX_WRAP_FUNC(GraphicsDeviceRef_listShaderTargets)(
	GraphicsDeviceRef *deviceRef, const Allocator *alloc, ListCharString *result, Error *e_rr
) {

	Bool s_uccess = true;
	ListDxAmdVirtualGpu gpus = (ListDxAmdVirtualGpu) { 0 };

	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(deviceRef), Dx);

	if(!deviceExt->amdAnalyzer.analyzer)
		goto clean;

	gotoIfError3(clean, DxAmdShaderAnalyzer_listVirtualGpus(&deviceExt->amdAnalyzer, alloc, &gpus, e_rr));
	gotoIfError3(clean, ListCharString_resize(result, gpus.length, alloc, e_rr));

	for(U64 i = 0; i < gpus.length; ++i)
		gotoIfError3(clean, CharString_createCopy(gpus.ptr[i].name, alloc, &result->ptrNonConst[i], e_rr));

clean:

	for(U64 i = 0; i < gpus.length; ++i)
		CharString_free(&gpus.ptrNonConst[i].name, alloc);

	ListDxAmdVirtualGpu_free(&gpus, alloc);
	return s_uccess;
}

//The driver reads the choice while initializing the adapter, so this device is not the one it applies to:
//it is only what the name is resolved against, since the id belongs to the driver rather than to the name.

Bool DX_WRAP_FUNC(GraphicsDeviceRef_selectShaderTarget)(
	GraphicsDeviceRef *deviceRef, const CharString *name, Error *e_rr
) {

	Bool s_uccess = true;
	ListDxAmdVirtualGpu gpus = (ListDxAmdVirtualGpu) { 0 };

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);
	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(deviceRef), Dx);

	if (!name || !CharString_length(*name)) {

		if(!DxAmdShaderAnalyzer_clearVirtualGpu())
			retError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_selectShaderTarget() couldn't clear the driver's target"
			));

		goto clean;
	}

	if(!deviceExt->amdAnalyzer.analyzer)
		retError(clean, Error_unsupportedOperation(
			0,
			"GraphicsDeviceRef_selectShaderTarget() needs AMD's D3D12 shader analyzer extension, which this driver lacks"
		));

	gotoIfError3(clean, DxAmdShaderAnalyzer_listVirtualGpus(&deviceExt->amdAnalyzer, alloc, &gpus, e_rr));

	for (U64 i = 0; i < gpus.length; ++i) {

		if(!CharString_equalsStringInsensitive(&gpus.ptr[i].name, name))
			continue;

		if(!DxAmdShaderAnalyzer_selectVirtualGpu(gpus.ptr[i].gpuId))
			retError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_selectShaderTarget() couldn't hand the target to the driver"
			));

		goto clean;
	}

	retError(clean, Error_notFound(
		1, 0, "GraphicsDeviceRef_selectShaderTarget()::name isn't one this driver reported (see listShaderTargets)"
	));

clean:

	for(U64 i = 0; i < gpus.length; ++i)
		CharString_free(&gpus.ptrNonConst[i].name, alloc);

	ListDxAmdVirtualGpu_free(&gpus, alloc);
	return s_uccess;
}
