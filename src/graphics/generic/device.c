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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "types/time.h"
#include "types/type_cast.h"

TListNamedImpl(ListLockPtr);
TListNamedImpl(ListCommandListRef);
TListNamedImpl(ListSwapchainRef);

void GraphicsDeviceInfo_print(const GraphicsDeviceInfo *deviceInfo, Bool printCapabilities) {

	if(!deviceInfo || !deviceInfo->ext)
		return;

	Log_debugLnx(
		"%s (%s %s):\n\t%s %u\n\tLUID %016llx\n\tUUID %016llx%016llx", 
		deviceInfo->name, 
		deviceInfo->driverName, 
		deviceInfo->driverInfo,
		(deviceInfo->type == EGraphicsDeviceType_CPU ? "CPU" : (
			deviceInfo->type == EGraphicsDeviceType_Dedicated ? "dGPU" : (
				deviceInfo->type == EGraphicsDeviceType_Integrated ? "iGPU" : "Simulated GPU"
			)
		)),
		deviceInfo->id,
		deviceInfo->capabilities.features & EGraphicsFeatures_LUID ? U64_swapEndianness(deviceInfo->luid) : 0,
		U64_swapEndianness(deviceInfo->uuid[0]),
		U64_swapEndianness(deviceInfo->uuid[1])
	);

	if (printCapabilities) {

		GraphicsDeviceCapabilities cap = deviceInfo->capabilities;

		//Features

		U32 feat = cap.features;

		Log_debugLnx("\tFeatures:");

		if(feat & EGraphicsFeatures_DirectRendering)
			Log_debugLnx("\t\tDirect rendering");

		//if(feat & EGraphicsFeatures_VariableRateShading)
		//	Log_debugLnx("\t\tVariable rate shading");

		if(feat & EGraphicsFeatures_MultiDrawIndirectCount)
			Log_debugLnx("\t\tMulti draw indirect count");

		if(feat & EGraphicsFeatures_MeshShader)
			Log_debugLnx("\t\tMesh shaders");

		if(feat & EGraphicsFeatures_GeometryShader)
			Log_debugLnx("\t\tGeometry shaders");

		if(feat & EGraphicsFeatures_TessellationShader)
			Log_debugLnx("\t\tTessellation shaders");

		if(feat & EGraphicsFeatures_SubgroupArithmetic)
			Log_debugLnx("\t\tSubgroup arithmetic");

		if(feat & EGraphicsFeatures_SubgroupShuffle)
			Log_debugLnx("\t\tSubgroup shuffle");

		if(feat & EGraphicsFeatures_Swapchain)
			Log_debugLnx("\t\tSwapchain");

		//if(feat & EGraphicsFeatures_Multiview)
		//	Log_debugLnx("\t\tMultiview");

		if(feat & EGraphicsFeatures_Raytracing)
			Log_debugLnx("\t\tRaytracing");

		if(feat & EGraphicsFeatures_RayPipeline)
			Log_debugLnx("\t\tRaytracing pipeline");

		if(feat & EGraphicsFeatures_RayQuery)
			Log_debugLnx("\t\tRay query");

		if(feat & EGraphicsFeatures_RayIndirect)
			Log_debugLnx("\t\tTraceRay indirect");

		if(feat & EGraphicsFeatures_RayMicromapOpacity)
			Log_debugLnx("\t\tRaytracing opacity micromap");

		if(feat & EGraphicsFeatures_RayMicromapDisplacement)
			Log_debugLnx("\t\tRaytracing displacement micromap");

		if(feat & EGraphicsFeatures_RayMotionBlur)
			Log_debugLnx("\t\tRaytracing motion blur");

		if(feat & EGraphicsFeatures_RayReorder)
			Log_debugLnx("\t\tRay reorder");

		if(feat & EGraphicsFeatures_DebugMarkers)
			Log_debugLnx("\t\tDebug markers");

		if(feat & EGraphicsFeatures_Wireframe)
			Log_debugLnx("\t\tWireframe (rasterizer fill mode: line)");

		if(feat & EGraphicsFeatures_LogicOp)
			Log_debugLnx("\t\tLogic op (blend state)");

		if(feat & EGraphicsFeatures_DualSrcBlend)
			Log_debugLnx("\t\tDual src blend (blend state)");

		//Data types

		U32 dat = cap.dataTypes;

		Log_debugLnx("\tData types:");
		
		if(dat & EGraphicsDataTypes_I64)
			Log_debugLnx("\t\t64-bit integers");
		
		if(dat & EGraphicsDataTypes_F16)
			Log_debugLnx("\t\t16-bit floats");
		
		if(dat & EGraphicsDataTypes_F64)
			Log_debugLnx("\t\t64-bit floats");
		
		if(dat & EGraphicsDataTypes_AtomicI64)
			Log_debugLnx("\t\t64-bit integer atomics (buffer)");
		
		if(dat & EGraphicsDataTypes_AtomicF32)
			Log_debugLnx("\t\t32-bit float atomics (buffer)");
		
		if(dat & EGraphicsDataTypes_AtomicF64)
			Log_debugLnx("\t\t64-bit float atomics (buffer)");
		
		if(dat & EGraphicsDataTypes_ASTC)
			Log_debugLnx("\t\tASTC compression");
		
		if(dat & EGraphicsDataTypes_BCn)
			Log_debugLnx("\t\tBCn compression");
		
		if(dat & EGraphicsDataTypes_MSAA2x)
			Log_debugLnx("\t\tMSAA 2x");
		
		if(dat & EGraphicsDataTypes_MSAA8x)
			Log_debugLnx("\t\tMSAA 8x");

		if(dat & EGraphicsDataTypes_MSAA16x)
			Log_debugLnx("\t\tMSAA 16x");
	}
}

Error GraphicsDeviceRef_dec(GraphicsDeviceRef **device) {
	return !RefPtr_dec(device) ? Error_invalidOperation(0, "GraphicsDeviceRef_dec()::device is invalid") : Error_none();
}

Error GraphicsDeviceRef_inc(GraphicsDeviceRef *device) {
	return !RefPtr_inc(device) ? Error_invalidOperation(0, "GraphicsDeviceRef_inc()::device is invalid") : Error_none();
}

//Ext

impl extern const U64 GraphicsDeviceExt_size;

impl Error GraphicsDevice_initExt(
	const GraphicsInstance *instance, 
	const GraphicsDeviceInfo *deviceInfo, 
	Bool verbose,
	GraphicsDeviceRef **deviceRef
);

impl void GraphicsDevice_postInit(GraphicsDevice *device);
impl Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext);

Bool GraphicsDevice_free(GraphicsDevice *device, Allocator alloc) {

	alloc;

	if(!device)
		return true;

	for(U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i) {

		for(U64 j = 0; j < device->resourcesInFlight[i].length; ++j)
			RefPtr_dec(device->resourcesInFlight[i].ptrNonConst + j);

		ListRefPtr_freex(&device->resourcesInFlight[i]);
	}

	DeviceBufferRef_dec(&device->frameData);
	DeviceBufferRef_dec(&device->staging);

	Lock_free(&device->lock);
	Lock_free(&device->allocator.lock);
	ListWeakRefPtr_freex(&device->pendingResources);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		AllocationBuffer_freex(&device->stagingAllocations[i]);

	GraphicsDevice_freeExt(GraphicsInstanceRef_ptr(device->instance), (void*) GraphicsInstance_ext(device, ));

	ListDeviceMemoryBlock_freex(&device->allocator.blocks);
	ListLockPtr_freex(&device->currentLocks);

	GraphicsInstanceRef_dec(&device->instance);

	return true;
}

Error GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef, 
	const GraphicsDeviceInfo *info, 
	Bool verbose, 
	GraphicsDeviceRef **deviceRef
) {

	if(!instanceRef || !info || !deviceRef)
		return Error_nullPointer(
			!instanceRef ? 0 : (!info ? 1 : 2), "GraphicsDeviceRef_create()::instanceRef, info and deviceRef are required"
		);

	if(*deviceRef)
		return Error_invalidParameter(1, 0, "GraphicsDeviceRef_create()::*deviceRef wasn't NULL, probably indicates memleak");

	//Create RefPtr

	Error err = Error_none();
	_gotoIfError(clean, RefPtr_createx(
		(U32)(sizeof(GraphicsDevice) + GraphicsDeviceExt_size),
		(ObjectFreeFunc) GraphicsDevice_free, 
		EGraphicsTypeId_GraphicsDevice, 
		deviceRef
	));
	
	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	
	_gotoIfError(clean, ListWeakRefPtr_reservex(&device->pendingResources, 128));

	device->info = *info;

	_gotoIfError(clean, GraphicsInstanceRef_inc(instanceRef));
	device->instance = instanceRef;

	//Create mem alloc, set info/instance/pending resources

	device->allocator = (DeviceMemoryAllocator) { .device = device };
	_gotoIfError(clean, ListDeviceMemoryBlock_reservex(&device->allocator.blocks, 16));

	device->allocator.lock = Lock_create();
	device->lock = Lock_create();

	//Create in flight resource refs

	for(U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i)
		_gotoIfError(clean, ListRefPtr_reservex(&device->resourcesInFlight[i], 64));

	//Create extended device

	_gotoIfError(clean, GraphicsDevice_initExt(GraphicsInstanceRef_ptr(instanceRef), info, verbose, deviceRef));

	//Create constant buffer and staging buffer / allocators

	//Allocate staging buffer.
	//64 MiB / 3 = 21.333 MiB per frame.
	//If out of mem this will grow to be bigger.
	//But it's only used for "small" allocations (< 16 MiB)
	//If a lot of these larger allocations are found it will resize the staging buffer to try to encompass it too.

	U64 stagingSize = 64 * MIBI;
	_gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(*deviceRef, stagingSize));

	//Allocate UBO

	_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		*deviceRef, 
		EDeviceBufferUsage_CPUAllocatedBit | EDeviceBufferUsage_InternalWeakRef, 
		CharString_createConstRefCStr("Per frame data"),
		sizeof(CBufferData) * 3, &device->frameData
	));

	GraphicsDevice_postInit(device);

clean:

	if (err.genericError)
		GraphicsDeviceRef_dec(deviceRef);

	return err;
}

Bool GraphicsDeviceRef_removePending(GraphicsDeviceRef *deviceRef, RefPtr *resource) {

	if(!deviceRef)
		return false;

	Bool supported = false;

	EGraphicsTypeId type = (EGraphicsTypeId) resource->typeId;

	switch (type) {

		case EGraphicsTypeId_DeviceBuffer:	
			supported = DeviceBufferRef_ptr(resource)->device == deviceRef;
			break;

		default:
			return false;
	}

	if(!supported)
		return false;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	ELockAcquire acq = Lock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	U64 found = ListWeakRefPtr_findFirst(device->pendingResources, resource, 0);
	Bool success = false;

	if (found == U64_MAX) {
		success = true;
		goto clean;
	}

	Error err = Error_none();
	_gotoIfError(clean, ListWeakRefPtr_erase(&device->pendingResources, found));

	success = true;

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	return success;
}

impl Error GraphicsDeviceRef_waitExt(GraphicsDeviceRef *deviceRef);

impl Error GraphicsDevice_submitCommandsImpl(
	GraphicsDeviceRef *deviceRef, ListCommandListRef commandLists, ListSwapchainRef swapchains
);

impl Error DeviceBufferRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending);

Error GraphicsDeviceRef_handleNextFrame(GraphicsDeviceRef *deviceRef, void *commandBuffer) {
	
	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_handleNextFrame()::deviceRef is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!Lock_isLockedForThread(&device->lock))
		return Error_invalidState(0, "GraphicsDeviceRef_handleNextFrame() requires device to be locked by caller");

	//Release resources that were in flight.
	//This might cause resource deletions because we might be the last one releasing them.
	//For example temporary staging resources are released this way.

	ListRefPtr *inFlight = &device->resourcesInFlight[device->submitId % 3];

	for (U64 i = 0; i < inFlight->length; ++i)
		RefPtr_dec(inFlight->ptrNonConst + i);

	Error err = Error_none();
	_gotoIfError(clean, ListRefPtr_clear(inFlight));

	//Release all allocations of buffer that was in flight

	if(!AllocationBuffer_freeAll(&device->stagingAllocations[device->submitId % 3]))
		_gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_handleNextFrame() AllocationBuffer_freeAll failed"));

	//Update buffer data

	for(U64 i = 0; i < device->pendingResources.length; ++i) {

		RefPtr *pending = device->pendingResources.ptr[i];

		EGraphicsTypeId type = (EGraphicsTypeId) pending->typeId;

		switch(type) {

			case EGraphicsTypeId_DeviceBuffer: 
				_gotoIfError(clean, DeviceBufferRef_flush(commandBuffer, deviceRef, pending));
				break;

			default:
				_gotoIfError(clean, Error_unsupportedOperation(
					5, "GraphicsDeviceRef_handleNextFrame() unsupported pending graphics object"
				));
		}
	}

	_gotoIfError(clean, ListWeakRefPtr_clear(&device->pendingResources));

clean:
	return err;
}

Error GraphicsDeviceRef_resizeStagingBuffer(GraphicsDeviceRef *deviceRef, U64 newSize) {

	Error err = Error_none();
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (device->staging) {

		//"Free" staging buffer.
		//If the staging buffer was already in flight this won't do anything until it's out of flight.

		for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
			AllocationBuffer_freex(&device->stagingAllocations[i]);

		DeviceBufferRef_dec(&device->staging);
	}

	_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef, 
		EDeviceBufferUsage_CPUAllocatedBit | EDeviceBufferUsage_InternalWeakRef, 
		CharString_createConstRefCStr("Staging buffer"),
		newSize, &device->staging
	));

	DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
	Buffer stagingBuffer = Buffer_createRef(staging->mappedMemory, newSize);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		_gotoIfError(clean, AllocationBuffer_createRefFromRegionx(
			stagingBuffer, newSize / 3 * i, newSize / 3, &device->stagingAllocations[i]
		));

clean:
	return err;
}

Error GraphicsDeviceRef_submitCommands(
	GraphicsDeviceRef *deviceRef, 
	ListCommandListRef commandLists, 
	ListSwapchainRef swapchains, 
	Buffer appData
) {

	//Validation

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_submitCommands()::deviceRef is required");

	if(!swapchains.length && !commandLists.length)
		return Error_invalidOperation(0, "GraphicsDeviceRef_submitCommands()::swapchains or commandLists is required");

	if(swapchains.length > 16)						//Hard limit of 16 swapchains
		return Error_invalidParameter(2, 1, "GraphicsDeviceRef_submitCommands()::swapchains.length is limited to 16");

	if(Buffer_length(appData) > sizeof(((CBufferData*)NULL)->appData))
		return Error_invalidParameter(3, 0, "GraphicsDeviceRef_submitCommands()::appData is limited to 368 bytes");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Error err = Error_none();

	Lock *lockPtr = &device->lock;
	ELockAcquire acq = Lock_lock(lockPtr, U64_MAX);

	if(acq < ELockAcquire_Success)
		return Error_invalidState(0, "GraphicsDeviceRef_submitCommands() couldn't acquire device lock");

	if(acq == ELockAcquire_Acquired)
		_gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr));

	lockPtr = NULL;

	//Validate command lists

	for(U64 i = 0; i < commandLists.length; ++i) {

		CommandListRef *cmdRef = commandLists.ptr[i];

		if(!cmdRef || cmdRef->typeId != (ETypeId) EGraphicsTypeId_CommandList)
			_gotoIfError(clean, Error_nullPointer(1, "GraphicsDeviceRef_submitCommands()::commandLists[i] is required"));

		CommandList *cmd = CommandListRef_ptr(cmdRef);

		if(cmd->device != deviceRef)
			_gotoIfError(clean, Error_unsupportedOperation(
				0, "GraphicsDeviceRef_submitCommands()::commandLists[i]'s device and the current device are different"
			));

		lockPtr = &cmd->lock;
		acq = Lock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			_gotoIfError(clean, Error_invalidState(
				1, "GraphicsDeviceRef_submitCommands()::commandLists[i] couldn't be acquired"
			));
		}

		if(acq == ELockAcquire_Acquired)
			_gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr));

		lockPtr = NULL;

		if(cmd->state != ECommandListState_Closed)
			_gotoIfError(clean, Error_invalidParameter(
				1, (U32)i, "GraphicsDeviceRef_submitCommands()::commandLists[i] wasn't closed properly"
			));
	}

	//Swapchains all need to have the same vsync option.

	for (U64 i = 0; i < swapchains.length; ++i) {

		SwapchainRef *swapchainRef = swapchains.ptr[i];

		for(U64 j = 0; j < i; ++j)
			if(swapchainRef == swapchains.ptr[j])
				_gotoIfError(clean, Error_invalidParameter(
					2, 2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is duplicated"
				));

		if(!swapchainRef || swapchainRef->typeId != (ETypeId) EGraphicsTypeId_Swapchain)
			_gotoIfError(clean, Error_nullPointer(2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is required"));

		Swapchain *swapchaini = SwapchainRef_ptr(swapchainRef);

		if(swapchaini->device != deviceRef)
			_gotoIfError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_submitCommands()::swapchains[i]'s device and the current device are different"
			));

		lockPtr = &swapchaini->lock;
		acq = Lock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {

			lockPtr = NULL;

			_gotoIfError(clean, Error_invalidState(
				2, "GraphicsDeviceRef_submitCommands()::swapchains[i] couldn't acquire lock"
			));
		}

		if(acq == ELockAcquire_Acquired)
			_gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr));

		lockPtr = NULL;

		//Validate if the swapchain with a different version is bound, if yes, we have a stale cmdlist

		for (U64 j = 0; j < commandLists.length; ++j) {

			CommandListRef *cmdRef = commandLists.ptr[i];
			CommandList *cmd = CommandListRef_ptr(cmdRef);

			for(U64 k = 0; k < cmd->activeSwapchains.length; ++k) {
			
				DeviceResourceVersion vK = cmd->activeSwapchains.ptr[k];

				if(vK.resource == swapchainRef && vK.version != swapchaini->versionId)
					_gotoIfError(clean, Error_invalidState(
						0, "GraphicsDeviceRef_submitCommands()::swapchains[i] has outdated commands in submitted command list"
					));
			}

		}
	}

	//Lock all resources linked to command lists

	for(U64 i = 0; i < device->pendingResources.length; ++i) {

		WeakRefPtr *res = device->pendingResources.ptr[i];

		EGraphicsTypeId id = (EGraphicsTypeId) res->typeId;

		switch (id) {

			case EGraphicsTypeId_DeviceBuffer:
				lockPtr = &DeviceBufferRef_ptr(res)->lock;
				break;

			default:
				_gotoIfError(clean, Error_unimplemented(
					0, "GraphicsDeviceRef_submitCommands() pendingResources contains unsupported type"	//TODO: DeviceTexture
				));
		}

		if(!lockPtr)
			continue;

		acq = Lock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			_gotoIfError(clean, Error_invalidState(2, "GraphicsDeviceRef_submitCommands() couldn't acquire resource"));
		}

		if(acq == ELockAcquire_Acquired)
			_gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr));

		lockPtr = NULL;
	}

	//Set app data

	DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData);
	CBufferData *data = (CBufferData*) frameData->mappedMemory + (device->submitId % 3);
	Ns now = Time_now();

	*data = (CBufferData) {
		.frameId = (U32) device->submitId,
		.time = device->firstSubmit ? (F32)((F64)(now - device->firstSubmit) / SECOND) : 0,
		.deltaTime = device->firstSubmit ? (F32)((F64)(now - device->lastSubmit) / SECOND) : 0,
		.swapchainCount = (U32) swapchains.length
	};

	Buffer_copy(Buffer_createRef((U8*)data->appData, sizeof(*data)), appData);

	//Submit impl should also set the swapchains and process all command lists and swapchains.
	//This is not present here because the API impl is the one in charge of how it is threaded.

	_gotoIfError(clean, GraphicsDevice_submitCommandsImpl(deviceRef, commandLists, swapchains));

	//Add resources from command lists to resources in flight

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];

	for (U64 j = 0; j < commandLists.length; ++j) {

		CommandListRef *cmdRef = commandLists.ptr[j];
		CommandList *cmd = CommandListRef_ptr(cmdRef);

		for(U64 i = 0; i < cmd->resources.length; ++i) {

			RefPtr *ptr = cmd->resources.ptr[i];

			if(ListRefPtr_contains(*currentFlight, ptr, 0))
				continue;

			_gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, ptr));
			RefPtr_inc(ptr);
		}
	}

	//Ensure our next fence value is used

	++device->submitId;
	device->lastSubmit = Time_now();

	if(!device->firstSubmit)
		device->firstSubmit = device->lastSubmit;

	device->pendingBytes = 0;

clean:

	if(lockPtr)
		Lock_unlock(lockPtr);

	for(U64 i = 0; i < device->currentLocks.length; ++i)
		Lock_unlock(device->currentLocks.ptrNonConst[i]);

	ListLockPtr_clear(&device->currentLocks);
	return err;
}

Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef || deviceRef->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_wait()::deviceRef is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Error err = Error_none();
	
	ELockAcquire acq = Lock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return Error_invalidOperation(0, "GraphicsDeviceRef_wait() device's lock couldn't be acquired");

	_gotoIfError(clean, GraphicsDeviceRef_waitExt(deviceRef));

	for (U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i) {

		//Release resources that were in flight.
		//This might cause resource deletions because we might be the last one releasing them.
		//For example temporary staging resources are released this way.

		ListRefPtr *inFlight = &device->resourcesInFlight[i];

		for (U64 j = 0; j < inFlight->length; ++j)
			RefPtr_dec(inFlight->ptrNonConst + j);

		_gotoIfError(clean, ListRefPtr_clear(inFlight));

		//Release all allocations of buffer that was in flight

		if(!AllocationBuffer_freeAll(&device->stagingAllocations[i]))
			_gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_wait() couldn't AllocationBuffer_freeAll"));
	}

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	return err;
}
