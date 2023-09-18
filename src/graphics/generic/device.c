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
#include "platforms/ext/listx.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/type_cast.h"

void GraphicsDeviceInfo_print(const GraphicsDeviceInfo *deviceInfo, Bool printCapabilities) {

	if(!deviceInfo || !deviceInfo->ext)
		return;

	Log_debugLn(
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

		Log_debugLn("\tFeatures:");

		if(feat & EGraphicsFeatures_TiledRendering)
			Log_debugLn("\t\tTile based rendering");

		//if(feat & EGraphicsFeatures_VariableRateShading)
		//	Log_debugLn("\t\tVariable rate shading");

		if(feat & EGraphicsFeatures_MultiDrawIndirectCount)
			Log_debugLn("\t\tMulti draw indirect count");

		if(feat & EGraphicsFeatures_MeshShader)
			Log_debugLn("\t\tMesh shaders");

		if(feat & EGraphicsFeatures_GeometryShader)
			Log_debugLn("\t\tGeometry shaders");

		if(feat & EGraphicsFeatures_TessellationShader)
			Log_debugLn("\t\tTessellation shaders");

		if(feat & EGraphicsFeatures_SubgroupArithmetic)
			Log_debugLn("\t\tSubgroup arithmetic");

		if(feat & EGraphicsFeatures_SubgroupShuffle)
			Log_debugLn("\t\tSubgroup shuffle");

		if(feat & EGraphicsFeatures_Swapchain)
			Log_debugLn("\t\tSwapchain");

		//if(feat & EGraphicsFeatures_Multiview)
		//	Log_debugLn("\t\tMultiview");

		if(feat & EGraphicsFeatures_Raytracing)
			Log_debugLn("\t\tRaytracing");

		if(feat & EGraphicsFeatures_RayPipeline)
			Log_debugLn("\t\tRaytracing pipeline");

		if(feat & EGraphicsFeatures_RayQuery)
			Log_debugLn("\t\tRay query");

		if(feat & EGraphicsFeatures_RayIndirect)
			Log_debugLn("\t\tTraceRay indirect");

		if(feat & EGraphicsFeatures_RayMicromapOpacity)
			Log_debugLn("\t\tRaytracing opacity micromap");

		if(feat & EGraphicsFeatures_RayMicromapDisplacement)
			Log_debugLn("\t\tRaytracing displacement micromap");

		if(feat & EGraphicsFeatures_RayMotionBlur)
			Log_debugLn("\t\tRaytracing motion blur");

		if(feat & EGraphicsFeatures_RayReorder)
			Log_debugLn("\t\tRay reorder");

		//Data types

		U32 dat = cap.dataTypes;

		Log_debugLn("\tData types:");
		
		if(dat & EGraphicsDataTypes_I64)
			Log_debugLn("\t\t64-bit integers");
		
		if(dat & EGraphicsDataTypes_F16)
			Log_debugLn("\t\t16-bit floats");
		
		if(dat & EGraphicsDataTypes_F64)
			Log_debugLn("\t\t64-bit floats");
		
		if(dat & EGraphicsDataTypes_AtomicI64)
			Log_debugLn("\t\t64-bit integer atomics (buffer)");
		
		if(dat & EGraphicsDataTypes_AtomicF32)
			Log_debugLn("\t\t32-bit float atomics (buffer)");
		
		if(dat & EGraphicsDataTypes_AtomicF64)
			Log_debugLn("\t\t64-bit float atomics (buffer)");
		
		if(dat & EGraphicsDataTypes_ASTC)
			Log_debugLn("\t\tASTC compression");
		
		if(dat & EGraphicsDataTypes_BCn)
			Log_debugLn("\t\tBCn compression");
		
		if(dat & EGraphicsDataTypes_MSAA2x)
			Log_debugLn("\t\tMSAA 2x");
		
		if(dat & EGraphicsDataTypes_MSAA8x)
			Log_debugLn("\t\tMSAA 8x");

		if(dat & EGraphicsDataTypes_MSAA16x)
			Log_debugLn("\t\tMSAA 16x");
	}
}

Error GraphicsDeviceRef_dec(GraphicsDeviceRef **device) {
	return !RefPtr_dec(device) ? Error_invalidOperation(0) : Error_none();
}

Error GraphicsDeviceRef_add(GraphicsDeviceRef *device) {
	return device ? (!RefPtr_inc(device) ? Error_invalidOperation(0) : Error_none()) : Error_nullPointer(0);
}

//Ext

impl Error GraphicsDevice_initExt(
	const GraphicsInstance *instance, 
	const GraphicsDeviceInfo *deviceInfo, 
	Bool verbose,
	GraphicsDeviceRef **deviceRef
);

impl Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext);

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

	//Create extended device

	Error err = GraphicsDevice_initExt(GraphicsInstanceRef_ptr(instanceRef), info, verbose, deviceRef);

	if(err.genericError)
		return err;

	/*

	if((err = List_createx(EGraphicsTypeId_Count, sizeof(GraphicsObjectFactory), &device->factories)).genericError) {
		GraphicsDevice_freeExt(instance, &device->ext);
		return err;
	}

	//Create factories

	for(U64 i = 0; i < EGraphicsTypeId_Count; ++i) {

		if (
			(err = GraphicsObjectFactory_create(
				EGraphicsTypeId_all[i], 
				EGraphicsTypeId_infoBytes[i],
				EGraphicsTypeId_objectBytes[i],
				(GraphicsObjectFactory*)device->factories.ptr + i
			)).genericError
		) {

			for(U64 j = 0; j < i; ++j)
				GraphicsObjectFactory_free(device, (GraphicsObjectFactory*)device->factories.ptr + j);

			List_freex(&device->factories);
			GraphicsDevice_freeExt(instance, &device->ext);

			return err;
		}
	}*/

	//Graphics device success

	GraphicsInstanceRef_add(instanceRef);

	 GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	 device->info = *info;
	 device->instance = instanceRef;

	return Error_none();
}

Bool GraphicsDevice_free(GraphicsDevice *device, Allocator alloc) {

	alloc;

	if(!device)
		return true;

	//TODO:
	//for(U64 j = 0; j < device->factories.length; ++j)
	//	success &= GraphicsObjectFactory_free(device, (GraphicsObjectFactory*)device->factories.ptr + j);

	//success &= List_freex(&device->factories);

	GraphicsDevice_freeExt(GraphicsInstanceRef_ptr(device->instance), (void*) GraphicsInstance_ext(device, ));
	GraphicsInstanceRef_dec(&device->instance);

	return true;
}

//Batch create objects.
//All objects created need to be freed by the runtime.

Error GraphicsDevice_createObjects(GraphicsDevice *device, EGraphicsTypeId type, List infos, List *result);
Bool GraphicsDevice_freeObjects(GraphicsDevice *device, List *objects);

//Create single objects.
//Prefer to use the batched versions if possible for less locks.

Error GraphicsDevice_createObject(GraphicsDevice *device, EGraphicsTypeId type, const void *info, RefPtr **result);
Bool GraphicsDevice_freeObject(GraphicsDevice *device, RefPtr **object);

//Optional. Not all factories allow finding or creating objects.
//So query type type if it supports it first.

Bool GraphicsDevice_supportsFind(EGraphicsTypeId type);

Error GraphicsDevice_findObjects(const GraphicsDevice *device, EGraphicsTypeId type, List infos, List *result);
Error GraphicsDevice_findOrCreateObjects(GraphicsDevice *device, EGraphicsTypeId type, List infos, List *result);

Error GraphicsDevice_findObject(const GraphicsDevice *device, EGraphicsTypeId type, const void *info, RefPtr **result);
Error GraphicsDevice_findOrCreateObject(GraphicsDevice *device, EGraphicsTypeId type, const void *info, RefPtr **result);
