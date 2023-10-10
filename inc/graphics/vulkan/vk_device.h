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

	EDescriptorType_ResourceCount,		//Count of totally accessible resources (without cbuffer)

	//Global CBuffer for communicating frameId, deltaTime, etc.

	EDescriptorType_CBuffer = EDescriptorType_ResourceCount,

	EDescriptorType_Count

} EDescriptorType;

static const U32 descriptorTypeCount[] = {

	2048,		//All samplers
	184464,		//~74% of RW textures (2D)
	32768,		//~13% of RW textures (Cube)
	32768,		//~13% of RW textures (3D)
	249999,		//All buffers (readonly)
	250000,		//All buffers (RW)
	6553,		//10% of 64Ki (3D + Cube) for RW 3D unorm
	4809,		//~7.3% of 64Ki (Remainder) (3D + Cube) for RW 3D snorm
	43690,		//66% of 64Ki (3D + Cube) for RW 3D float
	5242,		//8% of 64Ki (3D + Cube) for RW 3D int
	5242,		//8% of 64Ki (3D + Cube) for RW 3D uint
	92232,		//50% of 250K - 64Ki (2D) for 2D unorm
	9224,		//5% of 250K - 64Ki (2D) for 2D snorm
	61488,		//33% of 250K - 64Ki (2D) for 2D float
	10760,		//5.8% of 250K - 64Ki (2D) for 2D int
	10760,		//5.8% of 250K - 64Ki (2D) for 2D uint

	//Swappable CBuffer per frame (only 1 of them is bound each time)

	1,
	1,
	1
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
	VkDescriptorSet sets[EDescriptorType_Count + 2];			//1 per type and 2 extra for ubo to allow versioning
	VkPipelineLayout defaultLayout;								//Default layout if push constants aren't present

	VkDescriptorPool descriptorPool;

	VkPhysicalDeviceMemoryProperties memoryProperties;

	//Used for allocating descriptors

	Lock descriptorLock;
	Buffer freeList[EDescriptorType_ResourceCount];

	//256 x 3 UBO for per frame data.

	Buffer mappedUbo;
	VkBuffer ubo;
	VkDeviceMemory uboMem;

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

	U8 pad1;
	Bool anyScissor, anyViewport;
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
