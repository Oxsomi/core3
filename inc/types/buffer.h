#pragma once
#include "math/vec.h"
#include "allocator.h"

typedef struct BitRef {
	U8 *ptr, off;
} BitRef;

inline Bool BitRef_get(BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
inline void BitRef_set(BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }
inline void BitRef_reset(BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }

inline void BitRef_setTo(BitRef b, Bool v) { 

	if(!b.ptr) return;

	if(v) 
		BitRef_set(b);

	else BitRef_reset(b);
}

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output);

void Buffer_copy(Buffer dst, Buffer src);
void Buffer_revCopy(Buffer dst, Buffer src);		//Copies bytes from range backwards; useful if ranges overlap

Error Buffer_setBit(Buffer buf, U64 offset);
Error Buffer_resetBit(Buffer buf, U64 offset);

inline Error Buffer_setBitTo(Buffer buf, U64 offset, Bool value) {

	if (!value)
		return Buffer_resetBit(buf, offset);

	return Buffer_setBit(buf, offset);
}

Error Buffer_bitwiseOr(Buffer dst, Buffer src);
Error Buffer_bitwiseAnd(Buffer dst, Buffer src);
Error Buffer_bitwiseXor(Buffer dst, Buffer src);
Error Buffer_bitwiseNot(Buffer dst);

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits);
Error Buffer_unsetBitRange(Buffer dst, U64 dstOff, U64 bits);

Error Buffer_setAllBits(Buffer dst);
Error Buffer_unsetAllBits(Buffer dst);

inline Error Buffer_setAllBitsTo(Buffer buf, Bool isOn) {

	if (isOn)
		return Buffer_setAllBits(buf);

	return Buffer_unsetAllBits(buf);
}

U64 Buffer_hash(Buffer buf);

U32 Buffer_crc32(Buffer buf, U32 crcSeed);

//Comparison

Error Buffer_eq(Buffer buf0, Buffer buf1, Bool *result);		//Also compares size
Error Buffer_neq(Buffer buf0, Buffer buf1, Bool *result);		//Also compares size

//These should never be Buffer_free-d because Buffer doesn't know if it's allocated

inline Buffer Buffer_createNull() { return (Buffer) { 0 }; }
inline Buffer Buffer_createRef(void *v, U64 length) { return (Buffer) { .ptr = (U8*) v, .siz = length }; }

//All these functions allocate, so Buffer_free them later

Error Buffer_createCopy(Buffer buf, Allocator alloc, Buffer *output);
Error Buffer_createZeroBits(U64 siz, Allocator alloc, Buffer *output);
Error Buffer_createOneBits(U64 siz, Allocator alloc, Buffer *output);

inline Error Buffer_createBits(U64 siz, Bool value, Allocator alloc, Buffer *result) {

	if (!value)
		return Buffer_createZeroBits(siz, alloc, result);

	return Buffer_createOneBits(siz, alloc, result);
}

Error Buffer_free(Buffer *buf, Allocator alloc);
Error Buffer_createEmptyBytes(U64 siz, Allocator alloc, Buffer *output);

Error Buffer_createUninitializedBytes(U64 siz, Allocator alloc, Buffer *output);
Error Buffer_createSubset(Buffer buf, U64 offset, U64 siz, Buffer *output);

//Writing data

Error Buffer_offset(Buffer *buf, U64 siz);
Error Buffer_append(Buffer *buf, const void *v, U64 siz);
Error Buffer_appendBuffer(Buffer *buf, Buffer append);

inline Error Buffer_appendU64(Buffer *buf, U64 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendU32(Buffer *buf, U32 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendU16(Buffer *buf, U16 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendU8(Buffer *buf,  U8 v)  { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendC8(Buffer *buf,  C8 v)  { return Buffer_append(buf, &v, sizeof(v)); }

inline Error Buffer_appendI64(Buffer *buf, I64 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendI32(Buffer *buf, I32 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendI16(Buffer *buf, I16 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendI8(Buffer *buf,  I8 v)  { return Buffer_append(buf, &v, sizeof(v)); }

inline Error Buffer_appendF32(Buffer *buf, F32 v) { return Buffer_append(buf, &v, sizeof(v)); }

inline Error Buffer_appendF32x4(Buffer *buf, F32x4 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendF32x2(Buffer *buf, I32x2 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendI32x4(Buffer *buf, I32x4 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline Error Buffer_appendI32x2(Buffer *buf, I32x2 v) { return Buffer_append(buf, &v, sizeof(v)); }
