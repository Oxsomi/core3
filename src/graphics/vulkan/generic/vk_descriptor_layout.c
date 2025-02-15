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
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "types/container/string.h"
#include "platforms/ext/stringx.h"
#include "formats/oiSH/entries.h"

Bool VK_WRAP_FUNC(DescriptorLayout_free)(DescriptorLayout *layout) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(layout->device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	const VkDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Vk);

	for(U64 i = 0; i < 4; ++i)
		if(layoutExt->layouts[i])
			deviceExt->destroyDescriptorSetLayout(deviceExt->device, layoutExt->layouts[i], NULL);

	return true;
}

VkDescriptorType vkGetDescriptorType(ESHRegisterType regType) {
	
	switch (regType & ESHRegisterType_TypeMask) {
		
		case ESHRegisterType_Sampler:
		case ESHRegisterType_SamplerComparisonState:
			return !!(regType & ESHRegisterType_IsCombinedSampler) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
				VK_DESCRIPTOR_TYPE_SAMPLER;

		case ESHRegisterType_ByteAddressBuffer:
		case ESHRegisterType_StructuredBuffer:
		case ESHRegisterType_StructuredBufferAtomic:
		case ESHRegisterType_StorageBuffer:
		case ESHRegisterType_StorageBufferAtomic:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

		case ESHRegisterType_ConstantBuffer:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case ESHRegisterType_AccelerationStructure:		return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		case ESHRegisterType_SubpassInput:				return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

		default:
		case ESHRegisterType_Texture1D:
		case ESHRegisterType_Texture2D:
		case ESHRegisterType_Texture3D:
		case ESHRegisterType_TextureCube:
		case ESHRegisterType_Texture2DMS:
			return !!(regType & ESHRegisterType_IsWrite) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	}
}

TList(VkDescriptorSetLayoutBinding);
TListImpl(VkDescriptorSetLayoutBinding);

TList(VkDescriptorBindingFlags);
TListImpl(VkDescriptorBindingFlags);

Error VK_WRAP_FUNC(GraphicsDeviceRef_createDescriptorLayout)(
	GraphicsDeviceRef *dev,
	DescriptorLayout *layout,
	CharString name
) {

	Error err = Error_none();
	CharString tmpName = CharString_createNull();
	ListU64 sortedList = (ListU64) { 0 };
	ListVkDescriptorSetLayoutBinding bindings = (ListVkDescriptorSetLayoutBinding) { 0 };
	ListVkDescriptorBindingFlags flags = (ListVkDescriptorBindingFlags) { 0 };

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Vk);

	const DescriptorLayoutInfo info = layout->info;
	Bool anyBindless = !!(info.flags & EDescriptorLayoutFlags_AllowBindlessAny);

	//Basic setup

	VkDescriptorSetLayoutBindingFlagsCreateInfo partiallyBound[4] = {
		(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO
		}
	};

	VkDescriptorSetLayoutCreateInfo setInfo[4] = {
		(VkDescriptorSetLayoutCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
		}
	};

	for(U64 i = 1; i < 4; ++i) {
		setInfo[i] = setInfo[0];
		partiallyBound[i] = partiallyBound[0];
	}

	gotoIfError(clean, ListU64_reservex(&sortedList, info.bindings.length))
	gotoIfError(clean, ListVkDescriptorSetLayoutBinding_resizex(&bindings, info.bindings.length))

	if(anyBindless)
		gotoIfError(clean, ListVkDescriptorBindingFlags_resizex(&flags, info.bindings.length))

	U8 uniqueSetCounter = 0, isBindlessSet = 0;
	U32 sets[4];

	//Sort by set and merge shaders that allow it and check we only have 4 sets bound

	for(U32 i = 0; i < (U32) info.bindings.length; ++i) {

		gotoIfError(clean, ListU64_pushBack(&sortedList, ((U64)info.bindings.ptr[i].space << 32) | i, (Allocator) { 0 }))

		//Make sure the set is registered to avoid going over 4 sets

		DescriptorBinding binding = info.bindings.ptr[i];

		U8 j = 0;

		for(; j < uniqueSetCounter; ++j)
			if(sets[j] == binding.space)
				break;
			
		if (j == uniqueSetCounter) {

			if(uniqueSetCounter == 4)
				gotoIfError(clean, Error_outOfBounds(
					0, 4, 4, "GraphicsDeviceRef_createDescriptorLayout can only have 4 unique descriptor sets bound at once"
				))
			
			sets[uniqueSetCounter] = binding.space;
			++uniqueSetCounter;
		}

		if(
			!!(info.flags & EDescriptorLayoutFlags_AllowBindlessEverywhere) ||
			(!!(info.flags & EDescriptorLayoutFlags_AllowBindlessOnArrays) && binding.count > 1)
		)
			isBindlessSet |= 1 << j;
	}

	if(!ListU64_sort(sortedList))
		gotoIfError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createDescriptorLayout can't sort list"
		))

	//Create our sets and bindings

	U8 setPresent = 0;

	for (U64 i = 0; i < sortedList.length; ++i) {

		U64 key = sortedList.ptr[i];
		const DescriptorBinding *binding = &info.bindings.ptr[(U32) key];

		U32 count = binding->count;
		U32 vis = binding->visibility;

		VkShaderStageFlags stageFlags = 0;

		if(!!((vis >> ESHPipelineStage_Vertex) & 1))
			stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		if(!!((vis >> ESHPipelineStage_Pixel) & 1))
			stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		if(!!((vis >> ESHPipelineStage_Compute) & 1))
			stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		if(!!((vis >> ESHPipelineStage_GeometryExt) & 1))
			stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;

		if(!!((vis >> ESHPipelineStage_Hull) & 1))
			stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

		if(!!((vis >> ESHPipelineStage_Domain) & 1))
			stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

		if(!!((vis >> ESHPipelineStage_RaygenExt) & 1))
			stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		if(!!((vis >> ESHPipelineStage_CallableExt) & 1))
			stageFlags = VK_SHADER_STAGE_CALLABLE_BIT_KHR;

		if(!!((vis >> ESHPipelineStage_MissExt) & 1))
			stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;

		if(!!((vis >> ESHPipelineStage_ClosestHitExt) & 1))
			stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		if(!!((vis >> ESHPipelineStage_AnyHitExt) & 1))
			stageFlags = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

		if(!!((vis >> ESHPipelineStage_IntersectionExt) & 1))
			stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

		if(!!((vis >> ESHPipelineStage_MeshExt) & 1))
			stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;

		if(!!((vis >> ESHPipelineStage_TaskExt) & 1))
			stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;

		VkDescriptorType type = vkGetDescriptorType(binding->registerType);

		bindings.ptrNonConst[i] = (VkDescriptorSetLayoutBinding) {
			.binding = binding->id,
			.descriptorCount = count,
			.descriptorType = type,
			.stageFlags = stageFlags
		};

		VkDescriptorBindingFlags bindFlags =
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

		if(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			bindFlags &=~ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		if(
			!(info.flags & EDescriptorLayoutFlags_AllowBindlessEverywhere) &&
			!(!!(info.flags & EDescriptorLayoutFlags_AllowBindlessOnArrays) && binding->count > 1)
		)
			bindFlags = 0;

		flags.ptrNonConst[i] = bindFlags;

		//Link set

		U8 linkId = 0;

		for(U8 j = 1; j < 4; ++j)
			if (sets[j] == binding->space) {
				linkId = j;
				break;
			}

		if (!((setPresent >> linkId) & 1)) {

			setInfo[linkId].pBindings = &bindings.ptr[i];
			partiallyBound[linkId].pBindingFlags = &flags.ptr[i];

			if (!!((isBindlessSet >> linkId) & 1)) {
				setInfo[linkId].pNext = &partiallyBound[linkId];
				setInfo[linkId].flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
			}

			setPresent |= 1 << linkId;
		}

		++setInfo[linkId].bindingCount;
		++partiallyBound[linkId].bindingCount;
	}

	for(U8 i = 0; i < 4; ++i) {

		if(!setInfo[i].pBindings)
			continue;

		gotoIfError(clean, checkVkError(deviceExt->createDescriptorSetLayout(
			deviceExt->device, &setInfo[i], NULL, &layoutExt->layouts[i]
		)))

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name) && instanceExt->debugSetName) {

			gotoIfError(clean, CharString_formatx(&tmpName, "%.*s set %"PRIu8, (int) CharString_length(name), name.ptr, i))

			const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL,
				.pObjectName = tmpName.ptr,
				.objectHandle = (U64) layoutExt->layouts[i]
			};

			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
			CharString_freex(&tmpName);
		}
	}

clean:
	ListVkDescriptorSetLayoutBinding_freex(&bindings);
	ListVkDescriptorBindingFlags_freex(&flags);
	ListU64_freex(&sortedList);
	CharString_freex(&tmpName);
	return err;
}
