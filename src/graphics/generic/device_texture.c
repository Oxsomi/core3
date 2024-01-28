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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "platforms/ext/bufferx.h"
#include "formats/texture.h"
#include "types/string.h"

Error DeviceTextureRef_dec(DeviceTextureRef **texture) {
	return !RefPtr_dec(texture) ? Error_invalidOperation(0, "DeviceTextureRef_dec()::texture is required") : Error_none();
}

Error DeviceTextureRef_inc(DeviceTextureRef *texture) {
	return !RefPtr_inc(texture) ? Error_invalidOperation(0, "DeviceTextureRef_inc()::texture is required") : Error_none();
}

Error DeviceTextureRef_markDirty(DeviceTextureRef *tex, U16 x, U16 y, U16 z, U16 w, U16 h, U16 l) {

	if(!tex || tex->typeId != EGraphicsTypeId_DeviceTexture)
		return Error_nullPointer(0, "DeviceTextureRef_markDirty()::tex is required");

	DeviceTexture *texture = DeviceTextureRef_ptr(tex);

	//Check range

	if(x >= texture->width || x + w > texture->width)
		return Error_outOfBounds(1, x + w, texture->width, "DeviceTextureRef_markDirty()::x+w out of bounds");

	if(y >= texture->height || y + h > texture->height)
		return Error_outOfBounds(2, y + h, texture->height, "DeviceTextureRef_markDirty()::y+h out of bounds");

	if(z >= texture->length || z + l > texture->length)
		return Error_outOfBounds(3, z + l, texture->length, "DeviceTextureRef_markDirty()::z+l out of bounds");

	ELockAcquire acq0 = Lock_lock(&texture->lock, U64_MAX);

	if(acq0 < ELockAcquire_Success)
		return Error_invalidOperation(1, "DeviceTextureRef_markDirty() couldn't acquire texture lock");

	Error err = Error_none();
	ELockAcquire acq1 = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->device);

	if(texture->isPendingFullCopy)		//Already has a full pending change, so no need to check anything.
		goto clean;

	if(!(texture->usage & EDeviceTextureUsage_CPUBacked) && !(texture->isFirstFrame && !x && !y && !w && !h))
		_gotoIfError(clean, Error_invalidOperation(
			2, "DeviceTextureRef_markDirty() can only be called on first frame for entire resource or if it's CPU backed"
		));

	if(!w)
		w = texture->width - x;

	if(!h)
		h = texture->height - y;

	if(!l)
		l = texture->length - z;

	Bool fullRange = w == texture->width && h == texture->height && l == texture->length;

	U16 startX = (x + 3) &~ 3;
	U16 endX = (U16) U64_min((x + w + 3) &~ 3, texture->width);

	U16 startY = (y + 3) &~ 3;
	U16 endY = (U16) U64_min((y + h + 3) &~ 3, texture->height);

	U16 startZ = (z + 3) &~ 3;
	U16 endZ = (U16) U64_min((z + l + 3) &~ 3, texture->length);

	//If the entire texture is marked dirty, we have to make sure we don't duplicate it

	Bool shouldPush = false;

	if(fullRange) {
		_gotoIfError(clean, ListDevicePendingRange_clear(&texture->pendingChanges));
		texture->isPendingFullCopy = true;
		shouldPush = true;
	}

	//Otherwise we have to merge current pending ranges

	else {

		//Merge with pending changes
		//4 pixels on either side to avoid lots of fragmented copies

		if (texture->isPending) {

			U64 lastMatch = U64_MAX;

			for (U64 i = texture->pendingChanges.length - 1; i != U64_MAX; --i) {

				DevicePendingRange *pending = &texture->pendingChanges.ptrNonConst[i];
				TextureRange *texturei = &pending->texture;

				//If intersects, we either merge with first occurence or pop last occurence and merge range with current

				if (
					endX >= texturei->startRange[0] && startX <= texturei->endRange[0] &&
					endY >= texturei->startRange[1] && startY <= texturei->endRange[1] &&
					endZ >= texturei->startRange[2] && startZ <= texturei->endRange[2]
				) {

					if (lastMatch == U64_MAX) {

						texturei->startRange[0] = (U16) U64_min(texturei->startRange[0], x);
						texturei->endRange[0] = (U16) U64_max(texturei->endRange[0], x + w);

						texturei->startRange[1] = (U16) U64_min(texturei->startRange[1], y);
						texturei->endRange[1] = (U16) U64_max(texturei->endRange[1], y + h);

						texturei->startRange[2] = (U16) U64_min(texturei->startRange[2], z);
						texturei->endRange[2] = (U16) U64_max(texturei->endRange[2], z + l);
					}

					else {

						DevicePendingRange last = texture->pendingChanges.ptr[lastMatch];

						for(U64 j = 0; j < 3; ++j) {
							texturei->startRange[j] = (U16) U64_min(texturei->startRange[j], last.texture.startRange[j]);
							texturei->endRange[j] = (U16) U64_max(texturei->endRange[j], last.texture.endRange[j]);
						}

						_gotoIfError(clean, ListDevicePendingRange_erase(&texture->pendingChanges, lastMatch));
					}

					lastMatch = i;
				}
			}

			shouldPush = lastMatch == U64_MAX;
		}

		else shouldPush = true;
	}

	if (shouldPush) {

		if((texture->pendingChanges.length + 1) >> 32)
			_gotoIfError(clean, Error_outOfBounds(
				0, U32_MAX, U32_MAX, "DeviceTextureRef_markDirty() texture pendingRanges is limited to U32_MAX"
			));

		DevicePendingRange change = (DevicePendingRange) { .texture = { 
			.startRange = { x, y, z },
			.endRange = { x + w, y + h, z + l },
			.levelId = 0
		}};

		_gotoIfError(clean, ListDevicePendingRange_pushBackx(&texture->pendingChanges, change));
	}

	//Tell the device that on next submit it should handle copies from 

	if(texture->isPending)
		goto clean;

	texture->isPending = true;

	acq1 = Lock_lock(&device->lock, U64_MAX);

	if(acq1 < ELockAcquire_Success)
		_gotoIfError(clean, Error_invalidState(0, "DeviceTextureRef_markDirty() couldn't lock device"));

	_gotoIfError(clean, ListWeakRefPtr_pushBackx(&device->pendingResources, tex));

clean:

	if(acq1 == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	if(acq0 == ELockAcquire_Acquired)
		Lock_unlock(&texture->lock);

	return err;
}

impl extern const U64 DeviceTextureExt_size;
impl Bool DeviceTexture_freeExt(DeviceTexture *texture);
impl Error GraphicsDeviceRef_createTextureExt(GraphicsDeviceRef *dev, DeviceTexture *tex, CharString name);

Bool DeviceTexture_free(DeviceTexture *texture, Allocator allocator) {

	allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)texture - sizeof(RefPtr));

	Lock_free(&texture->lock);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->device);
	DeviceMemoryAllocator_freeAllocation(&device->allocator, texture->blockId, texture->blockOffset);

	Bool success = DeviceTexture_freeExt(texture);
	success &= GraphicsDeviceRef_removePending(texture->device, refPtr);
	success &= !GraphicsDeviceRef_dec(&texture->device).genericError;
	success &= Buffer_freex(&texture->cpuData);
	success &= ListDevicePendingRange_freex(&texture->pendingChanges);

	return success;
}

Error GraphicsDeviceRef_createTexture(
	GraphicsDeviceRef *dev, 
	ETextureType type,
	ETextureFormatId formatId, 
	EDeviceTextureUsage usage, 
	U16 width,
	U16 height,
	U16 length,
	CharString name, 
	Buffer *dat, 
	DeviceTextureRef **tex
) {

	if(!dev || dev->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createTexture()::dev is required");

	if(!formatId || formatId >= ETextureFormatId_Count)
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createTexture()::format is invalid");

	if(type >= ETextureType_Count)
		return Error_invalidParameter(1, 0, "GraphicsDeviceRef_createTexture()::type is invalid");

	ETextureFormat format = ETextureFormatId_unpack[formatId];

	if(!GraphicsDeviceInfo_supportsFormat(&GraphicsDeviceRef_ptr(dev)->info, format))
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createTexture()::format is unsupported");

	if(!width || !height || !length)
		return Error_invalidParameter(
			!width ? 5 : (!height ? 6 : 7), 0, "GraphicsDeviceRef_createTexture()::width, height and depth are required"
		);

	if(width > 16384 || height > 16384 || length > 256)
		return Error_invalidParameter(
			width > 16384 ? 5 : (height > 16384 ? 6 : 7), 0, 
			"GraphicsDeviceRef_createTexture()::width, height and or length exceed limit (16384, 16384 and 256 respectively)"
		);

	if(length > 1 && type != ETextureType_3D)
		return Error_invalidParameter(7, 0, "GraphicsDeviceRef_createTexture()::length can't be non zero if type isn't 3D");

	if(!dat || !Buffer_length(*dat))
		return Error_nullPointer(3, "GraphicsDeviceRef_createTexture()::dat is required");

	U64 texSize = ETextureFormat_getSize(format, width, height, length);

	if(texSize == U64_MAX || Buffer_length(*dat) != texSize)
		return Error_nullPointer(3, "GraphicsDeviceRef_createTexture()::dat must match expected texture size and alignment");

	Error err = RefPtr_createx(
		(U32)(sizeof(DeviceTexture) + DeviceTextureExt_size), 
		(ObjectFreeFunc) DeviceTexture_free, 
		EGraphicsTypeId_DeviceTexture, 
		tex
	);

	if(err.genericError)
		return err;

	//Ensure our buffer is still active when submitCommands is called by making a copy

	ELockAcquire acq = ELockAcquire_Invalid;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	//Init DeviceTexture

	DeviceTexture *texture = DeviceTextureRef_ptr(*tex);
	_gotoIfError(clean, GraphicsDeviceRef_inc(dev));

	*texture = (DeviceTexture) { 
		.device = dev, 
		.usage = usage,
		.isFirstFrame = true, 
		.textureFormatId = (U8) formatId,
		.readHandle = U32_MAX,
		.width = width,
		.height = height,
		.length = length
	};

	//Link buffer and pending ranges

	Buffer *bufPtr = &DeviceTextureRef_ptr(*tex)->cpuData;

	if (Buffer_isRef(*dat))
		_gotoIfError(clean, Buffer_createCopyx(*dat, bufPtr))

	else *bufPtr = *dat;

	*dat = Buffer_createNull();

	_gotoIfError(clean, ListDevicePendingRange_reservex(
		&texture->pendingChanges, usage & EDeviceTextureUsage_CPUBacked ? 16 : 1
	));

	_gotoIfError(clean, GraphicsDeviceRef_createTextureExt(dev, texture, name));
	texture->lock = Lock_create();

	_gotoIfError(clean, DeviceTextureRef_markDirty(*tex, 0, 0, 0, 0, 0, 0));

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->allocator.lock);

	if(err.genericError)
		DeviceTextureRef_dec(tex);

	return err;
}
