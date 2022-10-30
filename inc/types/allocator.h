#pragma once
#include "types.h"

typedef struct Buffer Buffer;
typedef struct Error Error;

typedef Error (*AllocFunc)(void *allocator, U64 siz, Buffer *output);
typedef Error (*FreeFunc)(void *allocator, Buffer buf);

typedef struct Allocator {
	void *ptr;
	AllocFunc alloc;
	FreeFunc free;
} Allocator;