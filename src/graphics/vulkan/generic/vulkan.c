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

#include "graphics/vulkan/vulkan.h"
#include "graphics/generic/device_info.h"
#include "formats/texture.h"
#include "types/error.h"

Error vkCheck(VkResult result) {

	if(result >= VK_SUCCESS)
		return Error_none();

	switch (result) {

		case VK_ERROR_OUT_OF_HOST_MEMORY:								return Error_outOfMemory(0);
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:								return Error_outOfMemory(1);
		case VK_ERROR_OUT_OF_POOL_MEMORY:								return Error_outOfMemory(2);
		case VK_ERROR_TOO_MANY_OBJECTS:									return Error_outOfMemory(3);
		case VK_ERROR_FRAGMENTED_POOL:									return Error_outOfMemory(4);
		case VK_ERROR_FRAGMENTATION:									return Error_outOfMemory(5);
		case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:						return Error_outOfMemory(6);

		case VK_ERROR_DEVICE_LOST:										return Error_invalidState(0);
		case VK_ERROR_SURFACE_LOST_KHR:									return Error_invalidState(1);
		case VK_ERROR_MEMORY_MAP_FAILED:								return Error_invalidState(2);
		case VK_ERROR_VALIDATION_FAILED_EXT:							return Error_invalidState(3);
		case VK_ERROR_OUT_OF_DATE_KHR:									return Error_invalidState(4);
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:							return Error_invalidState(5);

		case VK_ERROR_INCOMPATIBLE_DRIVER:								return Error_unsupportedOperation(0);
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:							return Error_unsupportedOperation(1);
		case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:					return Error_unsupportedOperation(2);

		case VK_ERROR_INVALID_EXTERNAL_HANDLE:							return Error_invalidParameter(0, 0);
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:					return Error_invalidParameter(1, 0);

		case VK_ERROR_LAYER_NOT_PRESENT:								return Error_notFound(0, 0);
		case VK_ERROR_EXTENSION_NOT_PRESENT:							return Error_notFound(1, 0);
		case VK_ERROR_FEATURE_NOT_PRESENT:								return Error_notFound(2, 0);
		case VK_ERROR_FORMAT_NOT_SUPPORTED:								return Error_notFound(3, 0);

		case VK_ERROR_NOT_PERMITTED_KHR:								return Error_unauthorized(0);

		case VK_ERROR_UNKNOWN:
		default:
			return Error_unsupportedOperation(3);
	}
}

Error mapVkFormat(
	EGraphicsDataTypes *types, 
	ETextureFormat format, 
	VkFormat *output, 
	Bool isForRenderTarget,
	Bool supportsUndefined
) {

	if(!output)
		return Error_nullPointer(2);

	switch (format) {

		case ETextureFormat_Undefined:

			if(!supportsUndefined)
				return Error_unsupportedOperation(4);

			*output = VK_FORMAT_UNDEFINED;
			break;

		//8-bit formats

		case ETextureFormat_R8:					*output = VK_FORMAT_R8_UNORM;					break;
		case ETextureFormat_RG8:				*output = VK_FORMAT_R8G8_UNORM;					break;
		case ETextureFormat_RGBA8:				*output = VK_FORMAT_R8G8B8A8_UNORM;				break;
		case ETextureFormat_BGRA8:				*output = VK_FORMAT_B8G8R8A8_UNORM;				break;

		case ETextureFormat_R8s:				*output = VK_FORMAT_R8_SNORM;					break;
		case ETextureFormat_RG8s:				*output = VK_FORMAT_R8G8_SNORM;					break;
		case ETextureFormat_RGBA8s:				*output = VK_FORMAT_R8G8B8A8_SNORM;				break;

		case ETextureFormat_R8u:				*output = VK_FORMAT_R8_UINT;					break;
		case ETextureFormat_RG8u:				*output = VK_FORMAT_R8G8_UINT;					break;
		case ETextureFormat_RGBA8u:				*output = VK_FORMAT_R8G8B8A8_UINT;				break;

		case ETextureFormat_R8i:				*output = VK_FORMAT_R8_SINT;					break;
		case ETextureFormat_RG8i:				*output = VK_FORMAT_R8G8_SINT;					break;
		case ETextureFormat_RGBA8i:				*output = VK_FORMAT_R8G8B8A8_SINT;				break;

		//16-bit formats

		case ETextureFormat_R16:				*output = VK_FORMAT_R16_UNORM;					break;
		case ETextureFormat_RG16:				*output = VK_FORMAT_R16G16_UNORM;				break;
		case ETextureFormat_RGBA16:				*output = VK_FORMAT_R16G16B16A16_UNORM;			break;

		case ETextureFormat_R16s:				*output = VK_FORMAT_R16_SNORM;					break;
		case ETextureFormat_RG16s:				*output = VK_FORMAT_R16G16_SNORM;				break;
		case ETextureFormat_RGBA16s:			*output = VK_FORMAT_R16G16B16A16_SNORM;			break;

		case ETextureFormat_R16u:				*output = VK_FORMAT_R16_UINT;					break;
		case ETextureFormat_RG16u:				*output = VK_FORMAT_R16G16_UINT;				break;
		case ETextureFormat_RGBA16u:			*output = VK_FORMAT_R16G16B16A16_UINT;			break;

		case ETextureFormat_R16i:				*output = VK_FORMAT_R16_SINT;					break;
		case ETextureFormat_RG16i:				*output = VK_FORMAT_R16G16_SINT;				break;
		case ETextureFormat_RGBA16i:			*output = VK_FORMAT_R16G16B16A16_SINT;			break;

		case ETextureFormat_R16f:				*output = VK_FORMAT_R16_SFLOAT;					break;
		case ETextureFormat_RG16f:				*output = VK_FORMAT_R16G16_SFLOAT;				break;
		case ETextureFormat_RGBA16f:			*output = VK_FORMAT_R16G16B16A16_SFLOAT;		break;

		//32-bit formats

		case ETextureFormat_R32u:				*output = VK_FORMAT_R32_UINT;					break;
		case ETextureFormat_RG32u:				*output = VK_FORMAT_R32G32_UINT;				break;
		case ETextureFormat_RGB32u:				*output = VK_FORMAT_R32G32B32_UINT;				break;
		case ETextureFormat_RGBA32u:			*output = VK_FORMAT_R32G32B32A32_UINT;			break;

		case ETextureFormat_R32i:				*output = VK_FORMAT_R32_SINT;					break;
		case ETextureFormat_RG32i:				*output = VK_FORMAT_R32G32_SINT;				break;
		case ETextureFormat_RGB32i:				*output = VK_FORMAT_R32G32B32_SINT;				break;
		case ETextureFormat_RGBA32i:			*output = VK_FORMAT_R32G32B32A32_SINT;			break;

		case ETextureFormat_R32f:				*output = VK_FORMAT_R32_SFLOAT;					break;
		case ETextureFormat_RG32f:				*output = VK_FORMAT_R32G32_SFLOAT;				break;
		case ETextureFormat_RGB32f:				*output = VK_FORMAT_R32G32B32_SFLOAT;			break;
		case ETextureFormat_RGBA32f:			*output = VK_FORMAT_R32G32B32A32_SFLOAT;		break;

		//Special formats

		case ETextureFormat_RGB10A2:			*output = VK_FORMAT_A2R10G10B10_UNORM_PACK32;	break;
		case ETextureFormat_BGR10A2:			*output = VK_FORMAT_A2B10G10R10_UNORM_PACK32;	break;

		//BCn formats

		case ETextureFormat_BC4:				*output = VK_FORMAT_BC4_UNORM_BLOCK;			break;
		case ETextureFormat_BC4s:				*output = VK_FORMAT_BC4_SNORM_BLOCK;			break;

		case ETextureFormat_BC5:				*output = VK_FORMAT_BC5_UNORM_BLOCK;			break;
		case ETextureFormat_BC5s:				*output = VK_FORMAT_BC5_SNORM_BLOCK;			break;

		case ETextureFormat_BC7:				*output = VK_FORMAT_BC7_UNORM_BLOCK;			break;
		case ETextureFormat_BC7_sRGB:			*output = VK_FORMAT_BC7_SRGB_BLOCK;				break;

		case ETextureFormat_BC6H:				*output = VK_FORMAT_BC6H_SFLOAT_BLOCK;			break;

		//ASTC formats

		case ETextureFormat_ASTC_4x4:			*output = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_4x4_sRGB:		*output = VK_FORMAT_ASTC_4x4_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_5x4:			*output = VK_FORMAT_ASTC_5x4_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_5x4_sRGB:		*output = VK_FORMAT_ASTC_5x4_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_5x5:			*output = VK_FORMAT_ASTC_5x5_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_5x5_sRGB:		*output = VK_FORMAT_ASTC_5x5_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_6x5:			*output = VK_FORMAT_ASTC_6x5_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_6x5_sRGB:		*output = VK_FORMAT_ASTC_6x5_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_6x6:			*output = VK_FORMAT_ASTC_6x6_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_6x6_sRGB:		*output = VK_FORMAT_ASTC_6x6_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_8x5:			*output = VK_FORMAT_ASTC_8x5_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_8x5_sRGB:		*output = VK_FORMAT_ASTC_8x5_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_8x6:			*output = VK_FORMAT_ASTC_8x6_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_8x6_sRGB:		*output = VK_FORMAT_ASTC_8x6_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_8x8:			*output = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_8x8_sRGB:		*output = VK_FORMAT_ASTC_8x8_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_10x5:			*output = VK_FORMAT_ASTC_10x5_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_10x5_sRGB:		*output = VK_FORMAT_ASTC_10x5_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_10x6:			*output = VK_FORMAT_ASTC_10x6_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_10x6_sRGB:		*output = VK_FORMAT_ASTC_10x6_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_10x8:			*output = VK_FORMAT_ASTC_10x8_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_10x8_sRGB:		*output = VK_FORMAT_ASTC_10x8_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_10x10:			*output = VK_FORMAT_ASTC_10x10_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_10x10_sRGB:	*output = VK_FORMAT_ASTC_10x10_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_12x10:			*output = VK_FORMAT_ASTC_12x10_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_12x10_sRGB:	*output = VK_FORMAT_ASTC_12x10_SRGB_BLOCK;		break;

		case ETextureFormat_ASTC_12x12:			*output = VK_FORMAT_ASTC_12x12_UNORM_BLOCK;		break;
		case ETextureFormat_ASTC_12x12_sRGB:	*output = VK_FORMAT_ASTC_12x12_SRGB_BLOCK;		break;

		//Invalid

		default:
			return Error_unsupportedOperation(0);
	}

	//Only support converting to type if it's supported

	Bool isBCn = *output >= VK_FORMAT_BC1_RGB_UNORM_BLOCK && *output <= VK_FORMAT_BC7_SRGB_BLOCK;

	if(types && !(*types & EGraphicsDataTypes_BCn) && isBCn)
		return Error_unsupportedOperation(1);

	Bool isASTC = *output >= VK_FORMAT_ASTC_4x4_UNORM_BLOCK && *output <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK;

	if(types && !(*types & EGraphicsDataTypes_ASTC) && isASTC)
		return Error_unsupportedOperation(2);

	if(isForRenderTarget && (isASTC || isBCn))
		return Error_unsupportedOperation(3);

	return Error_none();
}
