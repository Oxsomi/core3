#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/error.h"

#include <Windows.h>

u32 Thread_getId() { return GetCurrentThreadId(); }

u32 Thread_getLogicalCores() {

	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

DWORD ThreadFunc(struct Thread *thread) {
	thread->callback(thread->objectHandle);
	return 0;
}

struct Error Thread_create(
	ThreadCallbackFunction callback, void *objectHandle,
	struct Thread **thread
) {

	if(!thread)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if(*thread)
		return (struct Error) { 
			.genericError = GenericError_InvalidParameter, 
			.paramId = 2, .paramValue0 = (usz) thread 
		};

	if(!callback)
		return (struct Error) { .genericError = GenericError_NullPointer };

	struct Buffer buf = (struct Buffer) { 0 };

	struct Error err = Platform_instance.alloc.alloc(
		Platform_instance.alloc.ptr, sizeof(struct Thread), &buf
	);

	if (err.genericError)
		return err;

	struct Thread *thr = (*thread = (struct Thread*) buf.ptr);

	thr->callback = callback;
	thr->objectHandle = objectHandle;

	thr->nativeHandle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, thread, 0, NULL);

	if (!thr->nativeHandle) {
		Thread_free(thread);
		return (struct Error) { .genericError = GenericError_InvalidOperation };
	}

	return Error_none();
}

struct Error Thread_wait(struct Thread *thread, u32 maxWaitTimeMs) {

	if(WaitForSingleObject(thread->nativeHandle, !maxWaitTimeMs ? u32_MAX : maxWaitTimeMs) == WAIT_FAILED)
		return (struct Error) {
			.genericError = GenericError_InvalidOperation
		};

	return Error_none();
}