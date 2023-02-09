#pragma once
#include "types.h"
#include "allocator.h"

typedef Bool (*ObjectFreeFunc)(void *ptr, Allocator allocator);

typedef struct RefPtr {
	U64 refCount;
	void *ptr;
	Allocator alloc;
	ObjectFreeFunc free;
} RefPtr;

RefPtr RefPtr_create(void *ptr, Allocator alloc, ObjectFreeFunc free);

Bool RefPtr_add(RefPtr *ptr);
Bool RefPtr_sub(RefPtr *ptr);
