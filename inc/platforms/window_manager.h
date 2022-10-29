#pragma once
#include "types/list.h"
#include "math/vec.h"
#include "lock.h"
#include "window.h"
#include "monitor.h"

//Our manager can tell us when windows are done being used 

struct WindowManager {
	struct Lock lock;
	struct List windows;
};

extern const U8 WindowManager_maxTotalVirtualWindowCount;
impl extern const U8 WindowManager_maxTotalPhysicalWindowCount;

//Before doing any actions on WindowManager it needs to be locked.
//This includes getters, since otherwise the result might vary.

struct Error WindowManager_create(struct WindowManager *result);
struct Error WindowManager_free(struct WindowManager *manager);

impl struct Error WindowManager_createPhysical(
	struct WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	enum WindowHint hint,
	struct String title, 
	struct WindowCallbacks callbacks,
	enum WindowFormat format,
	struct Window **result
);

impl struct Error WindowManager_freePhysical(struct WindowManager *manager, struct Window **handle);

struct Error WindowManager_waitForExitAll(struct WindowManager *manager, Ns maxTimeout);

struct Error WindowManager_createVirtual(
	struct WindowManager *manager, 
	I32x2 size, 
	struct WindowCallbacks callbacks, 
	enum WindowFormat format,
	struct Window **result
);

struct Error WindowManager_freeVirtual(struct WindowManager *manager, struct Window **handle);

impl Bool WindowManager_supportsFormat(struct WindowManager manager, enum WindowFormat format);

inline WindowHandle WindowManager_maxWindows() {
	return (WindowHandle) WindowManager_maxTotalVirtualWindowCount + WindowManager_maxTotalPhysicalWindowCount;
}

inline Bool WindowManager_lock(struct WindowManager *manager, Ns maxTimeout) {
	return manager && Lock_lock(&manager->lock, maxTimeout);
}

inline Bool WindowManager_unlock(struct WindowManager *manager) {
	return manager && Lock_unlock(&manager->lock);
}

//Simple helper functions (need locks)

inline struct Window *WindowManager_getWindow(struct WindowManager *manager, WindowHandle windowHandle) {
	return 
		manager && Lock_isLockedForThread(manager->lock) ? 
			(struct Window*) List_ptr(manager->windows, windowHandle) : NULL;
}

//All types of windows

struct Error WindowManager_createWindow(
	struct WindowManager *manager, 
	I32x2 position, I32x2 size, enum WindowHint hint, struct String title, 
	struct WindowCallbacks callbacks, enum WindowFormat format,
	struct Window **w
);

struct Error WindowManager_freeWindow(struct WindowManager *manager, struct Window **w);
