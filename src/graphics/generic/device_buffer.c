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

//graphics/generic/device_buffer.c

#include "types/container/list_impl.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/device.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/descriptor_table.h"
#include "types/container/ref_ptr.h"
#include "formats/oiSH/sh_registers.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

TListImpl(DevicePendingRange);

Bool DeviceBufferRef_markDirty(DeviceBufferRef *buf, U64 offset, U64 count, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = buf ? GraphicsDeviceRef_getAlloc(DeviceBufferRef_ptr(buf)->resource.device) : NULL;

	DeviceBuffer *buffer = NULL;
	GraphicsDevice *device = NULL;
	ELockAcquire acq0 = ELockAcquire_Invalid;
	ELockAcquire acq1 = ELockAcquire_Invalid;

	if(!buf || buf->refPtrType->typeId != (ETypeId) EGraphicsTypeId_DeviceBuffer)
		retError(clean, Error_nullPointer(0, "DeviceBufferRef_markDirty()::buf is required"));

	buffer = DeviceBufferRef_ptr(buf);
	U64 bufLen = buffer->resource.size;

	//Check range

	if(offset >= bufLen || offset + count > bufLen)
		retError(clean, Error_outOfBounds(
			1, offset + count, bufLen, "DeviceBufferRef_markDirty()::offset+count out of bounds"
		));

	acq0 = SpinLock_lock(&buffer->lock, U64_MAX);

	if(acq0 < ELockAcquire_Success)
		retError(clean, Error_invalidOperation(1, "DeviceBufferRef_markDirty() couldn't acquire buffer lock"));

	device = GraphicsDeviceRef_ptr(buffer->resource.device);

	if(buffer->isPendingFullCopy)        //Already has a full pending change, so no need to check anything.
		goto clean;

	//The first-frame exception only permits the internal initial upload of a not-CPU-backed buffer.
	//That upload always has staged data in cpuData, so require it here.
	//Otherwise a freshly-created, not-backed buffer with no data would be wrongly accepted.

	if(
		!(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked) &&
		!(buffer->isFirstFrame && !offset && !count && Buffer_length(buffer->cpuData))
	)
		retError(clean, Error_invalidOperation(
			2,
			"DeviceBufferRef_markDirty() can only be called on first frame for the entire resource "
			"with staged CPU data, or if it's CPU backed"
		));

	if(!count)
		count = bufLen - offset;

	U64 start = offset &~ 255;
	U64 end = U64_min((offset + count + 255) &~ 255, bufLen);

	Bool fullRange = start == 0 && end == bufLen;

	//If the entire buffer is marked dirty, we have to make sure we don't duplicate it

	Bool shouldPush = false;

	if(fullRange) {
		gotoIfError3(clean, ListDevicePendingRange_clear(&buffer->pendingChanges, e_rr));
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
						gotoIfError3(clean, ListDevicePendingRange_erase(&buffer->pendingChanges, lastMatch, e_rr));
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
			retError(clean, Error_outOfBounds(
				0, U32_MAX, U32_MAX, "DeviceBufferRef_markDirty() buffer pendingRanges is limited to U32_MAX"));

		DevicePendingRange change = (DevicePendingRange) { .buffer = { .startRange = offset, .endRange = offset + count } };
		gotoIfError3(clean, ListDevicePendingRange_pushBack(&buffer->pendingChanges, change, alloc, e_rr));
	}

	//Tell the device that on next submit it should handle copies from

	if(buffer->isPending)
		goto clean;

	buffer->isPending = true;

	acq1 = SpinLock_lock(&device->lock, U64_MAX);

	if(acq1 < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "DeviceBufferRef_markDirty() couldn't lock device"));

	gotoIfError3(clean, ListWeakRefPtr_pushBack(&device->pendingResources, buf, alloc, e_rr));

clean:

	if(acq1 == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(acq0 == ELockAcquire_Acquired)
		SpinLock_unlock(&buffer->lock);

	return s_uccess;
}

void DeviceBuffer_free(DeviceBuffer *buffer, const Allocator *alloc) {

	(void)alloc;

	RefPtr *refPtr = (RefPtr*)((const U8*)buffer - sizeof(RefPtr));

	SpinLock_lock(&buffer->lock, U64_MAX);

	GraphicsDeviceRef *device = buffer->resource.device;

	if(buffer->bindlessDescriptorTable) {
		GraphicsDeviceRef_freeDescriptorBindless(device, buffer->bindlessDescriptorTable, buffer->readHandle, NULL);
		GraphicsDeviceRef_freeDescriptorBindless(device, buffer->bindlessDescriptorTable, buffer->writeHandle, NULL);
		RefPtr_dec(&buffer->bindlessDescriptorTable);
	}

	DeviceBuffer_freeExt(buffer);
	GraphicsResource_free(&buffer->resource, refPtr);
	Buffer_free(&buffer->cpuData, alloc);
	ListDevicePendingRange_free(&buffer->pendingChanges, alloc);
}

Bool GraphicsDeviceRef_createBufferIntern(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	U64 len,
	Bool allocate,
	DeviceBufferRef **ref,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->buffer, ref, e_rr));
	allocated = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if(!dev || dev->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createBufferIntern()::dev is required"));

	if(bindlessDescriptorTable && !(resourceFlags & EGraphicsResourceFlag_ExposeBindless))
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createBufferIntern() bindlessDescriptorTable is set, but disallowed"
		));

	if(bindlessDescriptorTable && bindlessDescriptorTable->refPtrType->typeId != (ETypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(
			0, "GraphicsDeviceRef_createBufferIntern()::bindlessDescriptorTable should be valid if non NULL"
		));

	if ((resourceFlags & EGraphicsResourceFlag_ExposeBindless) && !bindlessDescriptorTable)
		bindlessDescriptorTable = GraphicsDeviceRef_ptr(dev)->defaultDescriptorTable;

	if(!bindlessDescriptorTable && (resourceFlags & EGraphicsResourceFlag_ExposeBindless))
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createBufferIntern() can't expose resource for bindless without bindless"
		));

	if((usage & EDeviceBufferUsage_ScratchExt) && (usage != EDeviceBufferUsage_ScratchExt))
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_createBufferIntern() invalid scratch usage/flags"));

	if((usage & EDeviceBufferUsage_ASExt) && (resourceFlags || usage != EDeviceBufferUsage_ASExt))
		retError(clean, Error_invalidState(1, "GraphicsDeviceRef_createBufferIntern() invalid AS usage/flags"));

	if((usage & EDeviceBufferUsage_SBTExt) && (resourceFlags || usage != EDeviceBufferUsage_SBTExt))
		retError(clean, Error_invalidState(1, "GraphicsDeviceRef_createBufferIntern() invalid SBT usage/flags"));

	Bool isRTBufferType = usage & (EDeviceBufferUsage_ASExt | EDeviceBufferUsage_ScratchExt | EDeviceBufferUsage_ASReadExt);

	if (isRTBufferType && !(device->info.capabilities.features & EGraphicsFeatures_Raytracing))
		retError(clean, Error_invalidState(
			2, "GraphicsDeviceRef_createBufferIntern() AS or scratch buffer only allowed if raytracing feature is present"
		));

	if(!(resourceFlags & EGraphicsResourceFlag_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(dev));

	if(len > device->info.capabilities.maxBufferSize)
		retError(clean, Error_invalidState(
			2, "GraphicsDeviceRef_createBufferIntern() buffer length exceeds maxBufferSize"
		));

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

	if(bindlessDescriptorTable) {
		gotoIfError3(clean, RefPtr_inc(bindlessDescriptorTable));
		buf->bindlessDescriptorTable = bindlessDescriptorTable;
	}

	gotoIfError3(clean, ListDevicePendingRange_reserve(
		&buf->pendingChanges, buf->resource.flags & EGraphicsResourceFlag_CPUBacked ? 16 : 1, alloc, e_rr
	));

	if(allocate) {
		//Temporary if not CPUBacked
		gotoIfError3(clean, Buffer_createEmptyBytes(buf->resource.size, alloc, &buf->cpuData, e_rr));
		gotoIfError3(clean, DeviceBufferRef_markDirty(*ref, 0, 0, e_rr));
	}

	gotoIfError3(clean, GraphicsDeviceRef_createBufferExt(dev, buf, name, e_rr));

	//Create descriptor
	//TODO: Allow creation as StructuredBuffer

	Descriptor bufferDesc = Descriptor_buffer(*ref, 0, 0, NULL, 0);

	if(
		(buf->resource.flags & EGraphicsResourceFlag_ExposeBindlessRead) &&
		!GraphicsDeviceRef_allocateDescriptorBindless(
			dev,
			bindlessDescriptorTable,
			ESHRegisterType_ByteAddressBuffer,
			0,
			false,
			&bufferDesc,
			&buf->readHandle,
			e_rr
		)
	)
		goto clean;

	if(
		(buf->resource.flags & EGraphicsResourceFlag_ExposeBindlessWrite) &&
		!GraphicsDeviceRef_allocateDescriptorBindless(
			dev,
			bindlessDescriptorTable,
			ESHRegisterType_ByteAddressBuffer | ESHRegisterType_IsWrite,
			0,
			false,
			&bufferDesc,
			&buf->writeHandle,
			e_rr
		)
	)
		goto clean;

clean:

	if(!s_uccess && allocated)
		RefPtr_dec(ref);

	return s_uccess;
}

Bool GraphicsDeviceRef_createBuffer(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag resourceFlags,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	U64 len,
	DeviceBufferRef **buf,
	Error *e_rr
) {
	return GraphicsDeviceRef_createBufferIntern(
		dev,
		usage, resourceFlags, bindlessDescriptorTable, name, len, resourceFlags & EGraphicsResourceFlag_CPUBacked,
		buf, e_rr
	);
}

Bool GraphicsDeviceRef_createBufferData(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag flags,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	Buffer *dat,
	DeviceBufferRef **buf,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!dat)
		retError(clean, Error_nullPointer(4, "GraphicsDeviceRef_createBufferData()::dat is required"));

	gotoIfError3(clean, GraphicsDeviceRef_createBufferIntern(
		dev, usage, flags, bindlessDescriptorTable, name, Buffer_length(*dat), Buffer_isRef(*dat), buf, e_rr
	));

	//Stage the CPU data into cpuData before markDirty.
	//markDirty now requires cpuData to be present for a not-CPU-backed first-frame upload.
	//For the ref path createBufferIntern already allocated cpuData (and ran its own markDirty).

	DeviceBuffer *buffer = DeviceBufferRef_ptr(*buf);

	if(Buffer_isRef(*dat))
		Buffer_memcpy(buffer->cpuData, *dat);

	else {                            //Move
		buffer->cpuData = *dat;
		*dat = Buffer_createNull();
	}

	if (!DeviceBufferRef_markDirty(*buf, 0, 0, e_rr)) {
		RefPtr_dec(buf);
		s_uccess = false;
		goto clean;
	}

clean:
	return s_uccess;
}
