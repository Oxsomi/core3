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
#include "types/type_id.h"

//ETypeId but for graphics objects. 

typedef enum EGraphicsTypeId {

	EGraphicsTypeId_GraphicsInstance			= _makeObjectId(0xC4,  0, 0),
	EGraphicsTypeId_GraphicsDevice				= _makeObjectId(0xC4,  1, 0),

	EGraphicsTypeId_Swapchain					= _makeObjectId(0xC4,  2, 0),
	EGraphicsTypeId_CommandList					= _makeObjectId(0xC4,  3, 0),

	EGraphicsTypeId_RenderTexture				= _makeObjectId(0xC4,  4, 0),
	EGraphicsTypeId_RenderPass					= _makeObjectId(0xC4,  5, 0),

	EGraphicsTypeId_DeviceTexture				= _makeObjectId(0xC4,  6, 1),
	EGraphicsTypeId_DeviceBuffer				= _makeObjectId(0xC4,  7, 1),
	EGraphicsTypeId_Pipeline					= _makeObjectId(0xC4,  8, 1),
	EGraphicsTypeId_Sampler						= _makeObjectId(0xC4,  9, 1),
	EGraphicsTypeId_AccelerationStructureExt	= _makeObjectId(0xC4, 10, 1),

	EGraphicsTypeId_Count						= 11

} EGraphicsTypeId;

extern EGraphicsTypeId EGraphicsTypeId_all[EGraphicsTypeId_Count];
