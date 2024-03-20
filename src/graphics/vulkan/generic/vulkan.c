/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "graphics/vulkan/vulkan.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "formats/texture.h"
#include "types/error.h"

TListImpl(VkMappedMemoryRange);
TListImpl(VkBufferCopy);
TListImpl(VkImageCopy);
TListImpl(VkBufferImageCopy);
TListImpl(VkImageMemoryBarrier2);
TListImpl(VkBufferMemoryBarrier2);
TListImpl(VkPipeline);

Error vkCheck(VkResult result) {

	if(result >= VK_SUCCESS)
		return Error_none();

	switch (result) {

		case VK_ERROR_OUT_OF_HOST_MEMORY:				
			return Error_outOfMemory(0, "vkCheck() out of host memory");
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:				
			return Error_outOfMemory(1, "vkCheck() out of device memory");
		case VK_ERROR_OUT_OF_POOL_MEMORY:				
			return Error_outOfMemory(2, "vkCheck() out of pool memory");
		case VK_ERROR_TOO_MANY_OBJECTS:					
			return Error_outOfMemory(3, "vkCheck() too many objects");
		case VK_ERROR_FRAGMENTED_POOL:					
			return Error_outOfMemory(4, "vkCheck() fragmented pool");
		case VK_ERROR_FRAGMENTATION:					
			return Error_outOfMemory(5, "vkCheck() fragmentation");
		case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:		
			return Error_outOfMemory(6, "vkCheck() compression exhausted");
			
		case VK_ERROR_DEVICE_LOST:						
			return Error_invalidState(0, "vkCheck() device lost");
		case VK_ERROR_SURFACE_LOST_KHR:					
			return Error_invalidState(1, "vkCheck() surface lost");
		case VK_ERROR_MEMORY_MAP_FAILED:				
			return Error_invalidState(2, "vkCheck() memory map failed");
		case VK_ERROR_VALIDATION_FAILED_EXT:			
			return Error_invalidState(3, "vkCheck() validation failed");
		case VK_ERROR_OUT_OF_DATE_KHR:					
			return Error_invalidState(4, "vkCheck() out of date");
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:			
			return Error_invalidState(5, "vkCheck() native window in use");

			
		case VK_ERROR_INCOMPATIBLE_DRIVER:				
			return Error_unsupportedOperation(0, "vkCheck() incompatible driver");
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:			
			return Error_unsupportedOperation(1, "vkCheck() incompatible display");
		case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:	
			return Error_unsupportedOperation(2, "vkCheck() invalid image usage");
			
		case VK_ERROR_INVALID_EXTERNAL_HANDLE:			
			return Error_invalidParameter(0, 0, "vkCheck() invalid ext handle");
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:	
			return Error_invalidParameter(1, 0, "vkCheck() invalid capture addr");
			
		case VK_ERROR_LAYER_NOT_PRESENT:				
			return Error_notFound(0, 0, "vkCheck() layer not present");
		case VK_ERROR_EXTENSION_NOT_PRESENT:			
			return Error_notFound(1, 0, "vkCheck() extension not present");
		case VK_ERROR_FEATURE_NOT_PRESENT:				
			return Error_notFound(2, 0, "vkCheck() feature not present");
		case VK_ERROR_FORMAT_NOT_SUPPORTED:				
			return Error_notFound(3, 0, "vkCheck() format not supported");
			
		case VK_ERROR_NOT_PERMITTED_KHR:				
			return Error_unauthorized(0, "vkCheck() not permitted");

		case VK_ERROR_UNKNOWN:
		default:
			return Error_unsupportedOperation(3, "vkCheck() has unknown error");
	}
}

VkFormat mapVkFormat(ETextureFormat format) {

	switch (format) {

		default:										return VK_FORMAT_UNDEFINED;

		//8-bit formats

		case ETextureFormat_R8:							return  VK_FORMAT_R8_UNORM;
		case ETextureFormat_RG8:						return  VK_FORMAT_R8G8_UNORM;
		case ETextureFormat_RGBA8:						return  VK_FORMAT_R8G8B8A8_UNORM;
		case ETextureFormat_BGRA8:						return  VK_FORMAT_B8G8R8A8_UNORM;

		case ETextureFormat_R8s:						return  VK_FORMAT_R8_SNORM;
		case ETextureFormat_RG8s:						return  VK_FORMAT_R8G8_SNORM;
		case ETextureFormat_RGBA8s:						return  VK_FORMAT_R8G8B8A8_SNORM;

		case ETextureFormat_R8u:						return  VK_FORMAT_R8_UINT;
		case ETextureFormat_RG8u:						return  VK_FORMAT_R8G8_UINT;
		case ETextureFormat_RGBA8u:						return  VK_FORMAT_R8G8B8A8_UINT;

		case ETextureFormat_R8i:						return  VK_FORMAT_R8_SINT;
		case ETextureFormat_RG8i:						return  VK_FORMAT_R8G8_SINT;
		case ETextureFormat_RGBA8i:						return  VK_FORMAT_R8G8B8A8_SINT;

		//16-bit formats

		case ETextureFormat_R16:						return VK_FORMAT_R16_UNORM;
		case ETextureFormat_RG16:						return VK_FORMAT_R16G16_UNORM;
		case ETextureFormat_RGBA16:						return VK_FORMAT_R16G16B16A16_UNORM;

		case ETextureFormat_R16s:						return VK_FORMAT_R16_SNORM;
		case ETextureFormat_RG16s:						return VK_FORMAT_R16G16_SNORM;
		case ETextureFormat_RGBA16s:					return VK_FORMAT_R16G16B16A16_SNORM;

		case ETextureFormat_R16u:						return VK_FORMAT_R16_UINT;
		case ETextureFormat_RG16u:						return VK_FORMAT_R16G16_UINT;
		case ETextureFormat_RGBA16u:					return VK_FORMAT_R16G16B16A16_UINT;

		case ETextureFormat_R16i:						return VK_FORMAT_R16_SINT;
		case ETextureFormat_RG16i:						return VK_FORMAT_R16G16_SINT;
		case ETextureFormat_RGBA16i:					return VK_FORMAT_R16G16B16A16_SINT;

		case ETextureFormat_R16f:						return VK_FORMAT_R16_SFLOAT;
		case ETextureFormat_RG16f:						return VK_FORMAT_R16G16_SFLOAT;
		case ETextureFormat_RGBA16f:					return VK_FORMAT_R16G16B16A16_SFLOAT;

		//32-bit formats

		case ETextureFormat_R32u:						return VK_FORMAT_R32_UINT;
		case ETextureFormat_RG32u:						return VK_FORMAT_R32G32_UINT;
		case ETextureFormat_RGB32u:						return VK_FORMAT_R32G32B32_UINT;
		case ETextureFormat_RGBA32u:					return VK_FORMAT_R32G32B32A32_UINT;

		case ETextureFormat_R32i:						return VK_FORMAT_R32_SINT;
		case ETextureFormat_RG32i:						return VK_FORMAT_R32G32_SINT;
		case ETextureFormat_RGB32i:						return VK_FORMAT_R32G32B32_SINT;
		case ETextureFormat_RGBA32i:					return VK_FORMAT_R32G32B32A32_SINT;

		case ETextureFormat_R32f:						return VK_FORMAT_R32_SFLOAT;
		case ETextureFormat_RG32f:						return VK_FORMAT_R32G32_SFLOAT;
		case ETextureFormat_RGB32f:						return VK_FORMAT_R32G32B32_SFLOAT;
		case ETextureFormat_RGBA32f:					return VK_FORMAT_R32G32B32A32_SFLOAT;

		//Special formats

		case ETextureFormat_BGR10A2:					return VK_FORMAT_A2B10G10R10_UNORM_PACK32;

		//BCn formats

		case ETextureFormat_BC4:						return VK_FORMAT_BC4_UNORM_BLOCK;
		case ETextureFormat_BC4s:						return VK_FORMAT_BC4_SNORM_BLOCK;

		case ETextureFormat_BC5:						return VK_FORMAT_BC5_UNORM_BLOCK;
		case ETextureFormat_BC5s:						return VK_FORMAT_BC5_SNORM_BLOCK;

		case ETextureFormat_BC6H:						return VK_FORMAT_BC6H_SFLOAT_BLOCK;

		case ETextureFormat_BC7:						return VK_FORMAT_BC7_UNORM_BLOCK;
		case ETextureFormat_BC7_sRGB:					return VK_FORMAT_BC7_SRGB_BLOCK;

		//ASTC formats

		case ETextureFormat_ASTC_4x4:					return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
		case ETextureFormat_ASTC_4x4_sRGB:				return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;

		case ETextureFormat_ASTC_5x4:					return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
		case ETextureFormat_ASTC_5x4_sRGB:				return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;

		case ETextureFormat_ASTC_5x5:					return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
		case ETextureFormat_ASTC_5x5_sRGB:				return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;

		case ETextureFormat_ASTC_6x5:					return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
		case ETextureFormat_ASTC_6x5_sRGB:				return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;

		case ETextureFormat_ASTC_6x6:					return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
		case ETextureFormat_ASTC_6x6_sRGB:				return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;

		case ETextureFormat_ASTC_8x5:					return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
		case ETextureFormat_ASTC_8x5_sRGB:				return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;

		case ETextureFormat_ASTC_8x6:					return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
		case ETextureFormat_ASTC_8x6_sRGB:				return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;

		case ETextureFormat_ASTC_8x8:					return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
		case ETextureFormat_ASTC_8x8_sRGB:				return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;

		case ETextureFormat_ASTC_10x5:					return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
		case ETextureFormat_ASTC_10x5_sRGB:				return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;

		case ETextureFormat_ASTC_10x6:					return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
		case ETextureFormat_ASTC_10x6_sRGB:				return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;

		case ETextureFormat_ASTC_10x8:					return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
		case ETextureFormat_ASTC_10x8_sRGB:				return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;

		case ETextureFormat_ASTC_10x10:					return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
		case ETextureFormat_ASTC_10x10_sRGB:			return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;

		case ETextureFormat_ASTC_12x10:					return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
		case ETextureFormat_ASTC_12x10_sRGB:			return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;

		case ETextureFormat_ASTC_12x12:					return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
		case ETextureFormat_ASTC_12x12_sRGB:			return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
	}
}

VkDeviceAddress getVkDeviceAddress(DeviceData data) {
	return DeviceBufferRef_ptr(data.buffer)->resource.deviceAddress + data.offset;
}

VkDeviceOrHostAddressConstKHR getVkLocation(DeviceData data, U64 localOffset) {
	return (VkDeviceOrHostAddressConstKHR) { .deviceAddress = getVkDeviceAddress(data) + localOffset };
}
