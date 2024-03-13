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
	EOptExtensions_RayMotionBlur,
	EOptExtensions_RayReorder,
	EOptExtensions_MeshShader,
	EOptExtensions_Multiview,
	EOptExtensions_VariableRateShading,
	EOptExtensions_DynamicRendering,
	EOptExtensions_RayMicromapOpacity,
	EOptExtensions_RayMicromapDisplacement,
	EOptExtensions_AtomicF32,
	EOptExtensions_DeferredHostOperations

} EOptExtensions;

extern const C8 *optExtensionsName[];
extern U64 optExtensionsNameCount;

typedef struct VkGraphicsInstance {

	VkInstance instance;
	VkDebugReportCallbackEXT debugReportCallback;

	PFN_vkGetDeviceBufferMemoryRequirementsKHR getDeviceBufferMemoryRequirements;
	PFN_vkGetDeviceImageMemoryRequirementsKHR getDeviceImageMemoryRequirements;

	PFN_vkGetPhysicalDeviceFeatures2KHR getPhysicalDeviceFeatures2;
	PFN_vkGetPhysicalDeviceProperties2KHR getPhysicalDeviceProperties2;

	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getPhysicalDeviceSurfaceFormats;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getPhysicalDeviceSurfacePresentModes;
	PFN_vkGetSwapchainImagesKHR getSwapchainImages;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR getPhysicalDeviceSurfaceSupport;
	void *createSurfaceExt;

	PFN_vkSetDebugUtilsObjectNameEXT debugSetName;

	PFN_vkCmdDebugMarkerBeginEXT cmdDebugMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT cmdDebugMarkerEnd;
	PFN_vkCmdDebugMarkerInsertEXT cmdDebugMarkerInsert;

	PFN_vkCmdBeginRenderingKHR cmdBeginRendering;
	PFN_vkCmdEndRenderingKHR cmdEndRendering;

	PFN_vkCreateDebugReportCallbackEXT debugCreateReportCallback;

	PFN_vkDestroyDebugReportCallbackEXT debugDestroyReportCallback;

	PFN_vkAcquireNextImageKHR acquireNextImage;
	PFN_vkCreateSwapchainKHR createSwapchain;
	PFN_vkDestroySurfaceKHR destroySurface;
	PFN_vkDestroySwapchainKHR destroySwapchain;

	PFN_vkCmdDrawIndexedIndirectCountKHR cmdDrawIndexedIndirectCount;
	PFN_vkCmdDrawIndirectCountKHR cmdDrawIndirectCount;

	PFN_vkCmdBuildAccelerationStructuresKHR cmdBuildAccelerationStructures;
	PFN_vkCreateAccelerationStructureKHR createAccelerationStructure;
	PFN_vkCmdCopyAccelerationStructureKHR copyAccelerationStructure;
	PFN_vkDestroyAccelerationStructureKHR destroyAccelerationStructure;
	PFN_vkGetAccelerationStructureBuildSizesKHR getAccelerationStructureBuildSizes;
	PFN_vkGetDeviceAccelerationStructureCompatibilityKHR getAccelerationStructureCompatibility;

	PFN_vkGetBufferDeviceAddressKHR getBufferDeviceAddress;

	PFN_vkCmdTraceRaysKHR traceRays;
	PFN_vkCmdTraceRaysIndirectKHR traceRaysIndirect;
	PFN_vkCreateRayTracingPipelinesKHR createRaytracingPipelines;
	PFN_vkGetRayTracingShaderGroupHandlesKHR getRayTracingShaderGroupHandles;

	PFN_vkCmdPipelineBarrier2KHR cmdPipelineBarrier2;

} VkGraphicsInstance;

typedef struct ListConstC8 ListConstC8;

impl Error VkGraphicsInstance_getLayers(ListConstC8 *layers);
