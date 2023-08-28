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