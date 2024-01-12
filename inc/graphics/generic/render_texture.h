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

#pragma once
#include "types/vec.h"
#include "platforms/ref_ptr.h"
#include "formats/texture.h"

typedef RefPtr GraphicsDeviceRef;
typedef struct Error Error;
typedef struct CharString CharString;

typedef enum ERenderTextureType {

	ERenderTextureType_2D,
	ERenderTextureType_3D,
	ERenderTextureType_Cube,
	ERenderTextureType_Count

} ERenderTextureType;

typedef enum ERenderTextureUsage {

	ERenderTextureUsage_None,
	ERenderTextureUsage_ShaderRead		= 1 << 0,
	ERenderTextureUsage_ShaderWrite		= 1 << 1,

	ERenderTextureUsage_ShaderRW		= ERenderTextureUsage_ShaderRead | ERenderTextureUsage_ShaderWrite

} ERenderTextureUsage;

typedef struct RenderTexture {

	GraphicsDeviceRef *device;

	ERenderTextureType type;
	U32 readLocation;

	I32x4 size;

	ETextureFormat format;
	ERenderTextureUsage usage;

	U32 writeLocation;
	U32 padding;

} RenderTexture;

typedef RefPtr RenderTextureRef;

#define RenderTexture_ext(ptr, T) (!ptr ? NULL : (T##RenderTexture*)(ptr + 1))		//impl
#define RenderTextureRef_ptr(ptr) RefPtr_data(ptr, RenderTexture)

Error RenderTextureRef_dec(RenderTextureRef **renderTexture);
Error RenderTextureRef_inc(RenderTextureRef *renderTexture);

Error GraphicsDeviceRef_createRenderTexture(
	GraphicsDeviceRef *deviceRef, 
	ERenderTextureType type,
	I32x4 size, 
	ETextureFormatId format, 
	ERenderTextureUsage usage,
	CharString name,
	RenderTextureRef **renderTexture
);