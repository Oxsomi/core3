#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/thread.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "types/time.h"
#include "types/buffer.h"

const U8 WindowManager_maxTotalVirtualWindowCount = 16;

Error WindowManager_create(WindowManager *result) {

	if(!result)
		return Error_nullPointer(0, 0);

	if(result->windows.length || result->lock.data)
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

Error WindowManager_free(WindowManager *manager) {

	if(!manager)
		return Error_none();

	if(!Lock_isLockedForThread(manager->lock) && !WindowManager_lock(manager, 5 * seconds))
		return Error_timedOut(0, 5 * seconds);

	Error err = List_freex(&manager->windows);
	
	if(!WindowManager_unlock(manager))
		return Error_invalidOperation(0);

	Error errTemp = Lock_free(&manager->lock);

	if(errTemp.genericError)
		err = errTemp;

	*manager = (WindowManager) { 0 };
	return err;
}

Error WindowManager_createVirtual(
	WindowManager *manager,
	I32x2 size,
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **result
) {

	if(!manager || !result || !callbacks.onDraw)
		return Error_nullPointer(!manager ? 0 : (!result ? 4 : 2), 0);

	if(!Lock_isLockedForThread(manager->lock))
		return Error_invalidOperation(0);

	if(*result)
		return Error_invalidOperation(1);

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return Error_outOfBounds(1, 0, (U64) (I64) I32x2_x(size), (U64) (I64) I32x2_y(size));

	switch (format) {

		case EWindowFormat_rgba8:
		case EWindowFormat_hdr10a2:
		case EWindowFormat_rgba16f:
		case EWindowFormat_rgba32f:
			break;

		default:
			return Error_invalidEnum(3, 0, (U64) format, 0);
	}

	for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i) {

		Window *w = (Window*) List_ptr(
			manager->windows, i + WindowManager_maxTotalPhysicalWindowCount
		);

		if(!w)
			break;

		if (!(w->flags & EWindowFlags_IsActive)) {

			Buffer cpuVisibleBuffer = Buffer_createNull();

			Error err = Buffer_createEmptyBytesx(
				ETextureFormat_getSize((ETextureFormat) format, I32x2_x(size), I32x2_y(size)),
				&cpuVisibleBuffer
			);

			if(err.genericError)
				return err;

			Lock lock = (Lock) { 0 };

			if ((err = Lock_create(&lock)).genericError) {
				Buffer_free(&cpuVisibleBuffer, Platform_instance.alloc);
				return err;
			}

			*w = (Window) {

				.size = size,
				.cpuVisibleBuffer = cpuVisibleBuffer,
				.lock = lock,
				.callbacks = callbacks,

				.hint = EWindowHint_ProvideCPUBuffer,
				.format = format,
				.flags = EWindowFlags_IsFocussed | EWindowFlags_IsVirtual | EWindowFlags_IsActive
			};

			*result = w;

			if(callbacks.onCreate)
				callbacks.onCreate(w);

			return Error_none();
		}
	}

	return Error_outOfMemory(0);
}

Error WindowManager_freeVirtual(WindowManager *manager, Window **handle) {

	if(!manager || !handle || !*handle)
		return Error_nullPointer(!manager ? 0 : 1, 0);

	if(!Lock_isLockedForThread(manager->lock))
		return Error_invalidOperation(0);

	if(!Window_isVirtual(*handle))
		return Error_invalidOperation(1);

	Window *w = *handle;

	if(!Lock_isLockedForThread(w->lock))
		return Error_invalidOperation(2);

	if (!Lock_unlock(&w->lock)) 
		return Error_invalidOperation(3);

	if(w->callbacks.onDestroy)
		w->callbacks.onDestroy(w);

	Error err = Lock_free(&w->lock);

	Error errTemp = Buffer_free(&w->cpuVisibleBuffer, Platform_instance.alloc);

	*w = (Window) { 0 };
	*handle = NULL;

	return errTemp.genericError ? errTemp : err;
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

	if(!manager)
		return Error_nullPointer(0, 0);

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

		if(!Lock_isLockedForThread(manager->lock) && !Lock_lock(&manager->lock, left))
			return Error_invalidOperation(2);

		//Our windows have been released!

		if(WindowManager_getEmptyWindows(*manager) == WindowManager_maxWindows()) 
			return WindowManager_unlock(manager) ? Error_none() : Error_invalidOperation(3);

		//Grab our virtual windows, to ensure we complete them
		//The virtual window should properly exit if it's completed

		Bool containsVirtualWindow = false;

		for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i) {

			Window *w = (Window*) List_ptr(
				manager->windows, i + WindowManager_maxTotalPhysicalWindowCount
			);

			if(!w)
				break;

			if(w->flags & EWindowFlags_IsActive) {

				if(w->callbacks.onDraw) {

					if(Lock_lock(&w->lock, 5 * seconds)) {

						w->isDrawing = true;
						w->callbacks.onDraw(w);
						w->isDrawing = false;

						//Window might be terminated

						if(Lock_isLockedForThread(w->lock))
							Lock_unlock(&w->lock);
					}
				}

				containsVirtualWindow = true;
			}
		}

		//Release the lock to check for the next time

		if(!WindowManager_unlock(manager))
			return Error_invalidOperation(4);

		//Wait to ensure we don't waste cycles
		//Virtual windows are allowed to update as fast as possible though
		
		if(!containsVirtualWindow)
			Thread_sleep(10 * ms);

		//

		left = (Ns) I64_max(0, maxTimeout - (DNs)(Time_now() - start));
	}

	return Error_timedOut(0, maxTimeout);
}

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position, I32x2 size, EWindowHint hint, String title, 
	WindowCallbacks callbacks, EWindowFormat format,
	Window **w
) {

	if(!manager)
		return Error_nullPointer(0, 0);

	if (manager)
		return WindowManager_createPhysical(
			manager, position, size, hint, title, callbacks, format, w
		);

	return WindowManager_createVirtual(manager, size, callbacks, format, w);
}

Error WindowManager_freeWindow(WindowManager *manager, Window **w) {

	if (!manager || !w)
		return Error_nullPointer(!w ? 1 : 0, 0);

	if(!Lock_isLockedForThread(manager->lock))
		return Error_invalidOperation(1);

	if (Window_isVirtual(*w))
		return WindowManager_freeVirtual(manager, w);

	return WindowManager_freePhysical(manager, w);
}
