#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "types/buffer.h"

Bool Thread_free(Thread **thread) {

	if(!thread || !*thread)
		return true;

	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.ptr;

	Bool freed = free(allocator, Buffer_createManagedPtr(*thread, sizeof(Thread)));
	*thread = NULL;
	return freed;
}

Error Thread_waitAndCleanup(Thread **thread, U32 maxWaitTime) {

	if(!thread || !*thread)
		return Error_nullPointer(0, 0);

	Error err = Thread_wait(*thread, maxWaitTime);

	if(err.genericError == EGenericError_TimedOut)
		return err;

	Thread_free(thread);
	return err;
}
