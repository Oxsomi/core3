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
#include "types/math/math.h"
#include "types/base/thread.h"

TListImpl(VkCommandAllocator);
TListImpl(VkSemaphore);
TListImpl(VkResult);
TListImpl(VkSwapchainKHR);
TListImpl(VkPipelineStageFlags);
TListImpl(VkWriteDescriptorSet);

#define bindNextVkStruct(T, condition, ...)	\
	T tmp##T = __VA_ARGS__;				\
										\
	if(condition) {						\
		*currPNext = &tmp##T;			\
		currPNext = &tmp##T.pNext;		\
	}

TList(VkDeviceQueueCreateInfo);
TList(VkQueueFamilyProperties);
TListImpl(VkDeviceQueueCreateInfo);
TListImpl(VkQueueFamilyProperties);

#define getVkFunctionDevice(label, function, result) {											\
																								\
	PFN_vkVoidFunction v = vkGetDeviceProcAddr(deviceExt->device, #function); 					\
																								\
	if(!v)																						\
		gotoIfError(clean, Error_nullPointer(0, "getVkFunction() " #function " failed"))		\
																								\
	*(void**)&result = (void*) v;																\
}

Error VK_WRAP_FUNC(GraphicsDevice_init)(
	const GraphicsInstance *instance,
	const GraphicsDeviceInfo *physicalDevice,
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

		.geometryShader = !!(feat & EGraphicsFeatures_GeometryShader),
		.tessellationShader = true,

		.multiDrawIndirect = true,
		.sampleRateShading = true,

		.drawIndirectFirstInstance = true,
		.depthClamp = true,
		.depthBiasClamp = true,
		.samplerAnisotropy = true,

		.textureCompressionASTC_LDR = !!(types & EGraphicsDataTypes_ASTC),
		.textureCompressionBC = !!(types & EGraphicsDataTypes_BCn),

		.shaderUniformBufferArrayDynamicIndexing = true,
		.shaderSampledImageArrayDynamicIndexing = true,
		.shaderStorageBufferArrayDynamicIndexing = true,
		.shaderStorageImageArrayDynamicIndexing = true,

		.shaderFloat64 = !!(types & EGraphicsDataTypes_F64),
		.shaderInt64 = !!(types & EGraphicsDataTypes_I64),
		.shaderInt16 = true,

		.fillModeNonSolid = !!(feat & EGraphicsFeatures_Wireframe),
		.logicOp = !!(feat & EGraphicsFeatures_LogicOp),
		.dualSrcBlend = !!(feat & EGraphicsFeatures_DualSrcBlend),
		.shaderStorageImageMultisample = !!(feat & EGraphicsFeatures_WriteMSTexture)
	};

	VkPhysicalDeviceFeatures2 features2 = (VkPhysicalDeviceFeatures2) {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.features = features
	};

	void **currPNext = &features2.pNext;

	bindNextVkStruct(
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
	)

	bindNextVkStruct(
		VkPhysicalDevicePerformanceQueryFeaturesKHR,
		physicalDevice->capabilities.featuresExt & EVkGraphicsFeatures_PerfQuery,
		(VkPhysicalDevicePerformanceQueryFeaturesKHR) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR,
			.performanceCounterQueryPools = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR,
		physicalDevice->capabilities.featuresExt & EVkGraphicsFeatures_BufferDeviceAddress,
		(VkPhysicalDeviceBufferDeviceAddressFeaturesKHR) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
			.bufferDeviceAddress = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceSynchronization2Features,
		true,
		(VkPhysicalDeviceSynchronization2Features) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
			.synchronization2 = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceDynamicRenderingFeatures,
		feat & EGraphicsFeatures_DirectRendering,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.dynamicRendering = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceMeshShaderFeaturesEXT,
		feat & EGraphicsFeatures_MeshShader,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
			.meshShader = true,
			.taskShader = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceMultiviewFeatures,
		feat & EGraphicsFeatures_Multiview,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
			.multiview = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceFragmentShadingRateFeaturesKHR,
		feat & EGraphicsFeatures_VariableRateShading,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
			.pipelineFragmentShadingRate = true,
			.attachmentFragmentShadingRate = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceAccelerationStructureFeaturesKHR,
		feat & EGraphicsFeatures_Raytracing,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.accelerationStructure = true,
			.descriptorBindingAccelerationStructureUpdateAfterBind = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceRayQueryFeaturesKHR,
		feat & EGraphicsFeatures_RayQuery,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
			.rayQuery = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceRayTracingValidationFeaturesNV,
		feat & EGraphicsFeatures_RayValidation,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV,
			.rayTracingValidation = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
		feat & EGraphicsFeatures_RayPipeline,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			.rayTracingPipeline = true,
			.rayTraversalPrimitiveCulling = true,
			.rayTracingPipelineTraceRaysIndirect = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceRayTracingMotionBlurFeaturesNV,
		feat & EGraphicsFeatures_RayMotionBlur,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV,
			.rayTracingMotionBlur = true,
			.rayTracingMotionBlurPipelineTraceRaysIndirect = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV,
		feat & EGraphicsFeatures_RayReorder,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV,
			.rayTracingInvocationReorder = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceOpacityMicromapFeaturesEXT,
		feat & EGraphicsFeatures_RayMicromapOpacity,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT,
			.micromap = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceDisplacementMicromapFeaturesNV,
		feat & EGraphicsFeatures_RayMicromapDisplacement,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV,
			.displacementMicromap = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceShaderAtomicInt64Features,
		types & EGraphicsDataTypes_AtomicI64,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR,
			.shaderBufferInt64Atomics = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceShaderAtomicFloatFeaturesEXT,
		types & (EGraphicsDataTypes_AtomicF32 | EGraphicsDataTypes_AtomicF64),
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
			.shaderBufferFloat32AtomicAdd = !!(types & EGraphicsDataTypes_AtomicF32),
			.shaderBufferFloat32Atomics = !!(types & EGraphicsDataTypes_AtomicF32),
			.shaderBufferFloat64AtomicAdd = !!(types & EGraphicsDataTypes_AtomicF64),
			.shaderBufferFloat64Atomics = !!(types & EGraphicsDataTypes_AtomicF64)
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceShaderFloat16Int8Features,
		types & EGraphicsDataTypes_F16,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
			.shaderFloat16 = true
		}
	)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	ListConstC8 extensions = (ListConstC8) { 0 };
	ListVkDeviceQueueCreateInfo queues = (ListVkDeviceQueueCreateInfo) { 0 };
	ListVkQueueFamilyProperties queueFamilies = (ListVkQueueFamilyProperties) { 0 };
	CharString tempStr = CharString_createNull();

	Error err = Error_none();
	gotoIfError(clean, ListConstC8_reservex(&extensions, 32))
	gotoIfError(clean, ListVkDeviceQueueCreateInfo_reservex(&queues, EVkCommandQueue_Count))

	for(U64 i = 0; i < reqExtensionsNameCount; ++i)
		gotoIfError(clean, ListConstC8_pushBackx(&extensions, reqExtensionsName[i]))

	if(feat & EGraphicsFeatures_RayPipeline) {
		gotoIfError(clean, ListConstC8_pushBackx(&extensions, "VK_KHR_spirv_1_4"))
		gotoIfError(clean, ListConstC8_pushBackx(&extensions, "VK_KHR_shader_float_controls"))
	}

	if(feat & EGraphicsFeatures_VariableRateShading)
		gotoIfError(clean, ListConstC8_pushBackx(&extensions, "VK_KHR_create_renderpass2"))

	if(feat & EGraphicsFeatures_DirectRendering)
		gotoIfError(clean, ListConstC8_pushBackx(&extensions, "VK_KHR_depth_stencil_resolve"))

	for (U64 i = 0; i < optExtensionsNameCount; ++i) {

		const C8 *ptr = optExtensionsName[i];
		Bool on = false;

		switch (i) {

			case EOptExtensions_PerfQuery:					on = featEx & EVkGraphicsFeatures_PerfQuery;			break;
			case EOptExtensions_RayPipeline:				on = feat & EGraphicsFeatures_RayPipeline;				break;
			case EOptExtensions_RayQuery:					on = feat & EGraphicsFeatures_RayQuery;					break;
			case EOptExtensions_RayAcceleration:			on = feat & EGraphicsFeatures_Raytracing;				break;
			case EOptExtensions_RayMotionBlur:				on = feat & EGraphicsFeatures_RayMotionBlur;			break;
			case EOptExtensions_RayReorder:					on = feat & EGraphicsFeatures_RayReorder;				break;
			case EOptExtensions_MeshShader:					on = feat & EGraphicsFeatures_MeshShader;				break;
			case EOptExtensions_VariableRateShading:		on = feat & EGraphicsFeatures_VariableRateShading;		break;
			case EOptExtensions_DynamicRendering:			on = feat & EGraphicsFeatures_DirectRendering;			break;
			case EOptExtensions_RayMicromapOpacity:			on = feat & EGraphicsFeatures_RayMicromapOpacity;		break;
			case EOptExtensions_RayMicromapDisplacement:	on = feat & EGraphicsFeatures_RayMicromapDisplacement;	break;
			case EOptExtensions_AtomicF32:					on = types & EGraphicsDataTypes_AtomicF32;				break;
			case EOptExtensions_DeferredHostOperations:		on = feat & EGraphicsFeatures_Raytracing;				break;
			case EOptExtensions_RaytracingValidation:		on = feat & EGraphicsFeatures_RayValidation;			break;
			case EOptExtensions_ComputeDeriv:				on = feat & EGraphicsFeatures_ComputeDeriv;				break;
			case EOptExtensions_Maintenance4:				on = featEx & EVkGraphicsFeatures_Maintenance4;			break;
			case EOptExtensions_BufferDeviceAddress:		on = featEx & EVkGraphicsFeatures_BufferDeviceAddress;	break;
			case EOptExtensions_Bindless:					on = feat & EGraphicsFeatures_Bindless;					break;
			case EOptExtensions_DriverProperties:			on = featEx & EVkGraphicsFeatures_DriverProperties;		break;
			case EOptExtensions_AtomicI64:					on = types & EGraphicsDataTypes_AtomicI64;				break;
			case EOptExtensions_F16:						on = types & EGraphicsDataTypes_F16;					break;
			case EOptExtensions_MultiDrawIndirectCount:		on = feat & EGraphicsFeatures_MultiDrawIndirectCount;	break;

			default:
				continue;
		}

		if(on)
			gotoIfError(clean, ListConstC8_pushBackx(&extensions, ptr))
	}

	VkPhysicalDevice physicalDeviceExt = (VkPhysicalDevice) physicalDevice->ext;

	//Get queues

	U32 familyCount = 0;
	instanceExt->getPhysicalDeviceQueueFamilyProperties(physicalDeviceExt, &familyCount, NULL);

	if(!familyCount)
		gotoIfError(clean, Error_invalidOperation(0, "VkGraphicsDevice_init() no supported queues"))

	gotoIfError(clean, ListVkQueueFamilyProperties_resizex(&queueFamilies, familyCount))
	instanceExt->getPhysicalDeviceQueueFamilyProperties(physicalDeviceExt, &familyCount, queueFamilies.ptrNonConst);

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
		gotoIfError(clean, Error_invalidOperation(1, "VkGraphicsDevice_init() doesn't have copy, comp or gfx queue"))

	//Assign queues to queues (deviceInfo)

	F32 prio = 1;

	VkDeviceQueueCreateInfo graphicsQueue = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = graphicsQueueId,
		.queueCount = 1,
		.pQueuePriorities = &prio
	};

	gotoIfError(clean, ListVkDeviceQueueCreateInfo_pushBackx(&queues, graphicsQueue))

	VkDeviceQueueCreateInfo copyQueue = graphicsQueue;
	copyQueue.queueFamilyIndex = copyQueueId;

	if(copyQueueId != graphicsQueueId)
		gotoIfError(clean, ListVkDeviceQueueCreateInfo_pushBackx(&queues, copyQueue))

	VkDeviceQueueCreateInfo computeQueue = graphicsQueue;
	computeQueue.queueFamilyIndex = computeQueueId;

	if(computeQueueId != graphicsQueueId && computeQueueId != copyQueueId)
		gotoIfError(clean, ListVkDeviceQueueCreateInfo_pushBackx(&queues, computeQueue))

	//Create device

	VkDeviceCreateInfo deviceInfo = (VkDeviceCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.enabledExtensionCount = (U32) extensions.length,
		.ppEnabledExtensionNames = extensions.ptr,
		.queueCreateInfoCount = (U32) queues.length,
		.pQueueCreateInfos = queues.ptr
	};

	if (device->flags & EGraphicsDeviceFlags_IsVerbose) {

		Log_debugLnx("Enabling extensions:");

		for(U32 i = 0; i < (U32) extensions.length; ++i)
			Log_debugLnx("\t%s", extensions.ptr[i]);
	}

	gotoIfError(clean, checkVkError(instanceExt->createDevice(physicalDeviceExt, &deviceInfo, NULL, &deviceExt->device)))

	//Load functions even generic 1.2 functionality;
	//This is not done statically to prevent hard to track down issues if a function is missing.
	//Which can happen if an old vulkan runtime is present.
	//TODO: Downgrade to 1.1 with extensions to possibly allow more platforms

	getVkFunctionDevice(clean, vkAllocateMemory, deviceExt->allocateMemory)
	getVkFunctionDevice(clean, vkMapMemory, deviceExt->mapMemory)
	getVkFunctionDevice(clean, vkFreeMemory, deviceExt->freeMemory)
	getVkFunctionDevice(clean, vkCmdClearColorImage, deviceExt->cmdClearColorImage)
	getVkFunctionDevice(clean, vkCmdCopyImage, deviceExt->cmdCopyImage)
	getVkFunctionDevice(clean, vkCmdSetViewport, deviceExt->cmdSetViewport)
	getVkFunctionDevice(clean, vkCmdSetScissor, deviceExt->cmdSetScissor)
	getVkFunctionDevice(clean, vkCmdSetBlendConstants, deviceExt->cmdSetBlendConstants)
	getVkFunctionDevice(clean, vkCmdSetStencilReference, deviceExt->cmdSetStencilReference)
	getVkFunctionDevice(clean, vkCmdBindPipeline, deviceExt->cmdBindPipeline)
	getVkFunctionDevice(clean, vkCmdBindIndexBuffer, deviceExt->cmdBindIndexBuffer)
	getVkFunctionDevice(clean, vkCmdBindVertexBuffers, deviceExt->cmdBindVertexBuffers)
	getVkFunctionDevice(clean, vkCmdDrawIndexed, deviceExt->cmdDrawIndexed)
	getVkFunctionDevice(clean, vkCmdDraw, deviceExt->cmdDraw)
	getVkFunctionDevice(clean, vkCmdDrawIndexedIndirect, deviceExt->cmdDrawIndexedIndirect)
	getVkFunctionDevice(clean, vkCmdDrawIndirect, deviceExt->cmdDrawIndirect)
	getVkFunctionDevice(clean, vkCmdDispatch, deviceExt->cmdDispatch)
	getVkFunctionDevice(clean, vkCmdDispatchIndirect, deviceExt->cmdDispatchIndirect)
	getVkFunctionDevice(clean, vkCreateComputePipelines, deviceExt->createComputePipelines)
	getVkFunctionDevice(clean, vkDestroyPipeline, deviceExt->destroyPipeline)
	getVkFunctionDevice(clean, vkDestroyShaderModule, deviceExt->destroyShaderModule)
	getVkFunctionDevice(clean, vkDestroyBuffer, deviceExt->destroyBuffer)
	getVkFunctionDevice(clean, vkCreateBuffer, deviceExt->createBuffer)
	getVkFunctionDevice(clean, vkGetBufferMemoryRequirements2, deviceExt->getBufferMemoryRequirements2)
	getVkFunctionDevice(clean, vkBindBufferMemory, deviceExt->bindBufferMemory)
	getVkFunctionDevice(clean, vkUpdateDescriptorSets, deviceExt->updateDescriptorSets)
	getVkFunctionDevice(clean, vkFlushMappedMemoryRanges, deviceExt->flushMappedMemoryRanges)
	getVkFunctionDevice(clean, vkCmdCopyBuffer, deviceExt->cmdCopyBuffer)
	getVkFunctionDevice(clean, vkCmdCopyBufferToImage, deviceExt->cmdCopyBufferToImage)
	getVkFunctionDevice(clean, vkGetDeviceQueue, deviceExt->getDeviceQueue)
	getVkFunctionDevice(clean, vkCreateSemaphore, deviceExt->createSemaphore)
	getVkFunctionDevice(clean, vkCreateDescriptorSetLayout, deviceExt->createDescriptorSetLayout)
	getVkFunctionDevice(clean, vkCreatePipelineLayout, deviceExt->createPipelineLayout)
	getVkFunctionDevice(clean, vkCreateDescriptorPool, deviceExt->createDescriptorPool)
	getVkFunctionDevice(clean, vkAllocateDescriptorSets, deviceExt->allocateDescriptorSets)
	getVkFunctionDevice(clean, vkFreeCommandBuffers, deviceExt->freeCommandBuffers)
	getVkFunctionDevice(clean, vkDestroyCommandPool, deviceExt->destroyCommandPool)
	getVkFunctionDevice(clean, vkDestroySemaphore, deviceExt->destroySemaphore)
	getVkFunctionDevice(clean, vkDestroyDescriptorSetLayout, deviceExt->destroyDescriptorSetLayout)
	getVkFunctionDevice(clean, vkDestroyDescriptorPool, deviceExt->destroyDescriptorPool)
	getVkFunctionDevice(clean, vkDestroyPipelineLayout, deviceExt->destroyPipelineLayout)
	getVkFunctionDevice(clean, vkDeviceWaitIdle, deviceExt->deviceWaitIdle)
	getVkFunctionDevice(clean, vkCreateCommandPool, deviceExt->createCommandPool)
	getVkFunctionDevice(clean, vkResetCommandPool, deviceExt->resetCommandPool)
	getVkFunctionDevice(clean, vkAllocateCommandBuffers, deviceExt->allocateCommandBuffers)
	getVkFunctionDevice(clean, vkBeginCommandBuffer, deviceExt->beginCommandBuffer)
	getVkFunctionDevice(clean, vkCmdBindDescriptorSets, deviceExt->cmdBindDescriptorSets)
	getVkFunctionDevice(clean, vkEndCommandBuffer, deviceExt->endCommandBuffer)
	getVkFunctionDevice(clean, vkQueueSubmit, deviceExt->queueSubmit)
	getVkFunctionDevice(clean, vkQueuePresentKHR, deviceExt->queuePresentKHR)
	getVkFunctionDevice(clean, vkCreateGraphicsPipelines, deviceExt->createGraphicsPipelines)
	getVkFunctionDevice(clean, vkDestroyImageView, deviceExt->destroyImageView)
	getVkFunctionDevice(clean, vkCreateImage, deviceExt->createImage)
	getVkFunctionDevice(clean, vkGetImageMemoryRequirements2, deviceExt->getImageMemoryRequirements2)
	getVkFunctionDevice(clean, vkBindImageMemory, deviceExt->bindImageMemory)
	getVkFunctionDevice(clean, vkCreateImageView, deviceExt->createImageView)
	getVkFunctionDevice(clean, vkDestroySampler, deviceExt->destroySampler)
	getVkFunctionDevice(clean, vkCreateSampler, deviceExt->createSampler)
	getVkFunctionDevice(clean, vkCreateShaderModule, deviceExt->createShaderModule)
	getVkFunctionDevice(clean, vkDestroyImage, deviceExt->destroyImage)
	getVkFunctionDevice(clean, vkCreateFence, deviceExt->createFence)
	getVkFunctionDevice(clean, vkWaitForFences, deviceExt->waitForFences)
	getVkFunctionDevice(clean, vkResetFences, deviceExt->resetFences)
	getVkFunctionDevice(clean, vkDestroyFence, deviceExt->destroyFence)

	getVkFunctionDevice(clean, vkCmdPipelineBarrier2KHR, deviceExt->cmdPipelineBarrier2)
	getVkFunctionDevice(clean, vkGetSwapchainImagesKHR, deviceExt->getSwapchainImages)

	getVkFunctionDevice(clean, vkAcquireNextImageKHR, deviceExt->acquireNextImage)
	getVkFunctionDevice(clean, vkCreateSwapchainKHR, deviceExt->createSwapchain)
	getVkFunctionDevice(clean, vkDestroySwapchainKHR, deviceExt->destroySwapchain)

	if(feat & EGraphicsFeatures_MultiDrawIndirectCount) {
		getVkFunctionDevice(clean, vkCmdDrawIndexedIndirectCountKHR, deviceExt->cmdDrawIndexedIndirectCount)
		getVkFunctionDevice(clean, vkCmdDrawIndirectCountKHR, deviceExt->cmdDrawIndirectCount)
	}

	if(feat & EGraphicsFeatures_Raytracing) {
		getVkFunctionDevice(clean, vkCmdBuildAccelerationStructuresKHR, deviceExt->cmdBuildAccelerationStructures)
		getVkFunctionDevice(clean, vkCreateAccelerationStructureKHR, deviceExt->createAccelerationStructure)
		getVkFunctionDevice(clean, vkCmdCopyAccelerationStructureKHR, deviceExt->copyAccelerationStructure)
		getVkFunctionDevice(clean, vkDestroyAccelerationStructureKHR, deviceExt->destroyAccelerationStructure)
		getVkFunctionDevice(clean, vkGetAccelerationStructureBuildSizesKHR, deviceExt->getAccelerationStructureBuildSizes)
		getVkFunctionDevice(clean, vkGetDeviceAccelerationStructureCompatibilityKHR, deviceExt->getAccelerationStructureCompatibility)
	}

	if (feat & EGraphicsFeatures_RayPipeline) {
		getVkFunctionDevice(clean, vkCmdTraceRaysKHR, deviceExt->traceRays)
		getVkFunctionDevice(clean, vkCmdTraceRaysIndirectKHR, deviceExt->traceRaysIndirect)
		getVkFunctionDevice(clean, vkCreateRayTracingPipelinesKHR, deviceExt->createRaytracingPipelines)
		getVkFunctionDevice(clean, vkGetRayTracingShaderGroupHandlesKHR, deviceExt->getRayTracingShaderGroupHandles)
	}

	if(feat & EGraphicsFeatures_DirectRendering) {
		getVkFunctionDevice(clean, vkCmdBeginRenderingKHR, deviceExt->cmdBeginRendering)
		getVkFunctionDevice(clean, vkCmdEndRenderingKHR, deviceExt->cmdEndRendering)
	}

	if(feat & EVkGraphicsFeatures_BufferDeviceAddress)
		getVkFunctionDevice(clean, vkGetBufferDeviceAddressKHR, deviceExt->getBufferDeviceAddress)

	//Get queues

	//Graphics

	VkCommandQueue *graphicsQueueExt = &deviceExt->queues[EVkCommandQueue_Graphics];

	U32 resolvedId = 0;

	deviceExt->getDeviceQueue(
		deviceExt->device,
		graphicsQueueExt->queueId = graphicsQueueId,
		0,
		&graphicsQueueExt->queue
	);

	deviceExt->uniqueQueues[resolvedId] = graphicsQueueId;
	graphicsQueueExt->resolvedQueueId = resolvedId++;
	graphicsQueueExt->type = EVkCommandQueue_Graphics;

	VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType = VK_OBJECT_TYPE_QUEUE
	};

	if(device->flags & EGraphicsDeviceFlags_IsDebug && instanceExt->debugSetName) {
		debugName.pObjectName = "Graphics queue";
		debugName.objectHandle = (U64) graphicsQueueExt->queue;
		gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
	}

	//Compute

	VkCommandQueue *computeQueueExt = &deviceExt->queues[EVkCommandQueue_Compute];

	if(computeQueueId == graphicsQueueId)
		*computeQueueExt = *graphicsQueueExt;

	else {

		deviceExt->getDeviceQueue(
			deviceExt->device,
			computeQueueExt->queueId = computeQueueId,
			0,
			&computeQueueExt->queue
		);

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {
			debugName.pObjectName = "Compute queue";
			debugName.objectHandle = (U64) computeQueueExt->queue;
			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
		}

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

		deviceExt->getDeviceQueue(
			deviceExt->device,
			copyQueueExt->queueId = copyQueueId,
			0,
			&copyQueueExt->queue
		);

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {
			debugName.pObjectName = "Copy queue";
			debugName.objectHandle = (U64) copyQueueExt->queue;
			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
		}

		deviceExt->uniqueQueues[resolvedId] = copyQueueId;
		copyQueueExt->resolvedQueueId = resolvedId++;
		copyQueueExt->type = EVkCommandQueue_Copy;
	}

	//Create command recorder per queue per thread per frame in flight.
	//These will be initialized JIT because we don't know what thread will be accessing them.

	U64 threads = Platform_getThreads();
	gotoIfError(clean, ListVkCommandAllocator_resizex(&deviceExt->commandPools, MAX_FRAMES_IN_FLIGHT * threads * resolvedId))

	//Semaphores

	gotoIfError(clean, ListVkSemaphore_resizex(&deviceExt->submitSemaphores, MAX_FRAMES_IN_FLIGHT))

	for (U64 k = 0; k < MAX_FRAMES_IN_FLIGHT; ++k) {

		VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkSemaphore *semaphore = deviceExt->submitSemaphores.ptrNonConst + k;

		gotoIfError(clean, checkVkError(deviceExt->createSemaphore(deviceExt->device, &semaphoreInfo, NULL, semaphore)))

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

			gotoIfError(clean, CharString_formatx(&tempStr, "Queue submit semaphore %"PRIu64, k))

			VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_SEMAPHORE,
				.objectHandle = (U64) *semaphore,
				.pObjectName = tempStr.ptr,
			};

			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName2)))

			CharString_freex(&tempStr);
		}
	}

	//Create timeline semaphore

	VkFenceCreateInfo fenceInfo = (VkFenceCreateInfo) { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

	for(U64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		gotoIfError(clean, checkVkError(deviceExt->createFence(deviceExt->device, &fenceInfo, NULL, &deviceExt->commitFence[i])))

	deviceExt->resolvedQueues = resolvedId;

	//Create shared layout since we use bindless

	Bool hasRt = physicalDevice->capabilities.features & EGraphicsFeatures_Raytracing;

	for (U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i) {

		VkDescriptorSetLayoutBinding bindings[EDescriptorType_ResourceCount - 1];
		U8 bindingCount = 0;

		if (i == EDescriptorSetType_Resources) {

			for(U32 j = EDescriptorType_Texture2D; j < EDescriptorType_ResourceCount; ++j) {

				if(j == EDescriptorType_Sampler)
					continue;

				if(j == EDescriptorType_TLASExt && !hasRt)
					continue;

				U32 id = j;

				if(j > EDescriptorType_Sampler)
					--id;

				if(j > EDescriptorType_TLASExt && !hasRt)
					--id;

				bindings[id] = (VkDescriptorSetLayoutBinding) {
					.binding = id,
					.stageFlags = VK_SHADER_STAGE_ALL,
					.descriptorCount = descriptorTypeCount[j],
					.descriptorType =
						j == EDescriptorType_TLASExt ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR : (
							j < EDescriptorType_Buffer ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : (
								j <= EDescriptorType_RWBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER :
								VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
							)
						)
				};
			}

			bindingCount = EDescriptorType_ResourceCount - 1 - !hasRt;
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

		gotoIfError(clean, checkVkError(deviceExt->createDescriptorSetLayout(
			deviceExt->device, &setInfo, NULL, &deviceExt->setLayouts[i]
		)))
	}

	VkPipelineLayoutCreateInfo layoutInfo = (VkPipelineLayoutCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = EDescriptorSetType_UniqueLayouts,
		.pSetLayouts = deviceExt->setLayouts
	};

	gotoIfError(clean, checkVkError(deviceExt->createPipelineLayout(deviceExt->device, &layoutInfo, NULL, &deviceExt->defaultLayout)))

	//We only need one pool and 1 descriptor set per EDescriptorType since we use bindless.
	//Every resource is automatically allocated into their respective descriptor set.
	//Last descriptor set (cbuffer) is triple buffered to allow swapping part of the UBO

	VkDescriptorPoolSize poolSizes[] = {
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_SAMPLER, EDescriptorTypeCount_Sampler },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, EDescriptorTypeCount_RWTextures },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, EDescriptorTypeCount_Textures },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, EDescriptorTypeCount_SSBO },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
		(VkDescriptorPoolSize) { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, EDescriptorTypeCount_TLASExt }
	};

	VkDescriptorPoolCreateInfo poolInfo = (VkDescriptorPoolCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = EDescriptorSetType_Count,
		.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]) - !hasRt,
		.pPoolSizes = poolSizes
	};

	gotoIfError(clean, checkVkError(deviceExt->createDescriptorPool(deviceExt->device, &poolInfo, NULL, &deviceExt->descriptorPool)))

	//Last layout repeat 2-3x (that's the CBuffer which needs FRAMES_IN_FLIGHT different versions)

	VkDescriptorSetLayout setLayouts[EDescriptorSetType_Count];

	for(U64 i = 0; i < EDescriptorSetType_Count; ++i)
		setLayouts[i] = deviceExt->setLayouts[U64_min(i, EDescriptorSetType_UniqueLayouts - 1)];

	VkDescriptorSetAllocateInfo setInfo = (VkDescriptorSetAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = deviceExt->descriptorPool,
		.descriptorSetCount = EDescriptorSetType_Count,
		.pSetLayouts = setLayouts
	};

	gotoIfError(clean, checkVkError(deviceExt->allocateDescriptorSets(deviceExt->device, &setInfo, deviceExt->sets)))

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

		static const C8 *debugNames[] = {
			"Samplers",
			"Resources",
			"Global frame cbuffer (0)",
			"Global frame cbuffer (1)",
			"Global frame cbuffer (2)"
		};

		for (U32 i = 0; i < EDescriptorSetType_Count; ++i) {

			CharString_freex(&tempStr);

			gotoIfError(clean, CharString_formatx(&tempStr, "Descriptor set (%"PRIu32": %s)", i, debugNames[i]))

			VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
				.objectHandle = (U64) deviceExt->sets[i],
				.pObjectName = tempStr.ptr,
			};

			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName2)))

			CharString_freex(&tempStr);

			gotoIfError(clean, CharString_formatx(&tempStr, "Descriptor set layout (%"PRIu32": %s)", i, debugNames[i]))

			if(i < EDescriptorSetType_UniqueLayouts) {

				debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
					.objectHandle = (U64) deviceExt->setLayouts[i],
					.pObjectName = tempStr.ptr,
				};

				gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName2)))
			}
		}
	}

	//Get memory properties

	instanceExt->getPhysicalDeviceMemoryProperties((VkPhysicalDevice) physicalDevice->ext, &deviceExt->memoryProperties);

	//Determine when we need to flush.
	//As a rule of thumb I decided for 20% occupied mem by just copies.
	//Or if there's distinct shared mem available too it can allocate 10% more in that memory too
	// (as long as it doesn't exceed 33%).
	//Flush threshold is kept under 4 GiB to avoid TDRs because even if the mem is available it might be slow.

	Bool isDistinct = device->info.type == EGraphicsDeviceType_Dedicated;
	U64 cpuHeapSize = device->info.capabilities.sharedMemory;
	U64 gpuHeapSize = device->info.capabilities.dedicatedMemory;

	device->flushThreshold = U64_min(
		4 * GIBI,
		isDistinct ? U64_min(gpuHeapSize / 3, cpuHeapSize / 10 + gpuHeapSize / 5) :
		cpuHeapSize / 5
	);

	device->flushThresholdPrimitives = 20 * MIBI / 3;		//20M vertices per frame limit

	//Allocate temp storage for transitions

	gotoIfError(clean, ListVkBufferMemoryBarrier2_reservex(&deviceExt->bufferTransitions, 17))
	gotoIfError(clean, ListVkImageMemoryBarrier2_reservex(&deviceExt->imageTransitions, 16))
	gotoIfError(clean, ListVkImageCopy_reservex(&deviceExt->imageCopyRanges, 8))

clean:

	if(err.genericError)
		GraphicsDeviceRef_dec(deviceRef);

	CharString_freex(&tempStr);
	ListConstC8_freex(&extensions);
	ListVkDeviceQueueCreateInfo_freex(&queues);
	ListVkQueueFamilyProperties_freex(&queueFamilies);
	return err;
}

void VK_WRAP_FUNC(GraphicsDevice_postInit)(GraphicsDevice *device) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	//Fill last FRAMES_IN_FLIGHT descriptor sets with UBO[i] to ensure we only modify things in flight.

	VkDescriptorBufferInfo uboBufferInfo[MAX_FRAMES_IN_FLIGHT];
	VkWriteDescriptorSet uboDescriptor[MAX_FRAMES_IN_FLIGHT];

	for(U64 i = 0; i < FRAMES_IN_FLIGHT; ++i) {

		uboBufferInfo[i] = (VkDescriptorBufferInfo) {
			.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(device->frameData[i]), Vk)->buffer,
			.range = sizeof(CBufferData)
		};

		uboDescriptor[i] = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = deviceExt->sets[EDescriptorSetType_CBuffer0 + i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &uboBufferInfo[i]
		};
	}

	deviceExt->updateDescriptorSets(deviceExt->device, FRAMES_IN_FLIGHT, uboDescriptor, 0, NULL);
}

Bool VK_WRAP_FUNC(GraphicsDevice_free)(const GraphicsInstance *instance, void *ext) {

	if(!instance || !ext)
		return instance;

	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);
	VkGraphicsDevice *deviceExt = (VkGraphicsDevice*) ext;

	if(deviceExt->device) {

		for(U64 i = 0; i < deviceExt->commandPools.length; ++i) {

			const VkCommandAllocator alloc = deviceExt->commandPools.ptr[i];

			if(alloc.cmd)
				deviceExt->freeCommandBuffers(deviceExt->device, alloc.pool, 1, &alloc.cmd);

			if(alloc.pool)
				deviceExt->destroyCommandPool(deviceExt->device, alloc.pool, NULL);
		}

		for(U64 i = 0; i < deviceExt->submitSemaphores.length; ++i) {

			const VkSemaphore semaphore = deviceExt->submitSemaphores.ptr[i];

			if(semaphore)
				deviceExt->destroySemaphore(deviceExt->device, semaphore, NULL);
		}

		for(U64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
			if(deviceExt->commitFence[i])
				deviceExt->destroyFence(deviceExt->device, deviceExt->commitFence[i], NULL);

		for(U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i) {

			const VkDescriptorSetLayout layout = deviceExt->setLayouts[i];

			if(layout)
				deviceExt->destroyDescriptorSetLayout(deviceExt->device, layout, NULL);
		}

		if(deviceExt->descriptorPool)
			deviceExt->destroyDescriptorPool(deviceExt->device, deviceExt->descriptorPool, NULL);

		if(deviceExt->defaultLayout)
			deviceExt->destroyPipelineLayout(deviceExt->device, deviceExt->defaultLayout, NULL);

		instanceExt->destroyDevice(deviceExt->device, NULL);
	}

	ListVkCommandAllocator_freex(&deviceExt->commandPools);
	ListVkSemaphore_freex(&deviceExt->submitSemaphores);

	//Free temp storage

	ListVkPipelineStageFlags_freex(&deviceExt->waitStages);
	ListVkSemaphore_freex(&deviceExt->waitSemaphoresList);
	ListVkResult_freex(&deviceExt->results);
	ListU32_freex(&deviceExt->swapchainIndices);
	ListVkSwapchainKHR_freex(&deviceExt->swapchainHandles);
	ListVkBufferMemoryBarrier2_freex(&deviceExt->bufferTransitions);
	ListVkImageMemoryBarrier2_freex(&deviceExt->imageTransitions);
	ListVkImageCopy_freex(&deviceExt->imageCopyRanges);
	ListVkMappedMemoryRange_freex(&deviceExt->mappedMemoryRange);
	ListVkBufferImageCopy_freex(&deviceExt->bufferImageCopyRanges);
	ListVkBufferCopy_freex(&deviceExt->bufferCopies);

	return true;
}

//Executing commands

Error VK_WRAP_FUNC(GraphicsDeviceRef_wait)(GraphicsDeviceRef *deviceRef) {
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	return checkVkError(deviceExt->deviceWaitIdle(GraphicsDevice_ext(device, Vk)->device));
}

VkCommandAllocator *VkGraphicsDevice_getCommandAllocator(
	VkGraphicsDevice *device, U32 resolvedQueueId, U64 threadId, U8 frameInFlightId
) {

	const U64 threadCount = Platform_getThreads();

	if(!device || resolvedQueueId >= device->resolvedQueues || threadId >= threadCount || frameInFlightId >= MAX_FRAMES_IN_FLIGHT)
		return NULL;

	const U64 id = resolvedQueueId + (frameInFlightId * threadCount + threadId) * device->resolvedQueues;

	return device->commandPools.ptrNonConst + id;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Error VK_WRAP_FUNC(GraphicsDevice_submitCommands)(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains,
	CBufferData cbufferData
) {

	//Unpack ext

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	CharString temp = CharString_createNull();
	Error err = Error_none();

	//Reserve temp storage

	gotoIfError(clean, ListVkSwapchainKHR_clear(&deviceExt->swapchainHandles))
	gotoIfError(clean, ListVkSwapchainKHR_reservex(&deviceExt->swapchainHandles, swapchains.length))

	gotoIfError(clean, ListU32_clear(&deviceExt->swapchainIndices))
	gotoIfError(clean, ListU32_reservex(&deviceExt->swapchainIndices, swapchains.length))

	gotoIfError(clean, ListVkResult_clear(&deviceExt->results))
	gotoIfError(clean, ListVkResult_reservex(&deviceExt->results, swapchains.length))

	gotoIfError(clean, ListVkSemaphore_clear(&deviceExt->waitSemaphoresList))
	gotoIfError(clean, ListVkSemaphore_reservex(&deviceExt->waitSemaphoresList, swapchains.length + 1))

	gotoIfError(clean, ListVkPipelineStageFlags_clear(&deviceExt->waitStages))
	gotoIfError(clean, ListVkPipelineStageFlags_reservex(&deviceExt->waitStages, swapchains.length + 1))

	//Wait for previous frame semaphore

	VkFence *fence = &deviceExt->commitFence[(device->submitId - 1) % FRAMES_IN_FLIGHT];

	if (device->submitId > FRAMES_IN_FLIGHT) {

		gotoIfError(clean, checkVkError(deviceExt->waitForFences(
			deviceExt->device,
			1, fence,
			true,
			10 * SECOND
		)))

		gotoIfError(clean, checkVkError(deviceExt->resetFences(deviceExt->device, 1, fence)))
	}

	//Acquire swapchain images

	for(U64 i = 0; i < swapchains.length; ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(swapchains.ptr[i]);
		VkSwapchain *swapchainExt = TextureRef_getImplExtT(VkSwapchain, swapchains.ptr[i]);

		VkSemaphore semaphore = swapchainExt->semaphores.ptr[(device->submitId - 1) % FRAMES_IN_FLIGHT];

		UnifiedTexture *unifiedTexture = TextureRef_getUnifiedTextureIntern(swapchains.ptr[i], NULL);
		U32 currImg = 0;

		gotoIfError(clean, checkVkError(deviceExt->acquireNextImage(
			deviceExt->device,
			swapchainExt->swapchain,
			10 * SECOND,
			semaphore,
			VK_NULL_HANDLE,
			&currImg
		)))

		unifiedTexture->currentImageId = (U8) currImg;

		deviceExt->swapchainHandles.ptrNonConst[i] = swapchainExt->swapchain;
		deviceExt->swapchainIndices.ptrNonConst[i] = unifiedTexture->currentImageId;

		VkPipelineStageFlagBits pipelineStage =
			(swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0) |
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_TRANSFER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		gotoIfError(clean, ListVkSemaphore_pushBackx(&deviceExt->waitSemaphoresList, semaphore))
		gotoIfError(clean, ListVkPipelineStageFlags_pushBackx(&deviceExt->waitStages, pipelineStage))
	}

	//Prepare per frame cbuffer

	{
		DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[(device->submitId - 1) % FRAMES_IN_FLIGHT]);

		for (U32 i = 0; i < swapchains.length; ++i) {

			SwapchainRef *swapchainRef = swapchains.ptr[i];
			Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);

			Bool allowComputeExt = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

			UnifiedTextureImage managedImage = TextureRef_getCurrImage(swapchainRef, 0);

			cbufferData.swapchains[i * 2 + 0] = managedImage.readHandle;
			cbufferData.swapchains[i * 2 + 1] = allowComputeExt ? managedImage.writeHandle : 0;
		}

		*(CBufferData*)frameData->resource.mappedMemoryExt = cbufferData;

		DeviceMemoryBlock block = device->allocator.blocks.ptr[frameData->resource.blockId];
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (incoherent) {

			VkMappedMemoryRange range = (VkMappedMemoryRange){
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory)block.ext,
				.offset = frameData->resource.blockOffset,
				.size = sizeof(CBufferData)
			};

			gotoIfError(clean, checkVkError(deviceExt->flushMappedMemoryRanges(deviceExt->device, 1, &range)))
		}
	}

	//Record command list

	VkCommandBuffer commandBuffer = NULL;

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	U32 graphicsQueueId = queue.queueId;

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % FRAMES_IN_FLIGHT];

	if (commandLists.length) {

		U32 threadId = 0;

		VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, (U8)((device->submitId - 1) % FRAMES_IN_FLIGHT)
		);

		if(!allocator)
			gotoIfError(clean, Error_nullPointer(0, "VkGraphicsDevice_submitCommands() command allocator is NULL"))

		//We create command pools only the first FRAMES_IN_FLIGHT frames, after that they're cached.
		//This is because we have space for [queues][threads][FRAMES_IN_FLIGHT] command pools.
		//Allocating them all even though currently only 1 x FRAMES_IN_FLIGHT are used is quite suboptimal.
		//We only have the space to allow for using more in the future.

		if(!allocator->pool) {

			//TODO: Multi thread command recording

			VkCommandPoolCreateInfo poolInfo = (VkCommandPoolCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
				.queueFamilyIndex = queue.queueId
			};

			gotoIfError(clean, checkVkError(deviceExt->createCommandPool(deviceExt->device, &poolInfo, NULL, &allocator->pool)))

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				gotoIfError(clean, CharString_formatx(
					&temp,
					"%s command pool (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
						queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId,
					(device->submitId - 1) % FRAMES_IN_FLIGHT
				))

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_COMMAND_POOL,
					.pObjectName = temp.ptr,
					.objectHandle = (U64) allocator->pool
				};

				gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))

				CharString_freex(&temp);
			}
		}

		else gotoIfError(clean, checkVkError(deviceExt->resetCommandPool(
			deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
		)))

		//Allocate command buffer if not present yet

		if (!allocator->cmd) {

			VkCommandBufferAllocateInfo bufferInfo = (VkCommandBufferAllocateInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = allocator->pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			gotoIfError(clean, checkVkError(deviceExt->allocateCommandBuffers(deviceExt->device, &bufferInfo, &allocator->cmd)))

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				gotoIfError(clean, CharString_formatx(
					&temp,
					"%s command buffer (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
						queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId,
					(device->submitId - 1) % FRAMES_IN_FLIGHT
				))

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
					.pObjectName = temp.ptr,
					.objectHandle = (U64) allocator->cmd
				};

				gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))

				CharString_freex(&temp);
			}
		}

		//Start buffer

		commandBuffer = allocator->cmd;

		VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		};

		gotoIfError(clean, checkVkError(deviceExt->beginCommandBuffer(commandBuffer, &beginInfo)))

		//Start copies

		VkCommandBufferState state = (VkCommandBufferState) { .buffer = commandBuffer };
		gotoIfError(clean, GraphicsDeviceRef_handleNextFrame(deviceRef, &state))

		//Ensure ubo and staging buffer are the correct states

		VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

		VkDeviceBuffer *uboExt = DeviceBuffer_ext(DeviceBufferRef_ptr(device->frameData[(device->submitId - 1) % FRAMES_IN_FLIGHT]), Vk);

		gotoIfError(clean, VkDeviceBuffer_transition(
			uboExt,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_UNIFORM_READ_BIT,
			graphicsQueueId,
			0,
			0,
			&deviceExt->bufferTransitions,
			&dependency
		))

		if(dependency.bufferMemoryBarrierCount)
			deviceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);

		//Bind pipeline layout and descriptors since they stay the same for the entire frame.
		//For every bind point

		VkDescriptorSet sets[EDescriptorSetType_UniqueLayouts];

		for(U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i)
			sets[i] =
				i != EDescriptorSetType_CBuffer0 ? deviceExt->sets[i] :
				deviceExt->sets[EDescriptorSetType_CBuffer0 + ((device->submitId - 1) % FRAMES_IN_FLIGHT)];

		U64 bindingCount = device->info.capabilities.features & EGraphicsFeatures_RayPipeline ? 3 : 2;

		for(U64 i = 0; i < bindingCount; ++i)
			deviceExt->cmdBindDescriptorSets(
				commandBuffer,
				i == 0 ? VK_PIPELINE_BIND_POINT_COMPUTE : (
					i == 1 ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
				),
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
				(VK_WRAP_FUNC(CommandList_process))(commandList, deviceRef, info.op, ptr, &state);
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

			gotoIfError(clean, VkUnifiedTexture_transition(
				imageExt,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
				0,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				graphicsQueueId,
				&range,
				&deviceExt->imageTransitions,
				&dependency
			))

			if(RefPtr_inc(swapchainRef))
				gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, swapchainRef))
		}

		if(dependency.imageMemoryBarrierCount)
			deviceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);

		//End buffer

		gotoIfError(clean, checkVkError(deviceExt->endCommandBuffer(commandBuffer)))
	}

	//Submit queue
	//TODO: Multiple queues

	VkSemaphore signalSemaphores = deviceExt->submitSemaphores.ptr[(device->submitId - 1) % FRAMES_IN_FLIGHT];

	VkSubmitInfo submitInfo = (VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = (U32) deviceExt->waitSemaphoresList.length,
		.pWaitSemaphores = deviceExt->waitSemaphoresList.ptr,
		.signalSemaphoreCount = !!swapchains.length,
		.pSignalSemaphores = swapchains.length ? &signalSemaphores : NULL,
		.pCommandBuffers = &commandBuffer,
		.commandBufferCount = commandBuffer ? 1 : 0,
		.pWaitDstStageMask = deviceExt->waitStages.ptr
	};

	gotoIfError(clean, checkVkError(deviceExt->queueSubmit(queue.queue, 1, &submitInfo, *fence)))

	//Presents

	if(swapchains.length) {

		VkPresentInfoKHR presentInfo = (VkPresentInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &signalSemaphores,
			.swapchainCount = (U32) swapchains.length,
			.pSwapchains = deviceExt->swapchainHandles.ptr,
			.pImageIndices = deviceExt->swapchainIndices.ptr,
			.pResults = deviceExt->results.ptrNonConst
		};

		gotoIfError(clean, checkVkError(deviceExt->queuePresentKHR(queue.queue, &presentInfo)))

		for(U64 i = 0; i < deviceExt->results.length; ++i)
			gotoIfError(clean, checkVkError(deviceExt->results.ptr[i]))
	}

clean:

	ListVkImageCopy_clear(&deviceExt->imageCopyRanges);
	ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);
	ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);
	CharString_freex(&temp);

	return err;
}

Error VkGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, VkCommandBufferState *commandBuffer) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	Log_performanceLnx("VkGraphicsDevice_flush called!");

	//End current command list

	Error err;
	gotoIfError(clean, checkVkError(deviceExt->endCommandBuffer(commandBuffer->buffer)))

	//Submit only the copy command list

	const VkSubmitInfo submitInfo = (VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pCommandBuffers = &commandBuffer->buffer,
		.commandBufferCount = 1
	};

	const VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	gotoIfError(clean, checkVkError(deviceExt->queueSubmit(
		queue.queue,
		1, &submitInfo,
		deviceExt->commitFence[(device->submitId - 1) % FRAMES_IN_FLIGHT]
	)))

	//Wait for the device

	gotoIfError(clean, GraphicsDeviceRef_wait(deviceRef))

	//Reset command list

	const U32 threadId = 0;

	const VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
		deviceExt, queue.resolvedQueueId, threadId, (U8)((device->submitId - 1) % FRAMES_IN_FLIGHT)
	);

	gotoIfError(clean, checkVkError(deviceExt->resetCommandPool(
		deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
	)))

	//Re-open

	const VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	gotoIfError(clean, checkVkError(deviceExt->beginCommandBuffer(commandBuffer->buffer, &beginInfo)))

	//Re-bind descriptors

	VkDescriptorSet sets[EDescriptorSetType_UniqueLayouts];

	for(U32 i = 0; i < EDescriptorSetType_UniqueLayouts; ++i)
		sets[i] =
			i != EDescriptorSetType_CBuffer0 ? deviceExt->sets[i] :
			deviceExt->sets[EDescriptorSetType_CBuffer0 + ((device->submitId - 1) % FRAMES_IN_FLIGHT)];

	U64 bindingCount = device->info.capabilities.features & EGraphicsFeatures_RayPipeline ? 3 : 2;

	for(U64 i = 0; i < bindingCount; ++i)
		deviceExt->cmdBindDescriptorSets(
			commandBuffer->buffer,
			i == 0 ? VK_PIPELINE_BIND_POINT_COMPUTE : (
				i == 1 ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
			),
			deviceExt->defaultLayout,
			0, EDescriptorSetType_UniqueLayouts, sets,
			0, NULL
		);

	//Reset temporary variables to avoid invalid caching behavior

	for (U64 i = 0; i < EPipelineType_Count; ++i)
		commandBuffer->pipelines[i] = NULL;

	commandBuffer->boundScissor = (VkRect2D) { 0 };
	commandBuffer->boundViewport = (VkViewport) { 0 };
	commandBuffer->boundBuffers = (SetPrimitiveBuffersCmd) { 0 };
	commandBuffer->stencilRef = 0;
	commandBuffer->blendConstants = F32x4_zero();

clean:
	return err;
}
