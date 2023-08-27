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
#include "types/error.h"

//Ext

impl Error GraphicsDevice_initExt(const GraphicsInstance *instance, const GraphicsDeviceInfo *device, void **ext);
impl Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void **ext);

//

EGraphicsTypeId EGraphicsTypeId_all[EGraphicsTypeId_Count] = {
	EGraphicsTypeId_Texture,
	EGraphicsTypeId_RenderTexture,
	EGraphicsTypeId_Buffer,
	EGraphicsTypeId_Pipeline,
	EGraphicsTypeId_DescriptorSet,
	EGraphicsTypeId_RenderPass,
	EGraphicsTypeId_Sampler,
	EGraphicsTypeId_CommandList,
	EGraphicsTypeId_AccelerationStructure,
	EGraphicsTypeId_Swapchain
};

U64 EGraphicsTypeId_descBytes[EGraphicsTypeId_Count] = {
	0,		//sizeof(TextureDesc)
	0,		//sizeof(RenderTextureDesc)
	0,		//sizeof(BufferDesc)
	0,		//sizeof(PipelineDesc)
	0,		//sizeof(DescriptorSetDesc)
	0,		//sizeof(RenderPassDesc)
	0,		//sizeof(SamplerDesc)
	0,		//sizeof(CommandListDesc)
	0,		//sizeof(AccelerationStructureDesc)
	0		//sizeof(SwapchainDesc)
};

U64 EGraphicsTypeId_objectBytes[EGraphicsTypeId_Count] = {
	0,		//sizeof(TextureObject)
	0,		//sizeof(RenderTextureObject)
	0,		//sizeof(BufferObject)
	0,		//sizeof(PipelineObject)
	0,		//sizeof(DescriptorSetObject)
	0,		//sizeof(RenderPassObject)
	0,		//sizeof(SamplerObject)
	0,		//sizeof(CommandListObject)
	0,		//sizeof(AccelerationStructureObject)
	0		//sizeof(SwapchainObject)
};

//TODO:
/*
Error GraphicsDevice_create(const GraphicsInstance *instance, const U64 uuid[2], GraphicsDevice *device) {

	if(!instance || !uuid || !device)
		return Error_nullPointer(!instance ? 0 : (!uuid ? 2 : 3));

	if(device->ext)
		return Error_invalidParameter(2, 0);

	Error err = GraphicsInstance_getDeviceInfo(instance, deviceId, &device->info);

	if(err.genericError)
		return err;

	//Create extended device

	if((err = GraphicsDevice_initExt(instance, &device->info, &device->ext)).genericError)
		return err;

	if((err = List_createx(EGraphicsTypeId_Count, sizeof(GraphicsObjectFactory), &device->factories)).genericError) {
		GraphicsDevice_freeExt(instance, &device->ext);
		return err;
	}

	//Create factories

	for(U64 i = 0; i < EGraphicsTypeId_Count; ++i) {

		if (
			(err = GraphicsObjectFactory_create(
				EGraphicsTypeId_all[i], 
				EGraphicsTypeId_descBytes[i],
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
	}

	//Graphics device success

	return Error_none();
}*/

Bool GraphicsDevice_free(const GraphicsInstance *instance, GraphicsDevice *device) {

	if(!instance || !device || !device->ext)
		return instance;

	Bool success = true;

	//TODO:
	//for(U64 j = 0; j < device->factories.length; ++j)
	//	success &= GraphicsObjectFactory_free(device, (GraphicsObjectFactory*)device->factories.ptr + j);

	success &= List_freex(&device->factories);
	success &= GraphicsDevice_freeExt(instance, &device->ext);
	return success;
}

//Submit and wait until all submitted graphics tasks are done.

Error GraphicsDevice_submitCommands(GraphicsDevice *device, List commandLists);		//<RefPtr of CommandList>
Error GraphicsDevice_wait(GraphicsDevice *device, U64 timeout);

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
