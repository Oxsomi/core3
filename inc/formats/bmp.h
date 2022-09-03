#pragma once

//TODO: writeR(G)(B), loadR(G)(B)(A), compression

struct Buffer BMP_writeRGBA(
	struct Buffer buf, u16 w, u16 h, bool isFlipped, 
	struct Allocator allocator
);