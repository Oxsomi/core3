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
#include "types/base/type_id.h"

#ifdef __cplusplus
	extern "C" {
#endif

//ETypeId but for graphics objects.

typedef enum EGraphicsTypeId {

	EGraphicsTypeId_GraphicsInstance			= makeObjectId(0x1C33,  0, 0),
	EGraphicsTypeId_GraphicsDevice				= makeObjectId(0x1C33,  1, 0),

	EGraphicsTypeId_Swapchain					= makeObjectId(0x1C33,  2, 0),
	EGraphicsTypeId_CommandList					= makeObjectId(0x1C33,  3, 0),

	EGraphicsTypeId_RenderTexture				= makeObjectId(0x1C33,  4, 0),
	EGraphicsTypeId_RenderPass					= makeObjectId(0x1C33,  5, 0),

	EGraphicsTypeId_DeviceTexture				= makeObjectId(0x1C33,  6, 0),
	EGraphicsTypeId_DeviceBuffer				= makeObjectId(0x1C33,  7, 0),
	EGraphicsTypeId_Pipeline					= makeObjectId(0x1C33,  8, 0),
	EGraphicsTypeId_Sampler						= makeObjectId(0x1C33,  9, 0),

	EGraphicsTypeId_DepthStencil				= makeObjectId(0x1C33, 10, 0),

	//Requires EGraphicsFeatures_Raytracing

	EGraphicsTypeId_BLASExt						= makeObjectId(0x1C33, 11, 0),
	EGraphicsTypeId_TLASExt						= makeObjectId(0x1C33, 12, 0),

	EGraphicsTypeId_Count						= 13

} EGraphicsTypeId;

extern EGraphicsTypeId EGraphicsTypeId_all[EGraphicsTypeId_Count];

#ifdef __cplusplus
	}
#endif
