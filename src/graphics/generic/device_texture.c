/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//graphics/generic/device_texture.c

#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/pipeline_structs.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/texture_format.h"
#include "types/container/string.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

static inline U16 alignDown(U16 x, U16 alignment) {
	return x / alignment * alignment;
}

static inline U16 alignUp(U16 x, U16 alignment) {
	return (x + alignment - 1) / alignment * alignment;
}

Bool DeviceTextureRef_markDirty(DeviceTextureRef *tex, U16 x, U16 y, U16 z, U16 w, U16 h, U16 l, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = tex ? GraphicsDeviceRef_getAlloc(DeviceTextureRef_ptr(tex)->base.resource.device) : NULL;

	DeviceTexture *texture = NULL;
	GraphicsDevice *device = NULL;
	ELockAcquire acq0 = ELockAcquire_Invalid;
	ELockAcquire acq1 = ELockAcquire_Invalid;

	if(!tex || tex->refPtrType->typeId != (TypeId) EGraphicsTypeId_DeviceTexture)
		retError(clean, Error_nullPointer(0, "DeviceTextureRef_markDirty()::tex is required"));

	texture = DeviceTextureRef_ptr(tex);
	UnifiedTexture *utex = &texture->base;

	//Check range

	if(x >= utex->width || x + w > utex->width)
		retError(clean, Error_outOfBounds(
			1, x + w, utex->width, "DeviceTextureRef_markDirty()::x+w out of bounds"
		));

	if(y >= utex->height || y + h > utex->height)
		retError(clean, Error_outOfBounds(
			2, y + h, utex->height, "DeviceTextureRef_markDirty()::y+h out of bounds"
		));

	if(z >= utex->length || z + l > utex->length)
		retError(clean, Error_outOfBounds(
			3, z + l, utex->length, "DeviceTextureRef_markDirty()::z+l out of bounds"
		));

	acq0 = SpinLock_lock(&texture->lock, U64_MAX);

	if(acq0 < ELockAcquire_Success)
		retError(clean, Error_invalidOperation(1, "DeviceTextureRef_markDirty() couldn't acquire texture lock"));

	device = GraphicsDeviceRef_ptr(utex->resource.device);

	if(texture->isPendingFullCopy)        //Already has a full pending change, so no need to check anything.
		goto clean;

	//The first-frame exception only permits the internal initial upload of a not-CPU-backed texture.
	//That upload always has staged data in cpuData, so require it here.
	//Otherwise a not-backed texture with no data would be wrongly accepted.

	if(
		!(texture->base.resource.flags & EGraphicsResourceFlag_CPUBacked) &&
		!(texture->isFirstFrame && !x && !y && !z && !w && !h && !l && Buffer_length(texture->cpuData))
	)
		retError(clean, Error_invalidOperation(
			2,
			"DeviceTextureRef_markDirty() can only be called on first frame for the entire resource "
			"with staged CPU data, or if it's CPU backed"
		));

	if(!w)
		w = utex->width - x;

	if(!h)
		h = utex->height - y;

	if(!l)
		l = utex->length - z;

	ETextureFormat format = ETextureFormatId_unpack[utex->textureFormatId];

	U8 alignX = 4, alignY = 4, alignZ = 4;
	U8 realAlignX = 1, realAlignY = 1;

	if (ETextureFormat_getAlignment(format, &alignX, &alignY)) {        //Ensure alignment is respected later on
		realAlignX = alignX;
		realAlignY = alignY;
	}

	U16 startX = alignDown(x, alignX);
	U16 endX = (U16) U64_min(alignUp(x + w, alignX), utex->width);

	U16 startY = alignDown(y, alignY);
	U16 endY = (U16) U64_min(alignUp(y + h, alignY), utex->height);

	U16 startZ = alignDown(z, alignZ);
	U16 endZ = (U16) U64_min(alignUp(z + l, alignZ), utex->length);

	Bool fullRange = (endX - startX) == utex->width && (endY - startY) == utex->height && (endZ - startZ) == utex->length;

	//If the entire texture is marked dirty, we have to make sure we don't duplicate it

	Bool shouldPush = false;

	if(fullRange) {
		gotoIfError3(clean, ListDevicePendingRange_clear(&texture->pendingChanges, e_rr));
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

						gotoIfError3(clean, ListDevicePendingRange_erase(&texture->pendingChanges, lastMatch, e_rr));
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
			retError(clean, Error_outOfBounds(
				0, U32_MAX, U32_MAX, "DeviceTextureRef_markDirty() texture pendingRanges is limited to U32_MAX"));

		DevicePendingRange change = (DevicePendingRange) { .texture = {
			.startRange = { x, y, z },
			.endRange = { x + w, y + h, z + l },
			.levelId = 0
		}};

		gotoIfError3(clean, ListDevicePendingRange_pushBack(&texture->pendingChanges, change, alloc, e_rr));
	}

	//Tell the device that on next submit it should handle copies from

	if(texture->isPending)
		goto clean;

	texture->isPending = true;

	acq1 = SpinLock_lock(&device->lock, U64_MAX);

	if(acq1 < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "DeviceTextureRef_markDirty() couldn't lock device"));

	gotoIfError3(clean, ListWeakRefPtr_pushBack(&device->pendingResources, tex, alloc, e_rr));

clean:

	if(acq1 == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(acq0 == ELockAcquire_Acquired)
		SpinLock_unlock(&texture->lock);

	return s_uccess;
}

void DeviceTexture_free(DeviceTexture *texture, const Allocator *alloc) {

	(void)alloc;

	RefPtr *refPtr = (RefPtr*)((const U8*)texture - sizeof(RefPtr));

	SpinLock_lock(&texture->lock, U64_MAX);

	UnifiedTexture_free(refPtr);
	Buffer_free(&texture->cpuData, alloc);
	ListDevicePendingRange_free(&texture->pendingChanges, alloc);
}

Bool GraphicsDeviceRef_createTexture(
	GraphicsDeviceRef *dev,
	ETextureType type,
	ETextureFormatId formatId,
	EGraphicsResourceFlag flag,
	U16 width,
	U16 height,
	U16 length,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	Buffer *dat,
	DeviceTextureRef **tex,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	//Checked first, since getTypes on a NULL device yields a non NULL member pointer that faults in RefPtr_create.

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createTexture()::dev is required"));

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	if(!formatId || formatId >= ETextureFormatId_Count)
		retError(clean, Error_invalidParameter(2, 0, "GraphicsDeviceRef_createTexture()::format is invalid"));

	if(flag & EGraphicsResourceFlag_ShaderWrite)
		retError(clean, Error_invalidParameter(
			2, 0, "GraphicsDeviceRef_createTexture()::flag ShaderWrite is disallowed on DeviceTexture"
		));

	const ETextureFormat format = ETextureFormatId_unpack[formatId];
	const U64 texSize = ETextureFormat_getSize(format, width, height, length);

	if(texSize == U64_MAX || !dat || Buffer_length(*dat) != texSize)
		retError(clean, Error_nullPointer(
			3, "GraphicsDeviceRef_createTexture()::dat must match expected texture size and alignment"
		));

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->deviceTexture, tex, e_rr));
	allocated = true;

	if(!(flag & EGraphicsResourceFlag_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(dev));

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

	gotoIfError3(clean, UnifiedTexture_create(*tex, bindlessDescriptorTable, name, e_rr));

	if(Buffer_isRef(*dat)) {
		//Temporary if not CPUBacked
		gotoIfError3(clean, Buffer_createEmptyBytes(texSize, alloc, &texture->cpuData, e_rr));
		Buffer_memcpy(texture->cpuData, *dat);
	}

	else {                                    //Move
		texture->cpuData = *dat;
		*dat = Buffer_createNull();
	}

	gotoIfError3(clean, ListDevicePendingRange_reserve(
		&texture->pendingChanges, flag & EGraphicsResourceFlag_CPUBacked ? 16 : 1, alloc, e_rr
	));

	gotoIfError3(clean, DeviceTextureRef_markDirty(*tex, 0, 0, 0, 0, 0, 0, e_rr));

clean:

	if(!s_uccess && allocated)
		RefPtr_dec(tex);

	return s_uccess;
}

Bool DeviceTextureRef_pullRegion(
	DeviceTextureRef *tex,
	U16 x, U16 y, U16 z,
	U16 w, U16 h, U16 l,
	DevicePullCallback callback, void *context, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = NULL;

	GraphicsDevice *device = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;
	Bool owned = false;

	//Validated before the pointer is reinterpreted, since a wrong type would read a garbage device

	if(!tex || tex->refPtrType->typeId != (TypeId) EGraphicsTypeId_DeviceTexture)
		retError(clean, Error_nullPointer(0, "DeviceTextureRef_pullRegion()::tex is required"));

	DeviceTexture *texture = DeviceTextureRef_ptr(tex);
	alloc = GraphicsDeviceRef_getAlloc(texture->base.resource.device);

	if(!(texture->base.resource.flags & EGraphicsResourceFlag_CPUBacked))
		retError(clean, Error_invalidOperation(0, "DeviceTextureRef_pullRegion() requires a CPUBacked texture"));

	//Only the whole uncompressed texture for now; regions and block formats need the row math audited first

	if(ETextureFormat_getIsCompressed(ETextureFormatId_unpack[texture->base.textureFormatId]))
		retError(clean, Error_unsupportedOperation(
			0, "DeviceTextureRef_pullRegion() doesn't support compressed formats yet"
		));

	if(
		x || y || z ||
		(w && w != texture->base.width) || (h && h != texture->base.height) || (l && l != texture->base.length)
	)
		retError(clean, Error_unsupportedOperation(
			1, "DeviceTextureRef_pullRegion() only supports the full texture for now"
		));

	device = GraphicsDeviceRef_ptr(texture->base.resource.device);

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidOperation(1, "DeviceTextureRef_pullRegion() couldn't acquire device lock"));

	RefPtr_inc(tex);
	owned = true;

	const DevicePendingPull pull = (DevicePendingPull) {
		.resource = tex,
		.callback = callback,
		.context = context,
		.range = (DevicePendingRange) { .texture = (TextureRange) {
			.endRange = { texture->base.width, texture->base.height, texture->base.length }
		} }
	};

	gotoIfError3(clean, ListDevicePendingPull_pushBack(&device->pendingPulls, pull, alloc, e_rr));
	owned = false;

clean:

	if(owned)
		RefPtr_dec(&tex);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess;
}
