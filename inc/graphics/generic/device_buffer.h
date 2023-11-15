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
#include "platforms/ref_ptr.h"
#include "types/list.h"

typedef RefPtr GraphicsDeviceRef;
typedef struct CharString CharString;

typedef enum EDeviceBufferUsage {

	EDeviceBufferUsage_ShaderRead			= 1 << 0,		//Allow for use as ByteAddressBuffer
	EDeviceBufferUsage_ShaderWrite			= 1 << 1,		//Allow for use as RWByteAddressBuffer
	EDeviceBufferUsage_Vertex				= 1 << 2,		//Allow for use as vertex buffer
	EDeviceBufferUsage_Index				= 1 << 3,		//Allow for use as index buffer
	EDeviceBufferUsage_Indirect				= 1 << 4,		//Allow for use in indirect draw/dispatch calls
	EDeviceBufferUsage_CPUBacked			= 1 << 5,		//Keep a CPU side copy buffer for read/write operations
	EDeviceBufferUsage_CPUAllocatedBit		= 1 << 6,		//Keep buffer entirely on CPU

	EDeviceBufferUsage_InternalWeakRef		= 1 << 7,		//Internal only, uses weak device ref

	EDeviceBufferUsage_CPUAllocated		= EDeviceBufferUsage_CPUBacked | EDeviceBufferUsage_CPUAllocatedBit

} EDeviceBufferUsage;

typedef RefPtr DeviceBufferRef;

typedef struct DeviceBuffer {

	GraphicsDeviceRef *device;

	EDeviceBufferUsage usage;
	Bool isPendingFullCopy, isPending, isFirstFrame;
	U8 padding0;

	U32 readHandle, writeHandle;		//Place in heap/descriptor set. First 12 bits are reserved for type and/or version.

	U64 length;

	Buffer cpuData;						//Null if not cpu backed and uploaded. If not cpu backed this will free after upload

	List pendingChanges;				//Pending ranges (DevicePendingRange)

	void *mappedMemory;					//Mapped memory, only accessible through markDirty.

	U64 blockOffset;
	U32 blockId, padding1;

} DeviceBuffer;

#define DeviceBuffer_ext(ptr, T) (!ptr ? NULL : (T##DeviceBuffer*)(ptr + 1))		//impl
#define DeviceBufferRef_ptr(ptr) RefPtr_data(ptr, DeviceBuffer)

Error DeviceBufferRef_dec(DeviceBufferRef **buffer);
Error DeviceBufferRef_inc(DeviceBufferRef *buffer);

//Create empty buffer or initialized with data.
//Initializing to non zero isn't free due to copies.
//	Initializing to non zero will move the buffer to created DeviceBuffer. If it's a ref, it has to be kept active.

Error GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev, 
	EDeviceBufferUsage usage, 
	CharString name, 
	U64 len, 
	DeviceBufferRef **buf
);

Error GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev, 
	EDeviceBufferUsage usage, 
	CharString name, 
	Buffer *dat, 
	DeviceBufferRef **buf
);

//Mark the underlying data for the DeviceBuffer as dirty. 
//Count 0 indicates rest of the buffer starting at offset.
//Each region that doesn't intersect will be considered as 1 copy (otherwise it will be merged).
//Call this as little as possible while still not copying too much data.
//Only possible if buffer has a backed CPU buffer.

Error DeviceBufferRef_markDirty(DeviceBufferRef *buffer, U64 offset, U64 count);
