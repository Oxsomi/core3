#pragma once
#include "types/allocator.h"

typedef void (*ThreadCallbackFunction)(void*);

typedef struct Thread {
	ThreadCallbackFunction callback;
	void *nativeHandle, *objectHandle;
} Thread;

impl U32 Thread_getId();					//Current thread id
impl U32 Thread_getLogicalCores();

impl void Thread_sleep(Ns ns);				//Can be in a different time unit. Ex. on Windows it's rounded up to ms

impl Error Thread_create(ThreadCallbackFunction callback, void *objectHandle, Thread **thread);
Bool Thread_free(Thread **thread);

impl Error Thread_wait(Thread *thread, U32 maxWaitTimeMs);
Error Thread_waitAndCleanup(Thread **thread, U32 maxWaitTimeMs);
