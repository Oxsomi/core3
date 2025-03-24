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
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_buffer.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/container/string.h"

void VkDescriptor_loseRef(RefPtr *resource, TextureDescriptorRange texture) {

	if(!resource)
		return;

	UnifiedTexture tex = TextureRef_getUnifiedTexture(resource, NULL);
	VkUnifiedTexture *texExt = TextureRef_getImgExtT(resource, Vk, 0, texture.imageId);

	SpinLock *lock = &texExt->lock;
	ELockAcquire acq = SpinLock_lock(lock, 1 * SECOND);
	Error *e_rr = NULL;
	Bool s_uccess = true;

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "VkDescriptor_loseRef() couldn't acquire lock"))

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(tex.resource.device), Vk);

	U64 dat = 0;
	Buffer_memcpy(Buffer_createRef(&dat, sizeof(dat)), Buffer_createRefConst(&texture, sizeof(texture)));

	for (U64 viewId = 0; viewId < texExt->views.length; ++viewId)
		if (texExt->views.ptr[viewId].textureDescU64 == dat) {

			U64 refCnt = --texExt->views.ptrNonConst[viewId].refCount;

			if (!refCnt) {
				deviceExt->destroyImageView(deviceExt->device, texExt->views.ptr[viewId].view, NULL);
				texExt->views.ptrNonConst[viewId].view = NULL;		//Can't pop it, others might reference beyond this
			}

			break;
		}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	(void) s_uccess;
}

void VkDescriptorTable_loseRef(DescriptorTable *table, U64 i, U64 j) {

	DescriptorLayout *layout = DescriptorLayoutRef_ptr(table->layout);

	DescriptorBinding layoutBind = layout->info.bindings.ptr[i];
	DescriptorTableBinding tableBind = table->bindings.ptr[i];

	ESHRegisterType type = layoutBind.registerType & ESHRegisterType_TypeMask;

	//Clean up the views

	if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd) {

		if(layoutBind.count > 1)
			VkDescriptor_loseRef(tableBind.multiple.resources.ptr[j], tableBind.multiple.textures.ptr[j]);

		else VkDescriptor_loseRef(tableBind.single.resource, tableBind.single.texture);
	}
}

Bool VK_WRAP_FUNC(DescriptorTable_free)(DescriptorTable *table, Allocator alloc) {

	(void) alloc;

	DescriptorHeap *heap = DescriptorHeapRef_ptr(table->parent);
	VkDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Vk);
	VkDescriptorTable *tableExt = DescriptorTable_ext(table, Vk);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(heap->device), Vk);
	DescriptorLayout *layout = DescriptorLayoutRef_ptr(table->layout);

	U32 count = 0;

	for(; count < 4 && tableExt->sets[count]; ++count)
		;

	if(count)
		deviceExt->freeDescriptorSets(deviceExt->device, heapExt->pool, count, tableExt->sets);

	for (U64 i = 0; i < tableExt->ranges.length; ++i) {

		VkDescriptorTableRange *range = &tableExt->ranges.ptrNonConst[i];

		ESHRegisterType type = layout->info.bindings.ptr[i].registerType & ESHRegisterType_TypeMask;

		if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
			ListVkDescriptorImageInfo_freex(&range->updateImages);

		else if(type >= ESHRegisterType_BufferStart && type < ESHRegisterType_BufferEnd)
			ListVkDescriptorBufferInfo_freex(&range->updateBuffers);

		else if(type == ESHRegisterType_AccelerationStructure)
			ListVkAccelerationStructureKHR_freex(&range->tlases);

		else if(type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState)
			ListVkDescriptorImageInfo_freex(&range->updateImages);

		for(U64 j = 0; j < range->views.length; ++j) {

			U64 oldLoc = range->views.ptr[j];

			if (oldLoc)
				VkDescriptorTable_loseRef(table, i, j);
		}

		ListU32_freex(&range->views);
		ListU32_freex(&range->newViews);
	}

	ListVkDescriptorTableRange_freex(&tableExt->ranges);

	return true;
}

Error VK_WRAP_FUNC(DescriptorHeap_createDescriptorTable)(DescriptorHeapRef *heapRef, DescriptorTable *table, CharString name) {

	Error err = Error_none();
	CharString tmpName = CharString_createNull();

	const DescriptorHeap *heap = DescriptorHeapRef_ptr(heapRef);
	VkDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Vk);
	const GraphicsDevice *device = GraphicsDeviceRef_ptr(heap->device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	DescriptorLayout *layout = DescriptorLayoutRef_ptr(table->layout);

	VkDescriptorTable *tableExt = DescriptorTable_ext(table, Vk);
	VkDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Vk);

	U32 count = 1;

	for(; count < 4 && layoutExt->layouts[count]; ++count)
		;

	VkDescriptorSetAllocateInfo allocInfo = (VkDescriptorSetAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = heapExt->pool,
		.descriptorSetCount = count,
		.pSetLayouts = layoutExt->layouts
	};

	gotoIfError(clean, checkVkError(deviceExt->allocateDescriptorSets(deviceExt->device, &allocInfo, tableExt->sets)))
	gotoIfError(clean, ListVkDescriptorTableRange_resizex(&tableExt->ranges, layout->info.bindings.length))

	for (U64 i = 0; i < tableExt->ranges.length; ++i) {

		ESHRegisterType type = layout->info.bindings.ptr[i].registerType & ESHRegisterType_TypeMask;

		if(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd)
			gotoIfError(clean, ListU32_resizex(&tableExt->ranges.ptrNonConst[i].views, layout->info.bindings.ptr[i].count))
	}

	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	for (U8 i = 0; i < count; ++i)
		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name) && instanceExt->debugSetName) {

			gotoIfError(clean, CharString_formatx(&tmpName, "%.*s set %"PRIu8, (int) CharString_length(name), name.ptr, i))

			const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
				.pObjectName = tmpName.ptr,
				.objectHandle = (U64) tableExt->sets[i]
			};

			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
			CharString_freex(&tmpName);
		}

	//Prepare bind command(s)
	//We should sort the set ids, just in case it's 0,2,1 for example (that'd generate 2 bind commands)

	U32 sets[4] = { layoutExt->setIds[0], layoutExt->setIds[1], layoutExt->setIds[2], layoutExt->setIds[3] };
	ListU32 sortSets = (ListU32) { 0 };
	gotoIfError(clean, ListU32_createRef(sets, count, &sortSets))
	ListU32_sort(sortSets);

	U32 prevSetId = U32_MAX;

	for(U8 i = 0; i < count; ++i) {

		//If we're a subsequent id, we can simply increase that bind command

		U32 nextSetId = sets[i];

		if (prevSetId == U32_MAX || nextSetId != prevSetId + 1) {
			tableExt->offsets[tableExt->bindCommands] = nextSetId;
			++tableExt->bindCommands;
		}

		++tableExt->counts[tableExt->bindCommands - 1];
		prevSetId = nextSetId;
	}

clean:
	CharString_freex(&tmpName);
	return err;
}

Bool VK_WRAP_FUNC(DescriptorTable_unsetDescriptors)(
	DescriptorTable *table,
	U64 bindId,
	U64 arrayId,
	U64 count,
	Error *e_rr
) {

	(void) e_rr;

	DescriptorLayout *layout = DescriptorLayoutRef_ptr(table->layout);
	ESHRegisterType type = layout->info.bindings.ptr[bindId].registerType & ESHRegisterType_TypeMask;

	//unsetDescriptors can free views, but only textures have views

	if(!(type >= ESHRegisterType_TextureStart && type < ESHRegisterType_TextureEnd))
		return true;

	for(U64 i = 0; i < count; ++i) 
		VkDescriptorTable_loseRef(table, bindId, arrayId + i);

	return true;
}

Bool VK_WRAP_FUNC(DescriptorTable_setDescriptors)(
	DescriptorTable *table,
	U64 bindId,
	U64 arrayId,
	ListDescriptor darr,
	Error *e_rr
) {

	const DescriptorHeap *heap = DescriptorHeapRef_ptr(table->parent);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(heap->device), Vk);

	VkDescriptorTable *tableExt = DescriptorTable_ext(table, Vk);
	DescriptorLayout *layout = DescriptorLayoutRef_ptr(table->layout);
	VkDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Vk);
	ListDescriptorBinding bindings = layout->info.bindings;
	DescriptorBinding binding = bindings.ptr[bindId];

	ESHRegisterType type = binding.registerType & ESHRegisterType_TypeMask;
	Bool isWrite = binding.registerType & ESHRegisterType_IsWrite;
	Bool isArrayType = binding.registerType & ESHRegisterType_IsArray;

	VkDescriptorSet set = NULL;
	CharString temp = CharString_createNull();
	Bool allocatedNewViews = false;

	for(U8 i = 0; i < 4; ++i)
		if (layoutExt->setIds[i] == binding.binding.space) {
			set = tableExt->sets[i];
			break;
		}

	Bool s_uccess = true;

	VkWriteDescriptorSet descriptor = (VkWriteDescriptorSet) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding = binding.binding.binding,
		.dstArrayElement = (U32) arrayId,
		.dstSet = set,
		.descriptorCount = (U32) darr.length
	};

	VkWriteDescriptorSetAccelerationStructureKHR tlasDesc = (VkWriteDescriptorSetAccelerationStructureKHR) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
	};

	VkDescriptorTableRange *range = &tableExt->ranges.ptrNonConst[bindId];

	union {
		VkDescriptorBufferInfo buffer;
		VkDescriptorImageInfo image;
		VkAccelerationStructureKHR tlas;
	} tmpDesc;

	U32 newViewId = 0;

	//Note: even though d.resource is properly guarded everywhere, it can only be used with robustness.
	//		This extension is poorly supported everywhere, so can't use that. Maybe some day.

	switch (type) {

		case ESHRegisterType_ConstantBuffer: {

			if(darr.length > 1)
				gotoIfError2(clean, ListVkDescriptorBufferInfo_resizex(&range->updateBuffers, darr.length))

			VkDescriptorBufferInfo *buf = darr.length > 1 ? range->updateBuffers.ptrNonConst : &tmpDesc.buffer;

			for(U64 i = 0; i < darr.length; ++i) {

				Descriptor d = darr.ptr[i];
				
				if(d.resource && !Descriptor_endBuffer(d)) {
					U64 len = DeviceBufferRef_ptr(d.resource)->resource.size;
					d.buffer.endRegionAndCounterOffset.region48 |= len - Descriptor_startBuffer(d);
				}

				if (d.resource)
					buf[i] = (VkDescriptorBufferInfo) {
						.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(d.resource), Vk)->buffer,
						.offset = Descriptor_startBuffer(d),
						.range = Descriptor_bufferLength(d)
					};

				else buf[i] = (VkDescriptorBufferInfo) { 0 };
			}

			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor.pBufferInfo = buf;
			break;
		}

		case ESHRegisterType_Sampler:
		case ESHRegisterType_SamplerComparisonState: {

			if(darr.length > 1)
				gotoIfError2(clean, ListVkDescriptorImageInfo_resizex(&range->updateImages, darr.length))

			VkDescriptorImageInfo *img = darr.length > 1 ? range->updateImages.ptrNonConst : &tmpDesc.image;

			for(U64 i = 0; i < darr.length; ++i) {

				Descriptor d = darr.ptr[i];

				if (d.resource)
					img[i] = (VkDescriptorImageInfo) { .sampler = *Sampler_ext(SamplerRef_ptr(d.resource), Vk) };

				else img[i] = (VkDescriptorImageInfo) { 0 };
			}

			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			descriptor.pImageInfo = img;
			break;
		}

		case ESHRegisterType_AccelerationStructure: {

			if(darr.length > 1)
				gotoIfError2(clean, ListVkAccelerationStructureKHR_resizex(&range->tlases, darr.length))

			VkAccelerationStructureKHR *tlas = darr.length > 1 ? range->tlases.ptrNonConst : &tmpDesc.tlas;

			for(U64 i = 0; i < darr.length; ++i) {

				Descriptor d = darr.ptr[i];

				if (d.resource)
					tlas[i] = (VkAccelerationStructureKHR) { TLAS_ext(TLASRef_ptr(d.resource), Vk)->as };

				else tlas[i] = (VkAccelerationStructureKHR) { 0 };
			}

			tlasDesc.accelerationStructureCount = (U32) darr.length;
			tlasDesc.pAccelerationStructures = tlas;

			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			descriptor.pNext = &tlasDesc;
			break;
		}

		case ESHRegisterType_ByteAddressBuffer:
		case ESHRegisterType_StorageBuffer:
		case ESHRegisterType_StorageBufferAtomic:
		case ESHRegisterType_StructuredBuffer:
		case ESHRegisterType_StructuredBufferAtomic: {

			if(darr.length > 1)
				gotoIfError2(clean, ListVkDescriptorBufferInfo_resizex(&range->updateBuffers, darr.length))

			VkDescriptorBufferInfo *buf = darr.length > 1 ? range->updateBuffers.ptrNonConst : &tmpDesc.buffer;

			for(U64 i = 0; i < darr.length; ++i) {

				Descriptor d = darr.ptr[i];
				
				if(d.resource && !Descriptor_endBuffer(d)) {
					U64 len = DeviceBufferRef_ptr(d.resource)->resource.size;
					d.buffer.endRegionAndCounterOffset.region48 |= len - Descriptor_startBuffer(d);
				}

				if(d.buffer.counter)
					retError(clean, Error_unimplemented(
						0, "DescriptorTable_setDescriptor: No support for vulkan atomic storage buffers yet"
					))

				if (d.resource)
					buf[i] = (VkDescriptorBufferInfo) {
						.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(d.resource), Vk)->buffer,
						.offset = Descriptor_startBuffer(d),
						.range = Descriptor_bufferLength(d)
					};

				else buf[i] = (VkDescriptorBufferInfo) { 0 };
			}

			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptor.pBufferInfo = buf;
			break;
		}

		case ESHRegisterType_Texture1D:
		case ESHRegisterType_Texture2D:
		case ESHRegisterType_Texture3D:
		case ESHRegisterType_TextureCube:
		case ESHRegisterType_Texture2DMS: {

			if(darr.length > 1) {
				gotoIfError2(clean, ListVkDescriptorImageInfo_resizex(&range->updateImages, darr.length))
				gotoIfError2(clean, ListU32_resizex(&range->newViews, darr.length))
			}

			VkDescriptorImageInfo *img = darr.length > 1 ? range->updateImages.ptrNonConst : &tmpDesc.image;
			U32 *newViews = darr.length > 1 ? range->newViews.ptrNonConst : &newViewId;

			for(U64 i = 0; i < darr.length; ++i)
				newViews[i] = U32_MAX;

			allocatedNewViews = true;

			for(U64 i = 0; i < darr.length; ++i) {

				Descriptor d = darr.ptr[i];

				if(d.resource) {

					UnifiedTexture tex = TextureRef_getUnifiedTexture(d.resource, NULL);

					if(isArrayType && !d.texture.arrayCount)
						d.texture.arrayCount = tex.length - d.texture.arrayId;

					if (!isWrite) {
						if(!d.texture.mipCount)
							d.texture.mipCount = tex.levels - d.texture.mipId;
					}

					else if(!d.texture.mipCount)
						d.texture.mipCount = 1;
				}

				VkImageView view = NULL;
				gotoIfError3(clean, VkUnifiedTexture_getView(darr.ptr[i], binding.registerType, &view, &newViews[i], e_rr))

				//Turn view into descriptor

				if (d.resource)
					img[i] = (VkDescriptorImageInfo) {
						.imageView = view,
						.imageLayout = isWrite ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};

				else img[i] = (VkDescriptorImageInfo) { 0 };
			}
			
			descriptor.descriptorType = isWrite ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			descriptor.pImageInfo = img;

			allocatedNewViews = false;		//Success, no need to free the views

			for(U64 i = 0; i < darr.length; ++i)
				tableExt->ranges.ptr[bindId].views.ptrNonConst[arrayId + i] = range->newViews.ptr[i];

			break;
		}

		case ESHRegisterType_SubpassInput:				//Doesn't do anything
		default:
			descriptor.descriptorCount = 0;
			break;
	}

	if(descriptor.descriptorCount)
		deviceExt->updateDescriptorSets(deviceExt->device, 1, &descriptor, 0, NULL);

clean:
		
	if (allocatedNewViews) {		//Release temporary views if invalid

		U32 *newViews = darr.length > 1 ? range->newViews.ptrNonConst : &newViewId;

		ELockAcquire acq = ELockAcquire_Invalid;
		SpinLock *lock = NULL;

		for(U64 i = 0; i < darr.length; ++i) {

			Descriptor d = darr.ptr[i];

			if (!d.resource)
				continue;

			VkUnifiedTexture *texExt = TextureRef_getImgExtT(d.resource, Vk, 0, d.texture.imageId);

			if(!lock) {

				lock = &texExt->lock;
				acq = SpinLock_lock(lock, 1 * SECOND);

				if(acq < ELockAcquire_Success) {
					Log_warnLnx("Couldn't free view while cleaning up (%"PRIu64")", i);
					continue;
				}
			}

			U64 refCnt = --texExt->views.ptrNonConst[newViews[i]].refCount;

			if (!refCnt) {
				deviceExt->destroyImageView(deviceExt->device, texExt->views.ptr[newViews[i]].view, NULL);
				texExt->views.ptrNonConst[newViews[i]].view = NULL;
			}

			Bool releaseLock = true;

			if (i + 1 < darr.length) {
				Descriptor dnext = darr.ptr[i + 1];
				releaseLock = dnext.resource != d.resource || dnext.texture.imageId != d.texture.imageId;
			}

			if(releaseLock) {

				if(acq == ELockAcquire_Acquired)
					SpinLock_unlock(lock);

				acq = ELockAcquire_Invalid;
				lock = NULL;
			}
		}
	}

	CharString_freex(&temp);
	return s_uccess;
}
