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

#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/d3d12/dx_device.h"
#include "types/container/string.h"

Bool DX_WRAP_FUNC(Sampler_free)(Sampler *sampler) { (void)sampler; return true; }

Error DX_WRAP_FUNC(GraphicsDeviceRef_createSampler)(GraphicsDeviceRef *dev, Sampler *sampler, CharString name) {
	(void)name; (void) dev; (void) sampler;
	return Error_none();
}
