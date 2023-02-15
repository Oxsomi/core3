/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
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

impl Error WindowManager_createPhysical(
	WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	EWindowHint hint,
	String title, 
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **result
);

impl Bool WindowManager_freePhysical(WindowManager *manager, Window **handle);

Error WindowManager_waitForExitAll(WindowManager *manager, Ns maxTimeout);

Error WindowManager_createVirtual(
	WindowManager *manager, 
	I32x2 size, 
	WindowCallbacks callbacks, 
	EWindowFormat format,
	Window **result
);

Bool WindowManager_freeVirtual(WindowManager *manager, Window **handle);

impl Bool WindowManager_supportsFormat(WindowManager manager, EWindowFormat format);

WindowHandle WindowManager_maxWindows();

Bool WindowManager_lock(WindowManager *manager, Ns maxTimeout);
Bool WindowManager_unlock(WindowManager *manager);

//Simple helper functions (need locks)

Window *WindowManager_getWindow(WindowManager *manager, WindowHandle windowHandle);

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position, I32x2 size, EWindowHint hint, String title, 
	WindowCallbacks callbacks, EWindowFormat format,
	Window **w
);

Bool WindowManager_freeWindow(WindowManager *manager, Window **w);
