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

typedef struct Window Window;
typedef struct VkGraphicsInstance VkGraphicsInstance;

typedef struct VkSwapchain {

	VkSurfaceKHR surface;			//Platform's surface implementation
	VkSwapchainKHR swapchain;

	List semaphores;				//<VkSemaphore>
	List images;					//<VkManagedImage>

	VkSurfaceFormatKHR format;

	U32 currentIndex;				//Swapchain index

	List descriptorAllocations;

} VkSwapchain;

impl Error VkSurface_create(GraphicsDevice *device, const Window *window, VkSurfaceKHR *surface);

//Transitions entire resource rather than subresources

Error VkSwapchain_transition(
	VkManagedImage *image, 
	VkPipelineStageFlags2 stage, 
	VkAccessFlagBits2 access,
	VkImageLayout layout,
	U32 graphicsQueueId,
	const VkImageSubresourceRange *range,
	List *imageBarriers,
	VkDependencyInfo *dependency
);
