#pragma once
#include "types.h"

typedef struct Buffer Buffer;
typedef struct Error Error;

typedef Error (*AllocFunc)(void *allocator, U64 length, Buffer *output);

//Free should only return if it successfully freed.
//It shouldn't return any errors, as freeing also happens on cleanup.
//This could bring the program into an invalid state.
//
typedef Bool (*FreeFunc)(void *allocator, Buffer buf);

typedef struct Allocator {
	void *ptr;
	AllocFunc alloc;
	FreeFunc free;
} Allocator;
