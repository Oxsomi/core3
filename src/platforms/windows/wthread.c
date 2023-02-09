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
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}

DWORD ThreadFunc(Thread *thread) {

	if(thread && thread->callback)
		thread->callback(thread->objectHandle);

	return 0;
}

Error Thread_create(ThreadCallbackFunction callback, void *objectHandle, Thread **thread) {

	if(!thread)
		return Error_nullPointer(2, 0);

	if(*thread)
		return Error_invalidParameter(2, 0, 0);

	if(!callback)
		return Error_nullPointer(0, 0);

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
		return Error_platformError(0, GetLastError());
	}

	return Error_none();
}

Error Thread_wait(Thread *thread, U32 maxWaitTimeMs) {

	if(!thread)
		return Error_nullPointer(0, 0);

	if(WaitForSingleObject(thread->nativeHandle, maxWaitTimeMs) == WAIT_FAILED)
		return Error_timedOut(0, maxWaitTimeMs);

	return Error_none();
}
