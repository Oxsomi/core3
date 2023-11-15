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

#include "graphics/vulkan/vk_instance.h"
#include "platforms/ext/listx.h"
#include "types/error.h"
#include "types/list.h"
#include "types/buffer.h"

const C8 *vkValidation = "VK_LAYER_KHRONOS_validation";
const C8 *vkApiDump = "VK_LAYER_LUNARG_api_dump";

//#define _GRAPHICS_VERBOSE_DEBUGGING

Error VkGraphicsInstance_getLayers(List *layers) {

	if(!layers)
		return Error_nullPointer(0);

	if(layers->stride != sizeof(const C8*))
		return Error_invalidParameter(0, 0);

	#ifndef NDEBUG

		Buffer tmp = Buffer_createConstRef(&vkValidation, sizeof(vkValidation));
		Error err = List_pushBackx(layers, tmp);

		if(err.genericError)
			return err;

		#ifdef _GRAPHICS_VERBOSE_DEBUGGING
			tmp = Buffer_createConstRef(&vkApiDump, sizeof(vkApiDump));
			return List_pushBackx(layers, tmp);
		#else
			return Error_none();
		#endif

	#else
		return Error_none();
	#endif
}
