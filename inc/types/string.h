#pragma once
#include "types.h"

typedef c8 ShortString[64];

usz String_len(const c8 *c, usz maxSize);
usz String_lastIndex(const c8 *ptr, c8 c);
const c8 *String_lastPath(const c8 *ptr);