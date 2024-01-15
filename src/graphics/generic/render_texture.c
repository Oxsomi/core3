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

#include "graphics/generic/render_texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/pipeline_structs.h"
#include "types/error.h"
#include "types/string.h"

Error RenderTextureRef_dec(RenderTextureRef **renderTexture) {
	return !RefPtr_dec(renderTexture) ? Error_invalidOperation(0, "RenderTextureRef_dec()::renderTexture is invalid") : 
	Error_none();
}

Error RenderTextureRef_inc(RenderTextureRef *renderTexture) {
	return !RefPtr_inc(renderTexture) ? Error_invalidOperation(0, "RenderTextureRef_inc()::renderTexture is invalid") : 
	Error_none();
}

impl Error GraphicsDeviceRef_createRenderTextureExt(
	GraphicsDeviceRef *deviceRef, 
	ERenderTextureType type,
	I32x4 size, 
	ETextureFormat format, 
	ERenderTextureUsage usage,
	EMSAASamples msaa,
	CharString name,
	RenderTexture *renderTexture
);

impl Bool GraphicsDevice_freeRenderTextureExt(RenderTexture *data, Allocator alloc);

impl extern const U64 RenderTextureExt_size;

Bool GraphicsDevice_freeRenderTexture(RenderTexture *renderTexture, Allocator alloc) {
	GraphicsDevice_freeRenderTextureExt(renderTexture, alloc);
	GraphicsDeviceRef_dec(&renderTexture->device);
	return true;
}

Error GraphicsDeviceRef_createRenderTexture(
	GraphicsDeviceRef *deviceRef, 
	ERenderTextureType type,
	I32x4 size, 
	ETextureFormatId formatId, 
	ERenderTextureUsage usage,
	EMSAASamples msaa,
	CharString name,
	RenderTextureRef **renderTextureRef
) {

	if(!deviceRef || deviceRef->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createRenderTexture()::deviceRef is required");

	if (formatId >= ETextureFormatId_Count)
		return Error_invalidParameter(3, 0, "GraphicsDeviceRef_createRenderTexture()::format is out of bounds");

	if (type >= ERenderTextureType_Count)
		return Error_invalidParameter(1, 0, "GraphicsDeviceRef_createRenderTexture()::type is out of bounds");

	if(I32x4_any(I32x4_lt(size, I32x4_zero())))
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createRenderTexture()::size should be >= 0");

	if(I32x2_any(I32x2_eq(I32x2_fromI32x4(size), I32x2_zero())))
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createRenderTexture()::size.xy should be > 0");

	if(I32x4_w(size) != 0)
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createRenderTexture()::size.w should be 0");

	if(type != ERenderTextureType_3D && I32x4_z(size) != 0)
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createRenderTexture()::size.z should be 0 if type isn't 3D");

	if(type == ERenderTextureType_3D && I32x4_z(size) == 0)
		return Error_invalidParameter(2, 0, "GraphicsDeviceRef_createRenderTexture()::size.z should not be 0 if type is 3D");

	if(type != ERenderTextureType_2D)			//TODO: Implement 3D and cube textures
		return Error_unsupportedOperation(
			0, "GraphicsDeviceRef_createRenderTexture()::type: currently only 2D images are supported"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(msaa == EMSAASamples_x2Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA2x))
		return Error_unsupportedOperation(1, "GraphicsDeviceRef_createRenderTexture()::msaa MSAA2x is unsupported");

	else if(msaa == EMSAASamples_x8Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA8x))
		return Error_unsupportedOperation(2, "GraphicsDeviceRef_createRenderTexture()::msaa MSAA8x is unsupported");

	else if(msaa == EMSAASamples_x16Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA16x))
		return Error_unsupportedOperation(3, "GraphicsDeviceRef_createRenderTexture()::msaa MSAA16x is unsupported");

	if(msaa && usage & ERenderTextureUsage_ShaderRW)
		return Error_unsupportedOperation(
			4, "GraphicsDeviceRef_createRenderTexture()::msaa isn't allowed when ShaderRead or Write is enabled"
		);

	ETextureFormat format = ETextureFormatId_unpack[formatId];
	if(!GraphicsDeviceInfo_supportsRenderTextureFormat(&device->info, format))
		return Error_unsupportedOperation(0, "GraphicsDeviceRef_createRenderTexture()::format is unsupported or compressed");

	Error err = RefPtr_createx(
		(U32)(sizeof(RenderTexture) + RenderTextureExt_size), 
		(ObjectFreeFunc) GraphicsDevice_freeRenderTexture, 
		EGraphicsTypeId_RenderTexture, 
		renderTextureRef
	);

	if(err.genericError)
		return err;

	RenderTexture *renderTexture = RenderTextureRef_ptr(*renderTextureRef);
	_gotoIfError(clean, GraphicsDeviceRef_createRenderTextureExt(
		deviceRef, type, size, format, usage, msaa, name, renderTexture
	));

clean:

	if(err.genericError)
		RenderTextureRef_dec(renderTextureRef);

	return err;
}
