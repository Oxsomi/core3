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

//graphics/d3d12/generic/dx_device.c

#define INITGUID
#include <guiddef.h>
#include "types/base/platform_types.h"
#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/d3d12/direct3d12.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_swapchain.h"
#include "graphics/d3d12/dx_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/pipeline_layout.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/base/thread.h"
#include "types/base/constants.h"
#include "types/container/string_unicode.h"

#if _ARCH == ARCH_X86_64
	#include <nvapi.h>
#endif

void onDebugReport(
	D3D12_MESSAGE_CATEGORY category,
	D3D12_MESSAGE_SEVERITY severity,
	D3D12_MESSAGE_ID id,
	LPCSTR description,
	void *context
) {

	(void) context;

	const C8 *categoryStr = "Undefined";

	switch(category) {
		default:                                                                                    break;
		case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:    categoryStr = "Application defined";    break;
		case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:            categoryStr = "Misc";                    break;
		case D3D12_MESSAGE_CATEGORY_INITIALIZATION:            categoryStr = "Initialization";            break;
		case D3D12_MESSAGE_CATEGORY_CLEANUP:                categoryStr = "Cleanup";                break;
		case D3D12_MESSAGE_CATEGORY_COMPILATION:            categoryStr = "Compilation";            break;
		case D3D12_MESSAGE_CATEGORY_STATE_CREATION:            categoryStr = "State creation";            break;
		case D3D12_MESSAGE_CATEGORY_STATE_SETTING:            categoryStr = "State setting";            break;
		case D3D12_MESSAGE_CATEGORY_STATE_GETTING:            categoryStr = "State getting";            break;
		case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:    categoryStr = "Resource manipulation";    break;
		case D3D12_MESSAGE_CATEGORY_EXECUTION:                categoryStr = "Execution";                break;
		case D3D12_MESSAGE_CATEGORY_SHADER:                    categoryStr = "Shader";                    break;
	}

	switch(severity) {

		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
		case D3D12_MESSAGE_SEVERITY_ERROR:
			Log_errorLnx("Error %"PRIu32" (%s): %s", id, categoryStr, description);
			break;

		case D3D12_MESSAGE_SEVERITY_WARNING:
			Log_warnLnx("Warning %"PRIu32" (%s): %s", id, categoryStr, description);
			break;

		default:
			Log_debugLnx("Debug %"PRIu32" (%s): %s", id, categoryStr, description);
			break;
	}
}

#if _ARCH == ARCH_X86_64

	void onDebugReportNv(
		void *pUserData,
		NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY severity,
		const char *messageCode,
		const char *message,
		const char *messageDetails
	) {

		(void)pUserData;

		switch(severity) {

			case NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY_ERROR:
				Log_errorLnx("NVAPI D3D12 Error %s (%s): %s", messageCode, message, messageDetails);
				break;

			default:
				Log_warnLnx("NVAPI D3D12 Warning %s (%s): %s", messageCode, message, messageDetails);
				break;
		}
	}

#endif

TListImpl(DxCommandAllocator);
TListNamedImpl(ListID3D12Fence);

Bool DxGraphicsDevice_createDescriptorHeapSingle(
	DxGraphicsDevice *deviceExt,
	D3D12_DESCRIPTOR_HEAP_DESC desc,
	CharString *name,
	DxDescriptorHeapSingle *heap,
	Bool reqGpuHandle,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	ListU16 tmpName16 = (ListU16) { 0 };

	if(name->ptr)
		gotoIfError3(clean, CharString_toUTF16(*name, alloc, &tmpName16, e_rr));

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateDescriptorHeap(
		deviceExt->device,
		&desc,
		&IID_ID3D12DescriptorHeap,
		(void**) &heap->heap
	), e_rr));

	if(tmpName16.ptr)
		gotoIfError3(clean, dxCheck(heap->heap->lpVtbl->SetName(heap->heap, tmpName16.ptr), e_rr));

	heap->cpuIncrement = deviceExt->device->lpVtbl->GetDescriptorHandleIncrementSize(deviceExt->device, desc.Type);

	if(!heap->heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap->heap, &heap->cpuHandle))
		retError(clean, Error_nullPointer(0, "D3D12: GetCPUDescriptorHandleForHeapStart() returned NULL"));

	if(!reqGpuHandle)
		goto clean;

	heap->gpuIncrement = heap->cpuIncrement;

	if(!heap->heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(heap->heap, &heap->gpuHandle))
		retError(clean, Error_nullPointer(0, "D3D12: GetGPUDescriptorHandleForHeapStart() returned NULL"));

clean:
	CharString_free(name, alloc);
	ListU16_free(&tmpName16, alloc);
	return s_uccess;
}

Bool DX_WRAP_FUNC(GraphicsDevice_init)(
	const GraphicsInstance *instance,
	const GraphicsDeviceInfo *physicalDevice,
	GraphicsDeviceRef **deviceRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = instance ? instance->alloc : NULL;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	ID3DBlob *errBlob = NULL, *rootSigBlob = NULL;

	//Create device

	const DxGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Dx);

	gotoIfError3(clean, dxCheck(instanceExt->factory->lpVtbl->EnumAdapterByLuid(
		instanceExt->factory, *(const LUID*)&physicalDevice->luid,
		&IID_IDXGIAdapter4, (void**)&deviceExt->adapter4
	), e_rr));

	if(device->info.capabilities.featuresExt & EDxGraphicsFeatures_IndependentDevices)
	{
		gotoIfError3(clean, dxCheck(instanceExt->deviceFactoryNoSingleton->lpVtbl->CreateDevice(
			instanceExt->deviceFactoryNoSingleton,
			(IUnknown*)deviceExt->adapter4, D3D_FEATURE_LEVEL_11_0,
			&IID_ID3D12Device10, (void**) &deviceExt->device
		), e_rr));
	}

	else {
		gotoIfError3(clean, dxCheck(instanceExt->deviceFactorySingleton->lpVtbl->CreateDevice(
		instanceExt->deviceFactorySingleton,
		(IUnknown*)deviceExt->adapter4, D3D_FEATURE_LEVEL_11_0,
		&IID_ID3D12Device10, (void**) &deviceExt->device
	), e_rr));
	}

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->QueryInterface(
		deviceExt->device, &IID_ID3D12DeviceConfiguration1, (void**) &deviceExt->deviceConfig
	), e_rr));

	Bool isNv = device->info.vendor == EGraphicsVendorId_NV;
	(void) isNv;

	if(device->flags & EGraphicsDeviceFlags_IsDebug) {

		gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12DebugDevice, (void**) &deviceExt->debugDevice
		), e_rr));

		//Get infoQueue0 to disable some bogus validation messages

		if(SUCCEEDED(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12InfoQueue, (void**) &deviceExt->infoQueue0
		))) {

			gotoIfError3(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->SetBreakOnSeverity(
				deviceExt->infoQueue0, D3D12_MESSAGE_SEVERITY_CORRUPTION, true
			), e_rr));

			gotoIfError3(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->SetBreakOnSeverity(
				deviceExt->infoQueue0, D3D12_MESSAGE_SEVERITY_ERROR, true
			), e_rr));

			D3D12_MESSAGE_ID hide[] = {
				D3D12_MESSAGE_ID_CREATEDEVICE_DEBUG_LAYER_STARTUP_OPTIONS,
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CREATERESOURCE_INVALIDALIGNMENT    //To check if we allow small alignment
			};

			D3D12_INFO_QUEUE_FILTER filter = (D3D12_INFO_QUEUE_FILTER) {
				.DenyList = (D3D12_INFO_QUEUE_FILTER_DESC) {
					.NumIDs = (U32)(sizeof(hide) / sizeof(hide[0])),
					.pIDList = hide
				}
			};

			gotoIfError3(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->AddStorageFilterEntries(
				deviceExt->infoQueue0, &filter
			), e_rr));
		}

		//Get info queue 1 for validation errors in the console on Win11

		if(SUCCEEDED(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12InfoQueue1, (void**) &deviceExt->infoQueue1
		))) {

			DWORD callbackCookie = 0;

			gotoIfError3(clean, dxCheck(deviceExt->infoQueue1->lpVtbl->RegisterMessageCallback(
				deviceExt->infoQueue1,
				onDebugReport,
				D3D12_MESSAGE_CALLBACK_FLAG_NONE,
				NULL,
				&callbackCookie
			), e_rr));
		}

		#if _ARCH == ARCH_X86_64

			//Nv specific initialization

			if(isNv) {

				//Enable RT validation

				if(device->info.capabilities.features & EGraphicsFeatures_RayValidation) {

					NvAPI_D3D12_EnableRaytracingValidation(
						(ID3D12Device5*)deviceExt->device, NVAPI_D3D12_RAYTRACING_VALIDATION_FLAG_NONE
					);

					void *handle = NULL;
					NvAPI_Status status = NvAPI_D3D12_RegisterRaytracingValidationMessageCallback(
						(ID3D12Device5*)deviceExt->device, onDebugReportNv, NULL, &handle
					);

					if(status != NVAPI_OK)
						retError(clean, Error_invalidState(
							0, "NvAPI_D3D12_RegisterRaytracingValidationMessageCallback couldn't be called"));
				}
			}

		#endif
	}

	static const U32 nvExtSlot = 99999;        //space and u slot

	#if _ARCH == ARCH_X86_64

		//Enable NV extensions

		EGraphicsFeatures nvExt =
			EGraphicsFeatures_RayMicromapOpacity |
			EGraphicsFeatures_RayReorder | EGraphicsFeatures_RayValidation;

		if(isNv && (device->info.capabilities.features & nvExt)) {

			NvAPI_Status status = NvAPI_D3D12_SetNvShaderExtnSlotSpace((IUnknown*)deviceExt->device, nvExtSlot, nvExtSlot);

			if(status != NVAPI_OK)
				retError(clean, Error_invalidState(0, "NvAPI_D3D12_SetNvShaderExtnSlotSpace couldn't be called"));
		}

	#endif

	//Get queues

	deviceExt->queues[EDxCommandQueue_Copy] = (DxCommandQueue) {
		.type = EDxCommandQueue_Copy,
		.resolvedQueueId = 0
	};

	D3D12_COMMAND_QUEUE_DESC queueInfo = (D3D12_COMMAND_QUEUE_DESC) {
		.Type = D3D12_COMMAND_LIST_TYPE_COPY
	};

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandQueue(
		deviceExt->device,
		&queueInfo,
		&IID_ID3D12CommandQueue, (void**) &deviceExt->queues[EDxCommandQueue_Copy].queue
	), e_rr));

	deviceExt->queues[EDxCommandQueue_Compute] = (DxCommandQueue) {
		.type = EDxCommandQueue_Compute,
		.resolvedQueueId = 1
	};

	queueInfo.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandQueue(
		deviceExt->device,
		&queueInfo,
		&IID_ID3D12CommandQueue, (void**) &deviceExt->queues[EDxCommandQueue_Compute].queue
	), e_rr));

	deviceExt->queues[EDxCommandQueue_Graphics] = (DxCommandQueue) {
		.type = EDxCommandQueue_Graphics,
		.resolvedQueueId = 2
	};

	queueInfo.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandQueue(
		deviceExt->device,
		&queueInfo,
		&IID_ID3D12CommandQueue, (void**) &deviceExt->queues[EDxCommandQueue_Graphics].queue
	), e_rr));

	//Create command recorder per queue per thread per backbuffer.
	//We only allow triple buffering, so allocate for triple buffers.
	//These will be initialized JIT because we don't know what thread will be accessing them.

	U64 threads = Platform_getThreads();
	gotoIfError3(clean, ListDxCommandAllocator_resize(&deviceExt->commandPools, 3 * threads * 3, alloc, e_rr));

	//Create fence

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateFence(
		deviceExt->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**) &deviceExt->commitSemaphore
	), e_rr));

	//Create DSVs

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = 1
	};

	CharString tmpName = CharString_createNull();

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		tmpName = CharString_createRefCStrConst("DSV heap");

	gotoIfError3(clean, DxGraphicsDevice_createDescriptorHeapSingle(
		deviceExt, heapDesc, &tmpName, &deviceExt->cpuHeaps[ECPUDescriptorHeapType_DSV], false, alloc, e_rr));

	//Create RTVs

	heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = 8
	};

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		tmpName = CharString_createRefCStrConst("RTV heap");

	gotoIfError3(clean, DxGraphicsDevice_createDescriptorHeapSingle(
		deviceExt, heapDesc, &tmpName, &deviceExt->cpuHeaps[ECPUDescriptorHeapType_RTV], false, alloc, e_rr));

	//Allocate temp storage for transitions

	gotoIfError3(clean, ListD3D12_BUFFER_BARRIER_reserve(&deviceExt->bufferTransitions, 17, alloc, e_rr));
	gotoIfError3(clean, ListD3D12_TEXTURE_BARRIER_reserve(&deviceExt->imageTransitions, 16, alloc, e_rr));

	//Create command signatures for ExecuteIndirect

	D3D12_INDIRECT_ARGUMENT_DESC sigDesc[] = {
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH },
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS },
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED },
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW }
	};

	D3D12_COMMAND_SIGNATURE_DESC signatures[] = {
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DispatchIndirectCmd), .pArgumentDescs = &sigDesc[0] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(D3D12DispatchRaysIndirect), .pArgumentDescs = &sigDesc[1] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallIndexed), .pArgumentDescs = &sigDesc[2] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallUnindexed), .pArgumentDescs = &sigDesc[3] }
	};

	for(U64 i = 0; i < EExecuteIndirectCommand_Count; ++i) {

		if (i == EExecuteIndirectCommand_DispatchRays && !(device->info.capabilities.features & EGraphicsFeatures_RayPipeline))
			continue;

		signatures[i].NumArgumentDescs = 1;

		gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandSignature(
			deviceExt->device,
			&signatures[i],
			NULL,
			&IID_ID3D12CommandSignature, (void**) &deviceExt->commandSigs[i]
		), e_rr));
	}

clean:

	if(errBlob)
		errBlob->lpVtbl->Release(errBlob);

	if(rootSigBlob)
		rootSigBlob->lpVtbl->Release(rootSigBlob);

	if(!s_uccess)
		RefPtr_dec(deviceRef);

	return s_uccess;
}

U64 DX_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(GraphicsDevice *device, Bool isDeviceLocal) {

	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DXGI_QUERY_VIDEO_MEMORY_INFO vidMem = (DXGI_QUERY_VIDEO_MEMORY_INFO) { 0 };
	HRESULT hr = deviceExt->adapter4->lpVtbl->QueryVideoMemoryInfo(
		deviceExt->adapter4,
		0,
		isDeviceLocal ? DXGI_MEMORY_SEGMENT_GROUP_LOCAL : DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,
		&vidMem
	);

	if(FAILED(hr))
		return U64_MAX;

	return vidMem.CurrentUsage;
}

void DX_WRAP_FUNC(GraphicsDevice_free)(const GraphicsInstance *instance, void *ext) {

	if(!instance || !ext)
		return;

	const Allocator *alloc = instance->alloc;

	DxGraphicsDevice *deviceExt = (DxGraphicsDevice*)ext;

	if(deviceExt->device) {

		for(U64 i = 0; i < deviceExt->commandPools.length; ++i) {

			const DxCommandAllocator commandAlloc = deviceExt->commandPools.ptr[i];

			if(commandAlloc.cmd)
				commandAlloc.cmd->lpVtbl->Release(commandAlloc.cmd);

			if(commandAlloc.pool)
				commandAlloc.pool->lpVtbl->Release(commandAlloc.pool);
		}

		if(deviceExt->commitSemaphore)
			deviceExt->commitSemaphore->lpVtbl->Release(deviceExt->commitSemaphore);

		for(U64 i = 0; i < EExecuteIndirectCommand_Count; ++i)
			if(deviceExt->commandSigs[i])
				deviceExt->commandSigs[i]->lpVtbl->Release(deviceExt->commandSigs[i]);

		for(U64 i = 0; i < ECPUDescriptorHeapType_Count; ++i)
			if(deviceExt->cpuHeaps[i].heap)
				deviceExt->cpuHeaps[i].heap->lpVtbl->Release(deviceExt->cpuHeaps[i].heap);

		for(U64 i = 0; i < EDxCommandQueue_Count; ++i)
			if(deviceExt->queues[i].queue)
				deviceExt->queues[i].queue->lpVtbl->Release(deviceExt->queues[i].queue);

		deviceExt->device->lpVtbl->Release(deviceExt->device);
	}

	if(deviceExt->adapter4)
		deviceExt->adapter4->lpVtbl->Release(deviceExt->adapter4);

	if(deviceExt->infoQueue0)
		deviceExt->infoQueue0->lpVtbl->Release(deviceExt->infoQueue0);

	if(deviceExt->infoQueue1)
		deviceExt->infoQueue1->lpVtbl->Release(deviceExt->infoQueue1);

	if(deviceExt->deviceConfig)
		deviceExt->deviceConfig->lpVtbl->Release(deviceExt->deviceConfig);

	if(deviceExt->debugDevice) {

		//Validate exit for leaks

		deviceExt->debugDevice->lpVtbl->ReportLiveDeviceObjects(
			deviceExt->debugDevice, D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL
		);

		deviceExt->debugDevice->lpVtbl->Release(deviceExt->debugDevice);
	}

	ListDxCommandAllocator_free(&deviceExt->commandPools, alloc);

	//Free temp storage

	ListD3D12_BUFFER_BARRIER_free(&deviceExt->bufferTransitions, alloc);
	ListD3D12_TEXTURE_BARRIER_free(&deviceExt->imageTransitions, alloc);
}

//Executing commands

Bool DX_WRAP_FUNC(GraphicsDeviceRef_wait)(GraphicsDeviceRef *deviceRef, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	U64 completedValue = deviceExt->commitSemaphore->lpVtbl->GetCompletedValue(deviceExt->commitSemaphore);

	if (completedValue >= deviceExt->fenceId)
		return s_uccess;

	const HANDLE eventHandle = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);

	gotoIfError3(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
		deviceExt->commitSemaphore, deviceExt->fenceId, eventHandle
	), e_rr));

	WaitForSingleObject(eventHandle, INFINITE);

clean:
	CloseHandle(eventHandle);
	return s_uccess;
}

DxCommandAllocator *DxGraphicsDevice_getCommandAllocator(
	const DxGraphicsDevice *device,
	const U32 resolvedQueueId,
	const U64 threadId,
	const U8 backBufferId
) {

	const U64 threadCount = Platform_getThreads();

	if(!device || resolvedQueueId >= 3 || threadId >= threadCount || backBufferId >= 3)
		return NULL;

	const U64 id = resolvedQueueId + (backBufferId * threadCount + threadId) * 3;

	return device->commandPools.ptrNonConst + id;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

void GraphicsDevice_rebindDescriptors(GraphicsDevice *device, DxCommandBuffer *commandBuffer) {

	//Bind descriptor heaps, root signature and descriptor tables since they stay the same for the entire frame.
	//For every bind point.

	DxDescriptorHeap *heap = DescriptorHeap_ext(DescriptorHeapRef_ptr(device->defaultDescriptorHeaps), Dx);

	ID3D12DescriptorHeap *descriptorHeaps[2] = { heap->resourcesHeap.heap, heap->samplerHeap.heap };

	DxDescriptorTable *table = DescriptorTable_ext(DescriptorTableRef_ptr(device->defaultDescriptorTable), Dx);
	D3D12_GPU_DESCRIPTOR_HANDLE descriptorTable[2] = {
		heap->samplerHeap.gpuHandle.ptr + table->allocationLocations[1] * heap->resourcesHeap.gpuIncrement,
		heap->resourcesHeap.gpuHandle.ptr + table->allocationLocations[0] * heap->resourcesHeap.gpuIncrement
	};

	commandBuffer->lpVtbl->SetDescriptorHeaps(commandBuffer, 2, descriptorHeaps);

	PipelineLayout *defaultLayout = PipelineLayoutRef_ptr(device->defaultPipelineLayout);
	DxPipelineLayout *defaultLayoutExt = PipelineLayout_ext(defaultLayout, Dx);

	commandBuffer->lpVtbl->SetComputeRootSignature(commandBuffer, defaultLayoutExt->rootSig);
	commandBuffer->lpVtbl->SetGraphicsRootSignature(commandBuffer, defaultLayoutExt->rootSig);

	for(U32 i = 0; i < 2; ++i) {
		commandBuffer->lpVtbl->SetComputeRootDescriptorTable(commandBuffer, i, descriptorTable[i]);
		commandBuffer->lpVtbl->SetGraphicsRootDescriptorTable(commandBuffer, i, descriptorTable[i]);
	}

	DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[device->fifId]);
	D3D12_GPU_VIRTUAL_ADDRESS cbvLoc = frameData->resource.deviceAddress;

	Bool isNv = device->info.vendor == EGraphicsVendorId_NV;

	commandBuffer->lpVtbl->SetComputeRootConstantBufferView(commandBuffer, 2 + isNv, cbvLoc);
	commandBuffer->lpVtbl->SetGraphicsRootConstantBufferView(commandBuffer, 2 + isNv, cbvLoc);
}

Bool DX_WRAP_FUNC(GraphicsDevice_submitCommands)(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains,
	CBufferData cbufferData,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	//Unpack ext

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	HANDLE eventHandle = NULL;
	CharString temp = CharString_createNull();
	ListU16 temp16 = (ListU16) { 0 };

	//Wait for previous frame semaphore

	++deviceExt->fenceId;

	if (deviceExt->fenceId > device->framesInFlight) {

		eventHandle = CreateEventExW(NULL, NULL, 0, EVENT_ALL_ACCESS);

		gotoIfError3(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
			deviceExt->commitSemaphore, deviceExt->fenceId - device->framesInFlight, eventHandle
		), e_rr));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
		eventHandle = NULL;
	}

	//Prepare per frame cbuffer

	DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[device->fifId]);

	for (U32 i = 0; i < swapchains.length; ++i) {

		SwapchainRef *swapchainRef = swapchains.ptr[i];
		Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);

		Bool allowComputeExt = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

		UnifiedTextureImage managedImage = TextureRef_getCurrImage(swapchainRef, 0);

		cbufferData.swapchains[i * 2 + 0] = managedImage.readHandle;
		cbufferData.swapchains[i * 2 + 1] = allowComputeExt ? managedImage.writeHandle : 0;
	}

	*(CBufferData*)frameData->resource.mappedMemoryExt = cbufferData;

	//Record command list

	DxCommandBuffer *commandBuffer = NULL;

	DxCommandQueue queue = deviceExt->queues[EDxCommandQueue_Graphics];

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	if (commandLists.length) {

		U32 threadId = 0;

		DxCommandAllocator *allocator = DxGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, device->fifId
		);

		if(!allocator)
			retError(clean, Error_nullPointer(0, "D3D12GraphicsDevice_submitCommands() command allocator is NULL"));

		//We create command pools only the first 3 frames, after that they're cached.
		//This is because we have space for [queues][threads][3] command pools.
		//Allocating them all even though currently only 1x3 are used is quite suboptimal.
		//We only have the space to allow for using more in the future.

		if(!allocator->pool) {

			//TODO: Multi thread command recording

			gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandAllocator(
				deviceExt->device,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				&IID_ID3D12CommandAllocator,
				(void**) &allocator->pool
			), e_rr));

			if(device->flags & EGraphicsDeviceFlags_IsDebug) {

				gotoIfError3(clean, CharString_format(
					alloc, &temp, e_rr, "%s command pool (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EDxCommandQueue_Graphics ? "Graphics" : (
						queue.type == EDxCommandQueue_Compute ? "Compute" : "Copy"
					), threadId, device->fifId));

				gotoIfError3(clean, CharString_toUTF16(temp, alloc, &temp16, e_rr));

				gotoIfError3(clean, dxCheck(allocator->pool->lpVtbl->SetName(allocator->pool, temp16.ptr), e_rr));
				CharString_free(&temp, alloc);
				ListU16_free(&temp16, alloc);
			}
		}

		//Allocate command buffer if not present yet

		Bool isNew = !allocator->cmd;

		if (isNew) {

			gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandList(
				deviceExt->device,
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				allocator->pool,
				NULL,
				&IID_ID3D12GraphicsCommandList10,
				(void**) &allocator->cmd
			), e_rr));

			if(device->flags & EGraphicsDeviceFlags_IsDebug) {

				gotoIfError3(clean, CharString_format(
					alloc, &temp, e_rr, "%s command buffer (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EDxCommandQueue_Graphics ? "Graphics" : (
						queue.type == EDxCommandQueue_Compute ? "Compute" : "Copy"
					), threadId, device->fifId));

				gotoIfError3(clean, CharString_toUTF16(temp, alloc, &temp16, e_rr));
				gotoIfError3(clean, dxCheck(allocator->cmd->lpVtbl->SetName(allocator->cmd, temp16.ptr), e_rr));
				CharString_free(&temp, alloc);
				ListU16_free(&temp16, alloc);
			}
		}

		//Start buffer

		commandBuffer = allocator->cmd;

		if(!isNew) {
			gotoIfError3(clean, dxCheck(allocator->pool->lpVtbl->Reset(allocator->pool), e_rr));
			gotoIfError3(clean, dxCheck(commandBuffer->lpVtbl->Reset(commandBuffer, allocator->pool, NULL), e_rr));
		}

		//Start copies

		DxCommandBufferState state = (DxCommandBufferState) { .buffer = commandBuffer };
		state.boundPrimitiveTopology = U8_MAX;

		gotoIfError3(clean, GraphicsDeviceRef_handleNextFrame(deviceRef, &state, e_rr));

		//Ensure ubo and staging buffer are the correct states

		D3D12_BARRIER_GROUP dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

		DxDeviceBuffer *uboExt = DeviceBuffer_ext(frameData, Dx);

		gotoIfError3(clean, DxDeviceBuffer_transition(
			uboExt,
			D3D12_BARRIER_SYNC_VERTEX_SHADING,
			D3D12_BARRIER_ACCESS_CONSTANT_BUFFER,
			&deviceExt->bufferTransitions,
			&dependency, alloc, e_rr));

		if(dependency.NumBarriers)
			commandBuffer->lpVtbl->Barrier(commandBuffer, 1, &dependency);

		ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);

		GraphicsDevice_rebindDescriptors(device, commandBuffer);

		//Record commands

		for (U64 i = 0; i < commandLists.length; ++i) {

			state.scopeCounter = 0;
			CommandList *commandList = CommandListRef_ptr(commandLists.ptr[i]);
			const U8 *ptr = commandList->data.ptr;

			for (U64 j = 0; j < commandList->commandOps.length; ++j) {
				CommandOpInfo info = commandList->commandOps.ptr[j];
				CommandList_processExt(commandList, deviceRef, info.op, ptr, &state);
				ptr += info.opSize;
			}
		}

		//Transition back swapchains to present

		//Combine transitions into one call.

		dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_TEXTURE };

		for (U64 i = 0; i < swapchains.length; ++i) {

			SwapchainRef *swapchainRef = swapchains.ptr[i];
			DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(swapchainRef, Dx, 0);

			D3D12_BARRIER_SUBRESOURCE_RANGE range = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
				.NumMipLevels = 1,
				.NumArraySlices = 1,
				.NumPlanes = 1
			};

			gotoIfError3(clean, DxUnifiedTexture_transition(
				imageExt,
				D3D12_BARRIER_SYNC_RENDER_TARGET,
				D3D12_BARRIER_ACCESS_COMMON,
				D3D12_BARRIER_LAYOUT_PRESENT,
				&range,
				&deviceExt->imageTransitions,
				&dependency, alloc, e_rr));

			if(RefPtr_inc(swapchainRef))
				gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, swapchainRef, alloc, e_rr));
		}

		if(dependency.NumBarriers)
			commandBuffer->lpVtbl->Barrier(commandBuffer, 1, &dependency);

		ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);

		//End buffer

		gotoIfError3(clean, dxCheck(commandBuffer->lpVtbl->Close(commandBuffer), e_rr));
	}

	//Submit queue
	//TODO: Multiple queues

	queue.queue->lpVtbl->ExecuteCommandLists(queue.queue, 1, (ID3D12CommandList**) &commandBuffer);

	//Presents

	for(U64 i = 0; i < swapchains.length; ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(swapchains.ptr[i]);
		DxSwapchain *swapchainExt = TextureRef_getImplExtT(DxSwapchain, swapchains.ptr[i]);

		DXGI_PRESENT_PARAMETERS regions = (DXGI_PRESENT_PARAMETERS) { 0 };

		gotoIfError3(clean, dxCheck(swapchainExt->swapchain->lpVtbl->Present1(
			swapchainExt->swapchain,
			swapchain->presentMode == ESwapchainPresentMode_Fifo ? 1 : 0,
			swapchain->presentMode == ESwapchainPresentMode_Immediate ? DXGI_PRESENT_ALLOW_TEARING : 0,
			&regions
		), e_rr));

		UnifiedTexture *unifiedTexture = TextureRef_getUnifiedTextureIntern(swapchains.ptr[i], NULL);
		++unifiedTexture->currentImageId;
		unifiedTexture->currentImageId %= 3; //Always triple buffering, %3
	}

	//Fence value after present

	gotoIfError3(clean, dxCheck(
		queue.queue->lpVtbl->Signal(queue.queue, deviceExt->commitSemaphore, deviceExt->fenceId),
		e_rr
	));

clean:

	#if _ARCH == ARCH_X86_64

		//Regardless of device removal, we'll ask NV to report anything fishy to us.
		//It's technically possible that a TDR/Device removal is caused during setup time,
		//but it's very unlikely. As we do the bulk of D3D12 calls and all RT calls in submitCommands.
		//Otherwise we'd have to guard every dxCheck, which might not even have a device (could happen on an instance).

		if(device->info.capabilities.features & EGraphicsFeatures_RayValidation) {

			NvAPI_Status status = NvAPI_D3D12_FlushRaytracingValidationMessages((ID3D12Device5*)deviceExt->device);

			if(status != NVAPI_OK)
				retError(clean, Error_invalidState(0, "D3D12GraphicsDevice_submitCommands() flush RT val msgs failed"));
		}

	#endif

	if(eventHandle)
		CloseHandle(eventHandle);

	ListU16_free(&temp16, alloc);
	CharString_free(&temp, alloc);
	return s_uccess;
}

Bool DxGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, DxCommandBufferState *commandBuffer, Error *e_rr) {

	Bool s_uccess = true;

	if(commandBuffer->inRender)
		retError(clean, Error_invalidState(
			0, "DxGraphicsDevice_flush() can't flush while in render, because it can't efficiently be split up"
		));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	//End current command list

	HANDLE eventHandle = NULL;
	gotoIfError3(clean, dxCheck(commandBuffer->buffer->lpVtbl->Close(commandBuffer->buffer), e_rr));

	//Submit only the copy command list

	++deviceExt->fenceId;
	const DxCommandQueue queue = deviceExt->queues[EDxCommandQueue_Graphics];
	queue.queue->lpVtbl->ExecuteCommandLists(queue.queue, 1, (ID3D12CommandList**) &commandBuffer->buffer);
	gotoIfError3(clean, dxCheck(
		queue.queue->lpVtbl->Signal(queue.queue, deviceExt->commitSemaphore, deviceExt->fenceId),
		e_rr
	));

	//Wait for the device

	gotoIfError3(clean, GraphicsDeviceRef_wait(deviceRef, e_rr));

	//Reset command list

	const U32 threadId = 0;

	const DxCommandAllocator *allocator = DxGraphicsDevice_getCommandAllocator(
		deviceExt, queue.resolvedQueueId, threadId, (U8) device->fifId
	);

	gotoIfError3(clean, dxCheck(commandBuffer->buffer->lpVtbl->Reset(commandBuffer->buffer, allocator->pool, NULL), e_rr));

	commandBuffer->pipeline = NULL;
	commandBuffer->boundScissor = (D3D12_RECT) { 0 };
	commandBuffer->boundViewport = (D3D12_VIEWPORT) { 0 };
	commandBuffer->boundBuffers = (SetPrimitiveBuffersCmd) { 0 };
	commandBuffer->boundPrimitiveTopology = U8_MAX;
	commandBuffer->stencilRef = 0;
	commandBuffer->blendConstants = F32x4_zero();

	GraphicsDevice_rebindDescriptors(device, commandBuffer->buffer);

clean:

	if(eventHandle)
		CloseHandle(eventHandle);

	return s_uccess;
}
