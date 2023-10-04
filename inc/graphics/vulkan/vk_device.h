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
#include "platforms/lock.h"
#include "types/list.h"
#include "math/vec.h"

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

typedef enum EDescriptorType {

	EDescriptorType_Sampler,

	EDescriptorType_Texture2D,
	EDescriptorType_TextureCube,
	EDescriptorType_Texture3D,
	EDescriptorType_Buffer,

	EDescriptorType_RWBuffer,
	EDescriptorType_RWTexture3D,
	EDescriptorType_RWTexture3Ds,
	EDescriptorType_RWTexture3Df,
	EDescriptorType_RWTexture3Di,
	EDescriptorType_RWTexture3Du,
	EDescriptorType_RWTexture2D,
	EDescriptorType_RWTexture2Ds,
	EDescriptorType_RWTexture2Df,
	EDescriptorType_RWTexture2Di,
	EDescriptorType_RWTexture2Du,

	EDescriptorType_Count

} EDescriptorType;

static const U32 descriptorTypeCount[] = {
	2048,		//All samplers
	868928,		//~87% of textures
	65536,		//~6.5% of textures
	65536,		//~6.5% of textures
	1000000,	//All buffers
	1000000,	//All buffers
	13107,		//10% of 128Ki (3D + Cube)
	9610,		//~7.3% of 128Ki (Remainder) (3D + Cube)
	87381,		//66% of 128Ki (3D + Cube)
	10487,		//8% of 128Ki (3D + Cube)
	10487,		//8% of 128Ki (3D + Cube)
	434464,		//50% of 1M - 128Ki (2D)
	43446,		//5% of 1M - 128Ki (2D)
	289642,		//33% of 1M - 128Ki (2D)
	50688,		//6% of 1M - 128Ki (2D)
	50688		//6% of 1M - 128Ki (2D)
};

typedef struct VkGraphicsDevice {

	VkDevice device;
	VkCommandQueue queues[EVkCommandQueue_Count];		//Don't have to be unique queues! Indexed by EVkCommandQueue

	U32 uniqueQueues[EVkCommandQueue_Count];			//Queue families ([resolvedQueues], indexed through resolvedId)

	U32 resolvedQueues;
	U32 pad;

	//3D as 1D flat List<VkCommandAllocator>: resolvedQueueId + (backBufferId * threadCount + threadId) * resolvedQueues
	List commandPools;

	List submitSemaphores;

	VkSemaphore commitSemaphore;

	VkDescriptorSetLayout setLayouts[EDescriptorType_Count];
	VkDescriptorSet sets[EDescriptorType_Count];
	VkPipelineLayout defaultLayout;								//Default layout if push constants aren't present

	VkDescriptorPool descriptorPool;

	//Used for allocating descriptors

	Lock descriptorLock;
	Buffer freeList[EDescriptorType_Count];

	//Temporary storage for submit time stuff

	List waitSemaphores, results, swapchainIndices, swapchainHandles, waitStages;

} VkGraphicsDevice;

typedef struct VkCommandAllocator {

	VkCommandPool pool;
	VkCommandBuffer cmd;

} VkCommandAllocator;

typedef enum EVkCommandBufferFlags {

	EVkCommandBufferFlags_hasDepth		= 1 << 0,
	EVkCommandBufferFlags_hasStencil	= 1 << 1

} EVkCommandBufferFlags;

typedef struct VkImageRange {

	RefPtr *ref;

	ImageRange range;

} VkImageRange;

typedef struct VkCommandBufferState {		//Caching state variables

	I32x2 currentSize;						//Size of currently bound render target(s), 0 if none

	EVkCommandBufferFlags flags;
	U32 debugRegionStack;

	VkCommandBuffer buffer;

	U32 pad0;

	U8 pad1[3];
	U8 boundImageCount;

	VkImageRange boundImages[8];

	PipelineRef *boundPipelines[2];			//Graphics, Compute

} VkCommandBufferState;

VkCommandAllocator *VkGraphicsDevice_getCommandAllocator(
	VkGraphicsDevice *device, U32 resolvedQueueId, U32 threadId, U8 backBufferId
);

//Lower 20 bit: id
//4 bit higher: descriptor type
U32 VkGraphicsDevice_allocateDescriptor(VkGraphicsDevice *deviceExt, EDescriptorType type);
Bool VkGraphicsDevice_freeAllocations(VkGraphicsDevice *deviceExt, List *allocations);			//List<U32>
