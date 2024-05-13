/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "graphics/generic/render_texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/pipeline_structs.h"
#include "platforms/ext/ref_ptrx.h"
#include "formats/texture.h"
#include "types/error.h"
#include "types/string.h"

Error RenderTextureRef_dec(RenderTextureRef **renderTexture) {
	return !RefPtr_dec(renderTexture) ?
		Error_invalidOperation(0, "RenderTextureRef_dec()::renderTexture is invalid") : Error_none();
}

Error RenderTextureRef_inc(RenderTextureRef *renderTexture) {
	return !RefPtr_inc(renderTexture) ?
		Error_invalidOperation(0, "RenderTextureRef_inc()::renderTexture is invalid") : Error_none();
}

Bool GraphicsDevice_freeRenderTexture(RenderTexture *renderTexture, Allocator alloc) {
	(void)alloc;
	return UnifiedTexture_free((TextureRef*)((U8*)renderTexture - sizeof(RefPtr)));
}

Error GraphicsDeviceRef_createRenderTexture(
	GraphicsDeviceRef *deviceRef,
	ETextureType type,
	U16 width,
	U16 height,
	U16 length,
	ETextureFormatId formatId,
	EGraphicsResourceFlag flag,
	EMSAASamples msaa,
	CharString name,
	RenderTextureRef **renderTextureRef
) {

	Error err = RefPtr_createx(
		(U32)(sizeof(RenderTexture) + UnifiedTextureImageExt_size + sizeof(UnifiedTextureImage)),
		(ObjectFreeFunc) GraphicsDevice_freeRenderTexture,
		(ETypeId) EGraphicsTypeId_RenderTexture,
		renderTextureRef
	);

	if(err.genericError)
		return err;

	gotoIfError(clean, GraphicsDeviceRef_inc(deviceRef))

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

	gotoIfError(clean, UnifiedTexture_create(*renderTextureRef, name))

clean:

	if(err.genericError)
		RenderTextureRef_dec(renderTextureRef);

	return err;
}
