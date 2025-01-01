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

#define VK_USE_PLATFORM_WIN32_KHR
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "types/base/error.h"

Error VkSurface_create(GraphicsDevice *device, const Window *window, VkSurfaceKHR *surface) {

	if(!device || !window || !surface)
		return Error_nullPointer(!device ? 0 : (!window ? 0 : 1), "VkSurface_create()::device, window or surface is NULL");

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	const VkWin32SurfaceCreateInfoKHR surfaceInfo = (VkWin32SurfaceCreateInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hwnd = window->nativeHandle,
		.hinstance = Platform_instance->data
	};

	if (!instanceExt->createSurfaceExt)
		instanceExt->createSurfaceExt = (void*) vkGetInstanceProcAddr(instanceExt->instance, "vkCreateWin32SurfaceKHR");

	return vkCheck(
		((PFN_vkCreateWin32SurfaceKHR)instanceExt->createSurfaceExt)(instanceExt->instance, &surfaceInfo, NULL, surface)
	);
}
