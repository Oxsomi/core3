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

#define INITGUID
#include <guiddef.h>
#include "platforms/ext/listx_impl.h"
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
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/math/math.h"
#include "types/base/thread.h"

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

Error DX_WRAP_FUNC(GraphicsDevice_init)(
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

	gotoIfError(clean, dxCheck(instanceExt->deviceFactory->lpVtbl->CreateDevice(
		instanceExt->deviceFactory,
		(IUnknown*)deviceExt->adapter4, D3D_FEATURE_LEVEL_11_1,
		&IID_ID3D12Device10, (void**) &deviceExt->device
	)))

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->QueryInterface(
		deviceExt->device, &IID_ID3D12DeviceConfiguration1, (void**) &deviceExt->deviceConfig
	)))

	Bool isNv = device->info.vendor == EGraphicsVendorId_NV;

	if(device->flags & EGraphicsDeviceFlags_IsDebug) {

		gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12DebugDevice, (void**) &deviceExt->debugDevice
		)))

		//Get infoQueue0 to disable some bogus validation messages

		if(SUCCEEDED(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12InfoQueue, (void**) &deviceExt->infoQueue0
		))) {

			gotoIfError(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->SetBreakOnSeverity(
				deviceExt->infoQueue0, D3D12_MESSAGE_SEVERITY_CORRUPTION, true
			)))

			gotoIfError(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->SetBreakOnSeverity(
				deviceExt->infoQueue0, D3D12_MESSAGE_SEVERITY_ERROR, true
			)))

			D3D12_MESSAGE_ID hide[] = {
				D3D12_MESSAGE_ID_CREATEDEVICE_DEBUG_LAYER_STARTUP_OPTIONS,
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
			};

			D3D12_INFO_QUEUE_FILTER filter = (D3D12_INFO_QUEUE_FILTER) {
				.DenyList = (D3D12_INFO_QUEUE_FILTER_DESC) {
					.NumIDs = (U32)(sizeof(hide) / sizeof(hide[0])),
					.pIDList = hide
				}
			};

			gotoIfError(clean, dxCheck(deviceExt->infoQueue0->lpVtbl->AddStorageFilterEntries(
				deviceExt->infoQueue0, &filter
			)))
		}

		//Get info queue 1 for validation errors in the console on Win11

		if(SUCCEEDED(deviceExt->device->lpVtbl->QueryInterface(
			deviceExt->device,
			&IID_ID3D12InfoQueue1, (void**) &deviceExt->infoQueue1
		))) {

			DWORD callbackCookie = 0;

			gotoIfError(clean, dxCheck(deviceExt->infoQueue1->lpVtbl->RegisterMessageCallback(
				deviceExt->infoQueue1,
				onDebugReport,
				D3D12_MESSAGE_CALLBACK_FLAG_NONE,
				NULL,
				&callbackCookie
			)))
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
						gotoIfError(clean, Error_invalidState(
							0, "NvAPI_D3D12_RegisterRaytracingValidationMessageCallback couldn't be called"
						))
				}
			}

		#endif
	}

	#if _ARCH == ARCH_X86_64

		//Enable NV extensions

		static const U32 nvExtSlot = 99999;		//space and u slot
		EGraphicsFeatures nvExt =
			EGraphicsFeatures_RayMicromapOpacity | EGraphicsFeatures_RayMicromapDisplacement |
			EGraphicsFeatures_RayReorder | EGraphicsFeatures_RayValidation;

		if(isNv && (device->info.capabilities.features & nvExt)) {

			NvAPI_Status status = NvAPI_D3D12_SetNvShaderExtnSlotSpace((IUnknown*)deviceExt->device, nvExtSlot, nvExtSlot);

			if(status != NVAPI_OK)
				gotoIfError(clean, Error_invalidState(0, "NvAPI_D3D12_SetNvShaderExtnSlotSpace couldn't be called"))
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

	U64 threads = Platform_getThreads();
	gotoIfError(clean, ListDxCommandAllocator_resizex(&deviceExt->commandPools, 3 * threads * 3))

	//Create fence

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateFence(
		deviceExt->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**) &deviceExt->commitSemaphore
	)))

	//Create root signature

	D3D12_DESCRIPTOR_RANGE descRanges[17] = {

		//Unused register, but nv wants it

		(D3D12_DESCRIPTOR_RANGE) {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			.BaseShaderRegister = nvExtSlot,
			.RegisterSpace = nvExtSlot,
			.NumDescriptors = 1,
			.OffsetInDescriptorsFromTableStart = EDescriptorTypeOffsets_UAVEnd
		}
	};

	for(U32 i = 0; i < 16; ++i) {

		EDescriptorType type = i == 15 ? EDescriptorType_TLASExt : i;
		U32 offset = EDescriptorTypeOffsets_values[type];
		Bool isSrv = offset >= EDescriptorTypeOffsets_SRVStart && offset < EDescriptorTypeOffsets_SRVEnd;

		descRanges[i + 1] = (D3D12_DESCRIPTOR_RANGE) {
			.RangeType = isSrv ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			.NumDescriptors = descriptorTypeCount[type],
			.OffsetInDescriptorsFromTableStart = offset,
			.RegisterSpace = i
		};
	}

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

	err = dxCheck(deviceExt->deviceConfig->lpVtbl->SerializeVersionedRootSignature(
		deviceExt->deviceConfig, &rootSig, &rootSigBlob, &errBlob
	));

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

	device->flushThresholdPrimitives = 20 * MIBI / 3;		//20M vertices per frame limit

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
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(D3D12DispatchRaysIndirect), .pArgumentDescs = &sigDesc[1] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallIndexed), .pArgumentDescs = &sigDesc[2] },
		(D3D12_COMMAND_SIGNATURE_DESC) { .ByteStride = sizeof(DrawCallUnindexed), .pArgumentDescs = &sigDesc[3] }
	};

	for(U64 i = 0; i < EExecuteIndirectCommand_Count; ++i) {

		if (i == EExecuteIndirectCommand_DispatchRays && !(device->info.capabilities.features & EGraphicsFeatures_RayPipeline))
			continue;

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

void DX_WRAP_FUNC(GraphicsDevice_postInit)(GraphicsDevice *device) {		//No-op in D3D12, CBV can be made/bound at runtime :)
	(void)device;
}

Bool DX_WRAP_FUNC(GraphicsDevice_free)(const GraphicsInstance *instance, void *ext) {

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

	ListDxCommandAllocator_freex(&deviceExt->commandPools);

	//Free temp storage

	ListD3D12_BUFFER_BARRIER_freex(&deviceExt->bufferTransitions);
	ListD3D12_TEXTURE_BARRIER_freex(&deviceExt->imageTransitions);

	return true;
}

//Executing commands

Error DX_WRAP_FUNC(GraphicsDeviceRef_wait)(GraphicsDeviceRef *deviceRef) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	U64 completedValue = deviceExt->commitSemaphore->lpVtbl->GetCompletedValue(deviceExt->commitSemaphore);

	if (completedValue >= deviceExt->fenceId)
		return Error_none();

	const HANDLE eventHandle = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	Error err;

	gotoIfError(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
		deviceExt->commitSemaphore, deviceExt->fenceId, eventHandle
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

	const U64 threadCount = Platform_getThreads();

	if(!device || resolvedQueueId >= 3 || threadId >= threadCount || backBufferId >= 3)
		return NULL;

	const U64 id = resolvedQueueId + (backBufferId * threadCount + threadId) * 3;

	return device->commandPools.ptrNonConst + id;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Error DX_WRAP_FUNC(GraphicsDevice_submitCommands)(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains,
	CBufferData cbufferData
) {

	//Unpack ext

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	Error err = Error_none();
	HANDLE eventHandle = NULL;
	CharString temp = CharString_createNull();
	ListU16 temp16 = (ListU16) { 0 };

	//Wait for previous frame semaphore

	++deviceExt->fenceId;

	if (deviceExt->fenceId > 3) {

		eventHandle = CreateEventExW(NULL, NULL, 0, EVENT_ALL_ACCESS);

		gotoIfError(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
			deviceExt->commitSemaphore, deviceExt->fenceId - 3, eventHandle
		)))

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
		eventHandle = NULL;
	}

	//Acquire swapchain images in D3D12 is just a simple sequential id.
	//No mailbox, so no problem.

	for(U64 i = 0; i < swapchains.length; ++i) {
		UnifiedTexture *unifiedTexture = TextureRef_getUnifiedTextureIntern(swapchains.ptr[i], NULL);
		unifiedTexture->currentImageId = (device->submitId - 1) % 3;
	}

	//Prepare per frame cbuffer

	DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[(device->submitId - 1) % 3]);

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

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];

	if (commandLists.length) {

		U32 threadId = 0;

		DxCommandAllocator *allocator = DxGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, (device->submitId - 1) % 3
		);

		if(!allocator)
			gotoIfError(clean, Error_nullPointer(0, "D3D12GraphicsDevice_submitCommands() command allocator is NULL"))

		//We create command pools only the first 3 frames, after that they're cached.
		//This is because we have space for [queues][threads][3] command pools.
		//Allocating them all even though currently only 1x3 are used is quite suboptimal.
		//We only have the space to allow for using more in the future.

		if(!allocator->pool) {

			//TODO: Multi thread command recording

			gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandAllocator(
				deviceExt->device,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				&IID_ID3D12CommandAllocator,
				(void**) &allocator->pool
			)))

			if(device->flags & EGraphicsDeviceFlags_IsDebug) {

				gotoIfError(clean, CharString_formatx(
					&temp,
					"%s command pool (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EDxCommandQueue_Graphics ? "Graphics" : (
						queue.type == EDxCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId,
					(device->submitId - 1) % 3
				))

				gotoIfError(clean, CharString_toUTF16x(temp, &temp16))

				gotoIfError(clean, dxCheck(allocator->pool->lpVtbl->SetName(allocator->pool, temp16.ptr)))
				CharString_freex(&temp);
				ListU16_freex(&temp16);
			}
		}

		//Allocate command buffer if not present yet

		Bool isNew = !allocator->cmd;

		if (isNew) {

			gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommandList(
				deviceExt->device,
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				allocator->pool,
				NULL,
				&IID_ID3D12GraphicsCommandList10,
				(void**) &allocator->cmd
			)))

			if(device->flags & EGraphicsDeviceFlags_IsDebug) {

				gotoIfError(clean, CharString_formatx(
					&temp,
					"%s command buffer (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EDxCommandQueue_Graphics ? "Graphics" : (
						queue.type == EDxCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId,
					(device->submitId - 1) % 3
				))

				gotoIfError(clean, CharString_toUTF16x(temp, &temp16))
				gotoIfError(clean, dxCheck(allocator->cmd->lpVtbl->SetName(allocator->cmd, temp16.ptr)))
				CharString_freex(&temp);
				ListU16_freex(&temp16);
			}
		}

		//Start buffer

		commandBuffer = allocator->cmd;

		if(!isNew) {
			gotoIfError(clean, dxCheck(allocator->pool->lpVtbl->Reset(allocator->pool)))
			gotoIfError(clean, dxCheck(commandBuffer->lpVtbl->Reset(commandBuffer, allocator->pool, NULL)))
		}

		//Start copies

		DxCommandBufferState state = (DxCommandBufferState) { .buffer = commandBuffer };
		state.boundPrimitiveTopology = U8_MAX;

		gotoIfError(clean, GraphicsDeviceRef_handleNextFrame(deviceRef, &state))

		//Ensure ubo and staging buffer are the correct states

		D3D12_BARRIER_GROUP dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

		DxDeviceBuffer *uboExt = DeviceBuffer_ext(frameData, Dx);

		gotoIfError(clean, DxDeviceBuffer_transition(
			uboExt,
			D3D12_BARRIER_SYNC_VERTEX_SHADING,
			D3D12_BARRIER_ACCESS_CONSTANT_BUFFER,
			&deviceExt->bufferTransitions,
			&dependency
		))

		if(dependency.NumBarriers)
			commandBuffer->lpVtbl->Barrier(commandBuffer, 1, &dependency);

		ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);

		//Bind descriptor heaps, root signature and descriptor tables since they stay the same for the entire frame.
		//For every bind point.

		ID3D12DescriptorHeap *descriptorHeaps[] = {
			deviceExt->heaps[EDescriptorHeapType_Resources].heap,
			deviceExt->heaps[EDescriptorHeapType_Sampler].heap
		};

		D3D12_GPU_DESCRIPTOR_HANDLE descriptorTable[] = {
			deviceExt->heaps[EDescriptorHeapType_Resources].gpuHandle,
			deviceExt->heaps[EDescriptorHeapType_Sampler].gpuHandle
		};

		commandBuffer->lpVtbl->SetDescriptorHeaps(commandBuffer, 2, descriptorHeaps);

		commandBuffer->lpVtbl->SetComputeRootSignature(commandBuffer, deviceExt->defaultLayout);
		commandBuffer->lpVtbl->SetGraphicsRootSignature(commandBuffer, deviceExt->defaultLayout);

		for(U32 i = 0; i < 2; ++i) {
			commandBuffer->lpVtbl->SetComputeRootDescriptorTable(commandBuffer, i, descriptorTable[i]);
			commandBuffer->lpVtbl->SetGraphicsRootDescriptorTable(commandBuffer, i, descriptorTable[i]);
		}

		D3D12_GPU_VIRTUAL_ADDRESS cbvLoc = frameData->resource.deviceAddress;

		commandBuffer->lpVtbl->SetComputeRootConstantBufferView(commandBuffer, 2, cbvLoc);
		commandBuffer->lpVtbl->SetGraphicsRootConstantBufferView(commandBuffer, 2, cbvLoc);

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

			gotoIfError(clean, DxUnifiedTexture_transition(
				imageExt,
				D3D12_BARRIER_SYNC_RENDER_TARGET,
				D3D12_BARRIER_ACCESS_COMMON,
				D3D12_BARRIER_LAYOUT_PRESENT,
				&range,
				&deviceExt->imageTransitions,
				&dependency
			))

			if(RefPtr_inc(swapchainRef))
				gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, swapchainRef))
		}

		if(dependency.NumBarriers)
			commandBuffer->lpVtbl->Barrier(commandBuffer, 1, &dependency);

		ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions);

		//End buffer

		gotoIfError(clean, dxCheck(commandBuffer->lpVtbl->Close(commandBuffer)))
	}

	//Submit queue
	//TODO: Multiple queues

	queue.queue->lpVtbl->ExecuteCommandLists(queue.queue, 1, (ID3D12CommandList**) &commandBuffer);

	//Presents

	for(U64 i = 0; i < swapchains.length; ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(swapchains.ptr[i]);
		DxSwapchain *swapchainExt = TextureRef_getImplExtT(DxSwapchain, swapchains.ptr[i]);

		Bool allowTearing = swapchain->presentMode == ESwapchainPresentMode_Immediate;

		DXGI_PRESENT_PARAMETERS regions = (DXGI_PRESENT_PARAMETERS) { 0 };

		gotoIfError(clean, dxCheck(swapchainExt->swapchain->lpVtbl->Present1(
			swapchainExt->swapchain,
			allowTearing ? 0 : 1,
			allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0,
			&regions
		)))
	}

	//Fence value after present

	gotoIfError(clean, dxCheck(queue.queue->lpVtbl->Signal(queue.queue, deviceExt->commitSemaphore, deviceExt->fenceId)))

clean:

	#if _ARCH == ARCH_X86_64

		//Regardless of device removal, we'll ask NV to report anything fishy to us.
		//It's technically possible that a TDR/Device removal is caused during setup time,
		//but it's very unlikely. As we do the bulk of D3D12 calls and all RT calls in submitCommands.
		//Otherwise we'd have to guard every dxCheck, which might not even have a device (could happen on an instance).

		if(device->info.capabilities.features & EGraphicsFeatures_RayValidation) {

			NvAPI_Status status = NvAPI_D3D12_FlushRaytracingValidationMessages((ID3D12Device5*)deviceExt->device);

			if(status != NVAPI_OK)
				gotoIfError(clean, Error_invalidState(0, "D3D12GraphicsDevice_submitCommands() flush RT val msgs failed"));
		}

	#endif

	if(eventHandle)
		CloseHandle(eventHandle);

	ListU16_freex(&temp16);
	CharString_freex(&temp);
	return err;
}

Error DxGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, DxCommandBufferState *commandBuffer) {

	if(commandBuffer->inRender)
		return Error_invalidState(
			0, "DxGraphicsDevice_flush() can't flush while in render, because it can't efficiently be split up"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	//End current command list

	HANDLE eventHandle = NULL;
	Error err;
	gotoIfError(clean, dxCheck(commandBuffer->buffer->lpVtbl->Close(commandBuffer->buffer)))

	//Submit only the copy command list

	if(deviceExt->fenceId) {		//Ensure GPU is complete, so we don't override anything

		eventHandle = CreateEventExW(NULL, NULL, 0, EVENT_ALL_ACCESS);

		gotoIfError(clean, dxCheck(deviceExt->commitSemaphore->lpVtbl->SetEventOnCompletion(
			deviceExt->commitSemaphore, deviceExt->fenceId, eventHandle
		)))

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
		eventHandle = NULL;
	}

	++deviceExt->fenceId;
	const DxCommandQueue queue = deviceExt->queues[EDxCommandQueue_Graphics];
	queue.queue->lpVtbl->ExecuteCommandLists(queue.queue, 1, (ID3D12CommandList**) &commandBuffer->buffer);
	gotoIfError(clean, dxCheck(queue.queue->lpVtbl->Signal(queue.queue, deviceExt->commitSemaphore, deviceExt->fenceId)))

	//Wait for the device

	gotoIfError(clean, GraphicsDeviceRef_wait(deviceRef))

	//Reset command list

	const U32 threadId = 0;

	const DxCommandAllocator *allocator = DxGraphicsDevice_getCommandAllocator(
		deviceExt, queue.resolvedQueueId, threadId, (U8)((device->submitId - 1) % 3)
	);

	gotoIfError(clean, dxCheck(commandBuffer->buffer->lpVtbl->Reset(commandBuffer->buffer, allocator->pool, NULL)))

	commandBuffer->pipeline = NULL;
	commandBuffer->boundScissor = (D3D12_RECT) { 0 };
	commandBuffer->boundViewport = (D3D12_VIEWPORT) { 0 };
	commandBuffer->boundBuffers = (SetPrimitiveBuffersCmd) { 0 };
	commandBuffer->boundPrimitiveTopology = U8_MAX;
	commandBuffer->stencilRef = 0;
	commandBuffer->blendConstants = F32x4_zero();

clean:

	if(eventHandle)
		CloseHandle(eventHandle);

	return err;
}
