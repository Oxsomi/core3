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
#include "pipeline_structs.h"

typedef RefPtr GraphicsDeviceRef;
typedef struct Error Error;
typedef struct CharString CharString;

typedef struct DepthStencil {

	GraphicsDeviceRef *device;

	I32x2 size;

	U32 readLocation;

	EDepthStencilFormat format;

	EMSAASamples msaa;

	Bool allowShaderRead;
	U8 padding[3];

} DepthStencil;

typedef RefPtr DepthStencilRef;

#define DepthStencil_ext(ptr, T) (!ptr ? NULL : (T##DepthStencil*)(ptr + 1))		//impl
#define DepthStencilRef_ptr(ptr) RefPtr_data(ptr, DepthStencil)

Error DepthStencilRef_dec(DepthStencilRef **depthStencil);
Error DepthStencilRef_inc(DepthStencilRef *depthStencil);

Error GraphicsDeviceRef_createDepthStencil(
	GraphicsDeviceRef *deviceRef,
	I32x2 size,
	EDepthStencilFormat format,
	Bool allowShaderRead,
	EMSAASamples msaa,
	CharString name,
	DepthStencilRef **depthStencil
);
