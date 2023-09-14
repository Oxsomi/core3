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

#pragma once
#include "types/types.h"

typedef struct GraphicsDevice GraphicsDevice;
typedef struct Error Error;
typedef struct List List;
typedef struct Window Window;
typedef enum ETextureFormat ETextureFormat;

typedef struct SwapchainInfo {

	//Window that this swapchain is created for.
	//Input into createSwapchain, mutable after.

	const Window *window;

	//Device can be rotated and OS won't rotate for you (needs manual composite to flip/rotate).
	//This is generally on for android devices, and even though it is possible; the compositor is additional overhead.
	//You can manually adhere to window->monitors[0].orientation by 
	// rotating the camera matrix and adjusting fov.
	//This is set by the implementation, can't be manually turned off.

	Bool requiresManualComposite;

	//Double buffering (vsync on) or triple buffering (vsync off)
	//Input into createSwapchain, mutable after.

	Bool vsync;

} SwapchainInfo;

typedef struct Swapchain {

	SwapchainInfo info;

	void *ext;				//Underlying api implementation

} Swapchain;

impl Error GraphicsDevice_createSwapchain(GraphicsDevice *device, SwapchainInfo info, Swapchain *swapchain);
impl Bool GraphicsDevice_freeSwapchain(GraphicsDevice *device, Swapchain *swapchain);
