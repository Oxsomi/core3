#pragma once
#include "types/vec.h"

//Our manager can tell us when windows are done being used 

struct WindowManager {

	struct Lock *lock;

	struct Monitor *monitorArray;
	struct Window *windowArray;

	u8 monitorCount;
	u8 maxPhysicalWindowCount, maxVirtualWindowCount;
};

extern const usz WindowManager_maxVirtualWindowCount;

typedef u16 WindowHandle;

//Before doing any actions on WindowManager it needs to be locked.
//This includes getters, since otherwise the result might vary.

impl struct Error WindowManager_create(struct WindowManager **result);
impl struct Error WindowManager_free(struct WindowManager **manager);

impl struct Error WindowManager_createPhysical(
	struct WindowManager *manager,
	i32x2 position,
	i32x2 size, 
	enum WindowHint hint,
	struct String title, 
	struct WindowCallbacks callbacks,
	enum WindowFormat format,
	struct Window **result
);

impl struct Error WindowManager_freePhysical(struct WindowManager *manager, WindowHandle handle);

impl struct Error WindowManager_waitForExit(const struct Window *w);

struct Error WindowManager_createVirtual(
	struct WindowManager *manager, 
	struct Window **result,
	i32x2 size, 
	struct WindowCallbacks callbacks, 
	enum WindowFormat format
);

struct Error WindowManager_freeVirtual(struct WindowManager *manager, struct Window **handle);

//Simple helper functions (need locks)

u8 WindowManager_getEmptyPhysicalWindows();
u8 WindowManager_getEmptyVirtualWindows();
u8 WindowManager_getEmptyWindows();

inline struct Window *WindowManager_getWindow(WindowHandle windowHandle);
inline struct Monitor *WindowManager_getMonitor(u8 monitorId);

bool WindowManager_lock(struct WindowManager *manager);
bool WindowManager_unlock(struct WindowManager *manager);

//All types of windows

inline struct Error WindowManager_create(
	struct WindowManager *manager, 
	i32x2 position, i32x2 size, enum WindowHint hint, struct String title, 
	struct WindowCallbacks callbacks, enum WindowFormat format,
	struct Window **w
) {

	if (WindowManager_getEmptyPhysicalWindows())
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