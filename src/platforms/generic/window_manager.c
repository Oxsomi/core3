#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/timer.h"
#include "types/buffer.h"

const U8 WindowManager_maxTotalVirtualWindowCount = 16;

Error WindowManager_create(WindowManager *result) {

	if(!result)
		return (Error) { .genericError = GenericError_NullPointer };

	if(result->windows.length || result->lock.data)
		return (Error) { .genericError = GenericError_InvalidOperation };

	Error err;

	if((err = Lock_create(&result->lock)).genericError)
		return err;

	if ((err = List_create(
		WindowManager_maxWindows(), sizeof(Window),
		Platform_instance.alloc,
		&result->windows
	)).genericError) {
		Lock_free(&result->lock);
		return err;
	}

	return Error_none();
}

Error WindowManager_free(WindowManager *manager) {

	if(!manager)
		return (Error) { .genericError = GenericError_NullPointer };

	if(!Lock_isLockedForThread(manager->lock) && !WindowManager_lock(manager, 5 * seconds))
		return (Error) { .genericError = GenericError_TimedOut };

	Error err = List_free(&manager->windows, Platform_instance.alloc);
	
	if(!WindowManager_unlock(manager))
		return (Error) { .genericError = GenericError_InvalidOperation };

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
	WindowFormat format,
	Window **result
) {

	if(!manager || !result || !callbacks.onDraw)
		return (Error) { .genericError = GenericError_NullPointer, .paramId = manager ? 1 : (!result ? 0 : 2) };

	if(!Lock_isLockedForThread(manager->lock))
		return (Error) { .genericError = GenericError_InvalidOperation };

	if(*result)
		return (Error) { .genericError = GenericError_InvalidOperation, .paramId = 4 };

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return (Error) { 
			.genericError = GenericError_InvalidParameter, 
			.paramId = 1, 
			.paramValue0 = (U64) I32x2_x(size), 
			.paramValue1 = (U64) I32x2_y(size)
		};

	switch (format) {

		case WindowFormat_rgba8:
		case WindowFormat_hdr10a2:
		case WindowFormat_rgba16f:
		case WindowFormat_rgba32f:
			break;

		default:
			return (Error) { 
				.genericError = GenericError_InvalidParameter, 
				.paramId = 3, 
				.paramValue0 = (U64) format
			};
	}

	for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i) {

		Window *w = (Window*) List_ptr(
			manager->windows, i + WindowManager_maxTotalPhysicalWindowCount
		);

		if(!w)
			break;

		if (!(w->flags & WindowFlags_IsActive)) {

			Buffer cpuVisibleBuffer = Buffer_createNull();

			Error err = Buffer_createEmptyBytes(
				TextureFormat_getSize((TextureFormat) format, I32x2_x(size), I32x2_y(size)),
				Platform_instance.alloc,
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

				.hint = WindowHint_ProvideCPUBuffer,
				.format = format,
				.flags = WindowFlags_IsFocussed | WindowFlags_IsVirtual | WindowFlags_IsActive
			};

			*result = w;

			if(callbacks.onCreate)
				callbacks.onCreate(w);

			return Error_none();
		}
	}

	return (Error) { .genericError = GenericError_OutOfMemory };
}

Error WindowManager_freeVirtual(WindowManager *manager, Window **handle) {

	if(!manager || !handle || !*handle)
		return (Error) { .genericError = GenericError_NullPointer, .paramId = manager ? 1 : 0 };

	if(!Lock_isLockedForThread(manager->lock))
		return (Error) { .genericError = GenericError_InvalidOperation };

	if(!Window_isVirtual(*handle))
		return (Error) { .genericError = GenericError_InvalidOperation, .paramId = 1 };

	Window *w = *handle;

	if(!Lock_isLockedForThread(w->lock))
		return (Error) { .genericError = GenericError_InvalidOperation };

	if (!Lock_unlock(&w->lock)) 
		return (Error) { .genericError = GenericError_InvalidOperation };

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
		if(!(wstart->flags & WindowFlags_IsActive))
			++v;

	return v;
}

Error WindowManager_waitForExitAll(WindowManager *manager, Ns maxTimeout) {

	if(!manager)
		return (Error) { .genericError = GenericError_NullPointer };

	//Checking for WindowManager is a lot easier, as we can just acquire the lock
	//And check how many windows are active

	Ns start = Timer_now();

	if(!WindowManager_lock(manager, maxTimeout))
		return (Error) { .genericError = GenericError_InvalidOperation };

	//Check if we should exit

	if(WindowManager_getEmptyWindows(*manager) == WindowManager_maxWindows())
		return WindowManager_unlock(manager) ? Error_none() : 
			(Error) { .genericError = GenericError_InvalidOperation };

	//Now we have to make sure we still have time left to wait

	maxTimeout = U64_min(maxTimeout, I64_MAX);

	Ns left = (Ns) I64_max(0, maxTimeout - (DNs)(Timer_now() - start));

	//Keep checking until we run out of time

	while(left > 0) {

		//Try to reacquire the lock

		if(!Lock_isLockedForThread(manager->lock) && !Lock_lock(&manager->lock, left))
			return (Error) { .genericError = GenericError_InvalidOperation };

		//Our windows have been released!

		if(WindowManager_getEmptyWindows(*manager) == WindowManager_maxWindows()) {
			WindowManager_unlock(manager);
			return Error_none();
		}

		//Grab our virtual windows, to ensure we complete them
		//The virtual window should properly exit if it's completed

		Bool containsVirtualWindow = false;

		for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i) {

			Window *w = (Window*) List_ptr(
				manager->windows, i + WindowManager_maxTotalPhysicalWindowCount
			);

			if(!w)
				break;

			if(w->flags & WindowFlags_IsActive) {

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
			return (Error) { .genericError = GenericError_InvalidOperation };

		//Wait to ensure we don't waste cycles
		//Virtual windows are allowed to update as fast as possible though
		
		if(!containsVirtualWindow)
			Thread_sleep(10 * ms);

		//

		left = (Ns) I64_max(0, maxTimeout - (DNs)(Timer_now() - start));
	}

	return (Error) { .genericError = GenericError_TimedOut };
}

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position, I32x2 size, WindowHint hint, String title, 
	WindowCallbacks callbacks, WindowFormat format,
	Window **w
) {

	if(!manager)
		return (Error) { .genericError = GenericError_NullPointer };

	if (manager)
		return WindowManager_createPhysical(
			manager, position, size, hint, title, callbacks, format, w
		);

	return WindowManager_createVirtual(manager, size, callbacks, format, w);
}

Error WindowManager_freeWindow(WindowManager *manager, Window **w) {

	if (!manager)
		return (Error) { .genericError = GenericError_NullPointer };

	if (!w)
		return (Error) { .genericError = GenericError_NullPointer, .paramId = 1 };

	if(!Lock_isLockedForThread(manager->lock))
		return (Error) { .genericError = GenericError_InvalidOperation };

	if (Window_isVirtual(*w))
		return WindowManager_freeVirtual(manager, w);

	return WindowManager_freePhysical(manager, w);
}
