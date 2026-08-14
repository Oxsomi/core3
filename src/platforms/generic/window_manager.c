/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//platforms/generic/window_manager.c

#include "types/container/list_impl.h"
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/platforms_types.h"
#include "platforms/logx.h"
#include "platforms/input_device.h"
#include "types/container/ref_ptr.h"
#include "types/math/vec2i.h"
#include "types/base/string_base.h"
#include "types/base/thread.h"
#include "types/base/time.h"
#include "types/base/constants.h"

TListNamedImpl(ListWindowPtr);

const U32 WindowManager_magic = C8x4('W', 'I', 'N', 'D');

impl Bool WindowManager_createNative(WindowManager *w, Error *e_rr);
impl Bool WindowManager_freeNative(WindowManager *w);

impl Bool WindowManager_updateMonitors(WindowManager *wm, Error *e_rr);

void Window_free(void *ptr, const Allocator *allocator);

extern U32 Window_extSize;

Bool WindowManager_create(WindowManagerCallbacks callbacks, U64 extendedDataSize, WindowManager *manager, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;
	Buffer extendedData = Buffer_createNull();

	if (!manager)
		retError(clean, Error_nullPointer(2, "WindowManager_create()::manager is required"));

	if(extendedDataSize)
		gotoIfError3(clean, Buffer_createEmptyBytes(extendedDataSize, Platform_instance->alloc, &extendedData, e_rr));

	*manager = (WindowManager) {
		.isActive = WindowManager_magic,
		.owningThread = Thread_getId(),
		.callbacks = callbacks,
		.extendedData = extendedData,
		.windowTypePhysical = (RefPtrType) {
			.typeId = (TypeId) EPlatformsTypeId_Window,
			.lengthAndAlignment = RefPtrType_pack(sizeof(Window) + Window_extSize, alignof(Window)),
			.alloc = Platform_instance->alloc,
			.free = Window_free
		},
		.windowTypeVirtual = (RefPtrType) {
			.typeId = (TypeId) EPlatformsTypeId_Window,
			.lengthAndAlignment = RefPtrType_pack(sizeof(Window), alignof(Window)),
			.alloc = Platform_instance->alloc,
			.free = Window_free
		}
	};

	if(!WindowManager_createNative(manager, NULL))
		Log_warnLnx(
			"WindowManager_createNative failed, indicating a possible headless instance. Physical windows will be unavailable"
		);

	allocated = true;

	gotoIfError3(clean, WindowManager_updateMonitors(manager, e_rr));

	if(callbacks.onCreate)
		gotoIfError3(clean, callbacks.onCreate(manager, e_rr));

clean:

	if(!s_uccess && allocated) {
		manager->callbacks.onDestroy = NULL;        //onCreate hasn't succeeded, so don't call onDestroy.
		WindowManager_free(manager);
	}

	return s_uccess;
}

Bool WindowManager_isAccessible(const WindowManager *manager) {
	return manager && manager->isActive == WindowManager_magic && manager->owningThread == Thread_getId();
}

void WindowManager_free(WindowManager *manager) {

	if(!manager || manager->isActive != WindowManager_magic)
		return;

	if(manager->owningThread != Thread_getId())
		return;

	if(manager->callbacks.onDestroy)
		manager->callbacks.onDestroy(manager);

	manager->isActive = 0;

	if(manager->windows.length)
		Log_errorLnx("WindowManager shut down with window refs leaked. Trying its best to clean up, could cause corruption!");

	for(U64 i = manager->windows.length - 1; i != U64_MAX; --i) {

		RefPtr *ref = manager->windows.ptrNonConst[i];

		while(AtomicI64_load(&ref->refCount) > 0) {
			RefPtr *tmp = ref;
			RefPtr_dec(&tmp);    //Deleting the Window will also remove it from manager->windows
		}
	}

	ListWindowPtr_free(&manager->windows, Platform_instance->alloc);
	ListMonitor_free(&manager->monitors, Platform_instance->alloc);
	Buffer_free(&manager->extendedData, Platform_instance->alloc);

	WindowManager_freeNative(manager);
	Buffer_free(&manager->platformData, Platform_instance->alloc);
}

Bool WindowManager_adaptSizes(I32x2 *size, I32x2 *minSize, I32x2 *maxSize, Error *e_rr);

impl void WindowManager_freePhysical(Window *w);
impl Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr);

//Forgetting a window will erase it from the manager if relevant and will free the physical surface.
//Everything else will stay 'valid' until the last WindowRef is dropped, but the window is effectively gone.
//This is to allow the WindowRef to say alive for checking (e.g. IsActive) until the app is done with it.
void WindowManager_forgetWindow(WindowManager *manager, WindowRef *wRef) {

	ListWindowPtr_eraseFirst(&manager->windows, wRef, 0, NULL, NULL);

	Window *w = RefPtr_data(wRef, Window);

	if(w->flags & EWindowFlags_IsActive) {

		if(w->callbacks.onDestroy)
			w->callbacks.onDestroy(w);

		if(w->type == EWindowType_Physical)
			WindowManager_freePhysical(w);

		if(w->hint & EWindowHint_ProvideCPUBuffer)
			Buffer_free(&w->cpuVisibleBuffer, Platform_instance->alloc);

		w->flags &= ~EWindowFlags_IsActive;
	}
}

void Window_free(void *wGeneric, const Allocator *alloc) {

	Window *w = (Window*) wGeneric;

	if(!WindowManager_isAccessible(w->owner)) {
		Log_warnLnx("WindowManager tried to free Window but it didn't have access");
		return;
	}

	WindowManager_forgetWindow(w->owner, (WindowRef*)w - 1);     //User can drop Window before WindowManager does

	Buffer_free(&w->cpuVisibleBuffer, alloc);
	Buffer_free(&w->extendedData, alloc);
	CharString_free(&w->title, alloc);

	ListInputDevice *devices = &w->devices;

	for(U64 i = 0; i < devices->length; ++i)
		InputDevice_free(&devices->ptrNonConst[i], alloc);

	ListInputDevice_free(devices, alloc);
	ListMonitor_free(&w->monitors, alloc);
}

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
	WindowRef **result,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool alloc = false;

	Buffer cpuVisibleBuffer = Buffer_createNull();
	Buffer extendedData = Buffer_createNull();
	CharString titleCopy = CharString_createNull();

	if(!result)
		retError(clean, Error_nullPointer(
			!result ? 4 : 2, "WindowManager_createWindow()::result and callbacks.onDraw are required"
		));

	if(!WindowManager_isAccessible(manager))
		retError(clean, Error_invalidOperation(
			0, "WindowManager_createWindow() manager is NULL or inaccessible to current thread"
		));

	if(*result)
		retError(clean, Error_invalidOperation(
			1, "WindowManager_createWindow()::*result is not NULL, indicates possible memleak"
		));

	if(I32x2_any(I32x2_leq(size, I32x2_zero)))
		retError(clean, Error_outOfBounds(
			1, (U64) (I64) I32x2_x(size), (U64) (I64) I32x2_y(size),
			"WindowManager_createWindow()::size[i] must be >0"
		));

	if(CharString_length(title) >= 260)
		retError(clean, Error_outOfBounds(
			7, 260, 260, "WindowManager_createWindow()::title can't exceed 260 chars"
		));

	switch (format) {

		case EWindowFormat_RGBA8:

			#if _PLATFORM_TYPE != PLATFORM_ANDROID
				retError(clean, Error_invalidOperation(
					1, "WindowManager_createWindow()::RGBA8 is unsupported "
				));
			#endif

		case EWindowFormat_BGRA8:
		case EWindowFormat_BGR10A2:
		case EWindowFormat_RGBA16f:
		case EWindowFormat_RGBA32f:
			break;

		case EWindowFormat_AutoRGBA8:
			format = _PLATFORM_TYPE == PLATFORM_ANDROID ? EWindowFormat_RGBA8 : EWindowFormat_BGRA8;
			break;

		default:
			retError(clean, Error_invalidEnum(
				3, (U64) format, 0,
				"WindowManager_createWindow()::format must be one of BGRA8, BGR10A2, RGBA16f, RGBA32f"
			));
	}

	//Ensure the sizes are valid

	gotoIfError3(clean, WindowManager_adaptSizes(&size, &minSize, &maxSize, e_rr));

	if(type == EWindowType_Virtual) {
		gotoIfError3(clean, Buffer_createEmptyBytes(
			ETextureFormat_getSize((ETextureFormat) format, I32x2_x(size), I32x2_y(size), 1),
			Platform_instance->alloc,
			&cpuVisibleBuffer,
			e_rr
		));
	}

	else if(type == EWindowType_Physical) {

		if(!manager->platformData.ptr)
			retError(clean, Error_unsupportedOperation(
				0, "WindowManager_createWindow() can't create a physical window if createNative failed (headless)"
			));

		if(manager->isSingleWindow && (hint & (EWindowHint_NoBorder | EWindowHint_Transparency | EWindowHint_DisableResize))) {
			hint &= ~(EWindowHint_NoBorder | EWindowHint_Transparency | EWindowHint_DisableResize);
			Log_warnLnx(
				"WindowManager_createWindow() Physical window was created with unsupported flags;"
				" borderless, transparent or disableResize.\n"
				"The underlying system only supports one window at a time, so these hints are ignored."
			);
		}
	}

	if(extendedDataSize)
		gotoIfError3(clean, Buffer_createEmptyBytes(extendedDataSize, Platform_instance->alloc, &extendedData, e_rr));

	gotoIfError3(clean, CharString_createCopy(title, Platform_instance->alloc, &titleCopy, e_rr));

	gotoIfError3(clean, RefPtr_create(
		type == EWindowType_Virtual ? &manager->windowTypeVirtual : &manager->windowTypePhysical,
		result,
		e_rr
	));

	alloc = true;

	if(manager->isSingleWindow && type == EWindowType_Physical)
		for(U64 i = 0; i < manager->windows.length; ++i)
			if(RefPtr_data(manager->windows.ptr[i], Window)->type == EWindowType_Physical)
				retError(clean, Error_invalidOperation(
					0,
					"WindowManager_createWindow() attempted to create a physical window, but there was already one present. "
					"This is not allowed on single window systems."
				));

	Window *w = RefPtr_data(*result, Window);
	gotoIfError3(clean, ListWindowPtr_pushBack(&manager->windows, *result, Platform_instance->alloc, e_rr));

	*w = (Window) {

		.owner = manager,

		.type = type,
		.hint = (WindowHint) (hint | (type == EWindowType_Virtual ? EWindowHint_ProvideCPUBuffer : 0)),
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

	extendedData = Buffer_createNull();
	cpuVisibleBuffer = Buffer_createNull();
	titleCopy = CharString_createNull();

	if (type == EWindowType_Physical)
		gotoIfError3(clean, WindowManager_createWindowPhysical(w, e_rr));

clean:

	if(alloc && !s_uccess)
		RefPtr_dec(result);

	Buffer_free(&extendedData, Platform_instance->alloc);
	Buffer_free(&cpuVisibleBuffer, Platform_instance->alloc);
	CharString_free(&titleCopy, Platform_instance->alloc);

	return s_uccess;
}

impl void WindowManager_updateExt(WindowManager *manager);

Bool WindowManager_step(WindowManager *manager, Window *forcingUpdate, Error *e_rr) {

	Bool s_uccess = true;

	if(!WindowManager_isAccessible(manager))
		retError(clean, Error_invalidOperation(0, "WindowManager_step() manager is NULL or inaccessible to current thread"));

	//Update all windows first

	for (U64 i = manager->windows.length - 1; i != U64_MAX; --i) {

		WindowRef *wref = ListWindowPtr_at(manager->windows, i);
		Window *w = RefPtr_data(wref, Window);

		if(w == forcingUpdate)    //Has already been processed
			continue;

		if(!(w->flags & EWindowFlags_IsFinalized))    //Window not ready yet, wait until ready
			continue;

		//Update interface

		Bool requireDraw = w->type != EWindowType_Physical;

		#if _PLATFORM_TYPE != PLATFORM_WINDOWS

			//This is done manually by WM_PAINT on Windows since WindowManager_step isn't always called.
			//Sometimes repaints can happen at random moments, so WM_PAINT handles that.

			if(w->type == EWindowType_Physical) {

				InputDevice *dit = ListInputDevice_begin(w->devices);
				InputDevice *dend = ListInputDevice_end(w->devices);

				for(; dit != dend; ++dit)
					InputDevice_markUpdate(dit);
			}

			requireDraw = true;                            //We are in charge of the draw in non Windows systems

		#endif

		Bool isBackground = (w->flags & EWindowFlags_IsMinimized) && !(w->hint & EWindowHint_AllowBackgroundUpdates);

		if(!isBackground) {

			const Ns now = Time_now();

			if (w->callbacks.onUpdate) {
				const F64 dt = w->lastUpdate ? (now - w->lastUpdate) / (F64)SECOND : 0;
				w->callbacks.onUpdate(w, dt);
			}

			w->lastUpdate = now;

			if(requireDraw && w->callbacks.onDraw)          //Virtual or non Windows
				w->callbacks.onDraw(w);

			if(requireDraw && w->type == EWindowType_Physical && (w->hint & EWindowHint_ProvideCPUBuffer))
				Window_presentPhysical(w, NULL);
		}

		if (w->flags & EWindowFlags_ShouldTerminate)    //Just in case the window closed now, pop the window
			WindowManager_forgetWindow(manager, wref);  //refs still remain valid until user close them or WindowManager closes
	}

	//Then run window manager update; this polls all events (only used if it's not called by WM_PAINT)

	if(!forcingUpdate)
		WindowManager_updateExt(manager);

	if(manager->monitorsDirty) {

		manager->monitorsDirty = false;

		Error err = Error_none();
		if(!WindowManager_updateMonitors(manager, &err))
			Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_NewLine);
	}

	//Finally, we can notify manager that we're ready for draws/updates

	const Ns now = Time_now();

	if(manager->callbacks.onUpdate) {
		const F64 dt = manager->lastUpdate ? (now - manager->lastUpdate) / (F64)SECOND : 0;
		manager->callbacks.onUpdate(manager, dt);
	}

	if(manager->callbacks.onDraw)
		manager->callbacks.onDraw(manager);

	manager->lastUpdate = now;

clean:
	return s_uccess;
}

Bool WindowManager_wait(WindowManager *manager, Error *e_rr) {

	Bool s_uccess = true;

	if(!WindowManager_isAccessible(manager))
		retError(clean, Error_invalidOperation(
			0, "WindowManager_wait() manager is NULL or inaccessible to current thread"
		));

	while(manager->windows.length)
		gotoIfError3(clean, WindowManager_step(manager, NULL, e_rr));

clean:
	return s_uccess;
}

Bool WindowManager_adaptSizes(I32x2 *sizep, I32x2 *minSizep, I32x2 *maxSizep, Error *e_rr) {

	Bool s_uccess = true;

	if(!sizep || !minSizep || !maxSizep)
		retError(clean, Error_nullPointer(
			!sizep ? 0 : (!minSizep ? 1 : 2), "WindowManager_adaptSizes() requires sizep, minSizep and maxSizep"
		));

	const I32x2 size = *sizep;
	I32x2 minSize = *minSizep;
	I32x2 maxSize = *maxSizep;

	//Verify size

	if(I32x2_any(I32x2_leq(size, I32x2_zero)))
		retError(clean, Error_invalidParameter(2, 0, "WindowManager_adaptSizes()::*sizep should be >0"));

	//Verify min size.
	//By default should be 360p+.
	//Can't go below EResolution_SD.

	if(I32x2_any(I32x2_eq(minSize, I32x2_zero)))
		minSize = EResolution_get(EResolution_360);

	if(I32x2_any(I32x2_lt(minSize, EResolution_get(EResolution_SD))) || I32x2_any(I32x2_lt(minSize, I32x2_zero)))
		retError(clean, Error_invalidParameter(
			3, 0, "WindowManager_adaptSizes()::*minSizep should be >=240p (0 = 640x360, >=426x240)"
		));

	//Graphics APIs generally limit the resolution to 16Ki or 32Ki, so let's ensure the window can't get bigger than that

	if(I32x2_any(I32x2_eq(maxSize, I32x2_zero)))
		maxSize = I32x2_xx2(16384);

	if(I32x2_any(I32x2_gt(maxSize, I32x2_xx2(16384))) || I32x2_any(I32x2_lt(maxSize, I32x2_zero)))
		retError(clean, Error_invalidParameter(
			4, 0, "WindowManager_adaptSizes()::*maxSizep should be >=0 && <16384"
		));

	*minSizep = minSize;
	*maxSizep = maxSize;
	*sizep = I32x2_clamp(size, minSize, maxSize);

clean:
	return s_uccess;
}
