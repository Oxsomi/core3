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
#include "graphics/vulkan/vulkan.h"
#include "graphics/generic/command_list.h"
#include "types/container/list.h"
#include "types/math/vec.h"

typedef RefPtr PipelineRef;

//Special features that are only important for implementation, but we do want to be cached.

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

	ListVkSemaphore waitSemaphoresList;
	ListVkResult results;
	ListU32 swapchainIndices;
	ListVkSwapchainKHR swapchainHandles;
	ListVkPipelineStageFlags waitStages;
	ListVkBufferMemoryBarrier2 bufferTransitions;
	ListVkImageMemoryBarrier2 imageTransitions;
	ListVkImageCopy imageCopyRanges;
	ListVkBufferImageCopy bufferImageCopyRanges;
	ListVkMappedMemoryRange mappedMemoryRange;
	ListVkBufferCopy bufferCopies;

	PFN_vkGetSwapchainImagesKHR getSwapchainImages;

	PFN_vkCmdBeginRenderingKHR cmdBeginRendering;
	PFN_vkCmdEndRenderingKHR cmdEndRendering;

	PFN_vkAcquireNextImageKHR acquireNextImage;
	PFN_vkCreateSwapchainKHR createSwapchain;
	PFN_vkDestroySwapchainKHR destroySwapchain;

	PFN_vkCmdBuildAccelerationStructuresKHR cmdBuildAccelerationStructures;
	PFN_vkCreateAccelerationStructureKHR createAccelerationStructure;
	PFN_vkCmdCopyAccelerationStructureKHR copyAccelerationStructure;
	PFN_vkDestroyAccelerationStructureKHR destroyAccelerationStructure;
	PFN_vkGetAccelerationStructureBuildSizesKHR getAccelerationStructureBuildSizes;
	PFN_vkGetDeviceAccelerationStructureCompatibilityKHR getAccelerationStructureCompatibility;

	PFN_vkCmdTraceRaysKHR traceRays;
	PFN_vkCmdTraceRaysIndirectKHR traceRaysIndirect;
	PFN_vkCreateRayTracingPipelinesKHR createRaytracingPipelines;
	PFN_vkGetRayTracingShaderGroupHandlesKHR getRayTracingShaderGroupHandles;

	PFN_vkCmdPipelineBarrier2KHR cmdPipelineBarrier2;

	//These functions are manually loaded because the runtime will load them anyways.
	//However, some of these might not be present when statically linked or on the device itself.
	//And so they're just manually loaded.

	PFN_vkAllocateMemory allocateMemory;
	PFN_vkMapMemory mapMemory;
	PFN_vkFreeMemory freeMemory;
	PFN_vkCmdClearColorImage cmdClearColorImage;
	PFN_vkCmdCopyImage cmdCopyImage;
	PFN_vkCmdSetViewport cmdSetViewport;
	PFN_vkCmdSetScissor cmdSetScissor;
	PFN_vkCmdSetBlendConstants cmdSetBlendConstants;
	PFN_vkCmdSetStencilReference cmdSetStencilReference;
	PFN_vkCmdBindPipeline cmdBindPipeline;
	PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer;
	PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers;
	PFN_vkCmdDrawIndexed cmdDrawIndexed;
	PFN_vkCmdDraw cmdDraw;
	PFN_vkCmdDrawIndexedIndirectCount cmdDrawIndexedIndirectCount;
	PFN_vkCmdDrawIndirectCount cmdDrawIndirectCount;
	PFN_vkCmdDrawIndexedIndirect cmdDrawIndexedIndirect;
	PFN_vkCmdDrawIndirect cmdDrawIndirect;
	PFN_vkCmdDispatch cmdDispatch;
	PFN_vkCmdDispatchIndirect cmdDispatchIndirect;
	PFN_vkCreateComputePipelines createComputePipelines;
	PFN_vkDestroyPipeline destroyPipeline;
	PFN_vkDestroyShaderModule destroyShaderModule;
	PFN_vkDestroyBuffer destroyBuffer;
	PFN_vkCreateBuffer createBuffer;
	PFN_vkGetBufferMemoryRequirements2 getBufferMemoryRequirements2;
	PFN_vkBindBufferMemory bindBufferMemory;
	PFN_vkGetBufferDeviceAddress getBufferDeviceAddress;
	PFN_vkUpdateDescriptorSets updateDescriptorSets;
	PFN_vkFlushMappedMemoryRanges flushMappedMemoryRanges;
	PFN_vkCmdCopyBuffer cmdCopyBuffer;
	PFN_vkCmdCopyBufferToImage cmdCopyBufferToImage;
	PFN_vkGetDeviceQueue getDeviceQueue;
	PFN_vkCreateSemaphore createSemaphore;
	PFN_vkCreateDescriptorSetLayout createDescriptorSetLayout;
	PFN_vkCreatePipelineLayout createPipelineLayout;
	PFN_vkCreateDescriptorPool createDescriptorPool;
	PFN_vkAllocateDescriptorSets allocateDescriptorSets;
	PFN_vkFreeCommandBuffers freeCommandBuffers;
	PFN_vkDestroyCommandPool destroyCommandPool;
	PFN_vkDestroySemaphore destroySemaphore;
	PFN_vkDestroyDescriptorSetLayout destroyDescriptorSetLayout;
	PFN_vkDestroyDescriptorPool destroyDescriptorPool;
	PFN_vkDestroyPipelineLayout destroyPipelineLayout;
	PFN_vkDeviceWaitIdle deviceWaitIdle;
	PFN_vkWaitSemaphores waitSemaphores;
	PFN_vkCreateCommandPool createCommandPool;
	PFN_vkResetCommandPool resetCommandPool;
	PFN_vkAllocateCommandBuffers allocateCommandBuffers;
	PFN_vkBeginCommandBuffer beginCommandBuffer;
	PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets;
	PFN_vkEndCommandBuffer endCommandBuffer;
	PFN_vkQueueSubmit queueSubmit;
	PFN_vkQueuePresentKHR queuePresentKHR;
	PFN_vkCreateGraphicsPipelines createGraphicsPipelines;
	PFN_vkDestroyImageView destroyImageView;
	PFN_vkCreateImage createImage;
	PFN_vkGetImageMemoryRequirements2 getImageMemoryRequirements2;
	PFN_vkBindImageMemory bindImageMemory;
	PFN_vkCreateImageView createImageView;
	PFN_vkDestroySampler destroySampler;
	PFN_vkCreateSampler createSampler;
	PFN_vkCreateShaderModule createShaderModule;
	PFN_vkDestroyImage destroyImage;

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

Error VkGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, VkCommandBufferState *commandBuffer);
