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
#include "platforms/ext/bufferx.h"
#include "types/string.h"

TListImpl(DevicePendingRange);

Error DeviceBufferRef_dec(DeviceBufferRef **buffer) {
	return !RefPtr_dec(buffer) ? Error_invalidOperation(0, "DeviceBufferRef_dec()::buffer is required") : Error_none();
}

Error DeviceBufferRef_inc(DeviceBufferRef *buffer) {
	return !RefPtr_inc(buffer) ? Error_invalidOperation(0, "DeviceBufferRef_inc()::buffer is required") : Error_none();
}

Error DeviceBufferRef_markDirty(DeviceBufferRef *buf, U64 offset, U64 count) {

	if(!buf || buf->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_nullPointer(0, "DeviceBufferRef_markDirty()::buf is required");

	DeviceBuffer *buffer = DeviceBufferRef_ptr(buf);

	//Check range

	if(offset >= buffer->length || offset + count > buffer->length)
		return Error_outOfBounds(1, offset + count, buffer->length, "DeviceBufferRef_markDirty()::offset+count out of bounds");

	ELockAcquire acq0 = Lock_lock(&buffer->lock, U64_MAX);

	if(acq0 < ELockAcquire_Success)
		return Error_invalidOperation(1, "DeviceBufferRef_markDirty() couldn't acquire buffer lock");

	Error err = Error_none();
	ELockAcquire acq1 = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->device);

	if(buffer->isPendingFullCopy)		//Already has a full pending change, so no need to check anything.
		goto clean;

	if(!(buffer->usage & EDeviceBufferUsage_CPUBacked) && !(buffer->isFirstFrame && !offset && !count))
		_gotoIfError(clean, Error_invalidOperation(
			2, "DeviceBufferRef_markDirty() can only be called on first frame for entire resource or if it's CPU backed"
		));

	if(!count)
		count = buffer->length - offset;

	Bool fullRange = count == buffer->length;

	U64 start = (offset + 255) &~ 255;
	U64 end = U64_min((offset + count + 255) &~ 255, buffer->length);

	//If the entire buffer is marked dirty, we have to make sure we don't duplicate it

	Bool shouldPush = false;

	if(fullRange) {
		_gotoIfError(clean, ListDevicePendingRange_clear(&buffer->pendingChanges));
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
						pending->buffer.startRange = U64_min(pending->buffer.startRange, offset);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, offset + count);
					}

					else {
						DevicePendingRange last = buffer->pendingChanges.ptr[lastMatch];
						pending->buffer.startRange = U64_min(pending->buffer.startRange, last.buffer.startRange);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, last.buffer.endRange);
						_gotoIfError(clean, ListDevicePendingRange_erase(&buffer->pendingChanges, lastMatch));
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
			_gotoIfError(clean, Error_outOfBounds(
				0, U32_MAX, U32_MAX, "DeviceBufferRef_markDirty() buffer pendingRanges is limited to U32_MAX"
			));

		DevicePendingRange change = (DevicePendingRange) { .buffer = { .startRange = offset, .endRange = offset + count } };
		_gotoIfError(clean, ListDevicePendingRange_pushBackx(&buffer->pendingChanges, change));
	}

	//Tell the device that on next submit it should handle copies from

	if(buffer->isPending)
		goto clean;

	buffer->isPending = true;

	acq1 = Lock_lock(&device->lock, U64_MAX);

	if(acq1 < ELockAcquire_Success)
		_gotoIfError(clean, Error_invalidState(0, "DeviceBufferRef_markDirty() couldn't lock device"));

	_gotoIfError(clean, ListWeakRefPtr_pushBackx(&device->pendingResources, buf));

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

	allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)buffer - sizeof(RefPtr));

	Lock_free(&buffer->lock);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->device);
	DeviceMemoryAllocator_freeAllocation(&device->allocator, buffer->blockId, buffer->blockOffset);

	Bool success = DeviceBuffer_freeExt(buffer);
	success &= GraphicsDeviceRef_removePending(buffer->device, refPtr);

	if(!(buffer->usage & EDeviceBufferUsage_InternalWeakRef))
		success &= !GraphicsDeviceRef_dec(&buffer->device).genericError;

	success &= Buffer_freex(&buffer->cpuData);
	success &= ListDevicePendingRange_freex(&buffer->pendingChanges);

	return success;
}

Error GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	CharString name,
	U64 len,
	DeviceBufferRef **buf
) {

	if(!dev || dev->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createBuffer()::dev is required");

	Error err = RefPtr_createx(
		(U32)(sizeof(DeviceBuffer) + DeviceBufferExt_size),
		(ObjectFreeFunc) DeviceBuffer_free,
		EGraphicsTypeId_DeviceBuffer,
		buf
	);

	if(err.genericError)
		return err;

	ELockAcquire acq = ELockAcquire_Invalid;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	//Init DeviceBuffer

	DeviceBuffer *buffer = DeviceBufferRef_ptr(*buf);

	if(!(usage & EDeviceBufferUsage_InternalWeakRef))
		_gotoIfError(clean, GraphicsDeviceRef_inc(dev));

	*buffer = (DeviceBuffer) {
		.device = dev,
		.usage = usage,
		.length = len,
		.isFirstFrame = true,
		.readHandle = U32_MAX,
		.writeHandle = U32_MAX
	};

	_gotoIfError(clean, ListDevicePendingRange_reservex(
		&buffer->pendingChanges, usage & EDeviceBufferUsage_CPUBacked ? 16 : 1
	));

	if(usage & EDeviceBufferUsage_CPUBacked)
		_gotoIfError(clean, Buffer_createEmptyBytesx(buffer->length, &buffer->cpuData));

	acq = Lock_lock(&device->allocator.lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		_gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_createBuffer() couldn't acquire device allocator"));

	_gotoIfError(clean, GraphicsDeviceRef_createBufferExt(dev, buffer, name));
	buffer->lock = Lock_create();

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->allocator.lock);

	if(err.genericError)
		DeviceBufferRef_dec(buf);

	return err;
}

Error GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	CharString name,
	Buffer *dat,
	DeviceBufferRef **buf
) {

	if(!dat)
		return Error_nullPointer(2, "GraphicsDeviceRef_createBufferData()::dat is required");

	//Ensure our buffer is still active when submitCommands is called by making a copy

	Buffer copy = Buffer_createNull();

	if (Buffer_isRef(*dat) && Buffer_length(*dat)) {

		Error err = Buffer_createCopyx(*dat, &copy);

		if (err.genericError)
			return err;

		*dat = copy;
	}

	Error err = GraphicsDeviceRef_createBuffer(dev, usage, name, Buffer_length(*dat), buf);

	if (err.genericError) {
		Buffer_freex(&copy);
		return err;
	}

	DeviceBuffer *buffer = DeviceBufferRef_ptr(*buf);

	if(usage & EDeviceBufferUsage_CPUBacked)
		Buffer_copy(buffer->cpuData, *dat);
	
	else buffer->cpuData = *dat;

	if((err = DeviceBufferRef_markDirty(*buf, 0, 0)).genericError) {
		DeviceBufferRef_dec(buf);
		*dat = Buffer_createNull();
		return err;
	}

	*dat = Buffer_createNull();
	return Error_none();
}
