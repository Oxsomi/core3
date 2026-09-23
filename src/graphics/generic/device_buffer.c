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

	if(!buf || buf->refPtrType->typeId != (TypeId) EGraphicsTypeId_DeviceBuffer)
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

	//Three ways a range can be legal.
	//CPUBacked keeps its host copy for the life of the resource, so any range of it can be re-read at any time.
	//A STREAM source is the same situation reached differently: the stream outlives the buffer, so a range can
	// be re-read from it whenever, which is what makes a partial update possible without a host copy.
	//Everything else has only the staged cpuData, which is freed once the first upload has consumed it, so the
	// exception is exactly that upload: the first frame, the whole resource, with something to read.

	//CPUAllocatedBit puts the resource in host visible memory that stays MAPPED, so the caller writes into it
	//directly and the bytes are already where the device reads them. There is nothing to copy from, and the
	// range only has to be recorded so an incoherent heap gets its flush.

	const Bool anyRange =
		(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked) ||
		(buffer->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit) ||
		buffer->cpuStream;

	const Bool firstFullUpload =
		buffer->isFirstFrame && !offset && !count && Buffer_length(buffer->cpuData);

	if(!anyRange && !firstFullUpload)
		retError(clean, Error_invalidOperation(
			2,
			"DeviceBufferRef_markDirty() can only be called on first frame for the entire resource "
			"with staged CPU data, or if it's CPU backed or stream backed"
		));

	if(!count)
		count = bufLen - offset;

	//256 bytes on either side keeps a run of small marks from becoming a run of tiny copies. A stream source
	//may need MORE than that: one that decrypts or decompresses per chunk cannot serve half a chunk, so a range
	// that ends inside one would decode it anyway and then throw the tail away.

	U64 block = 256;

	if(buffer->cpuStream) {

		const U32 streamBlock = RefPtr_data(buffer->cpuStream, OxStream)->blockSize;

		if(streamBlock > block)
			block = streamBlock;
	}

	//Divided rather than masked: a stream's block is whatever that stream's chunk is and nothing promises it is
	// a power of two, where the 256 default happens to be one.

	U64 start = offset / block * block;
	U64 end = U64_min((offset + count + block - 1) / block * block, bufLen);

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

void DeviceBuffer_free(void *bufferGeneric, const Allocator *alloc) {

	DeviceBuffer *buffer = (DeviceBuffer*) bufferGeneric;

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
	RefPtr_dec(&buffer->cpuStream);
	ListDevicePendingRange_free(&buffer->pendingChanges, alloc);
}

//Three things all have to hold. There has to be a queue, which only a submit that was handed one provides.
//The stream has to DECLARE concurrent reads: the interface allows a safe implementation but does not require
// one, and a stream that seeks a shared handle would tear. And the work has to be worth the split, which is
// bytes times cost: a memcpy of a megabyte is not, a block cipher over the same megabyte is.
//
//Pieces land on the stream's block, since a source that decodes per chunk cannot be cut inside one.

#define DeviceBuffer_fanOutCost (1 * MIBI)

U8 DeviceBuffer_readPieces(const DeviceBuffer *buffer, const OxStream *stream, U64 length) {

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->resource.device);

	if(!device->uploadJobQueue || !(stream->streamType & EStreamType_ConcurrentRead))
		return 1;

	const U64 cost = length * (stream->readCost ? stream->readCost : 1);

	if(cost < DeviceBuffer_fanOutCost)
		return 1;

	const U64 block = stream->blockSize ? stream->blockSize : 1;
	const U64 blocks = (length + block - 1) / block;

	U64 pieces = cost / DeviceBuffer_fanOutCost;

	if(pieces > blocks)
		pieces = blocks;

	if(pieces > JobQueue_threadCount(device->uploadJobQueue))
		pieces = JobQueue_threadCount(device->uploadJobQueue);

	return pieces > 255 ? 255 : (U8) (pieces < 1 ? 1 : pieces);
}

typedef struct DeviceBufferReadJob {
	OxStream *stream;
	const Allocator *alloc;
	U64 offset, length;
	Buffer dst;
	JobGroup *group;
	Bool ok;
	U8 padding[7];
} DeviceBufferReadJob;

static Bool DeviceBuffer_readJob(void *data, U64 threadId, JobQueue *queue) {

	(void) threadId; (void) queue;

	DeviceBufferReadJob *job = (DeviceBufferReadJob*) data;
	Error err = Error_none();

	const Bool ok = job->stream->read(job->stream, job->offset, job->length, job->dst, job->alloc, &err);
	job->ok = ok;

	//Releasing the token is the last thing done with job: the fan-out returns the moment the latch drops,
	//and its jobs live on that stack frame.

	(void) JobGroup_leave(job->group, NULL);
	return ok;
}

//The pieces are block aligned and disjoint, so no two of them touch the same chunk of the source or the same
//bytes of the destination. The LAST one takes the remainder, which is what keeps the split exact.
//Every piece is waited for before this returns, so a failure in one is reported rather than outliving the call.

static Bool DeviceBuffer_readFanOut(
	const DeviceBuffer *buffer, OxStream *stream, U64 offset, U64 length, Buffer dst, U8 pieces,
	const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	Bool waited = false;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->resource.device);
	DeviceBufferReadJob jobs[255];
	JobGroup group = (JobGroup) { 0 };

	const U64 block = stream->blockSize ? stream->blockSize : 1;
	const U64 blocks = (length + block - 1) / block;
	const U64 perPiece = (blocks + pieces - 1) / pieces * block;

	U8 count = 0, entered = 0, pushed = 0;

	while(count < pieces && (U64) count * perPiece < length)
		++count;

	//The queue is the caller's, and this runs with the device locked, so the wait is scoped to a group.
	//Waiting on the QUEUE would run whatever else the caller put on it, under that lock, and a job of
	// theirs that touches the device would then deadlock against the thread waiting for it.
	//Scoped to a group this thread only ever runs its own reads, so it still drains them and unlocks even
	// when every worker is stuck behind the device.

	gotoIfError3(clean, JobGroup_create(&group, device->uploadJobQueue, NULL, NULL, NULL, e_rr));
	gotoIfError3(clean, JobGroup_enter(&group, count, e_rr));
	entered = count;

	for(U8 i = 0; i < count; ++i) {

		const U64 at = (U64) i * perPiece;
		const U64 len = U64_min(perPiece, length - at);

		jobs[i] = (DeviceBufferReadJob) {
			.stream = stream,
			.alloc = alloc,
			.offset = offset + at,
			.length = len,
			.dst = Buffer_createRef(dst.ptrNonConst + at, len),
			.group = &group
		};

		gotoIfError3(clean, JobQueue_pushGroup(
			device->uploadJobQueue, DeviceBuffer_readJob, jobs + i, NULL, &group, e_rr
		));

		++pushed;
	}

	gotoIfError3(clean, JobGroup_wait(&group, e_rr));
	waited = true;

	for(U8 i = 0; i < pushed; ++i)
		if(!jobs[i].ok)
			retError(clean, Error_invalidState(0, "DeviceBuffer_readUploadSource() a fanned out source read failed"));

clean:

	//A token taken for a job that a failed push never queued has nothing left to release it.

	for(U8 i = pushed; i < entered; ++i)
		(void) JobGroup_leave(&group, NULL);

	//A push that failed leaves earlier jobs running, and they write into dst, so they are waited for either way.

	if(!waited && pushed)
		(void) JobGroup_wait(&group, NULL);

	return s_uccess;
}

//Keeping a source is a property of the SOURCE and not of the resource, which is why keepSource is asked for at
//create rather than read off a resource flag: CPUAllocatedBit means the memory is host visible and mapped, so
// such a buffer needs the stream LESS, not more, and the device local one is what still needs a source to
// update through.

void DeviceBuffer_releaseUploadSource(DeviceBuffer *buffer, const Allocator *alloc) {

	if(!buffer)
		return;

	if(!(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_free(&buffer->cpuData, alloc);

	if(!buffer->keepStream)
		RefPtr_dec(&buffer->cpuStream);
}

//The stream read is POSITIONAL, so a range reaches the destination without the bytes before it being read.
//A stream reporting fewer bytes than asked for is an error rather than a short upload: the caller committed
// the range to a command list before this ran.

Bool DeviceBuffer_readUploadSource(
	DeviceBuffer *buffer, U64 offset, U64 length, Buffer dst, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	if(!buffer || !length)
		retError(clean, Error_nullPointer(0, "DeviceBuffer_readUploadSource()::buffer and length are required"));

	if(Buffer_length(dst) < length)
		retError(clean, Error_outOfBounds(
			3, length, Buffer_length(dst), "DeviceBuffer_readUploadSource()::dst is smaller than length"
		));

	//No source at all is legal in exactly ONE situation: a CPUAllocatedBit resource lives in host visible
	//memory that stays mapped, so the caller wrote straight into it and the bytes are already at the
	// destination. Copying them onto themselves is the only thing to avoid there.
	//
	//Anywhere ELSE, no source means a source went missing between the range being marked and the flush
	//reading it, and skipping quietly leaves the resource holding whatever it was allocated with. That is a
	// silent wrong upload, which is worse than any failure: it reads as data.

	if(!buffer->cpuStream && !Buffer_length(buffer->cpuData)) {

		if(buffer->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit)
			goto clean;

		retError(clean, Error_invalidState(
			0, "DeviceBuffer_readUploadSource() a pending range has no source left to read from"
		));
	}

	if(buffer->cpuStream) {

		OxStream *stream = RefPtr_data(buffer->cpuStream, OxStream);

		if(!stream->read)
			retError(clean, Error_unsupportedOperation(
				0, "DeviceBuffer_readUploadSource()::cpuStream is not readable"
			));

		const U8 pieces = DeviceBuffer_readPieces(buffer, stream, length);

		if(pieces < 2) {
			gotoIfError3(clean, stream->read(stream, offset, length, dst, alloc, e_rr));
			goto clean;
		}

		gotoIfError3(clean, DeviceBuffer_readFanOut(buffer, stream, offset, length, dst, pieces, alloc, e_rr));
		goto clean;
	}

	if(Buffer_length(buffer->cpuData) < offset + length)
		retError(clean, Error_outOfBounds(
			1, offset + length, Buffer_length(buffer->cpuData), "DeviceBuffer_readUploadSource()::range past cpuData"
		));

	Buffer_memcpy(dst, Buffer_createRefConst(buffer->cpuData.ptr + offset, length));

clean:
	return s_uccess;
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

	//Checked first, since getTypes on a NULL device yields a non NULL member pointer that faults in RefPtr_create.

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createBufferIntern()::dev is required"));

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if(bindlessDescriptorTable && !(resourceFlags & EGraphicsResourceFlag_ExposeBindless))
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createBufferIntern() bindlessDescriptorTable is set, but disallowed"
		));

	if(bindlessDescriptorTable && bindlessDescriptorTable->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
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

	//Refused rather than silently dropped: a predicate buffer on a device that cannot predicate would
	// otherwise carry a usage bit no backend honors, and the surprise surfaces later at startScope.

	if((usage & EDeviceBufferUsage_Predicate) && !(device->info.capabilities.features2 & EGraphicsFeatures2_Predication))
		retError(clean, Error_invalidState(
			3, "GraphicsDeviceRef_createBufferIntern() predicate buffer requires the Predication feature"
		));

	if(!(resourceFlags & EGraphicsResourceFlag_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(dev));

	if(len > device->info.capabilities.maxBufferSize)
		retError(clean, Error_invalidState(
			2, "GraphicsDeviceRef_createBufferIntern() buffer length exceeds maxBufferSize"
		));

	//Allocated only now that every argument has been accepted.
	//Creating the ref first meant a rejected argument freed a buffer whose resource.device was still NULL,
	// and DeviceBuffer_freeExt dereferences exactly that to find the backend, so every rejection segfaulted.

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->buffer, ref, e_rr));
	allocated = true;

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

	//A descriptor that could not be allocated leaves its handle at zero, and zero is the handle a shader reads
	//an UNRELATED resource through, so a create that shrugged this off would hand back a buffer that binds and
	// dispatches and reads someone else's bytes. It fails instead, carrying the reason the allocation gave.

	if(buf->resource.flags & EGraphicsResourceFlag_ExposeBindlessRead)
		gotoIfError3(clean, GraphicsDeviceRef_allocateDescriptorBindless(
			dev,
			bindlessDescriptorTable,
			EGfxRegisterType_ByteAddressBuffer,
			0,
			false,
			&bufferDesc,
			&buf->readHandle,
			e_rr
		));

	if(buf->resource.flags & EGraphicsResourceFlag_ExposeBindlessWrite)
		gotoIfError3(clean, GraphicsDeviceRef_allocateDescriptorBindless(
			dev,
			bindlessDescriptorTable,
			EGfxRegisterType_ByteAddressBuffer | EGfxRegisterType_IsWrite,
			0,
			false,
			&bufferDesc,
			&buf->writeHandle,
			e_rr
		));

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

Bool GraphicsDeviceRef_createBufferStream(
	GraphicsDeviceRef *dev,
	EDeviceBufferUsage usage,
	EGraphicsResourceFlag flags,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	StreamRef *stream,
	U64 size,
	Bool keepSource,
	DeviceBufferRef **buf,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool created = false;

	if(!stream || !buf)
		retError(clean, Error_nullPointer(5, "GraphicsDeviceRef_createBufferStream()::stream and buf are required"));

	if(!RefPtr_data(stream, OxStream)->read)
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDeviceRef_createBufferStream()::stream is not readable"
		));

	if(flags & EGraphicsResourceFlag_CPUBacked)
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createBufferStream() a stream source and CPUBacked are exclusive"
		));

	if(!size)
		retError(clean, Error_invalidParameter(6, 0, "GraphicsDeviceRef_createBufferStream()::size is required"));

	gotoIfError3(clean, GraphicsDeviceRef_createBufferIntern(
		dev, usage, flags, bindlessDescriptorTable, name, size, false, buf, e_rr
	));

	created = true;

	DeviceBuffer *buffer = DeviceBufferRef_ptr(*buf);

	gotoIfError3(clean, RefPtr_inc(stream));
	buffer->cpuStream = stream;
	buffer->keepStream = keepSource;

	gotoIfError3(clean, DeviceBufferRef_markDirty(*buf, 0, 0, e_rr));

clean:

	if(!s_uccess && created)
		RefPtr_dec(buf);

	return s_uccess;
}

Bool DeviceBufferRef_pullRegion(
	DeviceBufferRef *buf, U64 offset, U64 len, DevicePullCallback callback, void *context, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = NULL;

	GraphicsDevice *device = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;
	Bool owned = false;

	//Validated before the pointer is reinterpreted, since a wrong type would read a garbage device

	if(!buf || buf->refPtrType->typeId != (TypeId) EGraphicsTypeId_DeviceBuffer)
		retError(clean, Error_nullPointer(0, "DeviceBufferRef_pullRegion()::buf is required"));

	DeviceBuffer *buffer = DeviceBufferRef_ptr(buf);
	alloc = GraphicsDeviceRef_getAlloc(buffer->resource.device);
	const U64 bufLen = buffer->resource.size;

	//The result lands in cpuData, so there has to be one to land in

	if(!(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked))
		retError(clean, Error_invalidOperation(0, "DeviceBufferRef_pullRegion() requires a CPUBacked buffer"));

	if(offset >= bufLen)
		retError(clean, Error_outOfBounds(1, offset, bufLen, "DeviceBufferRef_pullRegion()::offset out of bounds"));

	if(!len)
		len = bufLen - offset;

	if(offset + len > bufLen)
		retError(clean, Error_outOfBounds(
			2, offset + len, bufLen, "DeviceBufferRef_pullRegion()::offset + len out of bounds"
		));

	device = GraphicsDeviceRef_ptr(buffer->resource.device);

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidOperation(1, "DeviceBufferRef_pullRegion() couldn't acquire device lock"));

	RefPtr_inc(buf);
	owned = true;

	const DevicePendingPull pull = (DevicePendingPull) {
		.resource = buf,
		.callback = callback,
		.context = context,
		.range = (DevicePendingRange) { .buffer = (BufferRange) { .startRange = offset, .endRange = offset + len } }
	};

	gotoIfError3(clean, ListDevicePendingPull_pushBack(&device->pendingPulls, pull, alloc, e_rr));
	owned = false;

clean:

	if(owned)
		RefPtr_dec(&buf);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess;
}
