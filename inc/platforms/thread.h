#pragma once
#include "types/allocator.h"

typedef void (*ThreadCallbackFunction)(void*);

struct Thread {
	ThreadCallbackFunction callback;
	void *nativeHandle;
	FreeFunc *allocFree;
	void *allocator;
	void *objectHandle;
};

u32 Thread_getId();
u32 Thread_getLogicalCores();

struct Thread *Thread_start(
	ThreadCallbackFunction callback, void* objectHandle, 
	AllocFunc alloc, FreeFunc free, void* allocator
);

void Thread_wait(struct Thread *thread, u32 maxWaitTimeMs);
void Thread_cleanup(struct Thread **thread);
void Thread_waitAndCleanup(struct Thread **thread, u32 maxWaitTime);