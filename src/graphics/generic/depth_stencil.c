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

//graphics/generic/depth_stencil.c

#include "graphics/generic/interface.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/device.h"
#include "types/container/ref_ptr.h"
#include "types/container/texture_format.h"
#include "types/base/error.h"
#include "types/container/string.h"

void GraphicsDevice_freeDepthStencil(DepthStencil *depthStencil, const Allocator *alloc) {
	(void)alloc;
	UnifiedTexture_free((TextureRef*)((U8*)depthStencil - sizeof(RefPtr)));
}

Bool GraphicsDeviceRef_createDepthStencil(
	GraphicsDeviceRef *deviceRef,
	U16 width,
	U16 height,
	EDepthStencilFormat format,
	Bool allowShaderRead,
	EMSAASamples msaa,
	DescriptorTableRef *bindlessDescriptorTable,
	CharString name,
	DepthStencilRef **depthStencilRef,
	Error *e_rr
) {

	Bool s_uccess = true;

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(deviceRef)->depthStencil, depthStencilRef, e_rr));

	gotoIfError3(clean, RefPtr_inc(deviceRef));

	*DepthStencilRef_ptr(*depthStencilRef) = (UnifiedTexture) {
		.resource = (GraphicsResource) {
			.device = deviceRef,
			.flags = allowShaderRead ? EGraphicsResourceFlag_ShaderRead : 0,
			.type = (U8) EResourceType_RenderTargetOrDepthStencil
		},
		.sampleCount = (U8) msaa,
		.depthFormat = (U8) format,
		.type = ETextureType_2D,
		.width = width,
		.height = height,
		.length = 1,
		.levels = 1,
		.images = 1
	};

	gotoIfError3(clean, UnifiedTexture_create(*depthStencilRef, bindlessDescriptorTable, name, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(depthStencilRef);

	return s_uccess;
}
