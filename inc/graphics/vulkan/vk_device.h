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

#pragma once
#include "graphics/vulkan/vulkan.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/command_list.h"
#include "types/lock.h"
#include "types/list.h"
#include "types/allocation_buffer.h"
#include "types/vec.h"

typedef RefPtr PipelineRef;

enum EVkDeviceVendor {
	EVkDeviceVendor_NV					= 0x10DE,
	EVkDeviceVendor_AMD					= 0x1002,
	EVkDeviceVendor_ARM					= 0x13B5,
	EVkDeviceVendor_QCOM				= 0x5143,
	EVkDeviceVendor_INTC				= 0x8086,
	EVkDeviceVendor_IMGT				= 0x1010
};

//Special features that are only important for implementation, but we do want to be cached.

typedef enum EVkGraphicsFeatures {
	EVkGraphicsFeatures_PerfQuery		= 1 << 0
} EVkGraphicsFeatures;

typedef enum EVkCommandQueue {

	EVkCommandQueue_Copy,					//Queue for dedicated host -> device copies
	EVkCommandQueue_Compute,
	EVkCommandQueue_Graphics,

	//EVkCommandQueue_VideoDecode,			//TODO:
	//EVkCommandQueue_VideoEncode

	EVkCommandQueue_Count

} EVkCommandQueue;

typedef struct VkCommandQueue {

	VkQueue queue;

	U32 queueId;					//Queue family
	U32 resolvedQueueId;			//Index into command pool array for that queue

	EVkCommandQueue type;
	U32 pad;

} VkCommandQueue;

typedef enum EDescriptorSetType {

	EDescriptorSetType_Sampler,
	EDescriptorSetType_Resources,
	EDescriptorSetType_CBuffer0,
	EDescriptorSetType_CBuffer1,		//Versioning
	EDescriptorSetType_CBuffer2,

	EDescriptorSetType_Count,
	EDescriptorSetType_UniqueLayouts = EDescriptorSetType_CBuffer1

} EDescriptorSetType;

typedef struct VkCommandAllocator {
	VkCommandPool pool;
	VkCommandBuffer cmd;
} VkCommandAllocator;

TList(VkCommandAllocator);
TList(VkSemaphore);
TList(VkResult);
TList(VkSwapchainKHR);
TList(VkPipelineStageFlags);
TList(VkWriteDescriptorSet);

typedef struct VkGraphicsDevice {

	VkDevice device;
	VkCommandQueue queues[EVkCommandQueue_Count];		//Don't have to be unique queues! Indexed by EVkCommandQueue

	U32 uniqueQueues[EVkCommandQueue_Count];			//Queue families ([resolvedQueues], indexed through resolvedId)

	U32 resolvedQueues;
	U32 pad;

	//3D as 1D flat List: resolvedQueueId + (backBufferId * threadCount + threadId) * resolvedQueues
	ListVkCommandAllocator commandPools;

	ListVkSemaphore submitSemaphores;

	VkSemaphore commitSemaphore;

	VkDescriptorSetLayout setLayouts[EDescriptorSetType_UniqueLayouts];
	VkDescriptorSet sets[EDescriptorSetType_Count];				//1 per type and 2 extra for ubo to allow versioning
	VkPipelineLayout defaultLayout;								//Default layout if push constants aren't present

	VkDescriptorPool descriptorPool;

	VkPhysicalDeviceMemoryProperties memoryProperties;

	//Temporary storage for submit time stuff

	ListVkSemaphore waitSemaphores;
	ListVkResult results;
	ListU32 swapchainIndices;
	ListVkSwapchainKHR swapchainHandles;
	ListVkPipelineStageFlags waitStages;
	ListVkBufferMemoryBarrier2 bufferTransitions;
	ListVkImageMemoryBarrier2 imageTransitions;
	ListVkImageCopy imageCopyRanges;

} VkGraphicsDevice;

typedef struct VkCommandBufferState {

	RefPtr *tempPipelines[EPipelineType_Count];		//Pipelines that were set via command, but not bound yet
	RefPtr *pipelines[EPipelineType_Count];			//Currently bound pipelines

	F32x4 blendConstants, tempBlendConstants;

	U8 stencilRef, tempStencilRef;
	U16 padding;

	U32 scopeCounter;

	VkCommandBuffer buffer;

	SetPrimitiveBuffersCmd boundBuffers, tempBoundBuffers;

	VkViewport boundViewport, tempViewport;
	VkRect2D boundScissor, tempScissor;

} VkCommandBufferState;

VkCommandAllocator *VkGraphicsDevice_getCommandAllocator(
	VkGraphicsDevice *device,
	U32 resolvedQueueId,
	U64 threadId,
	U8 backBufferId
);

Error VkDeviceMemoryAllocator_findMemory(
	VkGraphicsDevice *deviceExt,
	Bool cpuSided,
	U32 memoryBits,
	U32 *memoryId,
	VkMemoryPropertyFlags *propertyFlags,
	U64 *size
);

Error VkGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, VkCommandBuffer commandBuffer);
