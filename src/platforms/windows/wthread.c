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
#include "platforms/thread.h"
#include "platforms/windows/wplatform_ext.h"
#include "types/error.h"
#include "types/buffer.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

U32 Thread_getId() { return GetCurrentThreadId(); }

void Thread_sleep(Ns ns) {
	LARGE_INTEGER interval;
	interval.QuadPart = -(I64)((U64_min(ns, I64_MAX) + 99) / 100);
	((PlatformExt*)Platform_instance.dataExt)->ntDelayExecution(false, &interval);
}

U32 Thread_getLogicalCores() {

	if (Platform_instance.threads)
		return Platform_instance.threads;

	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return (Platform_instance.threads = info.dwNumberOfProcessors);
}

DWORD ThreadFunc(Thread *thread) {

	if(thread && thread->callback)
		thread->callback(thread->objectHandle);

	return 0;
}

Error Thread_create(ThreadCallbackFunction callback, void *objectHandle, Thread **thread) {

	if(!thread)
		return Error_nullPointer(2, "Thread_create()::thread is required");

	if(*thread)
		return Error_invalidParameter(2, 0, "Thread_create()::*thread isn't NULL, might indicate memleak");

	if(!callback)
		return Error_nullPointer(0, "Thread_create()::callback is required");

	Buffer buf = Buffer_createNull();

	Error err = Platform_instance.alloc.alloc(Platform_instance.alloc.ptr, sizeof(Thread), &buf);

	if (err.genericError)
		return err;

	Thread *thr = (*thread = (Thread*) buf.ptr);

	thr->callback = callback;
	thr->objectHandle = objectHandle;

	thr->nativeHandle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, thr, 0, NULL);

	if (!thr->nativeHandle) {
		Thread_free(thread);
		return Error_platformError(0, GetLastError(), "Thread_wait() couldn't create thread");
	}

	return Error_none();
}

Error Thread_wait(Thread *thread, U32 maxWaitTimeMs) {

	if(!thread)
		return Error_nullPointer(0, "Thread_wait()::thread is required");

	if(WaitForSingleObject(thread->nativeHandle, maxWaitTimeMs) == WAIT_FAILED)
		return Error_timedOut(0, maxWaitTimeMs, "Thread_wait() couldn't wait on thread");

	return Error_none();
}
