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
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device_buffer.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/time.h"

#define vkBindNext(T, condition, ...)	\
	T tmp##T = __VA_ARGS__;				\
										\
	if(condition) {						\
		*currPNext = &tmp##T;			\
		currPNext = &tmp##T.pNext;		\
	}

const U64 GraphicsDeviceExt_size = sizeof(VkGraphicsDevice);

//Convert command into API dependent instructions
impl Error CommandList_process(GraphicsDevice *device, ECommandOp op, const U8 *data, void *commandListExt);

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
		.shaderInt16 = true,

		.fillModeNonSolid = (Bool)(feat & EGraphicsFeatures_Wireframe),
		.logicOp = (Bool)(feat & EGraphicsFeatures_LogicOp),
		.dualSrcBlend = (Bool)(feat & EGraphicsFeatures_DualSrcBlend)
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

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	List extensions = List_createEmpty(sizeof(const C8*));
	List queues = List_createEmpty(sizeof(VkDeviceQueueCreateInfo));
	List queueFamilies = List_createEmpty(sizeof(VkQueueFamilyProperties));
	CharString tempStr = CharString_createNull();

	Error err = Error_none();
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

	//Create shared layout since we use bindless

	for (U32 i = 0; i < EDescriptorType_Count; ++i) {

		VkDescriptorSetLayoutBinding binding = (VkDescriptorSetLayoutBinding) {
			.stageFlags = VK_SHADER_STAGE_ALL,
			.descriptorCount = descriptorTypeCount[i]
		};

		switch (i) {

			case 0:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				break;

			case 1: case 2: case 3:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				break;

			case 4: case 16:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				break;

			case 5:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				break;

			default:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				break;
		}

		//One binding per set.

		VkDescriptorBindingFlags flag = 
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
			VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo partiallyBound = (VkDescriptorSetLayoutBindingFlagsCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = 1,
			.pBindingFlags = &flag
		};

		VkDescriptorSetLayoutCreateInfo setInfo = (VkDescriptorSetLayoutCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &partiallyBound,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
			.bindingCount = 1,
			.pBindings = &binding
		};

		_gotoIfError(clean, vkCheck(vkCreateDescriptorSetLayout(
			deviceExt->device, &setInfo, NULL, &deviceExt->setLayouts[i]
		)));

		if(i < EDescriptorType_ResourceCount)
			_gotoIfError(clean, Buffer_createEmptyBytesx((binding.descriptorCount + 7) >> 3, &deviceExt->freeList[i]));
	}

	VkPipelineLayoutCreateInfo layoutInfo = (VkPipelineLayoutCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = EDescriptorType_Count,
		.pSetLayouts = deviceExt->setLayouts
	};

	_gotoIfError(clean, vkCheck(vkCreatePipelineLayout(deviceExt->device, &layoutInfo, NULL, &deviceExt->defaultLayout)));

	//We only need one pool and 1 descriptor set per EDescriptorType since we use bindless.
	//Every resource is automatically allocated into their respective descriptor set.
	//Last descriptor set (cbuffer) is triple buffered to allow swapping part of the UBO

	U32 sampledImages = 
		descriptorTypeCount[EDescriptorType_Texture2D] + 
		descriptorTypeCount[EDescriptorType_Texture3D] +
		descriptorTypeCount[EDescriptorType_TextureCube];

	U32 storageImages = 
		descriptorTypeCount[EDescriptorType_RWTexture2D] + 
		descriptorTypeCount[EDescriptorType_RWTexture2Ds] + 
		descriptorTypeCount[EDescriptorType_RWTexture2Df] + 
		descriptorTypeCount[EDescriptorType_RWTexture2Di] + 
		descriptorTypeCount[EDescriptorType_RWTexture2Du] + 
		descriptorTypeCount[EDescriptorType_RWTexture3D] +
		descriptorTypeCount[EDescriptorType_RWTexture3Ds] +
		descriptorTypeCount[EDescriptorType_RWTexture3Df] +
		descriptorTypeCount[EDescriptorType_RWTexture3Di] +
		descriptorTypeCount[EDescriptorType_RWTexture3Du];

	VkDescriptorPoolSize poolSizes[] = {
		(VkDescriptorPoolSize) { .type = VK_DESCRIPTOR_TYPE_SAMPLER, descriptorTypeCount[EDescriptorType_Sampler] },
		(VkDescriptorPoolSize) { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storageImages },
		(VkDescriptorPoolSize) { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImages },
		(VkDescriptorPoolSize) { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorTypeCount[EDescriptorType_RWBuffer] },
		(VkDescriptorPoolSize) { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorTypeCount[EDescriptorType_Buffer] + 1 }
	};

	VkDescriptorPoolCreateInfo poolInfo = (VkDescriptorPoolCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = EDescriptorType_Count + 2,
		.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]),
		.pPoolSizes = poolSizes
	};

	_gotoIfError(clean, vkCheck(vkCreateDescriptorPool(deviceExt->device, &poolInfo, NULL, &deviceExt->descriptorPool)));

	VkDescriptorSetVariableDescriptorCountAllocateInfo allocationCount = (VkDescriptorSetVariableDescriptorCountAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.descriptorSetCount = EDescriptorType_Count + 2,
		.pDescriptorCounts = descriptorTypeCount
	};

	//Last layout repeat 3x (that's the CBuffer which needs 3 different versions)

	VkDescriptorSetLayout setLayouts[EDescriptorType_Count + 2];

	for(U64 i = 0; i < EDescriptorType_Count + 2; ++i)
		setLayouts[i] = deviceExt->setLayouts[U64_min(i, EDescriptorType_Count - 1)];

	VkDescriptorSetAllocateInfo setInfo = (VkDescriptorSetAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &allocationCount,
		.descriptorPool = deviceExt->descriptorPool,
		.descriptorSetCount = EDescriptorType_Count + 2,
		.pSetLayouts = setLayouts
	};

	_gotoIfError(clean, vkCheck(vkAllocateDescriptorSets(deviceExt->device, &setInfo, deviceExt->sets)));

	#ifndef NDEBUG

		static const C8 *debugNames[] = {
			"Sampler",
			"Texture2D",
			"TextureCube",
			"Texture3D",
			"Buffer",
			"RWBuffer",
			"RWTexture3D",
			"RWTexture3Ds",
			"RWTexture3Df",
			"RWTexture3Di",
			"RWTexture3Du",
			"RWTexture2D",
			"RWTexture2Ds",
			"RWTexture2Df",
			"RWTexture2Di",
			"RWTexture2Du",
			"Global frame cbuffer (0)",
			"Global frame cbuffer (1)",
			"Global frame cbuffer (2)"
		};

		if(instanceExt->debugSetName)
			for (U32 i = 0; i < EDescriptorType_Count + 2; ++i) {

				CharString_freex(&tempStr);

				_gotoIfError(clean, CharString_formatx(&tempStr, "Descriptor set (%i: %s)", i, debugNames[i]));

				VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
					.objectHandle = (U64) deviceExt->sets[i],
					.pObjectName = tempStr.ptr,
				};

				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));

				CharString_freex(&tempStr);

				_gotoIfError(clean, CharString_formatx(&tempStr, "Descriptor set layout (%i: %s)", i, debugNames[i]));

				if(i < EDescriptorType_Count) {

					debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
						.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
						.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
						.objectHandle = (U64) deviceExt->setLayouts[i],
						.pObjectName = tempStr.ptr,
					};

					_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName2)));
				}
			}

	#endif

	_gotoIfError(clean, Lock_create(&deviceExt->descriptorLock));

	//Get memory properties

	vkGetPhysicalDeviceMemoryProperties((VkPhysicalDevice) physicalDevice->ext, &deviceExt->memoryProperties);

	//Allocate staging buffer.
	//256 MiB - 256 * 3 to allow CBuffer at the end of the memory.
	//This gets divided into 3 for each frame id to have their own allocator into that subblock.
	//This allows way easier management.

	U64 stagingSize = DeviceMemoryBlock_defaultSize - sizeof(CBufferData) * 3;

	_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		*deviceRef, EDeviceBufferUsage_CPUAllocatedBit, CharString_createConstRefCStr("Staging buffer"),
		stagingSize, &deviceExt->staging
	));

	DeviceBuffer *staging = DeviceBufferRef_ptr(deviceExt->staging);
	VkDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Vk);

	Buffer stagingBuffer = Buffer_createRef(stagingExt->mappedMemory, stagingSize);

	for(U64 i = 0; i < sizeof(deviceExt->stagingAllocations) / sizeof(deviceExt->stagingAllocations[0]); ++i)
		_gotoIfError(clean, AllocationBuffer_createRefFromRegionx(
			stagingBuffer, stagingSize / 3 * i, stagingSize / 3, &deviceExt->stagingAllocations[i]
		));

	//Allocate UBO

	_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		*deviceRef, EDeviceBufferUsage_CPUAllocatedBit, CharString_createConstRefCStr("Per frame data"),
		sizeof(CBufferData) * 3, &deviceExt->ubo
	));

	//Fill last 3 descriptor sets with UBO[i] to ensure we only modify things in flight.

	VkDescriptorBufferInfo uboBufferInfo[3] = {
		(VkDescriptorBufferInfo) {
			.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(deviceExt->ubo), Vk)->buffer,
			.range = sizeof(CBufferData)
		}
	};

	uboBufferInfo[1] = uboBufferInfo[0];
	uboBufferInfo[2] = uboBufferInfo[0];

	uboBufferInfo[1].offset = uboBufferInfo[0].range * 1;
	uboBufferInfo[2].offset = uboBufferInfo[0].range * 2;

	VkWriteDescriptorSet uboDescriptor[3] = {
		(VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = deviceExt->sets[EDescriptorType_CBuffer + 0],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &uboBufferInfo[0]
		}
	};

	uboDescriptor[1] = uboDescriptor[0];
	uboDescriptor[2] = uboDescriptor[0];

	uboDescriptor[1].pBufferInfo = &uboBufferInfo[1];
	uboDescriptor[1].dstSet = deviceExt->sets[EDescriptorType_CBuffer + 1];
	uboDescriptor[2].pBufferInfo = &uboBufferInfo[2];
	uboDescriptor[2].dstSet = deviceExt->sets[EDescriptorType_CBuffer + 2];

	vkUpdateDescriptorSets(deviceExt->device, 3, uboDescriptor, 0, NULL);

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

		for(U32 i = 0; i < EDescriptorType_Count; ++i) {

			VkDescriptorSetLayout layout = deviceExt->setLayouts[i];

			if(layout)
				vkDestroyDescriptorSetLayout(deviceExt->device, layout, NULL);
		}

		if(deviceExt->descriptorPool)
			vkDestroyDescriptorPool(deviceExt->device, deviceExt->descriptorPool, NULL);

		if(deviceExt->defaultLayout)
			vkDestroyPipelineLayout(deviceExt->device, deviceExt->defaultLayout, NULL);

		vkDestroyDevice(deviceExt->device, NULL);
	}

	DeviceBufferRef_dec(&deviceExt->ubo);
	DeviceBufferRef_dec(&deviceExt->staging);

	List_freex(&deviceExt->commandPools);
	List_freex(&deviceExt->submitSemaphores);

	for(U32 i = 0; i < EDescriptorType_Count; ++i)
		Buffer_freex(&deviceExt->freeList[i]);

	Lock_free(&deviceExt->descriptorLock);

	//Free temp storage

	List_freex(&deviceExt->waitStages);
	List_freex(&deviceExt->waitSemaphores);
	List_freex(&deviceExt->results);
	List_freex(&deviceExt->swapchainIndices);
	List_freex(&deviceExt->swapchainHandles);

	for(U64 i = 0; i < sizeof(deviceExt->stagingAllocations) / sizeof(deviceExt->stagingAllocations[0]); ++i)
		AllocationBuffer_freex(&deviceExt->stagingAllocations[i]);

	return true;
}

//Executing commands

Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef)
		return Error_nullPointer(0);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	Error err = vkCheck(vkDeviceWaitIdle(deviceExt->device));

	if(err.genericError)
		return err;

	for (U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++i) {

		//Release resources that were in flight.
		//This might cause resource deletions because we might be the last one releasing them.
		//For example temporary staging resources are released this way.

		List *inFlight = &device->resourcesInFlight[i];

		for (U64 j = 0; j < inFlight->length; ++j)
			RefPtr_dec((RefPtr**)inFlight->ptr + j);

		_gotoIfError(clean, List_clear(inFlight));

		//Release all allocations of buffer that was in flight

		if(!AllocationBuffer_freeAll(&deviceExt->stagingAllocations[i]))
			_gotoIfError(clean, Error_invalidState(0));
	}

clean:
	return err;
}

U32 VkGraphicsDevice_allocateDescriptor(VkGraphicsDevice *deviceExt, EDescriptorType type) {

	if(!Lock_isLockedForThread(deviceExt->descriptorLock))
		return U32_MAX;

	Buffer buf = deviceExt->freeList[type];

	for(U32 i = 0; i < (U32)(Buffer_length(buf) << 3); ++i) {

		Bool bit = false;

		if(Buffer_getBit(buf, i, &bit).genericError)
			return U32_MAX;

		if(!bit) {
			Buffer_setBit(buf, i);
			return i | (type << 20);
		}
	}

	return U32_MAX;
}

Bool VkGraphicsDevice_freeAllocations(VkGraphicsDevice *deviceExt, List *allocations) {

	if(!Lock_isLockedForThread(deviceExt->descriptorLock))
		return false;

	Bool success = true;

	for (U64 i = 0; i < allocations->length; ++i) {

		U32 id = ((const U32*)allocations->ptr)[i];

		if(id != U32_MAX)
			success &= !Buffer_resetBit(deviceExt->freeList[id >> 20], id & ((1 << 20) - 1)).genericError;
	}

	List_clear(allocations);
	return success;
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

Error DeviceBufferRef_flush(VkCommandBuffer commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

	DeviceBuffer *buffer = DeviceBufferRef_ptr(pending);
	VkDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Vk);

	Error err = Error_none();
	List tempList = (List) { 0 };

	Bool isInFlight = false;
	List *currentFlight = &device->resourcesInFlight[device->submitId % 3];
	DeviceBufferRef *tempStagingResource = NULL;
	List pendingCopies = List_createEmpty(sizeof(VkBufferCopy));

	for(U64 j = 0; j < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++j) {

		List inFlight = device->resourcesInFlight[j];

		for(U64 i = 0; i < inFlight.length; ++i)
			if (((RefPtr**)inFlight.ptr)[i] == pending) {
				isInFlight = true;
				break;
			}

		if(isInFlight)
			break;
	}

	if(!isInFlight && bufferExt->mappedMemory) {

		DeviceMemoryBlock block = *(DeviceMemoryBlock*) List_ptr(device->allocator.blocks, bufferExt->blockId);
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if(incoherent) {
			tempList = List_createEmpty(sizeof(VkMappedMemoryRange));
			_gotoIfError(clean, List_resizex(&tempList, buffer->pendingChanges.length));
		}

		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

			DevicePendingRange range = *(DevicePendingRange*) List_ptr(buffer->pendingChanges, j);

			U64 start = range.buffer.startRange;
			U64 len = range.buffer.endRange - range.buffer.startRange;

			Buffer dst = Buffer_createRef((U8*)bufferExt->mappedMemory + start, len);
			Buffer src = Buffer_createConstRef(buffer->cpuData.ptr + start, len);

			Buffer_copy(dst, src);

			if(incoherent)
				*(VkMappedMemoryRange*)List_ptr(tempList, j) = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = start + bufferExt->blockOffset,
					.size = len
				};
		}

		if(incoherent)
			_gotoIfError(clean, vkCheck(vkFlushMappedMemoryRanges(
				deviceExt->device, (U32) tempList.length, (const VkMappedMemoryRange*) tempList.ptr
			)));
	}

	else {

		//TODO: Copy queue

		if(!deviceExt->staging)
			_gotoIfError(clean, Error_invalidOperation(0));

		U64 allocRange = 0;

		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {
			BufferRange bufferj = ((DevicePendingRange*) List_ptr(buffer->pendingChanges, j))->buffer;
			allocRange += bufferj.endRange - bufferj.startRange;
		}

		_gotoIfError(clean, List_resizex(&pendingCopies, buffer->pendingChanges.length));

		VkDependencyInfo dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		List_freex(&tempList);
		tempList = List_createEmpty(sizeof(VkBufferMemoryBarrier2));
		_gotoIfError(clean, List_reservex(&tempList, 1 + buffer->pendingChanges.length));

		if (allocRange > 64 * MIBI) {		//Resource is too big, allocate dedicated staging resource

			_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
				deviceRef, EDeviceBufferUsage_CPUAllocatedBit, CharString_createConstRefCStr("Dedicated staging buffer"),
				allocRange, &tempStagingResource
			));

			DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
			VkDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Vk);
			U8 *location = stagingResourceExt->mappedMemory;

			DeviceMemoryBlock block = *(DeviceMemoryBlock*) List_ptr(device->allocator.blocks, stagingResourceExt->blockId);
			Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				BufferRange bufferj = ((DevicePendingRange*) List_ptr(buffer->pendingChanges, j))->buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len), 
					Buffer_createConstRef(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				_gotoIfError(clean, VkDeviceBuffer_transition(
					bufferExt, 
					VK_PIPELINE_STAGE_2_COPY_BIT, 
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					graphicsQueueId,
					bufferj.startRange,
					len,
					&tempList,
					&dependency
				));

				((VkBufferCopy*)pendingCopies.ptr)[j] = (VkBufferCopy) {
					.srcOffset = allocRange,
					.dstOffset = bufferj.startRange,
					.size = len
				};
				
				allocRange += len;
			}

			if(incoherent) {

				VkMappedMemoryRange memoryRange = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = stagingResourceExt->blockOffset,
					.size = allocRange
				};

				vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
			}

			_gotoIfError(clean, VkDeviceBuffer_transition(
				stagingResourceExt, 
				VK_PIPELINE_STAGE_2_COPY_BIT, 
				VK_ACCESS_2_TRANSFER_READ_BIT,
				graphicsQueueId,
				0,
				allocRange,
				&tempList,
				&dependency
			));

			if(dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

			vkCmdCopyBuffer(
				commandBuffer, 
				stagingResourceExt->buffer, 
				bufferExt->buffer, 
				(U32) pendingCopies.length, 
				(const VkBufferCopy*) pendingCopies.ptr
			);

			//When staging resource is commited to current in flight then we can relinguish ownership.

			_gotoIfError(clean, List_pushBackx(currentFlight, Buffer_createConstRef(&tempStagingResource, sizeof(RefPtr*))));
			tempStagingResource = NULL;
		}

		//Use staging buffer

		else {

			_gotoIfError(clean, List_resizex(&pendingCopies, buffer->pendingChanges.length));

			AllocationBuffer *stagingBuffer = &deviceExt->stagingAllocations[device->submitId % 3];
			VkDeviceBuffer *stagingExt = DeviceBuffer_ext(DeviceBufferRef_ptr(deviceExt->staging), Vk);

			U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
			Error temp = AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location);

			if(temp.genericError && location == defaultLocation)		//Something else went wrong
				_gotoIfError(clean, temp);

			//We flush the GPU to ensure we have space if necessary.

			if (temp.genericError) {

				//TODO: Create secondary staging buffer before doing this!
				//		Because this is super slow!

				Log_performanceLn(
					"Pushing too much data to the GPU this frame for the staging buffer to handle\n"
					"Flushing outstanding changes to GPU to make space for new allocations."
				);

				//TODO: Commit commandBuffer here

				GraphicsDeviceRef_wait(deviceRef);
				_gotoIfError(clean, AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, &location));
			}

			DeviceMemoryBlock block = *(DeviceMemoryBlock*) List_ptr(device->allocator.blocks, stagingExt->blockId);
			Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				BufferRange bufferj = ((DevicePendingRange*) List_ptr(buffer->pendingChanges, j))->buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len), 
					Buffer_createConstRef(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				((VkBufferCopy*)pendingCopies.ptr)[j] = (VkBufferCopy) {
					.srcOffset = allocRange + (location - block.mappedMemory),
					.dstOffset = bufferj.startRange,
					.size = len
				};

				_gotoIfError(clean, VkDeviceBuffer_transition(
					bufferExt, 
					VK_PIPELINE_STAGE_2_COPY_BIT, 
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					graphicsQueueId,
					bufferj.startRange,
					len,
					&tempList,
					&dependency
				));

				allocRange += len;
			}

			if (incoherent) {

				VkMappedMemoryRange memoryRange = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = location - block.mappedMemory,
					.size = allocRange
				};

				vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
			}

			if(dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

			vkCmdCopyBuffer(
				commandBuffer, 
				stagingExt->buffer,
				bufferExt->buffer, 
				(U32) buffer->pendingChanges.length, 
				(const VkBufferCopy*) pendingCopies.ptr
			);
		}
	}

	if(!(buffer->usage & EDeviceBufferUsage_CPUBacked))
		Buffer_freex(&buffer->cpuData);

	buffer->isFirstFrame = buffer->isPending = buffer->isPendingFullCopy = false;
	_gotoIfError(clean, List_clear(&buffer->pendingChanges));

	if(RefPtr_inc(pending))
		_gotoIfError(clean, List_pushBackx(currentFlight, Buffer_createConstRef(&pending, sizeof(pending))));

clean:
	DeviceBufferRef_dec(&tempStagingResource);
	List_freex(&tempList);
	List_freex(&pendingCopies);
	return err;
}

Error GraphicsDeviceRef_submitCommands(GraphicsDeviceRef *deviceRef, List commandLists, List swapchains, Buffer appData) {

	//Validation and ext structs

	if(!deviceRef || (!swapchains.length && !commandLists.length))
		return Error_nullPointer(!deviceRef ? 0 : 1);

	if(swapchains.length && swapchains.stride != sizeof(SwapchainRef*))
		return Error_invalidParameter(2, 0);

	if(swapchains.length >= 60)						//Hard limit of 60 swapchains
		return Error_invalidParameter(2, 1);

	if(commandLists.length && commandLists.stride != sizeof(CommandListRef*))
		return Error_invalidParameter(1, 0);

	if(Buffer_length(appData) >= 256 - 16 - 4 * swapchains.length)
		return Error_invalidParameter(3, 0);

	//Validate command lists

	for(U64 i = 0; i < commandLists.length; ++i) {

		CommandList *cmd = CommandListRef_ptr(((CommandListRef**) commandLists.ptr)[i]);

		if(!cmd)
			return Error_nullPointer(1);

		if(cmd->device != deviceRef)
			return Error_unsupportedOperation(0);
	}

	//Swapchains all need to have the same vsync option.

	for (U64 i = 0; i < swapchains.length; ++i) {

		SwapchainRef *swapchainRef = ((SwapchainRef**) swapchains.ptr)[i];

		for(U64 j = 0; j < i; ++j)
			if(swapchainRef == ((SwapchainRef**) swapchains.ptr)[j])
				return Error_invalidParameter(2, 2);

		Swapchain *swapchaini = SwapchainRef_ptr(swapchainRef);

		if(!swapchaini)
			return Error_nullPointer(2);

		if(swapchaini->device != deviceRef)
			return Error_unsupportedOperation(1);
	}

	//Unpack pointers

	Error err = Error_none();
	CharString temp = CharString_createNull();
	List barriers = (List) { 0 };

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

	//Prepare per frame cbuffer

	VkDeviceBuffer *uboExt = DeviceBuffer_ext(DeviceBufferRef_ptr(deviceExt->ubo), Vk);

	{
		CBufferData *data = (CBufferData*) uboExt->mappedMemory + (device->submitId % 3);
		Ns now = Time_now();

		*data = (CBufferData) {
			.frameId = (U32) device->submitId,
			.time = device->firstSubmit ? (F32)((F64)(now - device->firstSubmit) / SECOND) : 0,
			.deltaTime = device->firstSubmit ? (F32)((F64)(now - device->lastSubmit) / SECOND) : 0,
			.swapchainCount = (U32) swapchains.length
		};

		for(U32 i = 0; i < data->swapchainCount; ++i) {

			Swapchain *swapchain = SwapchainRef_ptr(((SwapchainRef**) swapchains.ptr)[i]);
			VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

			data->appData[i] = ((const U32*)swapchainExt->descriptorAllocations.ptr)[swapchainExt->currentIndex * 2 + 0];
		}

		Buffer_copy(
			Buffer_createRef((U8*)data + 16 + 4 * swapchains.length, sizeof(*data)), 
			appData		//appData is already range checked (no out of bounds write)
		);

		DeviceMemoryBlock block = *(DeviceMemoryBlock*) List_ptr(device->allocator.blocks, uboExt->blockId);
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (incoherent) {

			VkMappedMemoryRange range = (VkMappedMemoryRange) {
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory) block.ext,
				.offset = uboExt->blockOffset + (device->submitId % 3) * sizeof(CBufferData),
				.size = sizeof(CBufferData)
			};

			_gotoIfError(clean, vkCheck(vkFlushMappedMemoryRanges(deviceExt->device, 1, &range)));
		}
	}

	//Record command list

	VkCommandBuffer commandBuffer = NULL;

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	U32 graphicsQueueId = queue.queueId;

	List *currentFlight = &device->resourcesInFlight[device->submitId % 3];
	
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

		//Ensure ubo and staging buffer are the correct states

		VkDependencyInfo dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		List_freex(&barriers);
		barriers = List_createEmpty(sizeof(VkBufferMemoryBarrier2));
		_gotoIfError(clean, List_reservex(&barriers, 2));

		DeviceBuffer *staging = DeviceBufferRef_ptr(deviceExt->staging);
		VkDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Vk);

		_gotoIfError(clean, VkDeviceBuffer_transition(
			stagingExt, 
			VK_PIPELINE_STAGE_2_COPY_BIT, 
			VK_ACCESS_2_TRANSFER_READ_BIT,
			graphicsQueueId,
			(device->submitId % 3) * (staging->length / 3),
			staging->length / 3,
			&barriers,
			&dependency
		));

		_gotoIfError(clean, VkDeviceBuffer_transition(
			uboExt, 
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, 
			VK_ACCESS_2_UNIFORM_READ_BIT,
			graphicsQueueId,
			(device->submitId % 3) * sizeof(CBufferData),
			sizeof(CBufferData),
			&barriers,
			&dependency
		));

		if(dependency.bufferMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		//Bind pipeline layout and descriptors since they stay the same for the entire frame.
		//For every bind point

		VkDescriptorSet sets[EDescriptorType_Count];

		for(U32 i = 0; i < EDescriptorType_Count; ++i)
			sets[i] = 
				i != EDescriptorType_CBuffer ? deviceExt->sets[i] :
				deviceExt->sets[EDescriptorType_CBuffer + (device->submitId % 3)];

		for(U64 i = 0; i < 2; ++i)
			vkCmdBindDescriptorSets(
				commandBuffer, 
				i == 0 ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, 
				deviceExt->defaultLayout, 
				0, EDescriptorType_Count, sets, 
				0, NULL
			);

		//Release resources that were in flight.
		//This might cause resource deletions because we might be the last one releasing them.
		//For example temporary staging resources are released this way.

		List *inFlight = &device->resourcesInFlight[device->submitId % 3];

		for (U64 i = 0; i < inFlight->length; ++i)
			RefPtr_dec((RefPtr**)inFlight->ptr + i);

		_gotoIfError(clean, List_clear(inFlight));

		//Release all allocations of buffer that was in flight

		if(!AllocationBuffer_freeAll(&deviceExt->stagingAllocations[device->submitId % 3]))
			_gotoIfError(clean, Error_invalidState(0));

		//Update buffer data

		for(U64 i = 0; i < device->pendingResources.length; ++i) {

			RefPtr *pending = *(RefPtr**) List_ptr(device->pendingResources, i);

			EGraphicsTypeId type = (EGraphicsTypeId) pending->typeId;

			switch(type) {

				case EGraphicsTypeId_DeviceBuffer: 
					_gotoIfError(clean, DeviceBufferRef_flush(commandBuffer, deviceRef, pending));
					break;

				default:
					_gotoIfError(clean, Error_unsupportedOperation(5));
			}
		}

		_gotoIfError(clean, List_clear(&device->pendingResources));

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

		dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		List_freex(&barriers);
		barriers = List_createEmpty(sizeof(VkImageMemoryBarrier2));;
		_gotoIfError(clean, List_reservex(&barriers, swapchains.length));

		for (U64 i = 0; i < swapchains.length; ++i) {

			SwapchainRef *swapchainRef = ((SwapchainRef**) swapchains.ptr)[i];
			Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);
			VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

			VkManagedImage *imageExt = &((VkManagedImage*)swapchainExt->images.ptr)[swapchainExt->currentIndex];

			VkImageSubresourceRange range = (VkImageSubresourceRange) {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			};

			_gotoIfError(clean, VkSwapchain_transition(
				imageExt,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 
				0, 
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				graphicsQueueId,
				&range,
				&barriers,
				&dependency
			));

			if(RefPtr_inc(swapchainRef))
				_gotoIfError(clean, List_pushBackx(currentFlight, Buffer_createConstRef(&swapchainRef, sizeof(swapchainRef))));
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

	//Add resources from command lists to resources in flight

	for (U64 j = 0; j < commandLists.length; ++j) {

		CommandListRef *cmdRef = ((CommandListRef**)commandLists.ptr)[j];
		CommandList *cmd = CommandListRef_ptr(cmdRef);

		for(U64 i = 0; i < cmd->resources.length; ++i) {

			RefPtr *ptr = ((RefPtr**) cmd->resources.ptr)[i];
			Buffer bufi = Buffer_createConstRef(&ptr, sizeof(ptr));

			if(List_contains(*currentFlight, bufi, 0))
				continue;

			if(RefPtr_inc(ptr))
				_gotoIfError(clean, List_pushBackx(currentFlight, bufi));
		}
	}

	//Ensure our next fence value is used

	++device->submitId;
	device->lastSubmit = Time_now();

	if(!device->firstSubmit)
		device->firstSubmit = device->lastSubmit;

clean: 
	List_freex(&barriers);
	CharString_freex(&temp);
	return err;
}
