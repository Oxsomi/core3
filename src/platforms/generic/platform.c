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

#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"

Platform Platform_instance = { 0 };

Error Platform_create(
	int cmdArgc,
	const C8 *cmdArgs[], 
	void *data, 
	FreeFunc free,
	AllocFunc alloc,
	void *allocator
) {

	U16 v = 1;

	if(!*(U8*)&v)
		return Error_unsupportedOperation(0, "Platform_create() failed, invalid endianness (only little endian supported)");

	if(Platform_instance.platformType != EPlatform_Uninitialized)
		return Error_invalidOperation(0, "Platform_create() failed, platform was already initialized");

	if(!cmdArgc || !cmdArgs)
		return Error_invalidParameter(!cmdArgc ? 0 : 1, 0, "Platform_create()::cmdArgc and cmdArgs are required");

	if(!free || !alloc)
		return Error_invalidParameter(!free ? 3 : 4, 0, "Platform_create()::free and alloc are required");

	Platform_instance =	(Platform) {
		.platformType = _PLATFORM_TYPE,
		.data = data,
		.alloc = (Allocator) {
			.free = free,
			.alloc = alloc,
			.ptr = allocator
		}
	};

	Error err = WindowManager_create(&Platform_instance.windowManager);

	if (err.genericError) {
		Platform_instance =	(Platform) { 0 };
		return err;
	}

	CharStringList sl = (CharStringList){ 0 };

	if(cmdArgc > 1) {

		err = CharStringList_createx(cmdArgc - 1, &sl);

		if (err.genericError) {
			WindowManager_free(&Platform_instance.windowManager);
			Platform_instance =	(Platform) { 0 };
			return err;
		}

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptr[i - 1] = CharString_createConstRefCStr(cmdArgs[i]);
	}

	Platform_instance.args = sl;

	if ((err = Platform_initExt(CharString_createConstRefCStr(cmdArgs[0]))).genericError) {
		CharStringList_freex(&sl);
		WindowManager_free(&Platform_instance.windowManager);
		Platform_instance =	(Platform) { 0 };
		return err;
	}

	return Error_none();
}

void Platform_cleanup() {

	if(Platform_instance.platformType == EPlatform_Uninitialized)
		return;

	CharString_freex(&Platform_instance.workingDirectory);
	WindowManager_free(&Platform_instance.windowManager);
	CharStringList_freex(&Platform_instance.args);

	Platform_cleanupExt();

	Platform_instance =	(Platform) { 0 };
}

Bool Lock_isLockedForThread(Lock *l) { 
	return AtomicI64_add(&l->lockedThreadId, 0) == Thread_getId();
}

void Log_printCapturedStackTrace(Allocator alloc, const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom(alloc, (const void**) stackTrace, _STACKTRACE_SIZE, lvl, options);
}

void Log_printCapturedStackTracex(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTrace(Platform_instance.alloc, stackTrace, lvl, options);
}
