#pragma once
#include "types/list.h"
#include "math/vec.h"
#include "lock.h"
#include "window.h"
#include "monitor.h"

//Our manager can tell us when windows are done being used 

typedef struct WindowManager {
	struct Lock lock;
	struct List windows;
} WindowManager;

extern const U8 WindowManager_maxTotalVirtualWindowCount;
impl extern const U8 WindowManager_maxTotalPhysicalWindowCount;

typedef struct Window Window;

//Before doing any actions on WindowManager it needs to be locked.
//This includes getters, since otherwise the result might vary.

Error WindowManager_create(WindowManager *result);
Error WindowManager_free(WindowManager *manager);

impl Error WindowManager_createPhysical(
	WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	WindowHint hint,
	String title, 
	WindowCallbacks callbacks,
	WindowFormat format,
	Window **result
);

impl Error WindowManager_freePhysical(WindowManager *manager, Window **handle);

Error WindowManager_waitForExitAll(WindowManager *manager, Ns maxTimeout);

Error WindowManager_createVirtual(
	WindowManager *manager, 
	I32x2 size, 
	WindowCallbacks callbacks, 
	WindowFormat format,
	Window **result
);

Error WindowManager_freeVirtual(WindowManager *manager, Window **handle);

impl Bool WindowManager_supportsFormat(WindowManager manager, WindowFormat format);

inline WindowHandle WindowManager_maxWindows() {
	return (WindowHandle) WindowManager_maxTotalVirtualWindowCount + WindowManager_maxTotalPhysicalWindowCount;
}

inline Bool WindowManager_lock(WindowManager *manager, Ns maxTimeout) {
	return manager && Lock_lock(&manager->lock, maxTimeout);
}

inline Bool WindowManager_unlock(WindowManager *manager) {
	return manager && Lock_unlock(&manager->lock);
}

//Simple helper functions (need locks)

inline Window *WindowManager_getWindow(WindowManager *manager, WindowHandle windowHandle) {
	return 
		manager && Lock_isLockedForThread(manager->lock) ? 
			(Window*) List_ptr(manager->windows, windowHandle) : NULL;
}

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position, I32x2 size, WindowHint hint, String title, 
	WindowCallbacks callbacks, WindowFormat format,
	Window **w
);

Error WindowManager_freeWindow(WindowManager *manager, Window **w);
