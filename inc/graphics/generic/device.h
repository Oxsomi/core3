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
#include "graphics/generic/allocator.h"
#include "platforms/ref_ptr.h"
#include "types/list.h"

typedef struct Error Error;
typedef RefPtr GraphicsInstanceRef;
typedef RefPtr DeviceBufferRef;

typedef struct CBufferData {

	U32 frameId;					//Can loop back to 0 after U32_MAX!
	F32 time;						//Time since launch of app
	F32 deltaTime;					//deltaTime since last frame.
	U32 swapchainCount;				//How many swapchains are present (will insert ids into appData)

	U32 swapchains[2 * 16];

	U32 appData[(512 - 16 - 2 * 16 * 4) / 4];

} CBufferData;

typedef struct BufferRange {
	U64 startRange;
	U64 endRange;
} BufferRange;

typedef struct TextureRange {
	U8 startRange[4];
	U8 endRange[4];
} TextureRange;

typedef union DevicePendingRange {

	BufferRange buffer;
	TextureRange texture;

} DevicePendingRange;

typedef struct GraphicsDevice {

	GraphicsInstanceRef *instance;

	GraphicsDeviceInfo info;

	U64 submitId;

	Ns lastSubmit;

	Ns firstSubmit;				//Start of time

	List pendingResources;		//<WeakRefPtr*> Resources pending copy from CPU to device next submit

	List resourcesInFlight[3];	//<RefPtr*> Resources in flight

	Lock lock;					//Lock for submission and marking resources dirty

	DeviceMemoryAllocator allocator;

	//Staging allocations and buffers that are used to transmit/receive data from the device

	DeviceBufferRef *staging;					//Staging buffer split by 3
	AllocationBuffer stagingAllocations[3];

	//Graphics constants (globals) accessible by all shaders

	DeviceBufferRef *frameData;

	//Temporary for processing command list and to avoid allocations

	List currentLocks;

	U64 pendingBytes;			//For determining if it's time to flush or to resize staging buffer

} GraphicsDevice;

typedef RefPtr GraphicsDeviceRef;

#define GraphicsDevice_ext(ptr, T) (!ptr ? NULL : (T##GraphicsDevice*)(ptr + 1))		//impl
#define GraphicsDeviceRef_ptr(ptr) RefPtr_data(ptr, GraphicsDevice)

Error GraphicsDeviceRef_dec(GraphicsDeviceRef **device);
Error GraphicsDeviceRef_inc(GraphicsDeviceRef *device);

Error GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef, 
	const GraphicsDeviceInfo *info, 
	Bool verbose,
	GraphicsDeviceRef **device
);

//Ensure there are no pending changes from non existent resources.
Bool GraphicsDeviceRef_removePending(GraphicsDeviceRef *deviceRef, RefPtr *resource);

//Submit commands to device
//List<CommandListRef*> commandLists
//List<SwapchainRef*> swapchains
//appData is up to a 368 byte per frame array used for transmitting render critical info.
Error GraphicsDeviceRef_submitCommands(GraphicsDeviceRef *deviceRef, List commandLists, List swapchains, Buffer appData);

//Wait on previously submitted commands
Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef);

//Private

Error GraphicsDeviceRef_handleNextFrame(GraphicsDeviceRef *deviceRef, void *commandBuffer);
Error GraphicsDeviceRef_resizeStagingBuffer(GraphicsDeviceRef *deviceRef, U64 newSize);
