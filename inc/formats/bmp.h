#pragma once
#include "types/allocator.h"

//TODO: writeR(G)(B), loadR(G)(B)(A), compression

struct Buffer BMP_writeRGBA(
	struct Buffer buf, u16 w, u16 h, bool isFlipped, 
	AllocFunc alloc, void *allocator
);