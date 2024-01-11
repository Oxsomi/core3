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

#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_sampler.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "types/string.h"

const U64 SamplerExt_size = sizeof(VkManagedSampler);

Bool Sampler_freeExt(Sampler *sampler) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(sampler->device), Vk);
	VkManagedSampler *samplerExt = Sampler_ext(sampler, VkManaged);

	if(sampler->samplerLocation != U32_MAX) {

		ELockAcquire acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {
			ListU32 allocationList = (ListU32) { 0 };
			ListU32_createRefConst(&sampler->samplerLocation, 1, &allocationList);
			VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);
			sampler->samplerLocation = U32_MAX;
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);
	}

	if(samplerExt->sampler)
		vkDestroySampler(deviceExt->device, samplerExt->sampler, NULL);

	return true;
}

VkSamplerAddressMode mapVkAddressMode(ESamplerAddressMode addressMode) {
	switch (addressMode) {
		case ESamplerAddressMode_MirrorRepeat:		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case ESamplerAddressMode_ClampToEdge:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case ESamplerAddressMode_ClampToBorder:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default:									return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

VkBorderColor mapVkBorderColor(ESamplerBorderColor borderColor) {
	switch (borderColor) {
		case ESamplerBorderColor_OpaqueBlackFloat:		return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		case ESamplerBorderColor_OpaqueBlackInt:		return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		case ESamplerBorderColor_OpaqueWhiteFloat:		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		case ESamplerBorderColor_OpaqueWhiteInt:		return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		default:										return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	}
}

Error GraphicsDeviceRef_createSamplerExt(GraphicsDeviceRef *dev, Sampler *sampler, CharString name) {

	Error err = Error_none();

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);
	instance;

	VkManagedSampler *samplerExt = Sampler_ext(sampler, VkManaged);

	SamplerInfo sinfo = sampler->info;
	ELockAcquire acq = ELockAcquire_Invalid;

	VkSamplerCreateInfo samplerInfo = (VkSamplerCreateInfo) {

		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = sinfo.filter & ESamplerFilterMode_LinearMag ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
		.minFilter = sinfo.filter & ESamplerFilterMode_LinearMin ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,

		.mipmapMode = 
			sinfo.filter & ESamplerFilterMode_LinearMip ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,

		.addressModeU = mapVkAddressMode(sinfo.addressU),
		.addressModeV = mapVkAddressMode(sinfo.addressV),
		.addressModeW = mapVkAddressMode(sinfo.addressW),
		.mipLodBias = sinfo.mipBias,
		.anisotropyEnable = (Bool) sinfo.aniso,
		.maxAnisotropy = sinfo.aniso,
		.compareEnable = sinfo.enableComparison,
		.compareOp = mapVkCompareOp(sinfo.comparisonFunction),
		.minLod = F16_castF32(sinfo.minLod),
		.maxLod = F16_castF32(sinfo.maxLod),
		.borderColor = mapVkBorderColor(sinfo.borderColor)
	};

	_gotoIfError(clean, vkCheck(vkCreateSampler(deviceExt->device, &samplerInfo, NULL, &samplerExt->sampler)));
	
	if (name.ptr) {
	
		#ifndef NDEBUG

			if(instance->debugSetName) {

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_SAMPLER,
					.pObjectName = name.ptr,
					.objectHandle = (U64) samplerExt->sampler
				};

				_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));
			}

		#endif
	}
	
	acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		_gotoIfError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createSamplerExt() couldn't acquire descriptor lock"
		));

	//Create readonly buffer

	sampler->samplerLocation = VkGraphicsDevice_allocateDescriptor(deviceExt, EDescriptorType_Sampler);

	if(sampler->samplerLocation == U32_MAX)
		_gotoIfError(clean, Error_outOfMemory(
			0, "GraphicsDeviceRef_createSamplerExt() couldn't allocate Sampler descriptor"
		));

	VkDescriptorImageInfo imageInfo = (VkDescriptorImageInfo) { .sampler = samplerExt->sampler };

	VkWriteDescriptorSet descriptor = (VkWriteDescriptorSet) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.dstSet = deviceExt->sets[EDescriptorSetType_Sampler],
		.dstArrayElement = sampler->samplerLocation & ((1 << 20) - 1),
		.pImageInfo = &imageInfo
	};

	vkUpdateDescriptorSets(deviceExt->device, 1, &descriptor, 0, NULL);

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&deviceExt->descriptorLock);

	return err;
}
