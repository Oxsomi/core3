#pragma once
#include "types/list.h"
#include "math/vec.h"
#include "lock.h"
#include "window.h"

//Our manager can tell us when windows are done being used 

typedef struct WindowManager {
	Lock lock;
	List windows;
} WindowManager;

extern const U8 WindowManager_MAX_VIRTUAL_WINDOWS;
impl extern const U8 WindowManager_MAX_PHYSICAL_WINDOWS;

//Before doing any actions on WindowManager it needs to be locked.
//This includes getters, since otherwise the result might vary.

Error WindowManager_create(WindowManager *result);
Bool WindowManager_free(WindowManager *manager);

impl Error WindowManager_createPhysical(
	WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	EWindowHint hint,
	String title, 
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **result
);

impl Bool WindowManager_freePhysical(WindowManager *manager, Window **handle);

Error WindowManager_waitForExitAll(WindowManager *manager, Ns maxTimeout);

Error WindowManager_createVirtual(
	WindowManager *manager, 
	I32x2 size, 
	WindowCallbacks callbacks, 
	EWindowFormat format,
	Window **result
);

Bool WindowManager_freeVirtual(WindowManager *manager, Window **handle);

impl Bool WindowManager_supportsFormat(WindowManager manager, EWindowFormat format);

WindowHandle WindowManager_maxWindows();

Bool WindowManager_lock(WindowManager *manager, Ns maxTimeout);
Bool WindowManager_unlock(WindowManager *manager);

//Simple helper functions (need locks)

Window *WindowManager_getWindow(WindowManager *manager, WindowHandle windowHandle);

//All types of windows

Error WindowManager_createWindow(
	WindowManager *manager, 
	I32x2 position, I32x2 size, EWindowHint hint, String title, 
	WindowCallbacks callbacks, EWindowFormat format,
	Window **w
);

Bool WindowManager_freeWindow(WindowManager *manager, Window **w);
