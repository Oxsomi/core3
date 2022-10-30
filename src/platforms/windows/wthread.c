#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "types/buffer.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

U32 Thread_getId() { return GetCurrentThreadId(); }

void Thread_sleep(Ns ns) {
	Sleep((DWORD)((U64_min(ns, U32_MAX * ms - ms) + ms - 1) / ms));
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

Error Thread_create(
	ThreadCallbackFunction callback, void *objectHandle,
	Thread **thread
) {

	if(!thread)
		return (Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if(*thread)
		return (Error) { 
			.genericError = GenericError_InvalidParameter, 
			.paramId = 2, .paramValue0 = (U64) thread 
		};

	if(!callback)
		return (Error) { .genericError = GenericError_NullPointer };

	Buffer buf = Buffer_createNull();

	Error err = Platform_instance.alloc.alloc(
		Platform_instance.alloc.ptr, sizeof(Thread), &buf
	);

	if (err.genericError)
		return err;

	Thread *thr = (*thread = (Thread*) buf.ptr);

	thr->callback = callback;
	thr->objectHandle = objectHandle;

	thr->nativeHandle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, thr, 0, NULL);

	if (!thr->nativeHandle) {
		Thread_free(thread);
		return (Error) { .genericError = GenericError_InvalidOperation };
	}

	return Error_none();
}

Error Thread_wait(Thread *thread, U32 maxWaitTimeMs) {

	if(WaitForSingleObject(thread->nativeHandle, maxWaitTimeMs) == WAIT_FAILED)
		return (Error) {
			.genericError = GenericError_TimedOut
		};

	return Error_none();
}
