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

#include "graphics/generic/buffer.h"
#include "graphics/generic/device.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "types/buffer.h"
#include "types/error.h"
#include "types/string.h"

Error GPUBufferRef_dec(GPUBufferRef **buffer) {
	return !RefPtr_dec(buffer) ? Error_invalidOperation(0) : Error_none();
}

Error GPUBufferRef_add(GPUBufferRef *buffer) {
	return buffer ? (!RefPtr_inc(buffer) ? Error_invalidOperation(0) : Error_none()) : Error_nullPointer(0);
}

Error GPUBufferRef_markDirty(GPUBufferRef *buf, U64 offset, U64 count) {

	if(!buf)
		return Error_nullPointer(0);

	GPUBuffer *buffer = GPUBufferRef_ptr(buf);

	if(!(buffer->usage & EGPUBufferUsage_CPUBacked) && !(buffer->isFirstFrame && !offset && !count))
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

				GPUPendingRange *pending = &((GPUPendingRange*)buffer->pendingChanges.ptr)[i];

				//If intersects, we either merge with first occurence or pop last occurence and merge range with current

				if (end >= pending->buffer.startRange && start <= pending->buffer.endRange) {

					if (lastMatch == U64_MAX) {
						pending->buffer.startRange = U64_min(pending->buffer.startRange, start);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, end);
					}

					else {

						GPUPendingRange last = ((GPUPendingRange*)buffer->pendingChanges.ptr)[lastMatch];

						pending->buffer.startRange = U64_min(pending->buffer.startRange, last.buffer.startRange);
						pending->buffer.endRange = U64_max(pending->buffer.endRange, last.buffer.endRange);

						Error err = List_popLocation(&buffer->pendingChanges, lastMatch, Buffer_createNull());

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

		//start/end are offset by 256 bytes to ensure there's less fragmenting.
		//Example:
		//vertexBuffer mark dirty uv 0 and 1. If stride < 256 (very likely) then this would mark up to 128 bytes.
		//Otherwise it'd have to do lots of small copies which is slow.
		//Recommended solution in that case would be making a uv only buffer and copying that with compute.
		//But this would 

		GPUPendingRange change = (GPUPendingRange) { .buffer = { .startRange = start, .endRange = end } };

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

impl extern const U64 GPUBufferExt_size;
impl Bool GPUBuffer_freeExt(GPUBuffer *buffer);
impl Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, GPUBuffer *buf, CharString name);

Bool GPUBuffer_free(GPUBuffer *buffer, Allocator allocator) {

	allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)buffer - sizeof(RefPtr));

	Bool success = GPUBuffer_freeExt(buffer);
	success &= GraphicsDeviceRef_removePending(buffer->device, refPtr);
	success &= !GraphicsDeviceRef_dec(&buffer->device).genericError;

	if(buffer->usage & EGPUBufferUsage_CPUBacked)
		success &= Buffer_freex(&buffer->cpuData);

	return success;
}

Error GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev, 
	EGPUBufferUsage usage, 
	CharString name, 
	U64 len, 
	GPUBufferRef **buf
) {

	if(!dev)
		return Error_nullPointer(0);

	Error err = RefPtr_createx(
		(U32)(sizeof(GPUBuffer) + GPUBufferExt_size), 
		(ObjectFreeFunc) GPUBuffer_free, 
		EGraphicsTypeId_GPUBuffer, 
		buf
	);

	if(err.genericError)
		return err;

	//Init GPUBuffer

	GPUBuffer *buffer = GPUBufferRef_ptr(*buf);

	_gotoIfError(clean, GraphicsDeviceRef_add(dev));

	*buffer = (GPUBuffer) {
		.device = dev,
		.usage = usage,
		.length = len,
		.pendingChanges = List_createEmpty(sizeof(GPUPendingRange)),
		.isFirstFrame = true
	};

	_gotoIfError(clean, List_reservex(&buffer->pendingChanges, usage & EGPUBufferUsage_CPUBacked ? 16 : 1));

	if(usage & EGPUBufferUsage_CPUBacked)
		_gotoIfError(clean, Buffer_createEmptyBytesx(buffer->length, &buffer->cpuData));

	_gotoIfError(clean, GraphicsDeviceRef_createBufferExt(dev, buffer, name));

clean:

	if(err.genericError)
		RefPtr_dec(buf);

	return err;
}

Error GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev, 
	EGPUBufferUsage usage, 
	CharString name, 
	Buffer *dat, 
	GPUBufferRef **buf
) {

	if(!dat)
		return Error_nullPointer(2);

	Error err = GraphicsDeviceRef_createBuffer(dev, usage, name, Buffer_length(*dat), buf);

	if(err.genericError)
		return err;

	GPUBuffer *buffer = GPUBufferRef_ptr(*buf);

	if(usage & EGPUBufferUsage_CPUBacked)
		Buffer_copy(buffer->cpuData, *dat);
	
	else buffer->cpuData = *dat;

	if((err = GPUBufferRef_markDirty(*buf, 0, 0)).genericError) {
		GPUBufferRef_dec(buf);
		return err;
	}

	*dat = Buffer_createNull();
	return Error_none();
}
