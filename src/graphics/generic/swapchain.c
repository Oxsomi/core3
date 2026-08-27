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

//graphics/generic/swapchain.c

#include "types/base/platform_types.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_allocator.h"
#include "platforms/window.h"
#include "types/container/ref_ptr.h"
#include "types/base/error.h"

Bool SwapchainRef_resize(SwapchainRef *swapchainRef, Error *e_rr) {

	Bool s_uccess = true;

	Swapchain *swapchain = NULL;
	ELockAcquire acqSwapchain = ELockAcquire_Invalid;

	if(!swapchainRef || swapchainRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_Swapchain)
		retError(clean, Error_nullPointer(0, "Swapchain_resize()::swapchain is required"));

	swapchain = SwapchainRef_ptr(swapchainRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(swapchain->base.resource.device);

	acqSwapchain = SpinLock_lock(&swapchain->lock, U64_MAX);

	if(acqSwapchain != ELockAcquire_Acquired)
		retError(clean, Error_invalidState(0, "Swapchain_resize() couldn't lock swapchain"));

	//Check if swapchain was in flight.
	//If yes, warn that the user has to flush manually

	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "Swapchain_resize() can't be called while recording commands"));

	Bool any = false;

	for (U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(ListRefPtr); ++i)
		if (ListRefPtr_contains(device->resourcesInFlight[i], swapchainRef, 0, NULL)) {
			any = true;
			break;
		}

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if (any)
		retError(clean, Error_invalidState(
			0, "Swapchain_resize() can't be called on a Swapchain that's in flight. Use GraphicsDeviceRef_wait in onResize"
		));

	//Resize with same format and same size is a NOP

	Window *window = swapchain->info.window;

	if(!window)
		retError(clean, Error_invalidState(0, "SwapchainRef_resize() window is invalid"));

	I32x2 newSize = window->size;
	EWindowFormat newFormat = window->format;
	WindowOrientation orientation = window->orientation;

	ETextureFormatId textureFormatId = 0;

	switch (newFormat) {
		case EWindowFormat_BGRA8:        textureFormatId = ETextureFormatId_BGRA8;        break;
		case EWindowFormat_RGBA8:        textureFormatId = ETextureFormatId_RGBA8;        break;
		case EWindowFormat_BGR10A2:      textureFormatId = ETextureFormatId_BGR10A2;      break;
		case EWindowFormat_RGBA16f:      textureFormatId = ETextureFormatId_RGBA16f;      break;
		case EWindowFormat_RGBA32f:      textureFormatId = ETextureFormatId_RGBA32f;      break;
		default:
			retError(clean, Error_invalidState(1, "Swapchain_resize() window format is unsupported"));
	}

	if(
		I32x2_x(newSize) == swapchain->base.width && I32x2_y(newSize) == swapchain->base.height &&
		swapchain->base.textureFormatId == textureFormatId &&
		swapchain->orientation == orientation
	)
		goto clean;

	//Otherwise, we properly resize

	if(swapchain->base.resource.flags & EGraphicsResourceFlag_InternalOwnsImages) {

		//A swapchain that owns its images allocates them at base.width and base.height,
		// so the new size has to be in place BEFORE they are made.
		//A physical one is instead handed images a presentation engine already sized.
		//The old images, their views and the block behind them go first, since nothing else will free them.

		swapchain->base.textureFormatId = (U8) textureFormatId;
		swapchain->base.width = (U16) I32x2_x(newSize);
		swapchain->base.height = (U16) I32x2_y(newSize);

		UnifiedTexture_freeExt(swapchainRef);

		if(swapchain->base.resource.allocated) {
			DeviceMemoryAllocator_freeAllocation(
				&device->allocator, swapchain->base.resource.blockId, swapchain->base.resource.blockOffset
			);
			swapchain->base.resource.allocated = false;
		}
	}

	else gotoIfError3(clean, GraphicsDeviceRef_createSwapchainExt(swapchain->base.resource.device, swapchainRef, e_rr));

	gotoIfError3(clean, UnifiedTexture_createExt(swapchainRef, &window->title, e_rr));        //Re-create views
	++swapchain->versionId;

	swapchain->base.textureFormatId = (U8) textureFormatId;
	swapchain->base.width = (U16) I32x2_x(newSize);
	swapchain->base.height = (U16) I32x2_y(newSize);

clean:

	if(acqSwapchain == ELockAcquire_Acquired)
		SpinLock_unlock(&swapchain->lock);

	return s_uccess;
}

void Swapchain_free(void *swapchainGeneric, const Allocator *alloc) {
	Swapchain *swapchain = (Swapchain*) swapchainGeneric;
	SpinLock_lock(&swapchain->lock, U64_MAX);
	Swapchain_freeExt(swapchain, alloc);
	UnifiedTexture_free((TextureRef*)((U8*)swapchain - sizeof(RefPtr)));
}

Bool GraphicsDeviceRef_createSwapchain(
	GraphicsDeviceRef *dev,
	SwapchainInfo info,
	Bool allowWriteExt,
	DescriptorTableRef *bindlessDescriptorTable,
	SwapchainRef **scRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool alloc = false;

	Window *window = info.window;

	//Checked first, since getTypes on a NULL device yields a non NULL member pointer that faults in RefPtr_create.

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createSwapchain()::dev is required"));

	if(!window)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_createSwapchain()::info.window is required"));

	//A VIRTUAL window has no surface to acquire from and nothing to present to,
	// so the swapchain owns its images and the frame ends in memory rather than on a screen.
	//That is what a machine with GPUs and no display has, and it is also what an encoder or an image sequence wants,
	// so it is a presentation TARGET rather than a fallback.
	//Everything above the backend sees an ordinary swapchain, which is what lets one renderer drive both.

	const Bool ownsImages = window->type == EWindowType_Virtual;

	if(!ownsImages && !window->nativeHandle)
		retError(clean, Error_nullPointer(
			1, "GraphicsDeviceRef_createSwapchain()::info.window (physical) has no native handle"
		));

	//On some platforms the images we get back != the images we request.
	//Even though we request 3, we might get 5 even.
	//https://github.com/googlesamples/vulkan-basic-samples/issues/24#issuecomment-442626040
	//Unfortunately we still have to allocate up to (48 + 16) * 2 = 128 bytes extra, not too bad though.

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->swapchain, scRef, e_rr));
	alloc = true;

	ETextureFormatId formatId = ETextureFormatId_BGRA8;

	switch (window->format) {
		case EWindowFormat_BGRA8:        formatId = ETextureFormatId_BGRA8;        break;
		case EWindowFormat_RGBA8:        formatId = ETextureFormatId_RGBA8;        break;
		case EWindowFormat_BGR10A2:      formatId = ETextureFormatId_BGR10A2;      break;
		case EWindowFormat_RGBA16f:      formatId = ETextureFormatId_RGBA16f;      break;
		case EWindowFormat_RGBA32f:      formatId = ETextureFormatId_RGBA32f;      break;
		default:
			retError(clean, Error_invalidState(1, "Swapchain_resize() window format is unsupported"));
	}

	Swapchain *swapchain = SwapchainRef_ptr(*scRef);

	gotoIfError3(clean, RefPtr_inc(dev));

	swapchain->info = info;
	swapchain->base = (UnifiedTexture) {
		.resource = (GraphicsResource) {
			.device = dev,
			.flags = (GraphicsResourceFlag) (
				(allowWriteExt ? EGraphicsResourceFlag_ShaderRWBindful : EGraphicsResourceFlag_ShaderRead) |
				(ownsImages ? EGraphicsResourceFlag_InternalOwnsImages : 0)
			),
			.type = (U8) EResourceType_Swapchain
		},
		.textureFormatId = (U8) formatId,
		.type = (U8) ETextureType_2D,
		.width = (U16) I32x2_x(window->size),
		.height = (U16) I32x2_y(window->size),
		.length = 1,
		.levels = 1,
		//A physical swapchain requests 3 and the presentation engine may return up to SWAPCHAIN_MAX_IMAGES, so a
		// consumer reads the count back rather than assumes it. A virtual swapchain owns its images and holds exactly
		// framesInFlight of them, one per frame that can be in flight, with no presentation engine to add slack.
		.images = ownsImages ? GraphicsDeviceRef_ptr(dev)->framesInFlight : 3,
		.maxImages = SWAPCHAIN_MAX_IMAGES
	};

	if(!swapchain->info.presentModePriorities[0]) {
		#if _PLATFORM_TYPE != PLATFORM_ANDROID
			swapchain->info.presentModePriorities[0] = ESwapchainPresentMode_Mailbox;        //Priority is to be low latency
			swapchain->info.presentModePriorities[1] = ESwapchainPresentMode_Immediate;
			swapchain->info.presentModePriorities[2] = ESwapchainPresentMode_FifoRelaxed;
			swapchain->info.presentModePriorities[3] = ESwapchainPresentMode_Fifo;
		#else
			swapchain->info.presentModePriorities[0] = ESwapchainPresentMode_Fifo;            //Priority is to conserve power
			swapchain->info.presentModePriorities[1] = ESwapchainPresentMode_FifoRelaxed;
			swapchain->info.presentModePriorities[2] = ESwapchainPresentMode_Mailbox;
			swapchain->info.presentModePriorities[3] = ESwapchainPresentMode_Immediate;
		#endif
	}

	if(!ownsImages)
		gotoIfError3(clean, GraphicsDeviceRef_createSwapchainExt(dev, *scRef, e_rr));

	gotoIfError3(clean, UnifiedTexture_create(*scRef, bindlessDescriptorTable, &window->title, e_rr));

clean:

	if(!s_uccess && alloc)
		RefPtr_dec(scRef);

	return s_uccess;
}
