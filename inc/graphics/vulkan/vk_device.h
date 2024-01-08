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

	EDescriptorType_ResourceCount		//Count of totally accessible resources (without cbuffer)

} EDescriptorType;

typedef enum EDescriptorSetType {

	EDescriptorSetType_Sampler,
	EDescriptorSetType_Resources,
	EDescriptorSetType_CBuffer0,
	EDescriptorSetType_CBuffer1,		//Versioning
	EDescriptorSetType_CBuffer2,

	EDescriptorSetType_Count,
	EDescriptorSetType_UniqueLayouts = EDescriptorSetType_CBuffer1

} EDescriptorSetType;

typedef enum EDescriptorTypeCount {

	EDescriptorTypeCount_Sampler		= 2048,		//All samplers

	EDescriptorTypeCount_Texture2D		= 184464,	//~74% of textures (2D)
	EDescriptorTypeCount_TextureCube	= 32768,	//~13% of textures (Cube)
	EDescriptorTypeCount_Texture3D		= 32768,	//~13% of textures (3D)
	EDescriptorTypeCount_Buffer			= 249999,	//All buffers (readonly)
	EDescriptorTypeCount_RWBuffer		= 250000,	//All buffers (RW)
	EDescriptorTypeCount_RWTexture3D	= 6553,		//10% of 64Ki (3D + Cube) for RW 3D unorm
	EDescriptorTypeCount_RWTexture3Ds	= 4809,		//~7.3% of 64Ki (Remainder) (3D + Cube) for RW 3D snorm
	EDescriptorTypeCount_RWTexture3Df	= 43690,	//66% of 64Ki (3D + Cube) for RW 3D float
	EDescriptorTypeCount_RWTexture3Di	= 5242,		//8% of 64Ki (3D + Cube) for RW 3D int
	EDescriptorTypeCount_RWTexture3Du	= 5242,		//8% of 64Ki (3D + Cube) for RW 3D uint
	EDescriptorTypeCount_RWTexture2D	= 92232,	//50% of 250K - 64Ki (2D) for 2D unorm
	EDescriptorTypeCount_RWTexture2Ds	= 9224,		//5% of 250K - 64Ki (2D) for 2D snorm
	EDescriptorTypeCount_RWTexture2Df	= 61488,	//33% of 250K - 64Ki (2D) for 2D float
	EDescriptorTypeCount_RWTexture2Di	= 10760,	//5.8% of 250K - 64Ki (2D) for 2D int
	EDescriptorTypeCount_RWTexture2Du	= 10760,	//5.8% of 250K - 64Ki (2D) for 2D uint

	EDescriptorTypeCount_Textures		= 
		EDescriptorTypeCount_Texture2D + EDescriptorTypeCount_Texture3D + EDescriptorTypeCount_TextureCube,

	EDescriptorTypeCount_RWTextures2D	=
		EDescriptorTypeCount_RWTexture2D  + EDescriptorTypeCount_RWTexture2Ds +	EDescriptorTypeCount_RWTexture2Df +
		EDescriptorTypeCount_RWTexture2Du + EDescriptorTypeCount_RWTexture2Di,

	EDescriptorTypeCount_RWTextures3D	=
		EDescriptorTypeCount_RWTexture3D  + EDescriptorTypeCount_RWTexture3Ds +	EDescriptorTypeCount_RWTexture3Df +
		EDescriptorTypeCount_RWTexture3Du + EDescriptorTypeCount_RWTexture3Di,

	EDescriptorTypeCount_RWTextures		= EDescriptorTypeCount_RWTextures2D + EDescriptorTypeCount_RWTextures3D,

	EDescriptorTypeCount_SSBO			= EDescriptorTypeCount_Buffer + EDescriptorTypeCount_RWBuffer

} EDescriptorTypeCount;

static const U32 descriptorTypeCount[] = {
	EDescriptorTypeCount_Sampler,
	EDescriptorTypeCount_Texture2D,
	EDescriptorTypeCount_TextureCube,
	EDescriptorTypeCount_Texture3D,
	EDescriptorTypeCount_Buffer,
	EDescriptorTypeCount_RWBuffer,
	EDescriptorTypeCount_RWTexture3D,
	EDescriptorTypeCount_RWTexture3Ds,
	EDescriptorTypeCount_RWTexture3Df,
	EDescriptorTypeCount_RWTexture3Di,
	EDescriptorTypeCount_RWTexture3Du,
	EDescriptorTypeCount_RWTexture2D,
	EDescriptorTypeCount_RWTexture2Ds,
	EDescriptorTypeCount_RWTexture2Df,
	EDescriptorTypeCount_RWTexture2Di,
	EDescriptorTypeCount_RWTexture2Du
};

typedef struct VkCommandAllocator {
	VkCommandPool pool;
	VkCommandBuffer cmd;
} VkCommandAllocator;

typedef struct DescriptorStackTrace {

	U32 resourceId, padding;

	void *stackTrace[8];

} DescriptorStackTrace;

TList(VkCommandAllocator);
TList(VkSemaphore);
TList(VkResult);
TList(VkSwapchainKHR);
TList(VkPipelineStageFlags);
TList(VkImageMemoryBarrier2);
TList(VkBufferMemoryBarrier2);
TList(VkWriteDescriptorSet);
TList(DescriptorStackTrace);

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

	//Used for allocating descriptors

	Lock descriptorLock;
	Buffer freeList[EDescriptorType_ResourceCount];
	ListDescriptorStackTrace descriptorStackTraces;

	//Temporary storage for submit time stuff

	ListVkSemaphore waitSemaphores;
	ListVkResult results;
	ListU32 swapchainIndices;
	ListVkSwapchainKHR swapchainHandles;
	ListVkPipelineStageFlags waitStages;
	ListVkBufferMemoryBarrier2 bufferTransitions;
	ListVkImageMemoryBarrier2 imageTransitions;

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
	U32 threadId, 
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

//Lower 20 bit: id
//4 bit higher: descriptor type
U32 VkGraphicsDevice_allocateDescriptor(VkGraphicsDevice *deviceExt, EDescriptorType type);
Bool VkGraphicsDevice_freeAllocations(VkGraphicsDevice *deviceExt, ListU32 *allocations);
