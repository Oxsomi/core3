#pragma once
#include "types.h"

struct Error Bit_get(struct Buffer buf, usz offset, bool *output);

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

struct Error Bit_eq(struct Buffer buf0, struct Buffer buf1, bool *result);
struct Error Bit_neq(struct Buffer buf0, struct Buffer buf1, bool *result);

//All these functions allocate, so Bit_free them later

struct Error Bit_duplicate(struct Buffer buf, struct Allocator alloc, struct Buffer *output);
struct Error Bit_empty(usz siz, struct Allocator alloc, struct Buffer *output);
struct Error Bit_full(usz siz, struct Allocator alloc, struct Buffer *output);

inline struct Error Bit_fill(usz siz, bool value, struct Allocator alloc, struct Buffer *result) {

	if (!value)
		return Bit_empty(siz, alloc, result);

	return Bit_full(siz, alloc, result);
}

struct Error Bit_free(struct Buffer *buf, struct Allocator alloc);
struct Error Bit_emptyBytes(usz siz, struct Allocator alloc, struct Buffer *output);

struct Error Bit_bytes(usz siz, struct Allocator alloc, struct Buffer *output);
struct Error Bit_subset(struct Buffer buf, usz offset, usz siz, struct Buffer *output);

//Writing data

struct Error Bit_offset(struct Buffer *buf, usz siz);
struct Error Bit_append(struct Buffer *buf, const void *v, usz siz);
struct Error Bit_appendBuffer(struct Buffer *buf, struct Buffer append);