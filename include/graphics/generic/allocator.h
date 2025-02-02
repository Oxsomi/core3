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

#pragma once
#include "types/container/list.h"
#include "types/base/error.h"
#include "types/container/allocation_buffer.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct GraphicsDevice GraphicsDevice;

typedef enum EResourceType EResourceType;

typedef struct DeviceMemoryBlock {

	AllocationBuffer allocations;	//Can be absent if dedicated

	U32 typeExt;					//Resource flags

	Bool isDedicated;
	Bool isActive;
	U16 allocationTypeExt;			//Allocation type flags (e.g. host visible)

	U8 *mappedMemoryExt;			//Not always available, can be done on a resource basis

	void *ext;						//Extended data (Can also be NULL if dedicated on some apis)

	#ifndef NDEBUG
		void *stackTrace[16];		//Tracking memleaks if Debug flag is on
	#else
		void *stackTrace[4];		//Callstack might still be needed for leaks if Debug flag for gfx instance
	#endif

} DeviceMemoryBlock;

TList(DeviceMemoryBlock);

typedef struct DeviceMemoryAllocator {

	GraphicsDevice *device;

	SpinLock lock;

	ListDeviceMemoryBlock blocks;

} DeviceMemoryAllocator;

//Locks automatically
Bool DeviceMemoryAllocator_freeAllocation(DeviceMemoryAllocator *allocator, U32 blockId, U64 blockOffset);

#ifdef __cplusplus
	}
#endif
