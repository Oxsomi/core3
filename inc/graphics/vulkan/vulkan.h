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

#pragma once
#include "types/container/list.h"
#include "graphics/generic/device_buffer.h"
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

#ifndef GRAPHICS_API_DYNAMIC
	#define VK_WRAP_FUNC(name) name##Ext
#else
	#define VK_WRAP_FUNC(name) Vk##name
#endif

#define VkAccessFlagBits2_WRITE (							\
	VK_ACCESS_2_SHADER_WRITE_BIT |							\
	VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |				\
	VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |		\
	VK_ACCESS_2_TRANSFER_WRITE_BIT |						\
	VK_ACCESS_2_HOST_WRITE_BIT |							\
	VK_ACCESS_2_MEMORY_WRITE_BIT |							\
	VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR |				\
	VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR |				\
	VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR |				\
	VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |			\
	VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT |	\
	VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV |			\
	VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |		\
	VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT |					\
	VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV)

typedef struct VkUnifiedTexture {
	VkImage image;
	VkImageView view;
	VkPipelineStageFlagBits2 lastStage;
	VkAccessFlagBits2 lastAccess;
	VkImageLayout lastLayout;
} VkUnifiedTexture;

typedef enum ECompareOp ECompareOp;

typedef struct VkBLAS {
	VkAccelerationStructureGeometryKHR geometry;
	VkAccelerationStructureBuildGeometryInfoKHR geometries;
	VkAccelerationStructureBuildRangeInfoKHR range;
	VkAccelerationStructureKHR as;
} VkBLAS;

typedef struct VkTLAS {
	VkAccelerationStructureGeometryKHR geometry;
	VkAccelerationStructureBuildGeometryInfoKHR geometries;
	VkAccelerationStructureBuildRangeInfoKHR range;
	VkAccelerationStructureKHR as;
} VkTLAS;

static const U32 raytracingShaderIdSize = 32;
static const U32 raytracingShaderAlignment = 64;

TList(VkMappedMemoryRange);
TList(VkBufferCopy);
TList(VkImageCopy);
TList(VkBufferImageCopy);
TList(VkImageMemoryBarrier2);
TList(VkBufferMemoryBarrier2);
TList(VkPipeline);

Error vkCheck(VkResult result);

//Pass types as non NULL to allow validating if the texture format is supported.
//Sometimes you don't want this, for example when serializing.
VkFormat mapVkFormat(ETextureFormat format);

VkCompareOp mapVkCompareOp(ECompareOp op);

VkDeviceAddress getVkDeviceAddress(DeviceData data);
VkDeviceOrHostAddressConstKHR getVkLocation(DeviceData data, U64 localOffset);

//Transitions entire resource rather than sub-resources

Error VkUnifiedTexture_transition(
	VkUnifiedTexture *image,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	VkImageLayout layout,
	U32 graphicsQueueId,
	const VkImageSubresourceRange *range,
	ListVkImageMemoryBarrier2 *imageBarriers,
	VkDependencyInfo *dependency
);
