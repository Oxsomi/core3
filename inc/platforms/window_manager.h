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
#include "types/list.h"
#include "math/vec.h"
#include "lock.h"
#include "window.h"

//Our manager can tell us when windows are done being used 

typedef struct WindowManager {
	Lock lock;
	List windows;
} WindowManager;

extern const U8 WindowManager_MAX_VIRTUAL_WINDOWS;
extern const U64 WindowManager_OUT_OF_WINDOWS;
impl extern const U8 WindowManager_MAX_PHYSICAL_WINDOWS;

//Before doing any actions on WindowManager it needs to be locked.
//This includes getters, since otherwise the result might vary.

Error WindowManager_create(WindowManager *result);
Bool WindowManager_free(WindowManager *manager);

Error WindowManager_adaptSizes(I32x2 *size, I32x2 *minSize, I32x2 *maxSize);

impl Error WindowManager_createPhysical(
	WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	I32x2 minSize,
	I32x2 maxSize,
	EWindowHint hint,
	CharString title, 
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **result
);

impl Bool WindowManager_freePhysical(WindowManager *manager, Window **handle);

Error WindowManager_waitForExitAll(WindowManager *manager);

Error WindowManager_createVirtual(
	WindowManager *manager, 
	I32x2 size, 
	I32x2 minSize,
	I32x2 maxSize,
	WindowCallbacks callbacks, 
	EWindowFormat format,
	Window **result
);

Bool WindowManager_freeVirtual(WindowManager *manager, Window **handle);

impl Bool WindowManager_supportsFormat(WindowManager manager, EWindowFormat format);

WindowHandle WindowManager_maxWindows();

//Simple helper functions (need locks)

Window *WindowManager_getWindow(WindowManager *manager, WindowHandle windowHandle);

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position, I32x2 size, 
	I32x2 minSize, I32x2 maxSize,
	EWindowHint hint, CharString title, 
	WindowCallbacks callbacks, EWindowFormat format,
	Window **w
);

Bool WindowManager_freeWindow(WindowManager *manager, Window **w);
