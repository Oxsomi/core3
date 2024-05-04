/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "types/lock.h"
#include "device_buffer.h"
#include "texture.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr DeviceTextureRef;

//DeviceTexture, UnifiedTexture (Resource, ...), UnifiedTextureImage_size[N], UnifiedTextureExt_size[N], TextureExt
typedef struct DeviceTexture {

	Bool isPendingFullCopy, isPending, isFirstFrame;
	U8 pad0;
	U32 pad1;

	Buffer cpuData;											//If not cpu backed this will free post upload

	ListDevicePendingRange pendingChanges;

	Lock lock;

	UnifiedTexture base;

} DeviceTexture;

#define DeviceTextureRef_ptr(ptr) RefPtr_data(ptr, DeviceTexture)

Error DeviceTextureRef_dec(DeviceTextureRef **texture);
Error DeviceTextureRef_inc(DeviceTextureRef *texture);

Error GraphicsDeviceRef_createTexture(
	GraphicsDeviceRef *dev,
	ETextureType type,
	ETextureFormatId format,
	EGraphicsResourceFlag flag,
	U16 width,					//<= 16384
	U16 height,					//^
	U16 length,					//<= 256
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

#ifdef __cplusplus
	}
#endif
