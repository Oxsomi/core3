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

//graphics/generic/descriptor_table.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "platforms/logx.h"
#include "types/container/ref_ptr.h"
#include "types/container/u128.h"
#include "types/base/string_base.h"
#include "types/base/string_read.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

TListImpl(TextureDescriptorRange);
TListImpl(BufferDescriptorRange);
TListImpl(DescriptorTableBinding);
TListImpl(DescriptorTableResourceRef);
TListImpl(Descriptor);
TListImpl(DescriptorStackTrace);

void DescriptorTableBinding_free(DescriptorTableBinding *binding, const Allocator *alloc, U8 type) {

	if(!(type & 4))
		return;

	type &= 3;

	ListU64_free(&binding->multiple.activeList, alloc);
	ListU64_free(&binding->multiple.maintainRef, alloc);
	ListWeakRefPtr_free(&binding->multiple.resources, alloc);
	ListDescriptorStackTrace_free(&binding->multiple.stackTraces, alloc);

	if(type == 0)
		ListTextureDescriptorRange_free(&binding->multiple.textures, alloc);

	else if(type == 1)
		ListBufferDescriptorRange_free(&binding->multiple.buffers, alloc);
}

void DescriptorTable_free(DescriptorTable *table, const Allocator *alloc) {

	//Announce descriptor leaks

	DescriptorHeap *parentPtr = DescriptorHeapRef_ptr(table->parent);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(parentPtr->device);

	const DescriptorLayoutInfo *info = &DescriptorLayoutRef_ptr(table->layout)->info;
	ListDescriptorBinding bindings = info->bindings;

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		for (U64 i = 0; i < table->bindings.length; ++i) {

			const DescriptorBinding *b = &bindings.ptr[i];
			const DescriptorTableBinding *tb = &table->bindings.ptr[i];
			CharString name = i < info->bindingNames.length ? info->bindingNames.ptr[i] : CharString_createNull();

			if (b->count > 1) {

				DescriptorStackTrace stack = (DescriptorStackTrace) { 0 };
				Buffer stackBuf = Buffer_createRef(stack.stackTrace, sizeof(stack.stackTrace));

				for(U64 j = 0, k = 0; j < b->count; ++j)
					if((tb->multiple.activeList.ptr[j >> 6] >> (j & 63)) & 1) {

						Log_warnLnx(
							"Leaked descriptor at %.*s[%"PRIu64"] (#%"PRIu64", id % "PRIu64", space % "PRIu64")",
							(int) CharString_length(name), name.ptr, j, i, b->binding.binding, b->binding.space
						);

						if (tb->multiple.stackTraces.length) {

							Buffer stackBufj = Buffer_createRefConst(
								tb->multiple.stackTraces.ptr[j].stackTrace, sizeof(stack.stackTrace)
							);

							if (!Buffer_eq(stackBuf, stackBufj)) {

								if(++k <= 8)
									Log_printCapturedStackTraceCustom(
										alloc,
										(const void**) stackBufj.ptr,
										sizeof(stack.stackTrace) / sizeof(stack.stackTrace[0]),
										ELogLevel_Warn,
										ELogOptions_None
									);

								Buffer_memcpy(stackBuf, stackBufj);
							}

							else if(k < 8)
								Log_warnLnx("(Omitted stacktrace, check last stack trace)");
						}
					}
			}

			else if(tb->single.resource) {

				Log_warnLnx(
					"Leaked descriptor at %.*s (#%"PRIu64", id %"PRIu64", space %"PRIu64")",
					(int) CharString_length(name), name.ptr, i, b->binding.binding, b->binding.space
				);

				if(tb->single.stackTrace.stackTrace[0])
					Log_printCapturedStackTraceCustom(
										alloc,
						(const void**) tb->single.stackTrace.stackTrace,
						sizeof(tb->single.stackTrace) / sizeof(void*),
						ELogLevel_Warn,
						ELogOptions_None
					);
			}
		}

	//Destroy

	if(table->acquiredAtomic)
		AtomicI64_dec(&parentPtr->descriptorTableCount);

	DescriptorTable_freeExt(table, alloc);

	for(U64 i = 0; i < table->bindings.length; ++i) {

		ESHRegisterType type = bindings.ptr[i].registerType & ESHRegisterType_TypeMask;
		U8 type8 = 2;

		if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
			type8 = 0;

		else if(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
			type8 = 1;

		if(bindings.ptr[i].count > 1)
			type8 |= 4;

		DescriptorTableBinding_free(&table->bindings.ptrNonConst[i], alloc, type8);
	}

	ListDescriptorTableBinding_free(&table->bindings, alloc);

	for (U64 i = 0; i < table->resources.length; ++i)
		RefPtr_dec(&table->resources.ptrNonConst[i].resource);

	ListDescriptorTableResourceRef_free(&table->resources, alloc);

	if(!(table->flags & EDescriptorTableFlags_InternalWeakDeviceRef))
		RefPtr_dec(&table->parent);
}

Bool DescriptorHeapRef_createDescriptorTable(
	DescriptorHeapRef *parent,
	DescriptorLayoutRef *layout,
	EDescriptorTableFlags flags,
	const CharString *name,
	DescriptorTableRef **tableRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = parent ? GraphicsDeviceRef_getAlloc(DescriptorHeapRef_ptr(parent)->device) : NULL;

	(void) flags;

	if(!parent || parent->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorHeap)
		retError(clean, Error_nullPointer(0, "DescriptorHeapRef_createDescriptorTable()::parent is required"));

	if(!layout || layout->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorLayout)
		retError(clean, Error_nullPointer(0, "DescriptorHeapRef_createDescriptorTable()::layout is required"));

	DescriptorHeap *parentPtr = DescriptorHeapRef_ptr(parent);
	DescriptorLayout *layoutPtr = DescriptorLayoutRef_ptr(layout);

	if(
		!(parentPtr->info.flags & EDescriptorHeapFlags_AllowBindless) &&
		(layoutPtr->info.flags & EDescriptorLayoutFlags_AllowBindlessAny)
	)
		retError(clean, Error_invalidOperation(
			0, "DescriptorHeapRef_createDescriptorTable() bindless descriptor set requires bindless descriptor heap"
		));

	ListDescriptorBinding bindings = layoutPtr->info.bindings;

	if(layoutPtr->info.flags & EDescriptorLayoutFlags_HasPushDescriptors)
		retError(clean, Error_invalidOperation(
			0, "DescriptorHeapRef_createDescriptorTable() creating a descriptor table for push descriptors is illegal"
		));

	GraphicsDeviceRef *dev = parentPtr->device;

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->descriptorTable, tableRef, e_rr));

	if(!(flags & EDescriptorTableFlags_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(parent));

	DescriptorTable *table = DescriptorTableRef_ptr(*tableRef);

	*table = (DescriptorTable) {
		.parent = parent,
		.layout = layout,
		.flags = flags
	};

	if (AtomicI64_inc(&parentPtr->descriptorTableCount) > parentPtr->info.maxDescriptorTables) {

		AtomicI64_dec(&parentPtr->descriptorTableCount);

		retError(clean, Error_outOfBounds(
			0, parentPtr->info.maxDescriptorTables, parentPtr->info.maxDescriptorTables,
			"DescriptorHeapRef_createDescriptorTable() descriptor heap is out of descriptor table slots"
		));
	}

	table->acquiredAtomic = true;

	gotoIfError3(clean, ListDescriptorTableBinding_resize(&table->bindings, bindings.length, alloc, e_rr));

	for (U64 i = 0; i < table->bindings.length; ++i) {

		U64 j = bindings.ptr[i].count;

		if(j <= 1)        //Uses the DescriptorTableBindingSingle saving a lot of allocations
			continue;

		DescriptorTableBindingMultiple *multiple = &table->bindings.ptrNonConst[i].multiple;

		gotoIfError3(clean, ListU64_resize(&multiple->activeList, (j + 63) >> 5, alloc, e_rr));
		gotoIfError3(clean, ListU64_resize(&multiple->maintainRef, (j + 63) >> 5, alloc, e_rr));
		gotoIfError3(clean, ListWeakRefPtr_resize(&multiple->resources, j, alloc, e_rr));

		if(GraphicsDeviceRef_ptr(dev)->flags & EGraphicsDeviceFlags_IsDebug)
			gotoIfError3(clean, ListDescriptorStackTrace_resize(&multiple->stackTraces, j, alloc, e_rr));

		ESHRegisterType type = bindings.ptr[i].registerType & ESHRegisterType_TypeMask;

		if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd) {
			gotoIfError3(clean, ListTextureDescriptorRange_resize(&multiple->textures, j, alloc, e_rr));
		}

		else if(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
			gotoIfError3(clean, ListBufferDescriptorRange_resize(&multiple->buffers, j, alloc, e_rr));
	}

	gotoIfError3(clean, DescriptorHeap_createDescriptorTableExt(parent, table, name, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(tableRef);

	return s_uccess;
}

Descriptor Descriptor_texture(
	TextureRef *texture, U8 mipId, U8 mipCount, U8 planeId, U8 imageId, U16 arrayId, U16 arrayCount
) {
	return (Descriptor) {
		.resource = texture,
		.texture = (TextureDescriptorRange) {

			.mipId = mipId,
			.mipCount = mipCount,
			.planeId = planeId,
			.imageId = imageId,

			.arrayId = arrayId,
			.arrayCount = arrayCount
		}
	};
}

Descriptor Descriptor_buffer(
	DeviceBufferRef *buffer, U64 startRegion, U64 endRegion, DeviceBufferRef *counter, U32 counterOffset
) {

	if(startRegion >> 48 || endRegion >> 48)
		return (Descriptor) { 0 };

	DescriptorRegionAndCounter startRegionCounter = (DescriptorRegionAndCounter) { .region48 = startRegion };
	DescriptorRegionAndCounter endRegionCounter = (DescriptorRegionAndCounter) { .region48 = endRegion };

	startRegionCounter.counter16.counterOffset16 = (U16) counterOffset;
	endRegionCounter.counter16.counterOffset16 = (U16) (counterOffset >> 16);

	return (Descriptor) {
		.resource = buffer,
		.buffer = (BufferDescriptorRange) {
			.startRegionAndCounterOffset = startRegionCounter,
			.endRegionAndCounterOffset = endRegionCounter,
			.counter = counter
		}
	};
}

Descriptor Descriptor_tlas(TLASRef *tlas) { return (Descriptor) { .resource = tlas }; }
Descriptor Descriptor_sampler(SamplerRef *sampler) { return (Descriptor) { .resource = sampler }; }

U64 Descriptor_startBuffer(const Descriptor *d) {
	return !d ? 0 : d->buffer.startRegionAndCounterOffset.region48 << 16 >> 16;
}

U64 Descriptor_endBuffer(const Descriptor *d) {
	return !d ? 0 : d->buffer.endRegionAndCounterOffset.region48 << 16 >> 16;
}

U64 Descriptor_bufferLength(const Descriptor *d) {
	return Descriptor_endBuffer(d) - Descriptor_startBuffer(d);
}

U32 Descriptor_counterOffset(const Descriptor *d) {
	return
		d->buffer.startRegionAndCounterOffset.counter16.counterOffset16 |
		((U32)d->buffer.endRegionAndCounterOffset.counter16.counterOffset16 << 16);
}

Bool DescriptorTable_loseRef(DescriptorTable *table, RefPtr *ref, Error *e_rr) {

	Bool s_uccess = true;

	if(!ref)
		return true;

	SpinLock *lock = &table->lock;
	ELockAcquire acq = SpinLock_lock(lock, 1 * SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_timedOut(0, 1 * SECOND, "DescriptorTable_loseRef() couldn't acquire descriptor range"));

	for(U64 i = 0; i < table->resources.length; ++i)
		if (table->resources.ptr[i].resource == ref) {

			if (!--table->resources.ptrNonConst[i].count) {        //Decrease refCount and decrease ref if last ref
				RefPtr_dec(&ref);
				gotoIfError3(clean, ListDescriptorTableResourceRef_erase(&table->resources, i, e_rr));
			}

			break;
		}

clean:

	if(lock && acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return s_uccess;
}

Bool DescriptorTable_addRef(DescriptorTable *table, RefPtr *ref, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = table ? GraphicsDeviceRef_getAlloc(DescriptorHeapRef_ptr(table->parent)->device) : NULL;

	if(!ref)
		return true;

	SpinLock *lock = &table->lock;
	ELockAcquire acq = SpinLock_lock(lock, 1 * SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_timedOut(0, 1 * SECOND, "DescriptorTable_loseRef() couldn't acquire descriptor range"));

	for(U64 i = 0; i < table->resources.length; ++i)
		if (table->resources.ptr[i].resource == ref) {
			++table->resources.ptrNonConst[i].count;
			goto clean;
		}

	gotoIfError3(clean, ListDescriptorTableResourceRef_pushBack(
		&table->resources, (DescriptorTableResourceRef) { .resource = ref, .count = 1 }, alloc, e_rr
	));

clean:

	if(lock && acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return s_uccess;
}

Bool DescriptorTableRef_unsetDescriptors(
	DescriptorTableRef *table,
	U64 bindId,
	U64 arrayId,
	U64 count,
	Error *e_rr
) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	SpinLock *lock = NULL;

	if(!table || table->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(0, "DescriptorTableRef_unsetDescriptors()::table is required"));

	DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);
	DescriptorLayout *layoutPtr = DescriptorLayoutRef_ptr(tablePtr->layout);
	ListDescriptorBinding bindings = layoutPtr->info.bindings;

	if(bindId >= bindings.length)
		retError(clean, Error_outOfBounds(
			0, bindId, bindings.length, "DescriptorTableRef_unsetDescriptors()::bindId out of bounds"
		));

	if(layoutPtr->info.flags & EDescriptorLayoutFlags_HasPushDescriptors)
		retError(clean, Error_invalidOperation(0, "DescriptorTableRef_unsetDescriptors() called on a push descriptor"));

	if(!count)
		retError(clean, Error_invalidOperation(0, "DescriptorTableRef_unsetDescriptors() needs count of >0"));

	DescriptorBinding b = bindings.ptr[bindId];

	if(arrayId >= b.count)
		retError(clean, Error_outOfBounds(0, arrayId, b.count, "DescriptorTableRef_unsetDescriptors()::arrayId out of bounds"));

	if(arrayId + count > b.count)
		retError(clean, Error_outOfBounds(
			0, arrayId + count, b.count, "DescriptorTableRef_unsetDescriptors()::arrayId + count out of bounds"
		));

	//Lock descriptor range and clear descriptor

	DescriptorTableBinding *binding = &tablePtr->bindings.ptrNonConst[bindId];
	lock = &binding->lock;

	if((acq = SpinLock_lock(lock, 1 * SECOND)) < ELockAcquire_Success)
		retError(clean, Error_timedOut(
			0,
			1 * SECOND,
			"DescriptorTableRef_unsetDescriptors() couldn't acquire descriptor range"
		));

	gotoIfError3(clean, DescriptorTable_unsetDescriptorsExt(tablePtr, bindId, arrayId, count, e_rr));

	if(b.count > 1)
		for (U64 i = arrayId; i < arrayId + count; ++i) {

			WeakRefPtr *ref = binding->multiple.resources.ptr[i];
			binding->multiple.activeList.ptrNonConst[i >> 6] &=~ ((U64)1 << (i & 63));

			if(ref && ref->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer) {
				gotoIfError3(clean, DescriptorTable_loseRef(tablePtr, binding->multiple.buffers.ptr[i].counter, e_rr));
				binding->multiple.buffers.ptrNonConst[i].counter = NULL;
			}

			if((binding->multiple.maintainRef.ptr[i >> 6] >> (i & 63)) & 1)
				gotoIfError3(clean, DescriptorTable_loseRef(tablePtr, ref, e_rr));

			binding->multiple.resources.ptrNonConst[i] = NULL;
		}

	else {
		WeakRefPtr *ref = binding->single.resource;

		if(ref && ref->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer) {
			gotoIfError3(clean, DescriptorTable_loseRef(tablePtr, binding->single.buffer.counter, e_rr));
			binding->single.buffer.counter = NULL;
		}

		if(binding->single.maintainRef)
			gotoIfError3(clean, DescriptorTable_loseRef(tablePtr, ref, e_rr));

		binding->single.resource = NULL;
	}

clean:

	if(lock && acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return s_uccess;
}

Bool Descriptor_eq(const Descriptor *a, const DescriptorTableBindingSingle *b, ESHRegisterType type) {

	if (!a || !b || a->resource != b->resource)
		return false;

	static_assert(
		sizeof(a->texture) == sizeof(U64),
		"Descriptor_eq() checks for texture by using data[0] (U64)"
	);

	static_assert(
		sizeof(a->buffer) == sizeof(U64) * 3,
		"Descriptor_eq() checks for texture by using data[0, 1 and 2] (U64[3])"
	);

	if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
		return a->data[0] == b->data[0];

	else if (type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
		return Buffer_eq(
			Buffer_createRefConst(a->data, sizeof(U64) * 3),
			Buffer_createRefConst(b->data, sizeof(U64) * 3)
		);

	return true;
}

Bool DescriptorTableRef_setDescriptors(
	DescriptorTableRef *table,
	U64 bindId,
	U64 arrayId,
	Bool maintainRef,
	const ListDescriptor *darr,
	Error *e_rr
) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	SpinLock *lock = NULL;

	if(!table || table->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(0, "DescriptorTableRef_setDescriptors()::table is required"));

	if(!darr)
		retError(clean, Error_nullPointer(4, "DescriptorTableRef_setDescriptors()::darr is required"));

	DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);
	DescriptorLayout *layoutPtr = DescriptorLayoutRef_ptr(tablePtr->layout);
	DescriptorHeap *heapPtr = DescriptorHeapRef_ptr(tablePtr->parent);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(heapPtr->device);

	ListDescriptorBinding bindings = layoutPtr->info.bindings;

	if(bindId >= bindings.length)
		retError(clean, Error_outOfBounds(
			0, bindId, bindings.length, "DescriptorTableRef_setDescriptors()::bindId out of bounds"
		));

	if(layoutPtr->info.flags & EDescriptorLayoutFlags_HasPushDescriptors)
		retError(clean, Error_invalidOperation(0, "DescriptorTableRef_setDescriptors() called on a push descriptor"));

	const DescriptorBinding *b = &bindings.ptr[bindId];
	ESHRegisterType type = b->registerType & ESHRegisterType_TypeMask;

	if(arrayId >= b->count)
		retError(clean, Error_outOfBounds(0, arrayId, b->count, "DescriptorTableRef_setDescriptors()::arrayId out of bounds"));

	if(arrayId + darr->length > b->count)
		retError(clean, Error_outOfBounds(
			0, arrayId + darr->length, b->count, "DescriptorTableRef_setDescriptors()::arrayId + darr.length out of bounds"
		));

	for(U64 j = 0; j < darr->length; ++j) {

		Descriptor d = darr->ptr[j];

		if(!d.resource) {

			if(
				d.buffer.counter ||
				d.buffer.startRegionAndCounterOffset.region48 ||
				d.buffer.endRegionAndCounterOffset.region48
			)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptors() descriptor should be empty if resource is NULL"
				));

			retError(clean, Error_invalidParameter(
				3, 0, "DescriptorTableRef_setDescriptors()::resource is required due to strict binding requirements"
			));
		}

		EGraphicsTypeId graphicsTypeId = (EGraphicsTypeId) d.resource->refPtrType->typeId;

		switch(graphicsTypeId) {

			//Validate buffer offset and resource params

			case EGraphicsTypeId_DeviceBuffer: {

				U32 counterOffset = Descriptor_counterOffset(&d);

				//Validate counter resource

				if(counterOffset && !d.buffer.counter)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() counterOffset must be 0 if there's no counter resource"
					));

				if(d.buffer.counter && !d.resource)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() counter must be NULL if resource is"
					));

				if (d.buffer.counter) {

					if(d.buffer.counter->refPtrType->typeId != (TypeId) EGraphicsTypeId_DeviceBuffer)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() counter must be a buffer"
						));

					if(type != ESHRegisterType_StructuredBufferAtomic)
						retError(clean, Error_invalidParameter(
							3, 0,
							"DescriptorTableRef_setDescriptors() can't set a counter resource if the type isn't "
							"Append/ConsumeBuffer"
						));

					GraphicsResource res = DeviceBufferRef_ptr(d.buffer.counter)->resource;

					if(counterOffset + 4 > res.size)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() counter offset out of bounds"
						));

					if(!(res.flags & EGraphicsResourceFlag_ShaderWrite))
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() counter resource isn't allowed to be written by shader"
						));
				}

				GraphicsResource res = DeviceBufferRef_ptr(d.resource)->resource;
				U64 len = res.size;

				if(!(res.flags & EGraphicsResourceFlag_ShaderWrite) && (b->registerType & ESHRegisterType_IsWrite))
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() resource isn't allowed to be written by shader"
					));

				U64 start = Descriptor_startBuffer(&d);

				if(Descriptor_endBuffer(&d) > len || start >= len)
					retError(clean, Error_outOfBounds(
						0, Descriptor_endBuffer(&d) > len ? Descriptor_endBuffer(&d) : start, len,
						"DescriptorTableRef_setDescriptors()::buffer start/end range out of bounds"
					));

				if(!Descriptor_endBuffer(&d))
					d.buffer.endRegionAndCounterOffset.region48 |= len - start;

				U64 descLen = Descriptor_bufferLength(&d);

				if(!(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd))
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() buffer resource set at a non buffer register"
					));

				if(type == ESHRegisterType_ConstantBuffer) {

					if(start & 255)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() constant buffer offset needs to be 256-byte aligned"
						));

					if(descLen & 255)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() constant buffer length needs to be 256-byte aligned"
						));

					if(descLen != b->constantBufferSize)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() constant buffer offset needs to be 256-byte aligned"
						));

					if(descLen > 64 * KIBI)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() constant buffer is limited to 64KiB"
						));

					if(!(DeviceBufferRef_ptr(d.resource)->usage & EDeviceBufferUsage_Uniform))
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() constant buffer needs the uniform buffer usage flag"
						));
				}

				//ShaderRead is not used for ConstantBuffer; it's EDeviceBufferUsage_Uniform

				else if(!(res.flags & EGraphicsResourceFlag_ShaderRead) && !(b->registerType & ESHRegisterType_IsWrite))
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() resource isn't allowed to be read by shader"
					));

				if(descLen > 2 * GIBI && type == ESHRegisterType_ByteAddressBuffer)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() byte address buffer is limited to 2GiB"
					));

				if (type == ESHRegisterType_StructuredBuffer || type == ESHRegisterType_StructuredBufferAtomic) {

					if(descLen / b->structedBufferStride > 2 * GIBI)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() structured buffer is limited to 2Gi elements"
						));

					if(descLen % b->structedBufferStride)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() structured buffer length should be aligned to stride"
						));

					if(start % b->structedBufferStride)
						retError(clean, Error_invalidParameter(
							3, 0,
							"DescriptorTableRef_setDescriptors() structured buffer startRange should be aligned to stride"
						));
				}

				if (type == ESHRegisterType_ByteAddressBuffer) {

					if(descLen % 4)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() byte address buffer length should be aligned to 4"
						));

					if(start % 4)
						retError(clean, Error_invalidParameter(
							3, 0, "DescriptorTableRef_setDescriptors() byte address buffer startRange should be aligned to 4"
						));
				}

				if (type == ESHRegisterType_StorageBuffer || type == ESHRegisterType_StorageBufferAtomic)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() storage buffer isn't supported yet"        //TODO:
					));

				break;
			}

			//No ranges are valid

			case EGraphicsTypeId_TLASExt:

				if(type != ESHRegisterType_AccelerationStructure)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() tlas set at a non tlas register"
					));

				if(
					d.buffer.counter ||
					d.buffer.startRegionAndCounterOffset.region48 ||
					d.buffer.endRegionAndCounterOffset.region48
				)
					retError(clean, Error_invalidParameter(
						3, 0,
						"DescriptorTableRef_setDescriptors() TLAS descriptor should be empty, only resource can be non NULL"
					));

				break;

			case EGraphicsTypeId_Sampler:

				if(type != ESHRegisterType_Sampler && type != ESHRegisterType_SamplerComparisonState)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() sampler set at a non sampler register"
					));

				if(
					d.resource &&
					(type == ESHRegisterType_SamplerComparisonState) != SamplerRef_ptr(d.resource)->info.enableComparison
				)
					retError(clean, Error_invalidParameter(
						3, 0,
						"DescriptorTableRef_setDescriptors() sampler doesn't match sampler type "
						"(SamplerState or SamplerComparisonState)"
					));

				if(
					d.buffer.counter ||
					d.buffer.startRegionAndCounterOffset.region48 ||
					d.buffer.endRegionAndCounterOffset.region48
				)
					retError(clean, Error_invalidParameter(
						3, 0,
						"DescriptorTableRef_setDescriptors() Sampler descriptor should be empty, only resource can be non NULL"
					));

				break;

			//Validate texture params and resource params

			case EGraphicsTypeId_DepthStencil:
			case EGraphicsTypeId_Swapchain:
			case EGraphicsTypeId_RenderTexture:
			case EGraphicsTypeId_DeviceTexture: {

				if(!(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd))
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() texture resource set at a non texture register"
					));

				if (type == ESHRegisterType_Texture1D)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() Texture1D isn't supported"
					));

				UnifiedTexture tex = TextureRef_getUnifiedTexture(d.resource, NULL);

				Bool isArray = b->registerType & ESHRegisterType_IsArray;
				Bool isWrite = b->registerType & ESHRegisterType_IsWrite;
				Bool isDepthBuffer = d.resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DepthStencil;
				Bool hasStencil = false;

				if(isDepthBuffer)
					hasStencil = tex.depthFormat >= EDepthStencilFormat_StencilStart;

				if(!(tex.resource.flags & EGraphicsResourceFlag_ShaderRead) && !(b->registerType & ESHRegisterType_IsWrite))
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() resource isn't allowed to be read by shader"
					));

				if(!(tex.resource.flags & EGraphicsResourceFlag_ShaderWrite) && (b->registerType & ESHRegisterType_IsWrite))
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() resource isn't allowed to be written by shader"
					));

				//Range check

				if(d.texture.mipId >= tex.levels || d.texture.mipId + d.texture.mipCount > tex.levels)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() texture specifies out of range mip"
					));

				if(d.texture.arrayId >= tex.length || d.texture.arrayId + d.texture.arrayCount > tex.length)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() texture specifies out of range array slice"
					));

				if(d.texture.imageId >= tex.images)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptors() texture specifies out of range imageId"
					));

				if(d.texture.planeId && (!isDepthBuffer || !hasStencil))
					retError(clean, Error_invalidParameter(
						3, 0,
						"DescriptorTableRef_setDescriptors() texture specifies planeId > 0 "
						"but it doesn't have a stencil buffer"
					));

				//Only arrays are allowed to have arrayCount and arrayId (excluding 3D textures and cubes)

				Bool isArrayType = isArray || type == ESHRegisterType_TextureCube || type == ESHRegisterType_Texture3D;

				if(!isArrayType && (d.texture.arrayId || (d.texture.arrayCount != tex.length && d.texture.arrayCount)))
					retError(clean, Error_invalidParameter(
						3, 0,
						"DescriptorTableRef_setDescriptors() texture specifies arrayCount and/or arrayId, "
						"but it's not an array, 3D or cube"
					));

				//Same thing as reading all TextureDescriptor values; Texture2DMS isn't allowed to specify anything
				// nor used as a regular texture

				if (tex.sampleCount && (
					d.texture.mipId || d.texture.planeId || d.texture.imageId ||
					(d.texture.mipCount != tex.levels && d.texture.mipCount)
				))
					retError(clean, Error_unsupportedOperation(
						0, "DescriptorTableRef_setDescriptors() (RW)Texture2DMS doesn't support specifying a subresource"
					));

				if(tex.sampleCount && type != ESHRegisterType_Texture2DMS)
					retError(clean, Error_unsupportedOperation(
						0, "DescriptorTableRef_setDescriptors() (RW)Texture2DMS can't be used as a regular texture"
					));

				//Default the arrayCount to the right size

				if(isArrayType && !d.texture.arrayCount)
					d.texture.arrayCount = tex.length - d.texture.arrayId;

				//TextureCube validation

				if(type == ESHRegisterType_TextureCube) {

					if((d.texture.arrayCount % 6) || !d.texture.arrayCount)
						retError(clean, Error_invalidState(
							0,
							"DescriptorTableRef_setDescriptors() TextureCube(Array) needs to specify a multiple of 6 arrayCount"
						));

					if(!isArray && (d.texture.arrayCount != 6 || d.texture.arrayId))
						retError(clean, Error_invalidState(
							0, "DescriptorTableRef_setDescriptors() TextureCube has size != 6 or arrayId != 0"
						));
				}

				//Only Texture2D(Array) can have a planeId

				if(type != ESHRegisterType_Texture2D && d.texture.planeId)
					retError(clean, Error_invalidState(
						0, "DescriptorTableRef_setDescriptors() Texture2D(Array) is the only one permitted to have a planeId"
					));

				//Validate UAV

				if(isWrite) {

					if(d.texture.mipCount > 1)
						retError(clean, Error_invalidState(
							0,
							"DescriptorTableRef_setDescriptors() RWTextures aren't allowed to have more than 1 mip selected"
						));

					if(tex.sampleCount && !(device->info.capabilities.features & EGraphicsFeatures_WriteMSTexture))
						retError(clean, Error_unsupportedOperation(
							0, "DescriptorTableRef_setDescriptors() RWTexture2DMS is unsupported on this device"
						));
				}

				break;
			}

			default:
				retError(clean, Error_invalidParameter(3, 0, "DescriptorTableRef_setDescriptors()::resource is incompatible"));
		}
	}

	//Capture stackTrace

	DescriptorStackTrace stack;
	Error_captureStackTrace(stack.stackTrace, (U8)(sizeof(DescriptorStackTrace) / sizeof(void*)), 1);

	//Lock descriptor and check if descriptor is the same

	DescriptorTableBinding *binding = &tablePtr->bindings.ptrNonConst[bindId];
	lock = &binding->lock;

	if((acq = SpinLock_lock(lock, 1 * SECOND)) < ELockAcquire_Success)
		retError(clean, Error_timedOut(0, 1 * SECOND, "DescriptorTableRef_setDescriptors() couldn't acquire descriptor range"));

	if(b->count > 1) {

		Bool allEq = true;
		DescriptorTableBindingSingle single = (DescriptorTableBindingSingle) { 0 };

		for(U64 j = arrayId; j < arrayId + darr->length; ++j) {

			single.resource = binding->multiple.resources.ptr[j];

			if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
				single.texture = binding->multiple.textures.ptr[j];

			else if (type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
				single.buffer = binding->multiple.buffers.ptr[j];

			if (!Descriptor_eq(&darr->ptr[j - arrayId], &single, type)) {
				allEq = false;
				break;
			}
		}

		if(allEq)
			goto clean;
	}

	else if(Descriptor_eq(&darr->ptr[0], &binding->single, type))
		goto clean;

	//Release previous descriptors and set new ones

	gotoIfError3(clean, DescriptorTableRef_unsetDescriptors(table, bindId, arrayId, darr->length, e_rr));
	gotoIfError3(clean, DescriptorTable_setDescriptorsExt(tablePtr, bindId, arrayId, darr, e_rr));

	if(maintainRef)
		for(U64 j = 0; j < darr->length; ++j) {

			RefPtr *res = darr->ptr[j].resource;
			Bool isBuffer = res && res->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer;

			Bool addRes = DescriptorTable_addRef(tablePtr, res, e_rr);
			Bool addCounter = !isBuffer ? true : addRes && DescriptorTable_addRef(tablePtr, darr->ptr[j].buffer.counter, e_rr);

			if (!addRes || !addCounter) {

				for(U64 k = 0; k < j; ++k) {

					RefPtr *resk = darr->ptr[k].resource;
					DescriptorTable_loseRef(tablePtr, resk, e_rr);

					if(resk && resk->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer)
						DescriptorTable_loseRef(tablePtr, darr->ptr[k].buffer.counter, e_rr);
				}

				if(addRes)
					DescriptorTable_loseRef(tablePtr, darr->ptr[j].resource, e_rr);

				goto clean;
			}
		}

	//Now that we have registered all resources, it should be safe to set them properly

	if(b->count > 1)
		for(U64 j = arrayId; j < arrayId + darr->length; ++j) {

			binding->multiple.resources.ptrNonConst[j] = darr->ptr[j - arrayId].resource;
			binding->multiple.activeList.ptrNonConst[j >> 6] |= (U64)1 << (j & 63);

			U64 *maintainRefPtr = &binding->multiple.maintainRef.ptrNonConst[j >> 6];

			if(!maintainRef)
				*maintainRefPtr &= ~((U64)1 << (j & 63));

			else *maintainRefPtr |= (U64)1 << (j & 63);

			if(binding->multiple.stackTraces.ptr)
				binding->multiple.stackTraces.ptrNonConst[j] = stack;

			if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
				binding->multiple.textures.ptrNonConst[j] = darr->ptr[j - arrayId].texture;

			else if(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
				binding->multiple.buffers.ptrNonConst[j] = darr->ptr[j - arrayId].buffer;
		}

	else {

		binding->single.resource = darr->ptr[0].resource;
		binding->single.stackTrace = stack;
		binding->single.maintainRef = maintainRef;

		if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
			binding->single.texture = darr->ptr[0].texture;

		else if(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
			binding->single.buffer = darr->ptr[0].buffer;
	}

clean:

	if(lock && acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return s_uccess;
}

Bool DescriptorTableRef_setDescriptor(
	DescriptorTableRef *table,
	U64 bindId,
	U64 arrayId,
	Bool maintainRef,
	const Descriptor *d,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListDescriptor descriptors = (ListDescriptor) { 0 };

	gotoIfError3(clean, ListDescriptor_createRefConst(d, 1, &descriptors, e_rr));
	gotoIfError3(clean, DescriptorTableRef_setDescriptors(table, bindId, arrayId, maintainRef, &descriptors, e_rr));

clean:
	return s_uccess;
}

U64 DescriptorTableRef_resolveRegisterName(DescriptorTableRef *table, const CharString *registerName) {

	if(!table || table->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		return U64_MAX;

	const DescriptorLayoutInfo *info = &DescriptorLayoutRef_ptr(DescriptorTableRef_ptr(table)->layout)->info;

	for(U64 i = 0; i < info->bindingNames.length; ++i)
		if(CharString_equalsString(&info->bindingNames.ptr[i], registerName, EStringCase_Sensitive))
			return i;

	return U64_MAX;
}

Bool DescriptorTableRef_setDescriptorByName(
	DescriptorTableRef *table,
	const CharString *registerName,
	U64 arrayId,
	Bool maintainRef,
	const Descriptor *d,
	Error *e_rr
) {

	Bool s_uccess = true;

	U64 binding = DescriptorTableRef_resolveRegisterName(table, registerName);

	if(binding == U64_MAX)
		retError(clean, Error_notFound(0, 1, "DescriptorTableRef_setDescriptorByName register not found"));

	gotoIfError3(clean, DescriptorTableRef_setDescriptor(table, binding, arrayId, maintainRef, d, e_rr));

clean:
	return s_uccess;
}

Bool DescriptorTableRef_setDescriptorsByName(
	DescriptorTableRef *table,
	const CharString *registerName,
	U64 arrayId,
	Bool maintainRef,
	const ListDescriptor *d,
	Error *e_rr
) {

	Bool s_uccess = true;

	U64 binding = DescriptorTableRef_resolveRegisterName(table, registerName);

	if(binding == U64_MAX)
		retError(clean, Error_notFound(0, 1, "DescriptorTableRef_setDescriptorsByName register not found"));

	gotoIfError3(clean, DescriptorTableRef_setDescriptors(table, binding, arrayId, maintainRef, d, e_rr));

clean:
	return s_uccess;
}

Bool DescriptorTableRef_unsetDescriptorsByName(
	DescriptorTableRef *table,
	const CharString *registerName,
	U64 arrayId,
	U64 count,
	Error *e_rr
) {

	Bool s_uccess = true;

	U64 binding = DescriptorTableRef_resolveRegisterName(table, registerName);

	if(binding == U64_MAX)
		retError(clean, Error_notFound(0, 1, "DescriptorTableRef_unsetDescriptorsByName register not found"));

	gotoIfError3(clean, DescriptorTableRef_unsetDescriptors(table, binding, arrayId, count, e_rr));

clean:
	return s_uccess;
}

Bool DescriptorTableRef_allocDescriptorByName(
	DescriptorTableRef *table,
	const CharString *registerName,
	U64 *arrayId,           //outputs arrayId into descriptor if success
	Bool maintainRef,
	const Descriptor *d,
	Error *e_rr
) {

	Bool s_uccess = true;

	U64 binding = DescriptorTableRef_resolveRegisterName(table, registerName);

	if (binding == U64_MAX)
		retError(clean, Error_notFound(0, 1, "DescriptorTableRef_unsetDescriptorsByName register not found"));

	gotoIfError3(clean, DescriptorTableRef_allocDescriptor(table, binding, arrayId, maintainRef, d, e_rr));

clean:
	return s_uccess;
}

Bool DescriptorTableRef_findBindlessRegister(
	DescriptorTableRef *table,
	ESHRegisterType regType,
	U32 strideOrLength,
	U16 *bindId,
	U8 *bindlessTypeId,
	RefPtr *resource,
	U8 planeId,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!table || !bindId || !bindlessTypeId || !resource)
		retError(clean, Error_nullPointer(
			!table || table->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable ? 0 : (
				!bindId ? 3 : (!bindlessTypeId ? 4 : 5)
			),
			"DescriptorTableRef_findBindlessRegister() requires table, bindId and resource"
		));

	DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);
	DescriptorLayout *layout = DescriptorLayoutRef_ptr(tablePtr->layout);
	ListDescriptorBinding bindings = layout->info.bindings;

	if(layout->info.flags & EDescriptorLayoutFlags_HasPushDescriptors)
		retError(clean, Error_invalidOperation(0, "DescriptorTableRef_findBindlessRegister() called on a push descriptor"));

	DescriptorHeap *heapPtr = DescriptorHeapRef_ptr(tablePtr->parent);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(heapPtr->device);
	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device);

	U8 resourceType = 0;
	Bool isDepthStencil = resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DepthStencil;

	if(TextureRef_isTexture(resource)) {

		if(isDepthStencil && (regType & ESHRegisterType_IsWrite))
			retError(clean, Error_invalidOperation(
				0, "DescriptorTableRef_findBindlessRegister() DepthStencil is not permitted on RW texture"
			));

		if (isDepthStencil) {

			UnifiedTexture tex = TextureRef_getUnifiedTexture(resource, NULL);

			if(planeId && !(tex.depthFormat >= EDepthStencilFormat_StencilStart))
				retError(clean, Error_invalidOperation(
					0, "DescriptorTableRef_findBindlessRegister() Requested DepthStencil planeId 1 but no stencil was present"
				));

			if (!planeId && tex.depthFormat == EDepthStencilFormat_S8X24Ext)
				retError(clean, Error_invalidOperation(
					0, "DescriptorTableRef_findBindlessRegister() Requested DepthStencil planeId 0 but no depth was present"
				));
		}

		resourceType = 0;
	}

	else if(resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer)
		resourceType = 1;

	else if(resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_Sampler)
		resourceType = 2;

	else if(resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_TLASExt)
		resourceType = 3;

	else {
		retError(clean, Error_invalidOperation(0, "DescriptorTableRef_findBindlessRegister() invalid resource type"));
	}

	if(!isDepthStencil && planeId)
		retError(clean, Error_invalidOperation(
			0, "DescriptorTableRef_findBindlessRegister() depth stencil is the only one allowed a planeId"
		));

	ESHRegisterType type = regType & ESHRegisterType_TypeMask;

	U8 otherResourceType = 0;

	if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
		otherResourceType = 0;

	else if(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
		otherResourceType = 1;

	else if(type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState)
		otherResourceType = 2;

	else if(type == ESHRegisterType_AccelerationStructure)
		otherResourceType = 3;

	else {
		retError(clean, Error_invalidOperation(1, "DescriptorTableRef_findBindlessRegister() invalid register type"));
	}

	if(resourceType != otherResourceType)
		retError(clean, Error_invalidOperation(
			2, "DescriptorTableRef_findBindlessRegister() mismatching resource and register type"
		));

	for(U8 i = 0; i < layout->bindlessTypeToBinding.length; ++i) {

		U16 bindIdi = layout->bindlessTypeToBinding.ptr[i];
		DescriptorBinding bind = bindings.ptr[bindIdi];

		if(bind.registerType != regType)
			continue;

		if(strideOrLength && bind.data != strideOrLength)
			continue;

		//Write textures need to match format somewhat;
		//For DXIL the rules are as follows:
		//    float textures are all implicitly castable (snorm, unorm, float) -> float
		//    uint/int/float textures aren't
		//For SPIRV the rules are as follows:
		//    Unknown allows anything
		//    Otherwise the format needs to match 1:1
		//    However, since we're relying on reading a descriptor at a certain place, we can't reuse.
		//
		//Read textures are simpler;
		//DXIL all textures are typeless.
		//SPIRV: Unknown allows anything, otherwise match 1:1

		if (otherResourceType == 0) {

			UnifiedTexture tex = TextureRef_getUnifiedTexture(resource, NULL);
			ETextureFormatId formatId = tex.textureFormatId;

			if (isDepthStencil)
				formatId = !planeId ? ETextureFormatId_R8u : (
					tex.depthFormat == EDepthStencilFormat_D16 ? ETextureFormatId_R16 : ETextureFormatId_R32f
				);

			ESHTexturePrimitive targPrim = bind.textureFormat.primitive & ESHTexturePrimitive_TypeMask;

			if((bind.registerType & ESHRegisterType_IsWrite) && targPrim != ESHTexturePrimitive_Count) {

				ESHTexturePrimitive prim =
					ESHTexturePrimitive_fromTextureFormat(ETextureFormatId_unpack[formatId]) & ESHTexturePrimitive_TypeMask;

				Bool compatible = false;

				switch (targPrim) {

					case ESHTexturePrimitive_Float:
					case ESHTexturePrimitive_Double:

						compatible =
							prim == ESHTexturePrimitive_Float || prim == ESHTexturePrimitive_Double ||
							prim == ESHTexturePrimitive_SNorm || prim == ESHTexturePrimitive_UNorm;

						break;

					default:
						compatible = prim == targPrim;
						break;
				}

				if(!compatible)
					continue;
			}

			if (instance->api == EGraphicsApi_Vulkan && bind.textureFormat.formatId && bind.textureFormat.formatId != formatId)
				continue;
		}

		*bindId = bindIdi;
		*bindlessTypeId = i;
		goto clean;
	}

	retError(clean, Error_notFound(0, 0, "DescriptorTableRef_findBindlessRegister() no legal resource found"));

clean:
	return s_uccess;
}

Bool DescriptorTableRef_allocDescriptor(
	DescriptorTableRef *table,
	U64 bindId,
	U64 *arrayId,
	Bool maintainRef,
	const Descriptor *d,
	Error *e_rr
) {

	Bool s_uccess = true;

	ELockAcquire acq = ELockAcquire_Invalid;
	SpinLock *lock = NULL;

	if(!table || table->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(0, "DescriptorTableRef_allocDescriptor()::table is required"));

	if(!arrayId)
		retError(clean, Error_nullPointer(2, "DescriptorTableRef_allocDescriptor()::arrayId is required"));

	DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);

	if(bindId >= tablePtr->bindings.length)
		retError(clean, Error_outOfBounds(
			1, bindId, tablePtr->bindings.length, "DescriptorTableRef_allocDescriptor()::bindId out of bounds"
		));

	DescriptorLayout *layout = DescriptorLayoutRef_ptr(tablePtr->layout);

	if(layout->info.flags & EDescriptorLayoutFlags_HasPushDescriptors)
		retError(clean, Error_invalidOperation(0, "DescriptorTableRef_allocDescriptor() called on a push descriptor"));

	ListDescriptorBinding bindings = layout->info.bindings;

	if(bindings.ptr[bindId].count <= 1)
		retError(clean, Error_invalidState(0, "DescriptorTableRef_allocDescriptor() can't be called on non arrays"));

	DescriptorTableBinding *binding = tablePtr->bindings.ptrNonConst + bindId;
	lock = &binding->lock;

	if((acq = SpinLock_lock(lock, 1 * SECOND)) < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "DescriptorTableRef_allocDescriptor() couldn't acquire lock"));

	//Quick search, we perform per 2 U128, since there's no U64 version yet.
	//TODO: This could be sped up by having a reduction bitset, for example:
	//        isFree0
	//        isFree00 isFree01
	//        isFree000 isFree001 isFree010 isFree011
	//        etc.

	ListU64 activeList = binding->multiple.activeList;
	U64 foundId = activeList.length;

	for(U64 j = 0; j < activeList.length; j += 2) {

		U128 big = U128_createU64x2(
			activeList.ptr[j],
			j + 1 < activeList.length ? activeList.ptr[j + 1] : 0
		);

		big = U128_not(big);

		U8 bit = U128_bitScanReverse(big);

		if(bit == U8_MAX)
			continue;

		foundId = (j << 7) | bit;
		break;
	}

	if(foundId >= activeList.length)
		retError(clean, Error_outOfMemory(0, "DescriptorTableRef_allocDescriptor() out of memory"));

	*arrayId = (U32) foundId;
	gotoIfError3(clean, DescriptorTableRef_setDescriptor(table, bindId, *arrayId, maintainRef, d, e_rr));

clean:

	if(lock && acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return s_uccess;
}

Bool DescriptorTableRef_allocDescriptorBindless(
	DescriptorTableRef *table,
	ESHRegisterType type,
	U32 strideOrLength,
	U16 *bindId,
	U8 *bindlessTypeId,
	U64 *arrayId,
	Bool maintainRef,
	const Descriptor *d,
	Error *e_rr
) {

	Bool s_uccess = true;

	ESHRegisterType type4 = type & ESHRegisterType_TypeMask;
	Bool isTexture = type4 >= ESHRegisterType_TextureStart && type4 < ESHRegisterType_TextureEnd;

	if(!d)
		retError(clean, Error_nullPointer(7, "DescriptorTableRef_allocDescriptorBindless()::d is required"));

	gotoIfError3(clean, DescriptorTableRef_findBindlessRegister(
		table,
		type,
		strideOrLength,
		bindId,
		bindlessTypeId,
		d->resource,
		isTexture ? d->texture.planeId : 0,
		e_rr
	));

	gotoIfError3(clean, DescriptorTableRef_allocDescriptor(
		table,
		*bindId,
		arrayId,
		maintainRef,
		d,
		e_rr
	));

clean:
	return s_uccess;
}
