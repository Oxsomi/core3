#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/error.h"

struct Error Thread_free(struct Thread **thread) {

	if(!thread || !*thread)
		return (struct Error) { .genericError = GenericError_NullPointer };

	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.ptr;

	struct Error err = free(allocator, (struct Buffer) { (u8*) *thread, sizeof(struct Thread) });
	*thread = NULL;
	return err;
}

struct Error Thread_waitAndCleanup(struct Thread **thread, u32 maxWaitTime) {

	if(!thread || !*thread)
		return (struct Error) { .genericError = GenericError_NullPointer };

	struct Error err = Thread_wait(*thread, maxWaitTime);

	if (err.genericError) {
		Thread_free(thread);
		return err;
	}

	return Thread_free(thread);
}