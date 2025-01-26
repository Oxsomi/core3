/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/math/vec.h"
#include "types/container/ref_ptr.h"
#include "types/base/lock.h"
#include "graphics/generic/texture.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsDeviceRef;
typedef struct Window Window;
typedef enum EWindowFormat EWindowFormat;

typedef enum ESwapchainPresentMode {
	ESwapchainPresentMode_None,
	ESwapchainPresentMode_Immediate,			//Low latency games (shooters, etc.): Tearing; Allows tearing for less latency
	ESwapchainPresentMode_Mailbox,				//High performance apps (games): Coalesing; Pops oldest image while continuing
	ESwapchainPresentMode_Fifo,					//Best quality & low power: Vsync; present images in order (no dropped frames)
	ESwapchainPresentMode_Count
} ESwapchainPresentMode;

typedef struct SwapchainInfo {

	//Window that this swapchain is created for. Call SwapchainRef_resize on resize
	const Window *window;

	//Priorities using ESwapchainPresentMode.
	//Tries to use presentModePriorities[i] until it reaches one that's supported.
	//If none are provided, it will fallback to the ideal fallback (Android: fifo, otherwise mailbox).
	//Don't set this if you want to use the default always.
	U8 presentModePriorities[ESwapchainPresentMode_Count - 1];

	U32 padding;

} SwapchainInfo;

typedef struct Swapchain {

	SwapchainInfo info;

	ESwapchainPresentMode presentMode;

	//Device can be rotated and OS won't rotate for you (needs manual composite to flip/rotate).
	//This is generally on for android devices, and even though it is possible; the compositor is additional overhead.
	//You can manually adhere to window->orientation by
	// rotating the camera matrix and adjusting fov.
	//This is set by the implementation, can't be manually turned off.
	Bool requiresManualComposite;
	U8 padding;
	U16 orientation;				//In deg; 0, 90, 180 or 270

	U64 versionId;				//Everytime this swapchain changes format or is resized this will increase.

	SpinLock lock;

	UnifiedTexture base;

} Swapchain;

#define SWAPCHAIN_VERSIONING 3		//How many swapchain images should be requested, should always be 3
#define SWAPCHAIN_MAX_DELTA 2		//How many swapchain images extra can be available (important for android devices)
#define SWAPCHAIN_MAX_IMAGES (SWAPCHAIN_VERSIONING + SWAPCHAIN_MAX_DELTA)

typedef RefPtr SwapchainRef;

#define SwapchainRef_ptr(ptr) RefPtr_data(ptr, Swapchain)

Error SwapchainRef_dec(SwapchainRef **swapchain);
Error SwapchainRef_inc(SwapchainRef *swapchain);

Error GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *device, SwapchainInfo info, Bool allowComputeExt, SwapchainRef **ref);
Error SwapchainRef_resize(SwapchainRef *swapchain);

#ifdef __cplusplus
	}
#endif
