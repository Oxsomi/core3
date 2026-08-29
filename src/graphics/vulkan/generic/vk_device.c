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

//graphics/vulkan/generic/vk_device.c

#include "types/container/list_impl.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"
#include "platforms/logx.h"
#include "platforms/platform.h"
#include "platforms/window.h"
#include "types/base/buffer_base.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

TListImpl(VkCommandAllocator);
TListImpl(VkSemaphore);
TListImpl(VkResult);
TListImpl(VkSwapchainKHR);
TListImpl(VkPipelineStageFlags);
TListImpl(VkQueryPool);

#define bindNextVkStruct(T, condition, ...) \
	T tmp##T = __VA_ARGS__;                 \
											\
	if(condition) {                         \
		*currPNext = &tmp##T;               \
		currPNext = &tmp##T.pNext;          \
	}

TList(VkDeviceQueueCreateInfo);
TList(VkQueueFamilyProperties);
TListImpl(VkDeviceQueueCreateInfo);
TListImpl(VkQueueFamilyProperties);

//Declared only, since vk_instance.c in the same library provides the implementation.

TList(VkExtensionProperties);

TListImpl(VkDescriptorBufferInfo);
TListImpl(VkDescriptorImageInfo);
TListImpl(VkAccelerationStructureKHR);
TListImpl(VkDescriptorTableRange);

#define getVkFunctionDevice(label, function, result) {                                          \
																								\
	PFN_vkVoidFunction v = instanceExt->getDeviceProcAddr(deviceExt->device, #function);        \
																								\
	if(!v)                                                                                      \
		retError(clean, Error_nullPointer(0, "getVkFunction() " #function " failed"));          \
																								\
	*(void**)&result = (void*) v;                                                               \
} (void) 0

//Names the extensions vkCreateDevice refused, since it only reports that one of them was missing.
//Most are requested because the device advertised them, but the raytracing, render pass and depth stencil resolve
// ones are requested off a feature bit instead, so those can be asked for on a device that never offered them.

static Bool VkGraphicsDevice_logMissingExtensions(
	const VkGraphicsInstance *instanceExt,
	VkPhysicalDevice physicalDevice,
	const ListConstC8 *requested,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListVkExtensionProperties supported = (ListVkExtensionProperties) { 0 };
	U32 count = 0;

	gotoIfError3(clean, checkVkError(
		instanceExt->enumerateDeviceExtensionProperties(physicalDevice, NULL, &count, NULL), e_rr
	));

	gotoIfError3(clean, ListVkExtensionProperties_resize(&supported, count, alloc, e_rr));

	gotoIfError3(clean, checkVkError(
		instanceExt->enumerateDeviceExtensionProperties(physicalDevice, NULL, &count, supported.ptrNonConst), e_rr
	));

	for (U64 i = 0; i < requested->length; ++i) {

		const CharString name = CharString_createRefCStrConst(requested->ptr[i]);
		Bool found = false;

		for (U64 j = 0; j < supported.length && !found; ++j)
			found = CharString_equalsCStringSensitive(&name, supported.ptr[j].extensionName);

		if(!found)
			Log_errorLnx("Vulkan: vkCreateDevice() requires %s, which this device doesn't support", requested->ptr[i]);
	}

clean:
	ListVkExtensionProperties_free(&supported, alloc);
	return s_uccess;
}

Bool VK_WRAP_FUNC(GraphicsDevice_init)(
	const GraphicsInstance *instance,
	const GraphicsDeviceInfo *physicalDevice,
	GraphicsDeviceRef **deviceRef,
	Error *e_rr
) {

	const Allocator *alloc = instance->alloc;

	Bool s_uccess = true;

	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);
	(void)instanceExt;

	EGraphicsFeatures feat = physicalDevice->capabilities.features;
	EGraphicsFeatures2 feat2 = physicalDevice->capabilities.features2;
	EVkGraphicsFeatures featEx = physicalDevice->capabilities.featuresExt;
	EGraphicsDataTypes types = physicalDevice->capabilities.dataTypes;

	VkPhysicalDeviceFeatures features = (VkPhysicalDeviceFeatures) {

		.fullDrawIndexUint32 = true,
		.imageCubeArray = true,
		.independentBlend = true,

		.geometryShader = (Bool)(feat & EGraphicsFeatures_GeometryShader),
		.tessellationShader = true,

		.multiDrawIndirect = true,
		.sampleRateShading = true,
		.shaderStorageImageReadWithoutFormat = true,
		.shaderStorageImageWriteWithoutFormat = true,

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
		.dualSrcBlend = (Bool)(feat & EGraphicsFeatures_DualSrcBlend),
		.shaderStorageImageMultisample = (Bool)(feat & EGraphicsFeatures_WriteMSTexture)
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
		VkPhysicalDeviceConditionalRenderingFeaturesEXT,
		feat2 & EGraphicsFeatures2_Predication,
		(VkPhysicalDeviceConditionalRenderingFeaturesEXT) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT,
			.conditionalRendering = true
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
			.descriptorBindingAccelerationStructureUpdateAfterBind = true,
			.accelerationStructureIndirectBuild = !!(feat2 & EGraphicsFeatures2_RayIndirectASBuild)
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

	//No NV fallback: on an SDK without the EXT struct the RayReorder claim can never be set (the feature
	// query site skips it), so there is nothing to chain.

	#ifdef VK_EXT_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME
		bindNextVkStruct(
			VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT,
			feat & EGraphicsFeatures_RayReorder,
			{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_EXT,
				.rayTracingInvocationReorder = true
			}
		)
	#endif

	bindNextVkStruct(
		VkPhysicalDeviceOpacityMicromapFeaturesEXT,
		feat & EGraphicsFeatures_RayMicromapOpacity,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT,
			.micromap = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceCooperativeVectorFeaturesNV,
		feat & EGraphicsFeatures_CoopVec,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_FEATURES_NV,
			.cooperativeVector = true,
			.cooperativeVectorTraining = !!(feat & EGraphicsFeatures_CoopVecTraining)
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceCooperativeMatrixFeaturesKHR,
		feat & EGraphicsFeatures_CoopMat,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR,
			.cooperativeMatrix = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceShaderFloat8FeaturesEXT,
		feat & EGraphicsFeatures_CoopFP8,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT8_FEATURES_EXT,
			.shaderFloat8 = true,
			.shaderFloat8CooperativeMatrix = !!(feat & EGraphicsFeatures_CoopMat)
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR,
		feat & EGraphicsFeatures_RayTriPosition,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR,
			.rayTracingPositionFetch = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR,
		feat & EGraphicsFeatures_Barycentrics,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,
			.fragmentShaderBarycentric = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDeviceDescriptorHeapFeaturesEXT,
		feat2 & EGraphicsFeatures2_DescriptorHeap,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT,
			.descriptorHeap = true
		}
	)

	//Both derivative group modes are requested, not just the one detection happens to look at.
	//Enabling the extension alone isn't enough for a features struct: without this the device advertises
	// ComputeDeriv while neither mode is actually on, and every ddx/ddy in a compute shader is then rejected
	// by validation at vkCreateShaderModule.
	//Which mode a shader needs is DXC's choice rather than ours: it emits the quad group for an even 2D
	// thread group and the linear group otherwise, and both map to this one OxC3 feature.

	//The KHR and NV structs are aliases of one another, so either enables the feature.
	//The #ifdef only picks whichever name the SDK in use actually declares, matching how vk_instance.c queries it.

	#ifdef VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME
		bindNextVkStruct(
			VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR,
			feat & EGraphicsFeatures_ComputeDeriv,
			{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR,
				.computeDerivativeGroupQuads = true,
				.computeDerivativeGroupLinear = true
			}
		)
	#else
		bindNextVkStruct(
			VkPhysicalDeviceComputeShaderDerivativesFeaturesNV,
			feat & EGraphicsFeatures_ComputeDeriv,
			{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV,
				.computeDerivativeGroupQuads = true,
				.computeDerivativeGroupLinear = true
			}
		)
	#endif

	bindNextVkStruct(
		VkPhysicalDeviceClusterAccelerationStructureFeaturesNV,
		feat2 & EGraphicsFeatures2_RayClusterAS,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_ACCELERATION_STRUCTURE_FEATURES_NV,
			.clusterAccelerationStructure = true
		}
	)

	bindNextVkStruct(
		VkPhysicalDevicePartitionedAccelerationStructureFeaturesNV,
		feat2 & EGraphicsFeatures2_RayPartitionedTLAS,
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PARTITIONED_ACCELERATION_STRUCTURE_FEATURES_NV,
			.partitionedAccelerationStructure = true
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
			.shaderBufferFloat32AtomicAdd = (Bool)(types & EGraphicsDataTypes_AtomicF32),
			.shaderBufferFloat32Atomics = (Bool)(types & EGraphicsDataTypes_AtomicF32),
			.shaderBufferFloat64AtomicAdd = (Bool)(types & EGraphicsDataTypes_AtomicF64),
			.shaderBufferFloat64Atomics = (Bool)(types & EGraphicsDataTypes_AtomicF64)
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

	deviceExt->framesInFlight = device->framesInFlight;        //Copy so it's known at delete

	ListConstC8 extensions = (ListConstC8) { 0 };
	ListVkDeviceQueueCreateInfo queues = (ListVkDeviceQueueCreateInfo) { 0 };
	ListVkQueueFamilyProperties queueFamilies = (ListVkQueueFamilyProperties) { 0 };
	CharString tempStr = CharString_createNull();

	gotoIfError3(clean, ListConstC8_reserve(&extensions, 32, alloc, e_rr));
	gotoIfError3(clean, ListVkDeviceQueueCreateInfo_reserve(&queues, EVkCommandQueue_Count, alloc, e_rr));

	for(U64 i = 0; i < reqExtensionsNameCount; ++i)
		gotoIfError3(clean, ListConstC8_pushBack(&extensions, reqExtensionsName[i], alloc, e_rr));

	//Every extension below comes from the optional table, so nothing is ever requested that the device didn't
	// advertise during enumeration.
	//The dependency extensions used to be pushed here off a feature bit instead, which is how a device could be
	// offered a feature it advertised and then refused for an extension it never had.

	for (U64 i = 0; i < optExtensionsNameCount; ++i) {

		const C8 *ptr = optExtensionsName[i];
		Bool on = false;

		switch (i) {

			case EOptExtensions_PerfQuery:                  on = featEx & EVkGraphicsFeatures_PerfQuery;            break;
			case EOptExtensions_RayPipeline:                on = feat & EGraphicsFeatures_RayPipeline;              break;
			case EOptExtensions_RayQuery:                   on = feat & EGraphicsFeatures_RayQuery;                 break;
			case EOptExtensions_RayAcceleration:            on = feat & EGraphicsFeatures_Raytracing;               break;
			case EOptExtensions_RayReorder:                 on = feat & EGraphicsFeatures_RayReorder;               break;
			case EOptExtensions_MeshShader:                 on = feat & EGraphicsFeatures_MeshShader;               break;
			case EOptExtensions_VariableRateShading:        on = feat & EGraphicsFeatures_VariableRateShading;      break;
			case EOptExtensions_DynamicRendering:           on = feat & EGraphicsFeatures_DirectRendering;          break;
			//EXT is the fallback: a device that got the KHR promotion runs that instead, see vk_instance.c

			case EOptExtensions_RayMicromapOpacity:
				on = (feat & EGraphicsFeatures_RayMicromapOpacity) && !(featEx & EVkGraphicsFeatures_OpacityMicromapKHR);
				break;

			case EOptExtensions_RayMicromapOpacityKHR:
			case EOptExtensions_DeviceAddressCommands:
				on = featEx & EVkGraphicsFeatures_OpacityMicromapKHR;
				break;
			case EOptExtensions_AtomicF32:                  on = types & EGraphicsDataTypes_AtomicF32;              break;
			case EOptExtensions_DeferredHostOperations:     on = feat & EGraphicsFeatures_Raytracing;               break;
			case EOptExtensions_RaytracingValidation:       on = feat & EGraphicsFeatures_RayValidation;            break;
			case EOptExtensions_ComputeDeriv:               on = feat & EGraphicsFeatures_ComputeDeriv;             break;
			case EOptExtensions_Maintenance4:               on = featEx & EVkGraphicsFeatures_Maintenance4;         break;
			case EOptExtensions_BufferDeviceAddress:        on = featEx & EVkGraphicsFeatures_BufferDeviceAddress;  break;
			case EOptExtensions_Bindless:                   on = feat & EGraphicsFeatures_Bindless;                 break;
			case EOptExtensions_DriverProperties:           on = featEx & EVkGraphicsFeatures_DriverProperties;     break;
			case EOptExtensions_AtomicI64:                  on = types & EGraphicsDataTypes_AtomicI64;              break;
			case EOptExtensions_F16:                        on = types & EGraphicsDataTypes_F16;                    break;
			case EOptExtensions_MultiDrawIndirectCount:     on = feat & EGraphicsFeatures_MultiDrawIndirectCount;   break;
			case EOptExtensions_MemoryBudget:               on = featEx & EVkGraphicsFeatures_MemoryBudget;         break;
			case EOptExtensions_CooperativeVector:          on = feat & EGraphicsFeatures_CoopVec;                  break;
			case EOptExtensions_CooperativeMatrix:          on = feat & EGraphicsFeatures_CoopMat;                  break;
			case EOptExtensions_ShaderFloat8:               on = feat & EGraphicsFeatures_CoopFP8;                  break;
			case EOptExtensions_RayTriPosition:             on = feat & EGraphicsFeatures_RayTriPosition;           break;
			case EOptExtensions_Barycentrics:               on = feat & EGraphicsFeatures_Barycentrics;             break;
			case EOptExtensions_DescriptorHeap:             on = feat2 & EGraphicsFeatures2_DescriptorHeap;         break;
			case EOptExtensions_RayClusterAS:               on = feat2 & EGraphicsFeatures2_RayClusterAS;           break;
			case EOptExtensions_RayPartitionedTLAS:         on = feat2 & EGraphicsFeatures2_RayPartitionedTLAS;     break;
			case EOptExtensions_PushDescriptor:             on = featEx & EVkGraphicsFeatures_PerformantPushDescriptor; break;
			case EOptExtensions_ConditionalRendering:       on = feat2 & EGraphicsFeatures2_Predication;            break;

			//Dependencies, requested alongside whichever feature needs them

			case EOptExtensions_DepthStencilResolve:        on = feat & EGraphicsFeatures_DirectRendering;          break;
			case EOptExtensions_Maintenance5:               on = feat2 & EGraphicsFeatures2_DescriptorHeap;         break;

			case EOptExtensions_CreateRenderpass2:
				on = feat & (EGraphicsFeatures_VariableRateShading | EGraphicsFeatures_DirectRendering);
				break;

			case EOptExtensions_Spirv14:
			case EOptExtensions_ShaderFloatControls:
				on = feat & (EGraphicsFeatures_RayPipeline | EGraphicsFeatures_RayQuery);
				break;

			default:
				continue;
		}

		if(on)
			gotoIfError3(clean, ListConstC8_pushBack(&extensions, ptr, alloc, e_rr));
	}

	VkPhysicalDevice physicalDeviceExt = (VkPhysicalDevice) physicalDevice->ext;

	//Get queues

	U32 familyCount = 0;
	instanceExt->getPhysicalDeviceQueueFamilyProperties(physicalDeviceExt, &familyCount, NULL);

	if(!familyCount)
		retError(clean, Error_invalidOperation(0, "VkGraphicsDevice_init() no supported queues"));

	gotoIfError3(clean, ListVkQueueFamilyProperties_resize(&queueFamilies, familyCount, alloc, e_rr));
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

		if(graphicsQueueId == U32_MAX && q.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsQueueId = i;
			deviceExt->timestampValidBits = q.timestampValidBits;
		}

		if(graphicsQueueId != U32_MAX && computeQueueId != U32_MAX && copyQueueId != U32_MAX)
			break;
	}

	//If there's no dedicated queue, we should use the one that supports it.

	if(computeQueueId == U32_MAX)
		computeQueueId = fallbackComputeQueueId;

	if(copyQueueId == U32_MAX)
		copyQueueId = fallbackCopyQueueId;

	//Ensure we have all queues.
	//Should be impossible, but still.

	if(copyQueueId == U32_MAX || computeQueueId == U32_MAX || graphicsQueueId == U32_MAX)
		retError(clean, Error_invalidOperation(1, "VkGraphicsDevice_init() doesn't have copy, comp or gfx queue"));

	//Assign queues to queues (deviceInfo)

	F32 prio = 1;

	VkDeviceQueueCreateInfo graphicsQueue = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = graphicsQueueId,
		.queueCount = 1,
		.pQueuePriorities = &prio
	};

	gotoIfError3(clean, ListVkDeviceQueueCreateInfo_pushBack(&queues, graphicsQueue, alloc, e_rr));

	VkDeviceQueueCreateInfo copyQueue = graphicsQueue;
	copyQueue.queueFamilyIndex = copyQueueId;

	if(copyQueueId != graphicsQueueId)
		gotoIfError3(clean, ListVkDeviceQueueCreateInfo_pushBack(&queues, copyQueue, alloc, e_rr));

	VkDeviceQueueCreateInfo computeQueue = graphicsQueue;
	computeQueue.queueFamilyIndex = computeQueueId;

	if(computeQueueId != graphicsQueueId && computeQueueId != copyQueueId)
		gotoIfError3(clean, ListVkDeviceQueueCreateInfo_pushBack(&queues, computeQueue, alloc, e_rr));

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

	const VkResult createResult = instanceExt->createDevice(physicalDeviceExt, &deviceInfo, NULL, &deviceExt->device);

	//vkCreateDevice reports that an extension was missing but never which one.
	//Four of them are requested from a feature bit rather than from the device's own extension list, so a device
	// advertising the feature without the extension fails here with nothing to go on.
	//Diagnostic only, so its own errors are swallowed and the original result is what propagates.

	if(createResult == VK_ERROR_EXTENSION_NOT_PRESENT)
		VkGraphicsDevice_logMissingExtensions(instanceExt, physicalDeviceExt, &extensions, alloc, NULL);

	gotoIfError3(clean, checkVkError(createResult, e_rr));

	//Load functions even generic 1.1 functionality;
	//This is not done statically to prevent hard to track down issues if a function is missing.
	//Which can happen if an old vulkan runtime is present.

	getVkFunctionDevice(clean, vkAllocateMemory, deviceExt->allocateMemory);
	getVkFunctionDevice(clean, vkMapMemory, deviceExt->mapMemory);
	getVkFunctionDevice(clean, vkFreeMemory, deviceExt->freeMemory);
	getVkFunctionDevice(clean, vkCmdClearColorImage, deviceExt->cmdClearColorImage);
	getVkFunctionDevice(clean, vkCmdCopyImage, deviceExt->cmdCopyImage);
	getVkFunctionDevice(clean, vkCmdSetViewport, deviceExt->cmdSetViewport);
	getVkFunctionDevice(clean, vkCmdSetScissor, deviceExt->cmdSetScissor);
	getVkFunctionDevice(clean, vkCmdSetBlendConstants, deviceExt->cmdSetBlendConstants);
	getVkFunctionDevice(clean, vkCmdSetStencilReference, deviceExt->cmdSetStencilReference);
	getVkFunctionDevice(clean, vkCmdBindPipeline, deviceExt->cmdBindPipeline);
	getVkFunctionDevice(clean, vkCmdPushConstants, deviceExt->cmdPushConstants);
	getVkFunctionDevice(clean, vkCmdBindIndexBuffer, deviceExt->cmdBindIndexBuffer);
	getVkFunctionDevice(clean, vkCmdBindVertexBuffers, deviceExt->cmdBindVertexBuffers);
	getVkFunctionDevice(clean, vkCmdDrawIndexed, deviceExt->cmdDrawIndexed);
	getVkFunctionDevice(clean, vkCmdDraw, deviceExt->cmdDraw);
	getVkFunctionDevice(clean, vkCmdDrawIndexedIndirect, deviceExt->cmdDrawIndexedIndirect);
	getVkFunctionDevice(clean, vkCmdDrawIndirect, deviceExt->cmdDrawIndirect);
	getVkFunctionDevice(clean, vkCmdDispatch, deviceExt->cmdDispatch);
	getVkFunctionDevice(clean, vkCmdDispatchIndirect, deviceExt->cmdDispatchIndirect);
	getVkFunctionDevice(clean, vkCreateComputePipelines, deviceExt->createComputePipelines);
	getVkFunctionDevice(clean, vkDestroyPipeline, deviceExt->destroyPipeline);
	getVkFunctionDevice(clean, vkDestroyShaderModule, deviceExt->destroyShaderModule);
	getVkFunctionDevice(clean, vkDestroyBuffer, deviceExt->destroyBuffer);
	getVkFunctionDevice(clean, vkCreateBuffer, deviceExt->createBuffer);
	getVkFunctionDevice(clean, vkGetBufferMemoryRequirements2, deviceExt->getBufferMemoryRequirements2);
	getVkFunctionDevice(clean, vkBindBufferMemory, deviceExt->bindBufferMemory);
	getVkFunctionDevice(clean, vkUpdateDescriptorSets, deviceExt->updateDescriptorSets);
	getVkFunctionDevice(clean, vkCmdCopyImageToBuffer, deviceExt->cmdCopyImageToBuffer);
	getVkFunctionDevice(clean, vkFlushMappedMemoryRanges, deviceExt->flushMappedMemoryRanges);
	getVkFunctionDevice(clean, vkCmdCopyBuffer, deviceExt->cmdCopyBuffer);
	getVkFunctionDevice(clean, vkCmdCopyBufferToImage, deviceExt->cmdCopyBufferToImage);
	getVkFunctionDevice(clean, vkGetDeviceQueue, deviceExt->getDeviceQueue);
	getVkFunctionDevice(clean, vkCreateSemaphore, deviceExt->createSemaphore);
	getVkFunctionDevice(clean, vkCreateDescriptorSetLayout, deviceExt->createDescriptorSetLayout);
	getVkFunctionDevice(clean, vkCreatePipelineLayout, deviceExt->createPipelineLayout);
	getVkFunctionDevice(clean, vkCreateDescriptorPool, deviceExt->createDescriptorPool);
	getVkFunctionDevice(clean, vkAllocateDescriptorSets, deviceExt->allocateDescriptorSets);
	getVkFunctionDevice(clean, vkFreeCommandBuffers, deviceExt->freeCommandBuffers);
	getVkFunctionDevice(clean, vkDestroyCommandPool, deviceExt->destroyCommandPool);
	getVkFunctionDevice(clean, vkDestroySemaphore, deviceExt->destroySemaphore);
	getVkFunctionDevice(clean, vkDestroyDescriptorSetLayout, deviceExt->destroyDescriptorSetLayout);
	getVkFunctionDevice(clean, vkDestroyDescriptorPool, deviceExt->destroyDescriptorPool);
	getVkFunctionDevice(clean, vkDestroyPipelineLayout, deviceExt->destroyPipelineLayout);
	getVkFunctionDevice(clean, vkDeviceWaitIdle, deviceExt->deviceWaitIdle);
	getVkFunctionDevice(clean, vkCreateCommandPool, deviceExt->createCommandPool);
	getVkFunctionDevice(clean, vkResetCommandPool, deviceExt->resetCommandPool);
	getVkFunctionDevice(clean, vkAllocateCommandBuffers, deviceExt->allocateCommandBuffers);
	getVkFunctionDevice(clean, vkBeginCommandBuffer, deviceExt->beginCommandBuffer);
	getVkFunctionDevice(clean, vkCmdBindDescriptorSets, deviceExt->cmdBindDescriptorSets);
	//Only resolvable when the extension was enabled, so a device without it leaves this NULL.
	//GraphicsDevice_rebindDescriptors reads that as "emulate" and binds a per frame set instead.

	if(featEx & EVkGraphicsFeatures_PerformantPushDescriptor)
		getVkFunctionDevice(clean, vkCmdPushDescriptorSetKHR, deviceExt->cmdPushDescriptorSet);

	getVkFunctionDevice(clean, vkEndCommandBuffer, deviceExt->endCommandBuffer);
	getVkFunctionDevice(clean, vkQueueSubmit, deviceExt->queueSubmit);
	getVkFunctionDevice(clean, vkCreateQueryPool, deviceExt->createQueryPool);
	getVkFunctionDevice(clean, vkDestroyQueryPool, deviceExt->destroyQueryPool);
	getVkFunctionDevice(clean, vkCmdResetQueryPool, deviceExt->cmdResetQueryPool);
	getVkFunctionDevice(clean, vkCmdWriteTimestamp, deviceExt->cmdWriteTimestamp);

	if(device->info.capabilities.features2 & EGraphicsFeatures2_Predication) {
		getVkFunctionDevice(clean, vkCmdBeginConditionalRenderingEXT, deviceExt->cmdBeginConditionalRendering);
		getVkFunctionDevice(clean, vkCmdEndConditionalRenderingEXT, deviceExt->cmdEndConditionalRendering);
	}
	getVkFunctionDevice(clean, vkGetQueryPoolResults, deviceExt->getQueryPoolResults);
	getVkFunctionDevice(clean, vkQueuePresentKHR, deviceExt->queuePresentKHR);
	getVkFunctionDevice(clean, vkCreateGraphicsPipelines, deviceExt->createGraphicsPipelines);
	getVkFunctionDevice(clean, vkDestroyImageView, deviceExt->destroyImageView);
	getVkFunctionDevice(clean, vkCreateImage, deviceExt->createImage);
	getVkFunctionDevice(clean, vkGetImageMemoryRequirements2, deviceExt->getImageMemoryRequirements2);
	getVkFunctionDevice(clean, vkBindImageMemory, deviceExt->bindImageMemory);
	getVkFunctionDevice(clean, vkCreateImageView, deviceExt->createImageView);
	getVkFunctionDevice(clean, vkDestroySampler, deviceExt->destroySampler);
	getVkFunctionDevice(clean, vkCreateSampler, deviceExt->createSampler);
	getVkFunctionDevice(clean, vkCreateShaderModule, deviceExt->createShaderModule);
	getVkFunctionDevice(clean, vkDestroyImage, deviceExt->destroyImage);
	getVkFunctionDevice(clean, vkCreateFence, deviceExt->createFence);
	getVkFunctionDevice(clean, vkWaitForFences, deviceExt->waitForFences);
	getVkFunctionDevice(clean, vkResetFences, deviceExt->resetFences);
	getVkFunctionDevice(clean, vkDestroyFence, deviceExt->destroyFence);
	getVkFunctionDevice(clean, vkFreeDescriptorSets, deviceExt->freeDescriptorSets);

	getVkFunctionDevice(clean, vkCmdPipelineBarrier2KHR, deviceExt->cmdPipelineBarrier2);
	getVkFunctionDevice(clean, vkGetSwapchainImagesKHR, deviceExt->getSwapchainImages);

	getVkFunctionDevice(clean, vkAcquireNextImageKHR, deviceExt->acquireNextImage);
	getVkFunctionDevice(clean, vkCreateSwapchainKHR, deviceExt->createSwapchain);
	getVkFunctionDevice(clean, vkDestroySwapchainKHR, deviceExt->destroySwapchain);

	if(feat & EGraphicsFeatures_MultiDrawIndirectCount) {
		getVkFunctionDevice(clean, vkCmdDrawIndexedIndirectCountKHR, deviceExt->cmdDrawIndexedIndirectCount);
		getVkFunctionDevice(clean, vkCmdDrawIndirectCountKHR, deviceExt->cmdDrawIndirectCount);
	}

	if(feat & EGraphicsFeatures_Raytracing) {
		getVkFunctionDevice(clean, vkCmdBuildAccelerationStructuresKHR, deviceExt->cmdBuildAccelerationStructures);
		getVkFunctionDevice(clean, vkCreateAccelerationStructureKHR, deviceExt->createAccelerationStructure);
		getVkFunctionDevice(clean, vkCmdCopyAccelerationStructureKHR, deviceExt->copyAccelerationStructure);
		getVkFunctionDevice(
			clean, vkCmdWriteAccelerationStructuresPropertiesKHR, deviceExt->writeAccelerationStructuresProperties
		);
		getVkFunctionDevice(clean, vkDestroyAccelerationStructureKHR, deviceExt->destroyAccelerationStructure);
		getVkFunctionDevice(clean, vkGetAccelerationStructureBuildSizesKHR, deviceExt->getAccelerationStructureBuildSizes);
		getVkFunctionDevice(
			clean,
			vkGetAccelerationStructureDeviceAddressKHR,
			deviceExt->getAccelerationStructureDeviceAddress
		);
		getVkFunctionDevice(
			clean,
			vkGetDeviceAccelerationStructureCompatibilityKHR,
			deviceExt->getAccelerationStructureCompatibility
		);

		//The EXT micromap entry points; the KHR promotion has none, its arrays build through the AS calls above

		if(
			(feat & EGraphicsFeatures_RayMicromapOpacity) &&
			!(device->info.capabilities.featuresExt & EVkGraphicsFeatures_OpacityMicromapKHR)
		) {
			getVkFunctionDevice(clean, vkCreateMicromapEXT, deviceExt->createMicromap);
			getVkFunctionDevice(clean, vkDestroyMicromapEXT, deviceExt->destroyMicromap);
			getVkFunctionDevice(clean, vkCmdBuildMicromapsEXT, deviceExt->cmdBuildMicromaps);
			getVkFunctionDevice(clean, vkGetMicromapBuildSizesEXT, deviceExt->getMicromapBuildSizes);
		}
	}

	if (feat & EGraphicsFeatures_RayPipeline) {
		getVkFunctionDevice(clean, vkCmdTraceRaysKHR, deviceExt->traceRays);
		getVkFunctionDevice(clean, vkCmdTraceRaysIndirectKHR, deviceExt->traceRaysIndirect);
		getVkFunctionDevice(clean, vkCreateRayTracingPipelinesKHR, deviceExt->createRaytracingPipelines);
		getVkFunctionDevice(clean, vkGetRayTracingShaderGroupHandlesKHR, deviceExt->getRayTracingShaderGroupHandles);
	}

	if(feat & EGraphicsFeatures_DirectRendering) {
		getVkFunctionDevice(clean, vkCmdBeginRenderingKHR, deviceExt->cmdBeginRendering);
		getVkFunctionDevice(clean, vkCmdEndRenderingKHR, deviceExt->cmdEndRendering);
	}

	if(featEx & EVkGraphicsFeatures_BufferDeviceAddress)
		getVkFunctionDevice(clean, vkGetBufferDeviceAddressKHR, deviceExt->getBufferDeviceAddress);

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

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {
		debugName.pObjectName = "Graphics queue";
		debugName.objectHandle = (U64) graphicsQueueExt->queue;
		gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
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
			gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
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
			gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
		}

		deviceExt->uniqueQueues[resolvedId] = copyQueueId;
		copyQueueExt->resolvedQueueId = resolvedId++;
		copyQueueExt->type = EVkCommandQueue_Copy;
	}

	//Create command recorder per queue per thread per frame in flight.
	//These will be initialized JIT because we don't know what thread will be accessing them.

	U64 threads = Platform_getThreads();
	gotoIfError3(clean, ListVkCommandAllocator_resize(
		&deviceExt->commandPools,
		device->framesInFlight * threads * resolvedId,
		alloc,
		e_rr
	));

	//Semaphores

	gotoIfError3(clean, ListVkSemaphore_resize(&deviceExt->submitSemaphores, device->framesInFlight, alloc, e_rr));

	for (U64 k = 0; k < device->framesInFlight; ++k) {

		VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkSemaphore *semaphore = deviceExt->submitSemaphores.ptrNonConst + k;

		gotoIfError3(clean, checkVkError(deviceExt->createSemaphore(deviceExt->device, &semaphoreInfo, NULL, semaphore), e_rr));

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

			gotoIfError3(clean, CharString_format(alloc, &tempStr, e_rr, "Queue submit semaphore %"PRIu64, k));

			VkDebugUtilsObjectNameInfoEXT debugName2 = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_SEMAPHORE,
				.objectHandle = (U64) *semaphore,
				.pObjectName = tempStr.ptr,
			};

			gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName2), e_rr));

			CharString_free(&tempStr, alloc);
		}
	}

	//Create timeline semaphore

	VkFenceCreateInfo fenceInfo = (VkFenceCreateInfo) { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

	for(U64 i = 0; i < device->framesInFlight; ++i)
		gotoIfError3(clean, checkVkError(
			deviceExt->createFence(deviceExt->device, &fenceInfo, NULL, &deviceExt->commitFence[i]),
			e_rr
		));

	deviceExt->resolvedQueues = resolvedId;

	//Get memory properties

	instanceExt->getPhysicalDeviceMemoryProperties((VkPhysicalDevice) physicalDevice->ext, &deviceExt->memoryProperties);

	//Allocate temp storage for transitions

	gotoIfError3(clean, ListVkBufferMemoryBarrier2_reserve(&deviceExt->bufferTransitions, 17, alloc, e_rr));
	gotoIfError3(clean, ListVkImageMemoryBarrier2_reserve(&deviceExt->imageTransitions, 16, alloc, e_rr));
	gotoIfError3(clean, ListVkImageCopy_reserve(&deviceExt->imageCopyRanges, 8, alloc, e_rr));

	//Alignment rules

	VkPhysicalDeviceProperties2 properties2 = (VkPhysicalDeviceProperties2) {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
	};

	instanceExt->getPhysicalDeviceProperties2((VkPhysicalDevice) device->info.ext, &properties2);

	deviceExt->atomSize = (U8) properties2.properties.limits.nonCoherentAtomSize;
	deviceExt->nonLinearAlignment = (U32) properties2.properties.limits.bufferImageGranularity;
	deviceExt->timestampPeriod = properties2.properties.limits.timestampPeriod;

	//One timestamp query pool per frame in flight, only where the device reports the capability.

	if(device->info.capabilities.features2 & EGraphicsFeatures2_Timestamps) {

		VkQueryPoolCreateInfo queryInfo = (VkQueryPoolCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType = VK_QUERY_TYPE_TIMESTAMP,
			.queryCount = GRAPHICS_TIMESTAMP_QUERIES
		};

		for(U64 i = 0; i < device->framesInFlight; ++i) {
			gotoIfError3(clean, checkVkError(
				deviceExt->createQueryPool(deviceExt->device, &queryInfo, NULL, &deviceExt->timestampPool[i]),
				e_rr
			));
			deviceExt->timestampCapacity[i] = GRAPHICS_TIMESTAMP_QUERIES;
		}
	}

	//DXGI adapter in case there's no other way to query memory

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		LUID luid = (LUID) { 0 };
		Buffer_memcpy(Buffer_createRef(&luid, sizeof(U64)), Buffer_createRefConst(&device->info.luid, sizeof(U64)));

		if(instanceExt->dxgiFactory && (device->info.capabilities.features & EGraphicsFeatures_LUID))
			instanceExt->dxgiFactory->lpVtbl->EnumAdapterByLuid(
				instanceExt->dxgiFactory,
				luid,
				&IID_IDXGIAdapter3,
				(void**) &deviceExt->dxgiAdapter
			);

	#endif

	//Find memory types

	gotoIfError3(clean, VkGraphicsDevice_findAllMemory(deviceExt, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(deviceRef);

	CharString_free(&tempStr, alloc);
	ListConstC8_free(&extensions, alloc);
	ListVkDeviceQueueCreateInfo_free(&queues, alloc);
	ListVkQueueFamilyProperties_free(&queueFamilies, alloc);
	return s_uccess;
}

Bool VkGraphicsDevice_findAllMemory(VkGraphicsDevice *deviceExt, Error *e_rr) {

	Bool s_uccess = true;

	deviceExt->hasDistinctMemory = true;
	deviceExt->hasLocalMemory = true;
	deviceExt->hasOnlyLocalMemory = false;

	for (U32 i = 0; i < deviceExt->memoryProperties.memoryHeapCount; ++i) {

		VkMemoryHeap heap = deviceExt->memoryProperties.memoryHeaps[i];
		heap.flags &= 1;                                                //OOB

		//Ignore 256MB 'BAR' aperture to allow AMD APU to work.
		if (heap.size > deviceExt->maxHeapSizes[heap.flags] && heap.size > 256 * MIBI) {
			deviceExt->maxHeapSizes[heap.flags] = heap.size;
			deviceExt->heapIds[heap.flags] = i;
		}
	}

	if (!deviceExt->maxHeapSizes[0]) {            //If there's only local heaps then we know we're on mobile. Use local heap.
		deviceExt->maxHeapSizes[0] = deviceExt->maxHeapSizes[1];
		deviceExt->heapIds[0] = deviceExt->heapIds[1];
		deviceExt->hasDistinctMemory = false;
		deviceExt->hasOnlyLocalMemory = true;
	}

	else if (!deviceExt->maxHeapSizes[1]) {        //If there's only host heaps then we know we're on AMD APU. Use host heap.
		deviceExt->maxHeapSizes[1] = deviceExt->maxHeapSizes[0];
		deviceExt->heapIds[1] = deviceExt->heapIds[0];
		deviceExt->hasDistinctMemory = false;
		deviceExt->hasLocalMemory = false;
	}

	if (!deviceExt->maxHeapSizes[0] || !deviceExt->maxHeapSizes[1])
		retError(clean, Error_notFound(0, 0, "VkGraphicsDevice_findAllMemory() failed, no heaps found"));

	if(!deviceExt->hasDistinctMemory)
		deviceExt->hasLocalMemory = false;

clean:
	return s_uccess;
}

U64 VK_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(GraphicsDevice *device, Bool isDeviceLocal) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	if(device->info.capabilities.featuresExt & EVkGraphicsFeatures_MemoryBudget) {

		VkPhysicalDeviceMemoryBudgetPropertiesEXT propertiesMemoryBudget = (VkPhysicalDeviceMemoryBudgetPropertiesEXT) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT
		};

		VkPhysicalDeviceMemoryProperties2 properties = (VkPhysicalDeviceMemoryProperties2) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
			.pNext = &propertiesMemoryBudget
		};

		VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);
		instanceExt->getPhysicalDeviceMemoryProperties2((VkPhysicalDevice) device->info.ext, &properties);

		return propertiesMemoryBudget.heapUsage[deviceExt->heapIds[isDeviceLocal]];
	}

	//If on windows, we can actually use a fallback in case the extension isn't present,
	//We can use our DXGI adapter to query instead.

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		if(deviceExt->dxgiAdapter) {

			DXGI_QUERY_VIDEO_MEMORY_INFO vidMem = (DXGI_QUERY_VIDEO_MEMORY_INFO) { 0 };
			HRESULT hr = deviceExt->dxgiAdapter->lpVtbl->QueryVideoMemoryInfo(
				deviceExt->dxgiAdapter,
				0,
				isDeviceLocal ? DXGI_MEMORY_SEGMENT_GROUP_LOCAL : DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,
				&vidMem
			);

			if(FAILED(hr))
				return U64_MAX;

			return vidMem.CurrentUsage;
		}

	#endif

	return U64_MAX;
}

void VK_WRAP_FUNC(GraphicsDevice_free)(const GraphicsInstance *instance, void *ext) {

	if(!instance || !ext)
		return;

	const Allocator *alloc = instance->alloc;

	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);
	VkGraphicsDevice *deviceExt = (VkGraphicsDevice*) ext;

	if(deviceExt->device) {

		for(U64 i = 0; i < deviceExt->commandPools.length; ++i) {

			const VkCommandAllocator vkAlloc = deviceExt->commandPools.ptr[i];

			if(vkAlloc.cmd)
				deviceExt->freeCommandBuffers(deviceExt->device, vkAlloc.pool, 1, &vkAlloc.cmd);

			if(vkAlloc.pool)
				deviceExt->destroyCommandPool(deviceExt->device, vkAlloc.pool, NULL);
		}

		for(U64 i = 0; i < deviceExt->submitSemaphores.length; ++i) {

			const VkSemaphore semaphore = deviceExt->submitSemaphores.ptr[i];

			if(semaphore)
				deviceExt->destroySemaphore(deviceExt->device, semaphore, NULL);
		}

		for(U64 i = 0; i < deviceExt->framesInFlight; ++i)
			if(deviceExt->commitFence[i])
				deviceExt->destroyFence(deviceExt->device, deviceExt->commitFence[i], NULL);

		for(U64 i = 0; i < deviceExt->framesInFlight; ++i)
			if(deviceExt->timestampPool[i])
				deviceExt->destroyQueryPool(deviceExt->device, deviceExt->timestampPool[i], NULL);

		for(U64 i = 0; i < deviceExt->compactionPools.length; ++i)
			deviceExt->destroyQueryPool(deviceExt->device, deviceExt->compactionPools.ptr[i], NULL);

		//Only set when push descriptors were emulated; destroying the pool frees the sets with it.

		if(deviceExt->cbufferPool)
			deviceExt->destroyDescriptorPool(deviceExt->device, deviceExt->cbufferPool, NULL);

		instanceExt->destroyDevice(deviceExt->device, NULL);
	}

	ListVkCommandAllocator_free(&deviceExt->commandPools, alloc);
	ListVkSemaphore_free(&deviceExt->submitSemaphores, alloc);

	//Free temp storage

	ListVkPipelineStageFlags_free(&deviceExt->waitStages, alloc);
	ListVkSemaphore_free(&deviceExt->waitSemaphoresList, alloc);
	for(U64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {

		for(U64 j = 0; j < deviceExt->retiredAs[i].length; ++j)
			deviceExt->destroyAccelerationStructure(deviceExt->device, deviceExt->retiredAs[i].ptr[j], NULL);

		ListVkAccelerationStructureKHR_free(&deviceExt->retiredAs[i], alloc);
	}

	ListVkQueryPool_free(&deviceExt->compactionPools, alloc);
	ListVkResult_free(&deviceExt->results, alloc);
	ListU32_free(&deviceExt->swapchainIndices, alloc);
	ListVkSwapchainKHR_free(&deviceExt->swapchainHandles, alloc);
	ListVkBufferMemoryBarrier2_free(&deviceExt->bufferTransitions, alloc);
	ListVkImageMemoryBarrier2_free(&deviceExt->imageTransitions, alloc);
	ListVkImageCopy_free(&deviceExt->imageCopyRanges, alloc);
	ListVkMappedMemoryRange_free(&deviceExt->mappedMemoryRange, alloc);
	ListVkBufferImageCopy_free(&deviceExt->bufferImageCopyRanges, alloc);
	ListVkBufferCopy_free(&deviceExt->bufferCopies, alloc);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		deviceExt->dxgiAdapter->lpVtbl->Release(deviceExt->dxgiAdapter);
	#endif
}

//Executing commands

Bool VK_WRAP_FUNC(GraphicsDeviceRef_wait)(GraphicsDeviceRef *deviceRef, Error *e_rr) {

	Bool s_uccess = true;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	//vkDeviceWaitIdle has no timeout, so a wedged submit turns the caller into a SILENT forever-hang.
	//A hard deadline would be dishonest in both directions: a slow but correct workload would be failed
	// by a number, and a real wedge would come back as an ordinary error a caller might swallow with the
	// device in an unknown state. So the wait itself never gives up: it runs in one second slices over
	// the pending commit fences and reports the stall while it lasts, which is what lets a pinned CI run
	// name the exact wait, and it fails only on what the driver actually reports, device loss included.
	//VK_TIMEOUT is tested by name, since checkVkError treats every non-negative result as success.
	//Deadline policy stays with the harness that owns the run, not with the runtime.

	VkFence pending[MAX_FRAMES_IN_FLIGHT];
	U32 pendingCount = 0;

	for(U8 i = 0; i < device->framesInFlight; ++i)
		if(deviceExt->commitFencePending[i])
			pending[pendingCount++] = deviceExt->commitFence[i];

	for(U64 waited = 0; pendingCount; ) {

		const VkResult res = deviceExt->waitForFences(deviceExt->device, pendingCount, pending, true, 1 * SECOND);

		if(res != VK_TIMEOUT) {
			gotoIfError3(clean, checkVkError(res, e_rr));
			break;
		}

		++waited;

		if(!(waited % 5))
			Log_performanceLnx(
				"GraphicsDeviceRef_wait() still waiting on the commit fences after %"PRIu64"s, "
				"the device may be wedged", waited
			);
	}

	gotoIfError3(clean, checkVkError(deviceExt->deviceWaitIdle(deviceExt->device), e_rr));

clean:
	return s_uccess;
}

VkCommandAllocator *VkGraphicsDevice_getCommandAllocator(
	VkGraphicsDevice *device,
	U32 resolvedQueueId,
	U64 threadId,
	U8 frameInFlightId,
	U8 fifCount
) {

	const U64 threadCount = Platform_getThreads();

	if(
		!device ||
		resolvedQueueId >= device->resolvedQueues ||
		threadId >= threadCount ||
		frameInFlightId >= fifCount
	)
		return NULL;

	const U64 id = resolvedQueueId + (frameInFlightId * threadCount + threadId) * device->resolvedQueues;

	if(id >= device->commandPools.length)    //This can technically happen if thread count changes at runtime (servers?)
		return NULL;

	return device->commandPools.ptrNonConst + id;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

//Stands in for VK_KHR_push_descriptor on devices that don't have it.
//Only the globals constant buffer is ever pushed, and each frame in flight has its own buffer that lives as long as
// the device, so a set per frame can be written once here and bound unchanged from then on.
//That is what makes the emulation cheap; there is no per frame update and so no risk of writing a set still in flight.
//Nothing has bound these sets yet on the call that creates them, so writing all of them up front is safe.

static Bool VkGraphicsDevice_createCBufferSets(GraphicsDevice *device, VkGraphicsDevice *deviceExt, Error *e_rr) {

	Bool s_uccess = true;

	const U32 count = device->framesInFlight;

	VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorBufferInfo bufferInfo[MAX_FRAMES_IN_FLIGHT];
	VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT];

	const VkDescriptorLayout *cbufferLayout =
		DescriptorLayout_ext(DescriptorLayoutRef_ptr(device->defaultCBufferLayout), Vk);

	if(!cbufferLayout || !cbufferLayout->layouts[0])
		retError(clean, Error_invalidState(0, "VkGraphicsDevice_createCBufferSets() missing constant buffer layout"));

	for(U32 i = 0; i < count; ++i)
		layouts[i] = cbufferLayout->layouts[0];

	const VkDescriptorPoolSize poolSize = (VkDescriptorPoolSize) {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = count
	};

	const VkDescriptorPoolCreateInfo poolInfo = (VkDescriptorPoolCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = count,
		.poolSizeCount = 1,
		.pPoolSizes = &poolSize
	};

	gotoIfError3(clean, checkVkError(
		deviceExt->createDescriptorPool(deviceExt->device, &poolInfo, NULL, &deviceExt->cbufferPool), e_rr
	));

	const VkDescriptorSetAllocateInfo allocInfo = (VkDescriptorSetAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = deviceExt->cbufferPool,
		.descriptorSetCount = count,
		.pSetLayouts = layouts
	};

	gotoIfError3(clean, checkVkError(
		deviceExt->allocateDescriptorSets(deviceExt->device, &allocInfo, deviceExt->cbufferSets), e_rr
	));

	for (U32 i = 0; i < count; ++i) {

		DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[i]);

		bufferInfo[i] = (VkDescriptorBufferInfo) {
			.buffer = DeviceBuffer_ext(frameData, Vk)->buffer,
			.offset = 0,
			.range = frameData->resource.size
		};

		writes[i] = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = deviceExt->cbufferSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &bufferInfo[i]
		};
	}

	deviceExt->updateDescriptorSets(deviceExt->device, count, writes, 0, NULL);

clean:

	//The pool doubles as the "already emulated" marker, so a half built one would be skipped on the next submit
	// and leave unallocated sets to be bound.

	if (!s_uccess && deviceExt->cbufferPool) {
		deviceExt->destroyDescriptorPool(deviceExt->device, deviceExt->cbufferPool, NULL);
		deviceExt->cbufferPool = NULL;
	}

	return s_uccess;
}

Bool GraphicsDevice_rebindDescriptors(GraphicsDevice *device, VkCommandBuffer commandBuffer, Error *e_rr) {

	Bool s_uccess = true;

	//Without bindless there's no default pipeline layout, table or push descriptor set to bind.
	//Every pipeline brings its own layout in that case, so there's nothing to do per frame.

	if(!device->defaultPipelineLayout || !device->defaultDescriptorTable)
		return true;

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	//A device without the extension leaves cmdPushDescriptorSet NULL, so the sets are built on first submit.
	//They can't be built at device creation because the globals buffers don't exist yet at that point.

	if(!deviceExt->cmdPushDescriptorSet && !deviceExt->cbufferPool) {

		Log_performanceLnx(
			"Vulkan: VK_KHR_push_descriptor is unavailable, emulating it with one descriptor set per frame in flight"
		);

		gotoIfError3(clean, VkGraphicsDevice_createCBufferSets(device, deviceExt, e_rr));
	}

	U64 bindingCount = device->info.capabilities.features & EGraphicsFeatures_RayPipeline ? 3 : 2;

	VkPipelineLayout *defaultLayoutExt = PipelineLayout_ext(PipelineLayoutRef_ptr(device->defaultPipelineLayout), Vk);

	VkDescriptorTable *table = DescriptorTable_ext(DescriptorTableRef_ptr(device->defaultDescriptorTable), Vk);

	for(U64 i = 0; i < bindingCount; ++i) {

		VkPipelineBindPoint bindPoint = i == 0 ? VK_PIPELINE_BIND_POINT_COMPUTE : (
			i == 1 ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
		);

		for(U64 j = 0, k = 0; j < table->bindCommands; ++j) {

			deviceExt->cmdBindDescriptorSets(
				commandBuffer,
				bindPoint,
				*defaultLayoutExt,
				table->offsets[j], table->counts[j], &table->sets[k],
				0, NULL
			);

			k += table->counts[j];
		}

		//The emulated path binds the set that was already written for this frame, so it costs one bind either way.

		if (!deviceExt->cmdPushDescriptorSet) {

			deviceExt->cmdBindDescriptorSets(
				commandBuffer,
				bindPoint,
				*defaultLayoutExt,
				2, 1, &deviceExt->cbufferSets[device->fifId],
				0, NULL
			);

			continue;
		}

		DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[device->fifId]);
		VkDeviceBuffer *frameDataExt = DeviceBuffer_ext(frameData, Vk);

		VkDescriptorBufferInfo bufferInfo = (VkDescriptorBufferInfo) {
			.buffer = frameDataExt->buffer,
			.offset = 0,
			.range = frameData->resource.size
		};

		VkWriteDescriptorSet cbv = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &bufferInfo
		};

		deviceExt->cmdPushDescriptorSet(
			commandBuffer,
			bindPoint,
			*defaultLayoutExt,
			2,
			1,
			&cbv
		);
	}

clean:
	return s_uccess;
}

Bool VK_WRAP_FUNC(GraphicsDevice_submitCommands)(
	GraphicsDeviceRef *deviceRef,
	const ListCommandListRef *commandLists,
	const ListSwapchainRef *swapchains,
	CBufferData *cbufferData,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	//Unpack ext

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	CharString temp = CharString_createNull();

	//Reserve temp storage

	gotoIfError3(clean, ListVkSwapchainKHR_clear(&deviceExt->swapchainHandles, e_rr));
	gotoIfError3(clean, ListVkSwapchainKHR_reserve(
		&deviceExt->swapchainHandles, !swapchains ? 0 : swapchains->length, alloc, e_rr
	));

	gotoIfError3(clean, ListU32_clear(&deviceExt->swapchainIndices, e_rr));
	gotoIfError3(clean, ListU32_reserve(&deviceExt->swapchainIndices, !swapchains ? 0 : swapchains->length, alloc, e_rr));

	gotoIfError3(clean, ListVkResult_clear(&deviceExt->results, e_rr));
	gotoIfError3(clean, ListVkResult_resize(&deviceExt->results, !swapchains ? 0 : swapchains->length, alloc, e_rr));

	gotoIfError3(clean, ListVkSemaphore_clear(&deviceExt->waitSemaphoresList, e_rr));
	gotoIfError3(clean, ListVkSemaphore_reserve(
		&deviceExt->waitSemaphoresList, (!swapchains ? 0 : swapchains->length) + 1, alloc, e_rr
	));

	gotoIfError3(clean, ListVkPipelineStageFlags_clear(&deviceExt->waitStages, e_rr));
	gotoIfError3(clean, ListVkPipelineStageFlags_reserve(
		&deviceExt->waitStages, (!swapchains ? 0 : swapchains->length) + 1, alloc, e_rr
	));

	//Wait for previous frame semaphore

	VkFence *fence = &deviceExt->commitFence[device->fifId];

	//Only wait when a submit is actually pending on this fence.
	//If the previous submit at this fifId failed the fence was left unsignaled, see commitFencePending.
	//Waiting would then just burn the full timeout on something nothing will signal.
	//So skip straight to reusing the unsignaled fence.

	if (device->submitId > device->framesInFlight && deviceExt->commitFencePending[device->fifId]) {

		gotoIfError3(clean, checkVkError(deviceExt->waitForFences(
			deviceExt->device,
			1, fence,
			true,
			1 * SECOND
		), e_rr));

		gotoIfError3(clean, checkVkError(deviceExt->resetFences(deviceExt->device, 1, fence), e_rr));

		//That fence also proves any compaction copy recorded in this slot has run, so the structures it
		// replaced can go. Their buffers ride resourcesInFlight; only the handles are left to us.

		ListVkAccelerationStructureKHR *retired = &deviceExt->retiredAs[device->fifId];

		for(U64 i = 0; i < retired->length; ++i)
			deviceExt->destroyAccelerationStructure(deviceExt->device, retired->ptr[i], NULL);

		gotoIfError3(clean, ListVkAccelerationStructureKHR_clear(retired, e_rr));
	}

	//Read back and resolve the timestamps of the frame that used this slot framesInFlight submits ago, now that its
	// fence has proven it done. A host read, no GPU copy: the fence is the barrier. Best effort, since a failed read
	// back only costs a frame of timings, never the submit.

	if(
		(device->info.capabilities.features2 & EGraphicsFeatures2_Timestamps) &&
		device->submitId > device->framesInFlight && deviceExt->commitFencePending[device->fifId] &&
		device->timingSlots[device->fifId]
	) {
		const U32 slots = device->timingSlots[device->fifId];
		Buffer ticks = Buffer_createNull();

		if(Buffer_createUninitializedBytes(slots * sizeof(U64), alloc, &ticks, NULL)) {

			const VkResult qr = deviceExt->getQueryPoolResults(
				deviceExt->device, deviceExt->timestampPool[device->fifId], 0, slots,
				slots * sizeof(U64), ticks.ptrNonConst, sizeof(U64), VK_QUERY_RESULT_64_BIT
			);

			if(qr == VK_SUCCESS) {

				const U32 vb = deviceExt->timestampValidBits;
				const U64 mask = vb >= 64 ? U64_MAX : (((U64) 1 << vb) - 1);
				U64 *t = (U64*) ticks.ptrNonConst;

				for(U32 k = 0; k < slots; ++k)
					t[k] &= mask;

				GraphicsDevice_resolveTimings(device, device->fifId, t, slots, deviceExt->timestampPeriod, alloc, NULL);
			}

			Buffer_free(&ticks, alloc);
		}
	}

	//Acquire swapchain images

	for(U64 i = 0; i < (!swapchains ? 0 : swapchains->length); ++i) {

		Swapchain *swapchain = SwapchainRef_ptr(swapchains->ptr[i]);
		UnifiedTexture *unifiedTexture = TextureRef_getUnifiedTextureIntern(swapchains->ptr[i], NULL);

		//A swapchain that owns its images has no presentation engine to acquire from and nothing to present to,
		// so the next image is simply the next one in the ring and no semaphore has to be waited on.
		//It is left out of the present lists entirely,
		// which is what keeps a frame with only these from presenting at all.

		if(swapchain->base.resource.flags & EGraphicsResourceFlag_InternalOwnsImages) {
			unifiedTexture->currentImageId = (U8) ((unifiedTexture->currentImageId + 1) % unifiedTexture->images);
			continue;
		}

		VkSwapchain *swapchainExt = TextureRef_getImplExtT(VkSwapchain, swapchains->ptr[i]);

		VkSemaphore semaphore = swapchainExt->semaphores.ptr[device->fifId];

		U32 currImg = 0;

		gotoIfError3(clean, checkVkError(deviceExt->acquireNextImage(
			deviceExt->device,
			swapchainExt->swapchain,
			1 * SECOND,
			semaphore,
			VK_NULL_HANDLE,
			&currImg
		), e_rr));

		unifiedTexture->currentImageId = (U8) currImg;

		//Pushed rather than written at i, since the virtual ones above leave gaps the present lists must not carry.

		gotoIfError3(clean, ListVkSwapchainKHR_pushBack(&deviceExt->swapchainHandles, swapchainExt->swapchain, alloc, e_rr));
		gotoIfError3(clean, ListU32_pushBack(&deviceExt->swapchainIndices, unifiedTexture->currentImageId, alloc, e_rr));

		VkPipelineStageFlagBits pipelineStage =
			(swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0) |
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_TRANSFER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		gotoIfError3(clean, ListVkSemaphore_pushBack(&deviceExt->waitSemaphoresList, semaphore, alloc, e_rr));
		gotoIfError3(clean, ListVkPipelineStageFlags_pushBack(&deviceExt->waitStages, pipelineStage, alloc, e_rr));
	}

	//Prepare per frame cbuffer

	{
		DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[device->fifId]);

		for (U32 i = 0; i < (!swapchains ? 0 : swapchains->length); ++i) {

			SwapchainRef *swapchainRef = swapchains->ptr[i];
			Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);

			Bool allowComputeExt = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

			UnifiedTextureImage managedImage = TextureRef_getCurrImage(swapchainRef, 0);

			if(cbufferData) {
				cbufferData->swapchains[i * 2 + 0] = managedImage.readHandle;
				cbufferData->swapchains[i * 2 + 1] = allowComputeExt ? managedImage.writeHandle : 0;
			}
		}

		if(cbufferData)
			*(CBufferData*)frameData->resource.mappedMemoryExt = *cbufferData;

		DeviceMemoryBlock block = device->allocator.blocks.ptr[frameData->resource.blockId];
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (incoherent) {

			VkMappedMemoryRange range = (VkMappedMemoryRange){
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = (VkDeviceMemory)block.ext,
				.offset = frameData->resource.blockOffset,
				.size = sizeof(CBufferData)
			};

			gotoIfError3(clean, checkVkError(deviceExt->flushMappedMemoryRanges(deviceExt->device, 1, &range), e_rr));
		}
	}

	//Record command list

	VkCommandBuffer commandBuffer = NULL;

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	U32 graphicsQueueId = queue.queueId;

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	if (commandLists && commandLists->length) {

		U32 threadId = 0;

		VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, device->fifId, device->framesInFlight
		);

		if(!allocator)
			retError(clean, Error_nullPointer(0, "VkGraphicsDevice_submitCommands() command allocator is NULL"));

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

			gotoIfError3(clean, checkVkError(
				deviceExt->createCommandPool(deviceExt->device, &poolInfo, NULL, &allocator->pool),
				e_rr
			));

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				gotoIfError3(clean, CharString_format(
					alloc, &temp, e_rr,
					"%s command pool (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
						queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId, device->fifId
				));

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_COMMAND_POOL,
					.pObjectName = temp.ptr,
					.objectHandle = (U64) allocator->pool
				};

				gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));

				CharString_free(&temp, alloc);
			}
		}

		else gotoIfError3(clean, checkVkError(deviceExt->resetCommandPool(
				deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
			), e_rr
		));

		//Allocate command buffer if not present yet

		if (!allocator->cmd) {

			VkCommandBufferAllocateInfo bufferInfo = (VkCommandBufferAllocateInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = allocator->pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			gotoIfError3(clean, checkVkError(
				deviceExt->allocateCommandBuffers(deviceExt->device, &bufferInfo, &allocator->cmd),
				e_rr
			));

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				gotoIfError3(clean, CharString_format(
					alloc, &temp, e_rr,
					"%s command buffer (thread: %"PRIu32", frame id: %"PRIu32")",
					queue.type == EVkCommandQueue_Graphics ? "Graphics" : (
						queue.type == EVkCommandQueue_Compute ? "Compute" : "Copy"
					),
					threadId, device->fifId
				));

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
					.pObjectName = temp.ptr,
					.objectHandle = (U64) allocator->cmd
				};

				gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));

				CharString_free(&temp, alloc);
			}
		}

		//Start buffer

		commandBuffer = allocator->cmd;

		VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		};

		gotoIfError3(clean, checkVkError(deviceExt->beginCommandBuffer(commandBuffer, &beginInfo), e_rr));

		//Start copies

		VkCommandBufferState state = (VkCommandBufferState) { .buffer = commandBuffer };
		gotoIfError3(clean, GraphicsDeviceRef_handleNextFrame(deviceRef, &state, e_rr));

		//Build this frame's timing entries and reset the pool it will write into. buildTimings clears the previous
		// frame's entries at this slot, so it must run after the resolve above has read them.

		device->timingSlots[device->fifId] = (device->info.capabilities.features2 & EGraphicsFeatures2_Timestamps)
			? GraphicsDevice_buildTimings(device, device->fifId, commandLists, alloc) : 0;

		deviceExt->timestampCursor = 0;

		//Grow the pool when a frame needs more slots than it currently holds; buildTimings capped total at the ceiling,
		// so the doubling converges. A pool is recreated only when a frame passes the high-water mark, so it happens
		// once and never for a caller that stays under the initial capacity.

		if(device->timingSlots[device->fifId] > deviceExt->timestampCapacity[device->fifId]) {

			U32 newCap = deviceExt->timestampCapacity[device->fifId] ? deviceExt->timestampCapacity[device->fifId] : 1;

			while(newCap < device->timingSlots[device->fifId])
				newCap <<= 1;

			if(deviceExt->timestampPool[device->fifId])
				deviceExt->destroyQueryPool(deviceExt->device, deviceExt->timestampPool[device->fifId], NULL);

			VkQueryPoolCreateInfo growInfo = (VkQueryPoolCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
				.queryType = VK_QUERY_TYPE_TIMESTAMP,
				.queryCount = newCap
			};

			if(deviceExt->createQueryPool(deviceExt->device, &growInfo, NULL, &deviceExt->timestampPool[device->fifId]) == VK_SUCCESS)
				deviceExt->timestampCapacity[device->fifId] = newCap;

			else {
				deviceExt->timestampPool[device->fifId] = NULL;
				deviceExt->timestampCapacity[device->fifId] = 0;
				device->timingSlots[device->fifId] = 0;
			}
		}

		if(device->timingSlots[device->fifId])
			deviceExt->cmdResetQueryPool(
				commandBuffer, deviceExt->timestampPool[device->fifId], 0, device->timingSlots[device->fifId]
			);

		//Ensure ubo and staging buffer are the correct states

		VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

		VkDeviceBuffer *uboExt = DeviceBuffer_ext(DeviceBufferRef_ptr(device->frameData[device->fifId]), Vk);

		gotoIfError3(clean, VkDeviceBuffer_transition(
			uboExt,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_UNIFORM_READ_BIT,
			graphicsQueueId,
			0,
			0,
			&deviceExt->bufferTransitions,
			&dependency, alloc, e_rr
		));

		if(dependency.bufferMemoryBarrierCount)
			deviceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions, e_rr);

		//Descriptors bind lazily at the first work op that needs them (bindful), so a purely bindful frame
		// never pays for the default set setup.

		//Record commands

		for (U64 i = 0; i < (!commandLists ? 0 : commandLists->length); ++i) {

			state.scopeCounter = 0;
			CommandList *commandList = CommandListRef_ptr(commandLists->ptr[i]);
			const U8 *ptr = commandList->data.ptr;

			for (U64 j = 0; j < commandList->commandOps.length; ++j) {
				CommandOpInfo info = commandList->commandOps.ptr[j];
				(VK_WRAP_FUNC(CommandList_process))(commandList, deviceRef, info.op, ptr, &state);
				ptr += info.opSize;
			}
		}

		//Readbacks are recorded after the frame's commands so they observe this frame's results

		gotoIfError3(clean, GraphicsDeviceRef_flushPendingPulls(deviceRef, &state, e_rr));

		//Transition back swapchains to present

		//Combine transitions into one call.

		dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		for (U64 i = 0; i < (!swapchains ? 0 : swapchains->length); ++i) {

			SwapchainRef *swapchainRef = swapchains->ptr[i];

			//PRESENT_SRC is meaningless for a swapchain that owns its images: nothing presents it,
			// and whatever reads it next transitions it the way it would any other render target.
			//It is still tracked in flight, since its images are as much in use as a presented one's.

			if(!(SwapchainRef_ptr(swapchainRef)->base.resource.flags & EGraphicsResourceFlag_InternalOwnsImages)) {

				VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(swapchainRef, Vk, 0);

				VkImageSubresourceRange range = (VkImageSubresourceRange) {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				};

				gotoIfError3(clean, VkUnifiedTexture_transition(
					imageExt,
					VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
					0,
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					graphicsQueueId,
					&range,
					&deviceExt->imageTransitions,
					&dependency, alloc, e_rr
				));
			}

			if(RefPtr_inc(swapchainRef))
				gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, swapchainRef, alloc, e_rr));
		}

		if(dependency.imageMemoryBarrierCount)
			deviceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

		ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions, e_rr);

		//End buffer

		gotoIfError3(clean, checkVkError(deviceExt->endCommandBuffer(commandBuffer), e_rr));
	}

	//Submit queue
	//TODO: Multiple queues

	VkSemaphore signalSemaphores = deviceExt->submitSemaphores.ptr[device->fifId];

	VkSubmitInfo submitInfo = (VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = (U32) deviceExt->waitSemaphoresList.length,
		.pWaitSemaphores = deviceExt->waitSemaphoresList.ptr,
		//Keyed on what will actually be PRESENTED, rather than on how many swapchains were submitted.
		//A binary semaphore signalled with nothing waiting on it stays signalled,
		// and signalling it again next frame is invalid,
		// so a frame whose swapchains all own their images must not signal one at all.

		.signalSemaphoreCount = (Bool) deviceExt->swapchainHandles.length,
		.pSignalSemaphores = swapchains && swapchains->length ? &signalSemaphores : NULL,
		.pCommandBuffers = &commandBuffer,
		.commandBufferCount = commandBuffer ? 1 : 0,
		.pWaitDstStageMask = deviceExt->waitStages.ptr
	};

	//Record whether the fence now has a submit pending on it BEFORE propagating any error: a failed submit
	//leaves it unsignaled, and the next frame at this fifId must know not to wait on it.

	VkResult submitRes = deviceExt->queueSubmit(queue.queue, 1, &submitInfo, *fence);
	deviceExt->commitFencePending[device->fifId] = submitRes == VK_SUCCESS;
	gotoIfError3(clean, checkVkError(submitRes, e_rr));

	//Presents

	//Only the swapchains the acquire above pushed, which is the physical ones.
	//A frame that drove nothing but swapchains owning their images presents nothing and ends in memory.

	if(deviceExt->swapchainHandles.length) {

		VkPresentInfoKHR presentInfo = (VkPresentInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &signalSemaphores,
			.swapchainCount = (U32) deviceExt->swapchainHandles.length,
			.pSwapchains = deviceExt->swapchainHandles.ptr,
			.pImageIndices = deviceExt->swapchainIndices.ptr,
			.pResults = deviceExt->results.ptrNonConst
		};

		gotoIfError3(clean, checkVkError(deviceExt->queuePresentKHR(queue.queue, &presentInfo), e_rr));

		//The results follow the PUSHED order, so walking the caller's list needs the same skip to stay in step.

		for(U64 i = 0, j = 0; i < (!swapchains ? 0 : swapchains->length); ++i) {

			SwapchainRef *swapchainRef = swapchains->ptr[i];
			Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);

			if(swapchain->base.resource.flags & EGraphicsResourceFlag_InternalOwnsImages)
				continue;

			const VkResult res = deviceExt->results.ptr[j];
			++j;

			gotoIfError3(clean, checkVkError(res, e_rr));

			if(res == VK_SUBOPTIMAL_KHR) {

				Window *window = swapchain->info.window;

				if(window)
					window->requireResize = true;
			}
		}
	}

clean:

	ListVkImageCopy_clear(&deviceExt->imageCopyRanges, e_rr);
	ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions, e_rr);
	ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions, e_rr);
	CharString_free(&temp, alloc);

	return s_uccess;
}

Bool VkGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, VkCommandBufferState *commandBuffer, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	if(GraphicsDevice_logOnce(device, EGraphicsDeviceMessage_SubmitFlushed))
		Log_performanceLnx(
			"Vulkan: submit was split mid recording because pending copies or AS builds crossed the flush "
			"threshold, which adds a GPU sync point; raise flushThreshold or batch smaller uploads "
			"(only logged once)"
		);

	//End current command list

	gotoIfError3(clean, checkVkError(deviceExt->endCommandBuffer(commandBuffer->buffer), e_rr));

	//Submit only the copy command list

	const VkSubmitInfo submitInfo = (VkSubmitInfo) {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pCommandBuffers = &commandBuffer->buffer,
		.commandBufferCount = 1
	};

	const VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	gotoIfError3(clean, checkVkError(deviceExt->queueSubmit(
		queue.queue,
		1, &submitInfo,
		deviceExt->commitFence[device->fifId]
	), e_rr));

	//Wait for the device

	gotoIfError3(clean, GraphicsDeviceRef_wait(deviceRef, e_rr));

	//The flush borrowed this frame's commit fence and waited on it through deviceWaitIdle, so it is now signaled.
	//Reset it, otherwise the frame's own vkQueueSubmit below would be handed an already-signaled fence, which is invalid.
	//The pending flag stays false until that real submit sets it.

	gotoIfError3(clean, checkVkError(
		deviceExt->resetFences(deviceExt->device, 1, &deviceExt->commitFence[device->fifId]), e_rr
	));
	deviceExt->commitFencePending[device->fifId] = false;

	//Reset command list

	const U32 threadId = 0;

	const VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
		deviceExt, queue.resolvedQueueId, threadId, device->fifId, device->framesInFlight
	);

	gotoIfError3(clean, checkVkError(deviceExt->resetCommandPool(
		deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
	), e_rr));

	//Re-open

	const VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	gotoIfError3(clean, checkVkError(deviceExt->beginCommandBuffer(commandBuffer->buffer, &beginInfo), e_rr));

	//The fresh buffer has no descriptor state, so the emitted state trackers reset and the next work op's
	// lazy bind re-emits whatever was last bound (default or custom), rather than eagerly binding defaults
	// the remaining commands might never use.

	commandBuffer->defaultDescriptorsBound = false;

	for(U8 bindfulI = 0; bindfulI < 3; ++bindfulI) {
		commandBuffer->lastBoundTable[bindfulI] = NULL;
		commandBuffer->lastBoundLayout[bindfulI] = VK_NULL_HANDLE;
		commandBuffer->lastPushLayout[bindfulI] = VK_NULL_HANDLE;
		commandBuffer->pushConstantsEmitted[bindfulI] = false;
		commandBuffer->lastPushDescLayout[bindfulI] = VK_NULL_HANDLE;
		commandBuffer->pushDescriptorsEmitted[bindfulI] = false;
	}

	//Reset temporary variables to avoid invalid caching behavior

	for (U64 i = 0; i < EPipelineType_Count; ++i)
		commandBuffer->pipelines[i] = NULL;

	commandBuffer->boundScissor = (VkRect2D) { 0 };
	commandBuffer->boundViewport = (VkViewport) { 0 };
	commandBuffer->boundBuffers = (SetPrimitiveBuffersCmd) { 0 };
	commandBuffer->stencilRef = 0;
	commandBuffer->blendConstants = F32x4_zero();

clean:
	return s_uccess;
}
