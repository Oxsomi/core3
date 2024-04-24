/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "graphics/generic/swapchain.h"
#include "graphics/generic/device.h"
#include "platforms/window.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/error.h"

Error SwapchainRef_dec(SwapchainRef **swapchain) {
	return !RefPtr_dec(swapchain) ?
		Error_invalidOperation(0, "SwapchainRef_dec()::swapchain is invalid") : Error_none();
}

Error SwapchainRef_inc(SwapchainRef *swapchain) {
	return !RefPtr_inc(swapchain) ?
		Error_invalidOperation(0, "SwapchainRef_inc()::swapchain is invalid") : Error_none();
}

impl Error GraphicsDeviceRef_createSwapchainExt(GraphicsDeviceRef *dev, SwapchainRef *swapchain);
impl Bool GraphicsDevice_freeSwapchainExt(Swapchain *data, Allocator alloc);
impl extern const U64 SwapchainExt_size;

impl Error UnifiedTexture_createExt(TextureRef *textureRef, CharString name);		//We need to be able to re-create views

Error SwapchainRef_resize(SwapchainRef *swapchainRef) {

	if(!swapchainRef || swapchainRef->typeId != EGraphicsTypeId_Swapchain)
		return Error_nullPointer(0, "Swapchain_resize()::swapchain is required");

	Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(swapchain->base.resource.device);

	if(Lock_lock(&swapchain->lock, U64_MAX) != ELockAcquire_Acquired)
		return Error_invalidState(0, "Swapchain_resize() couldn't lock swapchain");

	//Check if swapchain was in flight. If yes, warn that the user has to flush manually

	Error err = Error_none();
	const ELockAcquire acq = Lock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(0, "Swapchain_resize() can't be called while recording commands"))

	Bool any = false;

	for (U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(ListRefPtr); ++i)
		if (ListRefPtr_contains(device->resourcesInFlight[i], swapchainRef, 0, NULL)) {
			any = true;
			break;
		}

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	if (any)
		gotoIfError(clean, Error_invalidState(
			0, "Swapchain_resize() can't be called on a Swapchain that's in flight. Use GraphicsDeviceRef_wait in onResize"
		))

	//Resize with same format and same size is a NOP

	I32x2 newSize = swapchain->info.window->size;
	EWindowFormat newFormat = swapchain->info.window->format;

	ETextureFormatId textureFormatId = 0;

	switch (newFormat) {
		case EWindowFormat_BGRA8:		textureFormatId = ETextureFormatId_BGRA8;		break;
		case EWindowFormat_BGR10A2:		textureFormatId = ETextureFormatId_BGR10A2;		break;
		case EWindowFormat_RGBA16f:		textureFormatId = ETextureFormatId_RGBA16f;		break;
		case EWindowFormat_RGBA32f:		textureFormatId = ETextureFormatId_RGBA32f;		break;
		default:
			gotoIfError(clean, Error_invalidState(1, "Swapchain_resize() window format is unsupported"))
	}

	if(
		I32x2_x(newSize) == swapchain->base.width && I32x2_y(newSize) == swapchain->base.height &&
		swapchain->base.textureFormatId == textureFormatId
	)
		goto clean;

	//Otherwise, we properly resize

	gotoIfError(clean, GraphicsDeviceRef_createSwapchainExt(swapchain->base.resource.device, swapchainRef))
	gotoIfError(clean, UnifiedTexture_createExt(swapchainRef, swapchain->info.window->title))		//Re-create views
	++swapchain->versionId;

	swapchain->base.textureFormatId = (U8) textureFormatId;
	swapchain->base.width = (U16) I32x2_x(newSize);
	swapchain->base.height = (U16) I32x2_y(newSize);

clean:
	Lock_unlock(&swapchain->lock);
	return err;
}

Bool GraphicsDevice_freeSwapchain(Swapchain *swapchain, Allocator alloc) {
	Bool success = Lock_free(&swapchain->lock);
	success &= GraphicsDevice_freeSwapchainExt(swapchain, alloc);
	success &= UnifiedTexture_free((TextureRef*)((U8*)swapchain - sizeof(RefPtr)));
	return success;
}

Error GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *dev, SwapchainInfo info, Bool allowWriteExt, SwapchainRef **scRef) {

	if(!info.window || !info.window->nativeHandle)
		return Error_nullPointer(
			0, "GraphicsDeviceRef_createSwapchain()::deviceRef and info.window (physical) are required"
		);

	Error err = RefPtr_createx(
		(U32)(sizeof(Swapchain) + (UnifiedTextureImageExt_size + sizeof(UnifiedTextureImage)) * 3 + SwapchainExt_size),
		(ObjectFreeFunc) GraphicsDevice_freeSwapchain,
		(ETypeId) EGraphicsTypeId_Swapchain,
		scRef
	);

	if(err.genericError)
		return err;

	ETextureFormatId formatId = ETextureFormatId_BGRA8;

	switch (info.window->format) {
		case EWindowFormat_BGRA8:		formatId = ETextureFormatId_BGRA8;		break;
		case EWindowFormat_BGR10A2:		formatId = ETextureFormatId_BGR10A2;	break;
		case EWindowFormat_RGBA16f:		formatId = ETextureFormatId_RGBA16f;	break;
		case EWindowFormat_RGBA32f:		formatId = ETextureFormatId_RGBA32f;	break;
		default:
			gotoIfError(clean, Error_invalidState(1, "Swapchain_resize() window format is unsupported"))
	}

	Swapchain *swapchain = SwapchainRef_ptr(*scRef);

	gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	swapchain->info = info;
	swapchain->base = (UnifiedTexture) {
		.resource = (GraphicsResource) {
			.device = dev,
			.flags = allowWriteExt ? EGraphicsResourceFlag_ShaderRW : EGraphicsResourceFlag_ShaderRead,
			.type = (U8) EResourceType_Swapchain
		},
		.textureFormatId = (U8) formatId,
		.type = (U8) ETextureType_2D,
		.width = (U16) I32x2_x(info.window->size),
		.height = (U16) I32x2_y(info.window->size),
		.length = 1,
		.levels = 1,
		.images = 3		//Triple buffering
	};

	if(!swapchain->info.presentModePriorities[0]) {

		#if _PLATFORM_TYPE != PLATFORM_ANDROID
			swapchain->info.presentModePriorities[0] = ESwapchainPresentMode_Mailbox;			//Priority is to be low latency
			swapchain->info.presentModePriorities[1] = ESwapchainPresentMode_Immediate;
			swapchain->info.presentModePriorities[2] = ESwapchainPresentMode_Fifo;
			swapchain->info.presentModePriorities[3] = ESwapchainPresentMode_FifoRelaxed;
		#else
			swapchain->info.presentModePriorities[0] = ESwapchainPresentMode_Fifo;				//Priority is to conserve power
			swapchain->info.presentModePriorities[1] = ESwapchainPresentMode_FifoRelaxed;
			swapchain->info.presentModePriorities[2] = ESwapchainPresentMode_Immediate;
		#endif
	}

	gotoIfError(clean, GraphicsDeviceRef_createSwapchainExt(dev, *scRef))
	gotoIfError(clean, UnifiedTexture_create(*scRef, info.window->title))
	swapchain->lock = Lock_create();

clean:

	if(err.genericError)
		SwapchainRef_dec(scRef);

	return err;
}
