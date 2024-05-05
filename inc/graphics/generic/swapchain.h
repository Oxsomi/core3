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

#pragma once
#include "types/vec.h"
#include "types/ref_ptr.h"
#include "types/lock.h"
#include "texture.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsDeviceRef;
typedef struct Window Window;
typedef enum EWindowFormat EWindowFormat;

typedef enum ESwapchainPresentMode {
	ESwapchainPresentMode_None,
	ESwapchainPresentMode_Immediate,			//Low latency apps (shooters, etc.): Allows tearing for less latency
	ESwapchainPresentMode_Mailbox,				//High performance apps (games): Pops oldest image while continuing
	ESwapchainPresentMode_Fifo,					//Best quality: Always presents images in order (no dropped frames)
	ESwapchainPresentMode_FifoRelaxed,			//^ but if vblank is missed continue with the next (can allow tearing)
	ESwapchainPresentMode_Count
} ESwapchainPresentMode;

typedef struct SwapchainInfo {

	//Window that this swapchain is created for. Call SwapchainRef_resize on resize
	const Window *window;

	//Priorities using ESwapchainPresentMode.
	//Triple buffering is always on to allow mixing swapchains with different present modes.
	//Tries to use presentModePriorities[i] until it reaches one that's supported.
	//If none are provided, it will fallback to the ideal fallback (Android: fifo, otherwise mailbox).
	//On Android mailbox isn't supported, because it may return 4 images (conflicts with our versioning).
	//Don't set this if you want to use the default always.
	U8 presentModePriorities[ESwapchainPresentMode_Count - 1];

	U32 padding;

} SwapchainInfo;

typedef struct Swapchain {

	SwapchainInfo info;

	ESwapchainPresentMode presentMode;

	//Device can be rotated and OS won't rotate for you (needs manual composite to flip/rotate).
	//This is generally on for android devices, and even though it is possible; the compositor is additional overhead.
	//You can manually adhere to window->monitors[0].orientation by
	// rotating the camera matrix and adjusting fov.
	//This is set by the implementation, can't be manually turned off.
	Bool requiresManualComposite;
	U8 padding[3];

	U64 versionId;				//Everytime this swapchain changes format or is resized this will increase.

	Lock lock;

	UnifiedTexture base;

} Swapchain;

typedef RefPtr SwapchainRef;

#define SwapchainRef_ptr(ptr) RefPtr_data(ptr, Swapchain)

Error SwapchainRef_dec(SwapchainRef **swapchain);
Error SwapchainRef_inc(SwapchainRef *swapchain);

Error GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *device, SwapchainInfo info, Bool allowComputeExt, SwapchainRef **ref);
Error SwapchainRef_resize(SwapchainRef *swapchain);

#ifdef __cplusplus
	}
#endif
