#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/timer.h"
#include "types/buffer.h"

struct Error WindowManager_createSelf(struct WindowManager **result);

U16 WindowManager_maxWindows() {
	return WindowManager_maxTotalVirtualWindowCount + WindowManager_maxTotalPhysicalWindowCount;
}

struct Error WindowManager_freeSelf(struct WindowManager **manager) {

	if(!manager || !*manager)
		return (struct Error) { .genericError = GenericError_NullPointer };

	struct WindowManager *man = *manager;

	if(!Lock_isLockedForThread(man->lock) && !WindowManager_lock(man, 5 * seconds))
		return (struct Error) { .genericError = GenericError_TimedOut };

	struct Buffer buf = Buffer_createNull();
	struct Error err = Error_none(), errTemp = Error_none();

	buf = Buffer_createRef(man->monitorArray, U8_MAX * sizeof(struct Monitor));

	if(buf.ptr)
		err = Buffer_free(&buf, Platform_instance.alloc);

	U64 windows = WindowManager_maxWindows();

	buf = Buffer_createRef(man->windowArray, windows * sizeof(struct Window));

	if(buf.ptr)
		errTemp = Buffer_free(&buf, Platform_instance.alloc), errTemp;

	if(errTemp.genericError)
		err = errTemp;

	buf = Buffer_createRef(man, sizeof(struct WindowManager));

	errTemp = Error_none();

	if(buf.ptr)
		errTemp = Buffer_free(&buf, Platform_instance.alloc);

	if(errTemp.genericError)
		err = errTemp;

	*manager = NULL;
	return err;
}

U8 WindowManager_getEmptyPhysicalWindows(struct WindowManager manager) {

	if(!Lock_isLockedForThread(manager.lock))
		return 0;

	U8 v = 0;

	for(U8 i = 0; i < WindowManager_maxTotalPhysicalWindowCount; ++i)
		if(!manager.windowArray[i].nativeHandle) 
			++v;

	return v;
}

U8 WindowManager_getEmptyVirtualWindows(struct WindowManager manager) {

	if(!Lock_isLockedForThread(manager.lock))
		return 0;

	U8 v = 0;

	for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i)
		if(!manager.windowArray[(U64)i + WindowManager_maxTotalPhysicalWindowCount].cpuVisibleBuffer.ptr) 
			++v;

	return v;
}

U16 WindowManager_getEmptyWindows(struct WindowManager manager) {

	if(!Lock_isLockedForThread(manager.lock))
		return 0;

	U16 v = 0;

	for(U8 i = 0; i < WindowManager_maxTotalPhysicalWindowCount; ++i)
		if(!manager.windowArray[i].nativeHandle) 
			++v;

	for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i)
		if(!manager.windowArray[(U64)i + WindowManager_maxTotalPhysicalWindowCount].cpuVisibleBuffer.ptr) 
			++v;

	return v;
}

struct Error WindowManager_createVirtual(
	struct WindowManager *manager,
	I32x2 size,
	struct WindowCallbacks callbacks,
	enum WindowFormat format,
	struct Window **result
) {

	if(!manager || !result)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = manager ? 1 : 0 };

	if(!Lock_isLockedForThread(manager->lock))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if(*result)
		return (struct Error) { .genericError = GenericError_InvalidOperation, .paramId = 4 };

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return (struct Error) { 
			.genericError = GenericError_InvalidParameter, 
			.paramId = 1, 
			.paramValue0 = I32x2_x(size), 
			.paramValue1 = I32x2_y(size)
		};

	switch (format) {

		case WindowFormat_rgba8:
		case WindowFormat_hdr10a2:
		case WindowFormat_rgba16f:
		case WindowFormat_rgba32f:
			break;

		default:
			return (struct Error) { 
				.genericError = GenericError_InvalidParameter, 
				.paramId = 3, 
				.paramValue0 = (U64) format
			};
	}

	for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i) {

		struct Window *w = manager->windowArray + i + WindowManager_maxTotalPhysicalWindowCount;

		if (!w->cpuVisibleBuffer.ptr) {

			struct Buffer cpuVisibleBuffer = Buffer_createNull();

			struct Error err = Buffer_createEmptyBytes(
				TextureFormat_getSize((enum TextureFormat) format, (U64) I32x2_x(size), (U64) I32x2_y(size)),
				Platform_instance.alloc,
				&cpuVisibleBuffer
			);

			if(err.genericError)
				return err;

			struct Lock lock = (struct Lock) { 0 };

			if ((err = Lock_create(&lock)).genericError) {
				Buffer_free(&cpuVisibleBuffer, Platform_instance.alloc);
				return err;
			}

			*w = (struct Window) {

				.size = size,
				.cpuVisibleBuffer = cpuVisibleBuffer,
				.lock = lock,
				.callbacks = callbacks,

				.handle = (WindowHandle) i,

				.hint = WindowHint_ProvideCPUBuffer,
				.format = format,
				.flags = WindowFlags_IsFocussed | WindowFlags_IsVirtual | WindowFlags_IsActive
			};

			*result = w;

			if(callbacks.start)
				callbacks.start(w);
		}
	}

	return (struct Error) { .genericError = GenericError_OutOfMemory };
}

struct Error WindowManager_freeVirtual(struct WindowManager *manager, struct Window **handle) {

	if(!manager || !handle || !*handle)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = manager ? 1 : 0 };

	if(!Lock_isLockedForThread(manager->lock))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if(!Window_isVirtual(*handle))
		return (struct Error) { .genericError = GenericError_InvalidOperation, .paramId = 1 };

	struct Window *w = *handle;

	if(w->callbacks.end)
		w->callbacks.end(w);

	struct Error err = Lock_free(&w->lock);

	struct Error errTemp = Buffer_free(&w->cpuVisibleBuffer, Platform_instance.alloc);

	*w = (struct Window) { 0 };
	*handle = NULL;

	return errTemp.genericError ? errTemp : err;
}


struct Error WindowManager_waitForExit(struct Window *w, Ns maxTimeout) {

	if(!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	Ns start = Timer_now();

	//We lock to check window state
	//If there's no lock, then we've already been released

	if(w->lock.data && !Lock_isLockedForThread(w->lock) && !Lock_lock(w->lock, maxTimeout))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	//If our window isn't marked as active, then our window is gone
	//We've successfully waited

	if (!(w->flags & WindowFlags_IsActive))
		return Error_none();

	//Now we have to make sure we still have time left to wait

	Ns left = (Ns) I64_max(0, (DNs)(Timer_now() - start) - maxTimeout);

	//Release the lock, because otherwise our window can't resume itself
	
	if(!Lock_unlock(w->lock))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	//Keep checking until we run out of time

	while(left > 0) {

		//Wait to ensure we don't waste cycles
		//Virtual windows are allowed to run as fast as possible to produce the frames

		if(!Window_isVirtual(w))
			Thread_sleep(10 * ms);

		//Try to reacquire the lock

		if(w->lock.data && !Lock_isLockedForThread(w->lock) && !Lock_lock(w->lock, left))
			return (struct Error) { .genericError = GenericError_InvalidOperation };

		//Our window has been released!

		if (!(w->flags & WindowFlags_IsActive))
			return Error_none();

		//Virtual windows can draw really quickly

		if(Window_isVirtual(w) && w->callbacks.draw)
			w->callbacks.draw(w);

		//Release the lock to check for the next time

		if(!Lock_unlock(w->lock))
			return (struct Error) { .genericError = GenericError_InvalidOperation };

		//

		left = (Ns) I64_max(0, (DNs)(Timer_now() - start) - maxTimeout);
	}

	return (struct Error) { .genericError = GenericError_TimedOut };
}

struct Error WindowManager_waitForExitAll(struct WindowManager *manager, Ns maxTimeout) {

	if(!manager)
		return (struct Error) { .genericError = GenericError_NullPointer };

	//Checking for WindowManager is a lot easier, as we can just acquire the lock
	//And check how many windows are active

	Ns start = Timer_now();

	if(!WindowManager_lock(manager, maxTimeout))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	//Check if we should exit

	if(WindowManager_getEmptyWindows(*manager) == WindowManager_maxWindows())
		return WindowManager_unlock(manager) ? Error_none() : 
			(struct Error) { .genericError = GenericError_InvalidOperation };

	//Now we have to make sure we still have time left to wait

	Ns left = (Ns) I64_max(0, (DNs)(Timer_now() - start) - maxTimeout);

	//Keep checking until we run out of time

	while(left > 0) {

		//Try to reacquire the lock

		if(!Lock_isLockedForThread(manager->lock) && !Lock_lock(manager->lock, left))
			return (struct Error) { .genericError = GenericError_InvalidOperation };

		//Our windows have been released!

		if(WindowManager_getEmptyWindows(*manager) == WindowManager_maxWindows()) {
			Lock_unlock(manager->lock);
			return Error_none();
		}

		//Grab our virtual windows, to ensure we complete them
		//The virtual window should properly exit if it's completed

		Bool containsVirtualWindow = false;

		for(U8 i = 0; i < WindowManager_maxTotalVirtualWindowCount; ++i) {

			struct Window *w = manager->windowArray + i + WindowManager_maxTotalPhysicalWindowCount;

			if(w->flags & WindowFlags_IsActive) {

				if(w->callbacks.draw)
					w->callbacks.draw(w);

				//else //TODO: Terminate window, it doesn't have a draw routine!

				containsVirtualWindow = true;
			}
		}

		//Release the lock to check for the next time

		if(!Lock_unlock(manager->lock))
			return (struct Error) { .genericError = GenericError_InvalidOperation };

		//Wait to ensure we don't waste cycles
		//Virtual windows are allowed to update as fast as possible though
		
		if(!containsVirtualWindow)
			Thread_sleep(10 * ms);

		//

		left = (Ns) I64_max(0, (DNs)(Timer_now() - start) - maxTimeout);
	}

	return (struct Error) { .genericError = GenericError_TimedOut };
}
