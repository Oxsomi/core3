#pragma once
#include "math/vec.h"
#include "allocator.h"

struct BitRef {
	U8 *ptr, off;
};

inline Bool BitRef_get(struct BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
inline void BitRef_set(struct BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }
inline void BitRef_reset(struct BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }

inline void BitRef_setTo(struct BitRef b, Bool v) { 

	if(!b.ptr) return;

	if(v) 
		BitRef_set(b);

	else BitRef_reset(b);
}

struct Error Buffer_getBit(struct Buffer buf, U64 offset, Bool *output);

void Buffer_copy(struct Buffer dst, struct Buffer src);
void Buffer_revCopy(struct Buffer dst, struct Buffer src);		//Copies bytes from range backwards; useful if ranges overlap

struct Error Buffer_setBit(struct Buffer buf, U64 offset);
struct Error Buffer_resetBit(struct Buffer buf, U64 offset);

inline struct Error Buffer_setBitTo(struct Buffer buf, U64 offset, Bool value) {

	if (!value)
		return Buffer_resetBit(buf, offset);

	return Buffer_setBit(buf, offset);
}

struct Error Buffer_bitwiseOr(struct Buffer dst, struct Buffer src);
struct Error Buffer_bitwiseAnd(struct Buffer dst, struct Buffer src);
struct Error Buffer_bitwiseXor(struct Buffer dst, struct Buffer src);
struct Error Buffer_bitwiseNot(struct Buffer dst);

struct Error Buffer_setBitRange(struct Buffer dst, U64 dstOff, U64 bits);
struct Error Buffer_unsetBitRange(struct Buffer dst, U64 dstOff, U64 bits);

struct Error Buffer_setAllBits(struct Buffer dst);
struct Error Buffer_unsetAllBits(struct Buffer dst);

inline struct Error Buffer_setAllBitsTo(struct Buffer buf, Bool isOn) {

	if (isOn)
		return Buffer_setAllBits(buf);

	return Buffer_unsetAllBits(buf);
}

U64 Buffer_hash(struct Buffer buf);

//Comparison

struct Error Buffer_eq(struct Buffer buf0, struct Buffer buf1, Bool *result);		//Also compares size
struct Error Buffer_neq(struct Buffer buf0, struct Buffer buf1, Bool *result);		//Also compares size

//These should never be Buffer_free-d because Buffer doesn't know if it's allocated

inline struct Buffer Buffer_createNull() { return (struct Buffer) { 0 }; }
inline struct Buffer Buffer_createRef(void *v, U64 len) { return (struct Buffer) { .ptr = (U8*) v, .siz = len }; }

//All these functions allocate, so Buffer_free them later

struct Error Buffer_createCopy(struct Buffer buf, struct Allocator alloc, struct Buffer *output);
struct Error Buffer_createZeroBits(U64 siz, struct Allocator alloc, struct Buffer *output);
struct Error Buffer_createOneBits(U64 siz, struct Allocator alloc, struct Buffer *output);

inline struct Error Buffer_createBits(U64 siz, Bool value, struct Allocator alloc, struct Buffer *result) {

	if (!value)
		return Buffer_createZeroBits(siz, alloc, result);

	return Buffer_createOneBits(siz, alloc, result);
}

struct Error Buffer_free(struct Buffer *buf, struct Allocator alloc);
struct Error Buffer_createEmptyBytes(U64 siz, struct Allocator alloc, struct Buffer *output);

struct Error Buffer_createUninitializedBytes(U64 siz, struct Allocator alloc, struct Buffer *output);
struct Error Buffer_createSubset(struct Buffer buf, U64 offset, U64 siz, struct Buffer *output);

//Writing data

struct Error Buffer_offset(struct Buffer *buf, U64 siz);
struct Error Buffer_append(struct Buffer *buf, const void *v, U64 siz);
struct Error Buffer_appendBuffer(struct Buffer *buf, struct Buffer append);

inline struct Error Buffer_appendU64(struct Buffer *buf, U64 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendU32(struct Buffer *buf, U32 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendU16(struct Buffer *buf, U16 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendU8(struct Buffer *buf, U8 v) { return Buffer_append(buf, &v, sizeof(v)); }

inline struct Error Buffer_appendI64(struct Buffer *buf, I64 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendI32(struct Buffer *buf, I32 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendI16(struct Buffer *buf, I16 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendI8(struct Buffer *buf, I8 v) { return Buffer_append(buf, &v, sizeof(v)); }

inline struct Error Buffer_appendF64(struct Buffer *buf, F32 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendF32(struct Buffer *buf, F32 v) { return Buffer_append(buf, &v, sizeof(v)); }

inline struct Error Buffer_appendF32x4(struct Buffer *buf, F32x4 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendF32x2(struct Buffer *buf, I32x2 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendI32x4(struct Buffer *buf, I32x4 v) { return Buffer_append(buf, &v, sizeof(v)); }
inline struct Error Buffer_appendI32x2(struct Buffer *buf, I32x2 v) { return Buffer_append(buf, &v, sizeof(v)); }
