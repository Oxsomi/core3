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

	const C8 *prefix;
	ELogLevel level = ELogLevel_Debug;

	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		prefix = "Info: ";

	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
		prefix = "Warning: ";
		level = ELogLevel_Warn;
	}
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
		prefix = "Performance warning: ";
		level = ELogLevel_Performance;
	}
	else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		prefix = "Error: ";
		level = ELogLevel_Error;
	}

	else prefix = "Debug: ";

	Log_log(level, ELogOptions_Timestamp, CharString_createConstRefCStr(prefix));
	Log_log(level, ELogOptions_NewLine, CharString_createConstRefCStr(message));

	return VK_FALSE;
}

#define vkExtensionNoCheck(function, result) {												\
	*(void**)&result = (void*) vkGetInstanceProcAddr(vkInstance->instance, #function);		\
}

#define vkExtension(label, function, result) {													\
																								\
	PFN_vkVoidFunction v = vkGetInstanceProcAddr(vkInstance->instance, #function); 				\
																								\
	if(!v)																						\
		_gotoIfError(clean, Error_nullPointer(0));												\
																								\
	*(void**)&result = (void*) v;																\
}

Error GraphicsInstance_create(GraphicsApplicationInfo info, Bool isVerbose, GraphicsInstance *inst) {

	U32 layerCount = 0, extensionCount = 0;
	Error err = Error_none();
	List extensions = (List) { 0 }, layers = (List) { 0 };
	Buffer graphicsExt = (Buffer) { 0 };
	CharString title = (CharString) { 0 };

	List enabledExtensions = List_createEmpty(sizeof(const C8*));
	List enabledLayers = List_createEmpty(sizeof(const C8*));

	_gotoIfError(clean, vkCheck(vkEnumerateInstanceLayerProperties(&layerCount, NULL)));
	_gotoIfError(clean, vkCheck(vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL)));

	_gotoIfError(clean, CharString_createCopyx(info.name, &title));
	_gotoIfError(clean, List_createx(extensionCount, sizeof(VkExtensionProperties), &extensions));
	_gotoIfError(clean, List_createx(layerCount, sizeof(VkLayerProperties), &layers));

	_gotoIfError(clean, List_reservex(&enabledLayers, layerCount));
	_gotoIfError(clean, List_reservex(&enabledExtensions, extensionCount));

	_gotoIfError(clean, Buffer_createEmptyBytesx(sizeof(VkGraphicsInstance), &graphicsExt));

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

	VkGraphicsInstance *vkInstance = (VkGraphicsInstance*) graphicsExt.ptr;

	_gotoIfError(clean, vkCheck(vkCreateInstance(&instanceInfo, NULL, &vkInstance->instance)));
	
	//Load functions

	vkExtension(clean, vkGetImageMemoryRequirements2KHR, vkInstance->getImageMemoryRequirements2);
	vkExtension(clean, vkGetBufferMemoryRequirements2KHR, vkInstance->getBufferMemoryRequirements2);

	vkExtension(clean, vkGetPhysicalDeviceFeatures2KHR, vkInstance->getPhysicalDeviceFeatures2);
	vkExtension(clean, vkGetPhysicalDeviceProperties2KHR, vkInstance->getPhysicalDeviceProperties2);

	if(supportsDebug[1]) {
		vkExtension(clean, vkSetDebugUtilsObjectNameEXT, vkInstance->debugSetName);
		vkExtension(clean, vkCmdDebugMarkerBeginEXT, vkInstance->debugMarkerBegin);
		vkExtension(clean, vkCmdDebugMarkerEndEXT, vkInstance->debugMarkerEnd);
		vkExtension(clean, vkCmdDebugMarkerInsertEXT, vkInstance->debugMarkerInsert);
	}

	if(supportsDebug[0]) {
		vkExtension(clean, vkCreateDebugReportCallbackEXT, vkInstance->debugCreateReportCallback);
		vkExtension(clean, vkDestroyDebugReportCallbackEXT, vkInstance->debugDestroyReportCallback);
	}

	if(supportsSurface) {
		vkExtension(clean, vkAcquireNextImageKHR, vkInstance->acquireNextImage);
		vkExtension(clean, vkCreateSwapchainKHR, vkInstance->createSwapchain);
		vkExtension(clean, vkDestroySurfaceKHR, vkInstance->destroySurface);
		vkExtension(clean, vkDestroySwapchainKHR, vkInstance->destroySwapchain);
	}

	vkExtensionNoCheck(vkCmdDrawIndexedIndirectCountKHR, vkInstance->drawIndexedIndirectCount);
	vkExtensionNoCheck(vkCmdDrawIndirectCountKHR, vkInstance->drawIndirectCount);

	vkExtensionNoCheck(vkBuildAccelerationStructuresKHR, vkInstance->buildAccelerationStructures);
	vkExtensionNoCheck(vkCmdCopyAccelerationStructureKHR, vkInstance->copyAccelerationStructure);
	vkExtensionNoCheck(vkDestroyAccelerationStructureKHR, vkInstance->destroyAccelerationStructure);
	vkExtensionNoCheck(vkGetAccelerationStructureBuildSizesKHR, vkInstance->getAccelerationStructureBuildSizes);
	vkExtensionNoCheck(vkGetDeviceAccelerationStructureCompatibilityKHR, vkInstance->getAccelerationStructureCompatibility);

	vkExtensionNoCheck(vkCmdTraceRaysKHR, vkInstance->traceRays);
	vkExtensionNoCheck(vkCmdTraceRaysIndirectKHR, vkInstance->traceRaysIndirect);
	vkExtensionNoCheck(vkCreateRayTracingPipelinesKHR, vkInstance->createRaytracingPipelines);

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

		_gotoIfError(clean, vkCheck(vkInstance->debugCreateReportCallback(
			vkInstance->instance, &callbackInfo, NULL, &vkInstance->debugReportCallback
		)));

	#endif

	*inst = (GraphicsInstance) {
		.application = info,
		.api = EGraphicsApi_Vulkan,
		.apiVersion = application.apiVersion,
		.ext = vkInstance
	};

	graphicsExt = (Buffer) { 0 };		//It's transferred to inst

clean:

	CharString_freex(&title);
	Buffer_freex(&graphicsExt);

	List_freex(&enabledLayers);
	List_freex(&enabledExtensions);
	
	List_freex(&extensions);
	List_freex(&layers);

	return err;
}

Bool GraphicsInstance_free(GraphicsInstance *inst) {

	if(!inst || !inst->ext)
		return false;

	VkGraphicsInstance *vkInstance = (VkGraphicsInstance*) inst->ext;

	if(vkInstance->debugDestroyReportCallback)
		vkInstance->debugDestroyReportCallback(vkInstance->instance, vkInstance->debugReportCallback, NULL);

	vkDestroyInstance(vkInstance->instance, NULL);

	Buffer graphicsExt = Buffer_createManagedPtr(vkInstance, sizeof(*vkInstance));
	Buffer_freex(&graphicsExt);

	*inst = (GraphicsInstance) { 0 };

	return true;
}

Error GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, Bool isVerbose, List *result) {

	if(!inst || !inst->ext || !result)
		return Error_nullPointer(!inst ? 0 : 2);

	if(result->ptr)
		return Error_invalidParameter(1, 0);

	VkGraphicsInstance *graphicsExt = (VkGraphicsInstance*) inst->ext;

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

		const C8 *reqExtensionsName[] = {
			"VK_KHR_shader_draw_parameters",
			"VK_KHR_push_descriptor",
			"VK_KHR_dedicated_allocation",
			"VK_KHR_bind_memory2",
			"VK_KHR_get_memory_requirements2",
			"VK_EXT_shader_subgroup_ballot",
			"VK_EXT_shader_subgroup_vote",
			"VK_EXT_descriptor_indexing",
			"VK_EXT_multi_draw",
			"VK_KHR_driver_properties"
		};

		Bool reqExtensions[sizeof(reqExtensionsName) / sizeof(reqExtensionsName[0])] = { 0 };

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
			EOptExtensions_AtomicF32

		} EOptExtensions;

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
			"VK_EXT_shader_atomic_float"
		};

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
			limits.maxDescriptorSetUniformBuffersDynamic < 15
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

		//TBDR

		if (
			(vendor != EGraphicsVendor_AMD && vendor != EGraphicsVendor_NV) ||
			(_PLATFORM_TYPE != EPlatform_Windows && _PLATFORM_TYPE != EPlatform_Linux) ||
			!optExtensions[EOptExtensions_DynamicRendering]
		)
			capabilities.features |= EGraphicsFeatures_TiledRendering;

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

		VkPhysicalDeviceSubgroupProperties subgroup = (VkPhysicalDeviceSubgroupProperties) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES 
		};

		{
			VkPhysicalDeviceProperties2 tempProp2 = (VkPhysicalDeviceProperties2) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
				.pNext = &subgroup
			};

			graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp2);
		}

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

		//Mesh shaders

		if(optExtensions[EOptExtensions_MeshShader]) {

			VkPhysicalDeviceMeshShaderFeaturesEXT meshShader = (VkPhysicalDeviceMeshShaderFeaturesEXT) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT 
			};

			{
				VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 
					.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
					.pNext = &meshShader
				};

				graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);
			}

			if(
				!meshShader.taskShader || 
				!meshShader.primitiveFragmentShadingRateMeshShader || 
				!meshShader.multiviewMeshShader
			)
				meshShader.meshShader = false;

			if (meshShader.meshShader) {

				VkPhysicalDeviceMeshShaderPropertiesEXT meshShaderProp = (VkPhysicalDeviceMeshShaderPropertiesEXT) { 
					.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT 
				};

				{
					VkPhysicalDeviceProperties2 tempProp2 = (VkPhysicalDeviceProperties2) { 
						.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
						.pNext = &meshShaderProp
					};

					graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp2);
				}

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

		if(optExtensions[EOptExtensions_RayAcceleration]) {

			VkPhysicalDeviceAccelerationStructureFeaturesKHR rtasFeat = (VkPhysicalDeviceAccelerationStructureFeaturesKHR) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR 
			};

			{
				VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 
					.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
					.pNext = &rtasFeat
				};

				graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);
			}

			//Disable raytracing if minimum limits aren't reached

			if(rtasFeat.accelerationStructure) {

				VkPhysicalDeviceAccelerationStructurePropertiesKHR rtasProp = { 
					.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR 
				};

				{
					VkPhysicalDeviceProperties2 tempProp2 = (VkPhysicalDeviceProperties2) { 
						.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
						.pNext = &rtasProp
					};

					graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp2);
				}

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

				VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpFeat = (VkPhysicalDeviceRayTracingPipelineFeaturesKHR) { 
					.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR 
				};

				if(optExtensions[EOptExtensions_RayPipeline]) {

					VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 
						.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
						.pNext = &rtpFeat
					};

					graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);
				}

				//Query ray query

				VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeat = (VkPhysicalDeviceRayQueryFeaturesKHR) { 
					.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR 
				};

				if(optExtensions[EOptExtensions_RayQuery]) {

					VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 
						.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
						.pNext = &rayQueryFeat
					};

					graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);
				}

				//Disable if invalid state

				if (rtpFeat.rayTracingPipeline) {

					VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtpProp = { 
						.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR 
					};

					{
						VkPhysicalDeviceProperties2 tempProp2 = (VkPhysicalDeviceProperties2) { 
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
							.pNext = &rtpProp
						};

						graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp2);
					}

					if(
						!rtpFeat.rayTraversalPrimitiveCulling ||
						rtpProp.maxRayDispatchInvocationCount < GIBI ||
						rtpProp.maxRayHitAttributeSize < 32 ||
						rtpProp.maxRayRecursionDepth < 1 ||
						rtpProp.maxShaderGroupStride < 4096
					) {
						rayQueryFeat.rayQuery = false;
						rtpFeat.rayTracingPipeline = false;
					}
				}

				//Enable extension

				if(rayQueryFeat.rayQuery)
					capabilities.features |= EGraphicsFeatures_RayQuery;

				if(rtpFeat.rayTracingPipeline) {

					capabilities.features |= EGraphicsFeatures_RayPipeline;

					if(rtpFeat.rayTracingPipelineTraceRaysIndirect)
						capabilities.features |= EGraphicsFeatures_RayIndirect;
				}

				if(rayQueryFeat.rayQuery || rtpFeat.rayTracingPipeline) {

					capabilities.features |= EGraphicsFeatures_Raytracing;

					if(optExtensions[EOptExtensions_RayMotionBlur]) {

						VkPhysicalDeviceRayTracingMotionBlurFeaturesNV rayMotionBlurFeat = { 
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV 
						};

						{
							VkPhysicalDeviceFeatures2 tempFeat3 = (VkPhysicalDeviceFeatures2) { 
								.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
								.pNext = &rayMotionBlurFeat
							};

							graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat3);
						}

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

						VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV rayReorderFeat = { 
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV 
						};

						{
							VkPhysicalDeviceFeatures2 tempFeat3 = (VkPhysicalDeviceFeatures2) { 
								.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
								.pNext = &rayReorderFeat
							};

							graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat3);
						}

						VkPhysicalDeviceRayTracingInvocationReorderPropertiesNV rayReorderProp = { 
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV 
						};

						{
							VkPhysicalDeviceProperties2 tempProp3 = (VkPhysicalDeviceProperties2) { 
								.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
								.pNext = &rayReorderProp
							};

							graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp3);
						}

						if(
							rayReorderFeat.rayTracingInvocationReorder && 
							rayReorderProp.rayTracingInvocationReorderReorderingHint
						)
							capabilities.features |= EGraphicsFeatures_RayReorder;
					}

					if(optExtensions[EOptExtensions_RayMicromapOpacity]) {

						VkPhysicalDeviceOpacityMicromapFeaturesEXT  rayOpacityMicroFeat = { 
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT 
						};

						{
							VkPhysicalDeviceFeatures2 tempFeat3 = (VkPhysicalDeviceFeatures2) { 
								.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
								.pNext = &rayOpacityMicroFeat
							};

							graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat3);
						}

						if(rayOpacityMicroFeat.micromap)
							capabilities.features |= EGraphicsFeatures_RayMicromapOpacity;
					}

					if(optExtensions[EOptExtensions_RayMicromapDisplacement]) {

						VkPhysicalDeviceDisplacementMicromapFeaturesNV rayDispMicroFeat = { 
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV 
						};

						{
							VkPhysicalDeviceFeatures2 tempFeat3 = (VkPhysicalDeviceFeatures2) { 
								.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
								.pNext = &rayDispMicroFeat
							};

							graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat3);
						}

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

		VkPhysicalDeviceShaderFloat16Int8Features float16 = (VkPhysicalDeviceShaderFloat16Int8Features ) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES 
		};

		if(optExtensions[EOptExtensions_F16]) {

			VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &float16
			};

			graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);
		}

		if(float16.shaderFloat16)
			capabilities.dataTypes |= EGraphicsDataTypes_F16;

		if(features.shaderInt64)
			capabilities.dataTypes |= EGraphicsDataTypes_I64;

		if(features.shaderFloat64)
			capabilities.dataTypes |= EGraphicsDataTypes_F64;

		//Atomics

		VkPhysicalDeviceShaderAtomicInt64Features atomics64 = (VkPhysicalDeviceShaderAtomicInt64Features) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES 
		};

		if(optExtensions[EOptExtensions_AtomicI64]) {

			VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &atomics64
			};

			graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);
		}

		if(atomics64.shaderBufferInt64Atomics)
			capabilities.dataTypes |= EGraphicsDataTypes_AtomicI64;

		VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicsF32 = (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT 
		};

		if(optExtensions[EOptExtensions_AtomicF32]) {

			VkPhysicalDeviceFeatures2 tempFeat2 = (VkPhysicalDeviceFeatures2) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &atomicsF32
			};

			graphicsExt->getPhysicalDeviceFeatures2(dev, &tempFeat2);
		}

		if(atomicsF32.shaderBufferFloat32AtomicAdd && atomicsF32.shaderBufferFloat32Atomics)
			capabilities.dataTypes |= EGraphicsDataTypes_AtomicF32;

		if(atomicsF32.shaderBufferFloat64AtomicAdd && atomicsF32.shaderBufferFloat64Atomics)
			capabilities.dataTypes |= EGraphicsDataTypes_AtomicF64;

		//Compression

		if(features.textureCompressionASTC_LDR)
			capabilities.dataTypes |= EGraphicsDataTypes_ASTC;

		if(features.textureCompressionBC)
			capabilities.dataTypes |= EGraphicsDataTypes_BCn;

		//TODO:
		//EGraphicsDataTypes_HDR10A2
		//EGraphicsDataTypes_RGBA16f

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

		VkPhysicalDeviceIDProperties id = (VkPhysicalDeviceIDProperties) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES 
		};

		{
			VkPhysicalDeviceProperties2 tempProp2 = (VkPhysicalDeviceProperties2) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
				.pNext = &id
			};

			graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp2);
		}

		if(id.deviceLUIDValid)
			capabilities.features |= EGraphicsFeatures_SupportsLUID;


		VkPhysicalDeviceDriverProperties driver = (VkPhysicalDeviceDriverProperties ) { 
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES 
		};

		{
			VkPhysicalDeviceProperties2 tempProp2 = (VkPhysicalDeviceProperties2) { 
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
				.pNext = &driver
			};

			graphicsExt->getPhysicalDeviceProperties2(dev, &tempProp2);
		}

		//API dependent stuff for the runtime

		if(optExtensions[EOptExtensions_DebugMarker])
			capabilities.featuresExt |= EVkGraphicsFeatures_DebugMarker;

		if(optExtensions[EOptExtensions_PerfQuery])
			capabilities.featuresExt |= EVkGraphicsFeatures_PerfQuery;

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
