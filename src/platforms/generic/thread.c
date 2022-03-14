#include "platforms/thread.h"
#include "types/assert.h"

void Thread_cleanup(struct Thread **thread) {

	ocAssert("Thread is invalid", thread && *thread);

	(*thread)->allocFree((*thread)->allocator, (struct Buffer) { (u8*) *thread, sizeof(struct Thread) });
	*thread = NULL;
}

void Thread_waitAndCleanup(struct Thread **thread, u32 maxWaitTime) {
	Thread_wait(*thread, maxWaitTime);
	Thread_cleanup(thread);
}