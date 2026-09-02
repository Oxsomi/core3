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

//graphics/vulkan/vk_instance.h

#pragma once
#include "graphics/vulkan/vulkan.h"
#include "types/base/platform_types.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <dxgi1_6.h>
#endif

extern const C8 *reqExtensionsName[];
extern U64 reqExtensionsNameCount;

//Indexes the presence array built from reqExtensionsName, so the order has to match that table.

typedef enum EReqExtensions {
	EReqExtensions_Synchronization2,
	EReqExtensions_Swapchain
} EReqExtensions;

typedef enum EOptExtensions {
	EOptExtensions_PerfQuery,
	EOptExtensions_RayPipeline,
	EOptExtensions_RayQuery,
	EOptExtensions_RayAcceleration,

	EOptExtensions_Barycentrics,       //VK_KHR_fragment_shader_barycentric (SV_Barycentrics in fragment shaders)
	EOptExtensions_RayReorder,
	EOptExtensions_MeshShader,
	EOptExtensions_VariableRateShading,
	EOptExtensions_DynamicRendering,
	EOptExtensions_RayMicromapOpacity,
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
	EOptExtensions_MemoryBudget,
	EOptExtensions_CooperativeVector,
	EOptExtensions_CooperativeMatrix,
	EOptExtensions_ShaderFloat8,
	EOptExtensions_RayTriPosition,
	EOptExtensions_DescriptorHeap,
	EOptExtensions_RayClusterAS,
	EOptExtensions_RayPartitionedTLAS,
	EOptExtensions_PushDescriptor,

	//Dependencies of the features above rather than features of their own.
	//They're listed here so they're only ever requested when the device actually advertised them; a device that
	// offers a feature without its dependencies simply doesn't get the feature.

	EOptExtensions_CreateRenderpass2,
	EOptExtensions_DepthStencilResolve,
	EOptExtensions_Spirv14,
	EOptExtensions_ShaderFloatControls,
	EOptExtensions_Maintenance5,

	//The KHR promotion of RayMicromapOpacity above and the extension it hard depends on.
	//Appended rather than placed next to their sibling because this enum indexes the positional extension
	// name table in vk_instance.c.

	EOptExtensions_RayMicromapOpacityKHR,
	EOptExtensions_DeviceAddressCommands,

	EOptExtensions_ConditionalRendering
} EOptExtensions;

extern const C8 *optExtensionsName[];
extern U64 optExtensionsNameCount;

typedef struct VkGraphicsInstance {

	VkInstance instance;
	VkDebugReportCallbackEXT debugReportCallback;

	//The Vulkan loader is loaded dynamically (not statically linked) so we don't depend on an arch-specific
	//vulkan-1.lib. vulkanLib is the loaded loader; every other entry point is resolved through these two.
	void *vulkanLib;
	PFN_vkGetInstanceProcAddr getInstanceProcAddr;
	PFN_vkGetDeviceProcAddr getDeviceProcAddr;

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

	void *createSurfaceExt;  //Android, windows, etc.
	PFN_vkDestroySurfaceKHR destroySurface;

	PFN_vkGetPhysicalDeviceMemoryProperties2 getPhysicalDeviceMemoryProperties2;
	U64 padding0;

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		IDXGIFactory6 *dxgiFactory;
	#else
		U64 padding;
	#endif

	U64 padding1;

} VkGraphicsInstance;
