#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/assert.h"

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

struct Thread *Thread_create(
	ThreadCallbackFunction callback, void *objectHandle
) {
	ocAssert("Invalid thread inputs", callback);
	
	struct Thread* thread = (struct Thread*) Platform_instance.alloc(
		Platform_instance.allocator, sizeof(struct Thread)
	);

	thread->callback = callback;
	thread->objectHandle = objectHandle;

	thread->nativeHandle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, thread, 0, NULL);

	ocAssert("Create thread returned null", thread->nativeHandle);

	return thread;
}

void Thread_wait(struct Thread *thread, u32 maxWaitTimeMs) {
	WaitForSingleObject(thread->nativeHandle, !maxWaitTimeMs ? u32_MAX : maxWaitTimeMs);
}