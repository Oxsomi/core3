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

	GraphicsInstance *instance = (GraphicsInstance*) context;

	if(instance) {

		if(severity == D3D12_MESSAGE_SEVERITY_CORRUPTION || severity == D3D12_MESSAGE_SEVERITY_ERROR)
			AtomicI64_inc(&instance->validationErrors);

		else if(severity == D3D12_MESSAGE_SEVERITY_WARNING)
			AtomicI64_inc(&instance->validationWarnings);
	}

	const C8 *categoryStr = "Undefined";

	switch(category) {
		default:                                                                                    break;
		case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:    categoryStr = "Application defined";    break;
		case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:          categoryStr = "Misc";                   break;
		case D3D12_MESSAGE_CATEGORY_INITIALIZATION:         categoryStr = "Initialization";         break;
		case D3D12_MESSAGE_CATEGORY_CLEANUP:                categoryStr = "Cleanup";                break;
		case D3D12_MESSAGE_CATEGORY_COMPILATION:            categoryStr = "Compilation";            break;
		case D3D12_MESSAGE_CATEGORY_STATE_CREATION:         categoryStr = "State creation";         break;
		case D3D12_MESSAGE_CATEGORY_STATE_SETTING:          categoryStr = "State setting";          break;
		case D3D12_MESSAGE_CATEGORY_STATE_GETTING:          categoryStr = "State getting";          break;
		case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:  categoryStr = "Resource manipulation";  break;
		case D3D12_MESSAGE_CATEGORY_EXECUTION:              categoryStr = "Execution";              break;
		case D3D12_MESSAGE_CATEGORY_SHADER:                 categoryStr = "Shader";                 break;
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

		GraphicsInstance *instance = (GraphicsInstance*) pUserData;

		if(instance) {

			if(severity == NVAPI_D3D12_RAYTRACING_VALIDATION_MESSAGE_SEVERITY_ERROR)
				AtomicI64_inc(&instance->validationErrors);

			else AtomicI64_inc(&instance->validationWarnings);
		}

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

	if(name && name->ptr)
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

	else gotoIfError3(clean, dxCheck(
		instanceExt->deviceFactorySingleton->lpVtbl->CreateDevice(
			instanceExt->deviceFactorySingleton,
			(IUnknown*)deviceExt->adapter4, D3D_FEATURE_LEVEL_11_0,
			&IID_ID3D12Device10, (void**) &deviceExt->device
		),
		e_rr
	));

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

			//Only break into the debugger on validation errors when one is actually attached.
			//The D3D12 debug layer implements "break on severity" via a RaiseException.
			//With no debugger present that becomes an unhandled second-chance exception and hard-crashes the process.
			//Without the break the failing HRESULT just propagates as an Error, like the Vulkan backend does.

			if(IsDebuggerPresent()) {

				gotoIfError3(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->SetBreakOnSeverity(
					deviceExt->infoQueue0, D3D12_MESSAGE_SEVERITY_CORRUPTION, true
				), e_rr));

				gotoIfError3(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->SetBreakOnSeverity(
					deviceExt->infoQueue0, D3D12_MESSAGE_SEVERITY_ERROR, true
				), e_rr));
			}

			D3D12_MESSAGE_ID hide[] = {

				D3D12_MESSAGE_ID_CREATEDEVICE_DEBUG_LAYER_STARTUP_OPTIONS,
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CREATERESOURCE_INVALIDALIGNMENT,   //To check if we allow small alignment

				//The device itself always shows up alive in our own ReportLiveDeviceObjects call at shutdown,
				// since the debug device still holds it; leaked child objects report under their own ids.

				D3D12_MESSAGE_ID_LIVE_DEVICE
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
				(GraphicsInstance*) instance,        //Outlives the device, so the callback context stays valid
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
						(ID3D12Device5*)deviceExt->device, onDebugReportNv,
						(GraphicsInstance*) instance,        //Outlives the device, so the context stays valid
						&handle
					);

					if(status != NVAPI_OK)
						retError(clean, Error_invalidState(
							0, "NvAPI_D3D12_RegisterRaytracingValidationMessageCallback couldn't be called"
						));
				}
			}

		#endif
	}

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

	//Timestamp query heaps and readback buffers, one per frame in flight, and the period from the graphics queue.
	//D3D12 supports timestamps on the direct queue on every device, so this is unconditional.

	{
		U64 timestampFreq = 0;
		deviceExt->queues[EDxCommandQueue_Graphics].queue->lpVtbl->GetTimestampFrequency(
			deviceExt->queues[EDxCommandQueue_Graphics].queue, &timestampFreq
		);

		deviceExt->timestampPeriod = timestampFreq ? (F32) (1.0e9 / (F64) timestampFreq) : 0;
		device->info.capabilities.timestampPeriod = deviceExt->timestampPeriod;

		D3D12_QUERY_HEAP_DESC timestampHeapDesc = (D3D12_QUERY_HEAP_DESC) {
			.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
			.Count = GRAPHICS_TIMESTAMP_QUERIES
		};

		for(U64 i = 0; i < device->framesInFlight; ++i) {

			gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateQueryHeap(
				deviceExt->device, &timestampHeapDesc, &IID_ID3D12QueryHeap, (void**) &deviceExt->timestampHeap[i]
			), e_rr));

			//The results resolve into an ordinary readback DeviceBuffer, allocated and mapped for us.

			CharString readbackName = CharString_createRefCStrConst("Timestamp readback");

			gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
				*deviceRef,
				EDeviceBufferUsage_None,
				EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit |
				EGraphicsResourceFlag_CPUReadBit,
				NULL, &readbackName, GRAPHICS_TIMESTAMP_QUERIES * sizeof(U64), &deviceExt->timestampReadback[i], e_rr
			));

			deviceExt->timestampCapacity[i] = GRAPHICS_TIMESTAMP_QUERIES;
		}
	}

	//Create DSVs

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = 1
	};

	CharString tmpName = CharString_createNull();

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		tmpName = CharString_createRefCStrConst("DSV heap");

	gotoIfError3(clean, DxGraphicsDevice_createDescriptorHeapSingle(
		deviceExt, heapDesc, &tmpName, &deviceExt->cpuHeaps[ECPUDescriptorHeapType_DSV], false, alloc, e_rr
	));

	//Create RTVs

	heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = 8
	};

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		tmpName = CharString_createRefCStrConst("RTV heap");

	gotoIfError3(clean, DxGraphicsDevice_createDescriptorHeapSingle(
		deviceExt, heapDesc, &tmpName, &deviceExt->cpuHeaps[ECPUDescriptorHeapType_RTV], false, alloc, e_rr
	));

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
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DispatchIndirectCmd),       .pArgumentDescs = &sigDesc[0] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(D3D12DispatchRaysIndirect), .pArgumentDescs = &sigDesc[1] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallIndexed),           .pArgumentDescs = &sigDesc[2] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallUnindexed),         .pArgumentDescs = &sigDesc[3] }
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

		for(U64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {

			if(deviceExt->timestampHeap[i])
				deviceExt->timestampHeap[i]->lpVtbl->Release(deviceExt->timestampHeap[i]);

			RefPtr_dec(&deviceExt->timestampReadback[i]);

			RefPtr_dec(&deviceExt->dispatchRaysIndirect[i]);
			RefPtr_dec(&deviceExt->dispatchRaysIndirectStaging[i]);
		}

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

	//Validate exit for leaks, before the info queues go away so the report is still drained and counted below

	if(deviceExt->debugDevice)
		deviceExt->debugDevice->lpVtbl->ReportLiveDeviceObjects(
			deviceExt->debugDevice, D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL
		);

	//Without the live callback (pre Win11) stored messages would otherwise vanish uncounted at shutdown

	DxGraphicsDevice_logDebugMessages(deviceExt, (GraphicsInstance*) instance, alloc);

	if(deviceExt->infoQueue0)
		deviceExt->infoQueue0->lpVtbl->Release(deviceExt->infoQueue0);

	if(deviceExt->infoQueue1)
		deviceExt->infoQueue1->lpVtbl->Release(deviceExt->infoQueue1);

	if(deviceExt->deviceConfig)
		deviceExt->deviceConfig->lpVtbl->Release(deviceExt->deviceConfig);

	if(deviceExt->debugDevice)
		deviceExt->debugDevice->lpVtbl->Release(deviceExt->debugDevice);

	ListDxCommandAllocator_free(&deviceExt->commandPools, alloc);

	//Free temp storage

	for(U64 i = 0; i < deviceExt->compactionEmitPools.length; ++i)
		RefPtr_dec((RefPtr**) &deviceExt->compactionEmitPools.ptrNonConst[i]);

	for(U64 i = 0; i < deviceExt->compactionReadbackPools.length; ++i)
		RefPtr_dec((RefPtr**) &deviceExt->compactionReadbackPools.ptrNonConst[i]);

	ListRefPtr_free(&deviceExt->compactionEmitPools, alloc);
	ListRefPtr_free(&deviceExt->compactionReadbackPools, alloc);

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

	const HANDLE eventHandle = CreateEventExA(NULL, NULL, 0, EVENT_ALL_ACCESS);

	gotoIfError3(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
		deviceExt->commitSemaphore, deviceExt->fenceId, eventHandle
	), e_rr));

	//An INFINITE wait would inherit a wedged submit as a silent forever-hang, and a hard deadline would
	// fail slow but correct work by a number, so the wait runs in one second slices: it reports the
	// stall while it lasts and fails only on what the device actually reports, removal included.

	for(U64 waited = 0; ; ) {

		const DWORD waitRes = WaitForSingleObject(eventHandle, 1000);

		if(waitRes == WAIT_OBJECT_0)
			break;

		if(waitRes != WAIT_TIMEOUT)
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_wait() event wait failed"));

		gotoIfError3(clean, dxCheck(
			deviceExt->device->lpVtbl->GetDeviceRemovedReason(deviceExt->device), e_rr
		));

		++waited;

		if(!(waited % 5))
			Log_performanceLnx(
				"GraphicsDeviceRef_wait() still waiting on the commit fence after %"PRIu64"s, "
				"the device may be wedged", waited
			);
	}

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

	if(id >= device->commandPools.length)    //This can technically happen if thread count changes at runtime (servers?)
		return NULL;

	return device->commandPools.ptrNonConst + id;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

void GraphicsDevice_rebindDescriptors(GraphicsDevice *device, DxCommandBuffer *commandBuffer) {

	//Bind descriptor heaps, root signature and descriptor tables since they stay the same for the entire frame.
	//For every bind point.

	//Without bindless there's no default heap, table or root signature to bind.
	//Every pipeline brings its own layout in that case, so there's nothing to do per frame.

	if(!device->defaultPipelineLayout || !device->defaultDescriptorTable || !device->defaultDescriptorHeaps)
		return;

	DxDescriptorHeap *heap = DescriptorHeap_ext(DescriptorHeapRef_ptr(device->defaultDescriptorHeaps), Dx);

	ID3D12DescriptorHeap *descriptorHeaps[2] = { heap->resourcesHeap.heap, heap->samplerHeap.heap };

	DxDescriptorTable *table = DescriptorTable_ext(DescriptorTableRef_ptr(device->defaultDescriptorTable), Dx);
	//Each heap's increment comes from GetDescriptorHandleIncrementSize for its own descriptor type,
	// and sampler descriptors are not the same size as CBV/SRV/UAV ones on every adapter.
	//So the sampler offset has to scale by the sampler heap's stride.
	//Using the resource heap's happens to work only where the two coincide,
	// and lands somewhere else entirely on hardware where they don't.

	D3D12_GPU_DESCRIPTOR_HANDLE descriptorTable[2] = {
		{ heap->samplerHeap.gpuHandle.ptr + table->allocationLocations[1] * heap->samplerHeap.gpuIncrement },
		{ heap->resourcesHeap.gpuHandle.ptr + table->allocationLocations[0] * heap->resourcesHeap.gpuIncrement }
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

	const U32 globalsRootParam = defaultLayoutExt->rootParamPushDescriptors;

	commandBuffer->lpVtbl->SetComputeRootConstantBufferView(commandBuffer, globalsRootParam, cbvLoc);
	commandBuffer->lpVtbl->SetGraphicsRootConstantBufferView(commandBuffer, globalsRootParam, cbvLoc);
}

//The message callback route needs Win11, so on older Windows the stored messages sit unread in the info queue.
//Draining them when something fails is what turns "invalid argument" into the layer's actual complaint.
//GetMessageA is not a typo: windows.h's GetMessage macro was active when the vtbl struct was declared.

void DxGraphicsDevice_logDebugMessages(
	const DxGraphicsDevice *deviceExt, GraphicsInstance *instance, const Allocator *alloc
) {

	//With infoQueue1 (Win11) the message callback already printed and counted everything live,
	// so draining would duplicate

	if(!deviceExt || !deviceExt->infoQueue0 || deviceExt->infoQueue1)
		return;

	ID3D12InfoQueue1 *queue = deviceExt->infoQueue0;
	const U64 count = queue->lpVtbl->GetNumStoredMessages(queue);

	for (U64 i = 0; i < count; ++i) {

		SIZE_T len = 0;

		if(FAILED(queue->lpVtbl->GetMessageA(queue, i, NULL, &len)) || !len)
			continue;

		Buffer buf = Buffer_createNull();

		if(!Buffer_createEmptyBytes(len, alloc, &buf, NULL))
			continue;

		D3D12_MESSAGE *msg = (D3D12_MESSAGE*) buf.ptrNonConst;

		if(SUCCEEDED(queue->lpVtbl->GetMessageA(queue, i, msg, &len)) && msg->pDescription) {

			Log_errorLnx("D3D12: %s", msg->pDescription);

			if(instance) {

				if(msg->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION || msg->Severity == D3D12_MESSAGE_SEVERITY_ERROR)
					AtomicI64_inc(&instance->validationErrors);

				else if(msg->Severity == D3D12_MESSAGE_SEVERITY_WARNING)
					AtomicI64_inc(&instance->validationWarnings);
			}
		}

		Buffer_free(&buf, alloc);
	}

	queue->lpVtbl->ClearStoredMessages(queue);
}

//DispatchRaysIndirect keeps a per frame in flight argument buffer, and only when WriteBufferImmediate is
// unavailable an upload staging buffer for the shader binding table. Both are ordinary DeviceBuffers, so the
// allocator, state tracking and teardown are handled for us rather than by hand. This grows them to hold `slots`
// dispatches and never shrinks, mirroring GraphicsDeviceRef_resizeStagingReadbackBuffer: build the replacements
// first and swap only on success, so a failed grow keeps the smaller buffers rather than dropping to none.

static Bool DxGraphicsDevice_reserveDispatchRaysIndirect(GraphicsDeviceRef *deviceRef, U64 fifId, U32 slots, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	const U64 stride = sizeof(D3D12DispatchRaysIndirect);
	DeviceBufferRef *curArgs = deviceExt->dispatchRaysIndirect[fifId];

	if(!slots || (curArgs && DeviceBufferRef_ptr(curArgs)->resource.size >= (U64) slots * stride))
		return true;

	const Bool wbi = !!(device->info.capabilities.featuresExt & EDxGraphicsFeatures_WriteBufferImmediate);

	U32 newCap = curArgs ? (U32) (DeviceBufferRef_ptr(curArgs)->resource.size / stride) : 0;

	if(!newCap)
		newCap = GRAPHICS_DISPATCH_RAYS_INDIRECT_ARGS;

	while(newCap < slots)
		newCap <<= 1;

	const U64 newSize = (U64) newCap * stride;

	DeviceBufferRef *newArgs = NULL;
	DeviceBufferRef *newStaging = NULL;

	CharString argsName = CharString_createRefCStrConst("DispatchRaysIndirect args");

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_Indirect,
		EGraphicsResourceFlag_InternalWeakDeviceRef,
		NULL, &argsName, newSize, &newArgs, e_rr
	));

	if(!wbi) {

		CharString stagingName = CharString_createRefCStrConst("DispatchRaysIndirect SBT staging");

		gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
			deviceRef,
			EDeviceBufferUsage_None,
			EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
			NULL, &stagingName, newSize, &newStaging, e_rr
		));
	}

	RefPtr_dec(&deviceExt->dispatchRaysIndirect[fifId]);
	deviceExt->dispatchRaysIndirect[fifId] = newArgs;
	newArgs = NULL;

	if(!wbi) {
		RefPtr_dec(&deviceExt->dispatchRaysIndirectStaging[fifId]);
		deviceExt->dispatchRaysIndirectStaging[fifId] = newStaging;
		newStaging = NULL;
	}

clean:
	RefPtr_dec(&newArgs);
	RefPtr_dec(&newStaging);
	return s_uccess;
}

Bool DX_WRAP_FUNC(GraphicsDevice_submitCommands)(
	GraphicsDeviceRef *deviceRef,
	const ListCommandListRef *commandLists,
	const ListSwapchainRef *swapchains,
	CBufferData *cbufferData,
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

	//Error path bookkeeping, declared before the first goto so clean always reads initialized values.
	//recordingAllocator tracks which allocator owns a list that is mid recording, so a poisoned list can be dropped.

	DxCommandAllocator *recordingAllocator = NULL;
	Bool executed = false;

	DxCommandQueue queue = deviceExt->queues[EDxCommandQueue_Graphics];

	//Wait for previous frame semaphore

	++deviceExt->fenceId;

	if (deviceExt->fenceId > device->framesInFlight) {

		eventHandle = CreateEventExA(NULL, NULL, 0, EVENT_ALL_ACCESS);

		gotoIfError3(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
			deviceExt->commitSemaphore, deviceExt->fenceId - device->framesInFlight, eventHandle
		), e_rr));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
		eventHandle = NULL;
	}

	//Prepare per frame cbuffer

	DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[device->fifId]);

	for (U32 i = 0; i < (!swapchains ? 0 : swapchains->length); ++i) {

		SwapchainRef *swapchainRef = swapchains->ptr[i];
		Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);

		Bool allowComputeExt = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

		UnifiedTextureImage managedImage = TextureRef_getCurrImage(swapchainRef, 0);

		if(cbufferData) {
			cbufferData->swapchains[i * 2 + 0] = managedImage.readHandle;
			cbufferData->swapchains[i * 2 + 1] = allowComputeExt ? managedImage.writeHandle : 0;
		}
	}

	if(cbufferData)
		*(CBufferData*)frameData->resource.mappedMemoryExt = *cbufferData;

	//Record command list

	DxCommandBuffer *commandBuffer = NULL;

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	if (commandLists && commandLists->length) {

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
					), threadId, device->fifId
				));

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

		recordingAllocator = allocator;

		//Start copies

		DxCommandBufferState state = (DxCommandBufferState) { .buffer = commandBuffer };
		state.boundPrimitiveTopology = U8_MAX;

		gotoIfError3(clean, GraphicsDeviceRef_handleNextFrame(deviceRef, &state, e_rr));

		//Resolve the previous frame at this slot before buildTimings overwrites its entries; the fence wait above
		// proved it done, so the readback its ResolveQueryData filled is ready. D3D12 timestamps are full 64-bit.

		if(
			(device->info.capabilities.features2 & EGraphicsFeatures2_Timestamps) &&
			device->submitId > device->framesInFlight && device->timingSlots[device->fifId]
		) {
			const U64 *ticks =
				(const U64*) DeviceBufferRef_ptr(deviceExt->timestampReadback[device->fifId])->resource.mappedMemoryExt;

			GraphicsDevice_resolveTimings(
				device, device->fifId, ticks, device->timingSlots[device->fifId],
				deviceExt->timestampPeriod, alloc, NULL
			);
		}

		device->timingSlots[device->fifId] = (device->info.capabilities.features2 & EGraphicsFeatures2_Timestamps)
			? GraphicsDevice_buildTimings(device, device->fifId, commandLists, alloc) : 0;

		deviceExt->timestampCursor = 0;
		deviceExt->dispatchRaysIndirectCursor = 0;

		//Size the DispatchRaysIndirect argument buffer to this frame's indirect ray dispatches before any are
		// recorded, so the record path never has to drop one. Counting the ops is a quick walk. A failed grow is not
		// fatal: the smaller buffer stays and only dispatches past its capacity are dropped.

		{
			U32 driCount = 0;

			for(U64 i = 0; i < (commandLists ? commandLists->length : 0); ++i) {
				CommandList *cl = CommandListRef_ptr(commandLists->ptr[i]);
				for(U64 j = 0; j < cl->commandOps.length; ++j)
					if(cl->commandOps.ptr[j].op == ECommandOp_DispatchRaysIndirect)
						++driCount;
			}

			Error driErr = (Error) { 0 };

			if(driCount && !DxGraphicsDevice_reserveDispatchRaysIndirect(deviceRef, device->fifId, driCount, &driErr))
				Log_errorLnx("DispatchRaysIndirect could not grow its argument buffer, keeping the smaller one");
		}

		//Grow the heap and its readback buffer when a frame needs more slots than they hold, mirroring the Vulkan
		// path; the doubling happens once at the high-water mark and never for a caller under the initial capacity.

		if(device->timingSlots[device->fifId] > deviceExt->timestampCapacity[device->fifId]) {

			U32 newCap = deviceExt->timestampCapacity[device->fifId] ? deviceExt->timestampCapacity[device->fifId] : 1;

			while(newCap < device->timingSlots[device->fifId])
				newCap <<= 1;

			if(deviceExt->timestampHeap[device->fifId])
				deviceExt->timestampHeap[device->fifId]->lpVtbl->Release(deviceExt->timestampHeap[device->fifId]);

			//Cleared before the recreate so a failed CreateQueryHeap cannot leave a dangling pointer that the failure
			// branch below would release a second time.

			deviceExt->timestampHeap[device->fifId] = NULL;

			RefPtr_dec(&deviceExt->timestampReadback[device->fifId]);

			D3D12_QUERY_HEAP_DESC growHeapDesc = (D3D12_QUERY_HEAP_DESC) {
				.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = newCap
			};

			const HRESULT hrHeap = deviceExt->device->lpVtbl->CreateQueryHeap(
				deviceExt->device, &growHeapDesc, &IID_ID3D12QueryHeap, (void**) &deviceExt->timestampHeap[device->fifId]
			);

			CharString readbackName = CharString_createRefCStrConst("Timestamp readback");
			Error readbackErr = (Error) { 0 };

			const Bool grown = SUCCEEDED(hrHeap) && GraphicsDeviceRef_createBuffer(
				deviceRef,
				EDeviceBufferUsage_None,
				EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit |
				EGraphicsResourceFlag_CPUReadBit,
				NULL, &readbackName, (U64) newCap * sizeof(U64), &deviceExt->timestampReadback[device->fifId], &readbackErr
			);

			if(grown)
				deviceExt->timestampCapacity[device->fifId] = newCap;

			else {

				//A failed grow leaves this frame without timings rather than a torn buffer; release whatever the query
				// heap create produced so it does not leak, and the readback dec is a no-op if createBuffer never set it.

				if(deviceExt->timestampHeap[device->fifId])
					deviceExt->timestampHeap[device->fifId]->lpVtbl->Release(deviceExt->timestampHeap[device->fifId]);

				deviceExt->timestampHeap[device->fifId] = NULL;
				RefPtr_dec(&deviceExt->timestampReadback[device->fifId]);
				deviceExt->timestampCapacity[device->fifId] = 0;
				device->timingSlots[device->fifId] = 0;
			}
		}

		//Ensure the ubo is in the right state, but only when something can actually read it.
		//An op less submission would otherwise turn into a barrier only command list,
		// which the debug layer rightly flags as pointless synchronization.

		U64 totalOps = 0;

		for (U64 i = 0; i < commandLists->length; ++i)
			totalOps += CommandListRef_ptr(commandLists->ptr[i])->commandOps.length;

		D3D12_BARRIER_GROUP dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

		if (totalOps) {

			DxDeviceBuffer *uboExt = DeviceBuffer_ext(frameData, Dx);

			gotoIfError3(clean, DxDeviceBuffer_transition(
				uboExt,
				D3D12_BARRIER_SYNC_VERTEX_SHADING,
				D3D12_BARRIER_ACCESS_CONSTANT_BUFFER,
				&deviceExt->bufferTransitions,
				&dependency, alloc, e_rr
			));

			if(dependency.NumBarriers)
				commandBuffer->lpVtbl->Barrier(commandBuffer, 1, &dependency);

			ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
		}

		//Descriptors bind lazily at the first work op that needs them (bindful), so a purely bindful frame
		// never pays for the default heap and root signature setup.

		//Record commands

		for (U64 i = 0; i < (commandLists ? commandLists->length : 0); ++i) {

			state.scopeCounter = 0;
			CommandList *commandList = CommandListRef_ptr(commandLists->ptr[i]);
			const U8 *ptr = commandList->data.ptr;

			for (U64 j = 0; j < commandList->commandOps.length; ++j) {
				CommandOpInfo info = commandList->commandOps.ptr[j];
				CommandList_processExt(commandList, deviceRef, info.op, ptr, &state);
				ptr += info.opSize;
			}
		}

		//Readbacks are recorded after the frame's commands so they observe this frame's results

		gotoIfError3(clean, GraphicsDeviceRef_flushPendingPulls(deviceRef, &state, e_rr));

		//Copy this frame's timestamps out of the query heap into its readback buffer; the CPU reads them a frame on.

		if(device->timingSlots[device->fifId])
			commandBuffer->lpVtbl->ResolveQueryData(
				commandBuffer, deviceExt->timestampHeap[device->fifId], D3D12_QUERY_TYPE_TIMESTAMP,
				0, device->timingSlots[device->fifId],
				DeviceBuffer_ext(DeviceBufferRef_ptr(deviceExt->timestampReadback[device->fifId]), Dx)->buffer, 0
			);

		//Transition back swapchains to present

		//Combine transitions into one call.

		dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_TEXTURE };

		for (U64 i = 0; i < (swapchains ? swapchains->length : 0); ++i) {

			SwapchainRef *swapchainRef = swapchains->ptr[i];

			//The PRESENT layout is meaningless for a swapchain that owns its images: nothing presents it,
			// and whatever reads it next transitions it the way it would any other render target.
			//It is still tracked in flight, since its images are as much in use as a presented one's.

			if(!(SwapchainRef_ptr(swapchainRef)->base.resource.flags & EGraphicsResourceFlag_InternalOwnsImages)) {

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
					&dependency, alloc, e_rr
				));
			}

			if(RefPtr_inc(swapchainRef))
				gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, swapchainRef, alloc, e_rr));
		}

		if(dependency.NumBarriers)
			commandBuffer->lpVtbl->Barrier(commandBuffer, 1, &dependency);

		ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);

		//End buffer

		gotoIfError3(clean, dxCheck(commandBuffer->lpVtbl->Close(commandBuffer), e_rr));
		recordingAllocator = NULL;
	}

	//Submit queue
	//TODO: Multiple queues

	queue.queue->lpVtbl->ExecuteCommandLists(queue.queue, 1, (ID3D12CommandList**) &commandBuffer);
	executed = true;

	//Presents

	for(U64 i = 0; i < (!swapchains ? 0 : swapchains->length); ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(swapchains->ptr[i]);
		UnifiedTexture *unifiedTexture = TextureRef_getUnifiedTextureIntern(swapchains->ptr[i], NULL);

		//A swapchain that owns its images has nothing to present to, so the frame ends in memory.
		//The ring still advances:
		// that is what keeps a frame in flight from writing the image a previous one is still being read from.

		if(!(swapchain->base.resource.flags & EGraphicsResourceFlag_InternalOwnsImages)) {

			DxSwapchain *swapchainExt = TextureRef_getImplExtT(DxSwapchain, swapchains->ptr[i]);

			DXGI_PRESENT_PARAMETERS regions = (DXGI_PRESENT_PARAMETERS) { 0 };

			gotoIfError3(clean, dxCheck(swapchainExt->swapchain->lpVtbl->Present1(
				swapchainExt->swapchain,
				swapchain->presentMode == ESwapchainPresentMode_Fifo ? 1 : 0,
				swapchain->presentMode == ESwapchainPresentMode_Immediate ? DXGI_PRESENT_ALLOW_TEARING : 0,
				&regions
			), e_rr));
		}

		++unifiedTexture->currentImageId;
		unifiedTexture->currentImageId %= unifiedTexture->images;
	}

	//Fence value after present

	gotoIfError3(clean, dxCheck(
		queue.queue->lpVtbl->Signal(queue.queue, deviceExt->commitSemaphore, deviceExt->fenceId),
		e_rr
	));

clean:

	if(!s_uccess)
		DxGraphicsDevice_logDebugMessages(deviceExt, GraphicsInstanceRef_ptr(device->instance), alloc);

	//A list that failed during recording or Close is in an error state that Reset can't recover, so keeping it
	// would poison every later submit that cycles back to this allocator with closed command list errors.

	if(!s_uccess && recordingAllocator && recordingAllocator->cmd) {
		recordingAllocator->cmd->lpVtbl->Close(recordingAllocator->cmd);        //Best effort, so the pool can Reset
		recordingAllocator->cmd->lpVtbl->Release(recordingAllocator->cmd);
		recordingAllocator->cmd = NULL;
	}

	//The frame's fence value has to signal even on failure, or every later wait on this device hangs forever.
	//If nothing was submitted the CPU can signal it directly, otherwise the queue signals after the submitted work.

	if(!s_uccess) {

		if(!executed)
			deviceExt->commitSemaphore->lpVtbl->Signal(deviceExt->commitSemaphore, deviceExt->fenceId);

		else queue.queue->lpVtbl->Signal(queue.queue, deviceExt->commitSemaphore, deviceExt->fenceId);
	}

	#if _ARCH == ARCH_X86_64

		//Regardless of device removal, we'll ask NV to report anything fishy to us.
		//It's technically possible that a TDR/Device removal is caused during setup time, but it's very unlikely.
		//As we do the bulk of D3D12 calls and all RT calls in submitCommands.
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

	//The Vulkan twin hints the same way; see EGraphicsDeviceMessage_SubmitFlushed

	if(GraphicsDevice_logOnce(GraphicsDeviceRef_ptr(deviceRef), EGraphicsDeviceMessage_SubmitFlushed))
		Log_performanceLnx(
			"D3D12: submit was split mid recording because pending copies or AS builds crossed the flush "
			"threshold, which adds a GPU sync point; raise flushThreshold or batch smaller uploads "
			"(only logged once)"
		);

	Bool s_uccess = true;
	HANDLE eventHandle = NULL;

	if(commandBuffer->inRender)
		retError(clean, Error_invalidState(
			0, "DxGraphicsDevice_flush() can't flush while in render, because it can't efficiently be split up"
		));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	//End current command list

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

	//The fresh buffer has no descriptor state, so the emitted state trackers reset and the next work op's
	// lazy bind re-emits whatever was last bound (default or custom).

	commandBuffer->defaultDescriptorsBound = false;
	commandBuffer->lastBoundHeap = NULL;
	commandBuffer->lastBoundTable[0] = commandBuffer->lastBoundTable[1] = NULL;
	commandBuffer->lastRootSig[0] = commandBuffer->lastRootSig[1] = NULL;

clean:

	if(eventHandle)
		CloseHandle(eventHandle);

	return s_uccess;
}
