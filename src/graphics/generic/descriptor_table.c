/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/container/string.h"

Error DescriptorTableRef_dec(DescriptorTableRef **table) {
	return !RefPtr_dec(table) ?
		Error_invalidOperation(0, "DescriptorTableRef_dec()::table is required") : Error_none();
}

Error DescriptorTableRef_inc(DescriptorTableRef *table) {
	return !RefPtr_inc(table) ?
		Error_invalidOperation(0, "DescriptorTableRef_dec()::table is required") : Error_none();
}

Bool DescriptorTable_free(DescriptorTable *table, Allocator alloc) {

	(void)alloc;

	//Log_debugLnx("Destroy: %p", table);

	DescriptorHeap *parentPtr = DescriptorHeapRef_ptr(table->parent);

	if(table->acquiredAtomic)
		AtomicI64_dec(&parentPtr->descriptorTableCount);

	Bool success = DescriptorTable_freeExt(table, alloc);
	success &= !DescriptorHeapRef_dec(&table->parent).genericError;
	return success;
}

Error DescriptorHeapRef_createDescriptorTable(
	DescriptorHeapRef *parent,
	DescriptorLayoutRef *layout,
	EDescriptorTableFlags flags,
	CharString name,
	DescriptorTableRef **tableRef
) {

	(void) flags;

	if(!parent || parent->typeId != (ETypeId) EGraphicsTypeId_DescriptorHeap)
		return Error_nullPointer(0, "DescriptorHeapRef_createDescriptorTable()::parent is required");

	if(!layout || layout->typeId != (ETypeId) EGraphicsTypeId_DescriptorLayout)
		return Error_nullPointer(0, "DescriptorHeapRef_createDescriptorTable()::layout is required");

	DescriptorHeap *parentPtr = DescriptorHeapRef_ptr(parent);
	DescriptorLayout *layoutPtr = DescriptorLayoutRef_ptr(layout);

	if(
		!(parentPtr->info.flags & EDescriptorHeapFlags_AllowBindless) &&
		!!(layoutPtr->info.flags & EDescriptorLayoutFlags_AllowBindlessAny)
	)
		return Error_invalidOperation(
			0, "DescriptorHeapRef_createDescriptorTable() bindless descriptor set requires bindless descriptor heap"
		);

	GraphicsDeviceRef *dev = parentPtr->device;

	Error err = RefPtr_createx(
		(U32)(sizeof(DescriptorTable) + GraphicsDeviceRef_getObjectSizes(dev)->descriptorHeap),
		(ObjectFreeFunc) DescriptorTable_free,
		(ETypeId) EGraphicsTypeId_DescriptorTable,
		tableRef
	);

	if(err.genericError)
		return err;

	gotoIfError(clean, GraphicsDeviceRef_inc(parent))

	DescriptorTable *table = DescriptorTableRef_ptr(*tableRef);

	//Log_debugLnx("Create: DescriptorTable %.*s (%p)", (int) CharString_length(name), name.ptr, heap);

	*table = (DescriptorTable) {
		.parent = parent,
		.layout = layout,
		.flags = flags
	};

	if (AtomicI64_inc(&parentPtr->descriptorTableCount) > parentPtr->info.maxDescriptorTables) {

		AtomicI64_dec(&parentPtr->descriptorTableCount);

		gotoIfError(clean, Error_outOfBounds(
			0, parentPtr->info.maxDescriptorTables, parentPtr->info.maxDescriptorTables,
			"DescriptorHeapRef_createDescriptorTable() descriptor heap is out of descriptor table slots"
		))
	}

	table->acquiredAtomic = true;

	gotoIfError(clean, DescriptorHeap_createDescriptorTableExt(parent, table, name))

clean:

	if(err.genericError)
		DescriptorTableRef_dec(tableRef);

	return err;
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

U64 Descriptor_startBuffer(Descriptor d) {
	return d.buffer.startRegionAndCounterOffset.region48 << 16 >> 16;
}

U64 Descriptor_endBuffer(Descriptor d) {
	return d.buffer.startRegionAndCounterOffset.region48 << 16 >> 16;
}

U64 Descriptor_bufferLength(Descriptor d) {
	return Descriptor_endBufferRange(d) - Descriptor_startBufferRange(d);
}

U32 Descriptor_counterOffset(Descriptor d) {
	return
		d.buffer.startRegionAndCounterOffset.counter16.counterOffset16 |
		((U32)d.buffer.endRegionAndCounterOffset.counter16.counterOffset16 << 16);
}

Bool DescriptorTableRef_setDescriptor(
	DescriptorTableRef *table,
	U64 i,
	U64 arrayId,
	Descriptor d,
	Error *e_rr
) {
	Bool s_uccess = true;

	if(!table || table->typeId != (ETypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(0, "DescriptorTableRef_setDescriptor()::table is required"))

	DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);
	DescriptorHeap *heapPtr = DescriptorHeapRef_ptr(tablePtr->parent);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(heapPtr->device);

	ListDescriptorBinding bindings = DescriptorLayoutRef_ptr(tablePtr->layout)->info.bindings;

	if(i >= bindings.length)
		retError(clean, Error_outOfBounds(0, i, bindings.length, "DescriptorTableRef_setDescriptor()::i out of bounds"))

	DescriptorBinding b = bindings.ptr[i];
	ESHRegisterType type = b.registerType & ESHRegisterType_TypeMask;

	if(arrayId >= b.count)
		retError(clean, Error_outOfBounds(0, arrayId, b.count, "DescriptorTableRef_setDescriptor()::arrayId out of bounds"))

	if(!d.resource) {

		if(d.buffer.counter || d.buffer.startRegionAndCounterOffset.region48 || d.buffer.endRegionAndCounterOffset.region48)
			retError(clean, Error_invalidParameter(
				3, 0, "DescriptorTableRef_setDescriptor() descriptor should be empty if resource is NULL"
			))

		goto skipValidation;
	}

	switch((EGraphicsTypeId) d.resource->typeId) {

		//Validate buffer offset and resource params

		case EGraphicsTypeId_DeviceBuffer: {

			U32 counterOffset = Descriptor_counterOffset(d);

			//Validate counter resource

			if(counterOffset && !d.buffer.counter)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() counterOffset must be 0 if there's no counter resource"
				))

			if(d.buffer.counter && !d.resource)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() counter must be NULL if resource is"
				))

			if (d.buffer.counter) {

				if(d.buffer.counter->typeId != (ETypeId) EGraphicsTypeId_DeviceBuffer)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptor() counter must be a buffer"
					))

				if(type != ESHRegisterType_StructuredBufferAtomic)
					retError(clean, Error_invalidParameter(
						3, 0,
						"DescriptorTableRef_setDescriptor() can't set a counter resource if the type isn't "
						"Append/ConsumeBuffer"
					))

				GraphicsResource res = DeviceBufferRef_ptr(d.buffer.counter)->resource;

				if(counterOffset + 4 > res.size)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptor() counter offset out of bounds"
					))

				if(!(res.flags & EGraphicsResourceFlag_ShaderWrite))
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptor() counter resource isn't allowed to be written by shader"
					))
			}

			GraphicsResource res = DeviceBufferRef_ptr(d.resource)->resource;
			U64 len = res.size;
				
			if(!(res.flags & EGraphicsResourceFlag_ShaderRead) && !(b.registerType & ESHRegisterType_IsWrite))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() resource isn't allowed to be read by shader"
				))

			if(!(res.flags & EGraphicsResourceFlag_ShaderWrite) && !!(b.registerType & ESHRegisterType_IsWrite))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() resource isn't allowed to be written by shader"
				))

			U64 start = Descriptor_startBuffer(d);

			if(Descriptor_endBuffer(d) > len || start >= len)
				retError(clean, Error_outOfBounds(
					0, Descriptor_endBuffer(d) > len ? Descriptor_endBuffer(d) :start, len,
					"DescriptorTableRef_setDescriptor()::buffer start/end range out of bounds"
				))

			if(!Descriptor_endBuffer(d))
				d.buffer.endRegionAndCounterOffset.region48 |= len - start;

			U64 descLen = Descriptor_bufferLength(d);

			if(!(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() buffer resource set at a non buffer register"
				))

			if(!!(start & 255) && type == ESHRegisterType_ConstantBuffer)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() constant buffer offset needs to be 256-byte aligned"
				))

			if(!!(descLen & 255) && type == ESHRegisterType_ConstantBuffer)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() constant buffer length needs to be 256-byte aligned"
				))

			if(descLen != b.strideOrLength && type == ESHRegisterType_ConstantBuffer)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() constant buffer offset needs to be 256-byte aligned"
				))

			if(descLen > 64 * KIBI && type == ESHRegisterType_ConstantBuffer)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() constant buffer is limited to 64KiB"
				))

			if(descLen > 2 * GIBI && type == ESHRegisterType_ByteAddressBuffer)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() byte address buffer is limited to 2GiB"
				))

			if(descLen / b.strideOrLength > 2 * GIBI && (
				type == ESHRegisterType_StructuredBuffer || type == ESHRegisterType_StructuredBufferAtomic
			))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() structured buffer is limited to 2Gi elements"
				))

			if (type == ESHRegisterType_StructuredBuffer || type == ESHRegisterType_StructuredBufferAtomic) {

				if(descLen % b.strideOrLength)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptor() structured buffer length should be aligned to stride"
					))

				if(start % b.strideOrLength)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptor() structured buffer startRange should be aligned to stride"
					))
			}

			if (type == ESHRegisterType_ByteAddressBuffer) {

				if(descLen % 4)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptor() byte address buffer length should be aligned to 4"
					))

				if(start % 4)
					retError(clean, Error_invalidParameter(
						3, 0, "DescriptorTableRef_setDescriptor() byte address buffer startRange should be aligned to 4"
					))
			}

			if (type == ESHRegisterType_StorageBuffer || type == ESHRegisterType_StorageBufferAtomic)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() storage buffer isn't supported yet"		//TODO:
				))

			break;
		}

		//No ranges are valid

		case EGraphicsTypeId_TLASExt:

			if(type != ESHRegisterType_AccelerationStructure)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() tlas set at a non tlas register"
				))

			if(d.buffer.counter || d.buffer.startRegionAndCounterOffset.region48 || d.buffer.endRegionAndCounterOffset.region48)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() TLAS descriptor should be empty, only resource can be non NULL"
				))

			break;

		case EGraphicsTypeId_Sampler:

			if(type != ESHRegisterType_Sampler && type != ESHRegisterType_SamplerComparisonState)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() sampler set at a non sampler register"
				))

			if(
				d.resource &&
				(type == ESHRegisterType_SamplerComparisonState) != SamplerRef_ptr(d.resource)->info.enableComparison
			)
				retError(clean, Error_invalidParameter(
					3, 0,
					"DescriptorTableRef_setDescriptor() sampler doesn't match sampler type "
					"(SamplerState or SamplerComparisonState)"
				))

			if(d.buffer.counter || d.buffer.startRegionAndCounterOffset.region48 || d.buffer.endRegionAndCounterOffset.region48)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() Sampler descriptor should be empty, only resource can be non NULL"
				))

			break;

		//Validate texture params and resource params

		case EGraphicsTypeId_DepthStencil:
		case EGraphicsTypeId_Swapchain:
		case EGraphicsTypeId_RenderTexture:
		case EGraphicsTypeId_DeviceTexture: {

			if(!(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() texture resource set at a non texture register"
				))

			if (type == ESHRegisterType_Texture1D)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() Texture1D isn't supported"
				))

			UnifiedTexture tex = TextureRef_getUnifiedTexture(d.resource, NULL);

			Bool isArray = !!(b.registerType & ESHRegisterType_IsArray);
			Bool isWrite = !!(b.registerType & ESHRegisterType_IsWrite);
			Bool isDepthBuffer = d.resource->typeId == (ETypeId) EGraphicsTypeId_DepthStencil;
			Bool hasStencil = false;

			if(isDepthBuffer)
				hasStencil =
					tex.depthFormat >= EDepthStencilFormat_StencilStart && tex.depthFormat < EDepthStencilFormat_StencilEnd;

			if(!(tex.resource.flags & EGraphicsResourceFlag_ShaderRead) && !(b.registerType & ESHRegisterType_IsWrite))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() resource isn't allowed to be read by shader"
				))

			if(!(tex.resource.flags & EGraphicsResourceFlag_ShaderWrite) && !!(b.registerType & ESHRegisterType_IsWrite))
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() resource isn't allowed to be written by shader"
				))

			//Range check

			if(d.texture.mipId >= tex.levels || d.texture.mipId + d.texture.mipCount > tex.levels)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() texture specifies out of range mip"
				))

			if(d.texture.arrayId >= tex.length || d.texture.arrayId + d.texture.arrayCount > tex.length)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() texture specifies out of range array slice"
				))

			if(d.texture.imageId >= tex.images)
				retError(clean, Error_invalidParameter(
					3, 0, "DescriptorTableRef_setDescriptor() texture specifies out of range imageId"
				))

			if(!!d.texture.planeId && (!isDepthBuffer || !hasStencil))
				retError(clean, Error_invalidParameter(
					3, 0,
					"DescriptorTableRef_setDescriptor() texture specifies planeId > 0 but it doesn't have a stencil buffer"
				))

			//Only arrays are allowed to have arrayCount and arrayId (excluding 3D textures and cubes)

			Bool isArrayType = isArray || type == ESHRegisterType_TextureCube || type == ESHRegisterType_Texture3D;

			if(!isArrayType && (d.texture.arrayId || (d.texture.arrayCount != tex.length && d.texture.arrayCount)))
				retError(clean, Error_invalidParameter(
					3, 0,
					"DescriptorTableRef_setDescriptor() texture specifies arrayCount and/or arrayId, "
					"but it's not an array, 3D or cube"
				))

			//Same thing as reading all TextureDescriptor values; Texture2DMS isn't allowed to specify anything
			// nor used as a regular texture

			if (tex.sampleCount && (
				d.texture.mipId || d.texture.planeId || d.texture.imageId ||
				(d.texture.mipCount != tex.levels && d.texture.mipCount)
			))
				retError(clean, Error_unsupportedOperation(
					0, "DescriptorTableRef_setDescriptor() (RW)Texture2DMS doesn't support specifying a subresource"
				))

			if(tex.sampleCount && type != ESHRegisterType_Texture2DMS)
				retError(clean, Error_unsupportedOperation(
					0, "DescriptorTableRef_setDescriptor() (RW)Texture2DMS can't be used as a regular texture"
				))

			//Default the arrayCount to the right size

			if(isArrayType && !d.texture.arrayCount)
				d.texture.arrayCount = tex.length - d.texture.arrayId;

			//TextureCube validation

			if(type == ESHRegisterType_TextureCube) {

				if(!!(d.texture.arrayCount % 6) || !d.texture.arrayCount)
					retError(clean, Error_invalidState(
						0, "DescriptorTableRef_setDescriptor() TextureCube(Array) needs to specify a multiple of 6 arrayCount"
					))

				if(!isArray && (d.texture.arrayCount != 6 || d.texture.arrayId))
					retError(clean, Error_invalidState(
						0, "DescriptorTableRef_setDescriptor() TextureCube has size != 6 or arrayId != 0"
					))
			}

			//Only Texture2D(Array) can have a planeId

			if(type != ESHRegisterType_Texture2D && d.texture.planeId)
				retError(clean, Error_invalidState(
					0, "DescriptorTableRef_setDescriptor() Texture2D(Array) is the only one permitted to have a planeId"
				))

			//Validate SRV

			if (!isWrite) {
				if(!d.texture.mipCount)
					d.texture.mipCount = tex.levels - d.texture.mipId;
			}

			//Validate UAV

			else {

				if(d.texture.mipCount > 1)
					retError(clean, Error_invalidState(
						0,
						"DescriptorTableRef_setDescriptor() RWTextures aren't allowed to have more than 1 mip selected"
					))

				if(!d.texture.mipCount)
					d.texture.mipCount = 1;

				if(tex.sampleCount && !(device->info.capabilities.features & EGraphicsFeatures_WriteMSTexture))
					retError(clean, Error_unsupportedOperation(
						0, "DescriptorTableRef_setDescriptor() RWTexture2DMS is unsupported on this device"
					))
			}

			break;
		}

		default:
			retError(clean, Error_invalidParameter(3, 0, "DescriptorTableRef_setDescriptor()::resource is incompatible"))
	}

skipValidation:
	gotoIfError3(clean, DescriptorTable_setDescriptorExt(tablePtr, i, arrayId, d, e_rr))

clean:
	return s_uccess;
}

U64 DescriptorTableRef_resolveRegisterName(DescriptorTableRef *table, CharString registerName) {

	if(!table || table->typeId != (ETypeId) EGraphicsTypeId_DescriptorTable)
		return U64_MAX;

	DescriptorLayoutInfo info = DescriptorLayoutRef_ptr(DescriptorTableRef_ptr(table)->layout)->info;

	for(U64 i = 0; i < info.bindingNames.length; ++i)
		if(CharString_equalsStringSensitive(info.bindingNames.ptr[i], registerName))
			return i;

	return U64_MAX;
}

Bool DescriptorTableRef_setDescriptorByName(
	DescriptorTableRef *table,
	CharString registerName,
	U64 arrayId,
	Descriptor d,
	Error *e_rr
) {

	Bool s_uccess = true;
	U64 binding = DescriptorTableRef_resolveRegisterName(table, registerName);

	if(binding == U64_MAX)
		retError(clean, Error_notFound(0, 1, "DescriptorTableRef_setDescriptorByName register not found"))

	gotoIfError3(clean, DescriptorTableRef_setDescriptor(table, binding, arrayId, d, e_rr))

clean:
	return s_uccess;
}
