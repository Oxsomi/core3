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

//graphics/generic/render_texture.c

#include "graphics/generic/interface.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/pipeline_structs.h"
#include "types/container/ref_ptr.h"
#include "types/container/texture_format.h"
#include "types/base/error.h"

void GraphicsDevice_freeRenderTexture(RenderTexture *renderTexture, const Allocator *alloc) {
	(void)alloc;
	UnifiedTexture_free((TextureRef*)((U8*)renderTexture - sizeof(RefPtr)));
}

Bool GraphicsDeviceRef_createRenderTexture(
	GraphicsDeviceRef *deviceRef,
	ETextureType type,
	U16 width,
	U16 height,
	U16 length,
	ETextureFormatId formatId,
	EGraphicsResourceFlag flag,
	EMSAASamples msaa,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	RenderTextureRef **renderTextureRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(deviceRef)->renderTexture, renderTextureRef, e_rr));
	allocated = true;

	gotoIfError3(clean, RefPtr_inc(deviceRef));

	*RenderTextureRef_ptr(*renderTextureRef) = (UnifiedTexture) {
		.resource = (GraphicsResource) {
			.device = deviceRef,
			.flags = flag,
			.type = (U8) EResourceType_RenderTargetOrDepthStencil
		},
		.sampleCount = (U8) msaa,
		.textureFormatId = (U8) formatId,
		.type = (U8) type,
		.width = width,
		.height = height,
		.length = length,
		.levels = 1,
		.images = 1
	};

	gotoIfError3(clean, UnifiedTexture_create(*renderTextureRef, bindlessDescriptorTable, name, e_rr));

clean:

	if(!s_uccess && allocated)
		RefPtr_dec(renderTextureRef);

	return s_uccess;
}
