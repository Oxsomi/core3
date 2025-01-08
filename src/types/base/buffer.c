/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/base/buffer.h"
#include "types/base/error.h"

#include <string.h>

//Basic buffer helpers

U64 Buffer_length(Buffer buf) {
	return buf.lengthAndRefBits << 2 >> 2;
}

Buffer Buffer_createManagedPtr(void *ptr, U64 length) {

	if((U64)ptr >> 48 || length >> 48 || !ptr || !length)
		return (Buffer) { 0 };

	return (Buffer) { .ptr = ptr, .lengthAndRefBits = length };
}

Buffer Buffer_createRefFromBuffer(Buffer buf, Bool isConst) {

	if(!buf.ptr || (!isConst && Buffer_isConstRef(buf)))
		return (Buffer) { 0 };

	Buffer copy = buf;
	copy.lengthAndRefBits |= ((U64)1 << 63) | ((U64)isConst << 62);
	return copy;
}

Bool Buffer_isRef(Buffer buf) {
	return buf.lengthAndRefBits >> 62;
}

Bool Buffer_isConstRef(Buffer buf) {
	return (buf.lengthAndRefBits >> 62) == 3;
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

Bool Buffer_copy(Buffer dst, Buffer src) {

	if(!dst.ptr || !src.ptr)
		return true;

	if(Buffer_isConstRef(dst))
		return false;

	U64 dstLen = Buffer_length(dst);
	U64 srcLen = Buffer_length(src);
	memcpy(dst.ptrNonConst, src.ptr, srcLen <= dstLen ? srcLen : dstLen);
	return true;
}

Bool Buffer_revCopy(Buffer dst, Buffer src) {

	if(!dst.ptr || !src.ptr)
		return true;

	if(Buffer_isConstRef(dst))
		return false;

	U64 dstLen = Buffer_length(dst);
	U64 srcLen = Buffer_length(src);
	memmove(dst.ptrNonConst, src.ptr, srcLen <= dstLen ? srcLen : dstLen);
	return true;
}

Error Buffer_setBit(Buffer buf, U64 offset) {

	if(!buf.ptr)
		return Error_nullPointer(0, "Buffer_setBit()::buf is required");

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_setBit()::buf should be writable");

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, offset, Buffer_length(buf) << 3, "Buffer_setBit()::offset out of bounds");

	buf.ptrNonConst[offset >> 3] |= 1 << (offset & 7);
	return Error_none();
}

Error Buffer_resetBit(Buffer buf, U64 offset) {

	if(!buf.ptr)
		return Error_nullPointer(0, "Buffer_resetBit()::buf is required");

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_resetBit()::buf should be writable");

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, offset, Buffer_length(buf) << 3, "Buffer_resetBit()::offset out of bounds");

	buf.ptrNonConst[offset >> 3] &= ~(1 << (offset & 7));
	return Error_none();
}

#define BitOp(x, dst, src, ...) {														\
																						\
	if(!dst.ptr || !src.ptr)															\
		return Error_nullPointer(!dst.ptr ? 0 : 1, "BitOp::dst and src are required");	\
																						\
	if(Buffer_isConstRef(dst))															\
		return Error_constData(0, 0, "BitOp::dst should be writable");					\
																						\
	U64 dstLen = Buffer_length(dst), srcLen = Buffer_length(src);						\
	U64 l = dstLen <= srcLen ? dstLen : srcLen;											\
																						\
	if(!((U64)dst.ptr & 7) && !((U64)src.ptr & 7)) {									\
																						\
		for(U64 i = 0, j = l >> 3; i < j; ++i)											\
			*((U64*)dst.ptrNonConst + i) x *((const U64*)src.ptr + i);					\
																						\
		for (U64 i = l >> 3 << 3; i < l; ++i)											\
			dst.ptrNonConst[i] x src.ptr[i];											\
	}																					\
																						\
	else for(U8 i = 0; i < l; ++i)														\
		dst.ptrNonConst[i] x src.ptr[i];												\
																						\
	__VA_ARGS__																			\
																						\
	return Error_none();																\
}

Error Buffer_bitwiseOr(Buffer dst, Buffer src)  BitOp(|=, dst, src, )
Error Buffer_bitwiseXor(Buffer dst, Buffer src) BitOp(^=, dst, src, )
Error Buffer_bitwiseNot(Buffer dst) BitOp(=~, dst, dst, )
Error Buffer_bitwiseAnd(Buffer dst, Buffer src) BitOp(
	&=, dst, src,
	if(dstLen > srcLen)
		Buffer_unsetAllBits(Buffer_createRefConst(dst.ptrNonConst + srcLen, dstLen - srcLen));
)

Error Buffer_setAllBitsTo(Buffer buf, Bool isOn) {
	return isOn ? Buffer_setAllBits(buf) : Buffer_unsetAllBits(buf);
}

Buffer Buffer_createNull() { return (Buffer) { 0 }; }

Buffer Buffer_createRef(void *v, U64 length) {

	if(!length || !v)
		return Buffer_createNull();

	if(length >> 48 || (U64)v >> 48)		//Invalid addresses (unsupported by CPUs)
		return Buffer_createNull();

	return (Buffer) { .ptrNonConst = (U8*) v, .lengthAndRefBits = length | ((U64)1 << 63) };
}

Buffer Buffer_createRefConst(const void *v, U64 length) {

	if(!length || !v)
		return Buffer_createNull();

	if(length >> 48 || (U64)v >> 48)		//Invalid addresses (unsupported by CPUs)
		return Buffer_createNull();

	return (Buffer) { .ptr = (const U8*) v, .lengthAndRefBits = length | ((U64)3 << 62) };
}

Bool Buffer_eq(Buffer buf0, Buffer buf1) {

	if (!buf0.ptr && !buf1.ptr)
		return true;

	if (!buf0.ptr || !buf1.ptr)
		return false;

	const U64 len0 = Buffer_length(buf0);

	if(len0 != Buffer_length(buf1))
		return false;

	return memcmp(buf0.ptr, buf1.ptr, len0) == 0;
}

Bool Buffer_neq(Buffer buf0, Buffer buf1) { return !Buffer_eq(buf0, buf1); }

Error Buffer_offset(Buffer *buf, U64 length) {

	if(!length)
		return Error_none();

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
	Buffer_copy(Buffer_createRef(ptr, length), Buffer_createRefConst(v, length));
}

Error Buffer_appendBuffer(Buffer *buf, Buffer append) {

	if(!append.ptr)
		return Error_nullPointer(1, "Buffer_appendBuffer()::append is required");

	if(buf && Buffer_isConstRef(*buf))					//We need write access
		return Error_constData(0, 0, "Buffer_appendBuffer()::buf should be writable");

	U8 *ptr = buf->ptrNonConst;
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

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return Error_nullPointer(0, "Buffer_setBitRange()::dst is required");

	if(Buffer_isConstRef(dst))
		return Error_constData(0, 0, "Buffer_setBitRange()::dst should be writable");

	if(!bits)
		return Error_invalidParameter(2, 0, "Buffer_setBitRange()::bits should be non zero");

	if(dstOff + bits > (Buffer_length(dst) << 3))
		return Error_outOfBounds(1, dstOff + bits, Buffer_length(dst) << 3, "Buffer_setBitRange()::dstOff out of bounds");

	//Bits, unaligned

	if(dstOff & 7) {
		U8 mask = bits >= 8 ? 0xFF : ((1 << bits) - 1);
		mask <<= dstOff & 7;
		dst.ptrNonConst[dstOff >> 3] |= mask;
	}

	//Efficient part

	U64 off = (dstOff + 7) >> 3;
	U64 endOff = (dstOff + bits) >> 3;

	if(endOff > off)
		memset(dst.ptrNonConst + off, 0xFF, endOff - off);

	//Bits unaligned at end

	if((endOff << 3) != dstOff + bits) {
		bits = dstOff + bits - (endOff << 3);
		U8 mask = (1 << bits) - 1;
		mask <<= dstOff & 7;
		dst.ptrNonConst[endOff >> 3] |= mask;
	}

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

	//Bits, unaligned

	if(dstOff & 7) {
		U8 mask = bits >= 8 ? 0xFF : ((1 << bits) - 1);
		mask <<= dstOff & 7;
		dst.ptrNonConst[dstOff >> 3] &=~ mask;
	}

	//Efficient part

	U64 off = (dstOff + 7) >> 3;
	U64 endOff = (dstOff + bits) >> 3;

	if(endOff > off)
		memset(dst.ptrNonConst + off, 0x00, endOff - off);

	//Bits unaligned at end

	if((endOff << 3) != dstOff + bits) {
		bits = dstOff + bits - (endOff << 3);
		U8 mask = (1 << bits) - 1;
		mask <<= dstOff & 7;
		dst.ptrNonConst[endOff >> 3] &=~ mask;
	}

	return Error_none();
}

Error Buffer_setAllToInternal(Buffer buf, U8 b8) {

	if(!buf.ptr)
		return Error_nullPointer(0, "Buffer_setAllToInternal()::buf is required");

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_setAllToInternal()::buf should be writable");

	memset(buf.ptrNonConst, b8, Buffer_length(buf));
	return Error_none();
}

Error Buffer_setAllBits(Buffer dst) {
	return Buffer_setAllToInternal(dst, U8_MAX);
}

Error Buffer_unsetAllBits(Buffer dst) {
	return Buffer_setAllToInternal(dst, 0);
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