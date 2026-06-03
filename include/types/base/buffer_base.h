/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//types/base/buffer_base.h

#pragma once
#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/base/c8.h"
#include "types/base/algorithm.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Buffer {

	union {
		const U8 *ptr;
		U8 *ptrNonConst;        //Requires !Buffer_isConstRef(buf)
	};

	U64 lengthAndRefBits;        //refBits: [ b63 isRef, b62 isConst ]. Length should be max 48 bits

} Buffer;

//Buffer (more functions in types/container/buffer.h)

static inline U64 Buffer_length(const Buffer buf) {
	return buf.lengthAndRefBits << 2 >> 2;
}

static inline Bool Buffer_isRef(const Buffer buf) {
	return buf.lengthAndRefBits >> 62;
}

static inline Bool Buffer_isConstRef(const Buffer buf) {
	return (buf.lengthAndRefBits >> 62) == 3;
}

static inline Buffer Buffer_createNull() {
	Buffer buf = { 0 };
	return buf;
}

static inline Buffer Buffer_createManagedPtr(void *ptr, U64 length) {

	if (length >> 48 || !ptr || !length)
		return Buffer_createNull();

	Buffer buf;
	buf.ptrNonConst = (U8*)ptr;
	buf.lengthAndRefBits = length;
	return buf;
}

//These should never be Buffer_free-d because Buffer doesn't know if it's allocated

static inline Buffer Buffer_createRef(void *v, U64 length) {

	if (!length || !v)
		return Buffer_createNull();

	if (length >> 48)
		return Buffer_createNull();

	Buffer buf;
	buf.ptrNonConst = (U8*)v;
	buf.lengthAndRefBits = length | ((U64)1 << 63);
	return buf;
}

static inline Buffer Buffer_createRefConst(const void *v, U64 length) {

	if (!length || !v)
		return Buffer_createNull();

	if (length >> 48)
		return Buffer_createNull();

	Buffer buf;
	buf.ptr = (const U8*)v;
	buf.lengthAndRefBits = length | ((U64)3 << 62);
	return buf;
}

//Bit manipulation

Bool Buffer_memcpy(const Buffer dst, const Buffer src);            //Copies bytes from two separate ranges
Bool Buffer_memmove(const Buffer dst, const Buffer src);        //Copies bytes from two overlapping ranges

Bool Buffer_setAllToU8(const Buffer buf, U8 b8, Error *e_rr);
void Buffer_clearAllSecure(Buffer buf);                            //Clear all without allowing optimization (sensitive data)

static inline Bool Buffer_setAllBits(const Buffer dst, Error *e_rr) {
	return Buffer_setAllToU8(dst, U8_MAX, e_rr);
}

static inline Bool Buffer_unsetAllBits(const Buffer dst, Error *e_rr) {
	return Buffer_setAllToU8(dst, 0, e_rr);
}

static inline Bool Buffer_setAllBitsTo(const Buffer buf, Bool isOn, Error *e_rr) {
	return isOn ? Buffer_setAllBits(buf, e_rr) : Buffer_unsetAllBits(buf, e_rr);
}

Bool Buffer_getBit(const Buffer buf, U64 offset, Bool *output, Error *e_rr);
Bool Buffer_setBit(Buffer buf, U64 offset, Error *e_rr);
Bool Buffer_resetBit(Buffer buf, U64 offset, Error *e_rr);

static inline Bool Buffer_setBitTo(const Buffer buf, U64 offset, Bool value, Error *e_rr) {
	return !value ? Buffer_resetBit(buf, offset, e_rr) : Buffer_setBit(buf, offset, e_rr);
}

Bool Buffer_bitwiseOr(const Buffer dst, Buffer src, Error *e_rr);
Bool Buffer_bitwiseAnd(const Buffer dst, const Buffer src, Error *e_rr);
Bool Buffer_bitwiseXor(const Buffer dst, const Buffer src, Error *e_rr);
Bool Buffer_bitwiseNot(const Buffer dst, Error *e_rr);

Bool Buffer_setBitRange(const Buffer dst, U64 dstOff, U64 bits, Error *e_rr);
Bool Buffer_unsetBitRange(const Buffer dst, U64 dstOff, U64 bits, Error *e_rr);

//Comparison

ECompareResult Buffer_cmp(const Buffer buf0, const Buffer buf1);
static inline Bool Buffer_eq(const Buffer buf0, const Buffer buf1) { return Buffer_cmp(buf0, buf1) == ECompareResult_Eq; }
static inline Bool Buffer_neq(const Buffer buf0, const Buffer buf1) { return !Buffer_eq(buf0, buf1); }

//Use this instead of simply copying the Buffer to a new location
//A copy like this is only fine if the other doesn't get freed.
//In all other cases, createRefFromBuffer should be called on the one that shouldn't be freeing.
//If it needs to be refcounted, RefPtr should be used.
static inline Buffer Buffer_createRefFromBuffer(const Buffer buf, Bool isConst) {

	Buffer copy = { 0 };

	if (!buf.ptr || (!isConst && Buffer_isConstRef(buf)))
		return copy;

	copy = buf;
	copy.lengthAndRefBits |= ((U64)1 << 63) | ((U64)isConst << 62);
	return copy;
}

Bool Buffer_offset(Buffer *buf, U64 length, Error *e_rr);
Bool Buffer_appendBuffer(Buffer *buf, const Buffer append, Error *e_rr);

static inline Bool Buffer_append(Buffer *buf, const void *v, U64 length, Error *e_rr) {
	return Buffer_appendBuffer(buf, Buffer_createRefConst(v, length), e_rr);
}

Bool Buffer_consume(Buffer *buf, void *v, U64 length, Error *e_rr);

#define BUFFER_OP_IMPL(T)                                                                    \
																							\
static inline Bool Buffer_append##T(Buffer *buf, T v, Error *e_rr) {                        \
	return Buffer_append(buf, &v, sizeof(v), e_rr);                                            \
}                                                                                            \
																							\
static inline Bool Buffer_consume##T(Buffer *buf, T *v, Error *e_rr) {                        \
	return Buffer_consume(buf, v, sizeof(*v), e_rr);                                        \
}                                                                                            \
																							\
static inline T Buffer_read##T(Buffer buf, U64 off, Bool *success, Error *e_rr) {            \
																							\
	buf = Buffer_createRefFromBuffer(buf, true);                                            \
	T v = { 0 };                                                                            \
	if(!Buffer_offset(&buf, off, e_rr)) { if(success) *success = false; return v; }            \
																							\
	Bool ok = Buffer_consume##T(&buf, &v, e_rr);                                            \
	if(success) *success = ok;                                                                \
	return v;                                                                                \
}                                                                                            \
																							\
static inline Bool Buffer_write##T(Buffer buf, U64 off, T v, Error *e_rr) {                    \
	buf = Buffer_createRefFromBuffer(buf, false);                                            \
	if(!Buffer_offset(&buf, off, e_rr)) return false;                                        \
	return Buffer_append##T(&buf, v, e_rr);                                                    \
}

BUFFER_OP_IMPL(U64);
BUFFER_OP_IMPL(U32);
BUFFER_OP_IMPL(U16);
BUFFER_OP_IMPL(U8);

BUFFER_OP_IMPL(I64);
BUFFER_OP_IMPL(I32);
BUFFER_OP_IMPL(I16);
BUFFER_OP_IMPL(I8);

BUFFER_OP_IMPL(F64);
BUFFER_OP_IMPL(F32);

static inline Bool Buffer_isAscii(const Buffer buf) {

	for (U64 i = 0; i < Buffer_length(buf); ++i)
		if (!C8_isValidAscii((C8)buf.ptr[i]))
			return false;

	return !!Buffer_length(buf);
}

typedef struct BitRef {
	U8 *ptr;
	U8 off, isConst, padding[6];
} BitRef;

static inline Bool BitRef_get(const BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
static inline void BitRef_set(const BitRef b) { if (b.ptr && !b.isConst) *b.ptr |= 1 << b.off; }
static inline void BitRef_reset(const BitRef b) { if (b.ptr && !b.isConst) *b.ptr &= ~(1 << b.off); }

static inline void BitRef_setTo(const BitRef b, Bool v) {
	if (v) BitRef_set(b);
	else BitRef_reset(b);
}

#ifdef __cplusplus
	}
#endif
