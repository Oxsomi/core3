#pragma once
#include "types/types.h"

typedef struct Allocator Allocator;
typedef struct Error Error;

//TODO: writeR(G)(B), loadR(G)(B)(A), compression

Error BMP_writeRGBA(
	Buffer buf, U16 w, U16 h, Bool isFlipped, 
	Allocator allocator,
	Buffer *result
);
