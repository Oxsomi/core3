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

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <dxgi1_6.h>
#endif

extern const C8 *reqExtensionsName[];
extern U64 reqExtensionsNameCount;

typedef enum EOptExtensions {
	EOptExtensions_PerfQuery,
	EOptExtensions_RayPipeline,
	EOptExtensions_RayQuery,
	EOptExtensions_RayAcceleration,
	EOptExtensions_RayMotionBlur,
	EOptExtensions_RayReorder,
	EOptExtensions_MeshShader,
	EOptExtensions_VariableRateShading,
	EOptExtensions_DynamicRendering,
	EOptExtensions_RayMicromapOpacity,
	EOptExtensions_RayMicromapDisplacement,
	EOptExtensions_AtomicF32,
	EOptExtensions_DeferredHostOperations,
	EOptExtensions_RaytracingValidation,
	EOptExtensions_ComputeDeriv,
	EOptExtensions_Maintenance4,
	EOptExtensions_BufferDeviceAddress,
	EOptExtensions_Bindless,
	EOptExtensions_DriverProperties,
	EOptExtensions_AtomicI64,
	EOptExtensions_F16,
	EOptExtensions_MultiDrawIndirectCount,
	EOptExtensions_MemoryBudget
} EOptExtensions;

extern const C8 *optExtensionsName[];
extern U64 optExtensionsNameCount;

typedef struct VkGraphicsInstance {

	VkInstance instance;
	VkDebugReportCallbackEXT debugReportCallback;

	PFN_vkCreateInstance createInstance;
	PFN_vkEnumerateInstanceLayerProperties enumerateInstanceLayerProperties;
	PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties;

	PFN_vkCreateDevice createDevice;
	PFN_vkDestroyDevice destroyDevice;

	PFN_vkCreateDebugReportCallbackEXT debugCreateReportCallback;
	PFN_vkDestroyDebugReportCallbackEXT debugDestroyReportCallback;

	PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices;
	PFN_vkEnumerateDeviceLayerProperties enumerateDeviceLayerProperties;
	PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties;
	PFN_vkGetPhysicalDeviceFormatProperties getPhysicalDeviceFormatProperties;

	PFN_vkGetPhysicalDeviceFeatures2KHR getPhysicalDeviceFeatures2;
	PFN_vkGetPhysicalDeviceProperties2KHR getPhysicalDeviceProperties2;

	PFN_vkDestroyInstance destroyInstance;
	PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties;

	PFN_vkSetDebugUtilsObjectNameEXT debugSetName;
	PFN_vkCmdBeginDebugUtilsLabelEXT cmdDebugMarkerBegin;
	PFN_vkCmdEndDebugUtilsLabelEXT cmdDebugMarkerEnd;
	PFN_vkCmdInsertDebugUtilsLabelEXT cmdDebugMarkerInsert;
	
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getPhysicalDeviceSurfaceFormats;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getPhysicalDeviceSurfacePresentModes;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR getPhysicalDeviceSurfaceSupport;

	PFN_vkGetPhysicalDeviceQueueFamilyProperties getPhysicalDeviceQueueFamilyProperties;
	void *createSurfaceExt;									//Android, windows, etc.

	PFN_vkDestroySurfaceKHR destroySurface;

	PFN_vkGetPhysicalDeviceMemoryProperties2 getPhysicalDeviceMemoryProperties2;
	U64 padding0;

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		IDXGIFactory6 *dxgiFactory;
	#else
		U64 padding;
	#endif

} VkGraphicsInstance;

typedef struct ListConstC8 ListConstC8;
impl Error VkGraphicsInstance_getLayers(Bool isDebug, ListConstC8 *layers);
