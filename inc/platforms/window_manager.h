#pragma once
#include "types/vec.h"

//Our manager can tell us when windows are done being used 

struct WindowManager {

	struct Lock *lock;

	struct Monitor *monitorArray;
	struct Window *windowArray;

	U8 monitorCount;
	U8 maxPhysicalWindowCount, maxVirtualWindowCount;
};

extern const U64 WindowManager_maxTotalVirtualWindowCount;
impl extern const U64 WindowManager_maxTotalPhysicalWindowCount;

typedef U16 WindowHandle;

//Before doing any actions on WindowManager it needs to be locked.
//This includes getters, since otherwise the result might vary.

impl struct Error WindowManager_createSelf(struct WindowManager **result);
impl struct Error WindowManager_freeSelf(struct WindowManager **manager);

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

impl struct Error WindowManager_waitForExit(struct Window *w, Ns maxTimeout);
impl struct Error WindowManager_waitForExitAll(struct WindowManager *manager, Ns maxTimeout);

struct Error WindowManager_createVirtual(
	struct WindowManager *manager, 
	struct Window **result,
	I32x2 size, 
	struct WindowCallbacks callbacks, 
	enum WindowFormat format
);

struct Error WindowManager_freeVirtual(struct WindowManager *manager, struct Window **handle);

Bool WindowManager_supportsFormat(struct WindowManager manager, enum WindowFormat format);

//Simple helper functions (need locks)

U8 WindowManager_getEmptyPhysicalWindows(struct WindowManager manager);
U8 WindowManager_getEmptyVirtualWindows(struct WindowManager manager);
U8 WindowManager_getEmptyWindows(struct WindowManager manager);

inline struct Window *WindowManager_getWindow(WindowHandle windowHandle);
inline struct Monitor *WindowManager_getMonitor(U8 monitorId);

Bool WindowManager_lock(struct WindowManager *manager, Ns maxTimeout);
Bool WindowManager_unlock(struct WindowManager *manager);

//All types of windows

inline struct Error WindowManager_create(
	struct WindowManager *manager, 
	I32x2 position, I32x2 size, enum WindowHint hint, struct String title, 
	struct WindowCallbacks callbacks, enum WindowFormat format,
	struct Window **w
) {

	if (manager && WindowManager_getEmptyPhysicalWindows(*manager))
		return WindowManager_createPhysical(
			manager, position, size, hint, title, callbacks, format, w
		);

	return WindowManager_createVirtual(manager, w, size, callbacks, format);
}

inline struct Error WindowManager_free(struct WindowManager *manager, struct Window **w) {

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (Window_isVirtual(*w))
		return WindowManager_freeVirtual(manager, w);

	return WindowManager_freePhysical(manager, w);
}
