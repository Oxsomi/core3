#pragma once

//TODO: writeR(G)(B), loadR(G)(B)(A), compression

struct Error BMP_writeRGBA(
	struct Buffer buf, U16 w, U16 h, Bool isFlipped, 
	struct Allocator allocator,
	struct Buffer *result
);
