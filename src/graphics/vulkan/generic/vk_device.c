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

#include "platforms/ext/listx_impl.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/thread.h"
#include "types/time.h"

TListImpl(VkCommandAllocator);
TListImpl(VkSemaphore);
TListImpl(VkResult);
TListImpl(VkSwapchainKHR);
TListImpl(VkPipelineStageFlags);
TListImpl(VkWriteDescriptorSet);

#define vkBindNext(T, condition, ...)	\
	T tmp##T = __VA_ARGS__;				\
										\
	if(condition) {						\
		*currPNext = &tmp##T;			\
		currPNext = &tmp##T.pNext;		\
	}

const U64 GraphicsDeviceExt_size = sizeof(VkGraphicsDevice);

//Convert command into API dependent instructions
impl void CommandList_process(
	CommandList *commandList,
	GraphicsDevice *device,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
);

TList(VkDeviceQueueCreateInfo);
TList(VkQueueFamilyProperties);
TListImpl(VkDeviceQueueCreateInfo);
TListImpl(VkQueueFamilyProperties);

Error GraphicsDevice_initExt(
	const GraphicsInstance *instance,
	const GraphicsDeviceInfo *physicalDevice,
	Bool verbose,
	GraphicsDeviceRef **deviceRef
) {

	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);
	(void)instanceExt;

	EGraphicsFeatures feat = physicalDevice->capabilities.features;
	EGraphicsFeatures featEx = physicalDevice->capabilities.featuresExt;
	EGraphicsDataTypes types = physicalDevice->capabilities.dataTypes;

	VkPhysicalDeviceFeatures features = (VkPhysicalDeviceFeatures) {

		.fullDrawIndexUint32 = true,
		.imageCubeArray = true,
		.independentBlend = true,

		.geometryShader = (Bool)(feat & EGraphicsFeatures_GeometryShader),
		.tessellationShader = true,

		.multiDrawIndirect = true,
		.sampleRateShading = true,

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

	ListConstC8 extensions = (ListConstC8) { 0 };
	ListVkDeviceQueueCreateInfo queues = (ListVkDeviceQueueCreateInfo) { 0 };
	ListVkQueueFamilyProperties queueFamilies = (ListVkQueueFamilyProperties) { 0 };
	CharString tempStr = CharString_createNull();

	Error err = Error_none();
	_gotoIfError(clean, ListConstC8_reservex(&extensions, 32));
	_gotoIfError(clean, ListVkDeviceQueueCreateInfo_reservex(&queues, EVkCommandQueue_Count));

	for(U64 i = 0; i < reqExtensionsNameCount; ++i)
		_gotoIfError(clean, ListConstC8_pushBackx(&extensions, reqExtensionsName[i]));

	for (U64 i = 0; i < optExtensionsNameCount; ++i) {

		const C8 *ptr = optExtensionsName[i];
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
			_gotoIfError(clean, ListConstC8_pushBackx(&extensions, ptr));
	}

	VkPhysicalDevice physicalDeviceExt = (VkPhysicalDevice) physicalDevice->ext;

	//Get queues

	U32 familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceExt, &familyCount, NULL);

	if(!familyCount)
		_gotoIfError(clean, Error_invalidOperation(0, "GraphicsDevice_initExt() no supported queues"));

	_gotoIfError(clean, ListVkQueueFamilyProperties_resizex(&queueFamilies, familyCount));
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceExt, &familyCount, queueFamilies.ptrNonConst);

	//Assign queues to deviceExt (don't have to be unique)

	//Find queues

	U32 copyQueueId = U32_MAX;
	U32 computeQueueId = U32_MAX;
	U32 graphicsQueueId = U32_MAX;

	U32 fallbackCopyQueueId = U32_MAX;
	U32 fallbackComputeQueueId = U32_MAX;

	VkQueueFlags importantFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

	for (U32 i = 0; i < familyCount; ++i) {

		VkQueueFamilyProperties q = queueFamilies.ptr[i];

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
		_gotoIfError(clean, Error_invalidOperation(1, "GraphicsDevice_initExt() doesn't have copy, comp or gfx queue"));

	//Assign queues to queues (deviceInfo)

	F32 prio = 1;

	VkDeviceQueueCreateInfo graphicsQueue = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = graphicsQueueId,
		.queueCount = 1,
		.pQueuePriorities = &prio
	};

	_gotoIfError(clean, ListVkDeviceQueueCreateInfo_pushBackx(&queues, graphicsQueue));

	VkDeviceQueueCreateInfo copyQueue = graphicsQueue;
	copyQueue.queueFamilyIndex = copyQueueId;

	if(copyQueueId != graphicsQueueId)
		_gotoIfError(clean, ListVkDeviceQueueCreateInfo_pushBackx(&queues, copyQueue));

	VkDeviceQueueCreateInfo computeQueue = graphicsQueue;
	computeQueue.queueFamilyIndex = computeQueueId;

	if(computeQueueId != graphicsQueueId && computeQueueId != copyQueueId)
		_gotoIfError(clean, ListVkDeviceQueueCreateInfo_pushBackx(&queues, computeQueue));

	//Create device

	VkDeviceCreateInfo deviceInfo = (VkDeviceCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.enabledExtensionCount = (U32) extensions.length,
		.ppEnabledExtensionNames = extensions.ptr,
		.queueCreateInfoCount = (U32) queues.length,
		.pQueueCreateInfos = queues.ptr
	};

	if (verbose) {

		Log_debugLnx("Enabling extensions:");

		for(U32 i = 0; i < (U32) extensions.length; ++i)
			Log_debugLnx("\t%s", extensions.ptr[i]);
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
	//We only allow triple buffering, so allocate for triple buffers.
	//These will be initialized JIT because we don't know what thread will be accessing them.

	U64 threads = Thread_getLogicalCores();

	_gotoIfError(clean, ListVkCommandAllocator_resizex(&deviceExt->commandPools, 3 * threads * resolvedId));

	//Semaphores

	_gotoIfError(clean, ListVkSemaphore_resizex(&deviceExt->submitSemaphores, 3));

	for (U64 k = 0; k < 3; ++k) {

		VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkSemaphore *semaphore = deviceExt->submitSemaphores.ptrNonConst + k;

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

	for (U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i) {

		VkDescriptorSetLayoutBinding bindings[EDescriptorType_ResourceCount - 1];
		U8 bindingCount = 0;

		if (i == EDescriptorSetType_Resources) {

			for(U32 j = EDescriptorType_Texture2D; j < EDescriptorType_ResourceCount; ++j)
				bindings[j - 1] = (VkDescriptorSetLayoutBinding) {
					.binding = j - 1,
					.stageFlags = VK_SHADER_STAGE_ALL,
					.descriptorCount = descriptorTypeCount[j],
					.descriptorType =
						j < EDescriptorType_Buffer ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : (
							j <= EDescriptorType_RWBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER :
							VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
						)
				};

			bindingCount = EDescriptorType_ResourceCount - 1;
		}

		else {

			Bool isSampler = i == EDescriptorSetType_Sampler;

			bindings[0] = (VkDescriptorSetLayoutBinding) {
				.stageFlags = VK_SHADER_STAGE_ALL,
				.descriptorCount = isSampler ? EDescriptorTypeCount_Sampler : 1,
				.descriptorType = isSampler ? VK_DESCRIPTOR_TYPE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			};

			bindingCount = 1;
		}

		//One binding per set.

		VkDescriptorBindingFlags flags[EDescriptorType_ResourceCount - 1] = {
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
		};

		for(U32 j = 1; j < EDescriptorType_ResourceCount - 1; ++j)
			flags[j] = flags[0];

		if (i >= EDescriptorSetType_CBuffer0 && i <= EDescriptorSetType_CBuffer2)	//We don't touch CBuffer after bind
			flags[0] &=~ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo partiallyBound = (VkDescriptorSetLayoutBindingFlagsCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = bindingCount,
			.pBindingFlags = flags
		};

		VkDescriptorSetLayoutCreateInfo setInfo = (VkDescriptorSetLayoutCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &partiallyBound,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
			.bindingCount = bindingCount,
			.pBindings = bindings
		};

		_gotoIfError(clean, vkCheck(vkCreateDescriptorSetLayout(
			deviceExt->device, &setInfo, NULL, &deviceExt->setLayouts[i]
		)));
	}

	VkPipelineLayoutCreateInfo layoutInfo = (VkPipelineLayoutCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = EDescriptorSetType_UniqueLayouts,
		.pSetLayouts = deviceExt->setLayouts
	};

	_gotoIfError(clean, vkCheck(vkCreatePipelineLayout(deviceExt->device, &layoutInfo, NULL, &deviceExt->defaultLayout)));

	//We only need one pool and 1 descriptor set per EDescriptorType since we use bindless.
	//Every resource is automatically allocated into their respective descriptor set.
	//Last descriptor set (cbuffer) is triple buffered to allow swapping part of the UBO

	VkDescriptorPoolSize poolSizes[] = {
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_SAMPLER, EDescriptorTypeCount_Sampler },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, EDescriptorTypeCount_RWTextures },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, EDescriptorTypeCount_Textures },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, EDescriptorTypeCount_SSBO },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }
	};

	VkDescriptorPoolCreateInfo poolInfo = (VkDescriptorPoolCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = EDescriptorSetType_Count,
		.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]),
		.pPoolSizes = poolSizes
	};

	_gotoIfError(clean, vkCheck(vkCreateDescriptorPool(deviceExt->device, &poolInfo, NULL, &deviceExt->descriptorPool)));

	//Last layout repeat 3x (that's the CBuffer which needs 3 different versions)

	VkDescriptorSetLayout setLayouts[EDescriptorSetType_Count];

	for(U64 i = 0; i < EDescriptorSetType_Count; ++i)
		setLayouts[i] = deviceExt->setLayouts[U64_min(i, EDescriptorSetType_UniqueLayouts - 1)];

	VkDescriptorSetAllocateInfo setInfo = (VkDescriptorSetAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = deviceExt->descriptorPool,
		.descriptorSetCount = EDescriptorSetType_Count,
		.pSetLayouts = setLayouts
	};

	_gotoIfError(clean, vkCheck(vkAllocateDescriptorSets(deviceExt->device, &setInfo, deviceExt->sets)));

	#ifndef NDEBUG

		static const C8 *debugNames[] = {
			"Samplers",
			"Resources",
			"Global frame cbuffer (0)",
			"Global frame cbuffer (1)",
			"Global frame cbuffer (2)"
		};

		if(instanceExt->debugSetName)
			for (U32 i = 0; i < EDescriptorSetType_Count; ++i) {

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

				if(i < EDescriptorSetType_UniqueLayouts) {

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

	//Get memory properties

	vkGetPhysicalDeviceMemoryProperties((VkPhysicalDevice) physicalDevice->ext, &deviceExt->memoryProperties);

	//Determine when we need to flush.
	//As a rule of thumb I decided for 20% occupied mem by just copies.
	//Or if there's distinct shared mem available too it can allocate 10% more in that memory too
	// (as long as it doesn't exceed 33%).
	//Flush threshold is kept under 4 GiB to avoid TDRs because even if the mem is available it might be slow.

	U64 cpuHeapSize = 0;
	_gotoIfError(clean, VkDeviceMemoryAllocator_findMemory(deviceExt, true, U32_MAX, NULL, NULL, &cpuHeapSize));

	U64 gpuHeapSize = 0;
	_gotoIfError(clean, VkDeviceMemoryAllocator_findMemory(deviceExt, false, U32_MAX, NULL, NULL, &gpuHeapSize));

	Bool isDistinct = gpuHeapSize >> 63;
	gpuHeapSize &= (U64)I64_MAX;
	cpuHeapSize &= (U64)I64_MAX;

	device->flushThreshold = U64_min(
		4 * GIBI,
		isDistinct ? U64_min(gpuHeapSize / 3, cpuHeapSize / 10 + gpuHeapSize / 5) :
		cpuHeapSize / 5
	);

	//Allocate temp storage for transitions

	_gotoIfError(clean, ListVkBufferMemoryBarrier2_reservex(&deviceExt->bufferTransitions, 17));
	_gotoIfError(clean, ListVkImageMemoryBarrier2_reservex(&deviceExt->imageTransitions, 16));
	_gotoIfError(clean, ListVkImageCopy_reservex(&deviceExt->imageCopyRanges, 8));

clean:

	if(err.genericError)
		GraphicsDeviceRef_dec(deviceRef);

	CharString_freex(&tempStr);
	ListConstC8_freex(&extensions);
	ListVkDeviceQueueCreateInfo_freex(&queues);
	ListVkQueueFamilyProperties_freex(&queueFamilies);
	return err;
}

void GraphicsDevice_postInit(GraphicsDevice *device) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	//Fill last 3 descriptor sets with UBO[i] to ensure we only modify things in flight.

	VkDescriptorBufferInfo uboBufferInfo[3] = {
		(VkDescriptorBufferInfo) {
			.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(device->frameData), Vk)->buffer,
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
			.dstSet = deviceExt->sets[EDescriptorSetType_CBuffer0],
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
	uboDescriptor[1].dstSet = deviceExt->sets[EDescriptorSetType_CBuffer1];
	uboDescriptor[2].pBufferInfo = &uboBufferInfo[2];
	uboDescriptor[2].dstSet = deviceExt->sets[EDescriptorSetType_CBuffer2];

	vkUpdateDescriptorSets(deviceExt->device, 3, uboDescriptor, 0, NULL);
}

Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext) {

	if(!instance || !ext)
		return instance;

	VkGraphicsDevice *deviceExt = (VkGraphicsDevice*)ext;

	if(deviceExt->device) {

		for(U64 i = 0; i < deviceExt->commandPools.length; ++i) {

			VkCommandAllocator alloc = deviceExt->commandPools.ptr[i];

			if(alloc.cmd)
				vkFreeCommandBuffers(deviceExt->device, alloc.pool, 1, &alloc.cmd);

			if(alloc.pool)
				vkDestroyCommandPool(deviceExt->device, alloc.pool, NULL);
		}

		for(U64 i = 0; i < deviceExt->submitSemaphores.length; ++i) {

			VkSemaphore semaphore = deviceExt->submitSemaphores.ptr[i];

			if(semaphore)
				vkDestroySemaphore(deviceExt->device, semaphore, NULL);
		}

		if(deviceExt->commitSemaphore) {
			vkDestroySemaphore(deviceExt->device, deviceExt->commitSemaphore, NULL);
			deviceExt->commitSemaphore = NULL;
		}

		for(U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i) {

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

	ListVkCommandAllocator_freex(&deviceExt->commandPools);
	ListVkSemaphore_freex(&deviceExt->submitSemaphores);

	//Free temp storage

	ListVkPipelineStageFlags_freex(&deviceExt->waitStages);
	ListVkSemaphore_freex(&deviceExt->waitSemaphores);
	ListVkResult_freex(&deviceExt->results);
	ListU32_freex(&deviceExt->swapchainIndices);
	ListVkSwapchainKHR_freex(&deviceExt->swapchainHandles);
	ListVkBufferMemoryBarrier2_freex(&deviceExt->bufferTransitions);
	ListVkImageMemoryBarrier2_freex(&deviceExt->imageTransitions);
	ListVkImageCopy_freex(&deviceExt->imageCopyRanges);

	return true;
}

//Executing commands

Error GraphicsDeviceRef_waitExt(GraphicsDeviceRef *deviceRef) {
	return vkCheck(vkDeviceWaitIdle(GraphicsDevice_ext(GraphicsDeviceRef_ptr(deviceRef), Vk)->device));
}

VkCommandAllocator *VkGraphicsDevice_getCommandAllocator(
	VkGraphicsDevice *device, U32 resolvedQueueId, U64 threadId, U8 backBufferId
) {

	U64 threadCount = Thread_getLogicalCores();

	if(!device || resolvedQueueId >= device->resolvedQueues || threadId >= threadCount || backBufferId >= 3)
		return NULL;

	U64 id = resolvedQueueId + (backBufferId * threadCount + threadId) * device->resolvedQueues;

	return device->commandPools.ptrNonConst + id;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Error GraphicsDevice_submitCommandsImpl(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains
) {
	
	//Unpack ext

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	CharString temp = CharString_createNull();
	Error err = Error_none();

	//Reserve temp storage

	_gotoIfError(clean, ListVkSwapchainKHR_clear(&deviceExt->swapchainHandles));
	_gotoIfError(clean, ListVkSwapchainKHR_reservex(&deviceExt->swapchainHandles, swapchains.length));

	_gotoIfError(clean, ListU32_clear(&deviceExt->swapchainIndices));
	_gotoIfError(clean, ListU32_reservex(&deviceExt->swapchainIndices, swapchains.length));

	_gotoIfError(clean, ListVkResult_clear(&deviceExt->results));
	_gotoIfError(clean, ListVkResult_reservex(&deviceExt->results, swapchains.length));

	_gotoIfError(clean, ListVkSemaphore_clear(&deviceExt->waitSemaphores));
	_gotoIfError(clean, ListVkSemaphore_reservex(&deviceExt->waitSemaphores, swapchains.length + 1));

	_gotoIfError(clean, ListVkPipelineStageFlags_clear(&deviceExt->waitStages));
	_gotoIfError(clean, ListVkPipelineStageFlags_reservex(&deviceExt->waitStages, swapchains.length + 1));

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

	//Acquire swapchain images

	for(U64 i = 0; i < swapchains.length; ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(swapchains.ptr[i]);
		VkSwapchain *swapchainExt = TextureRef_getImplExtT(VkSwapchain, swapchains.ptr[i]);

		VkSemaphore semaphore = swapchainExt->semaphores.ptr[device->submitId % swapchainExt->semaphores.length];

		UnifiedTexture *unifiedTexture = TextureRef_getUnifiedTextureIntern(swapchains.ptr[i], NULL);
		U32 currImg = 0;

		_gotoIfError(clean, vkCheck(instanceExt->acquireNextImage(
			deviceExt->device,
			swapchainExt->swapchain,
			U64_MAX,
			semaphore,
			VK_NULL_HANDLE,
			&currImg
		)));

		unifiedTexture->currentImageId = (U8) currImg;

		deviceExt->swapchainHandles.ptrNonConst[i] = swapchainExt->swapchain;
		deviceExt->swapchainIndices.ptrNonConst[i] = unifiedTexture->currentImageId;

		VkPipelineStageFlagBits pipelineStage =
			swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT :
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		_gotoIfError(clean, ListVkSemaphore_pushBackx(&deviceExt->waitSemaphores, semaphore));
		_gotoIfError(clean, ListVkPipelineStageFlags_pushBackx(&deviceExt->waitStages, pipelineStage));
	}

	//Prepare per frame cbuffer

	{
		DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData);
		CBufferData *data = (CBufferData*) frameData->resource.mappedMemoryExt + (device->submitId % 3);

		for(U32 i = 0; i < data->swapchainCount; ++i) {

			SwapchainRef *swapchainRef = swapchains.ptr[i];
			Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);

			Bool allowCompute = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

			UnifiedTextureImage managedImage = TextureRef_getCurrImage(swapchainRef, 0);

			data->swapchains[i * 2 + 0] = managedImage.readHandle;

			if(allowCompute)
				data->swapchains[i * 2 + 1] = managedImage.writeHandle;
		}

		DeviceMemoryBlock block = device->allocator.blocks.ptr[frameData->resource.blockId];
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (incoherent) {

			VkMappedMemoryRange range = (VkMappedMemoryRange) {
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory) block.ext,
				.offset = frameData->resource.blockOffset + (device->submitId % 3) * sizeof(CBufferData),
				.size = sizeof(CBufferData)
			};

			_gotoIfError(clean, vkCheck(vkFlushMappedMemoryRanges(deviceExt->device, 1, &range)));
		}
	}

	//Record command list

	VkCommandBuffer commandBuffer = NULL;

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	U32 graphicsQueueId = queue.queueId;

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];
	
	if (commandLists.length) {

		U32 threadId = 0;

		VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, (U8)(device->submitId % 3)
		);

		if(!allocator)
			_gotoIfError(clean, Error_nullPointer(0, "GraphicsDevice_submitCommandsImpl() command allocator is NULL"));

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

		VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		};

		_gotoIfError(clean, vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo)));

		//Start copies

		_gotoIfError(clean, GraphicsDeviceRef_handleNextFrame(deviceRef, commandBuffer));

		VkCommandBufferState state = (VkCommandBufferState) { .buffer = commandBuffer };

		//Ensure ubo and staging buffer are the correct states

		VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

		VkDeviceBuffer *uboExt = DeviceBuffer_ext(DeviceBufferRef_ptr(device->frameData), Vk);

		_gotoIfError(clean, VkDeviceBuffer_transition(
			uboExt,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_UNIFORM_READ_BIT,
			graphicsQueueId,
			(device->submitId % 3) * sizeof(CBufferData),
			sizeof(CBufferData),
			&deviceExt->bufferTransitions,
			&dependency
		));

		if(dependency.bufferMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);

		//Bind pipeline layout and descriptors since they stay the same for the entire frame.
		//For every bind point

		VkDescriptorSet sets[EDescriptorSetType_UniqueLayouts];

		for(U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i)
			sets[i] =
				i != EDescriptorSetType_CBuffer0 ? deviceExt->sets[i] :
				deviceExt->sets[EDescriptorSetType_CBuffer0 + (device->submitId % 3)];

		for(U64 i = 0; i < 2; ++i)
			vkCmdBindDescriptorSets(
				commandBuffer,
				i == 0 ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
				deviceExt->defaultLayout,
				0, EDescriptorSetType_UniqueLayouts, sets,
				0, NULL
			);

		//Record commands

		for (U64 i = 0; i < commandLists.length; ++i) {

			state.scopeCounter = 0;
			CommandList *commandList = CommandListRef_ptr(commandLists.ptr[i]);
			const U8 *ptr = commandList->data.ptr;

			for (U64 j = 0; j < commandList->commandOps.length; ++j) {

				CommandOpInfo info = commandList->commandOps.ptr[j];
				
				//Extra debugging if an error happens while processing the command

				CommandList_process(commandList, device, info.op, ptr, &state);
				ptr += info.opSize;
			}
		}

		//Transition back swapchains to present

		//Combine transitions into one call.

		dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		for (U64 i = 0; i < swapchains.length; ++i) {

			SwapchainRef *swapchainRef = swapchains.ptr[i];
			VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(swapchainRef, Vk, 0);

			VkImageSubresourceRange range = (VkImageSubresourceRange) {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			};

			_gotoIfError(clean, VkUnifiedTexture_transition(
				imageExt,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
				0,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				graphicsQueueId,
				&range,
				&deviceExt->imageTransitions,
				&dependency
			));

			if(RefPtr_inc(swapchainRef))
				_gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, swapchainRef));
		}

		if(dependency.imageMemoryBarrierCount)
			instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);

		//End buffer

		_gotoIfError(clean, vkCheck(vkEndCommandBuffer(commandBuffer)));
	}

	//Submit queue
	//TODO: Multiple queues

	U64 waitValue = device->submitId - 3 + 1;

	VkSemaphore signalSemaphores[2] = {
		deviceExt->commitSemaphore,
		deviceExt->submitSemaphores.ptr[device->submitId % 3]
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
		.pWaitSemaphores = deviceExt->waitSemaphores.ptr,
		.signalSemaphoreCount = swapchains.length ? 2 : 1,
		.pSignalSemaphores = signalSemaphores,
		.pCommandBuffers = &commandBuffer,
		.commandBufferCount = commandBuffer ? 1 : 0,
		.pWaitDstStageMask = deviceExt->waitStages.ptr
	};

	_gotoIfError(clean, vkCheck(vkQueueSubmit(queue.queue, 1, &submitInfo, VK_NULL_HANDLE)));

	//Presents

	if(swapchains.length) {

		VkPresentInfoKHR presentInfo = (VkPresentInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = deviceExt->submitSemaphores.ptr + device->submitId % 3,
			.swapchainCount = (U32) swapchains.length,
			.pSwapchains = deviceExt->swapchainHandles.ptr,
			.pImageIndices = deviceExt->swapchainIndices.ptr,
			.pResults = deviceExt->results.ptrNonConst
		};

		_gotoIfError(clean, vkCheck(vkQueuePresentKHR(queue.queue, &presentInfo)));

		for(U64 i = 0; i < deviceExt->results.length; ++i)
			_gotoIfError(clean, vkCheck(deviceExt->results.ptr[i]));
	}

clean:

	ListVkImageCopy_clear(&deviceExt->imageCopyRanges);
	ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);
	ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);
	CharString_freex(&temp);

	return err;
}

Error VkGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, VkCommandBuffer commandBuffer) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	//End current command list

	Error err = Error_none();
	_gotoIfError(clean, vkCheck(vkEndCommandBuffer(commandBuffer)));

	//Submit only the copy command list

	U64 waitValue = device->submitId - 1;

	VkTimelineSemaphoreSubmitInfo timelineInfo = (VkTimelineSemaphoreSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
		.waitSemaphoreValueCount = device->submitId > 0,
		.pWaitSemaphoreValues = device->submitId > 0 ? &waitValue : NULL,
	};

	VkSubmitInfo submitInfo = (VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = &timelineInfo,
		.waitSemaphoreCount = timelineInfo.waitSemaphoreValueCount,
		.pWaitSemaphores = (VkSemaphore*) &deviceExt->commitSemaphore,
		.pCommandBuffers = &commandBuffer,
		.commandBufferCount = 1
	};

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	_gotoIfError(clean, vkCheck(vkQueueSubmit(queue.queue, 1, &submitInfo, VK_NULL_HANDLE)));

	//Wait for the device

	_gotoIfError(clean, GraphicsDeviceRef_wait(deviceRef));

	//Reset command list

	U32 threadId = 0;

	VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
		deviceExt, queue.resolvedQueueId, threadId, (U8)(device->submitId % 3)
	);

	_gotoIfError(clean, vkCheck(vkResetCommandPool(
		deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
	)));

	//Re-open

	VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	_gotoIfError(clean, vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo)));

clean:
	return err;
}
