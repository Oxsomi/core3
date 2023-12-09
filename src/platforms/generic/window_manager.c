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

#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/thread.h"
#include "platforms/log.h"
#include "platforms/input_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "types/time.h"
#include "types/buffer.h"
#include "types/error.h"

const U32 WindowManager_magic = (U32)'W' | ((U32)'I' << 8) | ((U32)'N' << 16) | ((U32)'D' << 24);

Error WindowManager_create(WindowManagerCallbacks callbacks, U64 extendedDataSize, WindowManager *manager) {

	if(!manager)
		return Error_nullPointer(2, "WindowManager_create()::manager is required");

	Buffer extendedData = Buffer_createNull();
	Error err = Buffer_createEmptyBytesx(extendedDataSize, &extendedData);

	if(err.genericError)
		return err;

	*manager = (WindowManager) { 
		.isActive = WindowManager_magic, 
		.owningThread = Thread_getId(), 
		.windows = List_createEmpty(sizeof(Window)),
		.callbacks = callbacks,
		.extendedData = extendedData
	};

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

	for(WindowHandle i = 0; i < (WindowHandle) manager->windows.length; ++i) {
		WindowHandle w = i + 1;
		WindowManager_freeWindow(manager, &w);
	}

	List_freex(&manager->windows);
	Buffer_freex(&manager->extendedData);
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
	WindowHandle *result
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
		(type == EWindowType_Virtual) || (hint & EWindowHint_ProvideCPUBuffer) ||
		(err = Buffer_createEmptyBytesx(
			ETextureFormat_getSize((ETextureFormat) format, I32x2_x(size), I32x2_y(size)),
			&cpuVisibleBuffer
		)).genericError
	)
		return err;

	WindowHandle i = 0;

	for(; i < (WindowHandle) manager->windows.length; ++i)
		if(!((Window*) List_ptr(manager->windows, i))->owner)
			break;

	if (i == (WindowHandle)-1) {
		Buffer_freex(&cpuVisibleBuffer);
		return Error_outOfBounds(
			0, (WindowHandle)-1, (WindowHandle)-1, "WindowManager_createVirtual() only allows up to (WindowHandle)-1 windows"
		);
	}

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

	Bool pushed = false;

	if(i == manager->windows.length) {
		Window empty = (Window) { 0 };
		if ((err = List_pushBackx(&manager->windows, Buffer_createConstRef(&empty, sizeof(empty)))).genericError) {
			Buffer_freex(&extendedData);
			Buffer_freex(&cpuVisibleBuffer);
			CharString_freex(&titleCopy);
			return err;
		}

		pushed = true;
	}

	Window *w = (Window*) List_ptr(manager->windows, i);
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
		.handle = i + 1,
		.title = titleCopy,
		.extendedData = extendedData
	};

	if (type == EWindowType_Physical && (err = WindowManager_createWindowPhysical(w)).genericError) {
		WindowManager_freePhysical(w);
		return err;
	}

	w->flags |= EWindowFlags_IsActive;

	if(callbacks.onCreate)
		callbacks.onCreate(w);

	if(callbacks.onResize)
		callbacks.onResize(w);

	*result = i + 1;
	return Error_none();
}

WindowHandle WindowManager_firstActiveWindow(WindowManager *manager) {

	WindowHandle i = 0;

	for(; i < (WindowHandle) manager->windows.length; ++i)
		if(((Window*) List_ptr(manager->windows, i))->owner)
			break;

	return i + 1;
}

Bool WindowManager_isActive(WindowManager *manager) {
	return WindowManager_firstActiveWindow(manager) != manager->windows.length;
}

WindowHandle WindowManager_countActiveWindows(WindowManager *manager) {

	WindowHandle counter = 0;

	for(WindowHandle i = 0; i < (WindowHandle) manager->windows.length; ++i)
		if(((Window*) List_ptr(manager->windows, i))->owner)
			++counter;

	return counter;
}

impl void Window_updateExt(Window *w);

Error WindowManager_wait(WindowManager *manager) {

	if(!WindowManager_isAccessible(manager))
		return Error_invalidOperation(0, "WindowManager_wait() manager is NULL or inaccessible to current thread");

	while(WindowManager_countActiveWindows(manager)) {

		//Update all windows

		for (U64 i = manager->windows.length - 1; i != U64_MAX; --i) {

			Window *w = (Window*) List_ptr(manager->windows, i);

			if(!w->owner)
				continue;

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

			if (w->flags & EWindowFlags_ShouldTerminate) {
				WindowHandle handle = (WindowHandle)i + 1;
				WindowManager_freeWindow(manager, &handle);
			}
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

Bool WindowManager_freeWindow(WindowManager *manager, WindowHandle *handle) {

	if(!handle)
		return false;

	Window *w = WindowManager_getWindow(manager, *handle);

	if(!w || !w->owner)
		return true;

	if(!WindowManager_isAccessible(w->owner)) 
		return false;

	//Ensure our window safely exits

	if(w->flags & EWindowFlags_IsActive && w->callbacks.onDestroy)
		w->callbacks.onDestroy(w);

	Buffer_freex(&w->cpuVisibleBuffer);
	Buffer_freex(&w->extendedData);
	CharString_freex(&w->title);

	List *devices = &w->devices;

	for(U64 i = 0; i < devices->length; ++i)
		InputDevice_free((InputDevice*) List_ptr(*devices, i));

	List_freex(devices);
	List_freex(&w->monitors);

	Bool b = true;

	if(w->type == EWindowType_Physical)
		b = WindowManager_freePhysical(w);

	*w = (Window) { 0 };

	if(*handle == manager->windows.length)
		while((w = (Window*)List_last(manager->windows)) != NULL && !w->owner)
			List_popBack(&manager->windows, Buffer_createNull());

	*handle = 0;
	return b;
}

Window *WindowManager_getWindow(WindowManager *manager, WindowHandle windowHandle) {
	return 
		manager && manager->owningThread == Thread_getId() && 
		windowHandle && windowHandle <= manager->windows.length ? 
		(Window*) List_ptr(manager->windows, windowHandle - 1) : NULL;
}
