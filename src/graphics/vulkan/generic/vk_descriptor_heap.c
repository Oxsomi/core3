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

#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "types/container/string.h"

Bool VK_WRAP_FUNC(DescriptorHeap_free)(DescriptorHeap *heap) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(heap->device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	const VkDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Vk);

	if(heapExt->pool)
		deviceExt->destroyDescriptorPool(deviceExt->device, heapExt->pool, NULL);

	return true;
}

Error VK_WRAP_FUNC(GraphicsDeviceRef_createDescriptorHeap)(GraphicsDeviceRef *dev, DescriptorHeap *heap, CharString name) {

	Error err = Error_none();

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Vk);

	const DescriptorHeapInfo info = heap->info;

	VkDescriptorPoolSize poolSizes[8];
	U32 counter = 0;

	if(info.maxAccelerationStructures)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, info.maxAccelerationStructures };

	if(info.maxSamplers)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_SAMPLER, info.maxSamplers };

	if(info.maxInputAttachments)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, info.maxInputAttachments };

	if(info.maxCombinedSamplers)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, info.maxCombinedSamplers };

	if(info.maxTextures)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, info.maxTextures };

	if(info.maxTexturesRW)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, info.maxTexturesRW };

	if(info.maxBuffersRW)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, info.maxBuffersRW };

	if(info.maxConstantBuffers)
		poolSizes[counter++] = (VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, info.maxConstantBuffers };

	VkDescriptorPoolCreateInfo poolInfo = (VkDescriptorPoolCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = !!(info.flags & EDescriptorHeapFlags_AllowBindless) ? 0 : VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = info.maxDescriptorTables * 4,
		.poolSizeCount = counter,
		.pPoolSizes = poolSizes
	};

	gotoIfError(clean, checkVkError(deviceExt->createDescriptorPool(deviceExt->device, &poolInfo, NULL, &heapExt->pool)))

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name) && instanceExt->debugSetName) {

		const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL,
			.pObjectName = name.ptr,
			.objectHandle = (U64) heapExt->pool
		};

		gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
	}

clean:
	return err;
}
