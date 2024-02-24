/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "platforms/log.h"
#include "platforms/input_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "types/time.h"
#include "types/string.h"

TListNamedImpl(ListWindowPtr);

const U32 WindowManager_magic = (U32)'W' | ((U32)'I' << 8) | ((U32)'N' << 16) | ((U32)'D' << 24);

impl Error WindowManager_createNative(WindowManager *w);
impl Bool WindowManager_freeNative(WindowManager *w);

Error WindowManager_create(WindowManagerCallbacks callbacks, U64 extendedDataSize, WindowManager *manager) {

	if (!manager)
		return Error_nullPointer(2, "WindowManager_create()::manager is required");

	Buffer extendedData = Buffer_createNull();
	Error err = Buffer_createEmptyBytesx(extendedDataSize, &extendedData);

	if (err.genericError)
		return err;

	*manager = (WindowManager) {
		.isActive = WindowManager_magic,
		.owningThread = Thread_getId(),
		.callbacks = callbacks,
		.extendedData = extendedData
	};

	if ((err = WindowManager_createNative(manager)).genericError) {
		*manager = (WindowManager) { 0 };
		return err;
	}

	if(callbacks.onCreate)
		callbacks.onCreate(manager);

	return Error_none();
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

Error WindowManager_adaptSizes(I32x2 *size, I32x2 *minSize, I32x2 *maxSize);

impl Bool WindowManager_freePhysical(Window *w);
impl Error WindowManager_createWindowPhysical(Window *w);

Error WindowManager_createWindow(

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
	Window **result
) {

	if(!result)
		return Error_nullPointer(!result ? 4 : 2, "WindowManager_createVirtual()::result and callbacks.onDraw are required");

	if(!WindowManager_isAccessible(manager))
		return Error_invalidOperation(0, "WindowManager_createVirtual() manager is NULL or inaccessible to current thread");

	if(*result)
		return Error_invalidOperation(1, "WindowManager_createVirtual()::*result is not NULL, indicates possible memleak");

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return Error_outOfBounds(
			1, (U64) (I64) I32x2_x(size), (U64) (I64) I32x2_y(size), "WindowManager_createVirtual()::size[i] must be >0"
		);

	if(CharString_length(title) >= 260)
		return Error_outOfBounds(7, 260, 260, "WindowManager_createWindow()::title can't exceed 260 chars");

	switch (format) {

		case EWindowFormat_BGRA8:
		case EWindowFormat_BGR10A2:
		case EWindowFormat_RGBA16f:
		case EWindowFormat_RGBA32f:
			break;

		default:
			return Error_invalidEnum(
				3, (U64) format, 0, "WindowManager_createVirtual()::format must be one of BGRA8, BGR10A2, RGBA16f, RGBA32f"
			);
	}

	//Ensure the sizes are valid

	Error err = WindowManager_adaptSizes(&size, &minSize, &maxSize);

	if(err.genericError)
		return err;

	Buffer cpuVisibleBuffer = Buffer_createNull();

	if(
		((type == EWindowType_Virtual) || (hint & EWindowHint_ProvideCPUBuffer)) &&
		(err = Buffer_createEmptyBytesx(
			ETextureFormat_getSize((ETextureFormat) format, I32x2_x(size), I32x2_y(size), 1),
			&cpuVisibleBuffer
		)).genericError
	)
		return err;

	Buffer extendedData = Buffer_createNull();

	if((err = Buffer_createEmptyBytesx(extendedDataSize, &extendedData)).genericError) {
		Buffer_freex(&cpuVisibleBuffer);
		return err;
	}

	CharString titleCopy = CharString_createNull();
	if((err = CharString_createCopyx(title, &titleCopy)).genericError) {
		Buffer_freex(&extendedData);
		Buffer_freex(&cpuVisibleBuffer);
		return err;
	}

	Buffer tmpWindow = Buffer_createNull();

	if ((err = Buffer_createEmptyBytesx(sizeof(Window), &tmpWindow)).genericError) {
		Buffer_freex(&extendedData);
		Buffer_freex(&cpuVisibleBuffer);
		CharString_freex(&titleCopy);
		return err;
	}

	Window *w = (Window*) tmpWindow.ptr;
	if ((err = ListWindowPtr_pushBackx(&manager->windows, w)).genericError) {
		Buffer_freex(&tmpWindow);
		Buffer_freex(&extendedData);
		Buffer_freex(&cpuVisibleBuffer);
		CharString_freex(&titleCopy);
		return err;
	}

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

	if (type == EWindowType_Physical && (err = WindowManager_createWindowPhysical(w)).genericError) {
		WindowManager_freeWindow(manager, result);
		return err;
	}

	w->flags |= EWindowFlags_IsActive;

	if(callbacks.onCreate)
		callbacks.onCreate(w);

	if(callbacks.onResize)
		callbacks.onResize(w);

	return Error_none();
}

impl void Window_updateExt(Window *w);

Error WindowManager_wait(WindowManager *manager) {

	if(!WindowManager_isAccessible(manager))
		return Error_invalidOperation(0, "WindowManager_wait() manager is NULL or inaccessible to current thread");

	while(manager->windows.length) {

		//Update all windows

		for (U64 i = manager->windows.length - 1; i != U64_MAX; --i) {

			Window *w = ListWindowPtr_at(manager->windows, i);

			//Update interface

			Ns now = Time_now();

			if (w->callbacks.onUpdate) {
				F64 dt = w->lastUpdate ? (now - w->lastUpdate) / (F64)SECOND : 0;
				w->callbacks.onUpdate(w, dt);
			}

			w->lastUpdate = now;

			if(w->type == EWindowType_Physical)
				Window_updateExt(w);

			else if(w->callbacks.onDraw)		//Virtual
				w->callbacks.onDraw(w);

			if (w->flags & EWindowFlags_ShouldTerminate)
				WindowManager_freeWindow(manager, &w);
		}

		Ns now = Time_now();

		if(manager->callbacks.onUpdate) {
			F64 dt = manager->lastUpdate ? (now - manager->lastUpdate) / (F64)SECOND : 0;
			manager->callbacks.onUpdate(manager, dt);
		}

		if(manager->callbacks.onDraw)
			manager->callbacks.onDraw(manager);

		manager->lastUpdate = now;
	}

	return Error_none();
}

Error WindowManager_adaptSizes(I32x2 *sizep, I32x2 *minSizep, I32x2 *maxSizep) {

	if(!sizep || !minSizep || !maxSizep)
		return Error_nullPointer(
			!sizep ? 0 : (!minSizep ? 1 : 2), "WindowManager_adaptSizes() requires sizep, minSizep and maxSizep"
		);

	I32x2 size = *sizep;
	I32x2 minSize = *minSizep;
	I32x2 maxSize = *maxSizep;

	//Verify size

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return Error_invalidParameter(2, 0, "WindowManager_adaptSizes()::*sizep should be >0");

	//Verify min size. By default should be 360p+.
	//Can't go below EResolution_SD.

	if(I32x2_any(I32x2_eq(minSize, I32x2_zero())))
		minSize = EResolution_get(EResolution_360);

	if(I32x2_any(I32x2_lt(minSize, EResolution_get(EResolution_SD))) || I32x2_any(I32x2_lt(minSize, I32x2_zero())))
		return Error_invalidParameter(3, 0, "WindowManager_adaptSizes()::*minSizep should be >=240p (0 = 640x360, >=426x240)");

	//Graphics APIs generally limit the resolution to 16Ki, so let's ensure the window can't get bigger than that

	if(I32x2_any(I32x2_eq(maxSize, I32x2_zero())))
		maxSize = I32x2_xx2(16384);

	if(I32x2_any(I32x2_gt(maxSize, I32x2_xx2(16384))) || I32x2_any(I32x2_lt(maxSize, I32x2_zero())))
		return Error_invalidParameter(4, 0, "WindowManager_adaptSizes()::*maxSizep should be >=0 && <16384");

	*minSizep = minSize;
	*maxSizep = maxSize;
	*sizep = I32x2_clamp(size, minSize, maxSize);

	return Error_none();
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
