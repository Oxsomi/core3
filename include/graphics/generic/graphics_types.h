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

//graphics/generic/graphics_types.h

#pragma once
#include "types/base/type_id.h"

#ifdef __cplusplus
	extern "C" {
#endif

//ETypeId but for graphics objects.

typedef enum EGraphicsTypeId {

	EGraphicsTypeId_GraphicsInstance             = makeObjectId(0x1C34,  0, 0),
	EGraphicsTypeId_GraphicsDevice               = makeObjectId(0x1C34,  1, 0),

	EGraphicsTypeId_Swapchain                    = makeObjectId(0x1C34,  2, 0),
	EGraphicsTypeId_CommandList                  = makeObjectId(0x1C34,  3, 0),

	EGraphicsTypeId_RenderTexture                = makeObjectId(0x1C34,  4, 0),
	EGraphicsTypeId_RenderPass                   = makeObjectId(0x1C34,  5, 0),

	EGraphicsTypeId_DeviceTexture                = makeObjectId(0x1C34,  6, 0),
	EGraphicsTypeId_DeviceBuffer                 = makeObjectId(0x1C34,  7, 0),
	EGraphicsTypeId_Pipeline                     = makeObjectId(0x1C34,  8, 0),
	EGraphicsTypeId_Sampler                      = makeObjectId(0x1C34,  9, 0),

	EGraphicsTypeId_DepthStencil                 = makeObjectId(0x1C34, 10, 0),

	EGraphicsTypeId_DescriptorLayout             = makeObjectId(0x1C34, 11, 0),
	EGraphicsTypeId_PipelineLayout               = makeObjectId(0x1C34, 12, 0),
	EGraphicsTypeId_DescriptorTable              = makeObjectId(0x1C34, 13, 0),
	EGraphicsTypeId_DescriptorHeap               = makeObjectId(0x1C34, 14, 0),

	//Requires EGraphicsFeatures_Raytracing

	EGraphicsTypeId_BLASExt                      = makeObjectId(0x1C34, 15, 0),
	EGraphicsTypeId_TLASExt                      = makeObjectId(0x1C34, 16, 0),

	EGraphicsTypeId_Count                        = 17

} EGraphicsTypeId;

extern EGraphicsTypeId EGraphicsTypeId_all[EGraphicsTypeId_Count];

#ifdef __cplusplus
	}
#endif
