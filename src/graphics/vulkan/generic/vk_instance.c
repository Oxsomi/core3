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

//graphics/vulkan/generic/vk_instance.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/dynamic_library.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

//The Vulkan loader (vkGetInstanceProcAddr / vkGetDeviceProcAddr) is loaded at runtime rather than statically linked,
// so the build doesn't depend on an arch-specific vulkan-1.lib (which fails to link on e.g. Windows arm64).
//Every other Vulkan entry point is then resolved through the loaded getInstanceProcAddr/getDeviceProcAddr.
//The actual loading uses the platform-abstracted DynamicLibrary API; only the library name is platform-specific.

static const C8 *VkGraphicsInstance_loaderName() {
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		return "vulkan-1.dll";
	#elif _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
		return "libvulkan.dylib";
	#elif _PLATFORM_TYPE == PLATFORM_ANDROID
		return "libvulkan.so";
	#else
		return "libvulkan.so.1";
	#endif
}

GraphicsObjectSizes VkGraphicsObjectSizes = {
	.blas = GraphicsObjectSize_create(VkBLAS),
	.tlas = GraphicsObjectSize_create(VkTLAS),
	.opacityMicromap = GraphicsObjectSize_create(VkOpacityMicromap),
	.pipeline = GraphicsObjectSize_createPadded(VkPipeline, 8),             //Align to 16
	.sampler = GraphicsObjectSize_createPadded(VkSampler, 8),               //Align to 16
	.buffer = GraphicsObjectSize_create(VkDeviceBuffer),
	.image = GraphicsObjectSize_create(VkUnifiedTexture),
	.swapchain = GraphicsObjectSize_create(VkSwapchain),
	.device = GraphicsObjectSize_create(VkGraphicsDevice),
	.instance = GraphicsObjectSize_create(VkGraphicsInstance),
	.descriptorLayout = GraphicsObjectSize_create(VkDescriptorLayout),
	.descriptorTable = GraphicsObjectSize_create(VkDescriptorTable),
	.descriptorHeap = GraphicsObjectSize_create(VkDescriptorHeap),
	.pipelineLayout = GraphicsObjectSize_createPadded(VkPipelineLayout, 8)  //Align to 16
};

//The packing above gives the size 24 bits and the alignment a byte, which is far more than any of these need;
//this is here so that stops being an assumption.

static_assert(
	sizeof(VkGraphicsDevice) < (1 << 24) && sizeof(VkGraphicsInstance) < (1 << 24) &&
	sizeof(VkSwapchain) < (1 << 24) && alignof(VkUnifiedTexture) <= 255 && alignof(VkGraphicsDevice) <= 255,
	"A Vulkan extension struct outgrew the size or alignment GraphicsObjectSize can pack"
);

//TextureRef_getImplExt hands back a void*, so it rounds to a cache line rather than to the alignment of a
//struct it can't name. That covers everything today; this is here so it stops being an assumption.

static_assert(
	alignof(VkSwapchain) <= 64 && alignof(VkUnifiedTexture) <= 64,
	"A Vulkan texture extension struct needs more than the cache line TextureRef_getImplExt rounds to"
);

#ifndef GRAPHICS_API_DYNAMIC
	const GraphicsObjectSizes *GraphicsInterface_getObjectSizes(EGraphicsApi api) {
		(void) api;
		return &VkGraphicsObjectSizes;
	}
#else
	EXPORT_SYMBOL GraphicsInterfaceTable GraphicsInterface_getTable(Platform *instance, GraphicsInterface *interf) {

		Platform_instance = instance;
		GraphicsInterface_instance = interf;

		return (GraphicsInterfaceTable) {

			.api = EGraphicsApi_Vulkan,
			.objectSizes = VkGraphicsObjectSizes,

			.blasInit = VkBLAS_init,
			.blasFlush = VkBLASRef_flush,
			.blasFree = VkBLAS_free,

			.opacityMicromapInit = VkOpacityMicromap_init,
			.opacityMicromapFlush = VkOpacityMicromapRef_flush,
			.opacityMicromapFree = VkOpacityMicromap_free,

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
			.bufferPull = VkDeviceBufferRef_pull,
			.bufferFree = VkDeviceBuffer_free,

			.textureCreate = VkUnifiedTexture_create,
			.textureFlush = VkDeviceTextureRef_flush,
			.texturePull = VkDeviceTextureRef_pull,
			.textureFree = VkUnifiedTexture_free,

			.swapchainCreate = VkGraphicsDeviceRef_createSwapchain,
			.swapchainFree = VkSwapchain_free,

			.descriptorLayoutCreate = VkGraphicsDeviceRef_createDescriptorLayout,
			.descriptorLayoutFree = VkDescriptorLayout_free,
			.pipelineLayoutCreate = VkGraphicsDeviceRef_createPipelineLayout,
			.pipelineLayoutFree = VkPipelineLayout_free,
			.descriptorHeapCreate = VkGraphicsDeviceRef_createDescriptorHeap,
			.descriptorHeapFree = VkDescriptorHeap_free,

			.descriptorTableCreate = VkDescriptorHeap_createDescriptorTable,
			.descriptorTableFree = VkDescriptorTable_free,
			.descriptorTableSet = VkDescriptorTable_setDescriptors,
			.descriptorTableUnset = VkDescriptorTable_unsetDescriptors,

			.memoryAllocate = VkDeviceMemoryAllocator_allocate,
			.memoryFree = VkDeviceMemoryAllocator_freeAllocation,

			.deviceInit = VkGraphicsDevice_init,
			.deviceWait = VkGraphicsDeviceRef_wait,
			.deviceFree = VkGraphicsDevice_free,
			.deviceSubmitCommands = VkGraphicsDevice_submitCommands,
			.deviceGetMemoryBudget = VkGraphicsDevice_getMemoryBudget,

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
	(void)layerPrefix;

	GraphicsInstance *instance = (GraphicsInstance*) userData;

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {

		if(instance)
			AtomicI64_inc(&instance->validationErrors);

		Log_errorLnx("Error: %s", message);
	}

	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {

		//GPU assisted validation force enables the device features it needs and warns about doing so.
		//That's an expected side effect of running maximum validation, not an app defect, so it's only logged.
		//The entrypoint warning is GPU-AV admitting its own push constant lookup can't attribute variables in
		// multi entrypoint SPIR-V below 1.4; legal modules, no validation lost, and the layer's condition is
		// inverted anyway (it fires when there are no push constants at all), so it's noise rather than a defect.

		const CharString messageStr = CharString_createRefCStrConst(message);
		const CharString adjusting = CharString_createRefCStrConst("validation is adjusting settings");
		const CharString multiEntry = CharString_createRefCStrConst("can't determine which entrypoint");

		if(
			instance &&
			!CharString_containsStringInsensitive(&messageStr, &adjusting, 0, 0) &&
			!CharString_containsStringInsensitive(&messageStr, &multiEntry, 0, 0)
		)
			AtomicI64_inc(&instance->validationWarnings);

		Log_warnLnx("Warning: %s", message);
	}

	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		Log_performanceLnx("Performance warning: %s", message);

	else Log_debugLnx("Debug: %s", message);

	return VK_FALSE;
}

#define getVkFunction(label, function, result) {                                                \
																								\
	PFN_vkVoidFunction v = instanceExt->getInstanceProcAddr(instanceExt->instance, #function);  \
																								\
	if(!v)                                                                                      \
		retError(clean, Error_nullPointer(0, "getVkFunction() " #function " failed"));          \
																								\
	*(void**)&result = (void*) v;                                                               \
} (void) 0

TList(VkExtensionProperties);
TList(VkLayerProperties);
TListImpl(VkExtensionProperties);
TListImpl(VkLayerProperties);

//#define _GRAPHICS_VERBOSE_DEBUGGING

Bool VK_WRAP_FUNC(GraphicsInstance_create)(
	const GraphicsApplicationInfo *info, GraphicsInstanceRef **instanceRef, Error *e_rr
) {

	const Allocator *alloc = GraphicsInstanceRef_ptr(*instanceRef)->alloc;

	Bool s_uccess = true;

	U32 layerCount = 0, extensionCount = 0;
	CharString title = (CharString) { 0 };

	ListVkExtensionProperties extensions = (ListVkExtensionProperties) { 0 };
	ListVkLayerProperties layers = (ListVkLayerProperties) { 0 };

	ListConstC8 enabledExtensions = (ListConstC8) { 0 }, enabledLayers = (ListConstC8) { 0 };

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	//Start with loading the functions

	//Load the Vulkan loader dynamically and grab vkGetInstanceProcAddr from it (system search, not the app dir).

	gotoIfError3(clean, DynamicLibrary_loadSystem(
		CharString_createRefCStrConst(VkGraphicsInstance_loaderName()), &instanceExt->vulkanLib, e_rr
	));

	gotoIfError3(clean, DynamicLibrary_loadSymbol(
		instanceExt->vulkanLib, CharString_createRefCStrConst("vkGetInstanceProcAddr"),
		(void**) &instanceExt->getInstanceProcAddr, e_rr
	));

	//These instances are okay with using NULL as the instance

	getVkFunction(clean, vkCreateInstance, instanceExt->createInstance);

	getVkFunction(clean, vkEnumerateInstanceLayerProperties, instanceExt->enumerateInstanceLayerProperties);
	getVkFunction(clean, vkEnumerateInstanceExtensionProperties, instanceExt->enumerateInstanceExtensionProperties);

	//Enumerate instance info

	gotoIfError3(clean, checkVkError(instanceExt->enumerateInstanceLayerProperties(&layerCount, NULL), e_rr));
	gotoIfError3(clean, checkVkError(instanceExt->enumerateInstanceExtensionProperties(NULL, &extensionCount, NULL), e_rr));

	gotoIfError3(clean, CharString_createCopy(info->name, alloc, &title, e_rr));
	gotoIfError3(clean, ListVkExtensionProperties_create(extensionCount, alloc, &extensions, e_rr));
	gotoIfError3(clean, ListVkLayerProperties_create(layerCount, alloc, &layers, e_rr));

	gotoIfError3(clean, ListConstC8_reserve(&enabledLayers, layerCount, alloc, e_rr));
	gotoIfError3(clean, ListConstC8_reserve(&enabledExtensions, extensionCount, alloc, e_rr));

	gotoIfError3(clean, checkVkError(instanceExt->enumerateInstanceLayerProperties(&layerCount, layers.ptrNonConst), e_rr));
	gotoIfError3(clean, checkVkError(
		instanceExt->enumerateInstanceExtensionProperties(NULL, &extensionCount, extensions.ptrNonConst), e_rr
	));

	Bool supportsDebug[2] = { 0 };

	if(instance->flags & EGraphicsInstanceFlags_IsVerbose)
		Log_debugLnx("Supported extensions:");

	Bool supportsColorSpace = false;

	const CharString swapchainColorspace = CharString_createRefCStrConst("VK_EXT_swapchain_colorspace");
	const CharString debugReport = CharString_createRefCStrConst("VK_EXT_debug_report");
	const CharString debugUtils = CharString_createRefCStrConst("VK_EXT_debug_utils");

	for(U64 i = 0; i < extensionCount; ++i) {

		const C8 *name = extensions.ptr[i].extensionName;
		CharString nameStr = CharString_createRefCStrConst(name);

		if(CharString_equalsStringSensitive(&nameStr, &swapchainColorspace))
			supportsColorSpace = true;

		else if(instance->flags & EGraphicsInstanceFlags_IsDebug) {

			if(CharString_equalsStringSensitive(&nameStr, &debugReport))
				supportsDebug[0] = true;

			else if(CharString_equalsStringSensitive(&nameStr, &debugUtils))
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
		gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, debugReport.ptr, alloc, e_rr));

	if(supportsDebug[1])
		gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, debugUtils.ptr, alloc, e_rr));

	//Force physical device properties and external memory

	gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, "VK_KHR_get_physical_device_properties2", alloc, e_rr));
	gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, "VK_KHR_external_memory_capabilities", alloc, e_rr));

	//MoltenVK requires us to enable portability enumeration

	Bool isMoltenVk = false;

	#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
		gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, "VK_KHR_portability_enumeration", alloc, e_rr));
		isMoltenVk = true;
	#endif

	//Enable so we can use swapchain khr

	gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, "VK_KHR_surface", alloc, e_rr));
	gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, _VK_SURFACE_EXT, alloc, e_rr));

	if (supportsColorSpace)
		gotoIfError3(clean, ListConstC8_pushBack(&enabledExtensions, swapchainColorspace.ptr, alloc, e_rr));

	U8 hasDebugLayer = 0;

	if(instance->flags & EGraphicsInstanceFlags_IsDebug)
		for(U64 i = 0; i < layerCount; ++i) {

			CharString name = CharString_createRefCStrConst(layers.ptr[i].layerName);
			U64 len = CharString_length(name);

			switch(len ? name.ptr[len - 1] : ' ') {

				case 'n':    //validatioN

					if(CharString_equalsCStringSensitive(&name, "VK_LAYER_KHRONOS_validation"))
						hasDebugLayer |= 1;

					break;

				#ifdef _GRAPHICS_VERBOSE_DEBUGGING

					case 'p':    //dumP

						if(CharString_equalsCStringSensitive(&name, "VK_LAYER_LUNARG_api_dump"))
							hasDebugLayer |= 2;

						break;

				#endif

				default:
					break;
			}

			if(hasDebugLayer & 3)
				break;
		}

	if (!hasDebugLayer && (instance->flags & EGraphicsInstanceFlags_IsDebug))
		Log_warnLnx("VkGraphicsInstance_create tried to enable debug layer, but no debug layer was present");

	if(hasDebugLayer & 1)
		gotoIfError3(clean, ListConstC8_pushBack(&enabledLayers, "VK_LAYER_KHRONOS_validation", alloc, e_rr));

	if(hasDebugLayer & 2)
		gotoIfError3(clean, ListConstC8_pushBack(&enabledLayers, "VK_LAYER_LUNARG_api_dump", alloc, e_rr));

	VkApplicationInfo application = (VkApplicationInfo) {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = title.ptr,
		.applicationVersion = info->version,
		.pEngineName = "OxC3",
		.engineVersion = VK_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH),
		.apiVersion = VK_MAKE_VERSION(1, 1, 0)
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

	if(hasDebugLayer & 1) {

		//Maximum validation

		VkValidationFeatureEnableEXT enables[] = {
			VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
			VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
			VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
			VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT
		};

		U32 count = instance->flags & EGraphicsInstanceFlags_DisableGPUBV ? 2 : 4;

		VkValidationFeaturesEXT features = (VkValidationFeaturesEXT) {
			.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
			.enabledValidationFeatureCount = count,
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

	gotoIfError3(clean, checkVkError(instanceExt->createInstance(&instanceInfo, NULL, &instanceExt->instance), e_rr));

	//Functions that aren't device dependent, but do need an instance

	getVkFunction(clean, vkDestroyInstance, instanceExt->destroyInstance);
	getVkFunction(clean, vkCreateDevice, instanceExt->createDevice);
	getVkFunction(clean, vkDestroyDevice, instanceExt->destroyDevice);

	//Device-level entry points are resolved via vkGetDeviceProcAddr (also fetched through the loader).
	getVkFunction(clean, vkGetDeviceProcAddr, instanceExt->getDeviceProcAddr);

	if (supportsDebug[0]) {
		getVkFunction(clean, vkCreateDebugReportCallbackEXT, instanceExt->debugCreateReportCallback);
		getVkFunction(clean, vkDestroyDebugReportCallbackEXT, instanceExt->debugDestroyReportCallback);
	}

	if (supportsDebug[1]) {
		getVkFunction(clean, vkSetDebugUtilsObjectNameEXT, instanceExt->debugSetName);
		getVkFunction(clean, vkCmdBeginDebugUtilsLabelEXT, instanceExt->cmdDebugMarkerBegin);
		getVkFunction(clean, vkCmdEndDebugUtilsLabelEXT, instanceExt->cmdDebugMarkerEnd);
		getVkFunction(clean, vkCmdInsertDebugUtilsLabelEXT, instanceExt->cmdDebugMarkerInsert);
	}

	getVkFunction(clean, vkEnumeratePhysicalDevices, instanceExt->enumeratePhysicalDevices);
	getVkFunction(clean, vkEnumerateDeviceLayerProperties, instanceExt->enumerateDeviceLayerProperties);
	getVkFunction(clean, vkEnumerateDeviceExtensionProperties, instanceExt->enumerateDeviceExtensionProperties);
	getVkFunction(clean, vkGetPhysicalDeviceFormatProperties, instanceExt->getPhysicalDeviceFormatProperties);
	getVkFunction(clean, vkGetPhysicalDeviceFeatures2KHR, instanceExt->getPhysicalDeviceFeatures2);
	getVkFunction(clean, vkGetPhysicalDeviceProperties2KHR, instanceExt->getPhysicalDeviceProperties2);
	getVkFunction(clean, vkGetPhysicalDeviceMemoryProperties, instanceExt->getPhysicalDeviceMemoryProperties);
	getVkFunction(clean, vkGetPhysicalDeviceQueueFamilyProperties, instanceExt->getPhysicalDeviceQueueFamilyProperties);

	getVkFunction(clean, vkGetPhysicalDeviceSurfaceFormatsKHR, instanceExt->getPhysicalDeviceSurfaceFormats);
	getVkFunction(clean, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, instanceExt->getPhysicalDeviceSurfaceCapabilities);
	getVkFunction(clean, vkGetPhysicalDeviceSurfacePresentModesKHR, instanceExt->getPhysicalDeviceSurfacePresentModes);
	getVkFunction(clean, vkGetPhysicalDeviceSurfaceSupportKHR, instanceExt->getPhysicalDeviceSurfaceSupport);
	getVkFunction(clean, vkGetPhysicalDeviceMemoryProperties2, instanceExt->getPhysicalDeviceMemoryProperties2);

	getVkFunction(clean, vkDestroySurfaceKHR, instanceExt->destroySurface);

	//Add debug callback

	if(supportsDebug[0]) {

		VkDebugReportCallbackCreateInfoEXT callbackInfo = (VkDebugReportCallbackCreateInfoEXT) {

			.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,

			.flags =
				VK_DEBUG_REPORT_DEBUG_BIT_EXT |
				VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
				VK_DEBUG_REPORT_ERROR_BIT_EXT |
				VK_DEBUG_REPORT_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,

			.pfnCallback = (PFN_vkDebugReportCallbackEXT) onDebugReport,
			.pUserData = instance
		};

		gotoIfError3(clean, checkVkError(instanceExt->debugCreateReportCallback(
			instanceExt->instance, &callbackInfo, NULL, &instanceExt->debugReportCallback
		), e_rr));
	}

	instance->api = EGraphicsApi_Vulkan;
	instance->apiVersion = application.apiVersion;

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		if(FAILED(CreateDXGIFactory2(0, &IID_IDXGIFactory6, (void**) &instanceExt->dxgiFactory)))
			retError(clean, Error_invalidState(0, "VkGraphicsInstance_create couldn't create DXGI factory"));
	#endif

clean:

	CharString_free(&title, alloc);

	ListConstC8_free(&enabledLayers, alloc);
	ListConstC8_free(&enabledExtensions, alloc);

	ListVkExtensionProperties_free(&extensions, alloc);
	ListVkLayerProperties_free(&layers, alloc);

	return s_uccess;
}

void VK_WRAP_FUNC(GraphicsInstance_free)(GraphicsInstance *inst, const Allocator *alloc) {

	(void)alloc;

	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(inst, Vk);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		if(instanceExt->dxgiFactory)
			instanceExt->dxgiFactory->lpVtbl->Release(instanceExt->dxgiFactory);
	#endif

	if(instanceExt->debugDestroyReportCallback && instanceExt->debugReportCallback)
		instanceExt->debugDestroyReportCallback(instanceExt->instance, instanceExt->debugReportCallback, NULL);

	//Instance creation can fail partway (e.g. no compatible driver);
	// in that case the RefPtr is still dec'd, so the ext might not be initialized yet.

	if(instanceExt->destroyInstance && instanceExt->instance)
		instanceExt->destroyInstance(instanceExt->instance, NULL);

	if(instanceExt->vulkanLib)
		DynamicLibrary_free(instanceExt->vulkanLib);
}

const C8 *reqExtensionsName[] = {
	"VK_KHR_synchronization2", "VK_KHR_swapchain"
};

U64 reqExtensionsNameCount = sizeof(reqExtensionsName) / sizeof(reqExtensionsName[0]);

const C8 *optExtensionsName[] = {

	"VK_KHR_performance_query",      "VK_KHR_ray_tracing_pipeline",   "VK_KHR_ray_query",
	"VK_KHR_acceleration_structure", "VK_KHR_fragment_shader_barycentric",

	//The cross vendor SER extension, not the NV one: the shader stack emits SPV_EXT_shader_invocation_reorder,
	// which is what this extension licenses.
	//The NV extension's SPIR-V requirement is the NV form with its own opcode numbering,
	// which OxC3 deliberately doesn't emit.
	//Deliberately NO NV fallback for older SDKs either: enabling the NV device extension can't run the EXT
	// SPIR-V this engine ships, so it would claim a feature every shader then fails on.
	//The string itself needs no SDK support; only the feature/property structs do, and those sites simply
	// skip the query when the SDK lacks them, which leaves the claim unset.

	"VK_EXT_ray_tracing_invocation_reorder",

	"VK_EXT_mesh_shader",            "VK_KHR_fragment_shading_rate",  "VK_KHR_dynamic_rendering",
	"VK_EXT_opacity_micromap",       "VK_EXT_shader_atomic_float",    "VK_KHR_deferred_host_operations",
	"VK_NV_ray_tracing_validation",

	//Deliberately the NV extension even where the SDK has the KHR one.
	//The only thing that consumes this is SPIR-V produced by DXC, and DXC declares
	// SPV_NV_compute_shader_derivatives, whose requirement is VK_NV_compute_shader_derivatives specifically.
	//Enabling the KHR device extension does not satisfy that, so a ddx in a compute shader was rejected at
	// vkCreateShaderModule while the device still advertised the feature.
	//A device exposing only the KHR extension therefore doesn't get the bit, which is the right answer: DXC
	// can't emit anything such a device could run either.

	"VK_NV_compute_shader_derivatives",

	"VK_KHR_maintenance4",        "VK_KHR_buffer_device_address", "VK_EXT_descriptor_indexing", "VK_KHR_driver_properties",
	"VK_KHR_shader_atomic_int64", "VK_KHR_shader_float16_int8",   "VK_KHR_draw_indirect_count", "VK_EXT_memory_budget",
	"VK_NV_cooperative_vector",   "VK_KHR_cooperative_matrix",    "VK_EXT_shader_float8",      "VK_KHR_ray_tracing_position_fetch",

	"VK_EXT_descriptor_heap", "VK_NV_cluster_acceleration_structure", "VK_NV_partitioned_acceleration_structure",

	"VK_KHR_push_descriptor",

	"VK_KHR_create_renderpass2", "VK_KHR_depth_stencil_resolve", "VK_KHR_spirv_1_4", "VK_KHR_shader_float_controls",
	"VK_KHR_maintenance5",

	"VK_KHR_opacity_micromap", "VK_KHR_device_address_commands",

	"VK_EXT_conditional_rendering"
};

U64 optExtensionsNameCount = sizeof(optExtensionsName) / sizeof(optExtensionsName[0]);

#define getDeviceProperties(Condition, StructName, Name, StructType)            \
	StructName Name = (StructName) { .sType = StructType };                     \
	if(Condition) {                                                             \
		*propertiesNext = &Name;                                                \
		propertiesNext = &Name.pNext;                                           \
	} (void) 0

#define getDeviceFeatures(Condition, StructName, Name, StructType)              \
	StructName Name = (StructName) { .sType = StructType };                     \
	if(Condition) {                                                             \
		*featuresNext = &Name;                                                  \
		featuresNext = &Name.pNext;                                             \
	}  (void) 0

//Reporting every unmet requirement instead of bailing on the first one is what tells an emulator or a weak GPU apart;
// one says "this will never run OxC3", the other says "this could run OxC3 if the graphics spec were lowered".
//deviceUnsupported logs the requirement and counts it, the device loop rejects on that count once every check has run.
//These read the device loop's locals (i, features, limits, unsupportedCount) so they only work inside that loop.

#define deviceUnsupported(...) { Log_debugLnx(__VA_ARGS__); ++unsupportedCount; }

#define requireFeature(x) if(!features.x) deviceUnsupported("Vulkan: Device %"PRIu32" lacks feature " #x, i)

#define requireLimit(x, minimum)                                                          \
	if(limits.x < (minimum))                                                              \
		deviceUnsupported(                                                                \
			"Vulkan: Device %"PRIu32" limit " #x " is %"PRIu64", needs %"PRIu64,          \
			i, (U64) limits.x, (U64) (minimum)                                            \
		)

//A few limits are floats (maxSamplerAnisotropy, maxSamplerLodBias, viewportBoundsRange), those print as %f.

#define requireLimitF(x, minimum)                                                         \
	if(limits.x < (minimum))                                                              \
		deviceUnsupported(                                                                \
			"Vulkan: Device %"PRIu32" limit " #x " is %f, needs %f",                      \
			i, (F64) limits.x, (F64) (minimum)                                            \
		)

TList(VkPhysicalDevice);
TListImpl(VkPhysicalDevice);

Bool VK_WRAP_FUNC(GraphicsInstance_getDeviceInfos)(const GraphicsInstance *inst, ListGraphicsDeviceInfo *result, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = inst ? inst->alloc : NULL;

	ListVkPhysicalDevice temp = (ListVkPhysicalDevice) { 0 };
	ListGraphicsDeviceInfo temp2 = (ListGraphicsDeviceInfo) { 0 };
	ListVkLayerProperties temp3 = (ListVkLayerProperties) { 0 };
	ListVkExtensionProperties temp4 = (ListVkExtensionProperties) { 0 };

	if(!inst || !result)
		retError(clean, Error_nullPointer(!inst ? 0 : 2, "VkGraphicsInstance_getDeviceInfos()::inst and result are required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(
			1, 0, "VkGraphicsInstance_getDeviceInfos()::result isn't empty, may indicate memleak"
		));

	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(inst, Vk);

	U32 deviceCount = 0;
	gotoIfError3(clean, checkVkError(instanceExt->enumeratePhysicalDevices(instanceExt->instance, &deviceCount, NULL), e_rr));

	gotoIfError3(clean, ListVkPhysicalDevice_create(deviceCount, alloc, &temp, e_rr));
	gotoIfError3(clean, ListGraphicsDeviceInfo_reserve(&temp2, deviceCount, alloc, e_rr));

	gotoIfError3(clean, checkVkError(
		instanceExt->enumeratePhysicalDevices(instanceExt->instance, &deviceCount, temp.ptrNonConst),
		e_rr
	));

	for (U32 i = 0, j = 0; i < deviceCount; ++i) {

		VkPhysicalDevice dev = temp.ptr[i];

		//Every unmet requirement is logged and counted here, the device is only rejected after the last check has run.

		U64 unsupportedCount = 0;

		//VkOnD3D12 might not match our feature set and it might cause confusion, because it shows 2 extra devices on QCOM
		//Even though we only really have 1 physical device there (it exists only for emulating GL).

		{
			VkPhysicalDeviceProperties2 properties2 = (VkPhysicalDeviceProperties2) {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
			};

			instanceExt->getPhysicalDeviceProperties2(dev, &properties2);

			CharString deviceName = CharString_createRefCStrConst(properties2.properties.deviceName);

			const CharString d3d12Warp = CharString_createRefCStrConst("Microsoft Direct3D12");

			//Logged rather than skipped in silence, because on a machine whose only Vulkan device is this one
			// the enumeration ends up empty with nothing in the log to explain it.

			if (CharString_startsWithStringSensitive(&deviceName, &d3d12Warp, 0)) {
				Log_debugLnx("Vulkan: Skipping device %"PRIu32", it is the Direct3D12 passthrough rather than a real device", i);
				continue;
			}
		}

		//Get extensions and layers

		U32 layerCount = 0, extensionCount = 0;

		gotoIfError3(clean, checkVkError(instanceExt->enumerateDeviceLayerProperties(dev, &layerCount, NULL), e_rr));
		gotoIfError3(clean, ListVkLayerProperties_resize(&temp3, layerCount, alloc, e_rr));
		gotoIfError3(clean, checkVkError(
			instanceExt->enumerateDeviceLayerProperties(dev, &layerCount, temp3.ptrNonConst),
			e_rr
		));

		gotoIfError3(clean, checkVkError(
			instanceExt->enumerateDeviceExtensionProperties(dev, NULL, &extensionCount, NULL),
			e_rr
		));

		gotoIfError3(clean, ListVkExtensionProperties_resize(&temp4, extensionCount, alloc, e_rr));
		gotoIfError3(clean, checkVkError(
			instanceExt->enumerateDeviceExtensionProperties(dev, NULL, &extensionCount, temp4.ptrNonConst), e_rr
		));

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

			for(U64 l = 0; l < reqExtensionsNameCount; ++l) {

				const CharString reqExt = CharString_createRefCStrConst(reqExtensionsName[l]);

				if (CharString_equalsCStringSensitive(&reqExt, name)) {
					reqExtensions[l] = true;
					found = true;
					break;
				}
			}

			//Check if optional extension

			if (!found)
				for(U64 l = 0; l < optExtensionsNameCount; ++l) {

					const CharString optExt = CharString_createRefCStrConst(optExtensionsName[l]);

					if (CharString_equalsCStringSensitive(&optExt, name)) {
						optExtensions[l] = true;
						break;
					}
				}
		}

		for(U64 k = 0; k < reqExtensionsNameCount; ++k)
			if(!reqExtensions[k] && CharString_calcStrLen(reqExtensionsName[k], U64_MAX))
				deviceUnsupported(
					"Vulkan: Unsupported device %"PRIu32", required extension %s wasn't present",
					i, reqExtensionsName[k]
				);

		//Grab limits and features

		//Build up list of properties

		VkPhysicalDeviceProperties2 properties2 = (VkPhysicalDeviceProperties2) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
		};

		void **propertiesNext = &properties2.pNext;

		getDeviceProperties(
			true, VkPhysicalDeviceSubgroupProperties, subgroup, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES
		);

		getDeviceProperties(
			optExtensions[EOptExtensions_MeshShader],
			VkPhysicalDeviceMeshShaderPropertiesEXT,
			meshShaderProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
		);

		getDeviceProperties(
			true,
			VkPhysicalDeviceMultiviewProperties,
			multiViewProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES
		);

		getDeviceProperties(
			optExtensions[EOptExtensions_VariableRateShading],
			VkPhysicalDeviceFragmentShadingRatePropertiesKHR,
			vrsProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR
		);

		getDeviceProperties(
			optExtensions[EOptExtensions_RayAcceleration],
			VkPhysicalDeviceAccelerationStructurePropertiesKHR,
			rtasProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
		);

		getDeviceProperties(
			optExtensions[EOptExtensions_RayPipeline],
			VkPhysicalDeviceRayTracingPipelinePropertiesKHR,
			rtpProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		);

		//No NV fallback: an SDK without the EXT structs can't claim RayReorder at all (the EXT SPIR-V the
		// engine emits wouldn't run on the NV extension), so the properties just stay zeroed.

		#ifdef VK_EXT_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME
			getDeviceProperties(
				optExtensions[EOptExtensions_RayReorder],
				VkPhysicalDeviceRayTracingInvocationReorderPropertiesEXT,
				rayReorderProp,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_EXT
			);
		#endif

		getDeviceProperties(
			optExtensions[EOptExtensions_Bindless],
			VkPhysicalDeviceDescriptorIndexingProperties,
			bindlessProp,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES
		);

		getDeviceProperties(true, VkPhysicalDeviceIDProperties, id, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);

		getDeviceProperties(
			optExtensions[EOptExtensions_DriverProperties], VkPhysicalDeviceDriverProperties, driver,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
		);

		getDeviceProperties(
			true, VkPhysicalDeviceMaintenance3Properties, memorySizeAndDescriptorSets,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES
		);

		getDeviceProperties(
			optExtensions[EOptExtensions_Maintenance4], VkPhysicalDeviceMaintenance4Properties, maxBufferSize,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES
		);

		//Gated on the extension rather than chained unconditionally, because the instance asks for Vulkan 1.1,
		// where this struct only exists if VK_KHR_push_descriptor does.
		//Handing a driver a struct for an extension it never advertised is exactly what emulators mishandle.
		//A device without the extension keeps a zeroed struct, which reads as no push descriptors and turns on
		// the emulated path rather than rejecting the device.

		getDeviceProperties(
			optExtensions[EOptExtensions_PushDescriptor], VkPhysicalDevicePushDescriptorPropertiesKHR, pushDescriptor,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR
		);

		instanceExt->getPhysicalDeviceProperties2(dev, &properties2);

		//This hack is because BAR aperture is 256 MiB, this is used to distinguish AMD APUs

		if(
			optExtensions[EOptExtensions_Maintenance4] && (
				maxBufferSize.maxBufferSize < 256 * MIBI || memorySizeAndDescriptorSets.maxMemoryAllocationSize < 256 * MIBI
			)
		)
			deviceUnsupported(
				"Vulkan: Unsupported device %"PRIu32", maxBufferSize and maxAllocationSize should exceed 256MiB", i
			);

		//Build up list of features

		VkPhysicalDeviceFeatures2 features2 = (VkPhysicalDeviceFeatures2) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
		};

		void **featuresNext = &features2.pNext;

		getDeviceFeatures(
			optExtensions[EOptExtensions_Bindless],
			VkPhysicalDeviceDescriptorIndexingFeatures,
			descriptorIndexing,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_DynamicRendering],
			VkPhysicalDeviceDynamicRenderingFeatures,
			dynamicRendering,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES
		);

		//Same reasoning as the push descriptor properties above; at Vulkan 1.1 this struct comes with the extension.

		getDeviceFeatures(
			reqExtensions[EReqExtensions_Synchronization2],
			VkPhysicalDeviceSynchronization2Features,
			sync2,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_MeshShader],
			VkPhysicalDeviceMeshShaderFeaturesEXT,
			meshShader,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_RaytracingValidation],
			VkPhysicalDeviceRayTracingValidationFeaturesNV,
			rtValidation,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV
		);

		#ifdef VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME        //Prevent failing for older SDK versions
			getDeviceFeatures(
				optExtensions[EOptExtensions_ComputeDeriv],
				VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR,
				computeDeriv,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR
			);
		#else
			getDeviceFeatures(
				optExtensions[EOptExtensions_ComputeDeriv],
				VkPhysicalDeviceComputeShaderDerivativesFeaturesNV,
				computeDeriv,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV
			);
		#endif

		getDeviceFeatures(
			true,
			VkPhysicalDeviceMultiviewFeatures,
			multiViewFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_VariableRateShading],
			VkPhysicalDeviceFragmentShadingRateFeaturesKHR,
			vrsFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayAcceleration],
			VkPhysicalDeviceAccelerationStructureFeaturesKHR,
			rtasFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayPipeline],
			VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
			rtpFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayQuery],
			VkPhysicalDeviceRayQueryFeaturesKHR,
			rayQueryFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR
		);

		//No NV fallback here either: a zeroed feature struct is what keeps the claim honest on older SDKs.

		#ifdef VK_EXT_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME
			getDeviceFeatures(
				optExtensions[EOptExtensions_RayReorder],
				VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT,
				rayReorderFeat,
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_EXT
			);
		#endif

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayMicromapOpacity],
			VkPhysicalDeviceOpacityMicromapFeaturesEXT,
			rayOpacityMicroFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT
		);

		//The KHR promotion is queried independently: a device may expose either or both, and which one answers
		// decides the struct vk_blas.c chains and whether 8-bit OMM indices are legal.

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayMicromapOpacityKHR] && optExtensions[EOptExtensions_DeviceAddressCommands],
			VkPhysicalDeviceOpacityMicromapFeaturesKHR,
			rayOpacityMicroFeatKhr,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_CooperativeVector],
			VkPhysicalDeviceCooperativeVectorFeaturesNV,
			coopVectorFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_FEATURES_NV
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_CooperativeMatrix],
			VkPhysicalDeviceCooperativeMatrixFeaturesKHR,
			coopMatrixFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_ShaderFloat8],
			VkPhysicalDeviceShaderFloat8FeaturesEXT,
			shaderFloat8Feat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT8_FEATURES_EXT
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_ConditionalRendering],
			VkPhysicalDeviceConditionalRenderingFeaturesEXT,
			condRenderFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayTriPosition],
			VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR,
			rayPositionFetchFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_Barycentrics],
			VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR,
			fragBaryFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_DescriptorHeap],
			VkPhysicalDeviceDescriptorHeapFeaturesEXT,
			descriptorHeapFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayClusterAS],
			VkPhysicalDeviceClusterAccelerationStructureFeaturesNV,
			rayClusterASFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_ACCELERATION_STRUCTURE_FEATURES_NV
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_RayPartitionedTLAS],
			VkPhysicalDevicePartitionedAccelerationStructureFeaturesNV,
			rayPartitionedTLASFeat,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PARTITIONED_ACCELERATION_STRUCTURE_FEATURES_NV
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_F16],
			VkPhysicalDeviceShaderFloat16Int8Features,
			float16,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_AtomicI64],
			VkPhysicalDeviceShaderAtomicInt64Features,
			atomics64,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_AtomicF32],
			VkPhysicalDeviceShaderAtomicFloatFeaturesEXT,
			atomicsF32,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_PerfQuery],
			VkPhysicalDevicePerformanceQueryFeaturesKHR,
			perfQuery,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR
		);

		getDeviceFeatures(
			optExtensions[EOptExtensions_BufferDeviceAddress], VkPhysicalDeviceBufferDeviceAddressFeaturesKHR, deviceAddress,
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR
		);

		instanceExt->getPhysicalDeviceFeatures2(dev, &features2);

		VkPhysicalDeviceProperties properties = properties2.properties;
		VkPhysicalDeviceFeatures features = features2.features;
		VkPhysicalDeviceLimits limits = properties.limits;

		//Ensure device is compatible first

		requireFeature(shaderInt16);
		requireFeature(shaderSampledImageArrayDynamicIndexing);
		requireFeature(shaderStorageBufferArrayDynamicIndexing);
		requireFeature(shaderUniformBufferArrayDynamicIndexing);
		requireFeature(samplerAnisotropy);
		requireFeature(drawIndirectFirstInstance);
		requireFeature(independentBlend);
		requireFeature(imageCubeArray);
		requireFeature(fullDrawIndexUint32);
		requireFeature(depthClamp);
		requireFeature(depthBiasClamp);

		//Block compression is one requirement that either BC or ASTC LDR satisfies, so it names both.

		if(!features.textureCompressionBC && !features.textureCompressionASTC_LDR)
			deviceUnsupported(
				"Vulkan: Device %"PRIu32" lacks feature textureCompressionBC or textureCompressionASTC_LDR", i
			);

		requireFeature(multiDrawIndirect);
		requireFeature(sampleRateShading);
		requireFeature(shaderStorageImageReadWithoutFormat);
		requireFeature(shaderStorageImageWriteWithoutFormat);

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

		requireLimit(maxColorAttachments, 8);
		requireLimit(maxFragmentOutputAttachments, 8);
		requireLimit(maxDescriptorSetInputAttachments, 7);
		requireLimit(maxPerStageDescriptorInputAttachments, 7);

		//MSAA is one requirement over eight limits at once, so it reports the intersection rather than each limit.

		if((allMSAA & requiredMSAA) != requiredMSAA)
			deviceUnsupported("Vulkan: Device %"PRIu32" doesn't support 1x and 4x MSAA on every attachment type", i);

		requireLimit(maxComputeSharedMemorySize, 16 * KIBI);
		requireLimit(maxComputeWorkGroupCount[0], U16_MAX);
		requireLimit(maxComputeWorkGroupCount[1], U16_MAX);
		requireLimit(maxComputeWorkGroupCount[2], U16_MAX);
		requireLimit(maxComputeWorkGroupSize[0], 1024);
		requireLimit(maxComputeWorkGroupSize[1], 1024);
		requireLimit(maxComputeWorkGroupSize[2], 64);
		requireLimit(maxFragmentCombinedOutputResources, 16);
		requireLimit(maxFragmentInputComponents, 112);
		requireLimit(maxFramebufferWidth, 16 * KIBI);
		requireLimit(maxFramebufferHeight, 16 * KIBI);
		requireLimit(maxImageDimension1D, 16 * KIBI);
		requireLimit(maxImageDimension2D, 16 * KIBI);
		requireLimit(maxImageDimensionCube, 16 * KIBI);
		requireLimit(maxViewportDimensions[0], 16 * KIBI);
		requireLimit(maxViewportDimensions[1], 16 * KIBI);
		requireLimit(maxFramebufferLayers, 256);
		requireLimit(maxImageDimension3D, 256);
		requireLimit(maxImageArrayLayers, 256);
		requireLimit(maxPushConstantsSize, 128);
		requireLimit(maxSamplerAllocationCount, 1024);
		requireLimitF(maxSamplerAnisotropy, 16);
		requireLimit(maxStorageBufferRange, 128 * MEGA);
		requireLimitF(maxSamplerLodBias, 4);
		requireLimit(maxUniformBufferRange, 64 * KIBI);
		requireLimit(maxVertexInputAttributeOffset, 2047);
		requireLimit(maxVertexInputAttributes, 16);
		requireLimit(maxVertexInputBindings, 16);
		requireLimit(maxVertexInputBindingStride, 2048);
		requireLimit(maxVertexOutputComponents, 124);
		requireLimit(maxMemoryAllocationCount, 4096);
		requireLimit(maxBoundDescriptorSets, 4);

		//viewportBoundsRange[0] is a lower bound that has to reach far enough into the negatives,
		// so unlike every other limit it fails when it's too high rather than too low.

		if(limits.viewportBoundsRange[0] > -32768)
			deviceUnsupported(
				"Vulkan: Device %"PRIu32" limit viewportBoundsRange[0] is %f, needs -32768 or lower",
				i, (F64) limits.viewportBoundsRange[0]
			);

		requireLimitF(viewportBoundsRange[1], 32767);

		//Tesselation is required, and the four per-stage component counts are only meaningful if the feature is there.
		//Reporting each minimum separately tells a device that is one limit short apart from one that has no tesselation.

		requireFeature(tessellationShader);

		if (features.tessellationShader) {

			requireLimit(maxTessellationControlPerVertexInputComponents, 124);
			requireLimit(maxTessellationControlPerVertexOutputComponents, 124);
			requireLimit(maxTessellationEvaluationInputComponents, 124);
			requireLimit(maxTessellationEvaluationOutputComponents, 124);
			requireLimit(maxTessellationControlTotalOutputComponents, 4088);
			requireLimit(maxTessellationControlPerPatchOutputComponents, 120);
			requireLimit(maxTessellationGenerationLevel, 64);
			requireLimit(maxTessellationPatchSize, 32);
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
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:      type = EGraphicsDeviceType_Dedicated;   break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:    type = EGraphicsDeviceType_Integrated;  break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:               type = EGraphicsDeviceType_CPU;         break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:       type = EGraphicsDeviceType_Simulated;   break;
			default:                                                                                break;
		}

		//Vendor

		EGraphicsVendorId vendor = EGraphicsVendorId_Unknown;

		switch (properties.vendorID) {
			case EGraphicsVendorPCIE_NV:                    vendor = EGraphicsVendorId_NV;          break;
			case EGraphicsVendorPCIE_AMD:                   vendor = EGraphicsVendorId_AMD;         break;
			case EGraphicsVendorPCIE_ARM:                   vendor = EGraphicsVendorId_ARM;         break;
			case EGraphicsVendorPCIE_QCOM:                  vendor = EGraphicsVendorId_QCOM;        break;
			case EGraphicsVendorPCIE_INTC:                  vendor = EGraphicsVendorId_INTC;        break;
			case EGraphicsVendorPCIE_IMGT:                  vendor = EGraphicsVendorId_IMGT;        break;
			case EGraphicsVendorPCIE_APPL:                  vendor = EGraphicsVendorId_APPL;        break;
			case EGraphicsVendorPCIE_SMSG:                  vendor = EGraphicsVendorId_SMSG;        break;
			case EGraphicsVendorPCIE_HWEI:                  vendor = EGraphicsVendorId_HWEI;        break;
			case EGraphicsVendorPCIE_GOGL:                  vendor = EGraphicsVendorId_GOGL;        break;
			case EGraphicsVendorPCIE_MESA:                  vendor = EGraphicsVendorId_MESA;        break;
			default:
				Log_debugLnx("Unrecognized vendor: %"PRIX32, properties.vendorID);                  break;
		}

		//Capabilities

		GraphicsDeviceCapabilities capabilities = (GraphicsDeviceCapabilities) { 0 };

		//Timestamps: core Vulkan, gated by timestampComputeAndGraphics, which implies non-zero valid bits on the
		// graphics and compute queues. timestampPeriod is the nanoseconds per tick used to convert results.

		if(limits.timestampComputeAndGraphics) {
			capabilities.features2 |= EGraphicsFeatures2_Timestamps;
			capabilities.timestampPeriod = limits.timestampPeriod;
		}

		//Predicated scopes; the inverted flag (skip while nonzero) is deliberately not exposed, since D3D12's
		// predication only offers this polarity and the API stays the intersection.

		if(optExtensions[EOptExtensions_ConditionalRendering] && condRenderFeat.conditionalRendering)
			capabilities.features2 |= EGraphicsFeatures2_Predication;

		//Query features

		if(features.logicOp)
			capabilities.features |= EGraphicsFeatures_LogicOp;

		if(features.dualSrcBlend)
			capabilities.features |= EGraphicsFeatures_DualSrcBlend;

		if(features.shaderStorageImageMultisample)
			capabilities.features |= EGraphicsFeatures_WriteMSTexture;

		if(features.fillModeNonSolid)
			capabilities.features |= EGraphicsFeatures_Wireframe;

		if(deviceAddress.bufferDeviceAddress)
			capabilities.featuresExt |= EVkGraphicsFeatures_BufferDeviceAddress;

		if(optExtensions[EOptExtensions_Maintenance4])
			capabilities.featuresExt |= EVkGraphicsFeatures_Maintenance4;

		if(optExtensions[EOptExtensions_MemoryBudget])
			capabilities.featuresExt |= EVkGraphicsFeatures_MemoryBudget;

		//Force enable synchronization

		if (!sync2.synchronization2)
			deviceUnsupported("Vulkan: Unsupported device %"PRIu32", Synchronization 2 unsupported!", i);

		//Check if indexing is properly supported

		if(optExtensions[EOptExtensions_Bindless]) {

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

			if(!eq)
				optExtensions[EOptExtensions_Bindless] = false;
		}

		//Direct rendering

		//Dynamic rendering only covers the attachments themselves; resolving a depth stencil one and the render pass
		// objects it's built on come from the two extensions below.
		//The spec makes those dependencies of dynamic rendering, but emulators advertise it without them, and
		// requesting an extension the device never offered is what fails device creation with nothing to go on.

		if (
			optExtensions[EOptExtensions_DynamicRendering] &&
			optExtensions[EOptExtensions_CreateRenderpass2] &&
			optExtensions[EOptExtensions_DepthStencilResolve] &&
			dynamicRendering.dynamicRendering
		)
			capabilities.features |= EGraphicsFeatures_DirectRendering;

		//Shader types

		if(features.geometryShader)
			capabilities.features |= EGraphicsFeatures_GeometryShader;

		//Multi draw

		if(limits.maxDrawIndirectCount >= GIBI && optExtensions[EOptExtensions_MultiDrawIndirectCount])
			capabilities.features |= EGraphicsFeatures_MultiDrawIndirectCount;

		//Subgroup operations

		if(subgroup.subgroupSize != 1) {

			VkSubgroupFeatureFlags requiredSubOp =
				VK_SUBGROUP_FEATURE_BASIC_BIT |
				VK_SUBGROUP_FEATURE_VOTE_BIT |
				VK_SUBGROUP_FEATURE_BALLOT_BIT;

			if((subgroup.supportedOperations & requiredSubOp) != requiredSubOp)
				deviceUnsupported(
					"Vulkan: Unsupported device %"PRIu32", one of the required subgroup operations was missing!", i
				);

			if(subgroup.subgroupSize < 4 || subgroup.subgroupSize > 128)
				deviceUnsupported("Vulkan: Unsupported device %"PRIu32", subgroup size is not in range 4-128!", i);

			capabilities.features |= EGraphicsFeatures_SubgroupOperations;
		}

		if(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)
			capabilities.features |= EGraphicsFeatures_SubgroupArithmetic;

		if(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)
			capabilities.features |= EGraphicsFeatures_SubgroupShuffle;

		if(subgroup.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT)
			capabilities.features |= EGraphicsFeatures_SubgroupQuad;

		//Multi view

		if(
			multiViewFeat.multiview &&
			multiViewProp.maxMultiviewViewCount >= 6 && multiViewProp.maxMultiviewInstanceIndex >= 134217727
		)
			capabilities.features |= EGraphicsFeatures_Multiview;

		//VRS

		if(
			optExtensions[EOptExtensions_VariableRateShading] &&
			optExtensions[EOptExtensions_CreateRenderpass2] &&
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
			meshShaderProp.maxTaskPayloadAndSharedMemorySize < 32 * KIBI ||
			meshShaderProp.maxTaskPayloadSize < 16 * KIBI ||
			meshShaderProp.maxTaskSharedMemorySize < 32 * KIBI ||
			meshShaderProp.maxTaskWorkGroupTotalCount < 4 * MIBI
		))
			capabilities.features |= EGraphicsFeatures_MeshShader;

		//The preference and hint properties gate nothing above: prefersCompactPrimitiveOutput and the
		// maxPreferred* invocation counts are scheduling advice, and the output granularities only say how
		// coarsely output allocations round (ANV rounds by 8 where NV rounds by 32; smaller is finer).
		//maxMeshMultiviewViewCount is not required either: ANV reports 1 with multiviewMeshShader off, so
		// MeshShader does NOT promise the multiview interplay and a mesh+multiview user checks it itself.

		//Raytracing

		if(rtValidation.rayTracingValidation)
			capabilities.features |= EGraphicsFeatures_RayValidation;

		//Both derivative group modes count, since which one a shader needs is DXC's choice rather than ours:
		// it emits the quad group for an even 2D thread group and the linear group otherwise.
		//Reading only the linear flag would advertise the feature to a shader that ends up needing quads.

		//BOTH derivative group modes are required, because which one a shader needs is DXC's choice,
		// it emits the quad group for an even 2D thread group and the linear group otherwise.
		//Granting on either mode alone makes vkCreateDevice fail with VK_ERROR_FEATURE_NOT_PRESENT on any device
		// supporting just one, which loses the whole device rather than only derivatives,
		// so this must stay an AND to match the request.

		if(computeDeriv.computeDerivativeGroupLinear && computeDeriv.computeDerivativeGroupQuads)
			capabilities.features |= EGraphicsFeatures_ComputeDeriv;

		//Not gated on raytracing either:
		// barycentrics is a fragment-shader feature.

		if(fragBaryFeat.fragmentShaderBarycentric)
			capabilities.features |= EGraphicsFeatures_Barycentrics;

		//Cooperative vectors/matrix (linalg) aren't gated on raytracing, so set them here rather than in the RT block.
		//Base tier of each is FP16 + INT8; CoopFP8 is the additive FP8 tier; training exposes the outer-product/reduce ops.
		if(coopVectorFeat.cooperativeVector)
			capabilities.features |= EGraphicsFeatures_CoopVec;

		if(coopVectorFeat.cooperativeVectorTraining)
			capabilities.features |= EGraphicsFeatures_CoopVecTraining;

		if(coopMatrixFeat.cooperativeMatrix)
			capabilities.features |= EGraphicsFeatures_CoopMat;

		if(shaderFloat8Feat.shaderFloat8)
			capabilities.features |= EGraphicsFeatures_CoopFP8;

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

						//Base alignment is a divisor requirement, so smaller is weaker: any power of two up
						// to 64 is satisfied by a table laid out at 64. ANV reports 16.

						!rtpProp.shaderGroupBaseAlignment ||
						rtpProp.shaderGroupBaseAlignment > 64 ||
						(rtpProp.shaderGroupBaseAlignment & (rtpProp.shaderGroupBaseAlignment - 1))
					)
				) {
					optExtensions[EOptExtensions_RayQuery] = false;
					optExtensions[EOptExtensions_RayPipeline] = false;
				}

				//Raytracing shaders are compiled against SPIRV 1.4, which brings the two below with it.
				//Without them the device would advertise raytracing and then fail creation on the extensions it
				// never offered, so the feature is dropped instead.

				if(!optExtensions[EOptExtensions_Spirv14] || !optExtensions[EOptExtensions_ShaderFloatControls]) {
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

					//RayReorder = the SER API is available (valid to call, possibly a no-op).
					//RayReorderActual = the driver hints it actually reorders, so it's worth restructuring shaders for.

					//The EXT device extension (requested above where the SDK has it) licenses exactly the
					// SPV_EXT_shader_invocation_reorder form the shader stack emits: the module validates, the
					// pipeline builds, and a record + reorder + invoke trace runs correctly (Shaders/raysSer).
					//The old oxc::HitObject struct wrapper in extension.RayReorder.hlsli device-losted on SPIR-V,
					// since it embedded the opaque hit object type in a composite.
					//oxc::HitObject is now a bare typedef with C-style free functions, so that failure mode is gone.

					//The query macros above declare rayReorderFeat/Prop, so this block only exists when the SDK
					// has the EXT types; an older SDK never claims RayReorder at all.

					#ifdef VK_EXT_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME
						if(rayReorderFeat.rayTracingInvocationReorder) {

							capabilities.features |= EGraphicsFeatures_RayReorder;

							if(rayReorderProp.rayTracingInvocationReorderReorderingHint)
								capabilities.features2 |= EGraphicsFeatures2_RayReorderActual;
						}
					#endif

					if(rayOpacityMicroFeat.micromap || rayOpacityMicroFeatKhr.micromap)
						capabilities.features |= EGraphicsFeatures_RayMicromapOpacity;

					//KHR is preferred when the device offers it.
					//It would also be the only VK path where 8-bit OMM indices are legal (VUID 11570 vs the EXT
					// VUID 10719), but RayMicromapOpacityU8 deliberately stays UNCLAIMED here: the KHR side isn't
					// implemented yet (micromap arrays refuse, the attach never ran on a real driver), and a
					// capability must not outrun its implementation.

					if(rayOpacityMicroFeatKhr.micromap)
						capabilities.featuresExt |= EVkGraphicsFeatures_OpacityMicromapKHR;

					if(rayPositionFetchFeat.rayTracingPositionFetch)
						capabilities.features |= EGraphicsFeatures_RayTriPosition;

					//Mega geometry (RTXMG); Vulkan splits it into cluster acceleration structures and partitioned TLAS

					if(rayClusterASFeat.clusterAccelerationStructure)
						capabilities.features2 |= EGraphicsFeatures2_RayClusterAS;

					if(rayPartitionedTLASFeat.partitionedAccelerationStructure)
						capabilities.features2 |= EGraphicsFeatures2_RayPartitionedTLAS;

					//GPU-driven AS builds; Vulkan only (see EGraphicsFeatures2_RayIndirectASBuild)

					if(rtasFeat.accelerationStructureIndirectBuild)
						capabilities.features2 |= EGraphicsFeatures2_RayIndirectASBuild;
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

		//Enforce support for the least descriptors.
		//Named one by one rather than as a single "binding tier" verdict,
		// because knowing which of the eleven fell short is what says whether a device is close or hopeless.

		requireLimit(maxPerStageDescriptorSamplers, 16);
		requireLimit(maxPerStageDescriptorUniformBuffers, 12);
		requireLimit(maxPerStageDescriptorStorageBuffers, 8);
		requireLimit(maxPerStageDescriptorSampledImages, 16);
		requireLimit(maxPerStageDescriptorStorageImages, 4);
		requireLimit(maxPerStageResources, 44);
		requireLimit(maxDescriptorSetSamplers, 80);
		requireLimit(maxDescriptorSetUniformBuffers, 72);
		requireLimit(maxDescriptorSetStorageBuffers, 24);
		requireLimit(maxDescriptorSetSampledImages, 96);
		requireLimit(maxDescriptorSetStorageImages, 24);

		//Hopefully enable bindless

		if(
			optExtensions[EOptExtensions_Bindless] &&
			bindlessProp.maxDescriptorSetUpdateAfterBindInputAttachments >= 8 &&
			bindlessProp.maxDescriptorSetUpdateAfterBindSampledImages >= 1000000 &&
			bindlessProp.maxDescriptorSetUpdateAfterBindSamplers >= 1024 &&
			bindlessProp.maxDescriptorSetUpdateAfterBindStorageBuffers >= 1000000 &&
			bindlessProp.maxDescriptorSetUpdateAfterBindStorageImages >= 1000000 &&
			bindlessProp.maxDescriptorSetUpdateAfterBindUniformBuffers >= 90 &&
			bindlessProp.maxPerStageDescriptorUpdateAfterBindInputAttachments >= 8 &&
			bindlessProp.maxPerStageDescriptorUpdateAfterBindSampledImages >= 1000000 &&
			bindlessProp.maxPerStageDescriptorUpdateAfterBindSamplers >= 1024 &&
			bindlessProp.maxPerStageDescriptorUpdateAfterBindStorageBuffers >= 1000000 &&
			bindlessProp.maxPerStageDescriptorUpdateAfterBindStorageImages >= 1000000 &&
			bindlessProp.maxPerStageDescriptorUpdateAfterBindUniformBuffers >= 15 &&
			bindlessProp.maxPerStageUpdateAfterBindResources >= 1000000 &&
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

		//Full bindless (descriptor heap); requires regular bindless too so the feature implies the same on both APIs

		//VK_EXT_descriptor_heap requires VK_KHR_maintenance5 in the enabled list, so a device that doesn't
		// advertise the dependency simply doesn't get the feature.

		if(
			(capabilities.features & EGraphicsFeatures_Bindless) &&
			optExtensions[EOptExtensions_DescriptorHeap] &&
			optExtensions[EOptExtensions_Maintenance5] &&
			descriptorHeapFeat.descriptorHeap
		)
			capabilities.features2 |= EGraphicsFeatures2_DescriptorHeap;

		//Push descriptors are a fast path, not a requirement, so a device without them is emulated rather than rejected.
		//Only the globals constant buffer is ever pushed, so the 32 minimum is far more than OxC3 asks for;
		// it's kept as the threshold so a driver advertising a token amount takes the emulated path instead.

		if(optExtensions[EOptExtensions_PushDescriptor] && pushDescriptor.maxPushDescriptors >= 32)
			capabilities.featuresExt |= EVkGraphicsFeatures_PerformantPushDescriptor;

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

		//Parallel to toSupport, so a device can be told which formats it is missing rather than just that one is missing.

		const C8 *toSupportName[] = {

			//R8s, RG8s, RGBA8s

			"R8_SNORM",
			"R8G8_SNORM",
			"R8G8B8A8_SNORM",

			//R16, R16s
			//RG16, RG16s

			"R16_SNORM",
			"R16G16_SNORM",

			"R16_UNORM",
			"R16G16_UNORM",

			//RGBA16, RGBA16s

			"R16G16B16A16_UNORM",
			"R16G16B16A16_SNORM",

			//BGRA8

			"B8G8R8A8_UNORM",

			//D32

			"D32_SFLOAT"
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

		//toSupport, toSupportName and reqFormatFeatFlags are indexed by the same k,
		// so a format added to one of them has to be added to all three.

		static_assert(
			sizeof(toSupportName) / sizeof(toSupportName[0]) == sizeof(toSupport) / sizeof(toSupport[0]) &&
			sizeof(reqFormatFeatFlags) / sizeof(reqFormatFeatFlags[0]) == sizeof(toSupport) / sizeof(toSupport[0]),
			"toSupport, toSupportName and reqFormatFeatFlags have to stay parallel"
		);

		for(U64 k = 0; k < sizeof(toSupport) / sizeof(VkFormat); ++k) {

			VkFormatProperties formatInfo = (VkFormatProperties) { 0 };
			instanceExt->getPhysicalDeviceFormatProperties(dev, toSupport[k], &formatInfo);

			VkFormatFeatureFlags reqk = reqFormatFeatFlags[k];

			if((formatInfo.optimalTilingFeatures & reqk) != reqk)
				deviceUnsupported(
					"Vulkan: Unsupported device %"PRIu32", required texture format %s not supported!",
					i, toSupportName[k]
				);
		}

		//Query for memory size

		VkGraphicsDevice fakeDevice = (VkGraphicsDevice) { 0 };
		instanceExt->getPhysicalDeviceMemoryProperties(dev, &fakeDevice.memoryProperties);

		//findAllMemory errors out of the entire enumeration when a device exposes no usable heap at all.
		//A device that already failed a requirement is rejected either way,
		// so it keeps its zeroed heap sizes and reports the heap requirement below instead of taking every device with it.

		if(unsupportedCount)
			(void) VkGraphicsDevice_findAllMemory(&fakeDevice, NULL);

		else gotoIfError3(clean, VkGraphicsDevice_findAllMemory(&fakeDevice, e_rr));

		U64 cpuHeapSize = fakeDevice.maxHeapSizes[0];
		U64 gpuHeapSize = fakeDevice.maxHeapSizes[1];

		if(cpuHeapSize < 512 * MIBI || gpuHeapSize < 512 * MIBI)
			deviceUnsupported("Vulkan: Unsupported device %"PRIu32", not enough VRAM or RAM (requires 512MB each).", i);

		//Every requirement has run by now, so the device can be rejected with the full list already logged above.

		if (unsupportedCount) {
			Log_debugLnx("Vulkan: Unsupported device %"PRIu32", %"PRIu64" requirement(s) not met", i, unsupportedCount);
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
			},

			//Reading RGB9E5 is required, so only the write half is probed here.

			(OptionalFormat) {
				.format = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
				.optFormat = EGraphicsDataTypes_WriteRGB9E5,
				.flags = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
			}
		};

		for(U64 k = 0; k < sizeof(optionalFormats) / sizeof(OptionalFormat); ++k) {

			VkFormatProperties formatInfo = (VkFormatProperties) { 0 };
			instanceExt->getPhysicalDeviceFormatProperties(dev, optionalFormats[k].format, &formatInfo);

			if((formatInfo.optimalTilingFeatures & optionalFormats[k].flags) == optionalFormats[k].flags)
				capabilities.dataTypes |= optionalFormats[k].optFormat;
		}

		//Linear filtering, which the loop above can't express: a bit here means EVERY format in its family
		// filters, so the test is an AND where the optional table is an OR.
		//Sampling these is already guaranteed by the required set; this is only about whether a linear
		// sampler over one does anything. A device missing it either trips validation or quietly point
		// samples, and nothing above this line would have caught either.

		static const VkFormat filter16Norm[] = {
			VK_FORMAT_R16_UNORM,           VK_FORMAT_R16_SNORM,
			VK_FORMAT_R16G16_UNORM,        VK_FORMAT_R16G16_SNORM,
			VK_FORMAT_R16G16B16A16_UNORM,  VK_FORMAT_R16G16B16A16_SNORM
		};

		static const VkFormat filter32f[] = {
			VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT
		};

		const struct {
			const VkFormat *formats;
			U64 count;
			EGraphicsDataTypes bit;
		} filterFamilies[] = {
			{ filter16Norm, sizeof(filter16Norm) / sizeof(VkFormat), EGraphicsDataTypes_LinearFilter16Norm },
			{ filter32f,    sizeof(filter32f)    / sizeof(VkFormat), EGraphicsDataTypes_LinearFilter32f }
		};

		for (U64 k = 0; k < sizeof(filterFamilies) / sizeof(filterFamilies[0]); ++k) {

			Bool all = true;

			for (U64 j = 0; j < filterFamilies[k].count; ++j) {

				VkFormatProperties formatInfo = (VkFormatProperties) { 0 };
				instanceExt->getPhysicalDeviceFormatProperties(dev, filterFamilies[k].formats[j], &formatInfo);

				if(!(formatInfo.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
					all = false;
					break;
				}
			}

			if(all)
				capabilities.dataTypes |= filterFamilies[k].bit;
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

		gotoIfError3(clean, ListGraphicsDeviceInfo_resize(&temp2, temp2.length + 1, alloc, e_rr));

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

		Buffer_memcpy(
			Buffer_createRef(info->name, sizeof(info->name)),
			Buffer_createRefConst(properties.deviceName, sizeof(properties.deviceName))
		);

		if(optExtensions[EOptExtensions_DriverProperties]) {

			Buffer_memcpy(
				Buffer_createRef(info->driverInfo, sizeof(info->driverInfo)),
				Buffer_createRefConst(driver.driverInfo, sizeof(driver.driverInfo))
			);

			info->capabilities.featuresExt |= EVkGraphicsFeatures_DriverProperties;
		}

		++j;
	}

	//A machine with no adapter at all and a machine whose adapters OxC3 turned down are very different results.
	//The first isn't ours to fail on and a test should skip it, the second is a real result and says our spec is too high,
	// so they get distinct error codes for the caller to branch on.
	//deviceCount is what the api itself reported, including the Direct3D12 WARP adapter the loop above skips,
	// so zero genuinely means there's no Vulkan adapter rather than that we rejected them all.

	if(!temp2.length) {

		if(!deviceCount)
			retError(clean, Error_notFound(0, 0, "VkGraphicsInstance_getDeviceInfos() no graphics adapter present"));

		retError(clean, Error_unsupportedOperation(0, "VkGraphicsInstance_getDeviceInfos() no supported OxC3 device found"));
	}

	*result = temp2;

clean:

	if(!s_uccess)
		ListGraphicsDeviceInfo_free(&temp2, alloc);

	ListVkPhysicalDevice_free(&temp, alloc);
	ListVkLayerProperties_free(&temp3, alloc);
	ListVkExtensionProperties_free(&temp4, alloc);
	return s_uccess;
}

#undef deviceUnsupported
#undef requireFeature
#undef requireLimit
#undef requireLimitF
