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

#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/platform_types.h"
#include "types/error.h"
#include "types/buffer.h"

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

	messageCode;
	object;
	location;
	objectType;
	userData;
	layerPrefix;

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		Log_errorLn("Error: %s", message);

	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		Log_warnLn("Warning: %s", message);

	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		Log_performanceLn("Performance warning: %s", message);

	else Log_debugLn("Debug: %s", message);

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
		_gotoIfError(clean, Error_nullPointer(0));												\
																								\
	*(void**)&result = (void*) v;																\
}

Bool GraphicsInstance_free(GraphicsInstance *data, Allocator alloc);

Error GraphicsInstance_create(GraphicsApplicationInfo info, Bool isVerbose, GraphicsInstanceRef **instanceRef) {

	U32 layerCount = 0, extensionCount = 0;
	List extensions = (List) { 0 }, layers = (List) { 0 };
	CharString title = (CharString) { 0 };

	List enabledExtensions = List_createEmpty(sizeof(const C8*));
	List enabledLayers = List_createEmpty(sizeof(const C8*));

	Error err = RefPtr_createx(
		(U32)(sizeof(GraphicsInstance) + sizeof(VkGraphicsInstance)), 
		(ObjectFreeFunc) GraphicsInstance_free, 
		EGraphicsTypeId_GraphicsInstance, 
		instanceRef
	);

	if(err.genericError)
		return err;

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	_gotoIfError(clean, vkCheck(vkEnumerateInstanceLayerProperties(&layerCount, NULL)));
	_gotoIfError(clean, vkCheck(vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL)));

	_gotoIfError(clean, CharString_createCopyx(info.name, &title));
	_gotoIfError(clean, List_createx(extensionCount, sizeof(VkExtensionProperties), &extensions));
	_gotoIfError(clean, List_createx(layerCount, sizeof(VkLayerProperties), &layers));

	_gotoIfError(clean, List_reservex(&enabledLayers, layerCount));
	_gotoIfError(clean, List_reservex(&enabledExtensions, extensionCount));

	_gotoIfError(clean, vkCheck(vkEnumerateInstanceLayerProperties(&layerCount, (VkLayerProperties*) layers.ptr)));

	_gotoIfError(clean, vkCheck(vkEnumerateInstanceExtensionProperties(
		NULL, &extensionCount, (VkExtensionProperties*) extensions.ptr
	)));

	Bool supportsDebug[2] = { 0 };

	#ifndef NDEBUG

		CharString debugReport = CharString_createConstRefCStr("VK_EXT_debug_report");
		CharString debugUtils = CharString_createConstRefCStr("VK_EXT_debug_utils");

		if(isVerbose)
			Log_debugLn("Supported extensions:");

	#endif

	Bool supportsSurface = false;
	Bool supportsSurfacePlatform = false;
	Bool supportsColorSpace = false;

	CharString surfaceKhr = CharString_createConstRefCStr("VK_KHR_surface");
	CharString surfacePlatform = CharString_createConstRefCStr(_VK_SURFACE_EXT);
	CharString swapchainColorspace = CharString_createConstRefCStr("VK_EXT_swapchain_colorspace");

	for(U64 i = 0; i < extensionCount; ++i) {

		const C8 *name = ((VkExtensionProperties*)extensions.ptr)[i].extensionName;
		CharString nameStr = CharString_createConstRefCStr(name);

		if(CharString_equalsString(nameStr, surfaceKhr, EStringCase_Sensitive))
			supportsSurface = true;

		else if(CharString_equalsString(nameStr, surfacePlatform, EStringCase_Sensitive))
			supportsSurfacePlatform = true;

		else if(CharString_equalsString(nameStr, swapchainColorspace, EStringCase_Sensitive))
			supportsColorSpace = true;

		#ifndef NDEBUG

			else if(CharString_equalsString(nameStr, debugReport, EStringCase_Sensitive))
				supportsDebug[0] = true;

			else if(CharString_equalsString(nameStr, debugUtils, EStringCase_Sensitive))
				supportsDebug[1] = true;

		#endif

		if(isVerbose)
			Log_debugLn("\t%s", name);
	}

	if(isVerbose) {

		Log_debugLn("Supported layers:");

		for(U64 i = 0; i < layerCount; ++i)
			Log_debugLn("\t%s", ((VkLayerProperties*)layers.ptr)[i].layerName);
	}

	#ifndef NDEBUG

		if(supportsDebug[0]) {
			Buffer ptrBuffer = Buffer_createConstRef(&debugReport.ptr, sizeof(const C8*));
			_gotoIfError(clean, List_pushBackx(&enabledExtensions, ptrBuffer));
		}

		if(supportsDebug[1]) {
			Buffer ptrBuffer = Buffer_createConstRef(&debugUtils.ptr, sizeof(const C8*));
			_gotoIfError(clean, List_pushBackx(&enabledExtensions, ptrBuffer));
		}

	#endif

	//Force physical device properties and external memory

	const C8 *physicalProperties = "VK_KHR_get_physical_device_properties2";
	Buffer ptrBuffer = Buffer_createConstRef(&physicalProperties, sizeof(const C8*));
	_gotoIfError(clean, List_pushBackx(&enabledExtensions, ptrBuffer));

	const C8 *externalMem = "VK_KHR_external_memory_capabilities";
	ptrBuffer = Buffer_createConstRef(&externalMem, sizeof(const C8*));
	_gotoIfError(clean, List_pushBackx(&enabledExtensions, ptrBuffer));

	//Enable so we can use swapchain khr

	if(supportsSurface && supportsSurfacePlatform) {

		ptrBuffer = Buffer_createConstRef(&surfaceKhr.ptr, sizeof(const C8*));
		_gotoIfError(clean, List_pushBackx(&enabledExtensions, ptrBuffer));

		ptrBuffer = Buffer_createConstRef(&surfacePlatform.ptr, sizeof(const C8*));
		_gotoIfError(clean, List_pushBackx(&enabledExtensions, ptrBuffer));

		if (supportsColorSpace) {
			ptrBuffer = Buffer_createConstRef(&swapchainColorspace.ptr, sizeof(const C8*));
			_gotoIfError(clean, List_pushBackx(&enabledExtensions, ptrBuffer));
		}
	}

	_gotoIfError(clean, VkGraphicsInstance_getLayers(&enabledLayers));

	VkApplicationInfo application = (VkApplicationInfo) {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = title.ptr,
		.applicationVersion = info.version,
		.pEngineName = "OxC3",
		.engineVersion = VK_MAKE_VERSION(0, 2, 0),
		.apiVersion = VK_MAKE_VERSION(1, 2, 0)
	};

	VkInstanceCreateInfo instanceInfo = (VkInstanceCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &application,
		.enabledLayerCount = (U32) enabledLayers.length,
		.ppEnabledLayerNames = (const char* const*) enabledLayers.ptr,
		.enabledExtensionCount = (U32) enabledExtensions.length,
		.ppEnabledExtensionNames = (const char* const*) enabledExtensions.ptr
	};

	if(isVerbose) {

		Log_debugLn("Enabling layers:");

		for(U64 i = 0; i < enabledLayers.length; ++i)
			Log_debugLn("\t%s", ((const char* const*)enabledLayers.ptr)[i]);

		Log_debugLn("Enabling extensions:");

		for(U64 i = 0; i < enabledExtensions.length; ++i)
			Log_debugLn("\t%s", ((const char* const*)enabledExtensions.ptr)[i]);

	}

	//Create instance

	_gotoIfError(clean, vkCheck(vkCreateInstance(&instanceInfo, NULL, &instanceExt->instance)));
	
	//Load functions

	vkExtension(clean, vkGetImageMemoryRequirements2KHR, instanceExt->getImageMemoryRequirements2);
	vkExtension(clean, vkGetBufferMemoryRequirements2KHR, instanceExt->getBufferMemoryRequirements2);

	vkExtension(clean, vkGetPhysicalDeviceFeatures2KHR, instanceExt->getPhysicalDeviceFeatures2);
	vkExtension(clean, vkGetPhysicalDeviceProperties2KHR, instanceExt->getPhysicalDeviceProperties2);

	vkExtension(clean, vkCmdPipelineBarrier2KHR, instanceExt->cmdPipelineBarrier2);

	vkExtensionNoCheck(vkGetPhysicalDeviceSurfaceFormatsKHR, instanceExt->getPhysicalDeviceSurfaceFormats);
	vkExtensionNoCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, instanceExt->getPhysicalDeviceSurfaceCapabilities);
	vkExtensionNoCheck(vkGetPhysicalDeviceSurfacePresentModesKHR, instanceExt->getPhysicalDeviceSurfacePresentModes);
	vkExtensionNoCheck(vkGetSwapchainImagesKHR, instanceExt->getSwapchainImagesKHR);

	vkExtensionNoCheck(vkCreateWin32SurfaceKHR, instanceExt->createWin32SurfaceKHR);

	if(supportsDebug[1]) {
		vkExtension(clean, vkSetDebugUtilsObjectNameEXT, instanceExt->debugSetName);
		vkExtension(clean, vkCmdDebugMarkerBeginEXT, instanceExt->debugMarkerBegin);
		vkExtension(clean, vkCmdDebugMarkerEndEXT, instanceExt->debugMarkerEnd);
		vkExtension(clean, vkCmdDebugMarkerInsertEXT, instanceExt->debugMarkerInsert);
	}

	if(supportsDebug[0]) {
		vkExtension(clean, vkCreateDebugReportCallbackEXT, instanceExt->debugCreateReportCallback);
		vkExtension(clean, vkDestroyDebugReportCallbackEXT, instanceExt->debugDestroyReportCallback);
	}

	if(supportsSurface) {
		vkExtension(clean, vkAcquireNextImageKHR, instanceExt->acquireNextImage);
		vkExtension(clean, vkCreateSwapchainKHR, instanceExt->createSwapchain);
		vkExtension(clean, vkDestroySurfaceKHR, instanceExt->destroySurface);
		vkExtension(clean, vkDestroySwapchainKHR, instanceExt->destroySwapchain);
	}

	vkExtensionNoCheck(vkCmdDrawIndexedIndirectCountKHR, instanceExt->drawIndexedIndirectCount);
	vkExtensionNoCheck(vkCmdDrawIndirectCountKHR, instanceExt->drawIndirectCount);

	vkExtensionNoCheck(vkBuildAccelerationStructuresKHR, instanceExt->buildAccelerationStructures);
	vkExtensionNoCheck(vkCmdCopyAccelerationStructureKHR, instanceExt->copyAccelerationStructure);
	vkExtensionNoCheck(vkDestroyAccelerationStructureKHR, instanceExt->destroyAccelerationStructure);
	vkExtensionNoCheck(vkGetAccelerationStructureBuildSizesKHR, instanceExt->getAccelerationStructureBuildSizes);
	vkExtensionNoCheck(vkGetDeviceAccelerationStructureCompatibilityKHR, instanceExt->getAccelerationStructureCompatibility);

	vkExtensionNoCheck(vkCmdTraceRaysKHR, instanceExt->traceRays);
	vkExtensionNoCheck(vkCmdTraceRaysIndirectKHR, instanceExt->traceRaysIndirect);
	vkExtensionNoCheck(vkCreateRayTracingPipelinesKHR, instanceExt->createRaytracingPipelines);

	//Add debug callback

	#ifndef NDEBUG
		
		VkDebugReportCallbackCreateInfoEXT callbackInfo = (VkDebugReportCallbackCreateInfoEXT) {
		
			.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,

			.flags = 
				VK_DEBUG_REPORT_ERROR_BIT_EXT | 
				VK_DEBUG_REPORT_WARNING_BIT_EXT | 
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,

			.pfnCallback = (PFN_vkDebugReportCallbackEXT) onDebugReport
		};

		_gotoIfError(clean, vkCheck(instanceExt->debugCreateReportCallback(
			instanceExt->instance, &callbackInfo, NULL, &instanceExt->debugReportCallback
		)));

	#endif

	*instance = (GraphicsInstance) {
		.application = info,
		.api = EGraphicsApi_Vulkan,
		.apiVersion = application.apiVersion
	};

	goto success;

clean:
	GraphicsInstanceRef_dec(instanceRef);

success:

	CharString_freex(&title);

	List_freex(&enabledLayers);
	List_freex(&enabledExtensions);
	
	List_freex(&extensions);
	List_freex(&layers);

	return err;
}

Bool GraphicsInstance_free(GraphicsInstance *inst, Allocator alloc) {

	alloc;

	if(!inst)
		return false;

	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(inst, Vk);

	if(instanceExt->debugDestroyReportCallback)
		instanceExt->debugDestroyReportCallback(instanceExt->instance, instanceExt->debugReportCallback, NULL);

	vkDestroyInstance(instanceExt->instance, NULL);

	return true;
}

const C8 *reqExtensionsName[] = {
	"VK_KHR_shader_draw_parameters",
	"VK_KHR_push_descriptor",
	"VK_KHR_dedicated_allocation",
	"VK_KHR_bind_memory2",
	"VK_KHR_get_memory_requirements2",
	"VK_EXT_shader_subgroup_ballot",
	"VK_EXT_shader_subgroup_vote",
	"VK_EXT_descriptor_indexing",
	"VK_KHR_driver_properties",
	"VK_KHR_synchronization2",
	"VK_KHR_timeline_semaphore"
};

U64 reqExtensionsNameCount = sizeof(reqExtensionsName) / sizeof(reqExtensionsName[0]);

const C8 *optExtensionsName[] = {

	#ifndef NDEBUG
		"VK_EXT_debug_marker",
	#else
		"",
	#endif

	"VK_KHR_shader_float16_int8",
	"VK_KHR_draw_indirect_count",
	"VK_KHR_shader_atomic_int64",
	"VK_KHR_performance_query",
	"VK_KHR_ray_tracing_pipeline",
	"VK_KHR_ray_query",
	"VK_KHR_acceleration_structure",
	"VK_KHR_swapchain",
	"VK_NV_ray_tracing_motion_blur",
	"VK_NV_ray_tracing_invocation_reorder",
	"VK_EXT_mesh_shader",
	"VK_KHR_dynamic_rendering",
	"VK_EXT_opacity_micromap",
	"VK_NV_displacement_micromap",
	"VK_KHR_fragment_shading_rate",
	"VK_EXT_shader_atomic_float",
	"VK_KHR_deferred_host_operations"
};

U64 optExtensionsNameCount = sizeof(optExtensionsName) / sizeof(optExtensionsName[0]);

#define getDeviceFeatures(StructName, Name, StructType)						\
																			\
StructName Name = (StructName) { .sType = StructType };						\
																			\
{																			\
	VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 	\
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,				\
		.pNext = &Name														\
	};																		\
																			\
	graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);				\
}
#define getDeviceProperties(StructName, Name, StructType)					\
																			\
StructName Name = (StructName) { .sType = StructType };						\
																			\
{																			\
	VkPhysicalDeviceProperties2 tempProp2 = (VkPhysicalDeviceProperties2) { \
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,			\
		.pNext = &Name														\
	};																		\
																			\
	graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp2);				\
}

Error GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, Bool isVerbose, List *result) {

	if(!inst || !result)
		return Error_nullPointer(!inst ? 0 : 2);

	if(result->ptr)
		return Error_invalidParameter(1, 0);

	VkGraphicsInstance *graphicsExt = GraphicsInstance_ext(inst, Vk);

	U32 deviceCount = 0;
	vkEnumeratePhysicalDevices(graphicsExt->instance, &deviceCount, NULL);

	List temp = (List) { 0 };
	List temp2 = List_createEmpty(sizeof(GraphicsDeviceInfo));
	List temp3 = List_createEmpty(sizeof(VkLayerProperties));
	List temp4 = List_createEmpty(sizeof(VkExtensionProperties));

	Error err = List_createx(deviceCount, sizeof(VkPhysicalDevice), &temp);

	if(err.genericError)
		return err;

	_gotoIfError(clean, List_reservex(&temp2, deviceCount));

	_gotoIfError(clean, vkCheck(vkEnumeratePhysicalDevices(
		graphicsExt->instance, &deviceCount, (VkPhysicalDevice*) temp.ptr
	)));

	for (U32 i = 0, j = 0; i < deviceCount; ++i) {

		VkPhysicalDevice dev = ((VkPhysicalDevice*)temp.ptr)[i];

		//Get extensions and layers

		U32 layerCount = 0, extensionCount = 0;

		_gotoIfError(clean, vkCheck(vkEnumerateDeviceLayerProperties(dev, &layerCount, NULL)));
		_gotoIfError(clean, List_resizex(&temp3, layerCount));

		_gotoIfError(clean, vkCheck(vkEnumerateDeviceLayerProperties(
			dev, &layerCount, (VkLayerProperties*) temp3.ptr
		)));

		_gotoIfError(clean, vkCheck(vkEnumerateDeviceExtensionProperties(dev, NULL, &extensionCount, NULL)));
		_gotoIfError(clean, List_resizex(&temp4, extensionCount));

		_gotoIfError(clean, vkCheck(
			vkEnumerateDeviceExtensionProperties(dev, NULL, &extensionCount, (VkExtensionProperties*)temp4.ptr)
		));

		//Log device for debugging

		if(isVerbose) {

			Log_debugLn("Supported device layers:");

			for(U32 k = 0; k < layerCount; ++k)
				Log_debugLn("\t%s", ((VkLayerProperties*)temp3.ptr)[k].layerName);

			Log_debugLn("Supported device extensions:");
		}

		//Check if all required extensions are present

		Bool reqExtensions[sizeof(reqExtensionsName) / sizeof(reqExtensionsName[0])] = { 0 };

		Bool optExtensions[sizeof(optExtensionsName) / sizeof(optExtensionsName[0])] = { 0 };

		for(U64 k = 0; k < extensionCount; ++k) {

			const C8 *name = ((VkExtensionProperties*)temp4.ptr)[k].extensionName;

			if(isVerbose)
				Log_debugLn("\t%s", name);

			//Check if required extension

			Bool found = false;

			for(U64 l = 0; l < sizeof(reqExtensions); ++l)
				if (CharString_equalsString(
					CharString_createConstRefCStr(reqExtensionsName[l]),
					CharString_createConstRefCStr(name),
					EStringCase_Sensitive
				)) {
					reqExtensions[l] = true;
					found = true;
					break;
				}

			//Check if optional extension

			if (!found)
				for(U64 l = 0; l < sizeof(optExtensions); ++l)
					if (CharString_equalsString(
						CharString_createConstRefCStr(optExtensionsName[l]),
						CharString_createConstRefCStr(name),
						EStringCase_Sensitive
					)) {
						optExtensions[l] = true;
						break;
					}
		}

		Bool supported = true;
		U64 firstUnavailable = 0;

		for(U64 k = 0; k < sizeof(reqExtensions); ++k)
			if(!reqExtensions[k] && strlen(reqExtensionsName[k])) {
				supported = false;
				firstUnavailable = k;
				break;
			}

		if (!supported) {

			Log_debugLn(
				"Vulkan: Unsupported GPU %u, one of the required extensions wasn't present (%s)", 
				i, reqExtensionsName[firstUnavailable]
			);

			continue;
		}

		//Grab limits and features

		VkPhysicalDeviceProperties2 properties2 = (VkPhysicalDeviceProperties2) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
		};

		VkPhysicalDeviceFeatures2 features2 = (VkPhysicalDeviceFeatures2) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
		};

		graphicsExt->getPhysicalDeviceProperties2(dev, &properties2);
		graphicsExt->getPhysicalDeviceFeatures2(dev, &features2);

		VkPhysicalDeviceProperties properties = properties2.properties;
		VkPhysicalDeviceFeatures features = features2.features;
		VkPhysicalDeviceLimits limits = properties.limits;

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
			!features.multiDrawIndirect
		) {
			Log_debugLn("Vulkan: Unsupported GPU %u, one of the required features wasn't enabled", i);
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
			limits.maxSamplerAllocationCount < 4000 ||
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
			limits.maxDescriptorSetStorageBuffersDynamic < 16 ||
			limits.maxDescriptorSetUniformBuffersDynamic < 15 ||
			limits.viewportBoundsRange[0] > -32768 ||
			limits.viewportBoundsRange[1] < 32767
		) {
			Log_debugLn("Vulkan: Unsupported GPU %u, one of the required limit wasn't sufficient", i);
			continue;
		}

		//Disable tesselation shaders if minimum isn't met

		if (
			features.tessellationShader && (
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
		)
			features.tessellationShader = false;

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
		}

		//Vendor

		EGraphicsVendor vendor = EGraphicsVendor_Unknown;

		switch (properties.vendorID) {
			case EVkDeviceVendor_NV:						vendor = EGraphicsVendor_NV;			break;
			case EVkDeviceVendor_AMD:						vendor = EGraphicsVendor_AMD;			break;
			case EVkDeviceVendor_ARM:						vendor = EGraphicsVendor_ARM;			break;
			case EVkDeviceVendor_QCOM:						vendor = EGraphicsVendor_QCOM;			break;
			case EVkDeviceVendor_INTC:						vendor = EGraphicsVendor_INTC;			break;
			case EVkDeviceVendor_IMGT:						vendor = EGraphicsVendor_IMGT;			break;
		}

		//Capabilities

		GraphicsDeviceCapabilities capabilities = (GraphicsDeviceCapabilities) { 0 };

		//Query features

		//Check if indexing is properly supported

		{
			getDeviceFeatures(
				VkPhysicalDeviceDescriptorIndexingFeatures, 
				current, 
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
			);

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
				.descriptorBindingUniformBufferUpdateAfterBind = true,
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

			//Make sure the padding is clear too.

			((U32*)&target)[1] = 0;
			((U32*)&current)[1] = 0;

			Bool eq = false;

			for(U32 q = 0; q < 4; ++q) {

				target.shaderInputAttachmentArrayDynamicIndexing = q & 1;
				target.shaderInputAttachmentArrayNonUniformIndexing = q >> 1;

				Buffer_eq(
					Buffer_createConstRef(&target, sizeof(target)), 
					Buffer_createConstRef(&current, sizeof(current)),
					&eq
				);

				if(eq)
					break;
			}

			if(!eq) {
				Log_debugLn("Vulkan: Unsupported GPU %u, descriptor indexing isn't properly supported.", i);
				continue;
			}
		}

		//TBDR

		if (
			(vendor != EGraphicsVendor_AMD && vendor != EGraphicsVendor_NV) ||
			(_PLATFORM_TYPE != EPlatform_Windows && _PLATFORM_TYPE != EPlatform_Linux) ||
			!optExtensions[EOptExtensions_DynamicRendering]
		)
			capabilities.features |= EGraphicsFeatures_TiledRendering;

		else {

			getDeviceFeatures(
				VkPhysicalDeviceDynamicRenderingFeatures, 
				dynamicRendering,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES
			);

			if(!dynamicRendering.dynamicRendering)
				capabilities.features |= EGraphicsFeatures_TiledRendering;
		}

		//Swapchain
		
		if(optExtensions[EOptExtensions_Swapchain])
			capabilities.features |= EGraphicsFeatures_Swapchain;

		//Shader types

		if(features.geometryShader)
			capabilities.features |= EGraphicsFeatures_GeometryShader;

		if(features.tessellationShader)
			capabilities.features |= EGraphicsFeatures_TessellationShader;

		//Multi draw

		if(optExtensions[EOptExtensions_MultiDrawIndirectCount] && limits.maxDrawIndirectCount >= GIBI)
			capabilities.features |= EGraphicsFeatures_MultiDrawIndirectCount;

		//Subgroup operations

		getDeviceProperties(
			VkPhysicalDeviceSubgroupProperties,
			subgroup,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES
		);

		VkSubgroupFeatureFlags requiredSubOp = 
			VK_SUBGROUP_FEATURE_BASIC_BIT | 
			VK_SUBGROUP_FEATURE_VOTE_BIT |
			VK_SUBGROUP_FEATURE_BALLOT_BIT;

		if((subgroup.supportedOperations & requiredSubOp) != requiredSubOp) {
			Log_debugLn("Vulkan: Unsupported GPU %u, one of the required subgroup operations wasn't supported!", i);
			continue;
		}

		if(subgroup.subgroupSize < 16 || subgroup.subgroupSize > 128) {
			Log_debugLn("Vulkan: Unsupported GPU %u, subgroup size is not in range 16-128!", i);
			continue;
		}

		if(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)
			capabilities.features |= EGraphicsFeatures_SubgroupArithmetic;

		if(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)
			capabilities.features |= EGraphicsFeatures_SubgroupShuffle;
		
		//Force enable synchronization and timeline semaphores

		{
			getDeviceFeatures(
				VkPhysicalDeviceTimelineSemaphoreFeatures,
				semaphore,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES
			);

			if (!semaphore.timelineSemaphore) {
				Log_debugLn("Vulkan: Unsupported GPU %u, Timeline semaphores unsupported!", i);
				continue;
			}
		}

		{
			getDeviceFeatures(
				VkPhysicalDeviceSynchronization2Features,
				sync2,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES
			);

			if (!sync2.synchronization2) {
				Log_debugLn("Vulkan: Unsupported GPU %u, Synchronization 2 unsupported!", i);
				continue;
			}
		}

		//Mesh shaders

		if(optExtensions[EOptExtensions_MeshShader]) {

			getDeviceFeatures(
				VkPhysicalDeviceMeshShaderFeaturesEXT,
				meshShader,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
			);

			if(
				!meshShader.taskShader || 
				!meshShader.primitiveFragmentShadingRateMeshShader || 
				!meshShader.multiviewMeshShader
			)
				meshShader.meshShader = false;

			if (meshShader.meshShader) {

				getDeviceProperties(
					VkPhysicalDeviceMeshShaderPropertiesEXT,
					meshShaderProp,
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
				);

				if(
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
				)
					meshShader.meshShader = false;
			}

			if(meshShader.meshShader)
				capabilities.features |= EGraphicsFeatures_MeshShader;
		}

		//Raytracing

		if(!optExtensions[EOptExtensions_DeferredHostOperations])
			optExtensions[EOptExtensions_RayAcceleration] = false;

		if(optExtensions[EOptExtensions_RayAcceleration]) {

			getDeviceFeatures(
				VkPhysicalDeviceAccelerationStructureFeaturesKHR,
				rtasFeat,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
			);

			//Disable raytracing if minimum limits aren't reached

			if(rtasFeat.accelerationStructure) {

				getDeviceProperties(
					VkPhysicalDeviceAccelerationStructurePropertiesKHR,
					rtasProp,
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
				);

				if(
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
					rtasProp.maxPrimitiveCount >= GIBI / 2
				)
					rtasFeat.accelerationStructure = false;
			}

			//Check if raytracing can be enabled

			if (rtasFeat.accelerationStructure) {

				//Query raytracing pipeline

				if(optExtensions[EOptExtensions_RayPipeline]) {

					getDeviceFeatures(
						VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
						rtpFeat,
						VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
					);

					if(!rtpFeat.rayTracingPipeline || !rtpFeat.rayTraversalPrimitiveCulling)
						optExtensions[EOptExtensions_RayPipeline] = false;

					else if(rtpFeat.rayTracingPipelineTraceRaysIndirect)
						capabilities.features |= EGraphicsFeatures_RayIndirect;
				}

				//Query ray query

				if(optExtensions[EOptExtensions_RayQuery]) {

					getDeviceFeatures(
						VkPhysicalDeviceRayQueryFeaturesKHR,
						rayQueryFeat,
						VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR
					);

					if(!rayQueryFeat.rayQuery)
						optExtensions[EOptExtensions_RayQuery] = false;
				}

				//Disable if invalid state

				if (optExtensions[EOptExtensions_RayPipeline]) {

					getDeviceProperties(
						VkPhysicalDeviceRayTracingPipelinePropertiesKHR,
						rtpProp,
						VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
					);

					if(
						rtpProp.maxRayDispatchInvocationCount < GIBI ||
						rtpProp.maxRayHitAttributeSize < 32 ||
						rtpProp.maxRayRecursionDepth < 1 ||
						rtpProp.maxShaderGroupStride < 4096
					) {
						optExtensions[EOptExtensions_RayQuery] = false;
						optExtensions[EOptExtensions_RayPipeline] = false;
						capabilities.features &=~ EGraphicsFeatures_RayIndirect;
					}
				}

				//Enable extension

				if(optExtensions[EOptExtensions_RayQuery])
					capabilities.features |= EGraphicsFeatures_RayQuery;

				if(optExtensions[EOptExtensions_RayPipeline])
					capabilities.features |= EGraphicsFeatures_RayPipeline;

				if(optExtensions[EOptExtensions_RayQuery] || optExtensions[EOptExtensions_RayPipeline]) {

					capabilities.features |= EGraphicsFeatures_Raytracing;

					if(optExtensions[EOptExtensions_RayMotionBlur]) {

						getDeviceFeatures(
							VkPhysicalDeviceRayTracingMotionBlurFeaturesNV,
							rayMotionBlurFeat,
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV
						);

						if(rayMotionBlurFeat.rayTracingMotionBlur)
							capabilities.features |= EGraphicsFeatures_RayMotionBlur;

						//Indirect must allow motion blur too

						if(
							!rayMotionBlurFeat.rayTracingMotionBlurPipelineTraceRaysIndirect &&
							capabilities.features & EGraphicsFeatures_RayIndirect
						)
							capabilities.features &= ~EGraphicsFeatures_RayIndirect;
					}

					if(optExtensions[EOptExtensions_RayReorder]) {

						getDeviceFeatures(
							VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV,
							rayReorderFeat,
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV
						);

						getDeviceProperties(
							VkPhysicalDeviceRayTracingInvocationReorderPropertiesNV,
							rayReorderProp,
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV
						);

						if(
							rayReorderFeat.rayTracingInvocationReorder && 
							rayReorderProp.rayTracingInvocationReorderReorderingHint
						)
							capabilities.features |= EGraphicsFeatures_RayReorder;
					}

					if(optExtensions[EOptExtensions_RayMicromapOpacity]) {

						getDeviceFeatures(
							VkPhysicalDeviceOpacityMicromapFeaturesEXT,
							rayOpacityMicroFeat,
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT
						);

						if(rayOpacityMicroFeat.micromap)
							capabilities.features |= EGraphicsFeatures_RayMicromapOpacity;
					}

					if(optExtensions[EOptExtensions_RayMicromapDisplacement]) {

						getDeviceFeatures(
							VkPhysicalDeviceDisplacementMicromapFeaturesNV,
							rayDispMicroFeat,
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV
						);

						if(rayDispMicroFeat.displacementMicromap)
							capabilities.features |= EGraphicsFeatures_RayMicromapDisplacement;
					}
				}
			}
		}

		//Variable rate shading

		if(optExtensions[EOptExtensions_VariableRateShading]) {
			//TODO:
			//https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_fragment_shading_rate.html
			//EGraphicsFeatures_VariableRateShading		= 1 << 1,
		}

		//Get data types

		if(optExtensions[EOptExtensions_F16]) {

			getDeviceFeatures(
				VkPhysicalDeviceShaderFloat16Int8Features,
				float16,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES
			);

			if(float16.shaderFloat16)
				capabilities.dataTypes |= EGraphicsDataTypes_F16;
		}

		if(features.shaderInt64)
			capabilities.dataTypes |= EGraphicsDataTypes_I64;

		if(features.shaderFloat64)
			capabilities.dataTypes |= EGraphicsDataTypes_F64;

		//Atomics

		if(optExtensions[EOptExtensions_AtomicI64]) {

			getDeviceFeatures(
				VkPhysicalDeviceShaderAtomicInt64Features,
				atomics64,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES
			);

			if(atomics64.shaderBufferInt64Atomics)
				capabilities.dataTypes |= EGraphicsDataTypes_AtomicI64;
		}

		if(optExtensions[EOptExtensions_AtomicF32]) {

			getDeviceFeatures(
				VkPhysicalDeviceShaderAtomicFloatFeaturesEXT,
				atomicsF32,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT
			);

			if(atomicsF32.shaderBufferFloat32AtomicAdd && atomicsF32.shaderBufferFloat32Atomics)
				capabilities.dataTypes |= EGraphicsDataTypes_AtomicF32;

			if(atomicsF32.shaderBufferFloat64AtomicAdd && atomicsF32.shaderBufferFloat64Atomics)
				capabilities.dataTypes |= EGraphicsDataTypes_AtomicF64;
		}

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

		if(allMSAA & VK_SAMPLE_COUNT_16_BIT)
			capabilities.dataTypes |= EGraphicsDataTypes_MSAA16x;

		//Get graphics binding tier

		if(
			limits.maxPerStageDescriptorSamplers < 2 * KIBI ||
			limits.maxPerStageDescriptorUniformBuffers < 250 * KILO ||
			limits.maxPerStageDescriptorStorageBuffers < 250 * KILO ||
			limits.maxPerStageDescriptorSampledImages < 250 * KILO ||
			limits.maxPerStageDescriptorStorageImages < 250 * KILO ||
			limits.maxPerStageResources < MEGA ||
			limits.maxDescriptorSetSamplers < 2 * KIBI ||
			limits.maxDescriptorSetUniformBuffers < 250 * KILO ||
			limits.maxDescriptorSetStorageBuffers < 250 * KILO ||
			limits.maxDescriptorSetSampledImages < 250 * KILO ||
			limits.maxDescriptorSetStorageImages < 250 * KILO
		) {
			Log_debugLn("Vulkan: Unsupported GPU %u, graphics binding tier not supported!", i);
			continue;
		}

		//Grab LUID/UUID

		getDeviceProperties(
			VkPhysicalDeviceIDProperties,
			id,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES
		);

		if(id.deviceLUIDValid)
			capabilities.features |= EGraphicsFeatures_LUID;

		getDeviceProperties(
			VkPhysicalDeviceDriverProperties,
			driver,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
		);

		//API dependent stuff for the runtime

		if(optExtensions[EOptExtensions_DebugMarker])
			capabilities.features |= EGraphicsFeatures_DebugMarkers;

		if(optExtensions[EOptExtensions_PerfQuery]) {

			getDeviceFeatures(
				VkPhysicalDevicePerformanceQueryFeaturesKHR,
				perfQuery,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR
			);

			if(perfQuery.performanceCounterQueryPools)
				capabilities.featuresExt |= EVkGraphicsFeatures_PerfQuery;
		}

		//Fully converted type

		_gotoIfError(clean, List_resize(&temp2, temp2.length + 1, (Allocator){ 0 }));

		GraphicsDeviceInfo *info = (GraphicsDeviceInfo*) temp2.ptr + j;

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
			Buffer_createConstRef(properties.deviceName, sizeof(properties.deviceName))
		);

		Buffer_copy(
			Buffer_createRef(info->driverName, sizeof(info->driverName)), 
			Buffer_createConstRef(driver.driverName, sizeof(driver.driverName))
		);

		Buffer_copy(
			Buffer_createRef(info->driverInfo, sizeof(info->driverInfo)), 
			Buffer_createConstRef(driver.driverInfo, sizeof(driver.driverInfo))
		);

		++j;
	}

	if(!temp2.length)
		_gotoIfError(clean, Error_unsupportedOperation(0));

	*result = temp2;
	goto success;

clean:
	List_freex(&temp2);
success:
	List_freex(&temp);
	List_freex(&temp3);
	List_freex(&temp4);
	return err;
}
