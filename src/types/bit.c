#include "types/bit.h"
#include "types/assert.h"
#include "math/math.h"

bool Bit_get(struct Buffer buf, usz offset) {
	ocAssert("Buffer cannot be null", buf.ptr && buf.siz);
	ocAssert("Buffer out of bounds", (offset >> 3) < buf.siz);
	return (buf.ptr[offset >> 3] >> (offset & 7)) & 1;
}

void Bit_set(struct Buffer buf, usz offset) {
	ocAssert("Buffer cannot be null", buf.ptr && buf.siz);
	ocAssert("Buffer out of bounds", (offset >> 3) < buf.siz);
	buf.ptr[offset >> 3] |= 1 << (offset & 7);
}

void Bit_reset(struct Buffer buf, usz offset) {
	ocAssert("Buffer cannot be null", buf.ptr && buf.siz);
	ocAssert("Buffer out of bounds", (offset >> 3) < buf.siz);
	buf.ptr[offset >> 3] &= ~(1 << (offset & 7));
}

void Bit_setTo(struct Buffer buf, usz offset, bool value) {

	if (!value)
		Bit_reset(buf, offset);

	else Bit_set(buf, offset);
}

#define BitOp(x,src,dst)																\
																						\
	ocAssert("Buffers cannot be null", dst.ptr && dst.siz && src.ptr && src.siz);		\
																						\
	usz l = Math_minu(dst.siz, src.siz);												\
																						\
	for(usz i = 0, j = l >> 3; i < j; ++i)												\
		*((u64*)dst.ptr + i) x *((const u64*)src.ptr + i);								\
																						\
	for (usz i = l >> 3 << 3; i < l; ++i)												\
		dst.ptr[i] x src.ptr[i];

void Bit_or(struct Buffer dst, struct Buffer src) {
	BitOp(|=, src, dst);
}

void Bit_xor(struct Buffer dst, struct Buffer src) {
	BitOp(^=, src, dst);
}

void Bit_and(struct Buffer dst, struct Buffer src) {

	BitOp(&=, src, dst);

	//Clear remainder of ours of bits that don't exist in src

	for (usz i = l; i < dst.siz; ++i)
		dst.ptr[i] = 0;
}

void Bit_not(struct Buffer dst) {
	BitOp(=~, dst, dst);
}

bool Bit_eq(struct Buffer buf0, struct Buffer buf1) {
																					
	ocAssert("Buffers cannot be null", buf0.ptr && buf0.siz && buf1.ptr && buf1.siz);
					
	if (buf0.siz != buf1.siz)
		return false;

	usz l = buf0.siz;												
																					
	for (usz i = 0, j = l >> 3; i < j; ++i)
		if (*((const u64*)buf0.ptr + i) != *((const u64*)buf1.ptr + i))
			return false;
																					
	for (usz i = l >> 3 << 3; i < l; ++i)
		if (buf0.ptr[i] != buf1.ptr[i])
			return false;

	return true;
}

bool Bit_neq(struct Buffer buf0, struct Buffer buf1) {
	return !Bit_eq(buf0, buf1);
}

#define BitOpSetRange(x, y)														\
																				\
	ocAssert("Buffer or size cannot be null", dst.ptr && dst.siz && bits);		\
	ocAssert("Buffer out of bounds", ((dstOff + bits - 1) >> 3) < dst.siz);		\
																				\
	usz dstOff8 = (dstOff + 7) >> 3;											\
	usz bitEnd = dstOff + bits;													\
																				\
	/*Bits, begin*/																\
																				\
	for (usz i = dstOff; i < bitEnd && i < dstOff8 << 3; ++i)					\
		dst.ptr[i >> 3] x (1 << (i & 7));										\
																				\
	/*Bytes, until u64 aligned*/												\
																				\
	usz dstOff64 = (dstOff8 + 7) >> 3;											\
	usz bitEnd8 = bitEnd >> 3;													\
																				\
	for (usz i = dstOff8; i < bitEnd8 && i < dstOff64 << 3; ++i)				\
		dst.ptr[i] = (u8) y;													\
																				\
	/*u64 aligned*/																\
																				\
	usz bitEnd64 = bitEnd8 >> 3;												\
																				\
	for(usz i = dstOff64; i < bitEnd64; ++i)									\
		*((u64*)dst.ptr + i) = y;												\
																				\
	/*Bytes unaligned at end*/													\
																				\
	for(usz i = bitEnd64 << 3; i < bitEnd8; ++i)								\
		dst.ptr[i] = (u8) y;													\
																				\
	/*Bits unaligned at end*/													\
																				\
	for (usz i = bitEnd8 << 3; i < bitEnd; ++i)									\
		dst.ptr[i >> 3] x (1 << (i & 7));

void Bit_setRange(struct Buffer dst, usz dstOff, usz bits) {
	BitOpSetRange(|=, u64_MAX);
}

void Bit_unsetRange(struct Buffer dst, usz dstOff, usz bits) {
	BitOpSetRange(&=~, 0);
}

#define BitOpSet(x, dst)																\
																						\
	ocAssert("Buffer cannot be null", dst.ptr && dst.siz);								\
																						\
	usz l = dst.siz;																	\
																						\
	for(usz i = 0, j = l >> 3; i < j; ++i)												\
		*((u64*)dst.ptr + i) = x;														\
																						\
	for (usz i = l >> 3 << 3; i < l; ++i)												\
		dst.ptr[i] = (u8)x;

void Bit_setAll(struct Buffer buf) {
	BitOpSet(u64_MAX, buf);
}

void Bit_unsetAll(struct Buffer buf) {
	BitOpSet(0, buf);
}

void Bit_setAllTo(struct Buffer buf, bool isOn) {

	if (isOn)
		Bit_setAll(buf);

	else Bit_unsetAll(buf);
}

struct Buffer Bit_empty(usz siz, AllocFunc alloc, void *allocator) {

	ocAssert("Allocator or siz cannot be null", alloc && siz);

	siz = (siz + 7) >> 3;	//Align to bytes

	struct Buffer buf = (struct Buffer){ .ptr = alloc(allocator, siz), .siz = siz };
	Bit_unsetAll(buf);

	return buf;
}

struct Buffer Bit_full(usz siz, AllocFunc alloc, void *allocator) {

	ocAssert("Allocator or siz cannot be null", alloc && siz);

	siz = (siz + 7) >> 3;	//Align to bytes

	struct Buffer buf = (struct Buffer){ .ptr = alloc(allocator, siz), .siz = siz };
	Bit_setAll(buf);

	return buf;
}

struct Buffer Bit_duplicate(struct Buffer buf, AllocFunc alloc, void *allocator) {

	ocAssert("Allocator or buf cannot be null", alloc && buf.ptr && buf.siz);

	struct Buffer cpy = (struct Buffer){ .ptr = alloc(allocator, buf.siz), .siz = buf.siz };
	BitOp(=, cpy, buf);
	return cpy;
}

struct Buffer Bit_fill(usz siz, bool value, AllocFunc alloc, void *allocator) {

	if (!value)
		return Bit_empty(siz, alloc, allocator);

	return Bit_full(siz, alloc, allocator);
}

void Bit_free(struct Buffer *buf, FreeFunc freeFunc, void *allocator) {

	ocAssert("Buffer, data or alloc cannot be null", buf && buf->ptr && buf->siz && freeFunc);

	freeFunc(allocator, *buf);
	*buf = (struct Buffer){ 0 };
}

struct Buffer Bit_emptyBytes(usz siz, AllocFunc alloc, void *allocator) {
	return Bit_empty(siz << 3, alloc, allocator);
}

struct Buffer Bit_bytes(usz siz, AllocFunc alloc, void *allocator) {
	ocAssert("Allocator or siz cannot be null", alloc && siz);
	return (struct Buffer){ .ptr = alloc(allocator, siz), .siz = siz };
}

void Bit_offset(struct Buffer *buf, usz siz) {
	ocAssert("Buffer, data or alloc cannot be null", buf && buf->ptr && buf->siz >= siz);
	buf->ptr += siz;
	buf->siz -= siz;
}

void Bit_appendU32(struct Buffer *buf, u32 v) {
	ocAssert("Buffer, data or alloc cannot be null", buf && buf->ptr && buf->siz >= 4);
	*((u32*)buf->ptr) = v;
	Bit_offset(buf, 4);
}

void Bit_copyBytes(u8 *ptr, const void *v, usz siz) {

	for (usz i = 0, j = siz >> 3; i < j; ++i)
		*((u64*)ptr + i) = *((const u64*)v + i);

	for (usz i = siz & ~7; i < siz; ++i)
		ptr[i] = *((const u8*)v + i);

}

void Bit_append(struct Buffer *buf, const void *v, usz siz) {

	ocAssert(
		"Buffer, data or alloc cannot be null",
		buf && buf->ptr && buf->siz >= siz && v && siz
	);

	Bit_copyBytes(buf->ptr, v, siz);
	Bit_offset(buf, siz);
}

void Bit_appendBuffer(struct Buffer *buf, struct Buffer append) {

	ocAssert(
		"Buffer, data or alloc cannot be null",
		buf && buf->ptr && buf->siz >= append.siz && 
		append.ptr && append.siz
	);

	Bit_copyBytes(buf->ptr, append.ptr, append.siz);
	Bit_offset(buf, append.siz);
}

struct Buffer Bit_subset(struct Buffer buf, usz offset, usz siz) {

	Bit_offset(&buf, offset);

	ocAssert("Buffer siz needs to be geq subset size", buf.siz >= siz);

	buf.siz = siz;
	return buf;
}