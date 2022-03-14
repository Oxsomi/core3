#pragma once
#include "types.h"
#include "allocator.h"

bool Bit_get(struct Buffer buf, usz offset);
void Bit_set(struct Buffer buf, usz offset);
void Bit_reset(struct Buffer buf, usz offset);
void Bit_setTo(struct Buffer buf, usz offset, bool value);
void Bit_or(struct Buffer dst, struct Buffer src);
void Bit_and(struct Buffer dst, struct Buffer src);
void Bit_xor(struct Buffer dst, struct Buffer src);
void Bit_not(struct Buffer dst);

void Bit_setRange(struct Buffer dst, usz dstOff, usz bits);
void Bit_unsetRange(struct Buffer dst, usz dstOff, usz bits);

void Bit_setAllTo(struct Buffer dst, bool isOn);
void Bit_setAll(struct Buffer dst);
void Bit_unsetAll(struct Buffer dst);

//Comparison

bool Bit_eq(struct Buffer buf0, struct Buffer buf1);
bool Bit_neq(struct Buffer buf0, struct Buffer buf1);

//All these functions allocate, so Bit_free them later

struct Buffer Bit_duplicate(struct Buffer buf, AllocFunc alloc, void *allocator);
struct Buffer Bit_empty(usz siz, AllocFunc alloc, void *allocator);
struct Buffer Bit_full(usz siz, AllocFunc alloc, void *allocator);
struct Buffer Bit_fill(usz siz, bool value, AllocFunc alloc, void *allocator);

void Bit_free(struct Buffer *buf, FreeFunc freeFunc, void *allocator);
struct Buffer Bit_emptyBytes(usz siz, AllocFunc alloc, void *allocator);

struct Buffer Bit_bytes(usz siz, AllocFunc alloc, void *allocator);
struct Buffer Bit_subset(struct Buffer buf, usz offset, usz siz);

//Writing data

void Bit_offset(struct Buffer *buf, usz siz);
void Bit_appendU32(struct Buffer *buf, u32 v);
void Bit_append(struct Buffer *buf, const void *v, usz siz);
void Bit_appendBuffer(struct Buffer *buf, struct Buffer append);