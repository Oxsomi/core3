/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
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
		return Error_unsupportedOperation(0);		//Invalid endianness

	if(Platform_instance.platformType != EPlatform_Uninitialized)
		return Error_invalidOperation(0);

	if(!cmdArgc || !cmdArgs)
		return Error_invalidParameter(!cmdArgc ? 0 : 1, 0, 0);

	if(!free || !alloc)
		return Error_invalidParameter(!free ? 3 : 4, 0, 0);

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

	StringList sl = (StringList){ 0 };

	if(cmdArgc > 1) {

		err = StringList_createx(cmdArgc - 1, &sl);

		if (err.genericError) {
			WindowManager_free(&Platform_instance.windowManager);
			Platform_instance =	(Platform) { 0 };
			return err;
		}

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptr[i - 1] = String_createConstRefUnsafe(cmdArgs[i]);
	}

	Platform_instance.args = sl;

	if ((err = Platform_initExt(&Platform_instance, String_createConstRefUnsafe(cmdArgs[0]))).genericError) {
		StringList_freex(&sl);
		WindowManager_free(&Platform_instance.windowManager);
		Platform_instance =	(Platform) { 0 };
		return err;
	}

	return Error_none();
}

void Platform_cleanup() {

	if(Platform_instance.platformType == EPlatform_Uninitialized)
		return;

	Platform_cleanupExt(&Platform_instance);

	String_freex(&Platform_instance.workingDirectory);
	WindowManager_free(&Platform_instance.windowManager);
	StringList_freex(&Platform_instance.args);

	Platform_instance =	(Platform) { 0 };
}

Bool Lock_isLocked(Lock l) {
	return l.lockThread;
}

void Log_printCapturedStackTrace(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom((const void**) stackTrace, _STACKTRACE_SIZE, lvl, options);
}
