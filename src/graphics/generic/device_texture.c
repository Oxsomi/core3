/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/interface.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/pipeline_structs.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/container/texture_format.h"
#include "types/math/math.h"
#include "types/container/string.h"

Error DeviceTextureRef_dec(DeviceTextureRef **texture) {
	return !RefPtr_dec(texture) ?
		Error_invalidOperation(0, "DeviceTextureRef_dec()::texture is required") : Error_none();
}

Error DeviceTextureRef_inc(DeviceTextureRef *texture) {
	return !RefPtr_inc(texture) ?
		Error_invalidOperation(0, "DeviceTextureRef_inc()::texture is required") : Error_none();
}

U16 alignDown(U16 x, U16 alignment) {
	return x / alignment * alignment;
}

U16 alignUp(U16 x, U16 alignment) {
	return (x + alignment - 1) / alignment * alignment;
}

Error DeviceTextureRef_markDirty(DeviceTextureRef *tex, U16 x, U16 y, U16 z, U16 w, U16 h, U16 l) {

	if(!tex || tex->typeId != (ETypeId) EGraphicsTypeId_DeviceTexture)
		return Error_nullPointer(0, "DeviceTextureRef_markDirty()::tex is required");

	DeviceTexture *texture = DeviceTextureRef_ptr(tex);
	UnifiedTexture *utex = &texture->base;

	//Check range

	if(x >= utex->width || x + w > utex->width)
		return Error_outOfBounds(
			1, x + w, utex->width, "DeviceTextureRef_markDirty()::x+w out of bounds"
		);

	if(y >= utex->height || y + h > utex->height)
		return Error_outOfBounds(
			2, y + h, utex->height, "DeviceTextureRef_markDirty()::y+h out of bounds"
		);

	if(z >= utex->length || z + l > utex->length)
		return Error_outOfBounds(
			3, z + l, utex->length, "DeviceTextureRef_markDirty()::z+l out of bounds"
		);

	ELockAcquire acq0 = SpinLock_lock(&texture->lock, U64_MAX);

	if(acq0 < ELockAcquire_Success)
		return Error_invalidOperation(1, "DeviceTextureRef_markDirty() couldn't acquire texture lock");

	Error err = Error_none();
	ELockAcquire acq1 = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(utex->resource.device);

	if(texture->isPendingFullCopy)		//Already has a full pending change, so no need to check anything.
		goto clean;

	if(
		!(texture->base.resource.flags & EGraphicsResourceFlag_CPUBacked) &&
		!(texture->isFirstFrame && !x && !y && !z && !w && !h && !l)
	)
		gotoIfError(clean, Error_invalidOperation(
			2, "DeviceTextureRef_markDirty() can only be called on first frame for entire resource or if it's CPU backed"
		))

	if(!w)
		w = utex->width - x;

	if(!h)
		h = utex->height - y;

	if(!l)
		l = utex->length - z;

	Bool fullRange = w == utex->width && h == utex->height && l == utex->length;

	ETextureFormat format = ETextureFormatId_unpack[utex->textureFormatId];

	U8 alignX = 4, alignY = 4, alignZ = 4;
	U8 realAlignX = 1, realAlignY = 1;

	if (ETextureFormat_getAlignment(format, &alignX, &alignY)) {		//Ensure alignment is respected later on
		realAlignX = alignX;
		realAlignY = alignY;
	}

	U16 startX = alignDown(x, alignX);
	U16 endX = (U16) U64_min(alignUp(x + w, alignX), utex->width);

	U16 startY = alignDown(y, alignY);
	U16 endY = (U16) U64_min(alignUp(y + h, alignY), utex->height);

	U16 startZ = alignDown(z, alignZ);
	U16 endZ = (U16) U64_min(alignUp(z + l, alignZ), utex->length);

	//If the entire texture is marked dirty, we have to make sure we don't duplicate it

	Bool shouldPush = false;

	if(fullRange) {
		gotoIfError(clean, ListDevicePendingRange_clear(&texture->pendingChanges))
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

				U16 *start = texturei->startRange;
				U16 *end = texturei->endRange;

				if (
					endX >= start[0] && startX <= end[0] &&
					endY >= start[1] && startY <= end[1] &&
					endZ >= start[2] && startZ <= end[2]
				) {

					if (lastMatch == U64_MAX) {

						start[0] = (U16) U64_min(start[0], alignDown(x, realAlignX));
						end[0] = (U16) U64_max(end[0], alignUp(x + w, realAlignX));

						start[1] = (U16) U64_min(start[1], alignDown(y, realAlignY));
						end[1] = (U16) U64_max(end[1], alignUp(y + h, realAlignY));

						start[2] = (U16) U64_min(start[2], z);
						end[2] = (U16) U64_max(end[2], z + l);
					}

					else {

						DevicePendingRange last = texture->pendingChanges.ptr[lastMatch];

						for(U64 j = 0; j < 3; ++j) {
							start[j] = (U16) U64_min(start[j], last.texture.startRange[j]);
							end[j] = (U16) U64_max(end[j], last.texture.endRange[j]);
						}

						gotoIfError(clean, ListDevicePendingRange_erase(&texture->pendingChanges, lastMatch))
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
			gotoIfError(clean, Error_outOfBounds(
				0, U32_MAX, U32_MAX, "DeviceTextureRef_markDirty() texture pendingRanges is limited to U32_MAX"
			))

		DevicePendingRange change = (DevicePendingRange) { .texture = {
			.startRange = { x, y, z },
			.endRange = { x + w, y + h, z + l },
			.levelId = 0
		}};

		gotoIfError(clean, ListDevicePendingRange_pushBackx(&texture->pendingChanges, change))
	}

	//Tell the device that on next submit it should handle copies from

	if(texture->isPending)
		goto clean;

	texture->isPending = true;

	acq1 = SpinLock_lock(&device->lock, U64_MAX);

	if(acq1 < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(0, "DeviceTextureRef_markDirty() couldn't lock device"))

	gotoIfError(clean, ListWeakRefPtr_pushBackx(&device->pendingResources, tex))

clean:

	if(acq1 == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(acq0 == ELockAcquire_Acquired)
		SpinLock_unlock(&texture->lock);

	return err;
}

Bool DeviceTexture_free(DeviceTexture *texture, Allocator allocator) {

	(void)allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)texture - sizeof(RefPtr));

	SpinLock_lock(&texture->lock, U64_MAX);

	Bool success = UnifiedTexture_free(refPtr);
	success &= Buffer_freex(&texture->cpuData);
	success &= ListDevicePendingRange_freex(&texture->pendingChanges);

	return success;
}

Error GraphicsDeviceRef_createTexture(
	GraphicsDeviceRef *dev,
	ETextureType type,
	ETextureFormatId formatId,
	EGraphicsResourceFlag flag,
	U16 width,
	U16 height,
	U16 length,
	CharString name,
	Buffer *dat,
	DeviceTextureRef **tex
) {

	if(!formatId || formatId >= ETextureFormatId_Count)
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createTexture()::format is invalid");

	if(flag & EGraphicsResourceFlag_ShaderWrite)
		return Error_invalidParameter(
			2, 0, "GraphicsDeviceRef_createTexture()::flag ShaderWrite is disallowed on DeviceTexture"
		);

	const ETextureFormat format = ETextureFormatId_unpack[formatId];
	const U64 texSize = ETextureFormat_getSize(format, width, height, length);

	if(texSize == U64_MAX || !dat || Buffer_length(*dat) != texSize)
		return Error_nullPointer(
			3, "GraphicsDeviceRef_createTexture()::dat must match expected texture size and alignment"
		);

	Error err = RefPtr_createx(
		(U32)(sizeof(DeviceTexture) + GraphicsDeviceRef_getObjectSizes(dev)->image + sizeof(UnifiedTextureImage)),
		(ObjectFreeFunc) DeviceTexture_free,
		(ETypeId) EGraphicsTypeId_DeviceTexture,
		tex
	);

	if(err.genericError)
		return err;

	if(!(flag & EGraphicsResourceFlag_InternalWeakDeviceRef))
		gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	DeviceTexture *texture = DeviceTextureRef_ptr(*tex);

	*texture = (DeviceTexture) {
		.base = (UnifiedTexture) {
			(GraphicsResource) {
				.device = dev,
				.size = texSize,
				.flags = flag,
				.type = EResourceType_DeviceTexture
			},
			.textureFormatId = (U8) formatId,
			.sampleCount = EMSAASamples_Off,
			.type = (U8) type,
			.width = width,
			.height = height,
			.length = length,
			.levels = 1,
			.images = 1
		},
		.isFirstFrame = true
	};

	gotoIfError(clean, UnifiedTexture_create(*tex, name))

	if(Buffer_isRef(*dat)) {
		gotoIfError(clean, Buffer_createEmptyBytesx(texSize, &texture->cpuData))		//Temporary if not CPUBacked
		Buffer_copy(texture->cpuData, *dat);
	}

	else {									//Move
		texture->cpuData = *dat;
		*dat = Buffer_createNull();
	}

	gotoIfError(clean, ListDevicePendingRange_reservex(
		&texture->pendingChanges, flag & EGraphicsResourceFlag_CPUBacked ? 16 : 1
	))

	gotoIfError(clean, DeviceTextureRef_markDirty(*tex, 0, 0, 0, 0, 0, 0))

clean:

	if(err.genericError)
		DeviceTextureRef_dec(tex);

	return err;
}
