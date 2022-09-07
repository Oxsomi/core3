#pragma once
#include "types.h"

struct BitRef {
	u8 *ptr, off;
};

inline bool BitRef_get(struct BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
inline void BitRef_set(struct BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }
inline void BitRef_reset(struct BitRef b) { if(b.ptr) *b.ptr |= 1 << b.off; }

inline void BitRef_setTo(struct BitRef b, bool v) { 

	if(!b.ptr) return;

	if(v) 
		BitRef_set(b);

	else BitRef_reset(b);
}

struct Error Bit_get(struct Buffer buf, usz offset, bool *output);

void Bit_copy(struct Buffer dst, struct Buffer src);
void Bit_revCopy(struct Buffer dst, struct Buffer src);		//Copies bytes from range backwards; useful if ranges overlap

struct Error Bit_set(struct Buffer buf, usz offset);
struct Error Bit_reset(struct Buffer buf, usz offset);

inline struct Error Bit_setTo(struct Buffer buf, usz offset, bool value) {

	if (!value)
		return Bit_reset(buf, offset);

	return Bit_set(buf, offset);
}

struct Error Bit_or(struct Buffer dst, struct Buffer src);
struct Error Bit_and(struct Buffer dst, struct Buffer src);
struct Error Bit_xor(struct Buffer dst, struct Buffer src);
struct Error Bit_not(struct Buffer dst);

struct Error Bit_setRange(struct Buffer dst, usz dstOff, usz bits);
struct Error Bit_unsetRange(struct Buffer dst, usz dstOff, usz bits);

struct Error Bit_setAll(struct Buffer dst);
struct Error Bit_unsetAll(struct Buffer dst);

inline struct Error Bit_setAllTo(struct Buffer buf, bool isOn) {

	if (isOn)
		return Bit_setAll(buf);

	return Bit_unsetAll(buf);
}

u64 Bit_hash(struct Buffer buf);

//Comparison

struct Error Bit_cmp(struct Buffer buf0, struct Buffer buf1, i8 *result);		//Returns -1: <, 0: ==, 1: >
struct Error Bit_eq(struct Buffer buf0, struct Buffer buf1, bool *result);		//Also compares size
struct Error Bit_neq(struct Buffer buf0, struct Buffer buf1, bool *result);		//Also compares size

inline struct Error Bit_lt(struct Buffer buf0, struct Buffer buf1, bool *result) { 
	i8 res = 0;
	struct Error e = Bit_cmp(buf0, buf1, &res);
	*result = res < 0;
	return e;
}

struct Error Bit_gt(struct Buffer buf0, struct Buffer buf1, bool *result) { 
	i8 res = 0;
	struct Error e = Bit_cmp(buf0, buf1, &res);
	*result = res > 0;
	return e;
}

//All these functions allocate, so Bit_free them later

struct Buffer Bit_createRef(void *v, usz len) { return (struct Buffer) { .ptr = (u8*) v, .siz = len }; }

struct Error Bit_createDuplicate(struct Buffer buf, struct Allocator alloc, struct Buffer *output);
struct Error Bit_createEmpty(usz siz, struct Allocator alloc, struct Buffer *output);
struct Error Bit_createFull(usz siz, struct Allocator alloc, struct Buffer *output);

inline struct Error Bit_createFilled(usz siz, bool value, struct Allocator alloc, struct Buffer *result) {

	if (!value)
		return Bit_createEmpty(siz, alloc, result);

	return Bit_createFull(siz, alloc, result);
}

struct Error Bit_free(struct Buffer *buf, struct Allocator alloc);
struct Error Bit_createEmptyBytes(usz siz, struct Allocator alloc, struct Buffer *output);

struct Error Bit_createBytes(usz siz, struct Allocator alloc, struct Buffer *output);
struct Error Bit_createSubset(struct Buffer buf, usz offset, usz siz, struct Buffer *output);

//Writing data

struct Error Bit_offset(struct Buffer *buf, usz siz);
struct Error Bit_append(struct Buffer *buf, const void *v, usz siz);
struct Error Bit_appendBuffer(struct Buffer *buf, struct Buffer append);