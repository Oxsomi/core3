/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "platforms/platform.h"
#include "platforms/osx/objective_c.h"
#include "types/error.h"
#include "types/thread.h"
#include "types/atomic.h"
#include "platforms/log.h"

//Port of https://github.com/CodaFi/C-Macs/blob/master/CMacs/AppDelegate.c
//Platform is the one that holds the NSApp, since there can only be done.

extern id NSApp;

typedef struct AppDelegate { Class cls; } AppDelegate;

//Wait til app is created

AtomicI64 isReady;

Bool Platform_signalReady(AppDelegate *self, SEL cmd, id notif) {
	(void)self; (void)cmd; (void)notif;
	AtomicI64_add(&isReady, 1);
	return true;
}

//Initialize all ObjectiveC classes and functions

Error Platform_initUnixExt() {

	Log_debugLnx("Start!");

	//Get all classes

	for (U64 i = 0; i < EObjCClass_Count; ++i) {

		Class c = objc_getClass(EObjCClass_names[i]);

		if(!c)
			return Error_invalidState(0, "Platform_initUnixExt() couldn't get required class");

		EObjCClass_obj[i] = c;
	}

	Log_debugLnx("Complete get classes!");

	//Get all functions

	for (U64 i = 0; i < EObjCFunc_Count; ++i) {

		SEL sel = sel_getUid(EObjCFunc_names[i]);

		if(!sel)
			return Error_invalidState(0, "Platform_initUnixExt() couldn't get SEL / function");

		EObjCFunc_obj[i] = c;
	}

	Log_debugLnx("Complete get functions!");

	//Create auto release pool

	id pool = class_createInstance(clsNSAutoreleasePool(), 0);

	if(!pool)
		return Error_invalidState(0, "Platform_initUnixExt() failed to create auto release pool");

	if(!ObjC_sendId(pool, selInit()))
		return Error_invalidState(0, "Platform_initUnixExt() failed to init auto release pool");

	Log_debugLnx("Complete create autorelease pool!");

	//Create delegate

	Class delegateClass = objc_allocateClassPair(clsNSObject(), "AppDelegate", 0);

	if(!delegateClass)
		return Error_invalidState(0, "Platform_initUnixExt() failed to create delegateClass");

	if(!class_addMethod(delegateClass, selApplicationDidFinishLaunching(), (IMP)Platform_signalReady, "i@:@"))
		return Error_invalidState(0, "Platform_initUnixExt() failed to add function to delegateClass");

	objc_registerClassPair(delegateClass);

	Log_debugLnx("Complete create delegate!");

	//Instantiate application with our delegate

	ObjC_sendId(clsNSApplication(), selSharedApplication());

	if(!NSApp)
		return Error_invalidState(0, "Platform_initUnixExt() failed to create NSApplication");

	id delegateObj = ObjC_sendId((id)objc_getClass("AppDelegate"), selAlloc());

	if(!delegateObj)
		return Error_invalidState(1, "Platform_initUnixExt() failed to create AppDelegate");

	delegateObj = ObjC_sendId(delegateObj, selInit());

	if(!delegateObj)
		return Error_invalidState(2, "Platform_initUnixExt() failed to init AppDelegate");

	ObjC_sendVoidPtr(NSApp, selSetDelegate(), delegateObj);

	Log_debugLnx("Complete create delegate!");

	ObjC_send(NSApp, selRun());

	//Wait for the app to be ready (interval of 100ns)

	Ns i = 0;

	while(!AtomicI64_load(&isReady) && i < 2 * SECOND) {
		Thread_sleep(100);
		i += 100;
	}

	if(i >= 2 * SECOND)
		return Error_invalidState(3, "Platform_initUnixExt() failed to initialize the app; timed out");

	Log_debugLnx("Success!");

	return Error_none();
}

void Platform_cleanupUnixExt() { }

I32 main(I32 argc, const C8 *argv[]) {

	Error err = Platform_create(argc, argv, NULL, NULL);

	if(err.genericError)
		return -1;

	I32 res = Program_run();
	Program_exit();
	Platform_cleanup();

	return res;
}