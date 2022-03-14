#include "types/string.h"
#include "math/math.h"

usz String_len(const c8 *c, usz maxSize) {

	if (!maxSize)
		maxSize = usz_MAX;

	usz i = 0;

	for (; i < maxSize && c[i]; ++i)
		;

	return i;
}

usz String_lastIndex(const c8 *ptr, c8 c) {

	if (!ptr) return usz_MAX;

	for (usz i = 0; ptr[i]; ++i)
		if (ptr[i] == c)
			return i;

	return usz_MAX;
}

const c8 *String_lastPath(const c8 *ptr) {

	if (!ptr) return NULL;

	usz last0 = String_lastIndex(ptr, '/');
	usz last1 = String_lastIndex(ptr, '\\');

	if (last0 == usz_MAX || last1 == usz_MAX)
		return ptr;

	if (last0 == usz_MAX)
		return ptr + last1;

	if (last1 == usz_MAX)
		return ptr + last0;

	return ptr + Math_minu(last0, last1);
}