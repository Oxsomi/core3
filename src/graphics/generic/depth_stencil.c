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

#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/device.h"
#include "platforms/ext/ref_ptrx.h"
#include "formats/texture.h"
#include "types/error.h"
#include "types/string.h"

Error DepthStencilRef_dec(DepthStencilRef **depth) {
	return !RefPtr_dec(depth) ?
		Error_invalidOperation(0, "DepthStencilRef_dec()::depth is invalid") : Error_none();
}

Error DepthStencilRef_inc(DepthStencilRef *depth) {
	return !RefPtr_inc(depth) ?
		Error_invalidOperation(0, "DepthStencilRef_inc()::depth is invalid") : Error_none();
}

Bool GraphicsDevice_freeDepthStencil(DepthStencil *depthStencil, Allocator alloc) {
	(void)alloc;
	return UnifiedTexture_free((TextureRef*)((U8*)depthStencil - sizeof(RefPtr)));
}

Error GraphicsDeviceRef_createDepthStencil(
	GraphicsDeviceRef *deviceRef,
	U16 width,
	U16 height,
	EDepthStencilFormat format,
	Bool allowShaderRead,
	EMSAASamples msaa,
	CharString name,
	DepthStencilRef **depthStencilRef
) {

	Error err = RefPtr_createx(
		(U32)(sizeof(DepthStencil) + UnifiedTextureImageExt_size + sizeof(UnifiedTextureImage)),
		(ObjectFreeFunc) GraphicsDevice_freeDepthStencil,
		(ETypeId) EGraphicsTypeId_DepthStencil,
		depthStencilRef
	);

	if(err.genericError)
		return err;

	gotoIfError(clean, GraphicsDeviceRef_inc(deviceRef))

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

	gotoIfError(clean, UnifiedTexture_create(*depthStencilRef, name))

clean:

	if(err.genericError)
		DepthStencilRef_dec(depthStencilRef);

	return err;
}
