#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/thread.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "types/time.h"
#include "types/buffer.h"
#include "types/error.h"

const U8 WindowManager_MAX_VIRTUAL_WINDOWS = 16;

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

Bool WindowManager_free(WindowManager *manager) {

	if(!manager)
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

	for(U8 i = 0; i < WindowManager_MAX_VIRTUAL_WINDOWS; ++i) {

		Window *w = (Window*) List_ptr(
			manager->windows, i + WindowManager_MAX_PHYSICAL_WINDOWS
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

Bool WindowManager_freeVirtual(WindowManager *manager, Window **handle) {

	if(!manager || !handle || !*handle || !Lock_isLockedForThread(manager->lock) || !Window_isVirtual(*handle))
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

		for(U8 i = 0; i < WindowManager_MAX_VIRTUAL_WINDOWS; ++i) {

			Window *w = (Window*) List_ptr(
				manager->windows, i + WindowManager_MAX_PHYSICAL_WINDOWS
			);

			if(!w)
				break;

			if(w->flags & EWindowFlags_IsActive) {

				if(w->callbacks.onDraw) {

					if(Lock_lock(&w->lock, 5 * SECOND)) {

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
			Thread_sleep(10 * MS);

		//

		left = (Ns) I64_max(0, maxTimeout - (DNs)(Time_now() - start));
	}

	return Error_timedOut(0, maxTimeout);
}

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position,
	I32x2 size,
	EWindowHint hint,
	String title, 
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **w
) {

	if(!manager)
		return Error_nullPointer(0, 0);

	if (manager)
		return WindowManager_createPhysical(manager, position, size, hint, title, callbacks, format, w);

	return WindowManager_createVirtual(manager, size, callbacks, format, w);
}

Bool WindowManager_freeWindow(WindowManager *manager, Window **w) {

	if (!manager || !w)
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
