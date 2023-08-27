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

typedef struct List List;

typedef struct VkGraphicsInstance {
	
	VkInstance instance;
	VkDebugReportCallbackEXT debugReportCallback;

	PFN_vkGetImageMemoryRequirements2KHR getImageMemoryRequirements2;
	PFN_vkGetBufferMemoryRequirements2KHR getBufferMemoryRequirements2;

	PFN_vkSetDebugUtilsObjectNameEXT debugSetName;

	PFN_vkCmdDebugMarkerBeginEXT debugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT debugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT debugMarkerInsert;

	PFN_vkCreateDebugReportCallbackEXT debugCreateReportCallback;

	PFN_vkDestroyDebugReportCallbackEXT debugDestroyReportCallback;

	PFN_vkDebugReportCallbackEXT debugReportCallbackFunc;

	PFN_vkAcquireNextImageKHR acquireNextImage;
	PFN_vkCreateSwapchainKHR createSwapchain;
	PFN_vkDestroySurfaceKHR destroySurface;
	PFN_vkDestroySwapchainKHR destroySwapchain;

	PFN_vkCmdDrawIndexedIndirectCountKHR drawIndexedIndirectCount;
	PFN_vkCmdDrawIndirectCountKHR drawIndirectCount;

	PFN_vkBuildAccelerationStructuresKHR buildAccelerationStructures;
	PFN_vkCmdCopyAccelerationStructureKHR copyAccelerationStructure;
	PFN_vkDestroyAccelerationStructureKHR destroyAccelerationStructure;
	PFN_vkGetAccelerationStructureBuildSizesKHR getAccelerationStructureBuildSizes;
	PFN_vkGetDeviceAccelerationStructureCompatibilityKHR getAccelerationStructureCompatibility;

	PFN_vkCmdTraceRaysKHR traceRays;
	PFN_vkCmdTraceRaysIndirectKHR traceRaysIndirect;
	PFN_vkCreateRayTracingPipelinesKHR createRaytracingPipelines;

} VkGraphicsInstance;

impl Error VkGraphicsInstance_getLayers(List *layers);
