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

#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/generic/device.h"
#include "platforms/ext/listx.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"

#define vkBindNext(T, condition, ...)	\
	T tmp##T = __VA_ARGS__;				\
										\
	if(condition) {						\
		*currPNext = &tmp##T;			\
		currPNext = &tmp##T.pNext;		\
	}

Error GraphicsDevice_initExt(
	const GraphicsInstance *instance, 
	const GraphicsDeviceInfo *physicalDevice, 
	Bool verbose,
	void **ext
) {

	instance;

	EGraphicsFeatures feat = physicalDevice->capabilities.features;
	EGraphicsFeatures featEx = physicalDevice->capabilities.featuresExt;
	EGraphicsDataTypes types = physicalDevice->capabilities.dataTypes;

	VkPhysicalDeviceFeatures features = (VkPhysicalDeviceFeatures) {

		.fullDrawIndexUint32 = true,
		.imageCubeArray = true,
		.independentBlend = true,

		.geometryShader = (Bool)(feat & EGraphicsFeatures_GeometryShader),
		.tessellationShader = (Bool)(feat & EGraphicsFeatures_TessellationShader),

		.multiDrawIndirect = true,

		.drawIndirectFirstInstance = true,
		.depthClamp = true,
		.depthBiasClamp = true,
		.samplerAnisotropy = true,

		.textureCompressionASTC_LDR = (Bool)(types & EGraphicsDataTypes_ASTC),
		.textureCompressionBC = (Bool)(types & EGraphicsDataTypes_BCn),

		.shaderUniformBufferArrayDynamicIndexing = true,
		.shaderSampledImageArrayDynamicIndexing = true,
		.shaderStorageBufferArrayDynamicIndexing = true,
		.shaderStorageImageArrayDynamicIndexing = true,

		.shaderFloat64 = (Bool)(types & EGraphicsDataTypes_F64),
		.shaderInt64 = (Bool)(types & EGraphicsDataTypes_I64),
		.shaderInt16 = true
	};

	VkPhysicalDeviceFeatures2 features2 = (VkPhysicalDeviceFeatures2) {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.features = features
	};

	void **currPNext = &features2.pNext;

	vkBindNext(
		VkPhysicalDeviceDescriptorIndexingFeatures, 
		true, 
		{
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
		}
	);

	vkBindNext(
		VkPhysicalDeviceMultiDrawFeaturesEXT, 
		true, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT,
			.multiDraw = true
		}
	);

	vkBindNext(
		VkPhysicalDevicePerformanceQueryFeaturesKHR, 
		physicalDevice->capabilities.featuresExt & EVkGraphicsFeatures_PerfQuery, 
		(VkPhysicalDevicePerformanceQueryFeaturesKHR) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR,
			.performanceCounterQueryPools = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceDynamicRenderingFeatures, 
		!(feat & EGraphicsFeatures_TiledRendering), 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.dynamicRendering = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceMeshShaderFeaturesEXT, 
		feat & EGraphicsFeatures_MeshShader, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
			.meshShader = true,
			.taskShader = true,
			.multiviewMeshShader = true,
			.primitiveFragmentShadingRateMeshShader = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceAccelerationStructureFeaturesKHR, 
		feat & EGraphicsFeatures_Raytracing, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.accelerationStructure = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceRayQueryFeaturesKHR, 
		feat & EGraphicsFeatures_RayQuery, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
			.rayQuery = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR, 
		feat & EGraphicsFeatures_RayPipeline, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			.rayTracingPipeline = true,
			.rayTraversalPrimitiveCulling = true,
			.rayTracingPipelineTraceRaysIndirect = (Bool)(feat & EGraphicsFeatures_RayIndirect)
		}
	);

	vkBindNext(
		VkPhysicalDeviceRayTracingMotionBlurFeaturesNV, 
		feat & EGraphicsFeatures_RayMotionBlur, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV,
			.rayTracingMotionBlur = true,
			.rayTracingMotionBlurPipelineTraceRaysIndirect = (Bool)(feat & EGraphicsFeatures_RayIndirect)
		}
	);

	vkBindNext(
		VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV, 
		feat & EGraphicsFeatures_RayReorder, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV,
			.rayTracingInvocationReorder = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceOpacityMicromapFeaturesEXT, 
		feat & EGraphicsFeatures_RayMicromapOpacity, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT,
			.micromap = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceDisplacementMicromapFeaturesNV, 
		feat & EGraphicsFeatures_RayMicromapDisplacement, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV,
			.displacementMicromap = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceShaderAtomicInt64Features, 
		types & EGraphicsDataTypes_AtomicI64, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR,
			.shaderBufferInt64Atomics = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, 
		types & (EGraphicsDataTypes_AtomicF32 | EGraphicsDataTypes_AtomicF64), 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
			.shaderBufferFloat32AtomicAdd = (Bool)(types & EGraphicsDataTypes_AtomicF32),
			.shaderBufferFloat32Atomics = (Bool)(types & EGraphicsDataTypes_AtomicF32),
			.shaderBufferFloat64AtomicAdd = (Bool)(types & EGraphicsDataTypes_AtomicF64),
			.shaderBufferFloat64Atomics = (Bool)(types & EGraphicsDataTypes_AtomicF64)
		}
	);

	vkBindNext(
		VkPhysicalDeviceShaderFloat16Int8Features, 
		types & EGraphicsDataTypes_F16, 
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
			.shaderFloat16 = true
		}
	);

	Error err = Error_none();
	List extensions = List_createEmpty(sizeof(const C8*));
	List queues = List_createEmpty(sizeof(VkDeviceQueueCreateInfo));
	List queueFamilies = List_createEmpty(sizeof(VkQueueFamilyProperties));

	_gotoIfError(clean, List_reservex(&extensions, 32));
	_gotoIfError(clean, List_reservex(&queues, EVkGraphicsQueue_Count));

	for(U64 i = 0; i < reqExtensionsNameCount; ++i)
		_gotoIfError(clean, List_pushBackx(&extensions, Buffer_createConstRef(reqExtensionsName + i, sizeof(const C8*))));

	for (U64 i = 0; i < optExtensionsNameCount; ++i) {

		const C8 **ptr = optExtensionsName + i;
		Bool on = false;

		switch (i) {

			case EOptExtensions_DebugMarker:				on = featEx & EVkGraphicsFeatures_DebugMarker;			break;
			case EOptExtensions_F16:						on = types & EGraphicsDataTypes_F16;					break;
			case EOptExtensions_MultiDrawIndirectCount:		on = feat & EGraphicsFeatures_MultiDrawIndirectCount;	break;
			case EOptExtensions_AtomicI64:					on = types & EGraphicsDataTypes_AtomicI64;				break;
			case EOptExtensions_PerfQuery:					on = featEx & EVkGraphicsFeatures_PerfQuery;			break;
			case EOptExtensions_RayPipeline:				on = feat & EGraphicsFeatures_RayPipeline;				break;
			case EOptExtensions_RayQuery:					on = feat & EGraphicsFeatures_RayQuery;					break;
			case EOptExtensions_RayAcceleration:			on = feat & EGraphicsFeatures_Raytracing;				break;
			case EOptExtensions_Swapchain:					on = feat & EGraphicsFeatures_Swapchain;				break;
			case EOptExtensions_RayMotionBlur:				on = feat & EGraphicsFeatures_RayMotionBlur;			break;
			case EOptExtensions_RayReorder:					on = feat & EGraphicsFeatures_RayReorder;				break;
			case EOptExtensions_MeshShader:					on = feat & EGraphicsFeatures_MeshShader;				break;
			case EOptExtensions_DynamicRendering:			on = !(feat & EGraphicsFeatures_TiledRendering);		break;
			case EOptExtensions_RayMicromapOpacity:			on = feat & EGraphicsFeatures_RayMicromapOpacity;		break;
			case EOptExtensions_RayMicromapDisplacement:	on = feat & EGraphicsFeatures_RayMicromapDisplacement;	break;
			case EOptExtensions_AtomicF32:					on = types & EGraphicsDataTypes_AtomicF32;				break;
			case EOptExtensions_DeferredHostOperations:		on = feat & EGraphicsFeatures_Raytracing;				break;
			case EOptExtensions_Sync2:						on = feat & EGraphicsFeatures_RayMicromapOpacity;		break;

			default:
				continue;

			//TODO:
			//case EOptExtensions_VariableRateShading:		on = feat & EGraphicsFeatures_Raytracing;				break;
		}

		if(on)
			_gotoIfError(clean, List_pushBackx(&extensions, Buffer_createConstRef(ptr, sizeof(const C8*))));
	}

	VkPhysicalDevice physicalDeviceExt = (VkPhysicalDevice) physicalDevice->ext;

	//Get queues

	U32 familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceExt, &familyCount, NULL);

	if(!familyCount)
		_gotoIfError(clean, Error_invalidOperation(0));

	_gotoIfError(clean, List_resizex(&queueFamilies, familyCount));
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceExt, &familyCount, (VkQueueFamilyProperties*) queueFamilies.ptr);

	//Assign queues to deviceExt (don't have to be unique)

	//Find queues

	U32 copyQueueId = U32_MAX;
	U32 computeQueueId = U32_MAX;
	U32 graphicsQueueId = U32_MAX;

	U32 fallbackCopyQueueId = U32_MAX;
	U32 fallbackComputeQueueId = U32_MAX;

	VkQueueFlags importantFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

	for (U32 i = 0; i < familyCount; ++i) {

		VkQueueFamilyProperties q = ((VkQueueFamilyProperties*)queueFamilies.ptr)[i];

		if(!q.queueCount)
			continue;

		if(copyQueueId == U32_MAX) {

			if((q.queueFlags & importantFlags) == VK_QUEUE_TRANSFER_BIT)
				copyQueueId = i;

			if(q.queueFlags & VK_QUEUE_TRANSFER_BIT)
				fallbackCopyQueueId = i;
		}

		if(computeQueueId == U32_MAX) {

			if(((q.queueFlags & importantFlags) &~ VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_COMPUTE_BIT)
				computeQueueId = i;

			if(q.queueFlags & VK_QUEUE_COMPUTE_BIT)
				fallbackComputeQueueId = i;
		}

		if(graphicsQueueId == U32_MAX && q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			graphicsQueueId = i;

		if(graphicsQueueId != U32_MAX && computeQueueId != U32_MAX && copyQueueId != U32_MAX)
			break;
	}

	//If there's no dedicated queue, we should use the one that supports it.

	if(computeQueueId == U32_MAX)
		computeQueueId = fallbackComputeQueueId;

	if(copyQueueId == U32_MAX)
		copyQueueId = fallbackCopyQueueId;

	//Ensure we have all queues. Should be impossible, but still.

	if(copyQueueId == U32_MAX || computeQueueId == U32_MAX || graphicsQueueId == U32_MAX)
		_gotoIfError(clean, Error_invalidOperation(1));

	//Assign queues to queues (deviceInfo)

	F32 prio = 1;

	VkDeviceQueueCreateInfo graphicsQueue = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = graphicsQueueId,
		.queueCount = 1,
		.pQueuePriorities = &prio
	};

	_gotoIfError(clean, List_pushBackx(&queues, Buffer_createConstRef(&graphicsQueue, sizeof(graphicsQueue))));

	VkDeviceQueueCreateInfo copyQueue = graphicsQueue;
	copyQueue.queueFamilyIndex = copyQueueId;

	if(copyQueueId != graphicsQueueId)
		_gotoIfError(clean, List_pushBackx(&queues, Buffer_createConstRef(&copyQueue, sizeof(copyQueue))));

	VkDeviceQueueCreateInfo computeQueue = graphicsQueue;
	computeQueue.queueFamilyIndex = computeQueueId;

	if(computeQueueId != graphicsQueueId && computeQueueId != copyQueueId)
		_gotoIfError(clean, List_pushBackx(&queues, Buffer_createConstRef(&computeQueue, sizeof(computeQueue))));

	//Create device

	VkDeviceCreateInfo deviceInfo = (VkDeviceCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.enabledExtensionCount = (U32) extensions.length,
		.ppEnabledExtensionNames = (const char* const*) extensions.ptr,
		.queueCreateInfoCount = (U32) queues.length,
		.pQueueCreateInfos = (const VkDeviceQueueCreateInfo*) queues.ptr
	};

	if (verbose) {

		Log_debugLn("Enabling extensions:");

		for(U32 i = 0; i < (U32) extensions.length; ++i)
			Log_debugLn("\t%s", ((const char* const*) extensions.ptr)[i]);
	}

	VkGraphicsDevice *deviceExt = (VkGraphicsDevice*) ext;

	_gotoIfError(clean, vkCheck(vkCreateDevice(physicalDeviceExt, &deviceInfo, NULL, &deviceExt->device)));

	//Get queues

	//Graphics

	vkGetDeviceQueue(
		deviceExt->device,
		graphicsQueueId,
		0,
		deviceExt->queues + EVkGraphicsQueue_Graphics
	);

	//Compute

	if(computeQueueId == graphicsQueueId)
		deviceExt->queues[EVkGraphicsQueue_Compute] = deviceExt->queues[EVkGraphicsQueue_Graphics];

	else vkGetDeviceQueue(
		deviceExt->device,
		computeQueueId,
		0,
		deviceExt->queues + EVkGraphicsQueue_Compute
	);

	deviceExt->queues[EVkGraphicsQueue_Raytracing] = deviceExt->queues[EVkGraphicsQueue_Compute];

	//Copy

	if(copyQueueId == graphicsQueueId)
		deviceExt->queues[EVkGraphicsQueue_Copy] = deviceExt->queues[EVkGraphicsQueue_Graphics];

	else if(copyQueueId == computeQueueId)
		deviceExt->queues[EVkGraphicsQueue_Copy] = deviceExt->queues[EVkGraphicsQueue_Compute];

	else vkGetDeviceQueue(
		deviceExt->device,
		copyQueueId,
		0,
		deviceExt->queues + EVkGraphicsQueue_Copy
	);

clean:
	List_freex(&extensions);
	List_freex(&queues);
	List_freex(&queueFamilies);
	return err;
}

Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void **ext) {

	if(!instance || !ext || !*ext)
		return instance;

	vkDestroyDevice(((VkGraphicsDevice*)ext)->device, NULL);
	*ext = NULL;
	return true;
}
