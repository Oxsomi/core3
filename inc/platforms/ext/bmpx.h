#pragma once
#include "formats/bmp.h"
#include "platforms/platform.h"

inline Error BMP_writeRGBAx(
	Buffer buf, U16 w, U16 h, Bool isFlipped,
	Buffer *result
) {
	return BMP_writeRGBA(buf, w, h, isFlipped, Platform_instance.alloc, result);
}