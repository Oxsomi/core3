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

//graphics/vulkan/vk_device.h

#pragma once
#include "graphics/vulkan/vulkan.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device.h"
#include "types/container/list.h"
#include "types/container/list_basic_types.h"
#include "types/math/vec4.h"
#include "types/base/platform_types.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS       //For fallback to query memory usage
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <dxgi1_4.h>
#endif

typedef RefPtr PipelineRef;

//Special features that are only important for implementation, but we do want to be cached.

typedef enum EVkCommandQueue {

	EVkCommandQueue_Copy,                    //Queue for dedicated host -> device copies
	EVkCommandQueue_Compute,
	EVkCommandQueue_Graphics,

	//EVkCommandQueue_VideoDecode,           //TODO:
	//EVkCommandQueue_VideoEncode

	EVkCommandQueue_Count

} EVkCommandQueue;

typedef struct VkCommandQueue {

	VkQueue queue;

	U32 queueId;                    //Queue family
	U32 resolvedQueueId;            //Index into command pool array for that queue

	EVkCommandQueue type;
	U32 pad;

} VkCommandQueue;

typedef enum EDescriptorSetType {

	EDescriptorSetType_Sampler,
	EDescriptorSetType_Resources,
	EDescriptorSetType_CBuffer0,
	EDescriptorSetType_CBuffer1,    //Versioning
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

typedef struct VkGraphicsDevice {

	VkDevice device;
	VkCommandQueue queues[EVkCommandQueue_Count];       //Don't have to be unique queues! Indexed by EVkCommandQueue

	U32 uniqueQueues[EVkCommandQueue_Count];            //Queue families ([resolvedQueues], indexed through resolvedId)

	U32 resolvedQueues;

	//3D as 1D flat List: resolvedQueueId + (frameInFlightId * threadCount + threadId) * resolvedQueues
	ListVkCommandAllocator commandPools;

	ListVkSemaphore submitSemaphores;

	VkFence commitFence[MAX_FRAMES_IN_FLIGHT];

	//Whether a submit is actually pending on commitFence[i].
	//A failed vkQueueSubmit leaves the fence reset-but-unsignaled, while submitId still advanced.
	//Without this the next frame at that fifId would wait the full timeout on a fence nothing will ever signal.
	//When false that wait, and its reset, is skipped and the still-unsignaled fence is reused directly.
	//This self-heals once submits succeed again.
	Bool commitFencePending[MAX_FRAMES_IN_FLIGHT];

	//Push descriptor emulation, only allocated on a device that lacks VK_KHR_push_descriptor.
	//One set per frame in flight, each pointing at that frame's globals buffer for the lifetime of the device,
	// so they're written once at first use and only bound afterwards.
	//cbufferPool doubles as the "already emulated" marker, since it's the first thing created.

	VkDescriptorPool cbufferPool;
	VkDescriptorSet cbufferSets[MAX_FRAMES_IN_FLIGHT];

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
	PFN_vkGetAccelerationStructureDeviceAddressKHR getAccelerationStructureDeviceAddress;
	PFN_vkGetDeviceAccelerationStructureCompatibilityKHR getAccelerationStructureCompatibility;

	//Only loaded on the EXT opacity micromap path; the KHR promotion builds micromap arrays through the
	// ordinary acceleration structure entry points above and has no micromap functions of its own.

	PFN_vkCreateMicromapEXT createMicromap;
	PFN_vkDestroyMicromapEXT destroyMicromap;
	PFN_vkCmdBuildMicromapsEXT cmdBuildMicromaps;
	PFN_vkGetMicromapBuildSizesEXT getMicromapBuildSizes;

	PFN_vkCmdTraceRaysKHR traceRays;
	PFN_vkCmdTraceRaysIndirectKHR traceRaysIndirect;
	PFN_vkCreateRayTracingPipelinesKHR createRaytracingPipelines;
	PFN_vkGetRayTracingShaderGroupHandlesKHR getRayTracingShaderGroupHandles;

	PFN_vkCmdPipelineBarrier2KHR cmdPipelineBarrier2;

	//Pipeline executable introspection (VK_KHR_pipeline_executable_properties); only loaded when the capability is on.

	PFN_vkGetPipelineExecutablePropertiesKHR getPipelineExecutableProperties;
	PFN_vkGetPipelineExecutableStatisticsKHR getPipelineExecutableStatistics;
	PFN_vkGetPipelineExecutableInternalRepresentationsKHR getPipelineExecutableInternalRepresentations;

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
	PFN_vkCmdPushConstants cmdPushConstants;
	PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer;
	PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers;
	PFN_vkCmdDrawIndexed cmdDrawIndexed;
	PFN_vkCmdDraw cmdDraw;
	PFN_vkCmdDrawIndexedIndirectCountKHR cmdDrawIndexedIndirectCount;
	PFN_vkCmdDrawIndirectCountKHR cmdDrawIndirectCount;
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
	PFN_vkGetBufferDeviceAddressKHR getBufferDeviceAddress;
	PFN_vkUpdateDescriptorSets updateDescriptorSets;
	PFN_vkFlushMappedMemoryRanges flushMappedMemoryRanges;
	PFN_vkCmdCopyBuffer cmdCopyBuffer;
	PFN_vkCmdCopyImageToBuffer cmdCopyImageToBuffer;
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
	PFN_vkCreateCommandPool createCommandPool;
	PFN_vkResetCommandPool resetCommandPool;
	PFN_vkAllocateCommandBuffers allocateCommandBuffers;
	PFN_vkBeginCommandBuffer beginCommandBuffer;
	PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets;
	PFN_vkCmdPushDescriptorSetKHR cmdPushDescriptorSet;
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
	PFN_vkCreateFence createFence;
	PFN_vkWaitForFences waitForFences;
	PFN_vkResetFences resetFences;
	PFN_vkDestroyFence destroyFence;
	PFN_vkFreeDescriptorSets freeDescriptorSets;

	U32 nonLinearAlignment;
	U8 framesInFlight; Bool hasLocalMemory;
	U16 atomSize;

	U64 maxHeapSizes[2];

	U32 heapIds[2];

	Bool hasDistinctMemory;
	Bool hasOnlyLocalMemory;
	U8 padding1[6];

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS        //For fallback to query memory usage
		IDXGIAdapter3 *dxgiAdapter;
	#else
		U64 padding2;
	#endif

	U64 padding3;

} VkGraphicsDevice;

typedef struct VkCommandBufferState {

	RefPtr *tempPipelines[EPipelineType_Count];   //Pipelines that were set via command, but not bound yet
	RefPtr *pipelines[EPipelineType_Count];       //Currently bound pipelines

	//Bindful: table state set by BindDescriptorTable, emitted lazily at the work ops.
	//defaultDescriptorsDirty means a custom layout bind disturbed the default table's sets, so the next work
	// op on a default layout pipeline has to rebind them.

	//Bindful: heap and table state set by the bind commands, emitted lazily at the work ops.
	//The heap has nothing to emit on Vulkan today (a descriptor heap is a pool), but the state is recorded so
	// VK_EXT_descriptor_heap can map the explicit bind directly later.
	//defaultDescriptorsBound starts false: the default sets only bind at the first work op that runs a
	// default layout pipeline, so a purely bindful frame never pays for them.

	RefPtr *boundDescriptorTable;
	RefPtr *boundDescriptorHeap;

	//Push constants are re-emitted per bind point, because a pipeline layout change invalidates them and
	// the three bind points each carry their own set

	U8 pushConstantData[128];
	U8 pushConstantSize;
	U8 pushConstantsEmitted[3];               //Per bind point: compute, graphics, rt
	U8 padding2[4];
	RefPtr *lastBoundTable[3];
	VkPipelineLayout lastBoundLayout[3];

	//What the constants were last pushed against, per bind point. Emitted alone can't answer it: a pipeline
	//layout change invalidates push constants, and a layout without bindings never touches lastBoundLayout.

	VkPipelineLayout lastPushLayout[3];

	//Push descriptors travel with the command buffer the same way, and a layout switch invalidates them too.

	Descriptor pushDescriptors[OXC3_MAX_PUSH_DESCRIPTORS];
	U8 pushDescriptorCount;
	U8 pushDescriptorsEmitted[3];             //Per bind point: compute, graphics, rt
	U8 padding4[4];
	VkPipelineLayout lastPushDescLayout[3];

	Bool defaultDescriptorsBound;
	U8 padding0[15];

	F32x4 blendConstants, tempBlendConstants;

	U8 stencilRef, tempStencilRef;
	U16 padding;

	U32 scopeCounter;

	VkCommandBuffer buffer;

	SetPrimitiveBuffersCmd boundBuffers, tempBoundBuffers;

	VkViewport boundViewport, tempViewport;
	VkRect2D boundScissor, tempScissor;

} VkCommandBufferState;

typedef struct VkDescriptorHeap {
	VkDescriptorPool pool;
	U64 padding;
} VkDescriptorHeap;

typedef struct VkDescriptorLayout {
	VkDescriptorSetLayout layouts[4];
	U32 setIds[4];
} VkDescriptorLayout;

TList(VkDescriptorBufferInfo);
TList(VkDescriptorImageInfo);
TList(VkAccelerationStructureKHR);

typedef struct VkDescriptorTableRange {

	union {
		ListVkDescriptorBufferInfo updateBuffers;
		ListVkDescriptorImageInfo updateImages;
		ListVkAccelerationStructureKHR tlases;
	};

	ListU32 views;
	ListU32 newViews;        //Temp data for new view ids

} VkDescriptorTableRange;

TList(VkDescriptorTableRange);

typedef struct VkDescriptorTable {

	VkDescriptorSet sets[4];
	ListVkDescriptorTableRange ranges;

	U8 bindCommands, pad[3], counts[4];
	U32 offsets[4];

} VkDescriptorTable;

VkCommandAllocator *VkGraphicsDevice_getCommandAllocator(
	VkGraphicsDevice *device,
	U32 resolvedQueueId,
	U64 threadId,
	U8 frameInFlightId,
	U8 fifCount
);

Bool VkGraphicsDevice_findAllMemory(VkGraphicsDevice *deviceExt, Error *e_rr);

Bool VkGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, VkCommandBufferState *commandBuffer, Error *e_rr);
