#pragma once
#include "types/types.h"

typedef struct Error Error;

Error BMP_writeRGBAx(
	Buffer buf,
	U16 w,
	U16 h,
	Bool isFlipped,
	Buffer *result
);
