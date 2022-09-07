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

impl struct Error Thread_create(
	ThreadCallbackFunction callback, void* objectHandle,
	struct Thread **thread
);

impl struct Error Thread_wait(struct Thread *thread, u32 maxWaitTimeMs);

struct Error Thread_free(struct Thread **thread);
struct Error Thread_waitAndCleanup(struct Thread **thread, u32 maxWaitTime);