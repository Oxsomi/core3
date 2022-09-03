#pragma once
#include "types/allocator.h"

typedef void (*ThreadCallbackFunction)(void*);

struct Thread {
	ThreadCallbackFunction callback;
	void *nativeHandle;
	void *objectHandle;
};

impl u32 Thread_getId();
impl u32 Thread_getLogicalCores();

impl struct Thread *Thread_create(
	ThreadCallbackFunction callback, void* objectHandle
);

impl void Thread_wait(struct Thread *thread, u32 maxWaitTimeMs);

void Thread_free(struct Thread **thread);
void Thread_waitAndCleanup(struct Thread **thread, u32 maxWaitTime);