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

#pragma once
#include "graphics/generic/device_info.h"
#include "platforms/ref_ptr.h"
#include "types/list.h"

typedef struct Error Error;
typedef RefPtr GraphicsInstanceRef;

typedef struct GraphicsDevice {

	GraphicsInstanceRef *instance;

	GraphicsDeviceInfo info;

	U64 submitId;

	//List factories;		//<GraphicsObjectFactory>

} GraphicsDevice;

typedef RefPtr GraphicsDeviceRef;

#define GraphicsDevice_ext(ptr, T) (T##GraphicsDevice*)(ptr + 1)		//impl
#define GraphicsDeviceRef_ptr(ptr) RefPtr_data(ptr, GraphicsDevice)

Error GraphicsDeviceRef_dec(GraphicsDeviceRef **device);
Error GraphicsDeviceRef_add(GraphicsDeviceRef *device);

Error GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef, 
	const GraphicsDeviceInfo *info, 
	Bool verbose,
	GraphicsDeviceRef **device
);

Bool GraphicsDevice_free(GraphicsDevice *device, Allocator alloc);

//Submit commands to device
//List<CommandListRef*> commandLists
//List<SwapchainRef*> swapchains
impl Error GraphicsDeviceRef_submitCommands(GraphicsDeviceRef *deviceRef, List commandLists, List swapchains);

//Wait on previously submitted commands
impl Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef);

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

