#pragma once
#include "platforms/platform.h"

inline Error Buffer_createCopyx(Buffer buf, Buffer *output) {
	return Buffer_createCopy(buf, Platform_instance.alloc, output);
}

inline Error Buffer_createZeroBitsx(U64 siz, Buffer *output) {
	return Buffer_createZeroBits(siz, Platform_instance.alloc, output);
}

inline Error Buffer_createOneBitsx(U64 siz, Buffer *output) {
	return Buffer_createOneBits(siz, Platform_instance.alloc, output);
}

inline Error Buffer_createBitsx(U64 siz, Bool value, Buffer *result) {

	if (!value)
		return Buffer_createZeroBitsx(siz, result);

	return Buffer_createOneBitsx(siz, result);
}

inline Error Buffer_freex(Buffer *buf) {
	return Buffer_free(buf, Platform_instance.alloc);
}

inline Error Buffer_createEmptyBytesx(U64 siz, Buffer *output) {
	return Buffer_createEmptyBytes(siz, Platform_instance.alloc, output);
}

inline Error Buffer_createUninitializedBytesx(U64 siz, Buffer *output) {
	return Buffer_createUninitializedBytes(siz, Platform_instance.alloc, output);
}
