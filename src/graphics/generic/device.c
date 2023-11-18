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

#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_buffer.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/type_cast.h"

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
	return !RefPtr_dec(device) ? Error_invalidOperation(0) : Error_none();
}

Error GraphicsDeviceRef_inc(GraphicsDeviceRef *device) {
	return !RefPtr_inc(device) ? Error_invalidOperation(0) : Error_none();
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

	Lock_free(&device->lock);
	Lock_free(&device->allocator.lock);
	List_freex(&device->pendingResources);

	for(U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i) {

		for(U64 j = 0; j < device->resourcesInFlight[i].length; ++j)
			RefPtr_dec((RefPtr**)device->resourcesInFlight[i].ptr + j);

		List_freex(&device->resourcesInFlight[i]);
	}

	DeviceBufferRef_dec(&device->frameData);
	DeviceBufferRef_dec(&device->staging);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		AllocationBuffer_freex(&device->stagingAllocations[i]);

	GraphicsDevice_freeExt(GraphicsInstanceRef_ptr(device->instance), (void*) GraphicsInstance_ext(device, ));

	List_freex(&device->allocator.blocks);

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
		return Error_nullPointer(!instanceRef ? 0 : (!info ? 1 : 2));

	if(*deviceRef)
		return Error_invalidParameter(1, 0);

	//Create RefPtr

	Error err = Error_none();
	_gotoIfError(clean, RefPtr_createx(
		(U32)(sizeof(GraphicsDevice) + GraphicsDeviceExt_size),
		(ObjectFreeFunc) GraphicsDevice_free, 
		EGraphicsTypeId_GraphicsDevice, 
		deviceRef
	));
	
	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	
	device->pendingResources = List_createEmpty(sizeof(RefPtr*));;
	_gotoIfError(clean, List_reservex(&device->pendingResources, 128));

	device->info = *info;

	_gotoIfError(clean, GraphicsInstanceRef_inc(instanceRef));
	device->instance = instanceRef;

	//Create mem alloc, set info/instance/pending resources

	device->allocator = (DeviceMemoryAllocator) {
		.device = device,
		.blocks = List_createEmpty(sizeof(DeviceMemoryBlock))
	};

	_gotoIfError(clean, List_reservex(&device->allocator.blocks, 16));
	_gotoIfError(clean, Lock_create(&device->allocator.lock));
	_gotoIfError(clean, Lock_create(&device->lock));

	//Create in flight resource refs

	for(U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i)  {
		device->resourcesInFlight[i] = List_createEmpty(sizeof(RefPtr*));
		_gotoIfError(clean, List_reservex(&device->resourcesInFlight[i], 64));
	}

	//Create extended device

	_gotoIfError(clean, GraphicsDevice_initExt(GraphicsInstanceRef_ptr(instanceRef), info, verbose, deviceRef));

	//Create constant buffer and staging buffer / allocators

	//Allocate staging buffer.
	//64 MiB - 256 * 3 to potentially allow CBuffer at the end of the memory.
	//This gets divided into 3 for each frame id to have their own allocator into that subblock.
	//This allows way easier management.

	U64 stagingSize = DeviceMemoryBlock_defaultSize - sizeof(CBufferData) * 3;

	_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		*deviceRef, 
		EDeviceBufferUsage_CPUAllocatedBit | EDeviceBufferUsage_InternalWeakRef, 
		CharString_createConstRefCStr("Staging buffer"),
		stagingSize, &device->staging
	));

	DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
	Buffer stagingBuffer = Buffer_createRef(staging->mappedMemory, stagingSize);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		_gotoIfError(clean, AllocationBuffer_createRefFromRegionx(
			stagingBuffer, stagingSize / 3 * i, stagingSize / 3, &device->stagingAllocations[i]
		));

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
		case EGraphicsTypeId_DeviceBuffer:	supported = DeviceBufferRef_ptr(resource)->device == deviceRef;		break;
		default:
			return false;
	}

	if(!supported)
		return false;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	if(!Lock_lock(&device->lock, U64_MAX))
		return false;

	U64 found = List_findFirst(device->pendingResources, Buffer_createConstRef(&resource, sizeof(resource)), 0);
	Bool success = false;

	if (found == U64_MAX) {
		success = true;
		goto clean;
	}

	Error err = Error_none();
	_gotoIfError(clean, List_erase(&device->pendingResources, found));

	success = true;

clean:
	Lock_unlock(&device->lock);
	return success;
}

impl Error GraphicsDeviceRef_waitExt(GraphicsDeviceRef *deviceRef);

Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef || deviceRef->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Error err = Error_none();
	
	if(!Lock_lock(&device->lock, U64_MAX))
		return Error_invalidOperation(0);

	_gotoIfError(clean, GraphicsDeviceRef_waitExt(deviceRef));

	for (U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i) {

		//Release resources that were in flight.
		//This might cause resource deletions because we might be the last one releasing them.
		//For example temporary staging resources are released this way.

		List *inFlight = &device->resourcesInFlight[i];

		for (U64 j = 0; j < inFlight->length; ++j)
			RefPtr_dec((RefPtr**)inFlight->ptr + j);

		_gotoIfError(clean, List_clear(inFlight));

		//Release all allocations of buffer that was in flight

		if(!AllocationBuffer_freeAll(&device->stagingAllocations[i]))
			_gotoIfError(clean, Error_invalidState(0));
	}

clean:
	Lock_unlock(&device->lock);
	return err;
}
