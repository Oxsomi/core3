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
	return !RefPtr_dec(swapchain) ? Error_invalidOperation(0) : Error_none();
}

Error SwapchainRef_inc(SwapchainRef *swapchain) {
	return !RefPtr_inc(swapchain) ? Error_invalidOperation(0) : Error_none();
}

impl Error GraphicsDeviceRef_createSwapchainExt(GraphicsDeviceRef *deviceRef, SwapchainInfo info, Swapchain *swapchain);
impl Bool GraphicsDevice_freeSwapchainExt(Swapchain *data, Allocator alloc);

impl extern const U64 SwapchainExt_size;

Error Swapchain_resize(Swapchain *swapchain) {

	if(!swapchain)
		return Error_nullPointer(0);

	//Resize with same format and same size is a NOP

	if(I32x2_eq2(swapchain->info.window->size, swapchain->size) && swapchain->info.window->format == swapchain->format)
		return Error_none();

	//Otherwise, we properly resize

	return GraphicsDeviceRef_createSwapchainExt(swapchain->device, swapchain->info, swapchain);
}

Bool GraphicsDevice_freeSwapchain(Swapchain *swapchain, Allocator alloc) {
	GraphicsDevice_freeSwapchainExt(swapchain, alloc);
	GraphicsDeviceRef_dec(&swapchain->device);
	return true;
}

Error GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *deviceRef, SwapchainInfo info, SwapchainRef **swapchainRef) {

	if(!deviceRef || !info.window || !info.window->nativeHandle)
		return Error_nullPointer(!deviceRef ? 1 : 0);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!(device->info.capabilities.features & EGraphicsFeatures_Swapchain))
		return Error_unsupportedOperation(0);

	Error err = RefPtr_createx(
		(U32)(sizeof(Swapchain) + SwapchainExt_size), 
		(ObjectFreeFunc) GraphicsDevice_freeSwapchain, 
		EGraphicsTypeId_Swapchain, 
		swapchainRef
	);

	if(err.genericError)
		return err;

	err = GraphicsDeviceRef_createSwapchainExt(deviceRef, info, SwapchainRef_ptr(*swapchainRef));

	if(err.genericError) {
		RefPtr_dec(swapchainRef);
		return err;
	}

	return Error_none();
}
