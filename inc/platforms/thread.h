#pragma once
#include "types/allocator.h"

typedef void (*ThreadCallbackFunction)(void*);

struct Thread {
	ThreadCallbackFunction callback;
	void *nativeHandle;
	void *objectHandle;
};

impl U32 Thread_getId();					//Current thread id
impl U32 Thread_getLogicalCores();

impl void Thread_sleep(Ns ns);				//Can be in a different time unit. Ex. on Windows it's rounded up to ms

impl struct Error Thread_create(
	ThreadCallbackFunction callback, void *objectHandle,
	struct Thread **thread
);

impl struct Error Thread_wait(struct Thread *thread, U32 maxWaitTimeMs);

struct Error Thread_free(struct Thread **thread);
struct Error Thread_waitAndCleanup(struct Thread **thread, U32 maxWaitTimeMs);