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
#include "types/type_id.h"
#include "types/list.h"

typedef struct Error Error;
typedef struct RefPtr RefPtr;
typedef struct GraphicsInstance GraphicsInstance;

//ETypeId but for graphics factories. 
//Properties contain if it allows lookup by an info struct.

typedef enum EGraphicsTypeId {

	EGraphicsTypeId_Texture					= _makeObjectId(0xC4, 0, 1),
	EGraphicsTypeId_RenderTexture			= _makeObjectId(0xC4, 1, 0),
	EGraphicsTypeId_Buffer					= _makeObjectId(0xC4, 2, 1),
	EGraphicsTypeId_Pipeline				= _makeObjectId(0xC4, 3, 1),
	EGraphicsTypeId_DescriptorSet			= _makeObjectId(0xC4, 4, 1),
	EGraphicsTypeId_RenderPass				= _makeObjectId(0xC4, 5, 0),
	EGraphicsTypeId_Sampler					= _makeObjectId(0xC4, 6, 1),
	EGraphicsTypeId_CommandList				= _makeObjectId(0xC4, 7, 0),
	EGraphicsTypeId_AccelerationStructure	= _makeObjectId(0xC4, 8, 1),
	EGraphicsTypeId_Swapchain				= _makeObjectId(0xC4, 9, 0),

	EGraphicsTypeId_Count					= 10

} EGraphicsTypeId;

extern EGraphicsTypeId EGraphicsTypeId_all[EGraphicsTypeId_Count];
extern U64 EGraphicsTypeId_descBytes[EGraphicsTypeId_Count];
extern U64 EGraphicsTypeId_objectBytes[EGraphicsTypeId_Count];

typedef struct GraphicsDevice {

	GraphicsDeviceInfo info;

	List factories;		//<GraphicsObjectFactory>

	void *ext;			//Underlying api implementation

} GraphicsDevice;

Error GraphicsDevice_create(const GraphicsInstance *instance, const GraphicsDeviceInfo *info, GraphicsDevice *device);
Bool GraphicsDevice_free(const GraphicsInstance *instance, GraphicsDevice *device);

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

