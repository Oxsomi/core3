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

typedef enum EGPUBufferUsage {

	EGPUBufferUsage_ShaderRead			= 1 << 0,		//Allow for use as ByteAddressBuffer
	EGPUBufferUsage_ShaderWrite			= 1 << 1,		//Allow for use as RWByteAddressBuffer
	EGPUBufferUsage_Vertex				= 1 << 2,		//Allow for use as vertex buffer
	EGPUBufferUsage_Index				= 1 << 3,		//Allow for use as index buffer
	EGPUBufferUsage_Indirect			= 1 << 4,		//Allow for use in indirect draw/dispatch calls
	EGPUBufferUsage_CPUBacked			= 1 << 5,		//Keep a CPU side copy buffer for read/write operations
	EGPUBufferUsage_CPUAllocatedBit		= 1 << 6,		//Keep buffer entirely on CPU for transfer or allowing more allocations

	EGPUBufferUsage_CPUAllocated		= EGPUBufferUsage_CPUBacked | EGPUBufferUsage_CPUAllocatedBit

} EGPUBufferUsage;

typedef RefPtr GPUBufferRef;

typedef struct GPUBuffer {

	GraphicsDeviceRef *device;

	EGPUBufferUsage usage;
	Bool isPendingFullCopy;
	Bool isPending;
	Bool isFirstFrame;
	U8 pad0[1];

	U64 length;

	Buffer cpuData;				//Null if not cpu backed and uploaded. If not cpu backed this will free after upload

	List pendingChanges;		//Pending ranges (GPUPendingRange)

} GPUBuffer;

#define GPUBuffer_ext(ptr, T) (!ptr ? NULL : (T##GPUBuffer*)(ptr + 1))		//impl
#define GPUBufferRef_ptr(ptr) RefPtr_data(ptr, GPUBuffer)

Error GPUBufferRef_dec(GPUBufferRef **buffer);
Error GPUBufferRef_add(GPUBufferRef *buffer);

//Create empty buffer or initialized with data.
//Initializing to non zero isn't free due to copies.
//	Initializing to non zero will move the buffer to created GPUBuffer. If it's a ref, it has to be kept active.

Error GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev, 
	EGPUBufferUsage usage, 
	CharString name, 
	U64 len, 
	GPUBufferRef **buf
);

Error GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev, 
	EGPUBufferUsage usage, 
	CharString name, 
	Buffer *dat, 
	GPUBufferRef **buf
);

//Mark the underlying data for the GPUBuffer as dirty. 
//Count 0 indicates rest of the buffer starting at offset.
//Each region that doesn't intersect will be considered as 1 copy (otherwise it will be merged).
//Call this as little as possible while still not copying too much data.
//Only possible if buffer has a backed CPU buffer.
Error GPUBufferRef_markDirty(GPUBufferRef *buffer, U64 offset, U64 count);
