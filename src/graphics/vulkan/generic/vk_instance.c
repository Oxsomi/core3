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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "platforms/dynamic_library.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/math/math.h"

GraphicsObjectSizes VkGraphicsObjectSizes = {
	.blas = sizeof(VkBLAS),
	.tlas = sizeof(VkTLAS),
	.pipeline = sizeof(VkPipeline) + 8,
	.sampler = sizeof(VkSampler) + 8,
	.buffer = sizeof(VkDeviceBuffer),
	.image = sizeof(VkUnifiedTexture),
	.swapchain = sizeof(VkSwapchain),
	.device = sizeof(VkGraphicsDevice),
	.instance = sizeof(VkGraphicsInstance)
};

#ifndef GRAPHICS_API_DYNAMIC
	const GraphicsObjectSizes *GraphicsInterface_getObjectSizes(EGraphicsApi api) {
		(void) api;
		return &VkGraphicsObjectSizes;
	}
#else
	EXPORT_SYMBOL GraphicsInterfaceTable GraphicsInterface_getTable(Platform *instance, GraphicsInterface *interface) {

		Platform_instance = instance;
		GraphicsInterface_instance = interface;

		return (GraphicsInterfaceTable) {

			.api = EGraphicsApi_Vulkan,
			.objectSizes = VkGraphicsObjectSizes,

			.blasInit = VkBLAS_init,
			.blasFlush = VkBLASRef_flush,
			.blasFree = VkBLAS_free,

			.tlasInit = VkTLAS_init,
			.tlasFlush = VkTLASRef_flush,
			.tlasFree = VkTLAS_free,

			.pipelineCreateGraphics = VkGraphicsDevice_createPipelineGraphics,
			.pipelineCreateCompute = VkGraphicsDevice_createPipelineCompute,
			.pipelineCreateRt = VkGraphicsDevice_createPipelineRaytracingInternal,
			.pipelineFree = VkPipeline_free,

			.samplerCreate = VkGraphicsDeviceRef_createSampler,
			.samplerFree = VkSampler_free,

			.bufferCreate = VkGraphicsDeviceRef_createBuffer,
			.bufferFlush = VkDeviceBufferRef_flush,
			.bufferFree = VkDeviceBuffer_free,

			.textureCreate = VkUnifiedTexture_create,
			.textureFlush = VkDeviceTextureRef_flush,
			.textureFree = VkUnifiedTexture_free,

			.swapchainCreate = VkGraphicsDeviceRef_createSwapchain,
			.swapchainFree = VkSwapchain_free,

			.memoryAllocate = VkDeviceMemoryAllocator_allocate,
			.memoryFree = VkDeviceMemoryAllocator_freeAllocation,

			.deviceInit = VkGraphicsDevice_init,
			.devicePostInit = VkGraphicsDevice_postInit,
			.deviceWait = VkGraphicsDeviceRef_wait,
			.deviceFree = VkGraphicsDevice_free,
			.deviceSubmitCommands = VkGraphicsDevice_submitCommands,
			.commandListProcess = VkCommandList_process,

			.instanceCreate = VkGraphicsInstance_create,
			.instanceFree = VkGraphicsInstance_free,
			.instanceGetDevices = VkGraphicsInstance_getDeviceInfos
		};
	}
#endif

VkBool32 onDebugReport(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType,
	U64 object,
	U64 location,
	I32 messageCode,
	const C8 *layerPrefix,
	const C8 *message,
	void *userData
) {

	(void)messageCode;
	(void)object;
	(void)location;
	(void)objectType;
	(void)userData;
	(void)layerPrefix;

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		Log_errorLnx("Error: %s", message);

	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		Log_warnLnx("Warning: %s", message);

	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		Log_performanceLnx("Performance warning: %s", message);

	else Log_debugLnx("Debug: %s", message);

	return VK_FALSE;
}

#define vkExtensionNoCheck(function, result) {													\
	*(void**)&result = (void*) vkGetInstanceProcAddr(instanceExt->instance, #function);			\
}

#define vkExtension(label, function, result) {													\
																								\
	PFN_vkVoidFunction v = vkGetInstanceProcAddr(instanceExt->instance, #function); 			\
																								\
	if(!v)																						\
		gotoIfError(clean, Error_nullPointer(0, "vkExtension() " #function " failed"))			\
																								\
	*(void**)&result = (void*) v;																\
}

TList(VkExtensionProperties);
TList(VkLayerProperties);
TListImpl(VkExtensionProperties);
TListImpl(VkLayerProperties);

Error VK_WRAP_FUNC(GraphicsInstance_create)(GraphicsApplicationInfo info, GraphicsInstanceRef **instanceRef) {

	U32 layerCount = 0, extensionCount = 0;
	CharString title = (CharString) { 0 };

	ListVkExtensionProperties extensions = (ListVkExtensionProperties) { 0 };
	ListVkLayerProperties layers = (ListVkLayerProperties) { 0 };

	ListConstC8 enabledExtensions = (ListConstC8) { 0 }, enabledLayers = (ListConstC8) { 0 };

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);
	Error err = Error_none();

	gotoIfError(clean, vkCheck(vkEnumerateInstanceLayerProperties(&layerCount, NULL)))
	gotoIfError(clean, vkCheck(vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL)))

	gotoIfError(clean, CharString_createCopyx(info.name, &title))
	gotoIfError(clean, ListVkExtensionProperties_createx(extensionCount, &extensions))
	gotoIfError(clean, ListVkLayerProperties_createx(layerCount, &layers))

	gotoIfError(clean, ListConstC8_reservex(&enabledLayers, layerCount))
	gotoIfError(clean, ListConstC8_reservex(&enabledExtensions, extensionCount))

	gotoIfError(clean, vkCheck(vkEnumerateInstanceLayerProperties(&layerCount, layers.ptrNonConst)))
	gotoIfError(clean, vkCheck(vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions.ptrNonConst)))

	Bool supportsDebug[2] = { 0 };

	CharString debugReport = CharString_createRefCStrConst("VK_EXT_debug_report");
	CharString debugUtils = CharString_createRefCStrConst("VK_EXT_debug_utils");

	if(instance->flags & EGraphicsInstanceFlags_IsVerbose)
		Log_debugLnx("Supported extensions:");

	Bool supportsColorSpace = false;

	CharString swapchainColorspace = CharString_createRefCStrConst("VK_EXT_swapchain_colorspace");

	for(U64 i = 0; i < extensionCount; ++i) {

		const C8 *name = extensions.ptr[i].extensionName;
		CharString nameStr = CharString_createRefCStrConst(name);

		if(CharString_equalsStringSensitive(nameStr, swapchainColorspace))
			supportsColorSpace = true;

		else if(instance->flags & EGraphicsInstanceFlags_IsDebug) {

			if(CharString_equalsStringSensitive(nameStr, debugReport))
				supportsDebug[0] = true;

			else if(CharString_equalsStringSensitive(nameStr, debugUtils))
				supportsDebug[1] = true;
		}

		if(instance->flags & EGraphicsInstanceFlags_IsVerbose)
			Log_debugLnx("\t%s", name);
	}

	if(instance->flags & EGraphicsInstanceFlags_IsVerbose) {

		Log_debugLnx("Supported layers:");

		for(U64 i = 0; i < layerCount; ++i)
			Log_debugLnx("\t%s", layers.ptr[i].layerName);
	}

	if(supportsDebug[0])
		gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, debugReport.ptr))

	if(supportsDebug[1])
		gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, debugUtils.ptr))

	//Force physical device properties and external memory

	gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, "VK_KHR_get_physical_device_properties2"))
	gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, "VK_KHR_external_memory_capabilities"))

	//MoltenVK requires us to enable portability enumeration

	Bool isMoltenVk = false;

	#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
		gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, "VK_KHR_portability_enumeration"))
		isMoltenVk = true;
	#endif

	//Enable so we can use swapchain khr

	gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, "VK_KHR_surface"))
	gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, _VK_SURFACE_EXT))

	if (supportsColorSpace)
		gotoIfError(clean, ListConstC8_pushBackx(&enabledExtensions, swapchainColorspace.ptr))

	gotoIfError(clean, VkGraphicsInstance_getLayers(instance->flags & EGraphicsInstanceFlags_IsDebug, &enabledLayers))

	VkApplicationInfo application = (VkApplicationInfo) {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = title.ptr,
		.applicationVersion = info.version,
		.pEngineName = "OxC3",
		.engineVersion = VK_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH),
		.apiVersion = VK_MAKE_VERSION(1, 2, 0)
	};

	VkInstanceCreateInfo instanceInfo = (VkInstanceCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &application,
		.enabledLayerCount = (U32) enabledLayers.length,
		.ppEnabledLayerNames = enabledLayers.ptr,
		.enabledExtensionCount = (U32) enabledExtensions.length,
		.ppEnabledExtensionNames = enabledExtensions.ptr
	};

	if(isMoltenVk)
		instanceInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

	if(instance->flags & EGraphicsInstanceFlags_IsDebug) {

		//Maximum validation

		VkValidationFeatureEnableEXT enables[] = {
			VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
			VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
			VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
			VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
		};

		VkValidationFeaturesEXT features = (VkValidationFeaturesEXT) {
			.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
			.enabledValidationFeatureCount = (U32) (sizeof(enables) / sizeof(enables[0])),
			.pEnabledValidationFeatures = enables
		};

		instanceInfo.pNext = &features;
	}

	if(instance->flags & EGraphicsInstanceFlags_IsVerbose) {

		Log_debugLnx("Enabling layers:");

		for(U64 i = 0; i < enabledLayers.length; ++i)
			Log_debugLnx("\t%s", enabledLayers.ptr[i]);

		Log_debugLnx("Enabling extensions:");

		for(U64 i = 0; i < enabledExtensions.length; ++i)
			Log_debugLnx("\t%s", enabledExtensions.ptr[i]);

	}

	//Create instance

	gotoIfError(clean, vkCheck(vkCreateInstance(&instanceInfo, NULL, &instanceExt->instance)))

	//Load functions

	vkExtension(clean, vkGetPhysicalDeviceFeatures2KHR, instanceExt->getPhysicalDeviceFeatures2)
	vkExtension(clean, vkGetPhysicalDeviceProperties2KHR, instanceExt->getPhysicalDeviceProperties2)

	vkExtension(clean, vkCmdPipelineBarrier2KHR, instanceExt->cmdPipelineBarrier2)

	vkExtension(clean, vkGetPhysicalDeviceSurfaceFormatsKHR, instanceExt->getPhysicalDeviceSurfaceFormats)
	vkExtension(clean, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, instanceExt->getPhysicalDeviceSurfaceCapabilities)
	vkExtension(clean, vkGetPhysicalDeviceSurfacePresentModesKHR, instanceExt->getPhysicalDeviceSurfacePresentModes)
	vkExtension(clean, vkGetSwapchainImagesKHR, instanceExt->getSwapchainImages)
	vkExtension(clean, vkGetPhysicalDeviceSurfaceSupportKHR, instanceExt->getPhysicalDeviceSurfaceSupport)

	if(supportsDebug[1]) {
		vkExtension(clean, vkSetDebugUtilsObjectNameEXT, instanceExt->debugSetName)
		vkExtension(clean, vkCmdDebugMarkerBeginEXT, instanceExt->cmdDebugMarkerBegin)
		vkExtension(clean, vkCmdDebugMarkerEndEXT, instanceExt->cmdDebugMarkerEnd)
		vkExtension(clean, vkCmdDebugMarkerInsertEXT, instanceExt->cmdDebugMarkerInsert)
	}

	if(supportsDebug[0]) {
		vkExtension(clean, vkCreateDebugReportCallbackEXT, instanceExt->debugCreateReportCallback)
		vkExtension(clean, vkDestroyDebugReportCallbackEXT, instanceExt->debugDestroyReportCallback)
	}

	vkExtension(clean, vkAcquireNextImageKHR, instanceExt->acquireNextImage)
	vkExtension(clean, vkCreateSwapchainKHR, instanceExt->createSwapchain)
	vkExtension(clean, vkDestroySurfaceKHR, instanceExt->destroySurface)
	vkExtension(clean, vkDestroySwapchainKHR, instanceExt->destroySwapchain)

	vkExtensionNoCheck(vkCmdBuildAccelerationStructuresKHR, instanceExt->cmdBuildAccelerationStructures)
	vkExtensionNoCheck(vkCreateAccelerationStructureKHR, instanceExt->createAccelerationStructure)
	vkExtensionNoCheck(vkCmdCopyAccelerationStructureKHR, instanceExt->copyAccelerationStructure)
	vkExtensionNoCheck(vkDestroyAccelerationStructureKHR, instanceExt->destroyAccelerationStructure)
	vkExtensionNoCheck(vkGetAccelerationStructureBuildSizesKHR, instanceExt->getAccelerationStructureBuildSizes)
	vkExtensionNoCheck(vkGetDeviceAccelerationStructureCompatibilityKHR, instanceExt->getAccelerationStructureCompatibility)

	vkExtensionNoCheck(vkCmdTraceRaysKHR, instanceExt->traceRays)
	vkExtensionNoCheck(vkCmdTraceRaysIndirectKHR, instanceExt->traceRaysIndirect)
	vkExtensionNoCheck(vkCreateRayTracingPipelinesKHR, instanceExt->createRaytracingPipelines)
	vkExtensionNoCheck(vkGetRayTracingShaderGroupHandlesKHR, instanceExt->getRayTracingShaderGroupHandles)

	vkExtensionNoCheck(vkCmdBeginRenderingKHR, instanceExt->cmdBeginRendering)
	vkExtensionNoCheck(vkCmdEndRenderingKHR, instanceExt->cmdEndRendering)

	//Add debug callback

	if(instance->flags & EGraphicsInstanceFlags_IsDebug) {

		VkDebugReportCallbackCreateInfoEXT callbackInfo = (VkDebugReportCallbackCreateInfoEXT) {

			.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,

			.flags =
				VK_DEBUG_REPORT_DEBUG_BIT_EXT |
				VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
				VK_DEBUG_REPORT_ERROR_BIT_EXT |
				VK_DEBUG_REPORT_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,

			.pfnCallback = (PFN_vkDebugReportCallbackEXT) onDebugReport
		};

		gotoIfError(clean, vkCheck(instanceExt->debugCreateReportCallback(
			instanceExt->instance, &callbackInfo, NULL, &instanceExt->debugReportCallback
		)))
	}

	instance->api = EGraphicsApi_Vulkan;
	instance->apiVersion = application.apiVersion;

clean:

	CharString_freex(&title);

	ListConstC8_freex(&enabledLayers);
	ListConstC8_freex(&enabledExtensions);

	ListVkExtensionProperties_freex(&extensions);
	ListVkLayerProperties_freex(&layers);

	return err;
}

Bool VK_WRAP_FUNC(GraphicsInstance_free)(GraphicsInstance *inst, Allocator alloc) {

	(void)alloc;

	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(inst, Vk);

	if(instanceExt->debugDestroyReportCallback)
		instanceExt->debugDestroyReportCallback(instanceExt->instance, instanceExt->debugReportCallback, NULL);

	vkDestroyInstance(instanceExt->instance, NULL);

	return true;
}

const C8 *reqExtensionsName[] = {
	"VK_KHR_push_descriptor",
	"VK_KHR_synchronization2",
	"VK_KHR_swapchain"
};

U64 reqExtensionsNameCount = sizeof(reqExtensionsName) / sizeof(reqExtensionsName[0]);

const C8 *optExtensionsName[] = {
	"VK_KHR_performance_query",
	"VK_KHR_ray_tracing_pipeline",
	"VK_KHR_ray_query",
	"VK_KHR_acceleration_structure",
	"VK_NV_ray_tracing_motion_blur",
	"VK_NV_ray_tracing_invocation_reorder",
	"VK_EXT_mesh_shader",
	"VK_KHR_fragment_shading_rate",
	"VK_KHR_dynamic_rendering",
	"VK_EXT_opacity_micromap",
	"VK_NV_displacement_micromap",
	"VK_EXT_shader_atomic_float",
	"VK_KHR_deferred_host_operations",
	"VK_NV_ray_tracing_validation",
	"VK_NV_compute_shader_derivatives",
	"VK_KHR_maintenance4"
};

U64 optExtensionsNameCount = sizeof(optExtensionsName) / sizeof(optExtensionsName[0]);

#define getDeviceProperties(Condition, StructName, Name, StructType)			\
	StructName Name = (StructName) { .sType = StructType };						\
	if(Condition) {																\
		*propertiesNext = &Name;												\
		propertiesNext = &Name.pNext;											\
	}

#define getDeviceFeatures(Condition, StructName, Name, StructType)				\
	StructName Name = (StructName) { .sType = StructType };						\
	if(Condition) {																\
		*featuresNext = &Name;													\
		featuresNext = &Name.pNext;												\
	}

TList(VkPhysicalDevice);
TListImpl(VkPhysicalDevice);

Error VK_WRAP_FUNC(GraphicsInstance_getDeviceInfos)(const GraphicsInstance *inst, ListGraphicsDeviceInfo *result) {

	if(!inst || !result)
		return Error_nullPointer(!inst ? 0 : 2, "VkGraphicsInstance_getDeviceInfos()::inst and result are required");

	if(result->ptr)
		return Error_invalidParameter(1, 0, "VkGraphicsInstance_getDeviceInfos()::result isn't empty, may indicate memleak");

	VkGraphicsInstance *graphicsExt = GraphicsInstance_ext(inst, Vk);

	U32 deviceCount = 0;
	Error err = vkCheck(vkEnumeratePhysicalDevices(graphicsExt->instance, &deviceCount, NULL));

	if(err.genericError)
		return err;

	ListVkPhysicalDevice temp = (ListVkPhysicalDevice) { 0 };
	ListGraphicsDeviceInfo temp2 = (ListGraphicsDeviceInfo) { 0 };
	ListVkLayerProperties temp3 = (ListVkLayerProperties) { 0 };
	ListVkExtensionProperties temp4 = (ListVkExtensionProperties) { 0 };

	gotoIfError(clean, ListVkPhysicalDevice_createx(deviceCount, &temp))
	gotoIfError(clean, ListGraphicsDeviceInfo_reservex(&temp2, deviceCount))

	gotoIfError(clean, vkCheck(vkEnumeratePhysicalDevices(graphicsExt->instance, &deviceCount, temp.ptrNonConst)))

	for (U32 i = 0, j = 0; i < deviceCount; ++i) {

		VkPhysicalDevice dev = temp.ptr[i];

		//Get extensions and layers

		U32 layerCount = 0, extensionCount = 0;

		gotoIfError(clean, vkCheck(vkEnumerateDeviceLayerProperties(dev, &layerCount, NULL)))
		gotoIfError(clean, ListVkLayerProperties_resizex(&temp3, layerCount))
		gotoIfError(clean, vkCheck(vkEnumerateDeviceLayerProperties(dev, &layerCount, temp3.ptrNonConst)))

		gotoIfError(clean, vkCheck(vkEnumerateDeviceExtensionProperties(dev, NULL, &extensionCount, NULL)))
		gotoIfError(clean, ListVkExtensionProperties_resizex(&temp4, extensionCount))
		gotoIfError(clean, vkCheck(vkEnumerateDeviceExtensionProperties(dev, NULL, &extensionCount, temp4.ptrNonConst)))

		//Log device for debugging

		if(inst->flags & EGraphicsInstanceFlags_IsVerbose) {

			Log_debugLnx("Supported device layers:");

			for(U32 k = 0; k < layerCount; ++k)
				Log_debugLnx("\t%s", temp3.ptr[k].layerName);

			Log_debugLnx("Supported device extensions:");
		}

		//Check if all required extensions are present

		Bool reqExtensions[sizeof(reqExtensionsName) / sizeof(reqExtensionsName[0])] = { 0 };
		Bool optExtensions[sizeof(optExtensionsName) / sizeof(optExtensionsName[0])] = { 0 };

		for(U64 k = 0; k < extensionCount; ++k) {

			const C8 *name = temp4.ptr[k].extensionName;

			if(inst->flags & EGraphicsInstanceFlags_IsVerbose)
				Log_debugLnx("\t%s", name);

			//Check if required extension

			Bool found = false;

			for(U64 l = 0; l < sizeof(reqExtensions); ++l)
				if (CharString_equalsStringSensitive(
					CharString_createRefCStrConst(reqExtensionsName[l]),
					CharString_createRefCStrConst(name)
				)) {
					reqExtensions[l] = true;
					found = true;
					break;
				}

			//Check if optional extension

			if (!found)
				for(U64 l = 0; l < sizeof(optExtensions); ++l)
					if (CharString_equalsStringSensitive(
						CharString_createRefCStrConst(optExtensionsName[l]),
						CharString_createRefCStrConst(name)
					)) {
						optExtensions[l] = true;
						break;
					}
		}

		Bool supported = true;
		U64 firstUnavailable = 0;

		for(U64 k = 0; k < sizeof(reqExtensions); ++k)
			if(!reqExtensions[k] && CharString_calcStrLen(reqExtensionsName[k], U64_MAX)) {
				supported = false;
				firstUnavailable = k;
				break;
			}

		if (!supported) {

			Log_debugLnx(
				"Vulkan: Unsupported device %"PRIu32", one of the required extensions wasn't present (%s)",
				i, reqExtensionsName[firstUnavailable]
			);

			continue;
		}

		//Grab limits and features

		//Build up list of properties

		VkPhysicalDeviceProperties2 properties2 = (VkPhysicalDeviceProperties2) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
		};

		void **propertiesNext = &properties2.pNext;

		getDeviceProperties(
			true, VkPhysicalDeviceSubgroupProperties, subgroup, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES
		)

		getDeviceProperties(
			optExtensions[EOptExtensions_MeshShader],
			VkPhysicalDeviceMeshShaderPropertiesEXT,
			meshShaderProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
		)

		getDeviceProperties(
			true,
			VkPhysicalDeviceMultiviewProperties,
			multiViewProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES
		)

		getDeviceProperties(
			optExtensions[EOptExtensions_VariableRateShading],
			VkPhysicalDeviceFragmentShadingRatePropertiesKHR,
			vrsProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR
		)

		getDeviceProperties(
			optExtensions[EOptExtensions_RayAcceleration],
			VkPhysicalDeviceAccelerationStructurePropertiesKHR,
			rtasProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
		)

		getDeviceProperties(
			optExtensions[EOptExtensions_RayPipeline],
			VkPhysicalDeviceRayTracingPipelinePropertiesKHR,
			rtpProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		)

		getDeviceProperties(
			optExtensions[EOptExtensions_RayReorder],
			VkPhysicalDeviceRayTracingInvocationReorderPropertiesNV,
			rayReorderProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV
		)

		getDeviceProperties(true, VkPhysicalDeviceIDProperties, id, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES)

		getDeviceProperties(
			true, VkPhysicalDeviceDriverProperties, driver, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
		)

		getDeviceProperties(
			true, VkPhysicalDeviceMaintenance3Properties, memorySizeAndDescriptorSets,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES
		)

		getDeviceProperties(
			optExtensions[EOptExtensions_Maintenance4], VkPhysicalDeviceMaintenance4Properties, maxBufferSize,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES
		)

		graphicsExt->getPhysicalDeviceProperties2(dev, &properties2);

		if(
			optExtensions[EOptExtensions_Maintenance4] && (
				maxBufferSize.maxBufferSize < 256 * MIBI || memorySizeAndDescriptorSets.maxMemoryAllocationSize < 256 * MIBI
			)
		) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", maxBufferSize and maxAllocationSize should exceed 256MiB", i);
			continue;
		}

		//Build up list of features

		VkPhysicalDeviceFeatures2 features2 = (VkPhysicalDeviceFeatures2) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
		};

		void **featuresNext = &features2.pNext;

		getDeviceFeatures(
			true,
			VkPhysicalDeviceDescriptorIndexingFeatures,
			descriptorIndexing,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_DynamicRendering],
			VkPhysicalDeviceDynamicRenderingFeatures,
			dynamicRendering,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES
		)

		getDeviceFeatures(
			true,
			VkPhysicalDeviceTimelineSemaphoreFeatures,
			semaphore,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES
		)

		getDeviceFeatures(
			true,
			VkPhysicalDeviceSynchronization2Features,
			sync2,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_MeshShader],
			VkPhysicalDeviceMeshShaderFeaturesEXT,
			meshShader,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RaytracingValidation],
			VkPhysicalDeviceRayTracingValidationFeaturesNV,
			rtValidation,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_ComputeDeriv],
			VkPhysicalDeviceComputeShaderDerivativesFeaturesNV ,
			computeDeriv,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV
		)

		getDeviceFeatures(
			true,
			VkPhysicalDeviceMultiviewFeatures,
			multiViewFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_VariableRateShading],
			VkPhysicalDeviceFragmentShadingRateFeaturesKHR,
			vrsFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayAcceleration],
			VkPhysicalDeviceAccelerationStructureFeaturesKHR,
			rtasFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayPipeline],
			VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
			rtpFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayQuery],
			VkPhysicalDeviceRayQueryFeaturesKHR,
			rayQueryFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayMotionBlur],
			VkPhysicalDeviceRayTracingMotionBlurFeaturesNV,
			rayMotionBlurFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayReorder],
			VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV,
			rayReorderFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayMicromapOpacity],
			VkPhysicalDeviceOpacityMicromapFeaturesEXT,
			rayOpacityMicroFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayMicromapDisplacement],
			VkPhysicalDeviceDisplacementMicromapFeaturesNV,
			rayDispMicroFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV
		)

		getDeviceFeatures(
			true,
			VkPhysicalDeviceShaderFloat16Int8Features,
			float16,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES
		)

		getDeviceFeatures(
			true,
			VkPhysicalDeviceShaderAtomicInt64Features,
			atomics64,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_AtomicF32],
			VkPhysicalDeviceShaderAtomicFloatFeaturesEXT,
			atomicsF32,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT
		)

		getDeviceFeatures(
			optExtensions[EOptExtensions_PerfQuery],
			VkPhysicalDevicePerformanceQueryFeaturesKHR,
			perfQuery,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR
		)

		getDeviceFeatures(
			true, VkPhysicalDeviceBufferDeviceAddressFeaturesKHR, deviceAddress,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR
		)

		graphicsExt->getPhysicalDeviceFeatures2(dev, &features2);

		//Query

		VkPhysicalDeviceProperties properties = properties2.properties;
		VkPhysicalDeviceFeatures features = features2.features;
		VkPhysicalDeviceLimits limits = properties.limits;

		if (limits.nonCoherentAtomSize > 256) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", nonCoherentAtomSize needs to be up to 256", i);
			continue;
		}

		//Ensure device is compatible first

		if(
			!features.shaderInt16 ||
			!features.shaderSampledImageArrayDynamicIndexing ||
			!features.shaderStorageBufferArrayDynamicIndexing ||
			!features.shaderUniformBufferArrayDynamicIndexing ||
			!features.samplerAnisotropy ||
			!features.drawIndirectFirstInstance ||
			!features.independentBlend ||
			!features.imageCubeArray ||
			!features.fullDrawIndexUint32 ||
			!features.depthClamp ||
			!features.depthBiasClamp ||
			!(features.textureCompressionBC || features.textureCompressionASTC_LDR) ||
			!features.multiDrawIndirect ||
			!features.sampleRateShading
		) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", one of the required features wasn't enabled", i);
			continue;
		}

		VkSampleCountFlags requiredMSAA = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;

		VkSampleCountFlags allMSAA =
			limits.framebufferColorSampleCounts &
			limits.framebufferDepthSampleCounts &
			limits.framebufferNoAttachmentsSampleCounts &
			limits.framebufferStencilSampleCounts &
			limits.sampledImageColorSampleCounts &
			limits.sampledImageDepthSampleCounts &
			limits.sampledImageIntegerSampleCounts &
			limits.sampledImageStencilSampleCounts;

		if (
			limits.maxColorAttachments < 8 ||
			limits.maxFragmentOutputAttachments < 8 ||
			limits.maxDescriptorSetInputAttachments < 7 ||
			limits.maxPerStageDescriptorInputAttachments < 7 ||
			(allMSAA & requiredMSAA) != requiredMSAA ||
			limits.maxComputeSharedMemorySize < 16 * KIBI ||
			limits.maxComputeWorkGroupCount[0] < U16_MAX ||
			limits.maxComputeWorkGroupCount[1] < U16_MAX ||
			limits.maxComputeWorkGroupCount[2] < U16_MAX ||
			limits.maxComputeWorkGroupSize[0] < 1024 ||
			limits.maxComputeWorkGroupSize[1] < 1024 ||
			limits.maxComputeWorkGroupSize[2] < 64 ||
			limits.maxFragmentCombinedOutputResources < 16 ||
			limits.maxFragmentInputComponents < 112 ||
			limits.maxFramebufferWidth < 16 * KIBI ||
			limits.maxFramebufferHeight < 16 * KIBI ||
			limits.maxImageDimension1D < 16 * KIBI ||
			limits.maxImageDimension2D < 16 * KIBI ||
			limits.maxImageDimensionCube < 16 * KIBI ||
			limits.maxViewportDimensions[0] < 16 * KIBI ||
			limits.maxViewportDimensions[1] < 16 * KIBI ||
			limits.maxFramebufferLayers < 256 ||
			limits.maxImageDimension3D < 256 ||
			limits.maxImageArrayLayers < 256 ||
			limits.maxPushConstantsSize < 128 ||
			limits.maxSamplerAllocationCount < 1024 ||
			limits.maxSamplerAnisotropy < 16 ||
			limits.maxStorageBufferRange < 250 * MEGA ||
			limits.maxSamplerLodBias < 4 ||
			limits.maxUniformBufferRange < 64 * KIBI ||
			limits.maxVertexInputAttributeOffset < 2047 ||
			limits.maxVertexInputAttributes < 16 ||
			limits.maxVertexInputBindings < 16 ||
			limits.maxVertexInputBindingStride < 2048 ||
			limits.maxVertexOutputComponents < 124 ||
			limits.maxMemoryAllocationCount < 4096 ||
			limits.maxBoundDescriptorSets < 4 ||
			limits.viewportBoundsRange[0] > -32768 ||
			limits.viewportBoundsRange[1] < 32767
		) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", one of the required limit wasn't sufficient", i);
			continue;
		}

		//Disable tesselation shaders if minimum isn't met

		if (
			!features.tessellationShader || (
				U64_min(
					U64_min(
						U64_min(
							limits.maxTessellationControlPerVertexInputComponents,
							limits.maxTessellationControlPerVertexOutputComponents
						),
						limits.maxTessellationEvaluationInputComponents
					),
					limits.maxTessellationEvaluationOutputComponents
				) < 124 ||
				limits.maxTessellationControlTotalOutputComponents < 4088 ||
				limits.maxTessellationControlPerPatchOutputComponents < 120 ||
				limits.maxTessellationGenerationLevel < 64 ||
				limits.maxTessellationPatchSize < 32
			)
		) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", tesselation isn't supported but is required", i);
			continue;
		}

		//Disable geometry shaders if minimum isn't met

		if (
			features.geometryShader && (
				limits.maxGeometryInputComponents < 64 ||
				limits.maxGeometryOutputComponents < 128 ||
				limits.maxGeometryOutputVertices < 256 ||
				limits.maxGeometryShaderInvocations < 32 ||
				limits.maxGeometryTotalOutputComponents < 1024
			)
		)
			features.geometryShader = false;

		//Convert enums

		//Device type

		EGraphicsDeviceType type = EGraphicsDeviceType_Other;

		switch (properties.deviceType) {
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:		type = EGraphicsDeviceType_Dedicated;	break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:	type = EGraphicsDeviceType_Integrated;	break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:				type = EGraphicsDeviceType_CPU;			break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:		type = EGraphicsDeviceType_Simulated;	break;
			default:																				break;
		}

		//Vendor

		EGraphicsVendorId vendor = EGraphicsVendorId_Unknown;

		switch (properties.vendorID) {
			case EGraphicsVendorPCIE_NV:					vendor = EGraphicsVendorId_NV;			break;
			case EGraphicsVendorPCIE_AMD:					vendor = EGraphicsVendorId_AMD;			break;
			case EGraphicsVendorPCIE_ARM:					vendor = EGraphicsVendorId_ARM;			break;
			case EGraphicsVendorPCIE_QCOM:					vendor = EGraphicsVendorId_QCOM;		break;
			case EGraphicsVendorPCIE_INTC:					vendor = EGraphicsVendorId_INTC;		break;
			case EGraphicsVendorPCIE_IMGT:					vendor = EGraphicsVendorId_IMGT;		break;
			case EGraphicsVendorPCIE_APPL:					vendor = EGraphicsVendorId_APPL;		break;
			default:																				break;
		}

		//Capabilities

		GraphicsDeviceCapabilities capabilities = (GraphicsDeviceCapabilities) { 0 };

		//Query features

		if(features.logicOp)
			capabilities.features |= EGraphicsFeatures_LogicOp;

		if(features.dualSrcBlend)
			capabilities.features |= EGraphicsFeatures_DualSrcBlend;

		if(features.shaderStorageImageMultisample)
			capabilities.features |= EGraphicsFeatures_WriteMSTexture;

		if(features.fillModeNonSolid)
			capabilities.features |= EGraphicsFeatures_Wireframe;

		if(!deviceAddress.bufferDeviceAddress) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", buffer device address is missing!", i);
			continue;
		}

		if(optExtensions[EOptExtensions_Maintenance4])
			capabilities.featuresExt |= EVkGraphicsFeatures_Maintenance4;

		//Force enable synchronization and timeline semaphores

		if (!semaphore.timelineSemaphore) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", Timeline semaphores unsupported!", i);
			continue;
		}

		if (!sync2.synchronization2) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", Synchronization 2 unsupported!", i);
			continue;
		}

		//Check if indexing is properly supported

		VkPhysicalDeviceDescriptorIndexingFeatures target = (VkPhysicalDeviceDescriptorIndexingFeatures) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
			.shaderUniformTexelBufferArrayDynamicIndexing = true,
			.shaderStorageTexelBufferArrayDynamicIndexing = true,
			.shaderUniformBufferArrayNonUniformIndexing = true,
			.shaderSampledImageArrayNonUniformIndexing = true,
			.shaderStorageBufferArrayNonUniformIndexing = true,
			.shaderStorageImageArrayNonUniformIndexing = true,
			.shaderUniformTexelBufferArrayNonUniformIndexing = true,
			.shaderStorageTexelBufferArrayNonUniformIndexing = true,
			.descriptorBindingSampledImageUpdateAfterBind = true,
			.descriptorBindingStorageImageUpdateAfterBind = true,
			.descriptorBindingStorageBufferUpdateAfterBind = true,
			.descriptorBindingUniformTexelBufferUpdateAfterBind = true,
			.descriptorBindingStorageTexelBufferUpdateAfterBind = true,
			.descriptorBindingUpdateUnusedWhilePending = true,
			.descriptorBindingPartiallyBound = true,
			.descriptorBindingVariableDescriptorCount = true,
			.runtimeDescriptorArray = true
		};

		Bool eq = false;

		for(U32 q = 0; q < 4; ++q) {

			target.shaderInputAttachmentArrayNonUniformIndexing = q & 1;
			target.descriptorBindingUniformBufferUpdateAfterBind = q >> 1;

			//We skip shaderInputAttachmentArrayDynamicIndexing as well by not starting there.

			U64 sz = sizeof(target) - offsetof(
				VkPhysicalDeviceDescriptorIndexingFeatures, shaderUniformTexelBufferArrayDynamicIndexing
			);

			eq = Buffer_eq(
				Buffer_createRefConst(&target.shaderUniformTexelBufferArrayDynamicIndexing, sz),
				Buffer_createRefConst(&descriptorIndexing.shaderUniformTexelBufferArrayDynamicIndexing, sz)
			);

			if(eq)
				break;
		}

		if(!eq) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", descriptor indexing isn't properly supported.", i);
			continue;
		}

		//Direct rendering

		if (
			(vendor == EGraphicsVendorId_NV || vendor == EGraphicsVendorId_AMD || vendor == EGraphicsVendorId_INTC) &&
			optExtensions[EOptExtensions_DynamicRendering] &&
			dynamicRendering.dynamicRendering
		)
			capabilities.features |= EGraphicsFeatures_DirectRendering;

		//Shader types

		if(features.geometryShader)
			capabilities.features |= EGraphicsFeatures_GeometryShader;

		//Multi draw

		if(limits.maxDrawIndirectCount >= GIBI)
			capabilities.features |= EGraphicsFeatures_MultiDrawIndirectCount;

		//Subgroup operations

		if(subgroup.subgroupSize != 1) {

			VkSubgroupFeatureFlags requiredSubOp =
				VK_SUBGROUP_FEATURE_BASIC_BIT |
				VK_SUBGROUP_FEATURE_VOTE_BIT |
				VK_SUBGROUP_FEATURE_BALLOT_BIT;

			if((subgroup.supportedOperations & requiredSubOp) != requiredSubOp) {
				Log_debugLnx("Vulkan: Unsupported device %"PRIu32", one of the required subgroup operations was missing!", i);
				continue;
			}

			if(subgroup.subgroupSize < 4 || subgroup.subgroupSize > 128) {
				Log_debugLnx("Vulkan: Unsupported device %"PRIu32", subgroup size is not in range 4-128!", i);
				continue;
			}

			capabilities.features |= EGraphicsFeatures_SubgroupOperations;
		}

		if(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)
			capabilities.features |= EGraphicsFeatures_SubgroupArithmetic;

		if(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)
			capabilities.features |= EGraphicsFeatures_SubgroupShuffle;

		//Multi view

		if(
			multiViewFeat.multiview &&
			multiViewProp.maxMultiviewViewCount >= 6 && multiViewProp.maxMultiviewInstanceIndex >= 134217727
		)
			capabilities.features |= EGraphicsFeatures_Multiview;

		//VRS

		if(
			optExtensions[EOptExtensions_VariableRateShading] &&
			vrsFeat.pipelineFragmentShadingRate &&
			vrsFeat.attachmentFragmentShadingRate &&
			vrsProp.maxFragmentSize.width >= 2 && vrsProp.maxFragmentSize.height >= 2 &&
			vrsProp.maxFragmentShadingRateCoverageSamples >= 16 &&
			vrsProp.maxFragmentShadingRateRasterizationSamples >= 4 &&
			vrsProp.fragmentShadingRateWithSampleMask
		)
			capabilities.features |= EGraphicsFeatures_VariableRateShading;

		//Mesh shaders

		if(optExtensions[EOptExtensions_MeshShader] && !(
			!meshShader.taskShader ||
			meshShaderProp.maxMeshMultiviewViewCount < 4 ||
			meshShaderProp.maxMeshOutputComponents < 127 ||
			meshShaderProp.maxMeshOutputLayers < 8 ||
			meshShaderProp.maxMeshOutputMemorySize < 32 * KIBI ||
			meshShaderProp.maxMeshOutputPrimitives < 256 ||
			meshShaderProp.maxMeshOutputVertices < 256 ||
			meshShaderProp.maxMeshPayloadAndOutputMemorySize < 47 * KIBI ||
			meshShaderProp.maxMeshPayloadAndSharedMemorySize < 28 * KIBI ||
			meshShaderProp.maxMeshSharedMemorySize < 28 * KIBI ||
			meshShaderProp.maxMeshWorkGroupCount[0] < U16_MAX ||
			meshShaderProp.maxMeshWorkGroupCount[1] < U16_MAX ||
			meshShaderProp.maxMeshWorkGroupCount[2] < U16_MAX ||
			meshShaderProp.maxMeshWorkGroupInvocations < 128 ||
			meshShaderProp.maxTaskWorkGroupInvocations < 128 ||
			meshShaderProp.maxMeshWorkGroupSize[0] < 128 ||
			meshShaderProp.maxMeshWorkGroupSize[1] < 128 ||
			meshShaderProp.maxMeshWorkGroupSize[2] < 128 ||
			meshShaderProp.maxTaskWorkGroupSize[0] < 128 ||
			meshShaderProp.maxTaskWorkGroupSize[1] < 128 ||
			meshShaderProp.maxTaskWorkGroupSize[2] < 128 ||
			meshShaderProp.maxMeshWorkGroupTotalCount < 4 * MIBI ||
			meshShaderProp.maxPreferredMeshWorkGroupInvocations < 32 ||
			meshShaderProp.maxPreferredTaskWorkGroupInvocations < 32 ||
			meshShaderProp.meshOutputPerPrimitiveGranularity < 32 ||
			meshShaderProp.meshOutputPerVertexGranularity < 32 ||
			meshShaderProp.maxTaskPayloadAndSharedMemorySize < 32 * KIBI ||
			meshShaderProp.maxTaskPayloadSize < 16 * KIBI ||
			meshShaderProp.maxTaskSharedMemorySize < 32 * KIBI ||
			meshShaderProp.maxTaskWorkGroupTotalCount < 4 * MIBI ||
			!meshShaderProp.prefersCompactPrimitiveOutput
		))
			capabilities.features |= EGraphicsFeatures_MeshShader;

		//Raytracing

		if(rtValidation.rayTracingValidation)
			capabilities.features |= EGraphicsFeatures_RayValidation;

		if(computeDeriv.computeDerivativeGroupLinear)
			capabilities.features |= EGraphicsFeatures_ComputeDeriv;

		if(!optExtensions[EOptExtensions_DeferredHostOperations])
			optExtensions[EOptExtensions_RayAcceleration] = false;

		if(!rtasFeat.descriptorBindingAccelerationStructureUpdateAfterBind)
			optExtensions[EOptExtensions_RayAcceleration] = false;

		if(optExtensions[EOptExtensions_RayAcceleration]) {

			//Disable raytracing if minimum limits aren't reached

			if(rtasFeat.accelerationStructure) {

				if(
					!(
						U64_min(
							U64_min(
								U64_min(
									rtasProp.maxPerStageDescriptorAccelerationStructures,
									rtasProp.maxPerStageDescriptorUpdateAfterBindAccelerationStructures
								),
								rtasProp.maxDescriptorSetAccelerationStructures
							),
							rtasProp.maxDescriptorSetUpdateAfterBindAccelerationStructures
						) >= 16 &&
						U64_min(rtasProp.maxGeometryCount, rtasProp.maxInstanceCount) >= 16777215 &&
						rtasProp.maxPrimitiveCount >= GIBI / 2 - 1
					)
				)
					rtasFeat.accelerationStructure = false;
			}

			//Check if raytracing can be enabled

			if (rtasFeat.accelerationStructure) {

				//Query raytracing pipeline

				if(optExtensions[EOptExtensions_RayPipeline]) {

					if(
						!rtpFeat.rayTracingPipeline || !rtpFeat.rayTraversalPrimitiveCulling ||
						!rtpFeat.rayTracingPipelineTraceRaysIndirect
					)
						optExtensions[EOptExtensions_RayPipeline] = false;
				}

				//Query ray query

				if(optExtensions[EOptExtensions_RayQuery] && !rayQueryFeat.rayQuery)
					optExtensions[EOptExtensions_RayQuery] = false;

				//Disable if invalid state

				if (
					optExtensions[EOptExtensions_RayPipeline] && (
						rtpProp.maxRayDispatchInvocationCount < GIBI ||
						rtpProp.maxRayHitAttributeSize < 32 ||
						rtpProp.maxRayRecursionDepth < 1 ||
						rtpProp.maxShaderGroupStride < 4096 ||
						rtpProp.shaderGroupHandleSize != 32 ||
						(rtpProp.shaderGroupBaseAlignment != 32 && rtpProp.shaderGroupBaseAlignment != 64)
					)
				) {
					optExtensions[EOptExtensions_RayQuery] = false;
					optExtensions[EOptExtensions_RayPipeline] = false;
				}

				//Enable extension

				if(optExtensions[EOptExtensions_RayQuery])
					capabilities.features |= EGraphicsFeatures_RayQuery;

				if(optExtensions[EOptExtensions_RayPipeline])
					capabilities.features |= EGraphicsFeatures_RayPipeline;

				if(optExtensions[EOptExtensions_RayQuery] || optExtensions[EOptExtensions_RayPipeline]) {

					capabilities.features |= EGraphicsFeatures_Raytracing;

					if(rayMotionBlurFeat.rayTracingMotionBlur)
						capabilities.features |= EGraphicsFeatures_RayMotionBlur;

					//Indirect must allow motion blur too (if it's enabled)

					if(
						rayMotionBlurFeat.rayTracingMotionBlur &&
						!rayMotionBlurFeat.rayTracingMotionBlurPipelineTraceRaysIndirect
					)
						capabilities.features &= ~EGraphicsFeatures_RayMotionBlur;

					if(rayReorderFeat.rayTracingInvocationReorder && rayReorderProp.rayTracingInvocationReorderReorderingHint)
						capabilities.features |= EGraphicsFeatures_RayReorder;

					if(rayOpacityMicroFeat.micromap)
						capabilities.features |= EGraphicsFeatures_RayMicromapOpacity;

					if(rayDispMicroFeat.displacementMicromap)
						capabilities.features |= EGraphicsFeatures_RayMicromapDisplacement;
				}
			}
		}

		//Get data types

		if(float16.shaderFloat16)
			capabilities.dataTypes |= EGraphicsDataTypes_F16;

		if(features.shaderInt64)
			capabilities.dataTypes |= EGraphicsDataTypes_I64;

		if(features.shaderFloat64)
			capabilities.dataTypes |= EGraphicsDataTypes_F64;

		//Vulkan always supports compute to swapchain

		capabilities.features |= EGraphicsFeatures_SwapchainCompute;

		//Atomics

		if(atomics64.shaderBufferInt64Atomics)
			capabilities.dataTypes |= EGraphicsDataTypes_AtomicI64;

		if(atomicsF32.shaderBufferFloat32AtomicAdd && atomicsF32.shaderBufferFloat32Atomics)
			capabilities.dataTypes |= EGraphicsDataTypes_AtomicF32;

		if(atomicsF32.shaderBufferFloat64AtomicAdd && atomicsF32.shaderBufferFloat64Atomics)
			capabilities.dataTypes |= EGraphicsDataTypes_AtomicF64;

		//Compression

		if(features.textureCompressionASTC_LDR)
			capabilities.dataTypes |= EGraphicsDataTypes_ASTC;

		if(features.textureCompressionBC)
			capabilities.dataTypes |= EGraphicsDataTypes_BCn;

		//MSAA

		if(allMSAA & VK_SAMPLE_COUNT_2_BIT)
			capabilities.dataTypes |= EGraphicsDataTypes_MSAA2x;

		if(allMSAA & VK_SAMPLE_COUNT_8_BIT)
			capabilities.dataTypes |= EGraphicsDataTypes_MSAA8x;

		//Enforce support for the least descriptors

		if(
			limits.maxPerStageDescriptorSamplers < 16 ||
			limits.maxPerStageDescriptorUniformBuffers < 12 ||
			limits.maxPerStageDescriptorStorageBuffers < 8 ||
			limits.maxPerStageDescriptorSampledImages < 16 ||
			limits.maxPerStageDescriptorStorageImages < 4 ||
			limits.maxPerStageResources < 44 ||
			limits.maxDescriptorSetSamplers < 80 ||
			limits.maxDescriptorSetUniformBuffers < 72 ||
			limits.maxDescriptorSetStorageBuffers < 24 ||
			limits.maxDescriptorSetSampledImages < 96 ||
			limits.maxDescriptorSetStorageImages < 24
		) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", graphics binding tier not supported!", i);
			continue;
		}

		//Hopefully enable bindless

		if(
			limits.maxPerStageDescriptorSamplers >= 2 * KIBI &&
			limits.maxPerStageDescriptorUniformBuffers >= 12 &&
			limits.maxPerStageDescriptorStorageBuffers >= 500 * KIBI &&
			limits.maxPerStageDescriptorSampledImages >= 250 * KIBI &&
			limits.maxPerStageDescriptorStorageImages >= 250 * KIBI &&
			limits.maxPerStageResources >= MEGA &&
			limits.maxDescriptorSetSamplers >= 2 * KIBI &&
			limits.maxDescriptorSetUniformBuffers >= 12 &&
			limits.maxDescriptorSetStorageBuffers >= 500 * KIBI &&
			limits.maxDescriptorSetSampledImages >= 250 * KIBI &&
			limits.maxDescriptorSetStorageImages >= 250 * KIBI
		)
			capabilities.features |= EGraphicsFeatures_Bindless;

		//Enforce format support
		//We don't enforce VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT because the standard guarantees it.

		VkFormat toSupport[]  = {

			//R8s, RG8s, RGBA8s

			VK_FORMAT_R8_SNORM,
			VK_FORMAT_R8G8_SNORM,
			VK_FORMAT_R8G8B8A8_SNORM,

			//R16, R16s
			//RG16, RG16s

			VK_FORMAT_R16_SNORM,
			VK_FORMAT_R16G16_SNORM,

			VK_FORMAT_R16_UNORM,
			VK_FORMAT_R16G16_UNORM,

			//RGBA16, RGBA16s

			VK_FORMAT_R16G16B16A16_UNORM,
			VK_FORMAT_R16G16B16A16_SNORM,

			//BGRA8

			VK_FORMAT_B8G8R8A8_UNORM,

			//D32

			VK_FORMAT_D32_SFLOAT
		};

		VkFormatFeatureFlags reqFormatDef =
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
			VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
			VK_FORMAT_FEATURE_BLIT_DST_BIT |
			VK_FORMAT_FEATURE_BLIT_SRC_BIT |
			VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
			VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;

		VkFormatFeatureFlags depthStencilDef =
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
			VK_FORMAT_FEATURE_BLIT_SRC_BIT |
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

		VkFormatFeatureFlags reqFormatFeatFlags[] = {

			//R8s, RG8s, RGBA8s
			//R16, R16s
			//RG16, RG16s
			//RGBA16, RGBA16s

			reqFormatDef, reqFormatDef, reqFormatDef,
			reqFormatDef, reqFormatDef,
			reqFormatDef, reqFormatDef,

			reqFormatDef, reqFormatDef,

			//BGRA8

			VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,

			//D32

			depthStencilDef
		};

		Bool unsupported = false;

		for(U64 k = 0; k < sizeof(toSupport) / sizeof(VkFormat); ++k) {

			VkFormatProperties formatInfo = (VkFormatProperties) { 0 };
			vkGetPhysicalDeviceFormatProperties(dev, toSupport[k], &formatInfo);

			VkFormatFeatureFlags reqk = reqFormatFeatFlags[k];

			if((formatInfo.optimalTilingFeatures & reqk) != reqk) {
				unsupported = true;
				break;
			}
		}

		if (unsupported) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", required texture format not supported!", i);
			continue;
		}

		//Query for memory size

		VkGraphicsDevice fakeDevice = (VkGraphicsDevice) { 0 };
		vkGetPhysicalDeviceMemoryProperties(dev, &fakeDevice.memoryProperties);

		U64 cpuHeapSize = 0;
		gotoIfError(clean, VkDeviceMemoryAllocator_findMemory(&fakeDevice, true, U32_MAX, NULL, NULL, &cpuHeapSize))
		cpuHeapSize &= (U64)I64_MAX;

		U64 gpuHeapSize = 0;
		gotoIfError(clean, VkDeviceMemoryAllocator_findMemory(&fakeDevice, false, U32_MAX, NULL, NULL, &gpuHeapSize))
		gpuHeapSize &= (U64)I64_MAX;

		if(cpuHeapSize < 512 * MIBI || gpuHeapSize < 512 * MIBI) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", not enough VRAM or RAM (requires 512MB each).", i);
			continue;
		}

		capabilities.sharedMemory = cpuHeapSize;
		capabilities.dedicatedMemory = gpuHeapSize;

		//Get optional formats (RGB32u, RGB32i, RGB32f)

		typedef struct OptionalFormat {

			VkFormat format;

			EGraphicsDataTypes optFormat;

			VkFormatFeatureFlags flags;

			Bool support;
			U8 pad[3];

		} OptionalFormat;

		OptionalFormat optionalFormats[]  = {

			(OptionalFormat) {
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.optFormat = EGraphicsDataTypes_RGB32f,
				.flags = reqFormatDef
			},

			(OptionalFormat) {
				.format = VK_FORMAT_R32G32B32_SINT,
				.optFormat = EGraphicsDataTypes_RGB32i,
				.flags = reqFormatDef
			},

			(OptionalFormat) {
				.format = VK_FORMAT_R32G32B32_UINT,
				.optFormat = EGraphicsDataTypes_RGB32u,
				.flags = reqFormatDef
			},

			(OptionalFormat) {
				.format = VK_FORMAT_D24_UNORM_S8_UINT,
				.optFormat = EGraphicsDataTypes_D24S8,
				.flags = depthStencilDef
			},

			(OptionalFormat) {
				.format = VK_FORMAT_D32_SFLOAT_S8_UINT,
				.optFormat = EGraphicsDataTypes_D32S8,
				.flags = depthStencilDef
			},

			(OptionalFormat) {
				.format = VK_FORMAT_S8_UINT,
				.optFormat = EGraphicsDataTypes_S8,
				.flags = depthStencilDef
			}
		};

		for(U64 k = 0; k < sizeof(optionalFormats) / sizeof(OptionalFormat); ++k) {

			VkFormatProperties formatInfo = (VkFormatProperties) { 0 };
			vkGetPhysicalDeviceFormatProperties(dev, optionalFormats[k].format, &formatInfo);

			if((formatInfo.optimalTilingFeatures & optionalFormats[k].flags) == optionalFormats[k].flags)
				capabilities.dataTypes |= optionalFormats[k].optFormat;
		}

		//Grab LUID/UUID

		if(id.deviceLUIDValid)
			capabilities.features |= EGraphicsFeatures_LUID;

		//API dependent stuff for the runtime

		if(perfQuery.performanceCounterQueryPools)
			capabilities.featuresExt |= EVkGraphicsFeatures_PerfQuery;

		capabilities.dataTypes |= EGraphicsDataTypes_I16;

		capabilities.maxBufferSize = maxBufferSize.maxBufferSize ? maxBufferSize.maxBufferSize : 256 * MIBI;
		capabilities.maxAllocationSize = memorySizeAndDescriptorSets.maxMemoryAllocationSize;

		//Fully converted type

		gotoIfError(clean, ListGraphicsDeviceInfo_resize(&temp2, temp2.length + 1, (Allocator){ 0 }))

		GraphicsDeviceInfo *info = temp2.ptrNonConst + j;

		*info = (GraphicsDeviceInfo) {
			.type = type,
			.vendor = vendor,
			.id = i,
			.capabilities = capabilities,
			.luid = *(const U64*)id.deviceLUID,
			.uuid = { ((const U64*)id.deviceUUID)[0], ((const U64*)id.deviceUUID)[1] },
			.ext = dev
		};

		Buffer_copy(
			Buffer_createRef(info->name, sizeof(info->name)),
			Buffer_createRefConst(properties.deviceName, sizeof(properties.deviceName))
		);

		Buffer_copy(
			Buffer_createRef(info->driverInfo, sizeof(info->driverInfo)),
			Buffer_createRefConst(driver.driverInfo, sizeof(driver.driverInfo))
		);

		++j;
	}

	if(!temp2.length)
		gotoIfError(clean, Error_unsupportedOperation(0, "VkGraphicsInstance_getDeviceInfos() no supported OxC3 device found"))

	*result = temp2;

clean:

	if(err.genericError)
		ListGraphicsDeviceInfo_freex(&temp2);

	ListVkPhysicalDevice_freex(&temp);
	ListVkLayerProperties_freex(&temp3);
	ListVkExtensionProperties_freex(&temp4);
	return err;
}
