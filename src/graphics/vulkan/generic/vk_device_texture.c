/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "graphics/generic/device_texture.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_buffer.h"
#include "formats/texture.h"
#include "types/ref_ptr.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"

Error DeviceTextureRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending) {

	VkCommandBuffer commandBuffer = (VkCommandBuffer) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

	DeviceTexture *texture = DeviceTextureRef_ptr(pending);
	VkUnifiedTexture *textureExt = TextureRef_getCurrImgExtT(pending, Vk, 0);

	Error err = Error_none();

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];
	DeviceBufferRef *tempStagingResource = NULL;

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

	gotoIfError(clean, ListVkBufferImageCopy_resizex(&deviceExt->bufferImageCopyRanges, texture->pendingChanges.length))

	VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	gotoIfError(clean, ListVkBufferMemoryBarrier2_reservex(&deviceExt->bufferTransitions, 2 + texture->pendingChanges.length))

	U8 alignmentX = 1, alignmentY = 1;
	ETextureFormat_getAlignment(format, &alignmentX, &alignmentY);

	VkImageSubresourceLayers range = (VkImageSubresourceLayers) { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT };

	if (allocRange >= 16 * MIBI) {		//Resource is too big, allocate dedicated staging resource

		gotoIfError(clean, GraphicsDeviceRef_createBuffer(
			deviceRef,
			EDeviceBufferUsage_None, EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
			CharString_createRefCStrConst("Dedicated staging buffer"),
			allocRange, &tempStagingResource
		))

		DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
		VkDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Vk);
		U8 *location = stagingResource->resource.mappedMemoryExt;

		DeviceMemoryBlock block = device->allocator.blocks.ptr[stagingResource->resource.blockId];
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
				Buffer_copy(
					Buffer_createRef(location + allocRange, rowLen * h * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width)
					Buffer_copy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (k - z) * h, rowLen * h),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (k - z) * h, rowLen * h)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					const U64 yOff = (j - y) / alignmentY;
					Buffer_copy(
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

			vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
		}

		gotoIfError(clean, VkDeviceBuffer_transition(
			stagingResourceExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_READ_BIT,
			graphicsQueueId,
			0,
			allocRange,
			&deviceExt->bufferTransitions,
			&dependency
		))

		VkImageSubresourceRange range2 = (VkImageSubresourceRange) {		//TODO: Add range
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		};

		gotoIfError(clean, VkUnifiedTexture_transition(
			textureExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			graphicsQueueId,
			&range2,
			&deviceExt->imageTransitions,
			&dependency
		))

		if(dependency.bufferMemoryBarrierCount || dependency.imageMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		vkCmdCopyBufferToImage(
			commandBuffer,
			stagingResourceExt->buffer,
			textureExt->image,
			textureExt->lastLayout,
			(U32) deviceExt->bufferImageCopyRanges.length,
			deviceExt->bufferImageCopyRanges.ptr
		);

		//When staging resource is committed to current in flight then we can relinquish ownership.

		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempStagingResource))
		tempStagingResource = NULL;
	}

	//Use staging buffer

	else {

		gotoIfError(clean, ListVkBufferImageCopy_resizex(&deviceExt->bufferImageCopyRanges, texture->pendingChanges.length))

		AllocationBuffer *stagingBuffer = &device->stagingAllocations[(device->submitId - 1) % 3];
		DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
		VkDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Vk);

		U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
		Error temp = AllocationBuffer_allocateBlockx(
			stagingBuffer, allocRange, compressed ? 16 : 4, (const U8**) &location
		);

		if(temp.genericError && location == defaultLocation)		//Something major went wrong
			gotoIfError(clean, temp)

		//We re-create the staging buffer to fit the new allocation.

		if (temp.genericError) {

			U64 prevSize = DeviceBufferRef_ptr(device->staging)->resource.size;

			//Allocate new staging buffer.

			U64 newSize = prevSize * 2 + allocRange * 3;
			gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize))
			gotoIfError(clean, AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location))

			staging = DeviceBufferRef_ptr(device->staging);
			stagingExt = DeviceBuffer_ext(staging, Vk);
		}

		DeviceMemoryBlock block = device->allocator.blocks.ptr[staging->resource.blockId];
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
				Buffer_copy(
					Buffer_createRef(location + allocRange, rowLen * h2 * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h2 * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->base.width)
					Buffer_copy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (k - z) * h2, rowLen * h2),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (k - z) * h2, rowLen * h2)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					U64 yOff = (j - y) / alignmentY;
					Buffer_copy(
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

			vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
		}

		if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

			gotoIfError(clean, VkDeviceBuffer_transition(						//Ensure resource is transitioned
				stagingExt,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT,
				graphicsQueueId,
				((device->submitId - 1) % 3) * (staging->resource.size / 3),
				staging->resource.size / 3,
				&deviceExt->bufferTransitions,
				&dependency
			))

			RefPtr_inc(device->staging);
			gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, device->staging))		//Add to in flight
		}

		VkImageSubresourceRange range2 = (VkImageSubresourceRange) {		//TODO: Add range
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		};

		gotoIfError(clean, VkUnifiedTexture_transition(
			textureExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			graphicsQueueId,
			&range2,
			&deviceExt->imageTransitions,
			&dependency
		))

		if(dependency.bufferMemoryBarrierCount || dependency.imageMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		vkCmdCopyBufferToImage(
			commandBuffer,
			stagingExt->buffer,
			textureExt->image,
			textureExt->lastLayout,
			(U32) deviceExt->bufferImageCopyRanges.length,
			deviceExt->bufferImageCopyRanges.ptr
		);
	}

	if(!(texture->base.resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_freex(&texture->cpuData);

	texture->isFirstFrame = texture->isPending = texture->isPendingFullCopy = false;
	gotoIfError(clean, ListDevicePendingRange_clear(&texture->pendingChanges))

	if(RefPtr_inc(pending))
		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))

	if (device->pendingBytes >= device->flushThreshold)
		gotoIfError(clean, VkGraphicsDevice_flush(deviceRef, commandBuffer))

clean:
	DeviceBufferRef_dec(&tempStagingResource);
	ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);
	ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);
	ListVkBufferImageCopy_clear(&deviceExt->bufferImageCopyRanges);
	return err;
}
