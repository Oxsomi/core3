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
#include "types/list.h"
#include "types/allocation_buffer.h"

typedef struct GraphicsDevice GraphicsDevice;
typedef struct CharString CharString;

typedef struct DeviceMemoryBlock {

	AllocationBuffer allocations;

	U32 typeExt;				//Resource flags

	Bool isDedicated;
	U8 padding;
	U16 allocationTypeExt;		//Allocation type flags (e.g. host visible)

	U8 *mappedMemory;

	void *ext;						//Extended data

} DeviceMemoryBlock;

typedef struct DeviceMemoryAllocator {

	GraphicsDevice *device;

	List blocks;

} DeviceMemoryAllocator;

static const U64 DeviceMemoryBlock_defaultSize = 67'108'864;

impl Error DeviceMemoryAllocator_allocate(
	DeviceMemoryAllocator *allocator, 
	void *requirementsExt, 
	Bool cpuSIded, 
	U32 *blockId, 
	U64 *blockOffset,
	CharString objectName				//Name of the object that allocates (for dedicated allocations)
);

Bool DeviceMemoryAllocator_freeAllocation(DeviceMemoryAllocator *allocator, U32 blockId, U64 blockOffset);
