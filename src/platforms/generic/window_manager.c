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

#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/thread.h"
#include "platforms/log.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/errorx.h"
#include "types/time.h"
#include "types/buffer.h"
#include "types/error.h"

const U8 WindowManager_MAX_VIRTUAL_WINDOWS = 8;
const U64 WindowManager_OUT_OF_WINDOWS = 0x1111111111;

Error WindowManager_create(WindowManager *result) {

	if(!result)
		return Error_nullPointer(0);

	if(result->lock.data)
		return Error_invalidOperation(0);

	Error err;

	if((err = Lock_create(&result->lock)).genericError)
		return err;

	if ((err = List_createx(
		WindowManager_maxWindows(), sizeof(Window),
		&result->windows
	)).genericError) {
		Lock_free(&result->lock);
		return err;
	}

	return Error_none();
}

Bool WindowManager_free(WindowManager *manager) {

	if(!manager || !manager->lock.data)
		return true;

	if(!Lock_isLockedForThread(manager->lock) && !WindowManager_lock(manager, 5 * SECOND))
		return false;

	List_freex(&manager->windows);
	
	if(!WindowManager_unlock(manager))
		return false;

	Bool freed = Lock_free(&manager->lock);
	*manager = (WindowManager) { 0 };
	return freed;
}

void WindowManager_virtualWindowLoop(Window *w) {

	if(w->callbacks.onDraw) {

		while(!(w->flags & EWindowFlags_ShouldThreadTerminate)) {

			if(Lock_lock(&w->lock, 5 * SECOND)) {

				w->isDrawing = true;
				w->callbacks.onDraw(w);
				w->isDrawing = false;

				Lock_unlock(&w->lock);
			}
		}
	}

	//Ensure we're allowed to free

	while (!Lock_lock(&Platform_instance.windowManager.lock, 1 * SECOND)) { }

	Lock_lock(&w->lock, 1 * SECOND);

	WindowManager_freeVirtual(&Platform_instance.windowManager, &w);
	Lock_unlock(&Platform_instance.windowManager.lock);

}

Error WindowManager_createVirtual(
	WindowManager *manager,
	I32x2 size,
	I32x2 minSize,
	I32x2 maxSize,
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **result
) {

	if(!manager || !manager->lock.data || !result || !callbacks.onDraw)
		return Error_nullPointer(!manager || !manager->lock.data  ? 0 : (!result ? 4 : 2));

	if(!Lock_isLockedForThread(manager->lock))
		return Error_invalidOperation(0);

	if(*result)
		return Error_invalidOperation(1);

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return Error_outOfBounds(1, (U64) (I64) I32x2_x(size), (U64) (I64) I32x2_y(size));

	switch (format) {

		case EWindowFormat_BGRA8:
		case EWindowFormat_BGR10A2:
		case EWindowFormat_RGBA16f:
		case EWindowFormat_RGBA32f:
			break;

		default:
			return Error_invalidEnum(3, (U64) format, 0);
	}

	//Ensure the sizes are valid

	Error err = WindowManager_adaptSizes(&size, &minSize, &maxSize);

	if(err.genericError)
		return err;

	for(U8 i = 0; i < WindowManager_MAX_VIRTUAL_WINDOWS; ++i) {

		Window *w = (Window*) List_ptr(manager->windows, i + WindowManager_MAX_PHYSICAL_WINDOWS);

		if(!w)
			break;

		if (!(w->flags & EWindowFlags_IsActive)) {

			Buffer cpuVisibleBuffer = Buffer_createNull();

			if((err = Buffer_createEmptyBytesx(
				ETextureFormat_getSize((ETextureFormat) format, I32x2_x(size), I32x2_y(size)),
				&cpuVisibleBuffer
			)).genericError)
				return err;

			Lock lock = (Lock) { 0 };

			if ((err = Lock_create(&lock)).genericError) {
				Buffer_freex(&cpuVisibleBuffer);
				return err;
			}

			*w = (Window) {

				.size = size,
				.prevSize = size,

				.cpuVisibleBuffer = cpuVisibleBuffer,
				.lock = lock,
				.callbacks = callbacks,

				.minSize = minSize,
				.maxSize = maxSize,

				.hint = EWindowHint_ProvideCPUBuffer,
				.format = format,
				.flags = EWindowFlags_IsFocussed | EWindowFlags_IsVirtual | EWindowFlags_IsActive
			};

			if(callbacks.onCreate)
				callbacks.onCreate(w);

			if ((err = Thread_create(
				(ThreadCallbackFunction) WindowManager_virtualWindowLoop, w, &w->mainThread)
			).genericError) {
				*w = (Window) { 0 };
				Lock_free(&lock);
				Buffer_freex(&cpuVisibleBuffer);
				return err;
			}

			if(w->callbacks.onResize)
				w->callbacks.onResize(w);

			*result = w;

			return Error_none();
		}
	}

	return Error_outOfMemory(0);
}

Bool WindowManager_freeVirtual(WindowManager *manager, Window **handle) {

	if(
		!manager || !manager->lock.data || 
		!handle || !*handle || 
		!Lock_isLockedForThread(manager->lock) || 
		!Window_isVirtual(*handle)
	)
		return false;

	Window *w = *handle;

	if(!Lock_isLockedForThread(w->lock) || !Lock_unlock(&w->lock)) 
		return false;

	if(w->callbacks.onDestroy)
		w->callbacks.onDestroy(w);

	Bool err = Lock_free(&w->lock);

	Buffer_free(&w->cpuVisibleBuffer, Platform_instance.alloc);

	*w = (Window) { 0 };
	*handle = NULL;

	return err;
}

inline U16 WindowManager_getEmptyWindows(WindowManager manager) {

	U16 v = 0;

	Window *wstart = (Window*) List_begin(manager.windows);
	Window *wend = (Window*) List_end(manager.windows);

	for(; wstart != wend; ++wstart)
		if(!(wstart->flags & EWindowFlags_IsActive))
			++v;

	return v;
}

Error WindowManager_waitForExitAll(WindowManager *manager, Ns maxTimeout) {

	if(!manager || !manager->lock.data)
		return Error_nullPointer(0);

	//Checking for WindowManager is a lot easier, as we can just acquire the lock
	//And check how many windows are active

	Ns start = Time_now();

	if(!WindowManager_lock(manager, maxTimeout))
		return Error_invalidOperation(0);

	//Check if we should exit

	if(WindowManager_getEmptyWindows(*manager) == WindowManager_maxWindows())
		return WindowManager_unlock(manager) ? Error_none() : Error_invalidOperation(1);

	//Now we have to make sure we still have time left to wait

	maxTimeout = U64_min(maxTimeout, I64_MAX);

	Ns left = (Ns) I64_max(0, maxTimeout - (DNs)(Time_now() - start));

	//Keep checking until we run out of time

	while(left > 0) {

		//Try to reacquire the lock

		while(!Lock_isLockedForThread(manager->lock) && !Lock_lock(&manager->lock, left))
			Thread_sleep(1 * MS);

		//Our windows have been released!

		if(WindowManager_getEmptyWindows(*manager) == WindowManager_maxWindows()) 
			return WindowManager_unlock(manager) ? Error_none() : Error_invalidOperation(3);

		//Grab our virtual windows, to ensure we complete them
		//The virtual window should properly exit if it's completed

		Bool containsVirtualWindow = false;

		for(U8 i = 0; i < WindowManager_MAX_VIRTUAL_WINDOWS; ++i) {

			Window *w = (Window*) List_ptr(manager->windows, i + WindowManager_MAX_PHYSICAL_WINDOWS);

			if(!w)
				break;

			if(w->flags & EWindowFlags_IsActive)
				containsVirtualWindow = true;
		}

		//Release the lock to check for the next time

		if(!WindowManager_unlock(manager))
			return Error_invalidOperation(4);

		//Wait to ensure we don't waste cycles
		//Virtual windows are allowed to update as fast as possible though
		
		if(!containsVirtualWindow)
			Thread_sleep(10 * MS);

		//

		left = (Ns) I64_max(0, maxTimeout - (DNs)(Time_now() - start));
	}

	return Error_timedOut(0, maxTimeout);
}

Error WindowManager_adaptSizes(I32x2 *sizep, I32x2 *minSizep, I32x2 *maxSizep) {

	I32x2 size = *sizep;
	I32x2 minSize = *minSizep;
	I32x2 maxSize = *maxSizep;

	//Verify size

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return Error_invalidParameter(2, 0);

	//Verify min size. By default should be 360p+.
	//Can't go below EResolution_SD.

	if(I32x2_any(I32x2_lt(minSize, I32x2_zero())))
		return Error_invalidParameter(3, 0);

	if(I32x2_any(I32x2_eq(minSize, I32x2_zero())))
		minSize = EResolution_get(EResolution_360);

	if(I32x2_any(I32x2_lt(minSize, EResolution_get(EResolution_SD))))
		return Error_invalidParameter(3, 0);

	//Graphics APIs generally limit the resolution to 16k, so let's ensure the window can't get bigger than that

	if(I32x2_any(I32x2_lt(maxSize, I32x2_zero())))
		return Error_invalidParameter(4, 0);

	if(I32x2_any(I32x2_eq(maxSize, I32x2_zero())))
		maxSize = EResolution_get(EResolution_16K);

	if(I32x2_any(I32x2_gt(maxSize, EResolution_get(EResolution_16K))))
		return Error_invalidParameter(4, 0);
		
	*minSizep = minSize;
	*maxSizep = maxSize;
	*sizep = I32x2_clamp(size, minSize, maxSize);

	return Error_none();
}

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position,
	I32x2 size,
	I32x2 minSize, I32x2 maxSize,
	EWindowHint hint,
	CharString title, 
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **w
) {

	if(!manager || !manager->lock.data)
		return Error_nullPointer(0);

	Error err = WindowManager_createPhysical(manager, position, size, minSize, maxSize, hint, title, callbacks, format, w);

	//Window manager doesn't support physical windows
	//So it will make a virtual one

	if(err.genericError == EGenericError_OutOfMemory && err.errorSubId == WindowManager_MAX_PHYSICAL_WINDOWS)
		return WindowManager_createVirtual(manager, size, minSize, maxSize, callbacks, format, w);

	return err;
}

Bool WindowManager_freeWindow(WindowManager *manager, Window **w) {

	if(!manager || !manager->lock.data)
		return false;

	if (!w || !*w)
		return true;

	if(!Lock_isLockedForThread(manager->lock))
		return false;

	return Window_isVirtual(*w) ? WindowManager_freeVirtual(manager, w) : WindowManager_freePhysical(manager, w);
}

WindowHandle WindowManager_maxWindows() {
	return (WindowHandle) WindowManager_MAX_VIRTUAL_WINDOWS + WindowManager_MAX_PHYSICAL_WINDOWS;
}

Bool WindowManager_lock(WindowManager *manager, Ns maxTimeout) {
	return manager && Lock_lock(&manager->lock, maxTimeout);
}

Bool WindowManager_unlock(WindowManager *manager) {
	return manager && Lock_unlock(&manager->lock);
}

//Simple helper functions (need locks)

Window *WindowManager_getWindow(WindowManager *manager, WindowHandle windowHandle) {
	return 
		manager && Lock_isLockedForThread(manager->lock) ? 
		(Window*) List_ptr(manager->windows, windowHandle) : NULL;
}
