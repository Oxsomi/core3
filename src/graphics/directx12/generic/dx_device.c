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
#include "graphics/directx12/directx12.h"
#include "graphics/directx12/dx_device.h"
#include "graphics/directx12/dx_swapchain.h"
#include "graphics/directx12/dx_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/math.h"
#include "types/thread.h"

#include <nvapi.h>

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
		default:																					break;
		case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:	categoryStr = "Application defined";	break;
		case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:			categoryStr = "Misc";					break;
		case D3D12_MESSAGE_CATEGORY_INITIALIZATION:			categoryStr = "Initialization";			break;
		case D3D12_MESSAGE_CATEGORY_CLEANUP:				categoryStr = "Cleanup";				break;
		case D3D12_MESSAGE_CATEGORY_COMPILATION:			categoryStr = "Compilation";			break;
		case D3D12_MESSAGE_CATEGORY_STATE_CREATION:			categoryStr = "State creation";			break;
		case D3D12_MESSAGE_CATEGORY_STATE_SETTING:			categoryStr = "State setting";			break;
		case D3D12_MESSAGE_CATEGORY_STATE_GETTING:			categoryStr = "State getting";			break;
		case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:	categoryStr = "Resource manipulation";	break;
		case D3D12_MESSAGE_CATEGORY_EXECUTION:				categoryStr = "Execution";				break;
		case D3D12_MESSAGE_CATEGORY_SHADER:					categoryStr = "Shader";					break;
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

TListImpl(DxCommandAllocator);
TListNamedImpl(ListID3D12Fence);

//Convert command into API dependent instructions
impl void CommandList_process(
	CommandList *commandList,
	GraphicsDevice *device,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
);

const U64 GraphicsDeviceExt_size = sizeof(DxGraphicsDevice);

Error GraphicsDevice_initExt(
	const GraphicsInstance *instance,
	const GraphicsDeviceInfo *physicalDevice,
	GraphicsDeviceRef **deviceRef
) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	Error err = Error_none();
	ID3DBlob *errBlob = NULL, *rootSigBlob = NULL;

	//Create device

	const DxGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Dx);

	gotoIfError(clean, dxCheck(instanceExt->factory->lpVtbl->EnumAdapterByLuid(
		instanceExt->factory, *(const LUID*)&physicalDevice->luid,
		&IID_IDXGIAdapter4, (void**)&deviceExt->adapter4
	)))

	gotoIfError(clean, dxCheck(D3D12CreateDevice(
		(IUnknown*)deviceExt->adapter4, D3D_FEATURE_LEVEL_12_1, 
		&IID_ID3D12Device5, (void**) &deviceExt->device
	)))

	Bool isNv = device->info.vendor == EGraphicsVendorId_NV;

	if(device->flags & EGraphicsDeviceFlags_IsDebug) {

		gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12DebugDevice, (void**) &deviceExt->debugDevice
		)))

		if(SUCCEEDED(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12InfoQueue1, (void**) &deviceExt->infoQueue1
		)))
			gotoIfError(clean, dxCheck(deviceExt->infoQueue1->lpVtbl->RegisterMessageCallback(
				deviceExt->infoQueue1,
				onDebugReport,
				D3D12_MESSAGE_CALLBACK_FLAG_NONE,
				NULL,
				NULL
			)))

		//Nv specific initialization

		if(isNv) {

			//Enable RT validation

			if(device->info.capabilities.features & EGraphicsFeatures_RayValidation) {

				NvAPI_D3D12_EnableRaytracingValidation(deviceExt->device, NVAPI_D3D12_RAYTRACING_VALIDATION_FLAG_NONE);

				void *handle = NULL;
				NvAPI_Status status = NvAPI_D3D12_RegisterRaytracingValidationMessageCallback(
					deviceExt->device, onDebugReportNv, NULL, &handle
				);

				if(status != NVAPI_OK)
					gotoIfError(clean, Error_invalidState(
						0, "NvAPI_D3D12_RegisterRaytracingValidationMessageCallback couldn't be called"
					))
			}
		}
	}

	//Enable NV extensions

	static const U32 nvExtSlot = 99999;		//space and u slot

	if(isNv) {

		NvAPI_Status status = NvAPI_D3D12_SetNvShaderExtnSlotSpace((IUnknown*)deviceExt->device, nvExtSlot, nvExtSlot);

		if(status != NVAPI_OK)
			gotoIfError(clean, Error_invalidState(0, "NvAPI_D3D12_SetNvShaderExtnSlotSpace couldn't be called"))
	}

	//Get queues

	deviceExt->queues[EDxCommandQueue_Copy] = (DxCommandQueue) {
		.type = EDxCommandQueue_Copy,
		.resolvedQueueId = 0
	};

	D3D12_COMMAND_QUEUE_DESC queueInfo = (D3D12_COMMAND_QUEUE_DESC) {
		.Type = D3D12_COMMAND_LIST_TYPE_COPY
	};

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandQueue(
		deviceExt->device,
		&queueInfo,
		&IID_ID3D12CommandQueue, (void**) &deviceExt->queues[EDxCommandQueue_Copy].queue
	)))

	deviceExt->queues[EDxCommandQueue_Compute] = (DxCommandQueue) {
		.type = EDxCommandQueue_Compute,
		.resolvedQueueId = 1
	};

	queueInfo.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandQueue(
		deviceExt->device,
		&queueInfo,
		&IID_ID3D12CommandQueue, (void**) &deviceExt->queues[EDxCommandQueue_Compute].queue
	)))

	deviceExt->queues[EDxCommandQueue_Graphics] = (DxCommandQueue) {
		.type = EDxCommandQueue_Graphics,
		.resolvedQueueId = 2
	};

	queueInfo.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandQueue(
		deviceExt->device,
		&queueInfo,
		&IID_ID3D12CommandQueue, (void**) &deviceExt->queues[EDxCommandQueue_Graphics].queue
	)))

	//Create command recorder per queue per thread per backbuffer.
	//We only allow triple buffering, so allocate for triple buffers.
	//These will be initialized JIT because we don't know what thread will be accessing them.

	U64 threads = Platform_instance.threads;
	gotoIfError(clean, ListDxCommandAllocator_resizex(&deviceExt->commandPools, 3 * threads * 3))

	//Create fence

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateFence(
		deviceExt->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**) & deviceExt->commitSemaphore
	)))

	//Create root signature

	D3D12_DESCRIPTOR_RANGE descRanges[] = {

		(D3D12_DESCRIPTOR_RANGE) {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = EDescriptorTypeOffsets_SRVEnd - EDescriptorTypeOffsets_SRVStart,
			.OffsetInDescriptorsFromTableStart = EDescriptorTypeOffsets_SRVStart
		},

		(D3D12_DESCRIPTOR_RANGE) {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			.NumDescriptors = EDescriptorTypeOffsets_UAVEnd - EDescriptorTypeOffsets_UAVStart,
			.OffsetInDescriptorsFromTableStart = EDescriptorTypeOffsets_UAVStart
		},

		//Unused register, but nv wants it

		(D3D12_DESCRIPTOR_RANGE) {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			.BaseShaderRegister = nvExtSlot,
			.RegisterSpace = nvExtSlot,
			.NumDescriptors = 1,
			.OffsetInDescriptorsFromTableStart = EDescriptorTypeOffsets_UAVEnd
		}
	};

	D3D12_DESCRIPTOR_RANGE samplerRange = (D3D12_DESCRIPTOR_RANGE) {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
		.NumDescriptors = EDescriptorTypeOffsets_SamplerCount,
		.OffsetInDescriptorsFromTableStart = EDescriptorTypeOffsets_Sampler
	};

	D3D12_ROOT_PARAMETER rootParam[] = {

		//All other SRV/UAVs are bound through a descriptor table

		(D3D12_ROOT_PARAMETER) {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE) {
				.NumDescriptorRanges = (U32)(sizeof(descRanges) / sizeof(descRanges[0])),
				.pDescriptorRanges = descRanges
			}
		},

		//Samplers are bound through another descriptor table

		(D3D12_ROOT_PARAMETER) {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE) {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &samplerRange
			}
		},

		//CBV at b0, space0

		(D3D12_ROOT_PARAMETER) {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
			.Descriptor = (D3D12_ROOT_DESCRIPTOR) { 0 }
		}
	};

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSig = (D3D12_VERSIONED_ROOT_SIGNATURE_DESC) {
		.Version = D3D_ROOT_SIGNATURE_VERSION_1,
		.Desc_1_0 = (D3D12_ROOT_SIGNATURE_DESC) {
			.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			.NumParameters = (U32)(sizeof(rootParam) / sizeof(rootParam[0])),
			.pParameters = rootParam
		}
	};

	err = dxCheck(D3D12SerializeVersionedRootSignature(&rootSig, &rootSigBlob, &errBlob));

	if(err.genericError) {

		if(errBlob)
			Log_errorLnx("D3D12: Create root signature failed: %s", (const C8*) errBlob->lpVtbl->GetBufferPointer(errBlob));

		goto clean;
	}

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateRootSignature(
		deviceExt->device,
		0, rootSigBlob->lpVtbl->GetBufferPointer(rootSigBlob), rootSigBlob->lpVtbl->GetBufferSize(rootSigBlob),
		&IID_ID3D12RootSignature, (void**) &deviceExt->defaultLayout
	)))

	//Create samplers

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		.NumDescriptors = EDescriptorTypeOffsets_SamplerCount,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	};

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateDescriptorHeap(
		deviceExt->device,
		&heapDesc,
		&IID_ID3D12DescriptorHeap,
		(void**) &deviceExt->heaps[EDescriptorHeapType_Sampler].heap
	)))

	//Create resources

	heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = EDescriptorTypeOffsets_ResourceCount,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	};

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateDescriptorHeap(
		deviceExt->device,
		&heapDesc,
		&IID_ID3D12DescriptorHeap,
		(void**) &deviceExt->heaps[EDescriptorHeapType_Resources].heap
	)))

	//Create DSVs

	heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = EDescriptorTypeOffsets_DSVCount
	};

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateDescriptorHeap(
		deviceExt->device,
		&heapDesc,
		&IID_ID3D12DescriptorHeap,
		(void**) &deviceExt->heaps[EDescriptorHeapType_DSV].heap
	)))

	//Create RTVs

	heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = EDescriptorTypeOffsets_RTVCount
	};

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateDescriptorHeap(
		deviceExt->device,
		&heapDesc,
		&IID_ID3D12DescriptorHeap,
		(void**) &deviceExt->heaps[EDescriptorHeapType_RTV].heap
	)))

	for (U32 i = 0; i < EDescriptorHeapType_Count; ++i) {

		DxHeap *heap = &deviceExt->heaps[i];

		if(device->flags & EGraphicsDeviceFlags_IsDebug) {

			static const wchar_t *debugNames[] = {
				L"Descriptor heap (0: Samplers)",
				L"Descriptor heap (1: Resources)",
				L"Descriptor heap (2: DSV)",
				L"Descriptor heap (3: RTV)"
			};

			gotoIfError(clean, dxCheck(heap->heap->lpVtbl->SetName(heap->heap, debugNames[i])))
		}

		D3D12_DESCRIPTOR_HEAP_TYPE type;

		switch(i) {
			case EDescriptorHeapType_Sampler:	type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;		break;
			default:							type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	break;
			case EDescriptorHeapType_DSV:		type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;			break;
			case EDescriptorHeapType_RTV:		type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;			break;
		}

		heap->cpuIncrement = deviceExt->device->lpVtbl->GetDescriptorHandleIncrementSize(deviceExt->device, type);

		if(!heap->heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap->heap, &heap->cpuHandle))
			gotoIfError(clean, Error_nullPointer(0, "D3D12: GetCPUDescriptorHandleForHeapStart() returned NULL"))

		if(i >= EDescriptorHeapType_DSV)		//No GPU descriptor handle offsets
			continue;

		heap->gpuIncrement = heap->cpuIncrement;

		if(!heap->heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(heap->heap, &heap->gpuHandle))
			gotoIfError(clean, Error_nullPointer(0, "D3D12: GetGPUDescriptorHandleForHeapStart() returned NULL"))
	}

	//Determine when we need to flush.
	//As a rule of thumb I decided for 20% occupied mem by just copies.
	//Or if there's distinct shared mem available too it can allocate 10% more in that memory too
	// (as long as it doesn't exceed 33%).
	//Flush threshold is kept under 4 GiB to avoid TDRs because even if the mem is available it might be slow.

	const Bool isDistinct = device->info.type == EGraphicsDeviceType_Dedicated;
	const U64 cpuHeapSize = device->info.capabilities.sharedMemory;
	const U64 gpuHeapSize = device->info.capabilities.dedicatedMemory;

	device->flushThreshold = U64_min(
		4 * GIBI,
		isDistinct ? U64_min(gpuHeapSize / 3, cpuHeapSize / 10 + gpuHeapSize / 5) :
		cpuHeapSize / 5
	);

	device->flushThresholdPrimitives = 100 * MIBI / 3;		//100M vertices per frame limit

	//Allocate temp storage for transitions

	gotoIfError(clean, ListD3D12_BUFFER_BARRIER_reservex(&deviceExt->bufferTransitions, 17))
	gotoIfError(clean, ListD3D12_TEXTURE_BARRIER_reservex(&deviceExt->imageTransitions, 16))

	//Create command signatures for ExecuteIndirect

	D3D12_INDIRECT_ARGUMENT_DESC sigDesc[] = {
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH },
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS },
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED },
		(D3D12_INDIRECT_ARGUMENT_DESC) { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW }
	};

	D3D12_COMMAND_SIGNATURE_DESC signatures[] = {
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DispatchIndirectCmd), .pArgumentDescs = &sigDesc[0] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DX12DispatchRaysIndirect), .pArgumentDescs = &sigDesc[1] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallIndexed), .pArgumentDescs = &sigDesc[2] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallUnindexed), .pArgumentDescs = &sigDesc[3] }
	};

	for(U64 i = 0; i < EExecuteIndirectCommand_Count; ++i) {

		signatures[i].NumArgumentDescs = 1;

		gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandSignature(
			deviceExt->device,
			&signatures[i],
			NULL,
			&IID_ID3D12CommandSignature, (void**) &deviceExt->commandSigs[i]
		)))
	}

clean:

	if(errBlob)
		errBlob->lpVtbl->Release(errBlob);

	if(rootSigBlob)
		rootSigBlob->lpVtbl->Release(rootSigBlob);

	if(err.genericError)
		GraphicsDeviceRef_dec(deviceRef);

	return err;
}

void GraphicsDevice_postInit(GraphicsDevice *device) {		//No-op in DX12, CBV can be made/bound at runtime :)
	(void)device;
}

Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext) {

	if(!instance || !ext)
		return instance;

	DxGraphicsDevice *deviceExt = (DxGraphicsDevice*)ext;

	if(deviceExt->device) {

		for(U64 i = 0; i < deviceExt->commandPools.length; ++i) {

			const DxCommandAllocator alloc = deviceExt->commandPools.ptr[i];

			if(alloc.cmd)
				alloc.cmd->lpVtbl->Release(alloc.cmd);

			if(alloc.pool)
				alloc.pool->lpVtbl->Release(alloc.pool);
		}

		if(deviceExt->commitSemaphore)
			deviceExt->commitSemaphore->lpVtbl->Release(deviceExt->commitSemaphore);

		for(U64 i = 0; i < EExecuteIndirectCommand_Count; ++i)
			if(deviceExt->commandSigs[i])
				deviceExt->commandSigs[i]->lpVtbl->Release(deviceExt->commandSigs[i]);

		for(U64 i = 0; i < EDescriptorHeapType_Count; ++i)
			if(deviceExt->heaps[i].heap)
				deviceExt->heaps[i].heap->lpVtbl->Release(deviceExt->heaps[i].heap);

		if(deviceExt->defaultLayout)
			deviceExt->defaultLayout->lpVtbl->Release(deviceExt->defaultLayout);

		for(U64 i = 0; i < EDxCommandQueue_Count; ++i)
			if(deviceExt->queues[i].queue)
				deviceExt->queues[i].queue->lpVtbl->Release(deviceExt->queues[i].queue);

		deviceExt->device->lpVtbl->Release(deviceExt->device);
	}

	if(deviceExt->adapter4)
		deviceExt->adapter4->lpVtbl->Release(deviceExt->adapter4);

	if(deviceExt->debugDevice) {

		//Validate exit for leaks

		deviceExt->debugDevice->lpVtbl->ReportLiveDeviceObjects(deviceExt->debugDevice, D3D12_RLDO_SUMMARY);

		deviceExt->debugDevice->lpVtbl->Release(deviceExt->debugDevice);
	}

	if(deviceExt->infoQueue1)
		deviceExt->infoQueue1->lpVtbl->Release(deviceExt->infoQueue1);

	ListDxCommandAllocator_freex(&deviceExt->commandPools);

	//Free temp storage

	ListD3D12_BUFFER_BARRIER_freex(&deviceExt->bufferTransitions);
	ListD3D12_TEXTURE_BARRIER_freex(&deviceExt->imageTransitions);

	return true;
}

//Executing commands */

Error GraphicsDeviceRef_waitExt(GraphicsDeviceRef *deviceRef) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	const HANDLE eventHandle = CreateEventExW(NULL, NULL, 0, EVENT_ALL_ACCESS);
	Error err;

	gotoIfError(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
		deviceExt->commitSemaphore, device->submitId, eventHandle
	)))

	WaitForSingleObject(eventHandle, INFINITE);

clean:
	CloseHandle(eventHandle);
	return err;
}

DxCommandAllocator *DxGraphicsDevice_getCommandAllocator(
	const DxGraphicsDevice *device,
	const U32 resolvedQueueId,
	const U64 threadId,
	const U8 backBufferId
) {

	const U64 threadCount = Platform_instance.threads;

	if(!device || resolvedQueueId >= 3 || threadId >= threadCount || backBufferId >= 3)
		return NULL;

	const U64 id = resolvedQueueId + (backBufferId * threadCount + threadId) * 3;

	return device->commandPools.ptrNonConst + id;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Error GraphicsDevice_submitCommandsImpl(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains
) {
	(void)deviceRef; (void)commandLists; (void)swapchains;
	return Error_unimplemented(0, "GraphicsDevice_submitCommandsImpl() is unimplemented");
}

/*
	//Unpack ext

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	CharString temp = CharString_createNull();
	Error err = Error_none();

	//Reserve temp storage

	gotoIfError(clean, ListVkSwapchainKHR_clear(&deviceExt->swapchainHandles))
	gotoIfError(clean, ListVkSwapchainKHR_reservex(&deviceExt->swapchainHandles, swapchains.length))

	gotoIfError(clean, ListU32_clear(&deviceExt->swapchainIndices))
	gotoIfError(clean, ListU32_reservex(&deviceExt->swapchainIndices, swapchains.length))

	gotoIfError(clean, ListVkResult_clear(&deviceExt->results))
	gotoIfError(clean, ListVkResult_reservex(&deviceExt->results, swapchains.length))

	gotoIfError(clean, ListVkSemaphore_clear(&deviceExt->waitSemaphores))
	gotoIfError(clean, ListVkSemaphore_reservex(&deviceExt->waitSemaphores, swapchains.length + 1))

	gotoIfError(clean, ListVkPipelineStageFlags_clear(&deviceExt->waitStages))
	gotoIfError(clean, ListVkPipelineStageFlags_reservex(&deviceExt->waitStages, swapchains.length + 1))

	//Wait for previous frame semaphore

	if (device->submitId >= 3) {

		U64 value = device->submitId - 3 + 1;

		VkSemaphoreWaitInfo waitInfo = (VkSemaphoreWaitInfo) {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.pSemaphores = &deviceExt->commitSemaphore,
			.semaphoreCount = 1,
			.pValues = &value
		};

		gotoIfError(clean, vkCheck(vkWaitSemaphores(deviceExt->device, &waitInfo, U64_MAX)))
	}

	//Acquire swapchain images

	for(U64 i = 0; i < swapchains.length; ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(swapchains.ptr[i]);
		VkSwapchain *swapchainExt = TextureRef_getImplExtT(VkSwapchain, swapchains.ptr[i]);

		VkSemaphore semaphore = swapchainExt->semaphores.ptr[device->submitId % swapchainExt->semaphores.length];

		UnifiedTexture *unifiedTexture = TextureRef_getUnifiedTextureIntern(swapchains.ptr[i], NULL);
		U32 currImg = 0;

		gotoIfError(clean, vkCheck(instanceExt->acquireNextImage(
			deviceExt->device,
			swapchainExt->swapchain,
			U64_MAX,
			semaphore,
			VK_NULL_HANDLE,
			&currImg
		)))

		unifiedTexture->currentImageId = (U8) currImg;

		deviceExt->swapchainHandles.ptrNonConst[i] = swapchainExt->swapchain;
		deviceExt->swapchainIndices.ptrNonConst[i] = unifiedTexture->currentImageId;

		VkPipelineStageFlagBits pipelineStage =
			(swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0) |
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_TRANSFER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		gotoIfError(clean, ListVkSemaphore_pushBackx(&deviceExt->waitSemaphores, semaphore))
		gotoIfError(clean, ListVkPipelineStageFlags_pushBackx(&deviceExt->waitStages, pipelineStage))
	}

	//Prepare per frame cbuffer

	{
		DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData);
		CBufferData *data = (CBufferData*)frameData->resource.mappedMemoryExt + (device->submitId % 3);

		for (U32 i = 0; i < data->swapchainCount; ++i) {

			SwapchainRef *swapchainRef = swapchains.ptr[i];
			Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);

			Bool allowCompute = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

			UnifiedTextureImage managedImage = TextureRef_getCurrImage(swapchainRef, 0);

			data->swapchains[i * 2 + 0] = managedImage.readHandle;
			data->swapchains[i * 2 + 1] = allowCompute ? managedImage.writeHandle : 0;
		}

		DeviceMemoryBlock block = device->allocator.blocks.ptr[frameData->resource.blockId];
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (incoherent) {

			VkMappedMemoryRange range = (VkMappedMemoryRange){
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory)block.ext,
				.offset = frameData->resource.blockOffset + (device->submitId % 3) * sizeof(CBufferData),
				.size = sizeof(CBufferData)
			};

			gotoIfError(clean, vkCheck(vkFlushMappedMemoryRanges(deviceExt->device, 1, &range)))
		}
	}

	//Record command list

	VkCommandBuffer commandBuffer = NULL;

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	U32 graphicsQueueId = queue.queueId;

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];

	if (commandLists.length) {

		U32 threadId = 0;

		VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, (U8)(device->submitId % 3)
		);

		if(!allocator)
			gotoIfError(clean, Error_nullPointer(0, "GraphicsDevice_submitCommandsImpl() command allocator is NULL"))

		//We create command pools only the first 3 frames, after that they're cached.
		//This is because we have space for [queues][threads][3] command pools.
		//Allocating them all even though currently only 1x3 are used is quite suboptimal.
		//We only have the space to allow for using more in the future.

		if(!allocator->pool) {

			//TODO: Multi thread command recording

			VkCommandPoolCreateInfo poolInfo = (VkCommandPoolCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
				.queueFamilyIndex = queue.queueId
			};

			gotoIfError(clean, vkCheck(vkCreateCommandPool(deviceExt->device, &poolInfo, NULL, &allocator->pool)))

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				gotoIfError(clean, CharString_formatx(
					&temp,
					"%s command pool (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
						queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId,
					device->submitId % 3
				))

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_COMMAND_POOL,
					.pObjectName = temp.ptr,
					.objectHandle = (U64) allocator->pool
				};

				gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)))

				CharString_freex(&temp);
			}
		}

		else gotoIfError(clean, vkCheck(vkResetCommandPool(
			deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
		)))

		//Allocate command buffer if not present yet

		if (!allocator->cmd) {

			VkCommandBufferAllocateInfo bufferInfo = (VkCommandBufferAllocateInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = allocator->pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			gotoIfError(clean, vkCheck(vkAllocateCommandBuffers(deviceExt->device, &bufferInfo, &allocator->cmd)))

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				gotoIfError(clean, CharString_formatx(
					&temp,
					"%s command buffer (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
						queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId,
					device->submitId % 3
				))

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
					.pObjectName = temp.ptr,
					.objectHandle = (U64) allocator->cmd
				};

				gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)))

				CharString_freex(&temp);
			}
		}

		//Start buffer

		commandBuffer = allocator->cmd;

		VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		};

		gotoIfError(clean, vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo)))

		//Start copies

		gotoIfError(clean, GraphicsDeviceRef_handleNextFrame(deviceRef, commandBuffer))

		VkCommandBufferState state = (VkCommandBufferState) { .buffer = commandBuffer };
		state.tempPrimitiveTopology = U8_MAX;
		state.tempStencilRef = 0;

		//Ensure ubo and staging buffer are the correct states

		VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

		VkDeviceBuffer *uboExt = DeviceBuffer_ext(DeviceBufferRef_ptr(device->frameData), Vk);

		gotoIfError(clean, VkDeviceBuffer_transition(
			uboExt,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_UNIFORM_READ_BIT,
			graphicsQueueId,
			(device->submitId % 3) * sizeof(CBufferData),
			sizeof(CBufferData),
			&deviceExt->bufferTransitions,
			&dependency
		))

		if(dependency.bufferMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);

		//Bind pipeline layout and descriptors since they stay the same for the entire frame.
		//For every bind point

		VkDescriptorSet sets[EDescriptorSetType_UniqueLayouts];

		for(U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i)
			sets[i] =
				i != EDescriptorSetType_CBuffer0 ? deviceExt->sets[i] :
				deviceExt->sets[EDescriptorSetType_CBuffer0 + (device->submitId % 3)];

		U64 bindingCount = device->info.capabilities.features & EGraphicsFeatures_RayPipeline ? 3 : 2;

		for(U64 i = 0; i < bindingCount; ++i)
			vkCmdBindDescriptorSets(
				commandBuffer,
				i == 0 ? VK_PIPELINE_BIND_POINT_COMPUTE : (
					i == 1 ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
				),
				deviceExt->defaultLayout,
				0, EDescriptorSetType_UniqueLayouts, sets,
				0, NULL
			);

		//Record commands

		for (U64 i = 0; i < commandLists.length; ++i) {

			state.scopeCounter = 0;
			CommandList *commandList = CommandListRef_ptr(commandLists.ptr[i]);
			const U8 *ptr = commandList->data.ptr;

			for (U64 j = 0; j < commandList->commandOps.length; ++j) {

				CommandOpInfo info = commandList->commandOps.ptr[j];

				//Extra debugging if an error happens while processing the command

				CommandList_process(commandList, device, info.op, ptr, &state);
				ptr += info.opSize;
			}
		}

		//Transition back swapchains to present

		//Combine transitions into one call.

		dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		for (U64 i = 0; i < swapchains.length; ++i) {

			SwapchainRef *swapchainRef = swapchains.ptr[i];
			VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(swapchainRef, Vk, 0);

			VkImageSubresourceRange range = (VkImageSubresourceRange) {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			};

			gotoIfError(clean, VkUnifiedTexture_transition(
				imageExt,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
				0,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				graphicsQueueId,
				&range,
				&deviceExt->imageTransitions,
				&dependency
			))

			if(RefPtr_inc(swapchainRef))
				gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, swapchainRef))
		}

		if(dependency.imageMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);

		//End buffer

		gotoIfError(clean, vkCheck(vkEndCommandBuffer(commandBuffer)))
	}

	//Submit queue
	//TODO: Multiple queues

	U64 waitValue = device->submitId - 3 + 1;

	VkSemaphore signalSemaphores[2] = {
		deviceExt->commitSemaphore,
		deviceExt->submitSemaphores.ptr[device->submitId % 3]
	};

	U64 signalValues[2] = { device->submitId + 1, 1 };

	VkTimelineSemaphoreSubmitInfo timelineInfo = (VkTimelineSemaphoreSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
		.waitSemaphoreValueCount = device->submitId >= 3,
		.pWaitSemaphoreValues = device->submitId >= 3 ? &waitValue : NULL,
		.signalSemaphoreValueCount = swapchains.length ? 2 : 1,
		.pSignalSemaphoreValues = signalValues
	};

	VkSubmitInfo submitInfo = (VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = &timelineInfo,
		.waitSemaphoreCount = (U32) deviceExt->waitSemaphores.length,
		.pWaitSemaphores = deviceExt->waitSemaphores.ptr,
		.signalSemaphoreCount = swapchains.length ? 2 : 1,
		.pSignalSemaphores = signalSemaphores,
		.pCommandBuffers = &commandBuffer,
		.commandBufferCount = commandBuffer ? 1 : 0,
		.pWaitDstStageMask = deviceExt->waitStages.ptr
	};

	gotoIfError(clean, vkCheck(vkQueueSubmit(queue.queue, 1, &submitInfo, VK_NULL_HANDLE)))

	//Presents

	if(swapchains.length) {

		VkPresentInfoKHR presentInfo = (VkPresentInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = deviceExt->submitSemaphores.ptr + device->submitId % 3,
			.swapchainCount = (U32) swapchains.length,
			.pSwapchains = deviceExt->swapchainHandles.ptr,
			.pImageIndices = deviceExt->swapchainIndices.ptr,
			.pResults = deviceExt->results.ptrNonConst
		};

		gotoIfError(clean, vkCheck(vkQueuePresentKHR(queue.queue, &presentInfo)))

		for(U64 i = 0; i < deviceExt->results.length; ++i)
			gotoIfError(clean, vkCheck(deviceExt->results.ptr[i]))
	}

clean:

	ListVkImageCopy_clear(&deviceExt->imageCopyRanges);
	ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);
	ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);
	CharString_freex(&temp);

	return err;
}*/

Error DxGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, DxCommandBuffer *commandBuffer) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	//End current command list

	HANDLE eventHandle = NULL;
	Error err;
	gotoIfError(clean, dxCheck(commandBuffer->lpVtbl->Close(commandBuffer)))

	//Submit only the copy command list

	ID3D12CommandList *commandList = NULL;
	gotoIfError(clean, dxCheck(commandBuffer->lpVtbl->QueryInterface(
		commandBuffer, &IID_ID3D12CommandList, (void**) &commandList
	)))

	if(device->submitId) {		//Ensure GPU is complete, so we don't override anything

		eventHandle = CreateEventExW(NULL, NULL, 0, EVENT_ALL_ACCESS);

		gotoIfError(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
			deviceExt->commitSemaphore, device->submitId - 1, eventHandle
		)))

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
		eventHandle = NULL;
	}

	const DxCommandQueue queue = deviceExt->queues[EDxCommandQueue_Graphics];
	queue.queue->lpVtbl->ExecuteCommandLists(queue.queue, 1, &commandList);
	gotoIfError(clean, dxCheck(queue.queue->lpVtbl->Signal(queue.queue, deviceExt->commitSemaphore, device->submitId)))

	//Wait for the device

	gotoIfError(clean, GraphicsDeviceRef_wait(deviceRef))

	//Reset command list

	const U32 threadId = 0;

	const DxCommandAllocator *allocator = DxGraphicsDevice_getCommandAllocator(
		deviceExt, queue.resolvedQueueId, threadId, (U8)(device->submitId % 3)
	);

	gotoIfError(clean, dxCheck(commandBuffer->lpVtbl->Reset(commandBuffer, allocator->pool, NULL)))

clean:

	if(eventHandle)
		CloseHandle(eventHandle);

	return err;
}