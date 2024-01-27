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

#include "graphics/generic/swapchain.h"
#include "graphics/generic/device.h"
#include "platforms/window.h"
#include "types/error.h"

Error SwapchainRef_dec(SwapchainRef **swapchain) {
	return !RefPtr_dec(swapchain) ? Error_invalidOperation(0, "SwapchainRef_dec()::swapchain is invalid") : Error_none();
}

Error SwapchainRef_inc(SwapchainRef *swapchain) {
	return !RefPtr_inc(swapchain) ? Error_invalidOperation(0, "SwapchainRef_inc()::swapchain is invalid") : Error_none();
}

impl Error GraphicsDeviceRef_createSwapchainExt(GraphicsDeviceRef *deviceRef, SwapchainInfo info, Swapchain *swapchain);
impl Bool GraphicsDevice_freeSwapchainExt(Swapchain *data, Allocator alloc);

impl extern const U64 SwapchainExt_size;

Error SwapchainRef_resize(SwapchainRef *swapchainRef) {

	if(!swapchainRef || swapchainRef->typeId != EGraphicsTypeId_Swapchain)
		return Error_nullPointer(0, "Swapchain_resize()::swapchain is required");

	Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(swapchain->device);

	if(Lock_lock(&swapchain->lock, U64_MAX) != ELockAcquire_Acquired)
		return Error_invalidState(0, "Swapchain_resize() couldn't lock swapchain");

	//Check if swapchain was in flight. If yes, warn that the user has to flush manually

	ELockAcquire acq = Lock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return Error_invalidState(0, "Swapchain_resize() can't be called while recording commands");

	Bool any = false;

	for (U64 i = 0; i < sizeof(device->resourcesInFlight) / sizeof(ListRefPtr); ++i)
		if (ListRefPtr_contains(device->resourcesInFlight[i], swapchainRef, 0, NULL)) {
			any = true;
			break;
		}

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	if (any)
		return Error_invalidState(
			0, "Swapchain_resize() can't be called on a Swapchain that's in flight. Use GraphicsDeviceRef_wait in onResize"
		);

	//Resize with same format and same size is a NOP

	Error err = Error_none();
	I32x2 newSize = swapchain->info.window->size;
	EWindowFormat newFormat = swapchain->info.window->format;

	if(I32x2_eq2(newSize, swapchain->size) && newFormat == swapchain->format)
		goto clean;

	//Otherwise, we properly resize

	_gotoIfError(clean, GraphicsDeviceRef_createSwapchainExt(swapchain->device, swapchain->info, swapchain));

	++swapchain->versionId;

	swapchain->format = newFormat;
	swapchain->size = newSize;

clean:
	Lock_unlock(&swapchain->lock);
	return err;
}

Bool GraphicsDevice_freeSwapchain(Swapchain *swapchain, Allocator alloc) {
	Lock_free(&swapchain->lock);
	GraphicsDevice_freeSwapchainExt(swapchain, alloc);
	GraphicsDeviceRef_dec(&swapchain->device);
	return true;
}

Error GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *deviceRef, SwapchainInfo info, SwapchainRef **swapchainRef) {

	if(!deviceRef || !info.window || !info.window->nativeHandle)
		return Error_nullPointer(
			!deviceRef ? 1 : 0, "GraphicsDeviceRef_createSwapchain()::deviceRef and info.window (physical) are required"
		);

	if(deviceRef->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(
			0, "GraphicsDeviceRef_createSwapchain()::deviceRef is an invalid type"
		);

	Error err = RefPtr_createx(
		(U32)(sizeof(Swapchain) + SwapchainExt_size), 
		(ObjectFreeFunc) GraphicsDevice_freeSwapchain, 
		EGraphicsTypeId_Swapchain, 
		swapchainRef
	);

	if(err.genericError)
		return err;

	Swapchain *swapchain = SwapchainRef_ptr(*swapchainRef);
	_gotoIfError(clean, GraphicsDeviceRef_createSwapchainExt(deviceRef, info, swapchain));
	swapchain->lock = Lock_create();

clean:

	if(err.genericError)
		SwapchainRef_dec(swapchainRef);

	return err;
}
