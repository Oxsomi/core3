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

//graphics/vulkan/generic/vk_device_texture.c

#include "graphics/generic/device_texture.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_buffer.h"
#include "types/container/texture_format.h"
#include "types/container/ref_ptr.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"
#include "types/base/constants.h"

Bool VK_WRAP_FUNC(DeviceTextureRef_flush)(
	void *commandBufferExt,
	GraphicsDeviceRef *deviceRef,
	DeviceTextureRef *pending,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	VkCommandBufferState *commandBuffer = (VkCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

	DeviceTexture *texture = DeviceTextureRef_ptr(pending);
	VkUnifiedTexture *textureExt = TextureRef_getCurrImgExtT(pending, Vk, 0);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];
	DeviceBufferRef *tempStagingResource = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	ETextureFormat format = ETextureFormatId_unpack[texture->base.textureFormatId];
	Bool compressed = ETextureFormat_getIsCompressed(format);

	//TODO: Copy queue

	U64 allocRange = 0;

	if(!texture->isPendingFullCopy) {

		//Calculate allocation range

		for(U64 j = 0; j < texture->pendingChanges.length; ++j) {

			const TextureRange texturej = texture->pendingChanges.ptr[j].texture;

			U64 siz = ETextureFormat_getSize(
				format, TextureRange_width(texturej), TextureRange_height(texturej), TextureRange_length(texturej)
			);

			allocRange += siz;
		}
	}

	else allocRange = texture->base.resource.size;

	device->pendingBytes += allocRange;

	gotoIfError3(clean, ListVkBufferImageCopy_resize(
		&deviceExt->bufferImageCopyRanges,
		texture->pendingChanges.length,
		alloc,
		e_rr
	));

	VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	gotoIfError3(clean, ListVkBufferMemoryBarrier2_reserve(
		&deviceExt->bufferTransitions,
		2 + texture->pendingChanges.length,
		alloc,
		e_rr
	));

	U8 alignmentX = 1, alignmentY = 1;
	ETextureFormat_getAlignment(format, &alignmentX, &alignmentY);

	VkImageSubresourceLayers range = (VkImageSubresourceLayers) { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT };

	if (allocRange >= DeviceBufferRef_ptr(device->staging)->resource.size / 4) {

		CharString dedicatedStagingName = CharString_createRefCStrConst("Dedicated staging buffer");

		gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
			deviceRef,
			EDeviceBufferUsage_None, EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
			NULL,
			&dedicatedStagingName,
			allocRange, &tempStagingResource, e_rr
		));

		DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
		VkDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Vk);
		U8 *location = stagingResource->resource.mappedMemoryExt;

		acq = SpinLock_lock(&device->allocator.lock, U64_MAX);

		DeviceMemoryBlock block = device->allocator.blocks.ptr[stagingResource->resource.blockId];

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->allocator.lock);

		acq = ELockAcquire_Invalid;

		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//Copy into our buffer

		allocRange = 0;

		for(U64 m = 0; m < texture->pendingChanges.length; ++m) {

			const TextureRange texturej = texture->pendingChanges.ptr[m].texture;

			const U16 x = texturej.startRange[0];
			const U16 y = texturej.startRange[1];
			const U16 z = texturej.startRange[2];

			const U16 w = TextureRange_width(texturej);
			const U16 h = TextureRange_height(texturej);
			const U16 l = TextureRange_length(texturej);

			const U64 rowOff = ETextureFormat_getSize(format, x, alignmentY, 1);
			const U64 rowLen = ETextureFormat_getSize(format, w, alignmentY, 1);
			const U64 len = ETextureFormat_getSize(format, w, h, l);
			const U64 start = rowLen * (y + z * h) + rowOff;

			if(w == texture->base.width && h == texture->base.height)
				Buffer_memcpy(
					Buffer_createRef(location + allocRange, rowLen * h * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width)
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (k - z) * h, rowLen * h),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (k - z) * h, rowLen * h)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					const U64 yOff = (j - y) / alignmentY;
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (yOff + (k - z) * h), rowLen),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (yOff + (k - z) * h), rowLen)
					);
				}
			}

			range.layerCount = l;

			deviceExt->bufferImageCopyRanges.ptrNonConst[m] = (VkBufferImageCopy) {
				.bufferOffset = allocRange,
				.imageSubresource = range,
				.imageOffset = (VkOffset3D) { .x = x, .y = y, .z = z },
				.imageExtent = (VkExtent3D) { .width = w, .height = h, .depth = l }
			};

			allocRange += len;
		}

		if(incoherent) {

			const VkMappedMemoryRange memoryRange = (VkMappedMemoryRange) {
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory) block.ext,
				.offset = stagingResource->resource.blockOffset,
				.size = allocRange
			};

			deviceExt->flushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
		}

		gotoIfError3(clean, VkDeviceBuffer_transition(
			stagingResourceExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_READ_BIT,
			graphicsQueueId,
			0,
			allocRange,
			&deviceExt->bufferTransitions,
			&dependency, alloc, e_rr));

		VkImageSubresourceRange range2 = (VkImageSubresourceRange) {        //TODO: Add range
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		};

		gotoIfError3(clean, VkUnifiedTexture_transition(
			textureExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			graphicsQueueId,
			&range2,
			&deviceExt->imageTransitions,
			&dependency, alloc, e_rr
		));

		if(dependency.bufferMemoryBarrierCount || dependency.imageMemoryBarrierCount)
			deviceExt->cmdPipelineBarrier2(commandBuffer->buffer, &dependency);

		deviceExt->cmdCopyBufferToImage(
			commandBuffer->buffer,
			stagingResourceExt->buffer,
			textureExt->image,
			textureExt->lastLayout,
			(U32) deviceExt->bufferImageCopyRanges.length,
			deviceExt->bufferImageCopyRanges.ptr
		);

		//When staging resource is committed to current in flight then we can relinquish ownership.

		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, tempStagingResource, alloc, e_rr));
		tempStagingResource = NULL;
	}

	//Use staging buffer

	else {

		gotoIfError3(clean, ListVkBufferImageCopy_resize(
			&deviceExt->bufferImageCopyRanges,
			texture->pendingChanges.length,
			alloc,
			e_rr
		));

		AllocationBuffer *stagingBuffer = &device->stagingAllocations[device->fifId];
		DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
		VkDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Vk);

		U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
		Error temp = Error_none();

		AllocationBufferAllocate allocBuffer = (AllocationBufferAllocate) {
			.allocationBuffer = stagingBuffer,
			.alignment = compressed ? 16 : 4,
			.alloc = alloc
		};

		Bool allocated = AllocationBuffer_allocateBlock(&allocBuffer, allocRange, (const U8**) &location, &temp);

		if(!allocated && location == defaultLocation) {        //Something major went wrong
			if(e_rr) *e_rr = temp;
			s_uccess = false;
			goto clean;
		}

		//We re-create the staging buffer to fit the new allocation.

		if (!allocated) {

			U64 prevSize = DeviceBufferRef_ptr(device->staging)->resource.size;

			//Allocate new staging buffer.

			U64 newSize = prevSize * 2 + allocRange * 3;
			gotoIfError3(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize, e_rr));

			allocBuffer = (AllocationBufferAllocate) {
				.allocationBuffer = stagingBuffer,
				.alignment = compressed ? 16 : 4,
				.alloc = alloc
			};

			gotoIfError3(clean, AllocationBuffer_allocateBlock(&allocBuffer, allocRange, (const U8**) &location, e_rr));

			staging = DeviceBufferRef_ptr(device->staging);
			stagingExt = DeviceBuffer_ext(staging, Vk);
		}

		acq = SpinLock_lock(&device->allocator.lock, U64_MAX);

		DeviceMemoryBlock block = device->allocator.blocks.ptr[staging->resource.blockId];

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->allocator.lock);

		acq = ELockAcquire_Invalid;

		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//Copy into our buffer

		allocRange = 0;

		for(U64 m = 0; m < texture->pendingChanges.length; ++m) {

			const TextureRange texturej = texture->pendingChanges.ptr[m].texture;

			U16 x = texturej.startRange[0];
			U16 y = texturej.startRange[1];
			U16 z = texturej.startRange[2];

			U16 w = TextureRange_width(texturej);
			U16 h = TextureRange_height(texturej);
			U16 l = TextureRange_length(texturej);

			U64 rowLen = ETextureFormat_getSize(format, w, alignmentY, 1);
			U64 rowOff = ETextureFormat_getSize(format, x, alignmentY, 1);
			U64 len = ETextureFormat_getSize(format, w, h, l);
			U64 h2 = h / alignmentY;
			U64 start = rowLen * (y + z * h2) + rowOff;

			if(w == texture->base.width && h == texture->base.height)
				Buffer_memcpy(
					Buffer_createRef(location + allocRange, rowLen * h2 * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h2 * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width)
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (k - z) * h2, rowLen * h2),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (k - z) * h2, rowLen * h2)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					U64 yOff = (j - y) / alignmentY;
					Buffer_memcpy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (yOff + (k - z) * h2), rowLen),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (yOff + (k - z) * h2), rowLen)
					);
				}
			}

			range.layerCount = l;

			deviceExt->bufferImageCopyRanges.ptrNonConst[m] = (VkBufferImageCopy) {
				.bufferOffset = allocRange + (location - stagingBuffer->buffer.ptr),
				.imageSubresource = range,
				.imageOffset = (VkOffset3D) { .x = x, .y = y, .z = z },
				.imageExtent = (VkExtent3D) { .width = w, .height = h, .depth = l }
			};

			allocRange += len;
		}

		if (incoherent) {

			VkMappedMemoryRange memoryRange = (VkMappedMemoryRange) {
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory) block.ext,
				.offset = location - block.mappedMemoryExt,
				.size = allocRange
			};

			deviceExt->flushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
		}

		if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

			gotoIfError3(clean, VkDeviceBuffer_transition(                        //Ensure resource is transitioned
				stagingExt,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT,
				graphicsQueueId,
				device->fifId * (staging->resource.size / device->framesInFlight),
				staging->resource.size / device->framesInFlight,
				&deviceExt->bufferTransitions,
				&dependency, alloc, e_rr
			));

			RefPtr_inc(device->staging);
			gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, device->staging, alloc, e_rr));        //Add to in flight
		}

		VkImageSubresourceRange range2 = (VkImageSubresourceRange) {        //TODO: Add range
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		};

		gotoIfError3(clean, VkUnifiedTexture_transition(
			textureExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			graphicsQueueId,
			&range2,
			&deviceExt->imageTransitions,
			&dependency, alloc, e_rr));

		if(dependency.bufferMemoryBarrierCount || dependency.imageMemoryBarrierCount)
			deviceExt->cmdPipelineBarrier2(commandBuffer->buffer, &dependency);

		deviceExt->cmdCopyBufferToImage(
			commandBuffer->buffer,
			stagingExt->buffer,
			textureExt->image,
			textureExt->lastLayout,
			(U32) deviceExt->bufferImageCopyRanges.length,
			deviceExt->bufferImageCopyRanges.ptr
		);
	}

	if(!(texture->base.resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_free(&texture->cpuData, alloc);

	texture->isFirstFrame = texture->isPending = texture->isPendingFullCopy = false;
	gotoIfError3(clean, ListDevicePendingRange_clear(&texture->pendingChanges, e_rr));

	if(RefPtr_inc(pending))
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));

	if (device->pendingBytes >= device->flushThreshold)
		gotoIfError3(clean, VkGraphicsDevice_flush(deviceRef, commandBuffer, e_rr));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->allocator.lock);

	RefPtr_dec(&tempStagingResource);
	ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions, e_rr);
	ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions, e_rr);
	ListVkBufferImageCopy_clear(&deviceExt->bufferImageCopyRanges, e_rr);
	return s_uccess;
}
