/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/math.h"
#include "types/vec.h"

Bool BitRef_get(BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
void BitRef_set(BitRef b) { if(b.ptr && !b.isConst) *b.ptr |= 1 << b.off; }
void BitRef_reset(BitRef b) { if(b.ptr && !b.isConst) *b.ptr &= ~(1 << b.off); }

void BitRef_setTo(BitRef b, Bool v) {
	if(v) BitRef_set(b);
	else BitRef_reset(b);
}

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output) {

	if(!output || !buf.ptr)
		return Error_nullPointer(!buf.ptr ? 0 : 2, "Buffer_getBit()::output and buf are required");

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, offset >> 3, Buffer_length(buf), "Buffer_getBit()::offset out of bounds");

	*output = (buf.ptr[offset >> 3] >> (offset & 7)) & 1;
	return Error_none();
}

Error Buffer_setBitTo(Buffer buf, U64 offset, Bool value) {
	return !value ? Buffer_resetBit(buf, offset) : Buffer_setBit(buf, offset);
}

//Copy forwards

Bool Buffer_copy(Buffer dst, Buffer src) {

	if(!dst.ptr || !src.ptr)
		return true;

	if(Buffer_isConstRef(dst))
		return false;

	const U64 dstLen = Buffer_length(dst), srcLen = Buffer_length(src);

	U64 *dstPtr = (U64*)dst.ptr, *dstEnd = dstPtr + (dstLen >> 3);
	const U64 *srcPtr = (const U64*)src.ptr, *srcEnd = srcPtr + (srcLen >> 3);

	for(; dstPtr < dstEnd && srcPtr < srcEnd; ++dstPtr, ++srcPtr)
		*dstPtr = *srcPtr;

	dstEnd = (U64*)(dst.ptr + dstLen);
	srcEnd = (const U64*)(src.ptr + srcLen);

	if((U64)dstPtr + 4 <= (U64)dstEnd && (U64)srcPtr + 4 <= (U64)srcEnd) {

		*(U32*)dstPtr = *(const U32*)srcPtr;

		dstPtr = (U64*)((U32*)dstPtr + 1);
		srcPtr = (const U64*)((const U32*)srcPtr + 1);
	}

	if ((U64)dstPtr + 2 <= (U64)dstEnd && (U64)srcPtr + 2 <= (U64)srcEnd) {

		*(U16*)dstPtr = *(const U16*)srcPtr;

		dstPtr = (U64*)((U16*)dstPtr + 1);
		srcPtr = (const U64*)((const U16*)srcPtr + 1);
	}

	if ((U64)dstPtr + 1 <= (U64)dstEnd && (U64)srcPtr + 1 <= (U64)srcEnd)
		*(U8*)dstPtr = *(const U8*)srcPtr;

	return true;
}

//Copy backwards; if ranges are overlapping this might be important

Bool Buffer_revCopy(Buffer dst, Buffer src) {

	if(!dst.ptr || !src.ptr)
		return true;

	if(Buffer_isConstRef(dst))
		return false;

	const U64 dstLen = Buffer_length(dst), srcLen = Buffer_length(src);

	U64 *dstPtr = (U64*)(dst.ptr + dstLen), *dstBeg = dstPtr - (dstLen >> 3);
	const U64 *srcPtr = (const U64*)(src.ptr + srcLen), *srcBeg = srcPtr - (srcLen >> 3);

	while(dstPtr > dstBeg && srcPtr > srcBeg) {
		--srcPtr; --dstPtr;
		*dstPtr = *srcPtr;
	}

	if((I64)dstPtr - 4 >= (I64)dst.ptr && (I64)srcPtr - 4 >= (I64)src.ptr ) {

		dstPtr = (U64*)((U32*)dstPtr - 1);
		srcPtr = (const U64*)((const U32*)srcPtr - 1);

		*(U32*)dstPtr = *(const U32*)srcPtr;
	}

	if ((I64)dstPtr - 2 >= (I64)dst.ptr && (I64)srcPtr - 2 >= (I64)src.ptr) {

		dstPtr = (U64*)((U16*)dstPtr - 1);
		srcPtr = (const U64*)((const U16*)srcPtr - 1);

		*(U16*)dstPtr = *(const U16*)srcPtr;
	}

	if ((I64)dstPtr - 1 >= (I64)dst.ptr && (I64)srcPtr - 1 >= (I64)src.ptr)
		*((U8*)dstPtr - 1) = *((const U8*)srcPtr - 1);

	return true;
}

//

Error Buffer_setBit(Buffer buf, U64 offset) {

	if(!buf.ptr)
		return Error_nullPointer(0, "Buffer_setBit()::buf is required");

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_setBit()::buf should be writable");

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, offset, Buffer_length(buf) << 3, "Buffer_setBit()::offset out of bounds");

	((U8*)buf.ptr)[offset >> 3] |= 1 << (offset & 7);
	return Error_none();
}

Error Buffer_resetBit(Buffer buf, U64 offset) {

	if(!buf.ptr)
		return Error_nullPointer(0, "Buffer_resetBit()::buf is required");

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_resetBit()::buf should be writable");

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, offset, Buffer_length(buf) << 3, "Buffer_resetBit()::offset out of bounds");

	((U8*)buf.ptr)[offset >> 3] &= ~(1 << (offset & 7));
	return Error_none();
}

#define BitOp(x, dst, src) {															\
																						\
	if(!dst.ptr || !src.ptr)															\
		return Error_nullPointer(!dst.ptr ? 0 : 1, "BitOp::dst and src are required");	\
																						\
	if(Buffer_isConstRef(dst))															\
		return Error_constData(0, 0, "BitOp::dst should be writable");					\
																						\
	U64 l = U64_min(Buffer_length(dst), Buffer_length(src));							\
																						\
	for(U64 i = 0, j = l >> 3; i < j; ++i)												\
		*((U64*)dst.ptr + i) x *((const U64*)src.ptr + i);								\
																						\
	for (U64 i = l >> 3 << 3; i < l; ++i)												\
		((U8*)dst.ptr)[i] x src.ptr[i];													\
																						\
	return Error_none();																\
}

Error Buffer_bitwiseOr(Buffer dst, Buffer src)  BitOp(|=, dst, src)
Error Buffer_bitwiseXor(Buffer dst, Buffer src) BitOp(^=, dst, src)
Error Buffer_bitwiseAnd(Buffer dst, Buffer src) BitOp(&=, dst, src)
Error Buffer_bitwiseNot(Buffer dst) BitOp(=~, dst, dst)

Error Buffer_setAllBitsTo(Buffer buf, Bool isOn) {
	return isOn ? Buffer_setAllBits(buf) : Buffer_unsetAllBits(buf);
}

Buffer Buffer_createNull() { return (Buffer) { 0 }; }

Buffer Buffer_createRef(void *v, U64 length) {

	if(!length || !v)
		return Buffer_createNull();

	if(length >> 48 || (U64)v >> 48)		//Invalid addresses (unsupported by CPUs)
		return Buffer_createNull();

	return (Buffer) { .ptr = (U8*) v, .lengthAndRefBits = length | ((U64)1 << 63) };
}

Buffer Buffer_createRefConst(const void *v, U64 length) {

	if(!length || !v)
		return Buffer_createNull();

	if(length >> 48 || (U64)v >> 48)		//Invalid addresses (unsupported by CPUs)
		return Buffer_createNull();

	return (Buffer) { .ptr = (U8*) v, .lengthAndRefBits = length | ((U64)3 << 62) };
}

Bool Buffer_eq(Buffer buf0, Buffer buf1) {

	if (!buf0.ptr && !buf1.ptr)
		return true;

	if (!buf0.ptr || !buf1.ptr)
		return false;

	const U64 len0 = Buffer_length(buf0);

	if(len0 != Buffer_length(buf1))
		return false;

	for (U64 i = 0, j = len0 >> 3; i < j; ++i)
		if (((const U64*)buf0.ptr)[i] != ((const U64*)buf1.ptr)[i])
			return false;

	for (U64 i = len0 >> 3 << 3; i < len0; ++i)
		if (buf0.ptr[i] != buf1.ptr[i])
			return false;

	return true;
}

Bool Buffer_neq(Buffer buf0, Buffer buf1) { return !Buffer_eq(buf0, buf1); }

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return Error_nullPointer(0, "Buffer_setBitRange()::dst is required");

	if(Buffer_isConstRef(dst))
		return Error_constData(0, 0, "Buffer_setBitRange()::dst should be writable");

	if(!bits)
		return Error_invalidParameter(2, 0, "Buffer_setBitRange()::bits should be non zero");

	if(dstOff + bits > (Buffer_length(dst) << 3))
		return Error_outOfBounds(1, dstOff + bits, Buffer_length(dst) << 3, "Buffer_setBitRange()::dstOff out of bounds");

	const U64 dstOff8 = (dstOff + 7) >> 3;
	const U64 bitEnd = dstOff + bits;

	//Bits, begin

	for (U64 i = dstOff; i < bitEnd && i < dstOff8 << 3; ++i)
		((U8*)dst.ptr)[i >> 3] |= (1 << (i & 7));

	//Bytes, until U64 aligned

	const U64 dstOff64 = (dstOff8 + 7) >> 3;
	const U64 bitEnd8 = bitEnd >> 3;

	for (U64 i = dstOff8; i < bitEnd8 && i < dstOff64 << 3; ++i)
		((U8*)dst.ptr)[i] = U8_MAX;

	//U64 aligned

	const U64 bitEnd64 = bitEnd8 >> 3;

	for(U64 i = dstOff64; i < bitEnd64; ++i)
		*((U64*)dst.ptr + i) = U64_MAX;

	//Bytes unaligned at end

	for(U64 i = bitEnd64 << 3; i < bitEnd8; ++i)
		((U8*)dst.ptr)[i] = U8_MAX;

	//Bits unaligned at end

	for (U64 i = bitEnd8 << 3; i < bitEnd; ++i)
		((U8*)dst.ptr)[i >> 3] |= (1 << (i & 7));

	return Error_none();
}

Error Buffer_unsetBitRange(Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return Error_nullPointer(0, "Buffer_unsetBitRange()::dst is required");

	if(Buffer_isConstRef(dst))
		return Error_constData(0, 0, "Buffer_unsetBitRange()::dst should be writable");

	if(!bits)
		return Error_invalidParameter(2, 0, "Buffer_unsetBitRange()::bits should be non zero");

	if(dstOff + bits > (Buffer_length(dst) << 3))
		return Error_outOfBounds(1, dstOff + bits, Buffer_length(dst) << 3, "Buffer_unsetBitRange()::dstOff out of bounds");

	const U64 dstOff8 = (dstOff + 7) >> 3;
	const U64 bitEnd = dstOff + bits;

	//Bits, begin

	for (U64 i = dstOff; i < bitEnd && i < dstOff8 << 3; ++i)
		((U8*)dst.ptr)[i >> 3] &=~ (1 << (i & 7));

	//Bytes, until U64 aligned

	const U64 dstOff64 = (dstOff8 + 7) >> 3;
	const U64 bitEnd8 = bitEnd >> 3;

	for (U64 i = dstOff8; i < bitEnd8 && i < dstOff64 << 3; ++i)
		((U8*)dst.ptr)[i] = 0;

	//U64 aligned

	const U64 bitEnd64 = bitEnd8 >> 3;

	for(U64 i = dstOff64; i < bitEnd64; ++i)
		*((U64*)dst.ptr + i) = 0;

	//Bytes unaligned at end

	for(U64 i = bitEnd64 << 3; i < bitEnd8; ++i)
		((U8*)dst.ptr)[i] = 0;

	//Bits unaligned at end

	for (U64 i = bitEnd8 << 3; i < bitEnd; ++i)
		((U8*)dst.ptr)[i >> 3] &=~ (1 << (i & 7));

	return Error_none();
}

Error Buffer_setAllToInternal(Buffer buf, U64 b64, U8 b8) {

	if(!buf.ptr)
		return Error_nullPointer(0, "Buffer_setAllToInternal()::buf is required");

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_setAllToInternal()::buf should be writable");

	const U64 l = Buffer_length(buf);

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)buf.ptr + i) = b64;

	for (U64 i = l >> 3 << 3; i < l; ++i)
		((U8*)buf.ptr)[i] = b8;

	return Error_none();
}

Error Buffer_createBits(U64 length, Bool value, Allocator alloc, Buffer *result) {
	return !value ? Buffer_createZeroBits(length, alloc, result) : Buffer_createOneBits(length, alloc, result);
}

Error Buffer_setAllBits(Buffer dst) {
	return Buffer_setAllToInternal(dst, U64_MAX, U8_MAX);
}

Error Buffer_unsetAllBits(Buffer dst) {
	return Buffer_setAllToInternal(dst, 0, 0);
}

Error Buffer_allocBitsInternal(U64 length, Allocator alloc, Buffer *result) {

	if(!length)
		return Error_invalidParameter(0, 0, "Buffer_allocBitsInternal()::length should be non zero");

	if(!alloc.alloc)
		return Error_nullPointer(1, "Buffer_allocBitsInternal()::alloc should have allocator");

	if(!result)
		return Error_nullPointer(0, "Buffer_allocBitsInternal()::result is required");

	if(result->ptr)
		return Error_invalidParameter(2, 0, "Buffer_allocBitsInternal()::result->ptr is non zero, could indicate memleak");

	length = (length + 7) >> 3;	//Align to bytes
	return alloc.alloc(alloc.ptr, length, result);
}

Error Buffer_createZeroBits(U64 length, Allocator alloc, Buffer *result) {

	Error e = Buffer_allocBitsInternal(length, alloc, result);

	if(e.genericError)
		return e;

	e = Buffer_unsetAllBits(*result);

	if(e.genericError)
		Buffer_free(result, alloc);

	return e;
}

Error Buffer_createOneBits(U64 length, Allocator alloc, Buffer *result) {

	Error e = Buffer_allocBitsInternal(length, alloc, result);

	if(e.genericError)
		return e;

	e = Buffer_setAllBits(*result);

	if(e.genericError)
		Buffer_free(result, alloc);

	return e;
}

Error Buffer_createCopy(Buffer buf, Allocator alloc, Buffer *result) {

	if (result && result->ptr)
		return Error_invalidOperation(0, "Buffer_createCopy()::result is non zero, could indicate memleak");

	if(!Buffer_length(buf)) {

		if(result)
			*result = Buffer_createNull();

		return Error_none();
	}

	const U64 l = Buffer_length(buf);
	const Error e = Buffer_allocBitsInternal(l << 3, alloc, result);

	if(e.genericError)
		return e;

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)result->ptr + i) = *((const U64*)buf.ptr + i);

	for (U64 i = l >> 3 << 3; i < l; ++i)
		((U8*)result->ptr)[i] = buf.ptr[i];

	return Error_none();
}

Bool Buffer_free(Buffer *buf, Allocator alloc) {

	if(!buf || !buf->ptr)
		return true;

	//References SHOULD NEVER be freed through the allocator.
	//We aren't the ones managing them

	if (Buffer_isRef(*buf)) {
		*buf = Buffer_createNull();
		return true;
	}

	//Otherwise we should free

	if(!alloc.free)
		return false;

	const Bool success = alloc.free(alloc.ptr, *buf);
	*buf = Buffer_createNull();
	return success;
}

Error Buffer_createEmptyBytes(U64 length, Allocator alloc, Buffer *output) {
	return Buffer_createZeroBits(length << 3, alloc, output);
}

Error Buffer_createUninitializedBytes(U64 length, Allocator alloc, Buffer *result) {
	return Buffer_allocBitsInternal(length << 3, alloc, result);
}

Error Buffer_offset(Buffer *buf, U64 length) {

	if(!buf || !buf->ptr)
		return Error_nullPointer(0, "Buffer_offset()::buf and buf->ptr are required");

	if(!Buffer_isRef(*buf))								//Ensure we don't accidentally call this and leak memory
		return Error_invalidOperation(0, "Buffer_offset() can only be called on a ref");

	const U64 bufLen = Buffer_length(*buf);

	if(length > bufLen)
		return Error_outOfBounds(1, length, bufLen, "Buffer_offset()::length is out of bounds");

	buf->ptr += length;

	//Maintain constness

	buf->lengthAndRefBits = (bufLen - length) | (buf->lengthAndRefBits >> 62 << 62);

	if(!bufLen)
		*buf = Buffer_createNull();

	return Error_none();
}

void Buffer_copyBytesInternal(U8 *ptr, const void *v, U64 length) {

	for (U64 i = 0, j = length >> 3; i < j; ++i)
		*((U64*)ptr + i) = *((const U64*)v + i);

	for (U64 i = length >> 3 << 3; i < length; ++i)
		ptr[i] = *((const U8*)v + i);
}

Error Buffer_appendBuffer(Buffer *buf, Buffer append) {

	if(!append.ptr)
		return Error_nullPointer(1, "Buffer_appendBuffer()::append is required");

	if(buf && Buffer_isConstRef(*buf))					//We need write access
		return Error_constData(0, 0, "Buffer_appendBuffer()::buf should be writable");

	void *ptr = buf ? (U8*)buf->ptr : NULL;

	const Error e = Buffer_offset(buf, Buffer_length(append));

	if(e.genericError)
		return e;

	Buffer_copyBytesInternal(ptr, append.ptr, Buffer_length(append));
	return Error_none();
}

Error Buffer_append(Buffer *buf, const void *v, U64 length) {
	return Buffer_appendBuffer(buf, Buffer_createRefConst(v, length));
}

Error Buffer_consume(Buffer *buf, void *v, U64 length) {

	if(!buf)
		return Error_nullPointer(!buf ? 0 : 1, "Buffer_consume()::buf is required");

	const void *ptr = buf ? buf->ptr : NULL;

	const Error e = Buffer_offset(buf, length);

	if(e.genericError)
		return e;

	if(v)
		Buffer_copyBytesInternal(v, ptr, length);

	return Error_none();
}

Error Buffer_createSubset(
	Buffer buf,
	U64 offset,
	U64 length,
	Bool isConst,
	Buffer *output
) {

	if (!output)
		return Error_nullPointer(4, "Buffer_createSubset()::output is required");

	if (output && output->ptr)
		return Error_invalidOperation(0, "Buffer_createSubset()::output->ptr is non zero, could indicate memleak");

	if(Buffer_isConstRef(buf) && !isConst)
		return Error_constData(0, 0, "Buffer_createSubset() tried creating non const subset from const data");

	//Since our buffer was passed here, it's safe to make a ref (to ensure Buffer_offset doesn't fail)

	buf = Buffer_createRefFromBuffer(buf, isConst);
	const Error e = Buffer_offset(&buf, offset);

	if(e.genericError)
		return e;

	if(length > Buffer_length(buf))
		return Error_outOfBounds(2, length, Buffer_length(buf), "Buffer_createSubset()::length out of bounds");

	buf.lengthAndRefBits = length | (buf.lengthAndRefBits >> 62 << 62);
	*output = buf;
	return Error_none();
}

Error Buffer_combine(Buffer a, Buffer b, Allocator alloc, Buffer *output) {

	const U64 alen = Buffer_length(a), blen = Buffer_length(b);

	if(alen + blen < alen)
		return Error_overflow(0, alen + blen, alen, "Buffer_combine() overflow");

	const Error err = Buffer_createUninitializedBytes(alen + blen, alloc, output);

	if(err.genericError)
		return err;

	Buffer_copy(*output, a);
	Buffer_copy(Buffer_createRef((U8*)output->ptr + alen, blen), b);
	return Error_none();
}

#define BUFFER_IMPL(T)																		\
Error Buffer_append##T(Buffer *buf, T v)	{ return Buffer_append(buf, &v, sizeof(v)); }	\
Error Buffer_consume##T(Buffer *buf, T *v)	{ return Buffer_consume(buf, v, sizeof(*v)); }

BUFFER_IMPL(U64);
BUFFER_IMPL(U32);
BUFFER_IMPL(U16);
BUFFER_IMPL(U8);

BUFFER_IMPL(I64);
BUFFER_IMPL(I32);
BUFFER_IMPL(I16);
BUFFER_IMPL(I8);

BUFFER_IMPL(F64);
BUFFER_IMPL(F32);

BUFFER_IMPL(I32x2);
BUFFER_IMPL(F32x2);
//BUFFER_IMPL(F64x2);		TODO:

BUFFER_IMPL(I32x4);
BUFFER_IMPL(F32x4);
//BUFFER_IMPL(F64x4);		TODO:

//UTF-8
//https://en.wikipedia.org/wiki/UTF-8

Error Buffer_readAsUtf8(Buffer buf, U64 i, UnicodeCodePointInfo *codepoint) {

	if(!codepoint)
		return Error_nullPointer(3, "Buffer_readAsUtf8()::codepoint is required");

	if(i >= U64_MAX - 4 || i + 1 > Buffer_length(buf))
		return Error_outOfBounds(0, i, Buffer_length(buf), "Buffer_readAsUtf8() out of bounds");

	//Ascii

	const U8 v0 = buf.ptr[i++];

	if (!(v0 >> 7)) {

		if(!C8_isValidAscii((C8)v0))
			return Error_invalidParameter(0, 0, "Buffer_readAsUtf8()::buf[i] didn't contain valid ascii");

		*codepoint = (UnicodeCodePointInfo) { .chars = 1, .bytes = 1, .index = v0 };
		return Error_none();
	}

	if(v0 < 0xC0)
		return Error_invalidParameter(0, 1, "Buffer_readAsUtf8()::buf[i] didn't contain valid UTF8");

	//2-4 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(1, i, Buffer_length(buf), "Buffer_readAsUtf8()::buf UTF8 ended unexpectedly");

	const U8 v1 = buf.ptr[i++];

	if(v1 < 0x80 || v1 >= 0xC0)
		return Error_invalidParameter(0, 2, "Buffer_readAsUtf8()::buf[i + 1] had invalid encoding");

	//2 bytes

	if (v0 < 0xE0) {
		*codepoint = (UnicodeCodePointInfo) { .chars = 2, .bytes = 2, .index = (((U16)v0 & 0x1F) << 6) | (v1 & 0x3F) };
		return Error_none();
	}

	//3 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(2, i, Buffer_length(buf), "Buffer_readAsUtf8()::buf UTF8 ended unexpectedly");

	const U8 v2 = buf.ptr[i++];

	if(v2 < 0x80 || v2 >= 0xC0)
		return Error_invalidParameter(0, 3, "Buffer_readAsUtf8()::buf[i + 2] had invalid encoding");

	if (v0 < 0xF0) {

		*codepoint = (UnicodeCodePointInfo) {
			.chars = 3, .bytes = 3, .index = (((U32)v0 & 0xF) << 12) | (((U32)v1 & 0x3F) << 6) | (v2 & 0x3F)
		};

		return Error_none();
	}

	//4 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(3, i, Buffer_length(buf), "Buffer_readAsUtf8()::buf UTF8 ended unexpectedly");

	const U8 v3 = buf.ptr[i++];

	if(v3 < 0x80 || v3 >= 0xC0)
		return Error_invalidParameter(0, 4, "Buffer_readAsUtf8()::buf[i + 3] had invalid encoding");

	*codepoint = (UnicodeCodePointInfo) {
		.chars = 4,
		.bytes = 4,
		.index = (((U32)v0 & 0x7) << 18) | (((U32)v1 & 0x3F) << 12) | (((U32)v2 & 0x3F) << 6) | (v3 & 0x3F)
	};

	return Error_none();
}

Error Buffer_writeAsUtf8(Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes) {

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_writeAsUtf8()::buf should be writable");

	if ((codepoint & 0x7F) == codepoint) {

		if(i + 1 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 1, Buffer_length(buf), "Buffer_writeAsUtf8()::i out of bounds");

		if(bytes)
			*bytes = 1;

		((U8*)buf.ptr)[i] = (U8) codepoint;
		return Error_none();
	}

	if ((codepoint & 0x7FF) == codepoint) {

		if(i + 2 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 2, Buffer_length(buf), "Buffer_writeAsUtf8()::i + 1 out of bounds");

		if(bytes)
			*bytes = 2;

		((U8*)buf.ptr)[i]		= 0xC0 | (U8)(codepoint >> 6);
		((U8*)buf.ptr)[i + 1]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	if ((codepoint & 0xFFFF) == codepoint) {

		if(i + 3 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 3, Buffer_length(buf), "Buffer_writeAsUtf8()::i + 2 out of bounds");

		if(bytes)
			*bytes = 3;

		((U8*)buf.ptr)[i]		= 0xE0 | (U8)(codepoint >> 12);
		((U8*)buf.ptr)[i + 1]	= 0x80 | (U8)((codepoint >> 6) & 0x3F);
		((U8*)buf.ptr)[i + 2]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	if (codepoint <= 0x10FFFF) {

		if(i + 4 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 4, Buffer_length(buf), "Buffer_writeAsUtf8()::i + 3 out of bounds");

		if(bytes)
			*bytes = 4;

		((U8*)buf.ptr)[i]		= 0xF0 | (U8)(codepoint >> 18);
		((U8*)buf.ptr)[i + 1]	= 0x80 | (U8)((codepoint >> 12) & 0x3F);
		((U8*)buf.ptr)[i + 2]	= 0x80 | (U8)((codepoint >> 6) & 0x3F);
		((U8*)buf.ptr)[i + 3]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	return Error_invalidParameter(2, 0, "Buffer_writeAsUtf8()::codepoint out of bounds (>0x10FFFF)");
}

//https://en.wikipedia.org/wiki/UTF-16

Error Buffer_readAsUtf16(Buffer buf, U64 i, UnicodeCodePointInfo *codepoint) {

	if(!codepoint)
		return Error_nullPointer(3, "Buffer_readAsUtf16()::codepoint is required");

	if(i >= U64_MAX - 2 || i + 2 > Buffer_length(buf))
		return Error_outOfBounds(0, i, Buffer_length(buf), "Buffer_readAsUtf16() out of bounds");

	//1 U16

	const U16 v0 = *(const U16*)(buf.ptr + i);
	i += 2;

	if (v0 <= 0xD7FF) {

		if(v0 <= 0x7F && !C8_isValidAscii((C8)v0))
			return Error_invalidParameter(0, 0, "Buffer_readAsUtf16()::buf[i] didn't contain valid ascii");

		*codepoint = (UnicodeCodePointInfo) { .chars = 1, .bytes = 2, .index = v0 };
		return Error_none();
	}

	if((v0 - 0xD800) >= (1 << 10))
		return Error_invalidParameter(0, 1, "Buffer_readAsUtf16()::buf[i] didn't contain valid UTF16");

	//2 U16s

	if(i + 2 > Buffer_length(buf))
		return Error_outOfBounds(1, i, Buffer_length(buf), "Buffer_readAsUtf16()::buf UTF16 ended unexpectedly");

	const U16 v1 = *(const U16*)(buf.ptr + i);

	if(v1 < 0xDC00 || (v1 - 0xDC00) >= (1 << 10))
		return Error_invalidParameter(0, 2, "Buffer_readAsUtf16()::buf[i + 1] had invalid encoding");

	*codepoint = (UnicodeCodePointInfo) { .chars = 2, .bytes = 4, .index = ((v0 - 0xD800) << 10) | (v1 - 0xDC00) | 0x10000 };
	return Error_none();
}

Error Buffer_writeAsUtf16(Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes) {

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_writeAsUtf16()::buf should be writable");

	if (codepoint <= 0xD7FF) {

		if(i + 2 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 2, Buffer_length(buf), "Buffer_writeAsUtf16()::i out of bounds");

		if(bytes)
			*bytes = 2;

		*(U16*)(buf.ptr + i) = (U16) codepoint;
		return Error_none();
	}

	if (codepoint <= 0x10FFFF) {

		if(i + 4 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 4, Buffer_length(buf), "Buffer_writeAsUtf16()::i + 4 out of bounds");

		if(bytes)
			*bytes = 4;

		((U16*)(buf.ptr + i))[0]		= 0xD800 | (U16)(codepoint >> 10);
		((U16*)(buf.ptr + i))[1]		= 0xDC00 | (U16)(codepoint & 1023);
		return Error_none();
	}

	return Error_invalidParameter(2, 0, "Buffer_writeAsUtf16()::codepoint out of bounds (>0x10FFFF)");
}

//Detect unicode

Bool Buffer_isUnicode(Buffer buf, F32 threshold, Bool isUtf16) {

	threshold = 1 - threshold;

	U64 i = 0;
	F64 counter = 0;
	const F64 invLen = 1.0 / (F64)Buffer_length(buf);
	const U64 minWidth = isUtf16 ? 2 : 1;

	while (i + minWidth <= Buffer_length(buf)) {

		UnicodeCodePointInfo info = (UnicodeCodePointInfo) { 0 };

		if(
			(!isUtf16 && (Buffer_readAsUtf8(buf, i, &info)).genericError) ||
			(isUtf16 && (Buffer_readAsUtf16(buf, i, &info)).genericError)
		) {
			counter += invLen;

			if(counter >= threshold)
				return false;

			i += minWidth;
			continue;
		}

		i += info.bytes;
	}

	return i;
}

Bool Buffer_isUtf8(Buffer buf, F32 threshold) { return Buffer_isUnicode(buf, threshold, false); }
Bool Buffer_isUtf16(Buffer buf, F32 threshold) { return Buffer_isUnicode(buf, threshold, true); }

Bool Buffer_isAscii(Buffer buf) {

	for (U64 i = 0; i < Buffer_length(buf); ++i)
		if(!C8_isValidAscii((C8) buf.ptr[i]))
			return false;

	return Buffer_length(buf);
}
