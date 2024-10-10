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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/directx12/dx_swapchain.h"
#include "graphics/directx12/dx_device.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "types/container/ref_ptr.h"
#include "platforms/ext/bufferx.h"

Error DX_WRAP_FUNC(GraphicsDeviceRef_createSwapchain)(GraphicsDeviceRef *deviceRef, SwapchainRef *swapchainRef) {

	Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);
	SwapchainInfo *info = &swapchain->info;

	//Prepare temporary free-ables and extended data.

	Error err;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxSwapchain *swapchainExt = TextureRef_getImplExtT(DxSwapchain, swapchainRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	DxGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Dx);

	const Window *window = info->window;

	Bool containsValid = false;
	Bool isFifo = false;

	for(U32 i = 0; i < sizeof(info->presentModePriorities); ++i)

		if(info->presentModePriorities[i] == ESwapchainPresentMode_Fifo) {
			isFifo = true;
			containsValid = true;
			break;
		}

		else if(info->presentModePriorities[i] == ESwapchainPresentMode_Immediate) {
			isFifo = false;
			containsValid = true;
			break;
		}

	if(!containsValid)
		gotoIfError(clean, Error_unsupportedOperation(
			0, "D3D12GraphicsDeviceRef_createSwapchain() only fifo and immediate are supported in D3D12"
		))

	if(swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite)
		gotoIfError(clean, Error_unsupportedOperation(
			1, "D3D12GraphicsDeviceRef_createSwapchain() D3D12 doesn't support writable swapchains"
		))

	DXGI_SWAP_CHAIN_FLAG flags = !isFifo ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	if(!swapchainExt->swapchain) {

		DXGI_USAGE usage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;

		DXGI_FORMAT format = 0;

		switch(window->format) {

			default:						format = DXGI_FORMAT_B8G8R8A8_UNORM;		break;
			case EWindowFormat_RGBA16f:		format = DXGI_FORMAT_R16G16B16A16_FLOAT;	break;
			case EWindowFormat_BGR10A2:		format = DXGI_FORMAT_R10G10B10A2_UNORM;		break;

			case EWindowFormat_RGBA32f:
				gotoIfError(clean, Error_unsupportedOperation(
					1, "D3D12GraphicsDeviceRef_createSwapchain() RGBA32f isn't supported"
				))
		}

		DXGI_SWAP_CHAIN_DESC1 desc1 = (DXGI_SWAP_CHAIN_DESC1) {
			.Format = format,
			.SampleDesc = (DXGI_SAMPLE_DESC) { .Count = 1 },
			.BufferUsage = usage,
			.BufferCount = 3,
			.Scaling = DXGI_SCALING_NONE,
			.SwapEffect = isFifo ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL : DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_IGNORE,
			.Flags = flags
		};

		gotoIfError(clean, dxCheck(instance->factory->lpVtbl->CreateSwapChainForHwnd(
			instance->factory,
			(IUnknown*) deviceExt->queues[EDxCommandQueue_Graphics].queue,
			window->nativeHandle,
			&desc1,
			NULL,
			NULL,
			&swapchainExt->swapchain
		)))

		swapchain->requiresManualComposite = false;
		swapchain->presentMode = isFifo ? ESwapchainPresentMode_Fifo : ESwapchainPresentMode_Immediate;
	}

	else {

		for(U8 i = 0; i < swapchain->base.images; ++i) {

			DxUnifiedTexture *managedImage = TextureRef_getImgExtT(swapchainRef, Dx, 0, i);
			managedImage->lastAccess = D3D12_BARRIER_ACCESS_NO_ACCESS;
			managedImage->lastLayout = D3D12_BARRIER_LAYOUT_COMMON;
			managedImage->lastSync = D3D12_BARRIER_SYNC_NONE;

			ID3D12Resource *img = managedImage->image;

			if(img)
				img->lpVtbl->Release(img);
		}

		gotoIfError(clean, dxCheck(swapchainExt->swapchain->lpVtbl->ResizeBuffers(
			swapchainExt->swapchain,
			3,
			0, 0, DXGI_FORMAT_UNKNOWN,		//Update to current w/h and keep format
			flags
		)))
	}

	//Acquire images

	for(U8 i = 0; i < swapchain->base.images; ++i)
		gotoIfError(clean, dxCheck(swapchainExt->swapchain->lpVtbl->GetBuffer(
			swapchainExt->swapchain, i,
			&IID_ID3D12Resource, (void**) &TextureRef_getImgExtT(swapchainRef, Dx, 0, i)->image
		)))

clean:
	return err;
}

Bool DX_WRAP_FUNC(Swapchain_free)(Swapchain *swapchain, Allocator alloc) {

	(void)alloc;

	SwapchainRef *swapchainRef = (RefPtr*) swapchain - 1;
	DxSwapchain *swapchainExt = TextureRef_getImplExtT(DxSwapchain, swapchainRef);

	for(U8 i = 0; i < swapchain->base.images; ++i) {

		ID3D12Resource *img = TextureRef_getImgExtT(swapchainRef, Dx, 0, i)->image;

		if(img)
			img->lpVtbl->Release(img);
	}

	if(swapchainExt->swapchain)
		swapchainExt->swapchain->lpVtbl->Release(swapchainExt->swapchain);

	return true;
}
