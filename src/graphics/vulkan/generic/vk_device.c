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
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "platforms/thread.h"
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
	GraphicsDeviceRef **deviceRef
) {

	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);
	instanceExt;

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
		VkPhysicalDevicePerformanceQueryFeaturesKHR, 
		physicalDevice->capabilities.featuresExt & EVkGraphicsFeatures_PerfQuery, 
		(VkPhysicalDevicePerformanceQueryFeaturesKHR) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR,
			.performanceCounterQueryPools = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceSynchronization2Features, 
		true, 
		(VkPhysicalDeviceSynchronization2Features) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
			.synchronization2 = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceTimelineSemaphoreFeatures, 
		true, 
		(VkPhysicalDeviceTimelineSemaphoreFeatures) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
			.timelineSemaphore = true
		}
	);

	vkBindNext(
		VkPhysicalDeviceDynamicRenderingFeatures, 
		feat & EGraphicsFeatures_DirectRendering, 
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

	Error err = RefPtr_createx(
		(U32)(sizeof(GraphicsDevice) + sizeof(VkGraphicsDevice)), 
		(ObjectFreeFunc) GraphicsDevice_free, 
		EGraphicsTypeId_GraphicsDevice, 
		deviceRef
	);

	if(err.genericError)
		return err;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	List extensions = List_createEmpty(sizeof(const C8*));
	List queues = List_createEmpty(sizeof(VkDeviceQueueCreateInfo));
	List queueFamilies = List_createEmpty(sizeof(VkQueueFamilyProperties));
	CharString tempStr = CharString_createNull();

	_gotoIfError(clean, List_reservex(&extensions, 32));
	_gotoIfError(clean, List_reservex(&queues, EVkCommandQueue_Count));

	for(U64 i = 0; i < reqExtensionsNameCount; ++i)
		_gotoIfError(clean, List_pushBackx(&extensions, Buffer_createConstRef(reqExtensionsName + i, sizeof(const C8*))));

	for (U64 i = 0; i < optExtensionsNameCount; ++i) {

		const C8 **ptr = optExtensionsName + i;
		Bool on = false;

		switch (i) {

			case EOptExtensions_DebugMarker:				on = feat & EGraphicsFeatures_DebugMarkers;				break;
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
			case EOptExtensions_DynamicRendering:			on = feat & EGraphicsFeatures_DirectRendering;			break;
			case EOptExtensions_RayMicromapOpacity:			on = feat & EGraphicsFeatures_RayMicromapOpacity;		break;
			case EOptExtensions_RayMicromapDisplacement:	on = feat & EGraphicsFeatures_RayMicromapDisplacement;	break;
			case EOptExtensions_AtomicF32:					on = types & EGraphicsDataTypes_AtomicF32;				break;
			case EOptExtensions_DeferredHostOperations:		on = feat & EGraphicsFeatures_Raytracing;				break;

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

	_gotoIfError(clean, vkCheck(vkCreateDevice(physicalDeviceExt, &deviceInfo, NULL, &deviceExt->device)));

	//Get queues

	//Graphics

	VkCommandQueue *graphicsQueueExt = &deviceExt->queues[EVkCommandQueue_Graphics];

	U32 resolvedId = 0;

	vkGetDeviceQueue(
		deviceExt->device,
		graphicsQueueExt->queueId = graphicsQueueId,
		0,
		&graphicsQueueExt->queue
	);

	deviceExt->uniqueQueues[resolvedId] = graphicsQueueId;
	graphicsQueueExt->resolvedQueueId = resolvedId++;
	graphicsQueueExt->type = EVkCommandQueue_Graphics;

	#ifndef NDEBUG

		VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_QUEUE
		};

		if(instanceExt->debugSetName) {
			debugName.pObjectName = "Graphics queue";
			debugName.objectHandle = (U64) graphicsQueueExt->queue;
			_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)));
		}

	#endif

	//Compute

	VkCommandQueue *computeQueueExt = &deviceExt->queues[EVkCommandQueue_Compute];

	if(computeQueueId == graphicsQueueId)
		*computeQueueExt = *graphicsQueueExt;

	else {

		vkGetDeviceQueue(
			deviceExt->device,
			computeQueueExt->queueId = computeQueueId,
			0,
			&computeQueueExt->queue
		);

		#ifndef NDEBUG
			if(instanceExt->debugSetName) {
				debugName.pObjectName = "Compute queue";
				debugName.objectHandle = (U64) computeQueueExt->queue;
				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)));
			}
		#endif

		deviceExt->uniqueQueues[resolvedId] = computeQueueId;
		computeQueueExt->resolvedQueueId = resolvedId++;
		computeQueueExt->type = EVkCommandQueue_Compute;
	}

	//Copy

	VkCommandQueue *copyQueueExt = &deviceExt->queues[EVkCommandQueue_Copy];

	if(copyQueueId == graphicsQueueId)
		*copyQueueExt = *graphicsQueueExt;

	else if(copyQueueId == computeQueueId)
		*copyQueueExt = *computeQueueExt;

	else {

		vkGetDeviceQueue(
			deviceExt->device,
			copyQueueExt->queueId = copyQueueId,
			0,
			&copyQueueExt->queue
		);

		#ifndef NDEBUG
			if(instanceExt->debugSetName) {
				debugName.pObjectName = "Copy queue";
				debugName.objectHandle = (U64) copyQueueExt->queue;
				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)));
			}
		#endif

		deviceExt->uniqueQueues[resolvedId] = copyQueueId;
		copyQueueExt->resolvedQueueId = resolvedId++;
		copyQueueExt->type = EVkCommandQueue_Copy;
	}

	//Create command recorder per queue per thread per backbuffer.
	//We only allow double and triple buffering, so allocate for triple buffers.
	//These will be initialized JIT because we don't know what thread will be accessing them.

	U32 threads = Thread_getLogicalCores();

	deviceExt->commandPools = List_createEmpty(sizeof(VkCommandAllocator));
	_gotoIfError(clean, List_resizex(&deviceExt->commandPools, 3 * threads * resolvedId));

	//Semaphores

	deviceExt->submitSemaphores = List_createEmpty(sizeof(VkSemaphore));
	_gotoIfError(clean, List_resizex(&deviceExt->submitSemaphores, 3));

	for (U64 k = 0; k < 3; ++k) {

		VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkSemaphore *semaphore = ((VkSemaphore*)deviceExt->submitSemaphores.ptr) + k;

		_gotoIfError(clean, vkCheck(vkCreateSemaphore(deviceExt->device, &semaphoreInfo, NULL, semaphore)));

		#ifndef NDEBUG

			if(instanceExt->debugSetName) {

				_gotoIfError(clean, CharString_formatx(&tempStr, "Queue submit semaphore %i", k));

				VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_SEMAPHORE,
					.objectHandle = (U64) *semaphore,
					.pObjectName = tempStr.ptr,
				};

				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));

				CharString_freex(&tempStr);
			}

		#endif
	}

	//Create timeline semaphore

	VkSemaphoreTypeCreateInfo timelineInfo = (VkSemaphoreTypeCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
	};

	VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &timelineInfo
	};

	_gotoIfError(clean, vkCheck(vkCreateSemaphore(deviceExt->device, &semaphoreInfo, NULL, &deviceExt->commitSemaphore)));

	deviceExt->resolvedQueues = resolvedId;
	goto success;

clean:

	GraphicsDeviceRef_dec(deviceRef);

success:

	CharString_freex(&tempStr);
	List_freex(&extensions);
	List_freex(&queues);
	List_freex(&queueFamilies);
	return err;
}

Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext) {

	if(!instance || !ext)
		return instance;

	VkGraphicsDevice *deviceExt = (VkGraphicsDevice*)ext;

	if(deviceExt->device) {

		for(U64 i = 0; i < deviceExt->commandPools.length; ++i) {

			VkCommandAllocator alloc = ((VkCommandAllocator*)deviceExt->commandPools.ptr)[i];

			if(alloc.cmd)
				vkFreeCommandBuffers(deviceExt->device, alloc.pool, 1, &alloc.cmd);

			if(alloc.pool)
				vkDestroyCommandPool(deviceExt->device, alloc.pool, NULL);
		}

		for(U64 i = 0; i < deviceExt->submitSemaphores.length; ++i) {

			VkSemaphore semaphore = ((VkSemaphore*)deviceExt->submitSemaphores.ptr)[i];

			if(semaphore)
				vkDestroySemaphore(deviceExt->device, semaphore, NULL);
		}

		if(deviceExt->commitSemaphore) {
			vkDestroySemaphore(deviceExt->device, deviceExt->commitSemaphore, NULL);
			deviceExt->commitSemaphore = NULL;
		}

		vkDestroyDevice(deviceExt->device, NULL);
	}

	List_freex(&deviceExt->commandPools);
	List_freex(&deviceExt->submitSemaphores);

	//Free temp storage

	List_freex(&deviceExt->waitStages);
	List_freex(&deviceExt->waitSemaphores);
	List_freex(&deviceExt->results);
	List_freex(&deviceExt->swapchainIndices);
	List_freex(&deviceExt->swapchainHandles);

	return true;
}

//Executing commands

Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef)
		return Error_nullPointer(0);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	return vkCheck(vkDeviceWaitIdle(deviceExt->device));
}

VkCommandAllocator *VkGraphicsDevice_getCommandAllocator(
	VkGraphicsDevice *device, U32 resolvedQueueId, U32 threadId, U8 backBufferId
) {

	U32 threadCount = Thread_getLogicalCores();

	if(!device || resolvedQueueId >= device->resolvedQueues || threadId >= threadCount || backBufferId >= 3)
		return NULL;

	U32 id = resolvedQueueId + (backBufferId * threadCount + threadId) * device->resolvedQueues;

	return (VkCommandAllocator*) device->commandPools.ptr + id;
}

Error GraphicsDeviceRef_submitCommands(GraphicsDeviceRef *deviceRef, List commandLists, List swapchains) {

	//Validation and ext structs

	if(!deviceRef || (!swapchains.length && !commandLists.length))
		return Error_nullPointer(!deviceRef ? 0 : 1);

	if(swapchains.length && swapchains.stride != sizeof(SwapchainRef*))
		return Error_invalidParameter(2, 0);

	if(commandLists.length && commandLists.stride != sizeof(CommandListRef*))
		return Error_invalidParameter(2, 0);

	//Validate command lists

	for(U64 i = 0; i < commandLists.length; ++i)
		if(!CommandListRef_ptr(((CommandListRef**) commandLists.ptr)[i]))
			return Error_nullPointer(1);

	//Swapchains all need to have the same vsync option.

	Swapchain *firstSwapchain = swapchains.length ? SwapchainRef_ptr(((SwapchainRef**) swapchains.ptr)[0]) : 0;

	if(swapchains.length && !firstSwapchain)
		return Error_nullPointer(2);

	Bool vsync = swapchains.length ? firstSwapchain->info.vsync : false;

	for (U64 i = 1; i < swapchains.length; ++i) {

		Swapchain *swapchaini = SwapchainRef_ptr(((SwapchainRef**) swapchains.ptr)[i]);

		if(!swapchaini)
			return Error_nullPointer(2);

		if(swapchaini->info.vsync != vsync)
			return Error_unsupportedOperation(0);
	}

	//Unpack pointers

	Error err = Error_none();
	CharString temp = CharString_createNull();
	List imageBarriers = List_createEmpty(sizeof(VkImageMemoryBarrier2));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);
	
	//Reserve temp storage

	if(!deviceExt->swapchainHandles.capacityAndRefInfo)
		deviceExt->swapchainHandles = List_createEmpty(sizeof(VkSwapchainKHR));

	_gotoIfError(clean, List_clear(&deviceExt->swapchainHandles));
	_gotoIfError(clean, List_reservex(&deviceExt->swapchainHandles, swapchains.length));

	if(!deviceExt->swapchainIndices.capacityAndRefInfo)
		deviceExt->swapchainIndices = List_createEmpty(sizeof(U32));

	_gotoIfError(clean, List_clear(&deviceExt->swapchainIndices));
	_gotoIfError(clean, List_reservex(&deviceExt->swapchainIndices, swapchains.length));

	if(!deviceExt->results.capacityAndRefInfo)
		deviceExt->results = List_createEmpty(sizeof(VkResult));

	_gotoIfError(clean, List_clear(&deviceExt->results));
	_gotoIfError(clean, List_reservex(&deviceExt->results, swapchains.length));

	if(!deviceExt->waitSemaphores.capacityAndRefInfo)
		deviceExt->waitSemaphores = List_createEmpty(sizeof(VkSemaphore));

	_gotoIfError(clean, List_clear(&deviceExt->waitSemaphores));
	_gotoIfError(clean, List_reservex(&deviceExt->waitSemaphores, swapchains.length + 1));

	if(!deviceExt->waitStages.capacityAndRefInfo)
		deviceExt->waitStages = List_createEmpty(sizeof(VkPipelineStageFlags));

	_gotoIfError(clean, List_clear(&deviceExt->waitStages));
	_gotoIfError(clean, List_reservex(&deviceExt->waitStages, swapchains.length + 1));

	//Acquire swapchain images

	VkPipelineStageFlagBits pipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	Buffer pipelineStageBuffer = Buffer_createConstRef(&pipelineStage, sizeof(pipelineStage));

	for(U64 i = 0; i < swapchains.length; ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(((SwapchainRef**) swapchains.ptr)[i]);
		VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

		VkSemaphore semaphore = 
			((VkSemaphore*)swapchainExt->semaphores.ptr)[device->submitId % swapchainExt->semaphores.length];

		_gotoIfError(clean, vkCheck(instanceExt->acquireNextImage(
			deviceExt->device,
			swapchainExt->swapchain,
			U64_MAX,
			semaphore,
			VK_NULL_HANDLE,
			&swapchainExt->currentIndex
		)));

		((VkSwapchainKHR*) deviceExt->swapchainHandles.ptr)[i] = swapchainExt->swapchain;
		((U32*) deviceExt->swapchainIndices.ptr)[i] = swapchainExt->currentIndex;

		Buffer acquireSemaphore = Buffer_createConstRef(&semaphore, sizeof(semaphore));
		_gotoIfError(clean, List_pushBackx(&deviceExt->waitSemaphores, acquireSemaphore));
		_gotoIfError(clean, List_pushBackx(&deviceExt->waitStages, pipelineStageBuffer));
	}

	//Wait for previous frame semaphore

	if (device->submitId >= 3) {

		U64 value = device->submitId - 3 + 1;

		VkSemaphoreWaitInfo waitInfo = (VkSemaphoreWaitInfo) {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.pSemaphores = &deviceExt->commitSemaphore,
			.semaphoreCount = 1,
			.pValues = &value
		};

		_gotoIfError(clean, vkCheck(vkWaitSemaphores(deviceExt->device, &waitInfo, U64_MAX)));
	}

	//Record command list

	VkCommandBuffer commandBuffer = NULL;

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	
	if (commandLists.length) {

		U32 threadId = 0;

		VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, (U8)(device->submitId % 3)
		);

		if(!allocator)
			_gotoIfError(clean, Error_nullPointer(0));

		//We create command pools only the first 3 frames, after that they're cached.
		//This is because we have space for [queues][threads][3] command pools.
		//Allocating them all even though currently only 1x3 are used is quite suboptimal.
		//We only have the space to allow for using more in the future.

		if(!allocator->pool) {

			//TODO: Multi thread command recording
		
			VkCommandPoolCreateInfo poolInfo = (VkCommandPoolCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
				.queueFamilyIndex = queue.queueId
			};

			_gotoIfError(clean, vkCheck(vkCreateCommandPool(deviceExt->device, &poolInfo, NULL, &allocator->pool)));

			#ifndef NDEBUG

				if(instanceExt->debugSetName) {

					_gotoIfError(clean, CharString_formatx(
						&temp, 
						"%s command pool (thread: %u, frame id: %u)",
						queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
							queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
						),
						threadId,
						device->submitId % 3
					));

					VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
						.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
						.objectType = VK_OBJECT_TYPE_COMMAND_POOL,
						.pObjectName = temp.ptr,
						.objectHandle = (U64) allocator->pool
					};

					_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)));

					CharString_freex(&temp);
				}

			#endif
		}

		else _gotoIfError(clean, vkCheck(vkResetCommandPool(
			deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
		)));

		//Allocate command buffer if not present yet

		if (!allocator->cmd) {

			VkCommandBufferAllocateInfo bufferInfo = (VkCommandBufferAllocateInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = allocator->pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			_gotoIfError(clean, vkCheck(vkAllocateCommandBuffers(deviceExt->device, &bufferInfo, &allocator->cmd)));

			#ifndef NDEBUG

				if(instanceExt->debugSetName) {

					_gotoIfError(clean, CharString_formatx(
						&temp, 
						"%s command buffer (thread: %u, frame id: %u)",
						queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
							queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
						),
						threadId,
						device->submitId % 3
					));

					VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
						.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
						.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
						.pObjectName = temp.ptr,
						.objectHandle = (U64) allocator->cmd
					};

					_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)));

					CharString_freex(&temp);
				}

			#endif
		}

		//Start buffer

		commandBuffer = allocator->cmd;

		VkCommandBufferState state = (VkCommandBufferState) {
			.buffer = commandBuffer
		};

		VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		};

		_gotoIfError(clean, vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo)));

		//Record commands

		for (U64 i = 0; i < commandLists.length; ++i) {

			CommandList *commandList = CommandListRef_ptr(((CommandListRef**) commandLists.ptr)[i]);
			const U8 *ptr = commandList->data.ptr;

			for (U64 j = 0; j < commandList->commandOps.length; ++j) {

				CommandOpInfo info = ((CommandOpInfo*) commandList->commandOps.ptr)[j];
				
				//Extra debugging if an error happens while processing the command

				err = CommandList_process(device, info.op, ptr, &state);

				if (err.genericError) {

					#ifndef NDEBUG

						const U8 *callstack = commandList->callstacks.ptr + commandList->callstacks.stride * j;

						Log_warnLn("Command process failed. Command inserted at callstack:");

						Log_printCapturedStackTraceCustom(
							(const void**) callstack, 
							commandList->callstacks.stride / sizeof(void*),
							ELogLevel_Error, 
							ELogOptions_Default
						);

					#endif

					goto clean;
				}

				ptr += info.opSize;
			}
		}

		//Check for invalid state.

		if(state.debugRegionStack)
			_gotoIfError(clean, Error_invalidOperation(0));

		if(!I32x2_eq2(state.currentSize, I32x2_zero()))
			_gotoIfError(clean, Error_invalidOperation(1));

		//Transition back swapchains to present

		//Combine transitions into one call.

		VkDependencyInfo dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		_gotoIfError(clean, List_reservex(&imageBarriers, swapchains.length));

		for (U64 i = 0; i < swapchains.length; ++i) {

			Swapchain *swapchain = SwapchainRef_ptr(((SwapchainRef**) swapchains.ptr)[i]);
			VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

			VkManagedImage *imageExt = &((VkManagedImage*)swapchainExt->images.ptr)[swapchainExt->currentIndex];

			VkImageSubresourceRange range = (VkImageSubresourceRange) {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			};

			U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

			_gotoIfError(clean, VkSwapchain_transition(
				imageExt,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 
				0, 
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				graphicsQueueId,
				&range,
				&imageBarriers,
				&dependency
			));
		}

		if(dependency.imageMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		//End buffer

		_gotoIfError(clean, vkCheck(vkEndCommandBuffer(commandBuffer)));
	}

	//Submit queue
	//TODO: Multiple queues

	U64 waitValue = device->submitId - 3 + 1;

	VkSemaphore signalSemaphores[2] = {
		deviceExt->commitSemaphore,
		((const VkSemaphore*)deviceExt->submitSemaphores.ptr)[device->submitId % 3]
	};

	U64 signalValues[2] = { device->submitId + 1, 1 };

	VkTimelineSemaphoreSubmitInfo timelineInfo = (VkTimelineSemaphoreSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
		.waitSemaphoreValueCount = device->submitId >= 3,
		.pWaitSemaphoreValues = device->submitId >= 3 ? &waitValue : NULL,
		.signalSemaphoreValueCount = swapchains.length ? 2 : 1,
		.pSignalSemaphoreValues = signalValues
	};

	VkSubmitInfo submitInfo = (VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = &timelineInfo,
		.waitSemaphoreCount = (U32) deviceExt->waitSemaphores.length,
		.pWaitSemaphores = (VkSemaphore*) deviceExt->waitSemaphores.ptr,
		.signalSemaphoreCount = swapchains.length ? 2 : 1,
		.pSignalSemaphores = signalSemaphores,
		.pCommandBuffers = &commandBuffer,
		.commandBufferCount = commandBuffer ? 1 : 0,
		.pWaitDstStageMask = (const VkPipelineStageFlags*) deviceExt->waitStages.ptr
	};

	_gotoIfError(clean, vkCheck(vkQueueSubmit(queue.queue, 1, &submitInfo, VK_NULL_HANDLE)));

	//Presents

	if(swapchains.length) {

		VkPresentInfoKHR presentInfo = (VkPresentInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = ((const VkSemaphore*) deviceExt->submitSemaphores.ptr) + device->submitId % 3,
			.swapchainCount = (U32) swapchains.length,
			.pSwapchains = (const VkSwapchainKHR*) deviceExt->swapchainHandles.ptr,
			.pImageIndices = (const U32*) deviceExt->swapchainIndices.ptr,
			.pResults = (VkResult*) deviceExt->results.ptr
		};

		_gotoIfError(clean, vkCheck(vkQueuePresentKHR(queue.queue, &presentInfo)));

		for(U64 i = 0; i < deviceExt->results.length; ++i)
			_gotoIfError(clean, vkCheck(((const VkResult*) deviceExt->results.ptr)[i]));
	}

	//Ensure our next fence value is used

	++device->submitId;

clean: 
	CharString_freex(&temp);
	return err;
}
