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

//types/base/buffer_base.c

#include "types/base/buffer_base.h"
#include "types/base/error.h"
#include "types/base/constants.h"

#include <string.h>

Bool Buffer_getBit(const Buffer buf, U64 offset, Bool *output, Error *e_rr) {

	Bool s_uccess = true;

	if(!output || !buf.ptr)
		retError(clean, Error_nullPointer(!buf.ptr ? 0 : 2, "Buffer_getBit()::output and buf are required"));

	if((offset >> 3) >= Buffer_length(buf))
		retError(clean, Error_outOfBounds(1, offset >> 3, Buffer_length(buf), "Buffer_getBit()::offset out of bounds"));

	*output = (buf.ptr[offset >> 3] >> (offset & 7)) & 1;

clean:
	return s_uccess;
}

Bool Buffer_memcpy(const Buffer dst, const Buffer src) {

	if(!dst.ptr || !src.ptr)
		return true;

	if(Buffer_isConstRef(dst))
		return false;

	U64 dstLen = Buffer_length(dst);
	U64 srcLen = Buffer_length(src);
	memcpy(dst.ptrNonConst, src.ptr, srcLen <= dstLen ? srcLen : dstLen);
	return true;
}

Bool Buffer_memmove(const Buffer dst, const Buffer src) {

	if(!dst.ptr || !src.ptr)
		return true;

	if(Buffer_isConstRef(dst))
		return false;

	U64 dstLen = Buffer_length(dst);
	U64 srcLen = Buffer_length(src);
	memmove(dst.ptrNonConst, src.ptr, srcLen <= dstLen ? srcLen : dstLen);
	return true;
}

Bool Buffer_setBit(const Buffer buf, U64 offset, Error *e_rr) {

	Bool s_uccess = true;

	if(!buf.ptr)
		retError(clean, Error_nullPointer(0, "Buffer_setBit()::buf is required"));

	if(Buffer_isConstRef(buf))
		retError(clean, Error_constData(0, 0, "Buffer_setBit()::buf should be writable"));

	if((offset >> 3) >= Buffer_length(buf))
		retError(clean, Error_outOfBounds(1, offset, Buffer_length(buf) << 3, "Buffer_setBit()::offset out of bounds"));

	buf.ptrNonConst[offset >> 3] |= 1 << (offset & 7);

clean:
	return s_uccess;
}

Bool Buffer_resetBit(Buffer buf, U64 offset, Error *e_rr) {

	Bool s_uccess = true;

	if(!buf.ptr)
		retError(clean, Error_nullPointer(0, "Buffer_resetBit()::buf is required"));

	if(Buffer_isConstRef(buf))
		retError(clean, Error_constData(0, 0, "Buffer_resetBit()::buf should be writable"));

	if((offset >> 3) >= Buffer_length(buf))
		retError(clean, Error_outOfBounds(1, offset, Buffer_length(buf) << 3, "Buffer_resetBit()::offset out of bounds"));

	buf.ptrNonConst[offset >> 3] &= ~(1 << (offset & 7));

clean:
	return s_uccess;
}

#define BitOp(x, dst, src, ...) {                                                                   \
																									\
	Bool s_uccess = true;                                                                           \
																									\
	if(!dst.ptr || !src.ptr)                                                                        \
		retError(clean, Error_nullPointer(!dst.ptr ? 0 : 1, "BitOp::dst and src are required"));    \
																									\
	if(Buffer_isConstRef(dst))                                                                      \
		retError(clean, Error_constData(0, 0, "BitOp::dst should be writable"));                    \
																									\
	U64 dstLen = Buffer_length(dst), srcLen = Buffer_length(src);                                   \
	U64 l = dstLen <= srcLen ? dstLen : srcLen;                                                     \
																									\
	if(!((U64)dst.ptr & 7) && !((U64)src.ptr & 7)) {                                                \
																									\
		for(U64 i = 0, j = l >> 3; i < j; ++i)                                                      \
			*((U64*)dst.ptrNonConst + i) x *((const U64*)src.ptr + i);                              \
																									\
		for (U64 i = l >> 3 << 3; i < l; ++i)                                                       \
			dst.ptrNonConst[i] x src.ptr[i];                                                        \
	}                                                                                               \
																									\
	else for(U8 i = 0; i < l; ++i)                                                                  \
		dst.ptrNonConst[i] x src.ptr[i];                                                            \
																									\
	__VA_ARGS__                                                                                     \
																									\
clean:                                                                                              \
	return s_uccess;                                                                                \
}

//TODO: Optimize this

Bool Buffer_bitwiseOr(const Buffer dst, const Buffer src, Error *e_rr)  BitOp(|=, dst, src, )
Bool Buffer_bitwiseXor(const Buffer dst, const Buffer src, Error *e_rr) BitOp(^=, dst, src, )
Bool Buffer_bitwiseNot(const Buffer dst, Error *e_rr) BitOp(=~, dst, dst, )
Bool Buffer_bitwiseAnd(const Buffer dst, const Buffer src, Error *e_rr) BitOp(
	&=, dst, src,
	if(dstLen > srcLen)
		gotoIfError3(clean, Buffer_unsetAllBits(Buffer_createRefConst(dst.ptrNonConst + srcLen, dstLen - srcLen), e_rr));
)

ECompareResult Buffer_cmp(const Buffer buf0, const Buffer buf1) {

	if (!buf0.ptr && !buf1.ptr)
		return ECompareResult_Eq;

	const U64 len0 = Buffer_length(buf0);
	const U64 len1 = Buffer_length(buf1);

	if(len0 != len1)
		return len0 < len1 ? ECompareResult_Lt : ECompareResult_Gt;

	int cmp = memcmp(buf0.ptr, buf1.ptr, len0);
	return cmp == 0 ? ECompareResult_Eq : (cmp < 0? ECompareResult_Lt : ECompareResult_Gt);
}

Bool Buffer_offset(Buffer *buf, U64 length, Error *e_rr) {

	Bool s_uccess = true;

	if (!length)
		goto clean;

	if(!buf || !buf->ptr)
		retError(clean, Error_nullPointer(0, "Buffer_offset()::buf and buf->ptr are required"));

	if(!Buffer_isRef(*buf))                                //Ensure we don't accidentally call this and leak memory
		retError(clean, Error_invalidOperation(0, "Buffer_offset() can only be called on a ref"));

	const U64 bufLen = Buffer_length(*buf);

	if(length > bufLen)
		retError(clean, Error_outOfBounds(1, length, bufLen, "Buffer_offset()::length is out of bounds"));

	buf->ptr += length;

	//Maintain constness

	Buffer_setLength(buf, bufLen - length);

	if(!bufLen)
		*buf = Buffer_createNull();

clean:
	return s_uccess;
}

void Buffer_copyBytesInternal(U8 *ptr, const void *v, U64 length) {
	Buffer_memcpy(Buffer_createRef(ptr, length), Buffer_createRefConst(v, length));
}

Bool Buffer_appendBuffer(Buffer *buf, const Buffer append, Error *e_rr) {

	Bool s_uccess = true;

	if(!append.ptr)
		retError(clean, Error_nullPointer(1, "Buffer_appendBuffer()::append is required"));

	if(buf && Buffer_isConstRef(*buf))                    //We need write access
		retError(clean, Error_constData(0, 0, "Buffer_appendBuffer()::buf should be writable"));

	U8 *ptr = buf->ptrNonConst;
	gotoIfError3(clean, Buffer_offset(buf, Buffer_length(append), e_rr));

	Buffer_copyBytesInternal(ptr, append.ptr, Buffer_length(append));

clean:
	return s_uccess;
}

Bool Buffer_consume(Buffer *buf, void *v, U64 length, Error *e_rr) {

	Bool s_uccess = true;

	if(!buf)
		retError(clean, Error_nullPointer(!buf ? 0 : 1, "Buffer_consume()::buf is required"));

	const void *ptr = buf ? buf->ptr : NULL;
	gotoIfError3(clean, Buffer_offset(buf, length, e_rr));

	if(v)
		Buffer_copyBytesInternal(v, ptr, length);

clean:
	return s_uccess;
}

Bool Buffer_setBitRange(const Buffer dst, U64 dstOff, U64 bits, Error *e_rr) {

	Bool s_uccess = true;

	if(!dst.ptr)
		retError(clean, Error_nullPointer(0, "Buffer_setBitRange()::dst is required"));

	if(Buffer_isConstRef(dst))
		retError(clean, Error_constData(0, 0, "Buffer_setBitRange()::dst should be writable"));

	if(!bits)
		retError(clean, Error_invalidParameter(2, 0, "Buffer_setBitRange()::bits should be non zero"));

	if(dstOff + bits > (Buffer_length(dst) << 3))
		retError(clean, Error_outOfBounds(
			1, dstOff + bits, Buffer_length(dst) << 3, "Buffer_setBitRange()::dstOff out of bounds"
		));

	//Split into: leading partial byte, whole bytes in the middle, trailing partial byte.
	//firstFull is the first byte entirely inside the range, lastFull one past the last such byte.
	//When the range doesn't span a byte boundary at all there is no middle, and the two partial writes
	// would overlap, so that case is handled on its own.

	const U64 end = dstOff + bits;
	const U64 firstFull = (dstOff + 7) >> 3;
	const U64 lastFull = end >> 3;

	if (firstFull > lastFull)
		dst.ptrNonConst[dstOff >> 3] |= (U8)(((1U << bits) - 1) << (dstOff & 7));

	else {

		if(dstOff & 7)
			dst.ptrNonConst[dstOff >> 3] |= (U8)(0xFF << (dstOff & 7));

		if(lastFull > firstFull)
			memset(dst.ptrNonConst + firstFull, 0xFF, lastFull - firstFull);

		//The trailing bits start at bit 0 of lastFull, so this mask isn't shifted

		if(end & 7)
			dst.ptrNonConst[lastFull] |= (U8)((1U << (end & 7)) - 1);
	}

clean:
	return s_uccess;
}

Bool Buffer_unsetBitRange(const Buffer dst, U64 dstOff, U64 bits, Error *e_rr) {

	Bool s_uccess = true;

	if(!dst.ptr)
		retError(clean, Error_nullPointer(0, "Buffer_unsetBitRange()::dst is required"));

	if(Buffer_isConstRef(dst))
		retError(clean, Error_constData(0, 0, "Buffer_unsetBitRange()::dst should be writable"));

	if(!bits)
		retError(clean, Error_invalidParameter(2, 0, "Buffer_unsetBitRange()::bits should be non zero"));

	if(dstOff + bits > (Buffer_length(dst) << 3))
		retError(clean, Error_outOfBounds(
			1, dstOff + bits, Buffer_length(dst) << 3, "Buffer_unsetBitRange()::dstOff out of bounds"
		));

	//Same split as Buffer_setBitRange, see the comment there

	const U64 end = dstOff + bits;
	const U64 firstFull = (dstOff + 7) >> 3;
	const U64 lastFull = end >> 3;

	if (firstFull > lastFull)
		dst.ptrNonConst[dstOff >> 3] &=~ (U8)(((1U << bits) - 1) << (dstOff & 7));

	else {

		if(dstOff & 7)
			dst.ptrNonConst[dstOff >> 3] &=~ (U8)(0xFF << (dstOff & 7));

		if(lastFull > firstFull)
			memset(dst.ptrNonConst + firstFull, 0x00, lastFull - firstFull);

		if(end & 7)
			dst.ptrNonConst[lastFull] &=~ (U8)((1U << (end & 7)) - 1);
	}

clean:
	return s_uccess;
}

Bool Buffer_setAllToU8(const Buffer buf, U8 b8, Error *e_rr) {

	Bool s_uccess = true;

	if(!buf.ptr)
		retError(clean, Error_nullPointer(0, "Buffer_setAllTo()::buf is required"));

	if(Buffer_isConstRef(buf))
		retError(clean, Error_constData(0, 0, "Buffer_setAllTo()::buf should be writable"));

	memset(buf.ptrNonConst, b8, Buffer_length(buf));

clean:
	return s_uccess;
}

static void *(*volatile memset_secure)(void *, int, size_t) = memset;

void Buffer_clearAllSecure(Buffer buf) {
	if(buf.ptr && !Buffer_isConstRef(buf))
		memset_secure(buf.ptrNonConst, 0, Buffer_length(buf));
}
