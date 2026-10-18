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

//graphics/d3d12/generic/dx_pipeline_cache.c

#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_internal.h"
#include "graphics/d3d12/dx_device.h"
#include "platforms/logx.h"
#include "types/container/buffer.h"
#include "types/base/error.h"

//A key as the name a library stores it under

static void DxPipeline_keyName(U64 key, wchar_t name[17]) {

	static const wchar_t hex[] = L"0123456789abcdef";

	for(U32 i = 0; i < 16; ++i)
		name[i] = hex[(key >> ((15 - i) * 4)) & 0xF];

	name[16] = 0;
}

Bool DxGraphicsDevice_buildPipeline(
	GraphicsDevice *device,
	Pipeline *pipeline,
	const void *desc,
	U64 key,
	U64 irBytes,
	ID3D12PipelineState **result,
	Error *e_rr
) {

	Bool s_uccess = true;
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	Bool isStored = false;

	const Bool isCompute = pipeline->type == EPipelineType_Compute;

	wchar_t name[17];
	DxPipeline_keyName(key, name);

	//Only what the library holds is asked for, since asking for anything else is a debug layer warning.
	//Under the device lock, since setting a pipeline cache replaces the library. A load can still fail (another
	// description under the same key), and then the PSO is created instead.

	HRESULT loaded = E_FAIL;
	U64 stored = 0;

	if(deviceExt->pipelineLibrary && GraphicsDevice_findPipelineSize(device, key, &stored)) {

		const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);
		ID3D12PipelineLibrary *library = deviceExt->pipelineLibrary;

		loaded = isCompute ?
			library->lpVtbl->LoadComputePipeline(library, name, desc, &IID_ID3D12PipelineState, (void**) result) :
			library->lpVtbl->LoadGraphicsPipeline(library, name, desc, &IID_ID3D12PipelineState, (void**) result);

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->lock);
	}

	if(FAILED(loaded)) {

		gotoIfError3(clean, dxCheck(isCompute ?
			deviceExt->device->lpVtbl->CreateComputePipelineState(
				deviceExt->device, desc, &IID_ID3D12PipelineState, (void**) result
			) :
			deviceExt->device->lpVtbl->CreateGraphicsPipelineState(
				deviceExt->device, desc, &IID_ID3D12PipelineState, (void**) result
			),
			e_rr
		));

		//Stored for the next run; a name already there (the same pipeline built twice this run) is simply refused

		if(deviceExt->pipelineLibrary) {

			const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);
			ID3D12PipelineLibrary *library = deviceExt->pipelineLibrary;

			isStored = SUCCEEDED(library->lpVtbl->StorePipeline(library, name, *result));

			if(isStored)
				++deviceExt->pipelineLibraryStored;

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(&device->lock);
		}
	}

	DxPipeline_trackMemory(pipeline, *result, irBytes);

	if(isStored)
		gotoIfError3(clean, GraphicsDevice_recordPipelineSize(device, key, pipeline->gpuBytes, e_rr));

clean:
	return s_uccess;
}

Bool DX_WRAP_FUNC(GraphicsDevice_loadPipelineCache)(GraphicsDevice *device, Buffer driverData, Bool *accepted, Error *e_rr) {

	Bool s_uccess = true;
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	const Allocator *alloc = GraphicsDevice_getAlloc(device);
	Buffer copy = Buffer_createNull();
	ID3D12PipelineLibrary *library = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	*accepted = false;

	if(!Buffer_length(driverData)) {
		*accepted = true;
		goto clean;
	}

	if(!deviceExt->pipelineLibrary)
		goto clean;

	acq = SpinLock_lock(&device->lock, U64_MAX);

	//A library can't merge another, so opening this one would drop what this run stored already

	if(deviceExt->pipelineLibraryStored) {
		Log_debugLnx("D3D12GraphicsDevice_loadPipelineCache() ignored a pipeline cache set after PSOs were built");
		goto clean;
	}

	//The library reads the blob for as long as it lives, so it gets a copy of its own

	gotoIfError3(clean, Buffer_createCopy(driverData, alloc, &copy, e_rr));

	//Another driver or adapter's blob is refused here (D3D12_ERROR_DRIVER_VERSION_MISMATCH, ADAPTER_NOT_FOUND)

	if(FAILED(deviceExt->device->lpVtbl->CreatePipelineLibrary(
		deviceExt->device, copy.ptr, Buffer_length(copy), &IID_ID3D12PipelineLibrary, (void**) &library
	)))
		goto clean;

	deviceExt->pipelineLibrary->lpVtbl->Release(deviceExt->pipelineLibrary);
	Buffer_free(&deviceExt->pipelineLibraryData, alloc);

	deviceExt->pipelineLibrary = library;
	deviceExt->pipelineLibraryData = copy;
	library = NULL;
	copy = Buffer_createNull();

	*accepted = true;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(library)
		library->lpVtbl->Release(library);

	Buffer_free(&copy, alloc);
	return s_uccess;
}

Bool DX_WRAP_FUNC(GraphicsDevice_savePipelineCache)(
	GraphicsDevice *device,
	const Allocator *alloc,
	Buffer *driverData,
	Error *e_rr
) {

	Bool s_uccess = true;
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!deviceExt->pipelineLibrary)
		goto clean;

	acq = SpinLock_lock(&device->lock, U64_MAX);

	const U64 size = deviceExt->pipelineLibrary->lpVtbl->GetSerializedSize(deviceExt->pipelineLibrary);

	if(!size)
		goto clean;

	gotoIfError3(clean, Buffer_createUninitializedBytes(size, alloc, driverData, e_rr));

	gotoIfError3(clean, dxCheck(deviceExt->pipelineLibrary->lpVtbl->Serialize(
		deviceExt->pipelineLibrary, (void*) driverData->ptr, size
	), e_rr));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(!s_uccess)
		Buffer_free(driverData, alloc);

	return s_uccess;
}
