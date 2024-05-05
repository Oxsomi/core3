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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/device_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/math.h"
#include "types/string.h"

TListImpl(DevicePendingRange);

Error DeviceBufferRef_dec(DeviceBufferRef **buffer) {
	return !RefPtr_dec(buffer) ?
		Error_invalidOperation(0, "DeviceBufferRef_dec()::buffer is required") : Error_none();
}

Error DeviceBufferRef_inc(DeviceBufferRef *buffer) {
	return !RefPtr_inc(buffer) ?
		Error_invalidOperation(0, "DeviceBufferRef_inc()::buffer is required") : Error_none();
}

Error DeviceBufferRef_markDirty(DeviceBufferRef *buf, U64 offset, U64 count) {

	if(!buf || buf->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_nullPointer(0, "DeviceBufferRef_markDirty()::buf is required");

	DeviceBuffer *buffer = DeviceBufferRef_ptr(buf);
	U64 bufLen = buffer->resource.size;

	//Check range

	if(offset >= bufLen || offset + count > bufLen)
		return Error_outOfBounds(
			1, offset + count, bufLen, "DeviceBufferRef_markDirty()::offset+count out of bounds"
		);

	ELockAcquire acq0 = Lock_lock(&buffer->lock, U64_MAX);

	if(acq0 < ELockAcquire_Success)
		return Error_invalidOperation(1, "DeviceBufferRef_markDirty() couldn't acquire buffer lock");

	Error err = Error_none();
	ELockAcquire acq1 = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->resource.device);

	if(buffer->isPendingFullCopy)		//Already has a full pending change, so no need to check anything.
		goto clean;

	if(!(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked) && !(buffer->isFirstFrame && !offset && !count))
		gotoIfError(clean, Error_invalidOperation(
			2, "DeviceBufferRef_markDirty() can only be called on first frame for entire resource or if it's CPU backed"
		))

	if(!count)
		count = bufLen - offset;

	Bool fullRange = count == bufLen;

	U64 start = (offset + 255) &~ 255;
	U64 end = U64_min((offset + count + 255) &~ 255, bufLen);

	//If the entire buffer is marked dirty, we have to make sure we don't duplicate it

	Bool shouldPush = false;

	if(fullRange) {
		gotoIfError(clean, ListDevicePendingRange_clear(&buffer->pendingChanges))
		buffer->isPendingFullCopy = true;
		shouldPush = true;
	}

	//Otherwise we have to merge current pending ranges

	else {

		//Merge with pending changes
		//256 bytes on either side to avoid lots of fragmented copies

		if (buffer->isPending) {

			U64 lastMatch = U64_MAX;

			for (U64 i = buffer->pendingChanges.length - 1; i != U64_MAX; --i) {

				DevicePendingRange *pending = &buffer->pendingChanges.ptrNonConst[i];

				//If intersects, we either merge with first occurence or pop last occurence and merge range with current

				if (end >= pending->buffer.startRange && start <= pending->buffer.endRange) {

					if (lastMatch == U64_MAX) {
						pending->buffer.startRange = U64_min(pending->buffer.startRange, start);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, end);
					}

					else {
						DevicePendingRange last = buffer->pendingChanges.ptr[lastMatch];
						pending->buffer.startRange = U64_min(pending->buffer.startRange, last.buffer.startRange);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, last.buffer.endRange);
						gotoIfError(clean, ListDevicePendingRange_erase(&buffer->pendingChanges, lastMatch))
					}

					lastMatch = i;
				}
			}

			shouldPush = lastMatch == U64_MAX;
		}

		else shouldPush = true;
	}

	if (shouldPush) {

		if((buffer->pendingChanges.length + 1) >> 32)
			gotoIfError(clean, Error_outOfBounds(
				0, U32_MAX, U32_MAX, "DeviceBufferRef_markDirty() buffer pendingRanges is limited to U32_MAX"
			))

		DevicePendingRange change = (DevicePendingRange) { .buffer = { .startRange = offset, .endRange = offset + count } };
		gotoIfError(clean, ListDevicePendingRange_pushBackx(&buffer->pendingChanges, change))
	}

	//Tell the device that on next submit it should handle copies from

	if(buffer->isPending)
		goto clean;

	buffer->isPending = true;

	acq1 = Lock_lock(&device->lock, U64_MAX);

	if(acq1 < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(0, "DeviceBufferRef_markDirty() couldn't lock device"))

	gotoIfError(clean, ListWeakRefPtr_pushBackx(&device->pendingResources, buf))

clean:

	if(acq1 == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	if(acq0 == ELockAcquire_Acquired)
		Lock_unlock(&buffer->lock);

	return err;
}

impl extern const U64 DeviceBufferExt_size;
impl Bool DeviceBuffer_freeExt(DeviceBuffer *buffer);
impl Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name);

Bool DeviceBuffer_free(DeviceBuffer *buffer, Allocator allocator) {

	(void)allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)buffer - sizeof(RefPtr));

	Lock_free(&buffer->lock);

	if (buffer->resource.flags & EGraphicsResourceFlag_ShaderRW) {

		GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->resource.device);
		const ELockAcquire acq = Lock_lock(&device->descriptorLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {

			const U32 allocations[2] = { buffer->readHandle, buffer->writeHandle };
			ListU32 allocationList = (ListU32) { 0 };
			ListU32_createRefConst(allocations, 2, &allocationList);
			GraphicsDeviceRef_freeDescriptors(buffer->resource.device, &allocationList);

			if(acq == ELockAcquire_Acquired)
				Lock_unlock(&device->descriptorLock);
		}
	}

	Bool success = DeviceBuffer_freeExt(buffer);
	success &= GraphicsResource_free(&buffer->resource, refPtr);
	success &= Buffer_freex(&buffer->cpuData);
	success &= ListDevicePendingRange_freex(&buffer->pendingChanges);
	return success;
}

Error GraphicsDeviceRef_createBufferIntern(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
	CharString name,
	U64 len,
	Bool allocate,
	DeviceBufferRef **ref
) {
	Error err = RefPtr_createx(
		(U32)(sizeof(DeviceBuffer) + DeviceBufferExt_size),
		(ObjectFreeFunc) DeviceBuffer_free,
		(ETypeId) EGraphicsTypeId_DeviceBuffer,
		ref
	);

	if(err.genericError)
		return err;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!dev || dev->typeId != EGraphicsTypeId_GraphicsDevice)
		gotoIfError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createBufferIntern()::dev is required"))

	if((usage & EDeviceBufferUsage_ScratchExt) && (usage != EDeviceBufferUsage_ScratchExt))
		gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_createBufferIntern() invalid scratch usage/flags"))

	if((usage & EDeviceBufferUsage_ASExt) && (resourceFlags || usage != EDeviceBufferUsage_ASExt))
		gotoIfError(clean, Error_invalidState(1, "GraphicsDeviceRef_createBufferIntern() invalid AS usage/flags"))

	if((usage & EDeviceBufferUsage_SBTExt) && (resourceFlags || usage != EDeviceBufferUsage_SBTExt))
		gotoIfError(clean, Error_invalidState(1, "GraphicsDeviceRef_createBufferIntern() invalid SBT usage/flags"))

	Bool isRTBufferType = usage & (EDeviceBufferUsage_ASExt | EDeviceBufferUsage_ScratchExt | EDeviceBufferUsage_ASReadExt);

	if (isRTBufferType && !(device->info.capabilities.features & EGraphicsFeatures_Raytracing))
		gotoIfError(clean, Error_invalidState(
			2, "GraphicsDeviceRef_createBufferIntern() AS or scratch buffer only allowed if raytracing feature is present"
		))

	if(!(resourceFlags & EGraphicsResourceFlag_InternalWeakDeviceRef))
		gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	DeviceBuffer *buf = DeviceBufferRef_ptr(*ref);

	*buf = (DeviceBuffer) {
		.resource = (GraphicsResource) {
			.device = dev,
			.size = len,
			.flags = (U16) resourceFlags,
			.type = EResourceType_DeviceBuffer
		},
		.usage = usage,
		.isFirstFrame = true
	};

	//Allocate

	if(buf->resource.flags & EGraphicsResourceFlag_ShaderRW) {

		acq = Lock_lock(&device->descriptorLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			gotoIfError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_createBufferIntern() couldn't acquire descriptor lock"
			))

		//Create images

		if(buf->resource.flags & EGraphicsResourceFlag_ShaderRead) {

			buf->readHandle = GraphicsDeviceRef_allocateDescriptor(dev, EDescriptorType_Buffer);

			if(buf->readHandle == U32_MAX)
				gotoIfError(clean, Error_outOfMemory(
					0, "GraphicsDeviceRef_createBufferIntern() couldn't allocate buffer descriptor"
				))
		}

		if(buf->resource.flags & EGraphicsResourceFlag_ShaderWrite) {

			buf->writeHandle = GraphicsDeviceRef_allocateDescriptor(dev, EDescriptorType_RWBuffer);

			if(buf->writeHandle == U32_MAX)
				gotoIfError(clean, Error_outOfMemory(
					0, "GraphicsDeviceRef_createBufferIntern() couldn't allocate buffer descriptor"
				))
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&device->descriptorLock);

		acq = ELockAcquire_Invalid;
	}

	gotoIfError(clean, ListDevicePendingRange_reservex(&buf->pendingChanges, usage & EGraphicsResourceFlag_CPUBacked ? 16 : 1))

	buf->lock = Lock_create();

	if(allocate) {
		gotoIfError(clean, Buffer_createEmptyBytesx(buf->resource.size, &buf->cpuData))		//Temporary if not CPUBacked
		gotoIfError(clean, DeviceBufferRef_markDirty(*ref, 0, 0))
	}

	gotoIfError(clean, GraphicsDeviceRef_createBufferExt(dev, buf, name))

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->descriptorLock);

	if(err.genericError)
		DeviceBufferRef_dec(ref);

	return err;
}

Error GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
	CharString name,
	U64 len,
	DeviceBufferRef **buf
) {
	return GraphicsDeviceRef_createBufferIntern(
		dev, usage, resourceFlags, name, len, resourceFlags & EGraphicsResourceFlag_CPUBacked, buf
	);
}

Error GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag flags,
	CharString name,
	Buffer *dat,
	DeviceBufferRef **buf
) {

	if(!dat)
		return Error_nullPointer(4, "GraphicsDeviceRef_createBufferData()::dat is required");

	Error err = GraphicsDeviceRef_createBufferIntern(
		dev, usage, flags, name, Buffer_length(*dat), Buffer_isRef(*dat), buf
	);

	if(err.genericError)
		return err;

	DeviceBuffer *buffer = DeviceBufferRef_ptr(*buf);

	if(Buffer_isRef(*dat))
		Buffer_copy(buffer->cpuData, *dat);

	else {							//Move
		buffer->cpuData = *dat;
		*dat = Buffer_createNull();
	}

	return Error_none();
}
