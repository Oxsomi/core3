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

//graphics/vulkan/vulkan.h

#pragma once
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/descriptor_table.h"
#include "types/container/list.h"

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

#ifndef GRAPHICS_API_DYNAMIC
	#define VK_WRAP_FUNC(name) name##Ext
#else
	#define VK_WRAP_FUNC(name) Vk##name
#endif

#define VkAccessFlagBits2_WRITE (                           \
	VK_ACCESS_2_SHADER_WRITE_BIT |                          \
	VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |                \
	VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |        \
	VK_ACCESS_2_TRANSFER_WRITE_BIT |                        \
	VK_ACCESS_2_HOST_WRITE_BIT |                            \
	VK_ACCESS_2_MEMORY_WRITE_BIT |                          \
	VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR |              \
	VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR |                \
	VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR |                \
	VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |          \
	VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT |  \
	VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV |           \
	VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |      \
	VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT |                    \
	VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV                   \
)

TList(VkMappedMemoryRange);
TList(VkBufferCopy);
TList(VkImageCopy);
TList(VkBufferImageCopy);
TList(VkImageMemoryBarrier2);
TList(VkBufferMemoryBarrier2);
TList(VkPipeline);

TList(VkPipelineShaderStageCreateInfo);

typedef struct VkImageViewMapping {

	union {
		TextureDescriptorRange textureDesc;
		U64 textureDescU64;
	};

	VkImageView view;
	U64 refCount;

} VkImageViewMapping;

TList(VkImageViewMapping);

typedef struct VkUnifiedTexture {

	VkImage image;

	VkPipelineStageFlagBits2 lastStage;

	VkAccessFlagBits2 lastAccess;

	VkImageLayout lastLayout;
	U32 padding;

	ListVkImageViewMapping views;

	SpinLock lock;                    //To acquire views

} VkUnifiedTexture;

typedef struct Descriptor Descriptor;
typedef enum ESHRegisterType ESHRegisterType;

Bool VkUnifiedTexture_getView(Descriptor d, ESHRegisterType type, VkImageView *view, U32 *viewId, Error *e_rr);

typedef enum ECompareOp ECompareOp;

typedef struct VkBLAS {
	VkAccelerationStructureGeometryKHR geometry;
	VkAccelerationStructureBuildGeometryInfoKHR geometries;
	VkAccelerationStructureBuildRangeInfoKHR range;
	VkAccelerationStructureKHR as;
	U64 padding;
} VkBLAS;

TListNamed(VkMicromapUsageEXT, ListVkMicromapUsageEXT);

//The usages have to be re-supplied to every BLAS that links this micromap, so the API shaped copy is built
// once at create and kept, rather than rebuilt per BLAS.

typedef struct VkOpacityMicromap {
	VkMicromapBuildInfoEXT build;
	VkMicromapEXT micromap;
	ListVkMicromapUsageEXT usages;
} VkOpacityMicromap;

typedef struct VkTLAS {
	VkAccelerationStructureGeometryKHR geometry;
	VkAccelerationStructureBuildGeometryInfoKHR geometries;
	VkAccelerationStructureBuildRangeInfoKHR range;
	VkAccelerationStructureKHR as;
	U64 padding;
} VkTLAS;

static const U32 raytracingShaderIdSize = 32;
static const U32 raytracingShaderAlignment = 64;

Bool checkVkError(VkResult result, Error *e_rr);

VkFormat mapVkFormat(ETextureFormat format);

VkCompareOp mapVkCompareOp(ECompareOp op);

VkDeviceAddress getVkDeviceAddress(DeviceData data);

static inline VkDeviceOrHostAddressConstKHR getVkLocation(DeviceData data, U64 localOffset) {
	const VkDeviceOrHostAddressConstKHR ret = { .deviceAddress = getVkDeviceAddress(data) + localOffset };
	return ret;
}

VkShaderStageFlags vkGetShaderStages(U32 vis);

//Same, but limited to the stages the device's enabled extensions make valid

typedef struct GraphicsDevice GraphicsDevice;
VkShaderStageFlags vkGetShaderStagesDevice(const GraphicsDevice *device, U32 vis);
VkDescriptorType vkGetDescriptorType(ESHRegisterType regType);

//Transitions entire resource rather than sub-resources

Bool VkUnifiedTexture_transition(
	VkUnifiedTexture *image,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	VkImageLayout layout,
	U32 graphicsQueueId,
	const VkImageSubresourceRange *range,
	ListVkImageMemoryBarrier2 *imageBarriers,
	VkDependencyInfo *dependency,
	const Allocator *alloc,
	Error *e_rr
);
