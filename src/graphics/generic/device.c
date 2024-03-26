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
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/time.h"

TListNamedImpl(ListLockPtr);
TListNamedImpl(ListCommandListRef);
TListNamedImpl(ListSwapchainRef);
TListImpl(DescriptorStackTrace);

Error GraphicsDeviceRef_dec(GraphicsDeviceRef **device) {
	return !RefPtr_dec(device) ?
		Error_invalidOperation(0, "GraphicsDeviceRef_dec()::device is invalid") : Error_none();
}

Error GraphicsDeviceRef_inc(GraphicsDeviceRef *device) {
	return !RefPtr_inc(device) ?
		Error_invalidOperation(0, "GraphicsDeviceRef_inc()::device is invalid") : Error_none();
}

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

	(void)alloc;

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
	ListWeakRefPtr_freex(&device->pendingBlases);
	ListWeakRefPtr_freex(&device->pendingTlases);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		AllocationBuffer_freex(&device->stagingAllocations[i]);

	GraphicsDevice_freeExt(GraphicsInstanceRef_ptr(device->instance), (void*) GraphicsInstance_ext(device, ));

	U64 leakedBlocks = 0;

	for (U64 i = 0; i < device->allocator.blocks.length; ++i) {
		const DeviceMemoryBlock block = device->allocator.blocks.ptr[i];
		leakedBlocks += (Bool)Buffer_length(block.allocations.buffer);
	}

	if(leakedBlocks)
		Log_warnLnx("Leaked graphics device memory (showing up to 16/"PRIu64" entries):", leakedBlocks);

	for (U64 i = 0; i < leakedBlocks && i < 16; ++i) {

		const DeviceMemoryBlock block = device->allocator.blocks.ptr[i];
		const U64 leaked = Buffer_length(block.allocations.buffer);

		if(!leaked)
			continue;

		Log_warnLnx("%"PRIu64": %"PRIu64" bytes", i, leaked);

		#ifndef NDEBUG
			Log_printCapturedStackTraceCustomx(
				(const void**) block.stackTrace, sizeof(block.stackTrace) / sizeof(void*),
				ELogLevel_Warn, ELogOptions_NewLine
			);
		#endif
	}

	ListDeviceMemoryBlock_freex(&device->allocator.blocks);
	ListLockPtr_freex(&device->currentLocks);

	//Announce descriptor memleaks

	for(U32 i = 0; i < EDescriptorType_ResourceCount; ++i)
		Buffer_freex(&device->freeList[i]);

	Lock_free(&device->descriptorLock);

	#ifndef NDEBUG

		//TODO: HashMap

		ListDescriptorStackTrace *stack = &device->descriptorStackTraces;

		if(stack->length)
			Log_warnLnx("Leaked %"PRIu64" descriptors. Displaying up to 16:", stack->length);

		for (U64 j = 0; j < stack->length; ++j) {

			const DescriptorStackTrace elem = stack->ptr[j];

			if(j < 16)
				Log_warnLnx(
					"%"PRIu64": Resource type: %"PRIu64", id: %"PRIu64,
					j, (U64)elem.resourceId >> 20, (U64)elem.resourceId & ((1 << 20) - 1)
				);

			Log_printCapturedStackTraceCustomx(
				(const void**) elem.stackTrace, sizeof(elem.stackTrace) / sizeof(void*), ELogLevel_Warn, ELogOptions_Default
			);
		}

		ListDescriptorStackTrace_freex(stack);

	#endif

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
			!instanceRef ? 0 : (!info ? 1 : 2),
			"GraphicsDeviceRef_create()::instanceRef, info and deviceRef are required"
		);

	if(instanceRef->typeId != EGraphicsTypeId_GraphicsInstance)
		return Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_create()::instanceRef was an invalid type"
		);

	if(*deviceRef)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_create()::*deviceRef wasn't NULL, probably indicates memleak"
		);

	//Create RefPtr

	Error err = Error_none();
	gotoIfError(clean, RefPtr_createx(
		(U32)(sizeof(GraphicsDevice) + GraphicsDeviceExt_size),
		(ObjectFreeFunc) GraphicsDevice_free,
		(ETypeId) EGraphicsTypeId_GraphicsDevice,
		deviceRef
	))

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);

	gotoIfError(clean, ListWeakRefPtr_reservex(&device->pendingResources, 128))

	device->info = *info;

	gotoIfError(clean, GraphicsInstanceRef_inc(instanceRef))
	device->instance = instanceRef;
	//Create mem alloc, set info/instance/pending resources

	device->allocator = (DeviceMemoryAllocator) { .device = device };
	gotoIfError(clean, ListDeviceMemoryBlock_reservex(&device->allocator.blocks, 16))

	device->allocator.lock = Lock_create();
	device->lock = Lock_create();

	//Create in flight resource refs

	for(U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i)
		gotoIfError(clean, ListRefPtr_reservex(&device->resourcesInFlight[i], 64))

	//Create descriptor type free list

	device->descriptorLock = Lock_create();

	for(U64 i = 0; i < EDescriptorType_ResourceCount; ++i)
		gotoIfError(clean, Buffer_createEmptyBytesx((descriptorTypeCount[i] + 7) >> 3, &device->freeList[i]))

	//Reserve sampler 0 because we want to be able to use read/write handle 0 as invalid.

	Lock_lock(&device->descriptorLock, U64_MAX);
	if(GraphicsDeviceRef_allocateDescriptor(*deviceRef, EDescriptorType_Texture2D) != 0)
		gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_create() couldn't reserve null descriptor (sampler 0)"))

	Lock_unlock(&device->descriptorLock);

	//Allocate temp storage for descriptor tracking

	#ifndef NDEBUG
		gotoIfError(clean, ListDescriptorStackTrace_reservex(&device->descriptorStackTraces, 16))
	#endif

	//Create extended device

	gotoIfError(clean, GraphicsDevice_initExt(GraphicsInstanceRef_ptr(instanceRef), info, verbose, deviceRef))

	//Create constant buffer and staging buffer / allocators

	//Allocate staging buffer.
	//64 MiB / 3 = 21.333 MiB per frame.
	//If out of mem this will grow to be bigger.
	//But it's only used for "small" allocations (< 16 MiB)
	//If a lot of these larger allocations are found it will resize the staging buffer to try to encompass it too.

	U64 stagingSize = 64 * MIBI;
	gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(*deviceRef, stagingSize))

	//Allocate UBO

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		*deviceRef,
		EDeviceBufferUsage_None,
		EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
		CharString_createRefCStrConst("Per frame data"),
		sizeof(CBufferData) * 3, &device->frameData
	))

	GraphicsDevice_postInit(device);

clean:

	if (err.genericError)
		GraphicsDeviceRef_dec(deviceRef);

	return err;
}

Bool GraphicsDeviceRef_removePending(GraphicsDeviceRef *deviceRef, RefPtr *resource) {

	if(!deviceRef || deviceRef->typeId != EGraphicsTypeId_GraphicsDevice)
		return false;

	Bool supported = false;

	const EGraphicsTypeId type = (EGraphicsTypeId) resource->typeId;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ListWeakRefPtr *pendingList = NULL;

	switch (type) {

		case EGraphicsTypeId_DeviceBuffer:
			supported = DeviceBufferRef_ptr(resource)->resource.device == deviceRef;
			pendingList = &device->pendingResources;
			break;

		case EGraphicsTypeId_DeviceTexture:
			supported = DeviceTextureRef_ptr(resource)->base.resource.device == deviceRef;
			pendingList = &device->pendingResources;
			break;

		case EGraphicsTypeId_BLASExt:
			supported = BLASRef_ptr(resource)->base.device == deviceRef;
			pendingList = &device->pendingBlases;
			break;

		case EGraphicsTypeId_TLASExt:
			supported = TLASRef_ptr(resource)->base.device == deviceRef;
			pendingList = &device->pendingTlases;
			break;

		default:
			return false;
	}

	if(!supported || !pendingList)
		return false;

	const ELockAcquire acq = Lock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	const U64 found = ListWeakRefPtr_findFirst(*pendingList, resource, 0, NULL);
	Bool success = false;

	if (found == U64_MAX) {
		success = true;
		goto clean;
	}

	Error err = Error_none();
	gotoIfError(clean, ListWeakRefPtr_erase(pendingList, found))

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
impl Error DeviceTextureRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending);
impl Error BLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending);
impl Error TLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending);

Error GraphicsDeviceRef_handleNextFrame(GraphicsDeviceRef *deviceRef, void *commandBuffer) {

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_handleNextFrame()::deviceRef is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!Lock_isLockedForThread(&device->lock))
		return Error_invalidState(
			0, "GraphicsDeviceRef_handleNextFrame() requires device to be locked by caller"
		);

	//Release resources that were in flight.
	//This might cause resource deletions because we might be the last one releasing them.
	//For example temporary staging resources are released this way.

	ListRefPtr *inFlight = &device->resourcesInFlight[device->submitId % 3];

	for (U64 i = 0; i < inFlight->length; ++i)
		RefPtr_dec(inFlight->ptrNonConst + i);

	Error err = Error_none();
	gotoIfError(clean, ListRefPtr_clear(inFlight))

	//Release all allocations of buffer that was in flight

	if(!AllocationBuffer_freeAll(&device->stagingAllocations[device->submitId % 3]))
		gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_handleNextFrame() AllocationBuffer_freeAll failed"))

	//Update buffer data

	for(U64 i = 0; i < device->pendingResources.length; ++i) {

		RefPtr *pending = device->pendingResources.ptr[i];

		EGraphicsTypeId type = (EGraphicsTypeId) pending->typeId;

		switch(type) {

			case EGraphicsTypeId_DeviceBuffer:
				gotoIfError(clean, DeviceBufferRef_flush(commandBuffer, deviceRef, pending))
				break;

			case EGraphicsTypeId_DeviceTexture:
				gotoIfError(clean, DeviceTextureRef_flush(commandBuffer, deviceRef, pending))
				break;

			default:
				gotoIfError(clean, Error_unsupportedOperation(
					5, "GraphicsDeviceRef_handleNextFrame() unsupported pending graphics object"
				))
		}
	}

	gotoIfError(clean, ListWeakRefPtr_clear(&device->pendingResources))

	//Update BLASes

	for(U64 i = 0; i < device->pendingBlases.length; ++i) {

		RefPtr *pending = device->pendingBlases.ptr[i];
		EGraphicsTypeId type = (EGraphicsTypeId) pending->typeId;

		if(type != EGraphicsTypeId_BLASExt)
			gotoIfError(clean, Error_unsupportedOperation(
				6, "GraphicsDeviceRef_handleNextFrame() unsupported pending BLAS type"
			))

		gotoIfError(clean, BLASRef_flush(commandBuffer, deviceRef, pending))
	}

	gotoIfError(clean, ListWeakRefPtr_clear(&device->pendingBlases))

	//Update TLASes

	for(U64 i = 0; i < device->pendingTlases.length; ++i) {

		RefPtr *pending = device->pendingTlases.ptr[i];
		EGraphicsTypeId type = (EGraphicsTypeId) pending->typeId;

		if(type != EGraphicsTypeId_TLASExt)
			gotoIfError(clean, Error_unsupportedOperation(
				6, "GraphicsDeviceRef_handleNextFrame() unsupported pending TLAS type"
			))

		gotoIfError(clean, TLASRef_flush(commandBuffer, deviceRef, pending))
	}

	gotoIfError(clean, ListWeakRefPtr_clear(&device->pendingTlases))

clean:
	return err;
}

Error GraphicsDeviceRef_resizeStagingBuffer(GraphicsDeviceRef *deviceRef, U64 newSize) {

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_resizeStagingBuffer()::deviceRef is required");

	Error err;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	newSize = (((newSize + 2) / 3 + 511) &~ 511) * 3;			//Align to ensure we never get incompatible staging buffers

	if (device->staging) {

		//"Free" staging buffer.
		//If the staging buffer was already in flight this won't do anything until it's out of flight.

		for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
			AllocationBuffer_freex(&device->stagingAllocations[i]);

		DeviceBufferRef_dec(&device->staging);
	}

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_None,
		EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
		CharString_createRefCStrConst("Staging buffer"),
		newSize, &device->staging
	))

	const DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
	const Buffer stagingBuffer = Buffer_createRef(staging->resource.mappedMemoryExt, newSize);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		gotoIfError(clean, AllocationBuffer_createRefFromRegionx(
			stagingBuffer, newSize / 3 * i, newSize / 3, &device->stagingAllocations[i]
		))

clean:
	return err;
}

Error GraphicsDeviceRef_submitCommands(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains,
	Buffer appData,
	F32 deltaTime,
	F32 time
) {

	//Validation

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_submitCommands()::deviceRef is required");

	if(!swapchains.length && !commandLists.length)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_submitCommands()::swapchains or commandLists is required"
		);

	if(swapchains.length > 16)						//Hard limit of 16 swapchains
		return Error_invalidParameter(
			2, 1, "GraphicsDeviceRef_submitCommands()::swapchains.length is limited to 16"
		);

	if(Buffer_length(appData) > sizeof(((CBufferData*)NULL)->appData))
		return Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_submitCommands()::appData is limited to 368 bytes"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Error err = Error_none();

	Lock *lockPtr = &device->lock;
	ELockAcquire acq = Lock_lock(lockPtr, U64_MAX);

	if(acq < ELockAcquire_Success)
		return Error_invalidState(0, "GraphicsDeviceRef_submitCommands() couldn't acquire device lock");

	if(acq == ELockAcquire_Acquired)
		gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr))

	lockPtr = NULL;

	//Validate command lists

	for(U64 i = 0; i < commandLists.length; ++i) {

		CommandListRef *cmdRef = commandLists.ptr[i];

		if(!cmdRef || cmdRef->typeId != (ETypeId) EGraphicsTypeId_CommandList)
			gotoIfError(clean, Error_nullPointer(1, "GraphicsDeviceRef_submitCommands()::commandLists[i] is required"))

		CommandList *cmd = CommandListRef_ptr(cmdRef);

		if(cmd->device != deviceRef)
			gotoIfError(clean, Error_unsupportedOperation(
				0, "GraphicsDeviceRef_submitCommands()::commandLists[i]'s device and the current device are different"
			))

		lockPtr = &cmd->lock;
		acq = Lock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			gotoIfError(clean, Error_invalidState(
				1, "GraphicsDeviceRef_submitCommands()::commandLists[i] couldn't be acquired"
			))
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr))

		lockPtr = NULL;

		if(cmd->state != ECommandListState_Closed)
			gotoIfError(clean, Error_invalidParameter(
				1, (U32)i, "GraphicsDeviceRef_submitCommands()::commandLists[i] wasn't closed properly"
			))
	}

	for (U64 i = 0; i < swapchains.length; ++i) {

		SwapchainRef *swapchainRef = swapchains.ptr[i];

		for(U64 j = 0; j < i; ++j)
			if(swapchainRef == swapchains.ptr[j])
				gotoIfError(clean, Error_invalidParameter(
					2, 2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is duplicated"
				))

		if(!swapchainRef || swapchainRef->typeId != (ETypeId) EGraphicsTypeId_Swapchain)
			gotoIfError(clean, Error_nullPointer(2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is required"))

		Swapchain *swapchaini = SwapchainRef_ptr(swapchainRef);

		if(swapchaini->base.resource.device != deviceRef)
			gotoIfError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_submitCommands()::swapchains[i]'s device and the current device are different"
			))

		lockPtr = &swapchaini->lock;
		acq = Lock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {

			lockPtr = NULL;

			gotoIfError(clean, Error_invalidState(
				2, "GraphicsDeviceRef_submitCommands()::swapchains[i] couldn't acquire lock"
			))
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr))

		lockPtr = NULL;

		//Validate if the swapchain with a different version is bound, if yes, we have a stale cmdlist

		for (U64 j = 0; j < commandLists.length; ++j) {

			CommandListRef *cmdRef = commandLists.ptr[i];
			CommandList *cmd = CommandListRef_ptr(cmdRef);

			for(U64 k = 0; k < cmd->activeSwapchains.length; ++k) {

				DeviceResourceVersion vK = cmd->activeSwapchains.ptr[k];

				if(vK.resource == swapchainRef && vK.version != swapchaini->versionId)
					gotoIfError(clean, Error_invalidState(
						0, "GraphicsDeviceRef_submitCommands()::swapchains[i] has outdated commands in submitted command list"
					))
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

			case EGraphicsTypeId_DeviceTexture:
				lockPtr = &DeviceTextureRef_ptr(res)->lock;
				break;

			default:
				gotoIfError(clean, Error_unimplemented(
					0, "GraphicsDeviceRef_submitCommands() pendingResources contains unsupported type"
				))
		}

		if(!lockPtr)
			continue;

		acq = Lock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			gotoIfError(clean, Error_invalidState(2, "GraphicsDeviceRef_submitCommands() couldn't acquire resource"))
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError(clean, ListLockPtr_pushBackx(&device->currentLocks, lockPtr))

		lockPtr = NULL;
	}

	//Set app data

	DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData);
	CBufferData *data = (CBufferData*) frameData->resource.mappedMemoryExt + (device->submitId % 3);
	Ns now = Time_now();

	*data = (CBufferData) {
		.frameId = (U32) device->submitId,
		.time = device->firstSubmit ? (F32)((F64)(now - device->firstSubmit) / SECOND) : 0,
		.deltaTime = device->firstSubmit ? (F32)((F64)(now - device->lastSubmit) / SECOND) : 0,
		.swapchainCount = (U32) swapchains.length
	};

	if (deltaTime >= 0) {
		data->deltaTime = deltaTime;
		data->time = time;
	}

	Buffer_copy(Buffer_createRef((U8*)data->appData, sizeof(*data)), appData);

	//Submit impl should also set the swapchains and process all command lists and swapchains.
	//This is not present here because the API impl is the one in charge of how it is threaded.

	gotoIfError(clean, GraphicsDevice_submitCommandsImpl(deviceRef, commandLists, swapchains))

	//Add resources from command lists to resources in flight

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];

	for (U64 j = 0; j < commandLists.length; ++j) {

		CommandListRef *cmdRef = commandLists.ptr[j];
		CommandList *cmd = CommandListRef_ptr(cmdRef);

		for(U64 i = 0; i < cmd->resources.length; ++i) {

			RefPtr *ptr = cmd->resources.ptr[i];

			if(ListRefPtr_contains(*currentFlight, ptr, 0, NULL))
				continue;

			gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, ptr))
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

	Error err;
	const ELockAcquire acq = Lock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return Error_invalidOperation(0, "GraphicsDeviceRef_wait() device's lock couldn't be acquired");

	gotoIfError(clean, GraphicsDeviceRef_waitExt(deviceRef))

	for (U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i) {

		//Release resources that were in flight.
		//This might cause resource deletions because we might be the last one releasing them.
		//For example temporary staging resources are released this way.

		ListRefPtr *inFlight = &device->resourcesInFlight[i];

		for (U64 j = 0; j < inFlight->length; ++j)
			RefPtr_dec(inFlight->ptrNonConst + j);

		gotoIfError(clean, ListRefPtr_clear(inFlight))

		//Release all allocations of buffer that was in flight

		if(!AllocationBuffer_freeAll(&device->stagingAllocations[i]))
			gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_wait() couldn't AllocationBuffer_freeAll"))
	}

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	return err;
}

U32 GraphicsDeviceRef_allocateDescriptor(GraphicsDeviceRef *deviceRef, EDescriptorType type) {

	if(!deviceRef || deviceRef->typeId != EGraphicsTypeId_GraphicsDevice || type >= EDescriptorType_ResourceCount)
		return U32_MAX;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!Lock_isLockedForThread(&device->descriptorLock))
		return U32_MAX;

	const Buffer buf = device->freeList[type];

	for(U32 i = 0; i < (U32)(Buffer_length(buf) << 3); ++i) {

		Bool bit = false;

		if(Buffer_getBit(buf, i, &bit).genericError)
			return U32_MAX;

		if(!bit) {

			U32 extendedType = 0, descType = type;

			if (type >= EDescriptorType_ExtendedType) {
				extendedType = (U32)(type - EDescriptorType_ExtendedType);
				descType = EDescriptorType_ExtendedType;
			}

			const U32 resourceId = i | (extendedType << 13) | (descType << 17);

			#ifndef NDEBUG

				DescriptorStackTrace stackTrace = (DescriptorStackTrace) {  .resourceId = resourceId };
				Log_captureStackTracex(stackTrace.stackTrace, sizeof(stackTrace.stackTrace) / sizeof(void*), 1);

				//Disable tracking for resourceId 0 because that's reserved as a safeguard
				//It's technically "leaked" but for a good reason.

				if(resourceId && ListDescriptorStackTrace_pushBackx(&device->descriptorStackTraces, stackTrace).genericError)
					return U32_MAX;

			#endif

			Buffer_setBit(buf, i);
			return resourceId;
		}
	}

	return U32_MAX;
}

Bool GraphicsDeviceRef_freeDescriptors(GraphicsDeviceRef *deviceRef, ListU32 *allocations) {

	if(!deviceRef || deviceRef->typeId != EGraphicsTypeId_GraphicsDevice)
		return U32_MAX;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!Lock_isLockedForThread(&device->descriptorLock))
		return false;

	Bool success = true;

	for (U64 i = 0; i < allocations->length; ++i) {

		const U32 id = allocations->ptr[i];

		if(id && id != U32_MAX) {		//0 and -1 are both invalid to avoid freeing uninitialized memory (NULL descriptor)

			const EDescriptorType type = ResourceHandle_getType(id);

			if(type >= EDescriptorType_ResourceCount)
				continue;

			const U32 realId = ResourceHandle_getId(id);

			success &= !Buffer_resetBit(device->freeList[type], realId).genericError;

			#ifndef NDEBUG

				//TODO: HashMap

				ListDescriptorStackTrace *stack = &device->descriptorStackTraces;

				for (U64 j = 0; j < stack->length; ++j)
					if (stack->ptr[j].resourceId == id) {
						ListDescriptorStackTrace_erase(stack, j);
						break;
					}

			#endif
		}
	}

	ListU32_clear(allocations);
	return success;
}

U64 ResourceHandle_pack3(U32 a, U32 b, U32 c) {
	return a | ((U64)b << 21) |  ((U64)c << 42);
}

I32x4 ResourceHandle_unpack3(U64 v) {

	const U32 a = v & ((1 << 21) - 1);
	const U32 b = (v >> 21) & ((1 << 21) - 1);
	const U32 c = v >> 42;

	return I32x4_create3((I32)a, (I32)b, (I32)c);
}

EDescriptorType ResourceHandle_getType(U32 handle) {

	EDescriptorType type = (EDescriptorType)(handle >> 17);
	const U32 realId = handle & ((1 << 17) - 1);

	if (type == EDescriptorType_ExtendedType)
		type += (realId >> 13) & 0xF;

	return type;
}

U32 ResourceHandle_getId(U32 handle) {

	if ((EDescriptorType)(handle >> 17) == EDescriptorType_ExtendedType)
		return handle & ((1 << 13) - 1);

	return handle & ((1 << 17) - 1);
}

Bool ResourceHandle_isValid(U32 handle) {

	const EDescriptorType type = ResourceHandle_getType(handle);

	if(type >= EDescriptorType_ResourceCount)
		return false;

	return ResourceHandle_getId(handle) >= descriptorTypeCount[type];
}
