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

//graphics/generic/texture.c

#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/resource.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/pipeline_structs.h"
#include "formats/oiSH/sh_registers.h"
#include "types/container/texture_format.h"
#include "types/base/string_base.h"
#include "types/base/error.h"
#include "types/math/vec2i.h"
#include "types/base/constants.h"

//Specifying DeviceResourceVersion* will force lock the resource to get the texture format, size and version id.
//When those properties aren't read, the lock isn't important.
//depthFormat is safe to read without lock for example, because only swapchain can change format at resize time.
//If the format doesn't need a lock, it will not return anything into *version, thus version->resource will remain unchanged.
UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version) {

	if(version)
		*version = (DeviceResourceVersion) { 0 };

	EGraphicsTypeId graphicsTypeId = (EGraphicsTypeId) (tex ? tex->refPtrType->typeId : ETypeId_Undefined);

	switch (graphicsTypeId) {

		default:                             return NULL;
		case EGraphicsTypeId_DeviceTexture:  return &DeviceTextureRef_ptr(tex)->base;
		case EGraphicsTypeId_RenderTexture:  return RenderTextureRef_ptr(tex);
		case EGraphicsTypeId_DepthStencil:   return DepthStencilRef_ptr(tex);

		case EGraphicsTypeId_Swapchain: {

			UnifiedTexture *utex = &SwapchainRef_ptr(tex)->base;

			if(!version)
				return utex;

			Swapchain *swapchain = SwapchainRef_ptr(tex);

			const ELockAcquire acq = !version ? ELockAcquire_AlreadyLocked : SpinLock_lock(&swapchain->lock, U64_MAX);

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
				SpinLock_unlock(&swapchain->lock);

			return utex;
		}
	}
}

Bool TextureRef_pullRegion(
	TextureRef *tex,
	U16 x, U16 y, U16 z,
	U16 w, U16 h, U16 l,
	U8 plane,
	TexturePullCallback callback, void *context, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = NULL;

	GraphicsDevice *device = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;
	Bool owned = false;

	//Validated before anything is dereferenced, since a wrong type would read a garbage device

	const EGraphicsTypeId typeId = (EGraphicsTypeId) (tex ? tex->refPtrType->typeId : ETypeId_Undefined);

	if(
		typeId != EGraphicsTypeId_RenderTexture && typeId != EGraphicsTypeId_DepthStencil &&
		typeId != EGraphicsTypeId_Swapchain
	)
		retError(clean, Error_nullPointer(
			0, "TextureRef_pullRegion()::tex needs to be a RenderTexture, DepthStencil or Swapchain"
		));

	if(!callback)
		retError(clean, Error_nullPointer(
			7, "TextureRef_pullRegion()::callback is required, since the data has nowhere else to land"
		));

	const UnifiedTexture utex = TextureRef_getUnifiedTexture(tex, NULL);

	if(utex.sampleCount)
		retError(clean, Error_invalidOperation(0, "TextureRef_pullRegion() doesn't support MSAA, resolve first"));

	//One pull carries one plane.
	//getPullBytes returning 0 is what says the format doesn't have the requested one,
	// which covers plane 1 on anything single planar and any plane past 1.

	if(plane && !utex.depthFormat)
		retError(clean, Error_invalidParameter(
			7, plane, "TextureRef_pullRegion()::plane has to be 0 for color formats"
		));

	if(utex.depthFormat && !EDepthStencilFormat_getPullBytes((EDepthStencilFormat) utex.depthFormat, plane))
		retError(clean, Error_invalidParameter(
			7, plane, "TextureRef_pullRegion()::plane doesn't exist on this format"
		));

	if(x >= utex.width || y >= utex.height || z >= utex.length)
		retError(clean, Error_outOfBounds(1, x, utex.width, "TextureRef_pullRegion()::x, y or z out of bounds"));

	if(!w)
		w = utex.width - x;

	if(!h)
		h = utex.height - y;

	if(!l)
		l = utex.length - z;

	if(x + w > utex.width || y + h > utex.height || z + l > utex.length)
		retError(clean, Error_outOfBounds(4, x + w, utex.width, "TextureRef_pullRegion() region out of bounds"));

	//Render targets are never block compressed, so no block snapping is needed here.
	//The destination is allocated now so a failed allocation surfaces here instead of at completion,
	// which lets the callback promise it always carries data.

	alloc = GraphicsDeviceRef_getAlloc(utex.resource.device);
	device = GraphicsDeviceRef_ptr(utex.resource.device);

	const U64 texel =
		utex.depthFormat ? EDepthStencilFormat_getPullBytes((EDepthStencilFormat) utex.depthFormat, plane) :
		ETextureFormat_getSize(ETextureFormatId_unpack[utex.textureFormatId], 1, 1, 1);

	Buffer data = Buffer_createNull();
	gotoIfError3(clean, Buffer_createUninitializedBytes(texel * w * h * l, alloc, &data, e_rr));

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success) {
		Buffer_free(&data, alloc);
		retError(clean, Error_invalidOperation(1, "TextureRef_pullRegion() couldn't acquire device lock"));
	}

	RefPtr_inc(tex);
	owned = true;

	const DevicePendingPull pull = (DevicePendingPull) {
		.resource = tex,
		.textureCallback = callback,
		.context = context,
		.range = (DevicePendingRange) { .texture = (TextureRange) {
			.startRange = { x, y, z },
			.endRange = { (U16)(x + w), (U16)(y + h), (U16)(z + l) },
			.planeId = plane
		} },
		.textureData = data
	};

	if(!ListDevicePendingPull_pushBack(&device->pendingPulls, pull, alloc, e_rr)) {
		Buffer_free(&data, alloc);
		s_uccess = false;
		goto clean;
	}

	owned = false;

clean:

	if(owned)
		RefPtr_dec(&tex);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess;
}

UnifiedTextureImage *TextureRef_getImageIntern(TextureRef *ref, U32 subResource, U8 imageId) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);    //No lock is required, imageCount stays the same

	if(!tex || imageId >= tex->images)
		return NULL;

	if(subResource)                //TODO: subResource
		return NULL;

	return (UnifiedTextureImage*)((U8*)tex + sizeof(*tex) + sizeof(UnifiedTextureImage) * imageId);
}

void *TextureRef_getImplExt(TextureRef *ref) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!tex)
		return NULL;

	//Avoid movement of SwapchainExt for example, by allowing a fixed reservation of images.

	const GraphicsObjectSize imageSize = GraphicsDeviceRef_getObjectSizes(tex->resource.device)->image;
	const U8 reserved = UnifiedTexture_reservedImages(tex->images, tex->maxImages);

	//Behind the last image ext block, rounded to a cache line so the backend's own struct starts aligned.

	//TODO: subResource
	const U64 implExt = GraphicsObjectSize_alignUp(
		(U64) (void*) (
			UnifiedTexture_imageExtBase(tex, imageSize, reserved) + GraphicsObjectSize_stride(imageSize) * reserved
		),
		64
	);

	return (UnifiedTextureImage*) (void*) implExt;
}

UnifiedTextureImage TextureRef_getImage(TextureRef *ref, U32 subResource, U8 imageId) {

	const UnifiedTextureImage *img = TextureRef_getImageIntern(ref, subResource, imageId);

	if(!img)
		return (UnifiedTextureImage) { 0 };

	return *img;
}

UnifiedTextureImage TextureRef_getCurrImage(TextureRef *ref, U32 subResource) {

	const UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!tex)
		return (UnifiedTextureImage) { 0 };

	const UnifiedTextureImage *img = TextureRef_getImageIntern(ref, subResource, tex->currentImageId);

	if(!img)
		return (UnifiedTextureImage) { 0 };

	return *img;
}

void *TextureRef_getImgExt(TextureRef *ref, U32 subResource, U8 imageId) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);    //No lock is required, imageCount stays the same

	if(!tex || imageId >= tex->images)
		return NULL;

	if(subResource)                //TODO: subResource
		return NULL;

	//Reserved rather than images: a swapchain reserves maxImages worth of both arrays, so the ext region
	// starts past the reservation and not past however many images are live right now.
	//This used to disagree with TextureRef_getImplExt, which always reserved.

	const GraphicsObjectSize imageSize = GraphicsDeviceRef_getObjectSizes(tex->resource.device)->image;
	const U8 reserved = UnifiedTexture_reservedImages(tex->images, tex->maxImages);

	return (UnifiedTextureImage*) (
		UnifiedTexture_imageExtBase(tex, imageSize, reserved) +
		GraphicsObjectSize_stride(imageSize) * imageId
	);
}

void *TextureRef_getCurrImgExt(TextureRef *ref, U32 subResource) {

	UnifiedTexture *tex = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!tex)
		return NULL;

	return TextureRef_getImgExt(ref, subResource, tex->currentImageId);
}

const UnifiedTexture *TextureRef_getUnifiedTextureFast(TextureRef *tex) {
	return TextureRef_getUnifiedTextureIntern(tex, NULL);
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
Bool TextureRef_isDepthStencil(TextureRef *tex) {
	return tex && tex->refPtrType->typeId == (TypeId) EGraphicsTypeId_DepthStencil;
}

Bool TextureRef_isRenderTargetWritable(TextureRef *tex) {
	return tex && (
		tex->refPtrType->typeId == (TypeId) EGraphicsTypeId_RenderTexture ||
		tex->refPtrType->typeId == (TypeId) EGraphicsTypeId_Swapchain
	);
}

void UnifiedTexture_free(TextureRef *textureRef) {

	UnifiedTexture *texture = TextureRef_getUnifiedTextureIntern(textureRef, NULL);
	GraphicsDeviceRef *device = texture->resource.device;

	//A half built texture (its create failed before the device was attached) owns nothing: the ext
	// free and the resource free both dispatch through the device, so a zeroed object must stop here.

	if(!device)
		return;

	if(texture->resource.flags & EGraphicsResourceFlag_ExposeBindless) {

		const UnifiedTextureImage *img = TextureRef_getImageIntern(textureRef, 0, 0);

		if(texture->bindlessDescriptorTable)
			for(U8 i = 0; i < texture->images; ++i) {
				GraphicsDeviceRef_freeDescriptorBindless(device, texture->bindlessDescriptorTable, img[i].readHandle, NULL);
				GraphicsDeviceRef_freeDescriptorBindless(device, texture->bindlessDescriptorTable, img[i].writeHandle, NULL);
				RefPtr_dec(&texture->bindlessDescriptorTable);
			}
	}

	UnifiedTexture_freeExt(textureRef);
	GraphicsResource_free(&texture->resource, textureRef);

	texture->resource = (GraphicsResource) { 0 };
}

Bool UnifiedTexture_create(TextureRef *ref, DescriptorTableRef *bindlessDescriptorTable, const CharString *name, Error *e_rr) {

	Bool s_uccess = true;

	UnifiedTexture *texturePtr = TextureRef_getUnifiedTextureIntern(ref, NULL);

	if(!texturePtr)
		retError(clean, Error_nullPointer(0, "UnifiedTexture_create()::texturePtr is required"));

	const UnifiedTexture texture = *texturePtr;

	if(texture.resource.allocated)
		retError(clean, Error_nullPointer(
			0, "UnifiedTexture_create()::texturePtr contains initialized resource, possible memleak"
		));

	if(!texture.resource.device || texture.resource.device->refPtrType->typeId != (TypeId)EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "UnifiedTexture_create()::texturePtr->resource.device is required"));

	if(bindlessDescriptorTable && !(texture.resource.flags & EGraphicsResourceFlag_ExposeBindless))
		retError(clean, Error_invalidState(0, "UnifiedTexture_create() bindlessDescriptorTable is set, but disallowed"));

	if(bindlessDescriptorTable && bindlessDescriptorTable->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(0, "UnifiedTexture_create()::bindlessDescriptorTable should be valid if non NULL"));

	if ((texture.resource.flags & EGraphicsResourceFlag_ExposeBindless) && !bindlessDescriptorTable)
		bindlessDescriptorTable = GraphicsDeviceRef_ptr(texture.resource.device)->defaultDescriptorTable;

	if(!bindlessDescriptorTable && (texture.resource.flags & EGraphicsResourceFlag_ExposeBindless))
		retError(clean, Error_invalidState(0, "UnifiedTexture_create() can't expose resource for bindless without bindless"));

	if(!texture.depthFormat && !texture.textureFormatId)
		retError(clean, Error_nullPointer(
			0, "UnifiedTexture_create()::texturePtr->depthFormat or textureFormatId is required"
		));

	if(texture.textureFormatId && texture.textureFormatId >= ETextureFormatId_Count)
		retError(clean, Error_invalidParameter(2, 0, "UnifiedTexture_create()::texturePtr->textureFormatId is invalid"));

	if(texture.depthFormat && texture.depthFormat >= EDepthStencilFormat_Count)
		retError(clean, Error_invalidParameter(2, 0, "UnifiedTexture_create()::texturePtr->depthFormat is required"));

	if(texture.sampleCount >= EMSAASamples_Count)
		retError(clean, Error_invalidParameter(2, 0, "UnifiedTexture_create()::texturePtr->sampleCount is invalid"));

	if(texture.sampleCount && texture.levels > 1)
		retError(clean, Error_invalidParameter(2, 0, "UnifiedTexture_create() MSAA textures can't have mips"));

	if(texture.type >= ETextureType_Count)
		retError(clean, Error_invalidParameter(1, 0, "UnifiedTexture_create()::texturePtr->type is invalid"));

	if(texture.resource.type == EResourceType_DeviceTexture && texture.sampleCount)
		retError(clean, Error_invalidParameter(
			1, 0, "UnifiedTexture_create()::texturePtr->msaa isn't allowed on a DeviceTexture"
		));

	if (texture.resource.type == EResourceType_Swapchain) {

		//A swapchain that owns its images holds one per frame in flight, which is two on Android, so it is held
		// to the device's count rather than to the floor a presentation engine hands a physical one.

		if (texture.resource.flags & EGraphicsResourceFlag_InternalOwnsImages) {
			if(texture.images != GraphicsDeviceRef_ptr(texture.resource.device)->framesInFlight)
				retError(clean, Error_invalidParameter(
					1, 0, "UnifiedTexture_create()::texturePtr->images has to be framesInFlight for a virtual swapchain"
				));
		}

		else if(texture.images < 3 || texture.images > 5)
			retError(clean, Error_invalidParameter(
				1, 0, "UnifiedTexture_create()::texturePtr->images is only allowed to be 3-5 swapchains"
			));
	}

	else if(texture.images != 1)
		retError(clean, Error_invalidParameter(
			1, 0, "UnifiedTexture_create()::texturePtr->images is only allowed to be 1 swapchains"
		));

	if(texture.resource.flags & EGraphicsResourceFlag_CPUAllocated && texture.resource.type != EResourceType_DeviceTexture)
		retError(clean, Error_invalidParameter(
			1, 0, "UnifiedTexture_create()::texturePtr->flags CPU flags are only allowed for DeviceTexture"
		));

	GraphicsDeviceRef *deviceRef = texture.resource.device;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const GraphicsDeviceInfo *info = &device->info;

	if(texture.textureFormatId) {

		const ETextureFormat format = ETextureFormatId_unpack[texture.textureFormatId];

		if(texture.resource.type == EResourceType_DeviceTexture) {

			if(!GraphicsDeviceInfo_supportsFormat(info, format))
				retError(clean, Error_invalidParameter(2, 0, "UnifiedTexture_create() format is unsupported"));
		}

		else if(!GraphicsDeviceInfo_supportsRenderTextureFormat(info, format))
			retError(clean, Error_invalidParameter(
				2, 0, "UnifiedTexture_create() format is unsupported as render texture"
			));
	}

	else if(!GraphicsDeviceInfo_supportsDepthStencilFormat(info, (EDepthStencilFormat) texture.depthFormat))
		retError(clean, Error_invalidParameter(
			2, 0, "UnifiedTexture_create() depthFormat is unsupported as depth texture"
		));

	if(!texture.width || !texture.height || !texture.length || !texture.levels)
		retError(clean, Error_invalidParameter(
			!texture.width ? 5 : (!texture.height ? 6 : (!texture.length ? 7 : 8)), 0,
			"UnifiedTexture_create()::texturePtr->width, height, depth and levels are required"
		));

	if(texture.levels > 1)
		retError(clean, Error_invalidParameter(
			2, 0, "UnifiedTexture_create()::texturePtr->levels > 1 isn't supported yet"  //TODO:
		));

	if(texture.width > 16384 || texture.height > 16384 || texture.length > 256)
		retError(clean, Error_invalidParameter(
			texture.width > 16384 ? 5 : (texture.height > 16384 ? 6 : 7), 0,
			"UnifiedTexture_create()::texturePtr->width, height and or length exceed limit "
			"(16384, 16384 and 256 respectively)"
		));

	if(texture.length > 1 && texture.type != ETextureType_3D)
		retError(clean, Error_invalidParameter(
			7, 0, "UnifiedTexture_create()::texturePtr->length can't be non zero if type isn't 3D"
		));

	if(texture.type != ETextureType_2D)
		retError(clean, Error_invalidParameter(
			1, 0, "UnifiedTexture_create()::texturePtr->type only supports 2D for now" //TODO:
		));

	if(texture.sampleCount == EMSAASamples_x2Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA2x))
		retError(clean, Error_unsupportedOperation(
			1, "UnifiedTexture_create()::texturePtr->sampleCount MSAA2x is unsupported"
		));

	if(texture.sampleCount == EMSAASamples_x8Ext && !(device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA8x))
		retError(clean, Error_unsupportedOperation(
			2, "UnifiedTexture_create()::texturePtr->sampleCount MSAA8x is unsupported"
		));

	if(
		texture.sampleCount && (texture.resource.flags & EGraphicsResourceFlag_ShaderWrite) &&
		!(device->info.capabilities.features & EGraphicsFeatures_WriteMSTexture)
	)
		retError(clean, Error_unsupportedOperation(
			4,
			"UnifiedTexture_create()::texturePtr->sampleCount isn't allowed when ShaderWrite is enabled, "
			"but the feature isn't enabled"
		));

	if(bindlessDescriptorTable) {

		if(!RefPtr_inc(bindlessDescriptorTable))
			retError(clean, Error_invalidState(0, "UnifiedTexture_create()::bindlessDescriptorTable was invalid"));

		texturePtr->bindlessDescriptorTable = bindlessDescriptorTable;
	}

	//Create resource

	gotoIfError3(clean, UnifiedTexture_createExt(ref, name, e_rr));

	//Allocate in descriptors

	if(texture.resource.flags & EGraphicsResourceFlag_ExposeBindless) {

		//Create images

		for(U8 i = 0; i < texture.images; ++i) {

			UnifiedTextureImage *img = TextureRef_getImageIntern(ref, 0 /* TODO: subResource */, i);
			Descriptor textureDesc = Descriptor_texture(ref, 0, 0, 0, i, 0, 0);

			if(
				(texture.resource.flags & EGraphicsResourceFlag_ExposeBindlessRead) &&
				!GraphicsDeviceRef_allocateDescriptorBindless(
					deviceRef,
					bindlessDescriptorTable,
					texture.type == ETextureType_2D ? ESHRegisterType_Texture2D : ESHRegisterType_Texture3D,
					0,
					false,
					&textureDesc,
					&img->readHandle,
					e_rr
				)
			)
				retError(clean, Error_invalidState(0, "UnifiedTexture_create() readHandle couldn't be allocated"));

			if(
				(texture.resource.flags & EGraphicsResourceFlag_ExposeBindlessWrite) &&
				!GraphicsDeviceRef_allocateDescriptorBindless(
					deviceRef,
					bindlessDescriptorTable,
					(texture.type == ETextureType_2D ? ESHRegisterType_Texture2D : ESHRegisterType_Texture3D) |
					ESHRegisterType_IsWrite,
					0,
					false,
					&textureDesc,
					&img->writeHandle,
					e_rr
				)
			)
				retError(clean, Error_invalidState(0, "UnifiedTexture_create() writeHandle couldn't be allocated"));
		}
	}

clean:
	return s_uccess;
}
