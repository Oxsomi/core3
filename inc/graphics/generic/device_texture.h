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
#include "platforms/lock.h"
#include "device_buffer.h"

typedef enum EDeviceTextureUsage {

	EDeviceTextureUsage_ShaderRead			= 1 << 0,		//Allow for use in shaders
	EDeviceTextureUsage_CPUBacked			= 1 << 1,		//Keep a CPU side copy texture for read/write operations
	EDeviceTextureUsage_CPUAllocatedBit		= 1 << 2,		//Keep entirely on CPU

	EDeviceTextureUsage_CPUAllocated		= EDeviceTextureUsage_CPUBacked | EDeviceTextureUsage_CPUAllocatedBit

} EDeviceTextureUsage;

typedef enum ETextureFormatId ETextureFormatId;
typedef enum ETextureType ETextureType;

typedef RefPtr DeviceTextureRef;

typedef struct DeviceTexture {

	GraphicsDeviceRef *device;

	EDeviceTextureUsage usage;
	Bool isPendingFullCopy, isPending, isFirstFrame;
	U8 textureFormatId;						//ETextureFormatId

	U32 readHandle;							//Place in heap/descriptor set. 12 bits are reserved for type and/or version
	U16 width, height;

	U64 blockOffset;
	U32 blockId;
	U16 length;
	U8 type;								//ETextureType
	U8 padding;

	Buffer cpuData;							//Null if not cpu backed & uploaded. If not cpu backed this will free post upload

	ListDevicePendingRange pendingChanges;

	Lock lock;

} DeviceTexture;

#define DeviceTexture_ext(ptr, T) (!ptr ? NULL : (T##DeviceTexture*)(ptr + 1))		//impl
#define DeviceTextureRef_ptr(ptr) RefPtr_data(ptr, DeviceTexture)

Error DeviceTextureRef_dec(DeviceTextureRef **texture);
Error DeviceTextureRef_inc(DeviceTextureRef *texture);

Error GraphicsDeviceRef_createTexture(
	GraphicsDeviceRef *dev, 
	ETextureType type,
	ETextureFormatId format, 
	EDeviceTextureUsage usage, 
	U16 width,				//<= 16384
	U16 height,				//^
	U16 length,				//<= 256
	CharString name, 
	Buffer *dat, 
	DeviceTextureRef **tex
);

//Mark the underlying data for the DeviceTexture as dirty. 
//Count 0 indicates rest of the texture starting at offset.
//Each region that doesn't intersect will be considered as 1 copy (otherwise it will be merged).
//Call this as little as possible while still not copying too much data.
//Only possible if texture has a backed CPU texture.

Error DeviceTextureRef_markDirty(DeviceTextureRef *texture, U16 x, U16 y, U16 z, U16 w, U16 h, U16 l);
