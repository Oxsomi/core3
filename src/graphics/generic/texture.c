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

#include "graphics/generic/texture.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/resource.h"
#include "formats/texture.h"
#include "types/string.h"

//Specifying DeviceResourceVersion* will force lock the resource to get the texture format, size and version id.
//When those properties aren't read, the lock isn't important.
//depthFormat is safe to read without lock for example, because only swapchain can change format at resize time.
//If the format doesn't need a lock, it will not return anything into *version, thus version->resource will remain unchanged.
UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version) {

	if(version)
		*version = (DeviceResourceVersion) { 0 };

	switch (tex ? tex->typeId : ETypeId_Undefined) {

		default:								return NULL;
		case EGraphicsTypeId_DeviceTexture:		return &DeviceTextureRef_ptr(tex)->base;
		case EGraphicsTypeId_RenderTexture:		return RenderTextureRef_ptr(tex);
		case EGraphicsTypeId_DepthStencil:		return DepthStencilRef_ptr(tex);

		case EGraphicsTypeId_Swapchain: {

			UnifiedTexture *utex = &SwapchainRef_ptr(tex)->base;

			if(!version)
				return utex;

			Swapchain *swapchain = SwapchainRef_ptr(tex);

			ELockAcquire acq = !version ? ELockAcquire_AlreadyLocked : Lock_lock(&swapchain->lock, U64_MAX);

			if(acq < ELockAcquire_Success)
				return NULL;

			if(version)
				*version = (DeviceResourceVersion) {
					.resource = tex,
					.version = swapchain->versionId,
					.size = I32x2_create2(utex->width, utex->height),
					.format = utex->textureFormatId
				};

			if(acq == ELockAcquire_Acquired)
				Lock_unlock(&swapchain->lock);

			return utex;
		}
	}
}

UnifiedTextureImage *TextureRef_getImageIntern(TextureRef *ref, U32 subResource, U8 imageId) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);	//No lock is required, imageCount stays the same

	if(!tex || imageId >= tex->images)
		return NULL;

	if(subResource)				//TODO: subResource
		return NULL;

	return (UnifiedTextureImage*)((U8*)tex + sizeof(*tex) + sizeof(UnifiedTextureImage) * imageId);
}

void *TextureRef_getImplExt(TextureRef *ref) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!tex)
		return NULL;

	//TODO: subResource
	return (UnifiedTextureImage*)(
		(U8*)tex + sizeof(*tex) + (sizeof(UnifiedTextureImage) + UnifiedTextureImageExt_size) * tex->images
	);
}

UnifiedTextureImage TextureRef_getImage(TextureRef *ref, U32 subResource, U8 imageId) {

	UnifiedTextureImage *img = TextureRef_getImageIntern(ref, subResource, imageId);

	if(!img)
		return (UnifiedTextureImage) { 0 };

	return *img;
}

UnifiedTextureImage TextureRef_getCurrImage(TextureRef *ref, U32 subResource) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!tex)
		return (UnifiedTextureImage) { 0 };

	UnifiedTextureImage *img = TextureRef_getImageIntern(ref, subResource, tex->currentImageId);

	if(!img)
		return (UnifiedTextureImage) { 0 };

	return *img;
}

void *TextureRef_getImgExt(TextureRef *ref, U32 subResource, U8 imageId) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);	//No lock is required, imageCount stays the same

	if(!tex || imageId >= tex->images)
		return NULL;

	if(subResource)				//TODO: subResource
		return NULL;

	return (UnifiedTextureImage*)(
		(U8*)tex + sizeof(*tex) + sizeof(UnifiedTextureImage) * tex->images + UnifiedTextureImageExt_size * imageId
	);
}

void *TextureRef_getCurrImgExt(TextureRef *ref, U32 subResource) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!tex)
		return NULL;

	return TextureRef_getImgExt(ref, subResource, tex->currentImageId);
}

UnifiedTexture TextureRef_getUnifiedTexture(TextureRef *tex, DeviceResourceVersion *version) {

	UnifiedTexture *utexPtr = TextureRef_getUnifiedTextureIntern(tex, version);

	if(!utexPtr)
		return (UnifiedTexture) { 0 };

	UnifiedTexture utex = *utexPtr;

	if (version && version->resource) {
		utex.width = (U16) I32x2_x(version->size);
		utex.height = (U16) I32x2_y(version->size);
		utex.textureFormatId = version->format;
	}

	return utex;
}

U32 TextureRef_getReadHandle(TextureRef *tex, U32 subResource, U8 imageId) {
	return TextureRef_getImage(tex, subResource, imageId).readHandle;
}

U32 TextureRef_getWriteHandle(TextureRef *tex, U32 subResource, U8 imageId) {
	return TextureRef_getImage(tex, subResource, imageId).writeHandle;
}

U32 TextureRef_getCurrReadHandle(TextureRef *tex, U32 subResource) {
	return TextureRef_getCurrImage(tex, subResource).readHandle;
}

U32 TextureRef_getCurrWriteHandle(TextureRef *tex, U32 subResource) {
	return TextureRef_getCurrImage(tex, subResource).writeHandle;
}

Bool TextureRef_isTexture(RefPtr *tex) { return TextureRef_getUnifiedTexture(tex, NULL).resource.device; }
Bool TextureRef_isDepthStencil(TextureRef *tex) { return tex && tex->typeId == EGraphicsTypeId_DepthStencil; }

Bool TextureRef_isRenderTargetWritable(TextureRef *tex) {
	return tex && (tex->typeId == EGraphicsTypeId_RenderTexture || tex->typeId == EGraphicsTypeId_Swapchain);
}

impl Bool UnifiedTexture_freeExt(TextureRef *textureRef);
impl Error UnifiedTexture_createExt(TextureRef *textureRef, CharString name);

Bool UnifiedTexture_free(TextureRef *textureRef) {

	UnifiedTexture *texture = TextureRef_getUnifiedTextureIntern(textureRef, NULL);
	GraphicsDeviceRef *deviceRef = texture->resource.device;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ELockAcquire acq = Lock_lock(&device->descriptorLock, U64_MAX);

	if (acq >= ELockAcquire_Success) {

		UnifiedTextureImage *img = TextureRef_getImageIntern(textureRef, 0, 0);

		ListU32 allocations = (ListU32) { 0 };
		ListU32_createRefConst((const U32*)img, texture->images * 2, &allocations);
		GraphicsDeviceRef_freeDescriptors(deviceRef, &allocations);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&device->descriptorLock);
	}

	Bool success = UnifiedTexture_freeExt(textureRef);
	success &= GraphicsResource_free(&texture->resource, textureRef);

	texture->resource = (GraphicsResource) { 0 };
	return success;
}

Error UnifiedTexture_create(TextureRef *ref, CharString name) {

	UnifiedTexture *texturePtr = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!texturePtr)
		return Error_nullPointer(0, "UnifiedTexture_create()::texturePtr is required");

	const UnifiedTexture texture = *texturePtr;

	if(texture.resource.allocated)
		return Error_nullPointer(0, "UnifiedTexture_create()::texturePtr contains initialized resource, possible memleak");

	if(!texture.resource.device || texture.resource.device->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "UnifiedTexture_create()::texturePtr->resource.device is required");

	if(!texture.depthFormat && !texture.textureFormatId)
		return Error_nullPointer(0, "UnifiedTexture_create()::texturePtr->depthFormat or textureFormatId is required");

	if(texture.textureFormatId && texture.textureFormatId >= ETextureFormatId_Count)
		return Error_invalidParameter(2, 0, "UnifiedTexture_create()::texturePtr->textureFormatId is invalid");

	if(texture.depthFormat && texture.depthFormat >= EDepthStencilFormat_Count)
		return Error_invalidParameter(2, 0, "UnifiedTexture_create()::texturePtr->depthFormat is required");

	if(texture.sampleCount >= EMSAASamples_Count)
		return Error_invalidParameter(2, 0, "UnifiedTexture_create()::texturePtr->sampleCount is invalid");

	if(texture.type >= ETextureType_Count)
		return Error_invalidParameter(1, 0, "UnifiedTexture_create()::texturePtr->type is invalid");

	if(texture.resource.type == EResourceType_DeviceTexture && texture.sampleCount)
		return Error_invalidParameter(1, 0, "UnifiedTexture_create()::texturePtr->msaa isn't allowed on a DeviceTexture");

	if (texture.resource.type == EResourceType_Swapchain) {
		if(texture.images != 3)
			return Error_invalidParameter(1, 0, "UnifiedTexture_create()::texturePtr->images is only allowed to be 3 swapchains");
	}

	else if(texture.images != 1)
		return Error_invalidParameter(1, 0, "UnifiedTexture_create()::texturePtr->images is only allowed to be 1 swapchains");

	if(texture.resource.flags & EGraphicsResourceFlag_CPUAllocated && texture.resource.type != EResourceType_DeviceTexture)
		return Error_invalidParameter(
			1, 0, "UnifiedTexture_create()::texturePtr->flags CPU flags are only allowed for DeviceTexture"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture.resource.device);
	const GraphicsDeviceInfo *info = &device->info;

	ETextureFormat format = ETextureFormat_Undefined;				//Only valid for non depth stencils

	if(texture.textureFormatId) {

		format = ETextureFormatId_unpack[texture.textureFormatId];

		if(texture.resource.type == EResourceType_DeviceTexture) {

			if(!GraphicsDeviceInfo_supportsFormat(info, format))
				return Error_invalidParameter(2, 0, "UnifiedTexture_create() format is unsupported");
		}

		else if(!GraphicsDeviceInfo_supportsRenderTextureFormat(info, format))
			return Error_invalidParameter(2, 0, "UnifiedTexture_create() format is unsupported as render texture");
	}

	else if(!GraphicsDeviceInfo_supportsDepthStencilFormat(info, (EDepthStencilFormat) texture.depthFormat))
		return Error_invalidParameter(2, 0, "UnifiedTexture_create() depthFormat is unsupported as depth texture");

	if(!texture.width || !texture.height || !texture.length || !texture.levels)
		return Error_invalidParameter(
			!texture.width ? 5 : (!texture.height ? 6 : (!texture.length ? 7 : 8)), 0,
			"UnifiedTexture_create()::texturePtr->width, height, depth and levels are required"
		);

	if(texture.levels > 1)
		return Error_invalidParameter(2, 0, "UnifiedTexture_create()::texturePtr->levels > 1 isn't supported yet");		//TODO:

	if(texture.width > 16384 || texture.height > 16384 || texture.length > 256)
		return Error_invalidParameter(
			texture.width > 16384 ? 5 : (texture.height > 16384 ? 6 : 7), 0,
			"UnifiedTexture_create()::texturePtr->width, height and or length exceed limit (16384, 16384 and 256 respectively)"
		);

	if(texture.length > 1 && texture.type != ETextureType_3D)
		return Error_invalidParameter(7, 0, "UnifiedTexture_create()::texturePtr->length can't be non zero if type isn't 3D");

	if(texture.type != ETextureType_2D)
		return Error_invalidParameter(1, 0, "UnifiedTexture_create()::texturePtr->type only supports 2D for now");		//TODO:

	if(texture.sampleCount == EMSAASamples_x2Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA2x))
		return Error_unsupportedOperation(1, "UnifiedTexture_create()::texturePtr->sampleCount MSAA2x is unsupported");

	else if(texture.sampleCount == EMSAASamples_x8Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA8x))
		return Error_unsupportedOperation(2, "UnifiedTexture_create()::texturePtr->sampleCount MSAA8x is unsupported");

	else if(texture.sampleCount == EMSAASamples_x16Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA16x))
		return Error_unsupportedOperation(3, "UnifiedTexture_create()::texturePtr->sampleCount MSAA16x is unsupported");

	if(texture.sampleCount && (texture.resource.flags & EGraphicsResourceFlag_ShaderRW))
		return Error_unsupportedOperation(
			4, "UnifiedTexture_create()::texturePtr->sampleCount isn't allowed when ShaderRead or Write is enabled"
		);

	//Allocate in descriptors

	Error err = Error_none();
	ELockAcquire acq = ELockAcquire_Invalid;

	if(texture.resource.flags & EGraphicsResourceFlag_ShaderRW) {

		acq = Lock_lock(&device->descriptorLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			_gotoIfError(clean, Error_invalidState(0, "UnifiedTexture_create() couldn't acquire descriptor lock"));

		//Create images

		for(U8 i = 0; i < texture.images; ++i) {

			UnifiedTextureImage *img = TextureRef_getImageIntern(ref, 0 /* TODO: subResource */, i);

			if(texture.resource.flags & EGraphicsResourceFlag_ShaderRead) {

				U32 locationRead = GraphicsDeviceRef_allocateDescriptor(
					texture.resource.device,
					texture.type == ETextureType_2D ? EDescriptorType_Texture2D : EDescriptorType_Texture3D
				);

				if(locationRead == U32_MAX)
					_gotoIfError(clean, Error_outOfMemory(0, "UnifiedTexture_create() couldn't allocate texture descriptor"));

				img->readHandle = locationRead;
			}

			if(texture.resource.flags & EGraphicsResourceFlag_ShaderWrite) {		//Not for DepthStencil

				EDescriptorType descType = UnifiedTexture_getWriteDescriptorType(texture);
				U32 locationWrite = GraphicsDeviceRef_allocateDescriptor(texture.resource.device, descType);

				if(locationWrite == U32_MAX)
					_gotoIfError(clean, Error_outOfMemory(0, "UnifiedTexture_create() couldn't allocate image descriptor"));

				img->writeHandle = locationWrite;
			}
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&device->descriptorLock);

		acq = ELockAcquire_Invalid;
	}

	_gotoIfError(clean, UnifiedTexture_createExt(ref, name));

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->descriptorLock);

	return err;
}

EDescriptorType UnifiedTexture_getWriteDescriptorType(UnifiedTexture texture) {

	ETexturePrimitive prim = ETextureFormat_getPrimitive(ETextureFormatId_unpack[texture.textureFormatId]);

	if (texture.type != ETextureType_2D)
		switch (prim) {
			default:						return EDescriptorType_RWTexture3D;
			case ETexturePrimitive_SNorm:	return EDescriptorType_RWTexture3Ds;
			case ETexturePrimitive_UInt:	return EDescriptorType_RWTexture3Du;
			case ETexturePrimitive_SInt:	return EDescriptorType_RWTexture3Di;
			case ETexturePrimitive_Float:	return EDescriptorType_RWTexture3Df;
		}

	switch (prim) {
		default:							return EDescriptorType_RWTexture2D;
		case ETexturePrimitive_SNorm:		return EDescriptorType_RWTexture2Ds;
		case ETexturePrimitive_UInt:		return EDescriptorType_RWTexture2Du;
		case ETexturePrimitive_SInt:		return EDescriptorType_RWTexture2Di;
		case ETexturePrimitive_Float:		return EDescriptorType_RWTexture2Df;
	}
}
