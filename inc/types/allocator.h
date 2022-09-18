#pragma once
#include "types.h"

typedef struct Error (*AllocFunc)(void *allocator, u64 siz, struct Buffer *output);
typedef struct Error (*FreeFunc)(void *allocator, struct Buffer buf);

struct Allocator {
	void *ptr;
	AllocFunc alloc;
	FreeFunc free;
};