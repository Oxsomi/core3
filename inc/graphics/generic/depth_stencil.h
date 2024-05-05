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

#pragma once
#include "types/ref_ptr.h"
#include "pipeline_structs.h"
#include "texture.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsDeviceRef;

typedef UnifiedTexture DepthStencil;

typedef RefPtr DepthStencilRef;

#define DepthStencilRef_ptr(ptr) RefPtr_data(ptr, DepthStencil)

Error DepthStencilRef_dec(DepthStencilRef **depthStencil);
Error DepthStencilRef_inc(DepthStencilRef *depthStencil);

Error GraphicsDeviceRef_createDepthStencil(
	GraphicsDeviceRef *deviceRef,
	U16 width,
	U16 height,
	EDepthStencilFormat format,
	Bool allowShaderRead,
	EMSAASamples msaa,
	CharString name,
	DepthStencilRef **depthStencil
);

#ifdef __cplusplus
	}
#endif
