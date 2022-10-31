#pragma once
#include "types.h"
#include "allocator.h"

typedef Error (*ObjectFreeFunc)(void *ptr, Allocator allocator);

typedef struct RefPtr {

	U64 refCount;
	void *ptr;
	Allocator alloc;
	ObjectFreeFunc free;

} RefPtr;

inline RefPtr RefPtr_create(void *ptr, Allocator alloc, ObjectFreeFunc free) {
	return (RefPtr) {
		.refCount = 1,
		.ptr = ptr,
		.alloc = alloc,
		.free = free
	};
}

bool RefPtr_add(RefPtr *ptr);
bool RefPtr_sub(RefPtr *ptr);
