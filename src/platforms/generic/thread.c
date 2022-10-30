#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "types/buffer.h"

Error Thread_free(Thread **thread) {

	if(!thread || !*thread)
		return (Error) { .genericError = EGenericError_NullPointer };

	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.ptr;

	Error err = free(allocator, Buffer_createRef(*thread, sizeof(Thread)));
	*thread = NULL;
	return err;
}

Error Thread_waitAndCleanup(Thread **thread, U32 maxWaitTime) {

	if(!thread || !*thread)
		return (Error) { .genericError = EGenericError_NullPointer };

	Error err = Thread_wait(*thread, maxWaitTime);

	if(err.genericError == EGenericError_TimedOut)
		return err;

	if (err.genericError) {
		Thread_free(thread);
		return err;
	}

	return Thread_free(thread);
}
