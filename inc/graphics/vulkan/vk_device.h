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
#include "types/list.h"

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

typedef enum EVkGraphicsQueue {

	EVkGraphicsQueue_Copy,					//Queue for dedicated host -> device copies
	EVkGraphicsQueue_Compute,
	EVkGraphicsQueue_Graphics,
	EVkGraphicsQueue_Raytracing,

	//EVkGraphicsQueue_VideoDecode,			//TODO:
	//EVkGraphicsQueue_VideoEncode

	EVkGraphicsQueue_Count

} EVkGraphicsQueue;

typedef struct VkGraphicsQueue {

	VkQueue queue;
	U32 queueId;					//Queue family
	U32 resolvedQueueId;			//Index into command pool array for that queue

} VkGraphicsQueue;

typedef struct VkGraphicsDevice {

	VkDevice device;
	VkGraphicsQueue queues[EVkGraphicsQueue_Count];		//Don't have to be unique queues! Indexed by EVkGraphicsQueue

	U32 uniqueQueues[EVkGraphicsQueue_Count];			//Queue families ([resolvedQueues], indexed through resolvedId)

	U32 resolvedQueues;
	U32 pad;

	//3D as 1D flat array: resolvedQueueId + (backBufferId * threadCount + threadId) * resolvedQueues
	List commandPools;

	List submitSemaphores;

	VkSemaphore commitSemaphore;

	//Temporary storage for submit time stuff

	List waitSemaphores, results, swapchainIndices, swapchainHandles, waitStages;

} VkGraphicsDevice;
