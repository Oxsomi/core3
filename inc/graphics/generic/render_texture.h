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

#pragma once
#include "types/math/vec.h"
#include "types/container/ref_ptr.h"
#include "graphics/generic/texture.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsDeviceRef;

typedef UnifiedTexture RenderTexture;

typedef RefPtr RenderTextureRef;

#define RenderTextureRef_ptr(ptr) RefPtr_data(ptr, RenderTexture)

Error RenderTextureRef_dec(RenderTextureRef **renderTexture);
Error RenderTextureRef_inc(RenderTextureRef *renderTexture);

Error GraphicsDeviceRef_createRenderTexture(
	GraphicsDeviceRef *deviceRef,
	ETextureType type,
	U16 width,
	U16 height,
	U16 length,
	ETextureFormatId format,
	EGraphicsResourceFlag flag,
	EMSAASamples msaa,
	CharString name,
	RenderTextureRef **renderTexture
);

#ifdef __cplusplus
	}
#endif
