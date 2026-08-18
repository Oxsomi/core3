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

//graphics/vulkan/generic/vk_opacity_micromap.c

#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/device.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_interface.h"
#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/base/error.h"

TListNamedImpl(ListVkMicromapUsageEXT);

Bool VK_WRAP_FUNC(OpacityMicromap_init)(OpacityMicromap *micromap, Error *e_rr) {

	Bool s_uccess = true;

	//The usages are translated once here rather than per BLAS, because every BLAS that links this micromap
	// has to hand the same set back to the driver.

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	VkOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Vk);

	gotoIfError3(clean, ListVkMicromapUsageEXT_resize(
		&micromapExt->usages, micromap->usages.length, alloc, e_rr
	));

	for (U64 i = 0; i < micromap->usages.length; ++i) {

		const OpacityMicromapUsage usage = micromap->usages.ptr[i];

		//Mapped through a switch rather than cast, so a header renumbering breaks the build instead of the
		// opacity data.

		VkOpacityMicromapFormatEXT format = VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT;

		switch ((EOpacityMicromapFormat) usage.format) {

			case EOpacityMicromapFormat_Opacity2State:
				format = VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT;
				break;

			case EOpacityMicromapFormat_Opacity4State:
				format = VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT;
				break;

			default:
				retError(clean, Error_unsupportedOperation(0, "VkOpacityMicromap_init() unsupported format"));
		}

		micromapExt->usages.ptrNonConst[i] = (VkMicromapUsageEXT) {
			.count = usage.count,
			.subdivisionLevel = usage.subdivisionLevel,
			.format = (U32) format
		};
	}

clean:
	return s_uccess;
}

void VK_WRAP_FUNC(OpacityMicromap_free)(OpacityMicromap *micromap) {

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	VkOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Vk);

	ListVkMicromapUsageEXT_free(&micromapExt->usages, alloc);
}

Bool VK_WRAP_FUNC(OpacityMicromapRef_flush)(
	void *commandBufferExt, GraphicsDeviceRef *deviceRef, OpacityMicromapRef *pending, Error *e_rr
) {

	Bool s_uccess = true;

	(void) commandBufferExt; (void) deviceRef; (void) pending;

	retError(clean, Error_unsupportedOperation(
		0, "VkOpacityMicromapRef_flush() building an opacity micromap isn't implemented yet"
	));

clean:
	return s_uccess;
}
