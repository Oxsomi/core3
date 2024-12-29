/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/base/types.h"
#include "types/base/error.h"

#include <ctype.h>
#include <string.h>

#undef FixedPoint

#define FixedPoint(frac, integ)																\
																							\
typedef U64 FP##integ##f##frac;																\
																							\
FP##integ##f##frac FP##integ##f##frac##_add(FP##integ##f##frac a, FP##integ##f##frac b) {	\
	return a + b;																			\
}																							\
																							\
FP##integ##f##frac FP##integ##f##frac##_sub(FP##integ##f##frac a, FP##integ##f##frac b) {	\
	return a - b;																			\
}																							\
																							\
FP##integ##f##frac FP##integ##f##frac##_fromDouble(F64 v) {									\
																							\
	Bool sign = v < 0;																		\
																							\
	if(sign)																				\
		v = -v;																				\
																							\
	v *= (1 << frac);																		\
	FP##integ##f##frac res = (FP##integ##f##frac)v;											\
																							\
	if(sign) {																				\
		--res;																				\
		res ^= ((FP##integ##f##frac)1 << ((frac + integ) + 1)) - 1;							\
	}																						\
																							\
	return res;																				\
}																							\
																							\
F64 FP##integ##f##frac##_toDouble(FP##integ##f##frac value) {								\
																							\
	if (value >> (frac + integ)) {															\
		value ^= ((FP##integ##f##frac)1 << ((frac + integ) + 1)) - 1;						\
		++value;																			\
		return value * (-1.0 / (1 << frac));												\
	} 																						\
																							\
	return value * (1.0 / (1 << frac)); 													\
}

FixedPoint(4, 37)
FixedPoint(6, 46)

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

C8 C8_toLower(C8 c) {
	return (C8) tolower(c);
}

C8 C8_toUpper(C8 c) {
	return (C8) toupper(c);
}

C8 C8_transform(C8 c, EStringTransform transform) {
	return transform == EStringTransform_None ? c : (
		transform == EStringTransform_Lower ? C8_toLower(c) :
		C8_toUpper(c)
	);
}

Bool C8_isBin(C8 c) { return c == '0' || c == '1'; }
Bool C8_isOct(C8 c) { return c >= '0' && c <= '7'; }
Bool C8_isDec(C8 c) { return c >= '0' && c <= '9'; }

Bool C8_isUpperCase(C8 c) { return c >= 'A' && c <= 'Z'; }
Bool C8_isLowerCase(C8 c) { return c >= 'a' && c <= 'z'; }
Bool C8_isUpperCaseHex(C8 c) { return c >= 'A' && c <= 'F'; }
Bool C8_isLowerCaseHex(C8 c) { return c >= 'a' && c <= 'f'; }
Bool C8_isWhitespace(C8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
Bool C8_isNewLine(C8 c) { return c == '\n' || c == '\r'; }

Bool C8_isHex(C8 c) { return C8_isDec(c) || C8_isUpperCaseHex(c) || C8_isLowerCaseHex(c); }
Bool C8_isNyto(C8 c) { return C8_isDec(c) || C8_isUpperCase(c) || C8_isLowerCase(c) || c == '_' || c == '$'; }
Bool C8_isAlphaNumeric(C8 c) { return C8_isDec(c) || C8_isUpperCase(c) || C8_isLowerCase(c); }
Bool C8_isAlpha(C8 c) { return C8_isUpperCase(c) || C8_isLowerCase(c); }

Bool C8_isSymbol(C8 c) {

	Bool symbolRange0 = c > ' ' && c < '0';		//~"#%&'()*+,-./$
	Bool symbolRange1 = c > '9' && c < 'A';		//:;<=>?@
	Bool symbolRange2 = c > 'Z' && c < 'a';		//[\]^`_
	Bool symbolRange3 = c > 'z' && c < 0x7F;	//{|}~

	return symbolRange0 || symbolRange1 || symbolRange2 || symbolRange3;
}

Bool C8_isLexerSymbol(C8 c) {
	return C8_isSymbol(c) && c != '$' && c != '_';
}

Bool C8_isValidAscii(C8 c) { return (c >= 0x20 && c < 0x7F) || c == '\t' || c == '\n' || c == '\r'; }

Bool C8_isValidFileName(C8 c) {
	return
		(c >= 0x20 && c < 0x7F) &&
		c != '<' && c != '>' && c != ':' && c != '"' && c != '|' &&
		c != '?' && c != '*' && c != '/' && c != '\\';
}

U8 C8_bin(C8 c) { return c == '0' ? 0 : (c == '1' ? 1 : U8_MAX); }
U8 C8_oct(C8 c) { return C8_isOct(c) ? c - '0' : U8_MAX; }
U8 C8_dec(C8 c) { return C8_isDec(c) ? c - '0' : U8_MAX; }

U8 C8_hex(C8 c) {

	if (C8_isDec(c))
		return c - '0';

	if (C8_isUpperCaseHex(c))
		return c - 'A' + 10;

	if (C8_isLowerCaseHex(c))
		return c - 'a' + 10;

	return U8_MAX;
}

U8 C8_nyto(C8 c) {

	if (C8_isDec(c))
		return c - '0';

	if (C8_isUpperCase(c))
		return c - 'A' + 10;

	if (C8_isLowerCase(c))
		return c - 'a' + 36;

	if (c == '_')
		return 62;

	return c == '$' ? 63 : U8_MAX;
}

C8 C8_createBin(U8 v) { return (v == 0 ? '0' : (v == 1 ? '1' : C8_MAX)); }
C8 C8_createOct(U8 v) { return v < 8 ? '0' + v : C8_MAX; }
C8 C8_createDec(U8 v) { return v < 10 ? '0' + v : C8_MAX; }
C8 C8_createHex(U8 v) { return v < 10 ? '0' + v : (v < 16 ? 'A' + v - 10 : C8_MAX); }

//Nytodecimal: 0-9A-Za-z_$

C8 C8_createNyto(U8 v) {
	return v < 10 ? '0' + v : (
		v < 36 ? 'A' + v - 10 : (
			v < 62 ? 'a' + v - 36 : (
				v == 62 ? '_' : (
					v == 63 ? '$' : C8_MAX
				)
			)
		)
	);
}

//Basic buffer helpers

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

	U64 dstLen = Buffer_length(dst);
	U64 srcLen = Buffer_length(src);
	memcpy(dst.ptrNonConst, src.ptr, srcLen <= dstLen ? srcLen : dstLen);
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

#define BitOp(x, dst, src) {															\
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
