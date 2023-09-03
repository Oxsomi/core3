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

extern const C8 *reqExtensionsName[];
extern U64 reqExtensionsNameCount;

typedef enum EOptExtensions {

	EOptExtensions_DebugMarker,
	EOptExtensions_F16,
	EOptExtensions_MultiDrawIndirectCount,
	EOptExtensions_AtomicI64,
	EOptExtensions_PerfQuery,
	EOptExtensions_RayPipeline,
	EOptExtensions_RayQuery,
	EOptExtensions_RayAcceleration,
	EOptExtensions_Swapchain,
	EOptExtensions_RayMotionBlur,
	EOptExtensions_RayReorder,
	EOptExtensions_MeshShader,
	EOptExtensions_DynamicRendering,
	EOptExtensions_RayMicromapOpacity,
	EOptExtensions_RayMicromapDisplacement,
	EOptExtensions_VariableRateShading,
	EOptExtensions_AtomicF32,
	EOptExtensions_DeferredHostOperations

} EOptExtensions;

extern const C8 *optExtensionsName[];
extern U64 optExtensionsNameCount;

typedef struct VkGraphicsInstance {
	
	VkInstance instance;
	VkDebugReportCallbackEXT debugReportCallback;

	PFN_vkGetImageMemoryRequirements2KHR getImageMemoryRequirements2;
	PFN_vkGetBufferMemoryRequirements2KHR getBufferMemoryRequirements2;

	PFN_vkGetPhysicalDeviceFeatures2KHR getPhysicalDeviceFeatures2;
	PFN_vkGetPhysicalDeviceProperties2KHR getPhysicalDeviceProperties2;

	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getPhysicalDeviceSurfaceFormats;

	PFN_vkSetDebugUtilsObjectNameEXT debugSetName;

	PFN_vkCmdDebugMarkerBeginEXT debugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT debugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT debugMarkerInsert;

	PFN_vkCreateDebugReportCallbackEXT debugCreateReportCallback;

	PFN_vkDestroyDebugReportCallbackEXT debugDestroyReportCallback;

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
