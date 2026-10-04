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

//graphics/generic/pipeline_cache.c

#include "graphics/generic/device.h"
#include "graphics/generic/device_internal.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "platforms/logx.h"
#include "types/container/buffer.h"
#include "types/base/error.h"

//The blob GraphicsDeviceRef_getPipelineCache hands out (oiPC, docs/file/oiPC.md): this header, then sizeCount
// (key, bytes) pairs, then the driver's own cache data.

#define PipelineCacheHeader_MAGIC 0x4350696F            //oiPC
#define PipelineCacheHeader_VERSION 10                  //1.0

typedef struct PipelineCacheHeader {
	U32 magic;
	U8 version, flags;                  //No flags yet
	U8 api;                             //EGraphicsApi
	U8 padding;
	U32 sizeCount;
	U32 padding1;
	U64 driverBytes;
} PipelineCacheHeader;

U64 Pipeline_hash(U64 hash, const void *data, U64 length) {
	return Buffer_fnv1a64(Buffer_createRefConst(data, length), hash);
}

U64 Pipeline_hashString(U64 hash, const C8 *str) {
	const CharString s = CharString_createRefCStrConst(str);
	return Pipeline_hash(hash, s.ptr, CharString_length(s));
}

static U64 Pipeline_hashDescriptorLayout(U64 hash, DescriptorLayoutRef *ref) {

	const DescriptorLayout *layout = ref ? DescriptorLayoutRef_ptr(ref) : NULL;
	const U64 bindings = layout ? layout->info.bindings.length : U64_MAX;        //Absent differs from empty

	hash = Pipeline_hash(hash, &bindings, sizeof(bindings));

	if(!layout)
		return hash;

	hash = Pipeline_hash(hash, &layout->info.flags, sizeof(layout->info.flags));
	return Pipeline_hash(hash, layout->info.bindings.ptr, bindings * sizeof(layout->info.bindings.ptr[0]));
}

U64 Pipeline_hashLayout(U64 hash, PipelineLayoutRef *ref) {

	if(!ref)
		return Pipeline_hash(hash, &ref, sizeof(ref));

	const PipelineLayoutInfo *info = &PipelineLayoutRef_ptr(ref)->info;

	hash = Pipeline_hash(hash, &info->flags, sizeof(info->flags));
	hash = Pipeline_hash(hash, &info->pushConstants, sizeof(info->pushConstants));
	hash = Pipeline_hashDescriptorLayout(hash, info->bindings);
	return Pipeline_hashDescriptorLayout(hash, info->pushDescriptors);
}

//The pairs stay sorted by key, so both lookups are a binary search; returns the index of the first key >= key.

static U64 GraphicsDevice_pipelineSizeAt(const GraphicsDevice *device, U64 key) {

	U64 lo = 0, hi = device->pipelineSizes.length / 2;

	while(lo < hi) {

		const U64 mid = (lo + hi) / 2;

		if(device->pipelineSizes.ptr[mid * 2] < key)
			lo = mid + 1;

		else hi = mid;
	}

	return lo;
}

Bool GraphicsDevice_findPipelineSize(GraphicsDevice *device, U64 key, U64 *bytes) {

	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	const U64 i = GraphicsDevice_pipelineSizeAt(device, key);
	const Bool found = i * 2 < device->pipelineSizes.length && device->pipelineSizes.ptr[i * 2] == key;

	if(found)
		*bytes = device->pipelineSizes.ptr[i * 2 + 1];

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return found;
}

Bool GraphicsDevice_recordPipelineSize(GraphicsDevice *device, U64 key, U64 bytes, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDevice_getAlloc(device);
	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "GraphicsDevice_recordPipelineSize() couldn't lock the device"));

	const U64 i = GraphicsDevice_pipelineSizeAt(device, key);

	if(i * 2 < device->pipelineSizes.length && device->pipelineSizes.ptr[i * 2] == key) {
		device->pipelineSizes.ptrNonConst[i * 2 + 1] = bytes;
		goto clean;
	}

	gotoIfError3(clean, ListU64_insert(&device->pipelineSizes, i * 2, bytes, alloc, e_rr));
	gotoIfError3(clean, ListU64_insert(&device->pipelineSizes, i * 2, key, alloc, e_rr));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess;
}

Bool GraphicsDeviceRef_getPipelineCache(GraphicsDeviceRef *deviceRef, const Allocator *alloc, Buffer *data, Error *e_rr) {

	Bool s_uccess = true;
	Buffer driverData = Buffer_createNull();
	Bool writing = false;
	ELockAcquire acq = ELockAcquire_Invalid;
	GraphicsDevice *device = NULL;

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice || !data)
		retError(clean, Error_nullPointer(
			!data ? 2 : 0, "GraphicsDeviceRef_getPipelineCache()::deviceRef and data are required"
		));

	if(Buffer_length(*data))
		retError(clean, Error_invalidParameter(2, 0, "GraphicsDeviceRef_getPipelineCache()::data isn't empty"));

	writing = true;
	device = GraphicsDeviceRef_ptr(deviceRef);

	gotoIfError3(clean, GraphicsDevice_savePipelineCacheExt(device, alloc, &driverData, e_rr));

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_getPipelineCache() couldn't lock the device"));

	const U64 sizesBytes = device->pipelineSizes.length * sizeof(U64);
	const U64 total = sizeof(PipelineCacheHeader) + sizesBytes + Buffer_length(driverData);

	gotoIfError3(clean, Buffer_createUninitializedBytes(total, alloc, data, e_rr));

	const PipelineCacheHeader header = (PipelineCacheHeader) {
		.magic = PipelineCacheHeader_MAGIC,
		.version = PipelineCacheHeader_VERSION,
		.api = (U8) GraphicsInstanceRef_ptr(device->instance)->api,
		.sizeCount = (U32) (device->pipelineSizes.length / 2),
		.driverBytes = Buffer_length(driverData)
	};

	U8 *out = (U8*) data->ptr;
	*(PipelineCacheHeader*) out = header;
	Buffer_memcpy(Buffer_createRef(out + sizeof(header), sizesBytes), ListU64_bufferConst(device->pipelineSizes));
	Buffer_memcpy(Buffer_createRef(out + sizeof(header) + sizesBytes, Buffer_length(driverData)), driverData);

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	Buffer_free(&driverData, alloc);

	if(!s_uccess && writing)
		Buffer_free(data, alloc);

	return s_uccess;
}

Bool GraphicsDeviceRef_setPipelineCache(GraphicsDeviceRef *deviceRef, Buffer data, Error *e_rr) {

	Bool s_uccess = true;

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_setPipelineCache()::deviceRef is required"));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const U64 length = Buffer_length(data);

	//Unaligned reads, since the caller's blob may sit anywhere (a file read into the middle of a buffer)

	PipelineCacheHeader header = (PipelineCacheHeader) { 0 };

	if(length >= sizeof(header))
		Buffer_memcpy(Buffer_createRef(&header, sizeof(header)), Buffer_createRefConst(data.ptr, sizeof(header)));

	const U64 sizesBytes = (U64) header.sizeCount * 2 * sizeof(U64);

	if(
		length < sizeof(header) ||
		header.magic != PipelineCacheHeader_MAGIC ||
		header.version != PipelineCacheHeader_VERSION ||
		header.api != (U8) GraphicsInstanceRef_ptr(device->instance)->api ||
		length - sizeof(header) < sizesBytes ||
		length - sizeof(header) - sizesBytes != header.driverBytes
	) {
		Log_debugLnx("GraphicsDeviceRef_setPipelineCache() ignored data that isn't this API's pipeline cache");
		goto clean;
	}

	const U8 *sizes = data.ptr + sizeof(header);
	const Buffer driverData = Buffer_createRefConst(sizes + sizesBytes, header.driverBytes);

	Bool accepted = false;
	gotoIfError3(clean, GraphicsDevice_loadPipelineCacheExt(device, driverData, &accepted, e_rr));

	//The sizes describe that driver's code, so they're only taken with its data

	if(!accepted) {
		Log_debugLnx("GraphicsDeviceRef_setPipelineCache() ignored a pipeline cache from another device or driver");
		goto clean;
	}

	for(U64 i = 0; i < header.sizeCount; ++i) {

		U64 pair[2];
		Buffer_memcpy(Buffer_createRef(pair, sizeof(pair)), Buffer_createRefConst(sizes + i * sizeof(pair), sizeof(pair)));

		gotoIfError3(clean, GraphicsDevice_recordPipelineSize(device, pair[0], pair[1], e_rr));
	}

clean:
	return s_uccess;
}
