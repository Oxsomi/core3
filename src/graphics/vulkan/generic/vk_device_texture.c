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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_buffer.h"
#include "formats/texture.h"
#include "platforms/ref_ptr.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"

const U64 DeviceTextureExt_size = sizeof(VkManagedImage);

Error GraphicsDeviceRef_createTextureExt(GraphicsDeviceRef *deviceRef, DeviceTexture *texture, CharString name) {

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkManagedImage *textureExt = (VkManagedImage*) DeviceTexture_ext(texture, );
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	textureExt->readHandle = textureExt->writeHandle = U32_MAX;

	VkFormat vkFormat = mapVkFormat(ETextureFormatId_unpack[texture->textureFormatId]);
	Bool is3D = texture->type != ETextureType_2D;

	U32 depth = texture->type == ETextureType_3D ? texture->length : (texture->type == ETextureType_Cube ? 6 : 1);

	VkImageCreateInfo imageInfo = (VkImageCreateInfo) {

		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = is3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
		.format = vkFormat,
		.extent = (VkExtent3D) { .width = texture->width, .height = texture->height, .depth = depth },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,

		.usage =
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |		//TODO: Check for compressed images? vkCmdCopyBufferToImage requires this!
			(texture->usage & EDeviceTextureUsage_ShaderRead ? VK_IMAGE_USAGE_SAMPLED_BIT : 0),

		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	//Allocate memory

	VkDeviceImageMemoryRequirementsKHR imageReq = (VkDeviceImageMemoryRequirementsKHR) {
		.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR,
		.pCreateInfo = &imageInfo
	};

	VkMemoryDedicatedRequirements dedicatedReq = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
	};

	VkMemoryRequirements2 requirements = (VkMemoryRequirements2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = &dedicatedReq
	};

	instance->getDeviceImageMemoryRequirements(deviceExt->device, &imageReq, &requirements);

	_gotoIfError(clean, DeviceMemoryAllocator_allocate(
		&device->allocator,
		&requirements,
		false,
		&textureExt->blockId,
		&textureExt->blockOffset,
		EResourceType_DeviceTexture,
		name
	));

	DeviceMemoryBlock block = device->allocator.blocks.ptr[textureExt->blockId];

	VkImage *image = &textureExt->image;
	_gotoIfError(clean, vkCheck(vkCreateImage(deviceExt->device, &imageInfo, NULL, image)));

	_gotoIfError(clean, vkCheck(vkBindImageMemory(
		deviceExt->device, *image, (VkDeviceMemory) block.ext, textureExt->blockOffset
	)));
	
	if(CharString_length(name)) {

		#ifndef NDEBUG

			if(instance->debugSetName) {

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_IMAGE,
					.pObjectName = name.ptr,
					.objectHandle =  (U64) *image
				};

				_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));
			}

		#endif
	}

	//Image views

	//TODO: 1 UAV and SRV per face for cubemap (2D), 3D won't to avoid allocating too many (and doesn't need it).

	VkImageView *view = &textureExt->view;

	VkImageViewCreateInfo viewCreate = (VkImageViewCreateInfo) {

		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = textureExt->image,

		.viewType =
			texture->type == ETextureType_Cube ? VK_IMAGE_VIEW_TYPE_CUBE :
			(is3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D),

		.format = vkFormat,

		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
			.levelCount = 1
		}
	};

	_gotoIfError(clean, vkCheck(vkCreateImageView(deviceExt->device, &viewCreate, NULL, view)));
	
	if(CharString_length(name)) {

		#ifndef NDEBUG

			if(instance->debugSetName) {

				_gotoIfError(clean, CharString_formatx(&temp, "%.*s view", CharString_length(name), name.ptr));

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
					.pObjectName = temp.ptr,
					.objectHandle =  (U64) *view
				};

				_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));

				CharString_freex(&temp);
			}

		#endif
	}

	//Allocate in descriptors

	//TODO: Allocate descriptors (read & write) per face if cubemap

	if(texture->usage & EDeviceTextureUsage_ShaderRead) {

		acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			_gotoIfError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_createRenderTextureExt() couldn't acquire descriptor lock"
			));

		//Create readonly image

		EDescriptorType descType = is3D ? EDescriptorType_Texture3D : EDescriptorType_Texture2D;
		U32 locationRead = VkGraphicsDevice_allocateDescriptor(deviceExt, descType);

		if(locationRead == U32_MAX)
			_gotoIfError(clean, Error_outOfMemory(
				0, "GraphicsDeviceRef_createRenderTextureExt() couldn't allocate image descriptor"
			));

		texture->readHandle = textureExt->readHandle = locationRead;

		VkDescriptorImageInfo descriptorImageInfo = (VkDescriptorImageInfo) {
			.imageView = *view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		VkWriteDescriptorSet writeDescriptorSet = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = deviceExt->sets[EDescriptorSetType_Resources],
			.descriptorCount = 1,
			.pImageInfo = &descriptorImageInfo,
			.dstBinding = descType - 1,
			.dstArrayElement = locationRead & ((1 << 20) - 1),
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
		};

		vkUpdateDescriptorSets(deviceExt->device, 1, &writeDescriptorSet, 0, NULL);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);

		acq = ELockAcquire_Invalid;
	}

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&deviceExt->descriptorLock);
	
	CharString_freex(&temp);
	return err;
}

Bool DeviceTexture_freeExt(DeviceTexture *texture) {

	VkManagedImage *image = (VkManagedImage*) DeviceTexture_ext(texture, );
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(texture->device), Vk);

	if(image->readHandle != U32_MAX) {

		ELockAcquire acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {
			ListU32 allocationList = (ListU32) { 0 };
			ListU32_createRefConst(&image->readHandle, 1, &allocationList);
			VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);
	}

	if(image->view)
		vkDestroyImageView(deviceExt->device, image->view, NULL);

	if(image->image)
		vkDestroyImage(deviceExt->device, image->image, NULL);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->device);
	DeviceMemoryAllocator_freeAllocation(&device->allocator, image->blockId, image->blockOffset);

	return true;
}

Error DeviceTextureRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending) {

	VkCommandBuffer commandBuffer = (VkCommandBuffer) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

	DeviceTexture *texture = DeviceTextureRef_ptr(pending);
	VkManagedImage *textureExt = (VkManagedImage*) DeviceTexture_ext(texture, );

	Error err = Error_none();
	ListVkMappedMemoryRange tempMappedMemRanges = { 0 };
	ListVkBufferMemoryBarrier2 tempBufferBarriers = { 0 };
	ListVkImageMemoryBarrier2 tempImageBarriers = { 0 };

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];
	DeviceBufferRef *tempStagingResource = NULL;
	ListVkBufferImageCopy pendingCopies = { 0 };

	ETextureFormat format = ETextureFormatId_unpack[texture->textureFormatId];

	//TODO: Copy queue

	U64 allocRange = 0;

	if(!texture->isPendingFullCopy) {

		//Calculate allocation range

		for(U64 j = 0; j < texture->pendingChanges.length; ++j) {

			TextureRange texturej = texture->pendingChanges.ptr[j].texture;

			U64 siz = ETextureFormat_getSize(
				format, TextureRange_width(texturej), TextureRange_height(texturej), TextureRange_length(texturej)
			);

			allocRange += siz;
		}
	}

	else allocRange = texture->allocSize;

	device->pendingBytes += allocRange;

	_gotoIfError(clean, ListVkBufferImageCopy_resizex(&pendingCopies, texture->pendingChanges.length));

	VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	_gotoIfError(clean, ListVkBufferMemoryBarrier2_reservex(&tempBufferBarriers, 2 + texture->pendingChanges.length));

	U8 alignmentX = 1, alignmentY = 1;
	ETextureFormat_getAlignment(format, &alignmentX, &alignmentY);

	VkImageSubresourceLayers range = (VkImageSubresourceLayers) { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT };

	if (allocRange >= 16 * MIBI) {		//Resource is too big, allocate dedicated staging resource

		_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_CPUAllocatedBit, CharString_createRefCStrConst("Dedicated staging buffer"),
			allocRange, &tempStagingResource
		));

		DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
		VkDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Vk);
		U8 *location = stagingResource->mappedMemory;

		DeviceMemoryBlock block = device->allocator.blocks.ptr[stagingResource->blockId];
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//Copy into our buffer

		allocRange = 0;

		for(U64 m = 0; m < texture->pendingChanges.length; ++m) {

			TextureRange texturej = texture->pendingChanges.ptr[m].texture;

			U16 x = texturej.startRange[0];
			U16 y = texturej.startRange[1];
			U16 z = texturej.startRange[2];

			U16 w = TextureRange_width(texturej);
			U16 h = TextureRange_height(texturej);
			U16 l = TextureRange_length(texturej);

			U32 rowOff = (U32) ETextureFormat_getSize(format, x, alignmentY, 1);
			U32 rowLen = (U32) ETextureFormat_getSize(format, w, alignmentY, 1);
			U64 len = ETextureFormat_getSize(format, w, h, l);
			U64 start = (U64) rowLen * (y + z * h) + rowOff;

			if(w == texture->width && h == texture->height)
				Buffer_copy(
					Buffer_createRef(location + allocRange, rowLen * h * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->width)
					Buffer_copy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (k - z) * h, rowLen * h),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (k - z) * h, rowLen * h)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					U64 yOff = (j - y) / alignmentY;
					Buffer_copy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (yOff + (k - z) * h), rowLen),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (yOff + (k - z) * h), rowLen)
					);
				}
			}

			range.layerCount = l;

			pendingCopies.ptrNonConst[m] = (VkBufferImageCopy) {
				.bufferOffset = allocRange,
				.imageSubresource = range,
				.imageOffset = (VkOffset3D) { .x = x, .y = y, .z = z },
				.imageExtent = (VkExtent3D) { .width = w, .height = h, .depth = l }
			};

			allocRange += len;
		}

		if(incoherent) {

			VkMappedMemoryRange memoryRange = (VkMappedMemoryRange) {
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory) block.ext,
				.offset = stagingResource->blockOffset,
				.size = allocRange
			};

			vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
		}

		_gotoIfError(clean, VkDeviceBuffer_transition(
			stagingResourceExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_READ_BIT,
			graphicsQueueId,
			0,
			allocRange,
			&tempBufferBarriers,
			&dependency
		));

		VkImageSubresourceRange range2 = (VkImageSubresourceRange) {		//TODO: Add range
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		};

		_gotoIfError(clean, VkManagedImage_transition(
			textureExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			graphicsQueueId,
			&range2,
			&tempImageBarriers,
			&dependency
		));

		if(dependency.bufferMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		vkCmdCopyBufferToImage(
			commandBuffer,
			stagingResourceExt->buffer,
			textureExt->image,
			textureExt->lastLayout,
			(U32) pendingCopies.length,
			pendingCopies.ptr
		);

		//When staging resource is commited to current in flight then we can relinguish ownership.

		_gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempStagingResource));
		tempStagingResource = NULL;
	}

	//Use staging buffer

	else {

		_gotoIfError(clean, ListVkBufferImageCopy_resizex(&pendingCopies, texture->pendingChanges.length));

		AllocationBuffer *stagingBuffer = &device->stagingAllocations[device->submitId % 3];
		DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
		VkDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Vk);

		U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
		Error temp = AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location);

		if(temp.genericError && location == defaultLocation)		//Something major went wrong
			_gotoIfError(clean, temp);

		//We re-create the staging buffer to fit the new allocation.

		if (temp.genericError) {

			U64 prevSize = DeviceBufferRef_ptr(device->staging)->length;

			//Allocate new staging buffer.

			U64 newSize = prevSize * 2 + allocRange * 3;
			_gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize));
			_gotoIfError(clean, AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location));

			staging = DeviceBufferRef_ptr(device->staging);
			stagingExt = DeviceBuffer_ext(staging, Vk);
		}

		DeviceMemoryBlock block = device->allocator.blocks.ptr[staging->blockId];
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//Copy into our buffer

		allocRange = 0;

		for(U64 m = 0; m < texture->pendingChanges.length; ++m) {

			TextureRange texturej = texture->pendingChanges.ptr[m].texture;

			U16 x = texturej.startRange[0];
			U16 y = texturej.startRange[1];
			U16 z = texturej.startRange[2];

			U16 w = TextureRange_width(texturej);
			U16 h = TextureRange_height(texturej);
			U16 l = TextureRange_length(texturej);

			U32 rowLen = (U32) ETextureFormat_getSize(format, w, alignmentY, 1);
			U32 rowOff = (U32) ETextureFormat_getSize(format, x, alignmentY, 1);
			U64 len = ETextureFormat_getSize(format, w, h, l);
			U64 start = (U64) rowLen * (y + z * h) + rowOff;

			if(w == texture->width && h == texture->height)
				Buffer_copy(
					Buffer_createRef(location + allocRange, rowLen * h * l),
					Buffer_createRefConst(texture->cpuData.ptr + start, rowLen * h * l)
				);

			else for(U64 k = z; k < z + l; ++k) {

				if(w == texture->width)
					Buffer_copy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (k - z) * h, rowLen * h),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (k - z) * h, rowLen * h)
					);

				else for (U64 j = y; j < y + h; j += alignmentY) {
					U64 yOff = (j - y) / alignmentY;
					Buffer_copy(
						Buffer_createRef(location + allocRange + (U64)rowLen * (yOff + (k - z) * h), rowLen),
						Buffer_createRefConst(texture->cpuData.ptr + start + (U64)rowLen * (yOff + (k - z) * h), rowLen)
					);
				}
			}

			range.layerCount = l;

			pendingCopies.ptrNonConst[m] = (VkBufferImageCopy) {
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
				.offset = location - block.mappedMemory,
				.size = allocRange
			};

			vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
		}

		if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

			_gotoIfError(clean, VkDeviceBuffer_transition(						//Ensure resource is transitioned
				stagingExt,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT,
				graphicsQueueId,
				(device->submitId % 3) * (staging->length / 3),
				staging->length / 3,
				&tempBufferBarriers,
				&dependency
			));

			RefPtr_inc(device->staging);
			_gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, device->staging));		//Add to in flight
		}

		VkImageSubresourceRange range2 = (VkImageSubresourceRange) {		//TODO: Add range
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		};

		_gotoIfError(clean, VkManagedImage_transition(
			textureExt,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			graphicsQueueId,
			&range2,
			&tempImageBarriers,
			&dependency
		));

		if(dependency.bufferMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		vkCmdCopyBufferToImage(
			commandBuffer,
			stagingExt->buffer,
			textureExt->image,
			textureExt->lastLayout,
			(U32) pendingCopies.length,
			pendingCopies.ptr
		);
	}

	if(!(texture->usage & EDeviceTextureUsage_CPUBacked))
		Buffer_freex(&texture->cpuData);

	texture->isFirstFrame = texture->isPending = texture->isPendingFullCopy = false;
	_gotoIfError(clean, ListDevicePendingRange_clear(&texture->pendingChanges));

	if(RefPtr_inc(pending))
		_gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending));

	if (device->pendingBytes >= device->flushThreshold)
		VkGraphicsDevice_flush(deviceRef, commandBuffer);

clean:
	DeviceBufferRef_dec(&tempStagingResource);
	ListVkMappedMemoryRange_freex(&tempMappedMemRanges);
	ListVkBufferMemoryBarrier2_freex(&tempBufferBarriers);
	ListVkImageMemoryBarrier2_freex(&tempImageBarriers);
	ListVkBufferImageCopy_freex(&pendingCopies);
	return err;
}
