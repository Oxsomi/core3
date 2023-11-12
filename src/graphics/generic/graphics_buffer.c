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

#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "types/buffer.h"
#include "types/error.h"
#include "types/string.h"

Error DeviceBufferRef_dec(DeviceBufferRef **buffer) {
	return !RefPtr_dec(buffer) ? Error_invalidOperation(0) : Error_none();
}

Error DeviceBufferRef_inc(DeviceBufferRef *buffer) {
	return !RefPtr_inc(buffer) ? Error_invalidOperation(0) : Error_none();
}

Error DeviceBufferRef_markDirty(DeviceBufferRef *buf, U64 offset, U64 count) {

	if(!buf)
		return Error_nullPointer(0);

	DeviceBuffer *buffer = DeviceBufferRef_ptr(buf);

	if(!(buffer->usage & EDeviceBufferUsage_CPUBacked) && !(buffer->isFirstFrame && !offset && !count))
		return Error_invalidOperation(0);

	//Check range

	if(offset >= buffer->length || offset + count > buffer->length)
		return Error_outOfBounds(1, offset + count, buffer->length);

	if(buffer->isPendingFullCopy)		//Already has a full pending change, so no need to check anything.
		return Error_none();

	if(!count)
		count = buffer->length - offset;

	Bool fullRange = count == buffer->length;

	U64 start = U64_max(offset, 256) - 256;
	U64 end = U64_min(offset + count + 256, buffer->length);

	//If the entire buffer is marked dirty, we have to make sure we don't duplicate it

	Bool shouldPush = false;

	if(fullRange) {

		Error err = List_clear(&buffer->pendingChanges);

		if(err.genericError)
			return err;

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

				DevicePendingRange *pending = &((DevicePendingRange*)buffer->pendingChanges.ptr)[i];

				//If intersects, we either merge with first occurence or pop last occurence and merge range with current

				if (end >= pending->buffer.startRange && start <= pending->buffer.endRange) {

					if (lastMatch == U64_MAX) {
						pending->buffer.startRange = U64_min(pending->buffer.startRange, offset);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, offset + count);
					}

					else {

						DevicePendingRange last = ((DevicePendingRange*)buffer->pendingChanges.ptr)[lastMatch];

						pending->buffer.startRange = U64_min(pending->buffer.startRange, last.buffer.startRange);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, last.buffer.endRange);

						Error err = List_erase(&buffer->pendingChanges, lastMatch);

						if(err.genericError)
							return err;
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
			return Error_outOfBounds(0, U32_MAX, U32_MAX);

		DevicePendingRange change = (DevicePendingRange) { .buffer = { .startRange = offset, .endRange = offset + count } };

		Error err = List_pushBackx(&buffer->pendingChanges, Buffer_createConstRef(&change, sizeof(change)));

		if(err.genericError)
			return err;
	}

	//Tell the device that on next submit it should handle copies from 

	if(buffer->isPending)
		return Error_none();

	buffer->isPending = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->device);
	Error err = List_pushBackx(&device->pendingResources, Buffer_createConstRef(&buf, sizeof(buf)));

	if(err.genericError)
		List_clear(&buffer->pendingChanges);

	return err;
}

impl extern const U64 DeviceBufferExt_size;
impl Bool DeviceBuffer_freeExt(DeviceBuffer *buffer);
impl Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name);

Bool DeviceBuffer_free(DeviceBuffer *buffer, Allocator allocator) {

	allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)buffer - sizeof(RefPtr));

	Bool success = DeviceBuffer_freeExt(buffer);
	success &= GraphicsDeviceRef_removePending(buffer->device, refPtr);

	if(!(buffer->usage & EDeviceBufferUsage_InternalWeakRef))
		success &= !GraphicsDeviceRef_dec(&buffer->device).genericError;

	if(buffer->usage & EDeviceBufferUsage_CPUBacked)
		success &= Buffer_freex(&buffer->cpuData);

	success &= List_freex(&buffer->pendingChanges);

	return success;
}

Error GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev, 
	EDeviceBufferUsage usage, 
	CharString name, 
	U64 len, 
	DeviceBufferRef **buf
) {

	if(!dev)
		return Error_nullPointer(0);

	Error err = RefPtr_createx(
		(U32)(sizeof(DeviceBuffer) + DeviceBufferExt_size), 
		(ObjectFreeFunc) DeviceBuffer_free, 
		EGraphicsTypeId_DeviceBuffer, 
		buf
	);

	if(err.genericError)
		return err;

	//Init DeviceBuffer

	DeviceBuffer *buffer = DeviceBufferRef_ptr(*buf);

	if(!(usage & EDeviceBufferUsage_InternalWeakRef))
		_gotoIfError(clean, GraphicsDeviceRef_inc(dev));

	*buffer = (DeviceBuffer) {
		.device = dev,
		.usage = usage,
		.length = len,
		.pendingChanges = List_createEmpty(sizeof(DevicePendingRange)),
		.isFirstFrame = true
	};

	_gotoIfError(clean, List_reservex(&buffer->pendingChanges, usage & EDeviceBufferUsage_CPUBacked ? 16 : 1));

	if(usage & EDeviceBufferUsage_CPUBacked)
		_gotoIfError(clean, Buffer_createEmptyBytesx(buffer->length, &buffer->cpuData));

	_gotoIfError(clean, GraphicsDeviceRef_createBufferExt(dev, buffer, name));

clean:

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
		return Error_nullPointer(2);

	Error err = GraphicsDeviceRef_createBuffer(dev, usage, name, Buffer_length(*dat), buf);

	if(err.genericError)
		return err;

	DeviceBuffer *buffer = DeviceBufferRef_ptr(*buf);

	if(usage & EDeviceBufferUsage_CPUBacked)
		Buffer_copy(buffer->cpuData, *dat);
	
	else buffer->cpuData = *dat;

	if((err = DeviceBufferRef_markDirty(*buf, 0, 0)).genericError) {
		DeviceBufferRef_dec(buf);
		return err;
	}

	*dat = Buffer_createNull();
	return Error_none();
}
