#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/assert.h"

void Thread_free(struct Thread **thread) {

	ocAssert("Thread is invalid", thread && *thread);

	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.ptr;

	free(allocator, (struct Buffer) { (u8*) *thread, sizeof(struct Thread) });
	*thread = NULL;
}

void Thread_waitAndCleanup(struct Thread **thread, u32 maxWaitTime) {
	Thread_wait(*thread, maxWaitTime);
	Thread_free(thread);
}