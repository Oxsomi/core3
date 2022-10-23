#pragma once
#include "math/vec.h"
#include "lock.h"
#include "window.h"
#include "monitor.h"

//Our manager can tell us when windows are done being used 

struct WindowManager {

	struct Lock lock;

	struct Monitor *monitorArray;
	struct Window *windowArray;

	U8 monitorCount;
};

extern const U8 WindowManager_maxTotalVirtualWindowCount;
impl extern const U8 WindowManager_maxTotalPhysicalWindowCount;

typedef U16 WindowHandle;

//Before doing any actions on WindowManager it needs to be locked.
//This includes getters, since otherwise the result might vary.

struct Error WindowManager_createSelf(struct WindowManager **result);
struct Error WindowManager_freeSelf(struct WindowManager **manager);

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

impl struct Error WindowManager_freePhysical(struct WindowManager *manager, WindowHandle handle);

struct Error WindowManager_waitForExit(struct Window *w, Ns maxTimeout);
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

//Simple helper functions (need locks)

U8 WindowManager_getEmptyPhysicalWindows(struct WindowManager manager);
U8 WindowManager_getEmptyVirtualWindows(struct WindowManager manager);
U16 WindowManager_getEmptyWindows(struct WindowManager manager);

U16 WindowManager_maxWindows();

inline struct Window *WindowManager_getWindow(struct WindowManager *manager, WindowHandle windowHandle) {
	return 
		manager && Lock_isLockedForThread(manager->lock) && 
		windowHandle < (WindowHandle)WindowManager_maxTotalPhysicalWindowCount + WindowManager_maxTotalVirtualWindowCount ?
		manager->windowArray + windowHandle : NULL;
}

inline struct Monitor *WindowManager_getMonitor(struct WindowManager *manager, U8 monitorId) {
	return manager && Lock_isLockedForThread(manager->lock) && monitorId < manager->monitorCount ?
		manager->monitorArray + monitorId : NULL;
}

inline Bool WindowManager_lock(struct WindowManager *manager, Ns maxTimeout) {
	return manager && Lock_lock(manager->lock, maxTimeout);
}

inline Bool WindowManager_unlock(struct WindowManager *manager) {
	return manager && Lock_unlock(manager->lock);
}

//All types of windows

inline struct Error WindowManager_create(
	struct WindowManager *manager, 
	I32x2 position, I32x2 size, enum WindowHint hint, struct String title, 
	struct WindowCallbacks callbacks, enum WindowFormat format,
	struct Window **w
) {

	if(!manager)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (manager && WindowManager_getEmptyPhysicalWindows(*manager))
		return WindowManager_createPhysical(
			manager, position, size, hint, title, callbacks, format, w
		);

	return WindowManager_createVirtual(manager, size, callbacks, format, w);
}

inline struct Error WindowManager_free(struct WindowManager *manager, struct Window **w) {

	if (!manager)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 1 };

	if(!Lock_isLockedForThread(manager->lock))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if (Window_isVirtual(*w))
		return WindowManager_freeVirtual(manager, w);

	return WindowManager_freePhysical(manager, w);
}
