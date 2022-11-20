#pragma once
#include "platforms/platform.h"

inline Error Buffer_createCopyx(Buffer buf, Buffer *output) {
	return Buffer_createCopy(buf, Platform_instance.alloc, output);
}

inline Error Buffer_createZeroBitsx(U64 length, Buffer *output) {
	return Buffer_createZeroBits(length, Platform_instance.alloc, output);
}

inline Error Buffer_createOneBitsx(U64 length, Buffer *output) {
	return Buffer_createOneBits(length, Platform_instance.alloc, output);
}

inline Error Buffer_createBitsx(U64 length, Bool value, Buffer *result) {

	if (!value)
		return Buffer_createZeroBitsx(length, result);

	return Buffer_createOneBitsx(length, result);
}

inline Error Buffer_freex(Buffer *buf) {
	return Buffer_free(buf, Platform_instance.alloc);
}

inline Error Buffer_createEmptyBytesx(U64 length, Buffer *output) {
	return Buffer_createEmptyBytes(length, Platform_instance.alloc, output);
}

inline Error Buffer_createUninitializedBytesx(U64 length, Buffer *output) {
	return Buffer_createUninitializedBytes(length, Platform_instance.alloc, output);
}
