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
#include "types/base/lock.h"
#include "device.h"
#include "resource.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsDeviceRef;

typedef enum EDeviceBufferUsage {

	EDeviceBufferUsage_None					= 0,

	EDeviceBufferUsage_Vertex				= 1 << 0,		//Allow for use as vertex buffer
	EDeviceBufferUsage_Index				= 1 << 1,		//Allow for use as index buffer
	EDeviceBufferUsage_Indirect				= 1 << 2,		//Allow for use in indirect draw/dispatch calls

	//Raytracing types

	EDeviceBufferUsage_ScratchExt			= 1 << 3,		//Allow for internal use as scratch buffer
	EDeviceBufferUsage_ASExt				= 1 << 4,		//Allow for internal use as acceleration structure
	EDeviceBufferUsage_ASReadExt			= 1 << 5,		//Allow buffer to be read by AS creation
	EDeviceBufferUsage_SBTExt				= 1 << 6		//Allow for internal use as shader binding table

} EDeviceBufferUsage;

typedef RefPtr DeviceBufferRef;

TList(DevicePendingRange);

typedef struct DeviceData {
	DeviceBufferRef *buffer;
	U64 offset, len;
} DeviceData;

typedef struct DeviceBuffer {

	GraphicsResource resource;

	EDeviceBufferUsage usage;
	Bool isPendingFullCopy, isPending, isFirstFrame;
	U8 padding0;

	Buffer cpuData;							//Null if not cpu backed & uploaded. If not cpu backed this will free post upload

	ListDevicePendingRange pendingChanges;

	SpinLock lock;

	U32 readHandle, writeHandle;

} DeviceBuffer;

#define DeviceBuffer_ext(ptr, T) (!ptr ? NULL : (T##DeviceBuffer*)(ptr + 1))		//impl
#define DeviceBufferRef_ptr(ptr) RefPtr_data(ptr, DeviceBuffer)

Error DeviceBufferRef_dec(DeviceBufferRef **buffer);
Error DeviceBufferRef_inc(DeviceBufferRef *buffer);

//Create empty buffer or initialized with data.
//Initializing to non-zero isn't free due to copies.
//	Initializing to non-zero will move the buffer to created DeviceBuffer, unless it's a ref (then it will create a new one)

Error GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
	CharString name,
	U64 len,
	DeviceBufferRef **buf
);

Error GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
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

#ifdef __cplusplus
	}
#endif
