/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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
#include "graphics/vulkan/vk_instance.h"
#include "types/base/error.h"
#include "types/container/list.h"
#include "types/container/string.h"
#include "types/container/buffer.h"

const C8 *vkApiDump = "VK_LAYER_LUNARG_api_dump";

//#define _GRAPHICS_VERBOSE_DEBUGGING

Error VkGraphicsInstance_getLayers(Bool isDebug, ListConstC8 *layers) {

	(void) layers;

	if(isDebug) {
		#ifdef _GRAPHICS_VERBOSE_DEBUGGING
			return ListConstC8_pushBackx(layers, vkApiDump);
		#else
			return Error_none();
		#endif
	}

	return Error_none();
}
