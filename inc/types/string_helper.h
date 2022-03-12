#pragma once
#include "types.h"

inline usz String_len(const c8 *c, usz maxSize) {

	if (!maxSize)
		maxSize = usz_MAX;

	usz i = 0;

	for (; i < maxSize && c[i]; ++i)
		;

	return i;
}