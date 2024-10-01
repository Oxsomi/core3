/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "platforms/ext/listx_impl.h"
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "types/thread.h"
#include "platforms/input_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/time.h"
#include "types/string.h"

TListNamedImpl(ListWindowPtr);

const U32 WindowManager_magic = (U32)'W' | ((U32)'I' << 8) | ((U32)'N' << 16) | ((U32)'D' << 24);

impl Bool WindowManager_createNative(WindowManager *w, Error *e_rr);
impl Bool WindowManager_freeNative(WindowManager *w);

Bool WindowManager_create(WindowManagerCallbacks callbacks, U64 extendedDataSize, WindowManager *manager, Error *e_rr) {

	Bool s_uccess = true;
	Buffer extendedData = Buffer_createNull();

	if (!manager)
		retError(clean, Error_nullPointer(2, "WindowManager_create()::manager is required"))

	gotoIfError2(clean, Buffer_createEmptyBytesx(extendedDataSize, &extendedData))

	*manager = (WindowManager) {
		.isActive = WindowManager_magic,
		.owningThread = Thread_getId(),
		.callbacks = callbacks,
		.extendedData = extendedData
	};

	gotoIfError3(clean, WindowManager_createNative(manager, e_rr))

	if(callbacks.onCreate)
		callbacks.onCreate(manager);

clean:
	return s_uccess;
}

Bool WindowManager_isAccessible(const WindowManager *manager) {
	return manager && manager->isActive == WindowManager_magic && manager->owningThread == Thread_getId();
}

Bool WindowManager_free(WindowManager *manager) {

	if(!manager || manager->isActive != WindowManager_magic)
		return true;

	if(manager->owningThread != Thread_getId())
		return false;

	if(manager->callbacks.onDestroy)
		manager->callbacks.onDestroy(manager);

	manager->isActive = 0;

	for(U64 i = 0; i < manager->windows.length; ++i)
		WindowManager_freeWindow(manager, &manager->windows.ptrNonConst[i]);

	ListWindowPtr_freex(&manager->windows);
	Buffer_freex(&manager->extendedData);

	WindowManager_freeNative(manager);
	Buffer_freex(&manager->platformData);
	return true;
}

Bool WindowManager_adaptSizes(I32x2 *size, I32x2 *minSize, I32x2 *maxSize, Error *e_rr);

impl Bool WindowManager_freePhysical(Window *w);
impl Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr);

Bool WindowManager_createWindow(

	WindowManager *manager,

	EWindowType type,

	I32x2 position,
	I32x2 size,
	I32x2 minSize,
	I32x2 maxSize,

	EWindowHint hint,
	CharString title,
	WindowCallbacks callbacks,
	EWindowFormat format,
	U64 extendedDataSize,
	Window **result,
	Error *e_rr
) {

	Bool s_uccess = true;

	Buffer cpuVisibleBuffer = Buffer_createNull();
	Buffer extendedData = Buffer_createNull();
	CharString titleCopy = CharString_createNull();
	Buffer tmpWindow = Buffer_createNull();

	if(!result)
		retError(clean, Error_nullPointer(
			!result ? 4 : 2, "WindowManager_createVirtual()::result and callbacks.onDraw are required"
		))

	if(!WindowManager_isAccessible(manager))
		retError(clean, Error_invalidOperation(
			0, "WindowManager_createVirtual() manager is NULL or inaccessible to current thread"
		))

	if(*result)
		retError(clean, Error_invalidOperation(
			1, "WindowManager_createVirtual()::*result is not NULL, indicates possible memleak"
		))

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		retError(clean, Error_outOfBounds(
			1, (U64) (I64) I32x2_x(size), (U64) (I64) I32x2_y(size),
			"WindowManager_createVirtual()::size[i] must be >0"
		))

	if(CharString_length(title) >= 260)
		retError(clean, Error_outOfBounds(
			7, 260, 260, "WindowManager_createWindow()::title can't exceed 260 chars"
		))

	switch (format) {

		case EWindowFormat_BGRA8:
		case EWindowFormat_BGR10A2:
		case EWindowFormat_RGBA16f:
		case EWindowFormat_RGBA32f:
			break;

		default:
			retError(clean, Error_invalidEnum(
				3, (U64) format, 0,
				"WindowManager_createVirtual()::format must be one of BGRA8, BGR10A2, RGBA16f, RGBA32f"
			))
	}

	//Ensure the sizes are valid

	gotoIfError3(clean, WindowManager_adaptSizes(&size, &minSize, &maxSize, e_rr))

	if((type == EWindowType_Virtual) || (hint & EWindowHint_ProvideCPUBuffer))
		gotoIfError2(clean, Buffer_createEmptyBytesx(
			ETextureFormat_getSize((ETextureFormat) format, I32x2_x(size), I32x2_y(size), 1),
			&cpuVisibleBuffer
		))

	gotoIfError2(clean, Buffer_createEmptyBytesx(extendedDataSize, &extendedData))
	gotoIfError2(clean, CharString_createCopyx(title, &titleCopy))
	gotoIfError2(clean, Buffer_createEmptyBytesx(sizeof(Window), &tmpWindow))

	Window *w = (Window*) tmpWindow.ptr;
	gotoIfError2(clean, ListWindowPtr_pushBackx(&manager->windows, w))

	*w = (Window) {

		.owner = manager,

		.type = type,
		.hint = hint | (type == EWindowType_Virtual ? EWindowHint_ProvideCPUBuffer : 0),
		.format = format,
		.flags = (type == EWindowType_Virtual ? EWindowFlags_IsFocussed : 0),

		.offset = position,
		.size = size,
		.minSize = minSize,
		.maxSize = maxSize,
		.prevSize = size,

		.cpuVisibleBuffer = cpuVisibleBuffer,
		.callbacks = callbacks,
		.title = titleCopy,
		.extendedData = extendedData
	};

	if (type == EWindowType_Physical)
		gotoIfError3(clean, WindowManager_createWindowPhysical(w, e_rr))

	w->flags |= EWindowFlags_IsActive;

	if(callbacks.onCreate)
		callbacks.onCreate(w);

	if(callbacks.onResize)
		callbacks.onResize(w);

	*result = w;

clean:

	if(!s_uccess) {
		Buffer_freex(&tmpWindow);
		Buffer_freex(&extendedData);
		Buffer_freex(&cpuVisibleBuffer);
		CharString_freex(&titleCopy);
	}

	return s_uccess;
}

impl void WindowManager_updateExt(WindowManager *manager);

Bool WindowManager_step(WindowManager *manager, Window *forcingUpdate) {
	
	if(!manager)
		return false;

	//Update all windows first

	for (U64 i = manager->windows.length - 1; i != U64_MAX; --i) {

		Window *w = ListWindowPtr_at(manager->windows, i);

		if(w == forcingUpdate)	//Has already been processed
			continue;

		//Update interface

		const Ns now = Time_now();

		if (w->callbacks.onUpdate) {
			const F64 dt = w->lastUpdate ? (now - w->lastUpdate) / (F64)SECOND : 0;
			w->callbacks.onUpdate(w, dt);
		}

		w->lastUpdate = now;

		if(w->type != EWindowType_Physical && w->callbacks.onDraw)		//Virtual
			w->callbacks.onDraw(w);

		if (w->flags & EWindowFlags_ShouldTerminate)					//Just in case the window closed now
			WindowManager_freeWindow(manager, &w);
	}

	//Then run window manager update; this polls all events (only used if it's not called by WM_PAINT)

	if(!forcingUpdate)
		WindowManager_updateExt(manager);

	//Finally, we can notify manager that we're ready for draws/updates

	const Ns now = Time_now();

	if(manager->callbacks.onUpdate) {
		const F64 dt = manager->lastUpdate ? (now - manager->lastUpdate) / (F64)SECOND : 0;
		manager->callbacks.onUpdate(manager, dt);
	}

	if(manager->callbacks.onDraw)
		manager->callbacks.onDraw(manager);

	manager->lastUpdate = now;

	return true;
}

Bool WindowManager_wait(WindowManager *manager, Error *e_rr) {

	Bool s_uccess = true;

	if(!WindowManager_isAccessible(manager))
		retError(clean, Error_invalidOperation(
			0, "WindowManager_wait() manager is NULL or inaccessible to current thread"
		))

	while(manager->windows.length)
		WindowManager_step(manager, NULL);

clean:
	return s_uccess;
}

Bool WindowManager_adaptSizes(I32x2 *sizep, I32x2 *minSizep, I32x2 *maxSizep, Error *e_rr) {

	Bool s_uccess = true;

	if(!sizep || !minSizep || !maxSizep)
		retError(clean, Error_nullPointer(
			!sizep ? 0 : (!minSizep ? 1 : 2), "WindowManager_adaptSizes() requires sizep, minSizep and maxSizep"
		))

	const I32x2 size = *sizep;
	I32x2 minSize = *minSizep;
	I32x2 maxSize = *maxSizep;

	//Verify size

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		retError(clean, Error_invalidParameter(2, 0, "WindowManager_adaptSizes()::*sizep should be >0"))

	//Verify min size. By default should be 360p+.
	//Can't go below EResolution_SD.

	if(I32x2_any(I32x2_eq(minSize, I32x2_zero())))
		minSize = EResolution_get(EResolution_360);

	if(I32x2_any(I32x2_lt(minSize, EResolution_get(EResolution_SD))) || I32x2_any(I32x2_lt(minSize, I32x2_zero())))
		retError(clean, Error_invalidParameter(
			3, 0, "WindowManager_adaptSizes()::*minSizep should be >=240p (0 = 640x360, >=426x240)"
		))

	//Graphics APIs generally limit the resolution to 16Ki, so let's ensure the window can't get bigger than that

	if(I32x2_any(I32x2_eq(maxSize, I32x2_zero())))
		maxSize = I32x2_xx2(16384);

	if(I32x2_any(I32x2_gt(maxSize, I32x2_xx2(16384))) || I32x2_any(I32x2_lt(maxSize, I32x2_zero())))
		retError(clean, Error_invalidParameter(
			4, 0, "WindowManager_adaptSizes()::*maxSizep should be >=0 && <16384"
		))

	*minSizep = minSize;
	*maxSizep = maxSize;
	*sizep = I32x2_clamp(size, minSize, maxSize);

clean:
	return s_uccess;
}

Bool WindowManager_freeWindow(WindowManager *manager, Window **w) {

	if(!w || !*w)
		return false;

	Window *win = *w;

	if(!WindowManager_isAccessible(win->owner))
		return false;

	//Ensure our window safely exits

	if(win->flags & EWindowFlags_IsActive && win->callbacks.onDestroy)
		win->callbacks.onDestroy(win);

	Buffer_freex(&win->cpuVisibleBuffer);
	Buffer_freex(&win->extendedData);
	CharString_freex(&win->title);

	ListInputDevice *devices = &win->devices;

	for(U64 i = 0; i < devices->length; ++i)
		InputDevice_free(&devices->ptrNonConst[i]);

	ListInputDevice_freex(devices);
	ListMonitor_freex(&win->monitors);

	Bool b = true;

	if(win->type == EWindowType_Physical)
		b = WindowManager_freePhysical(win);

	Buffer windowBuf = Buffer_createManagedPtr(win, sizeof(*win));

	ListWindowPtr_eraseFirst(&manager->windows, win, 0, NULL);
	Buffer_freex(&windowBuf);
	*w = NULL;
	return b;
}
