#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/type_cast.h"
#include "math/math.h"
#include "math/vec.h"

Bool BitRef_get(BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
void BitRef_set(BitRef b) { if(b.ptr && !b.isConst) *b.ptr |= 1 << b.off; }
void BitRef_reset(BitRef b) { if(b.ptr && !b.isConst) *b.ptr &= ~(1 << b.off); }

void BitRef_setTo(BitRef b, Bool v) { 
	if(v) BitRef_set(b);
	else BitRef_reset(b);
}

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output) {

	if(!output || !buf.ptr)
		return Error_nullPointer(!buf.ptr ? 0 : 2, 0);

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, 0, offset >> 3, Buffer_length(buf));

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

	U64 dstLen = Buffer_length(dst), srcLen = Buffer_length(src);

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

	U64 dstLen = Buffer_length(dst), srcLen = Buffer_length(src);

	U64 *dstPtr = (U64*)(dst.ptr + dstLen), *dstBeg = (U64*)(dstPtr - (dstLen >> 3));
	const U64 *srcPtr = (const U64*)(src.ptr + srcLen), *srcBeg = (U64*)(srcPtr - (srcLen >> 3));

	for(; dstPtr > dstBeg && srcPtr > srcBeg; ) {
		--srcPtr; --dstPtr;
		*dstPtr = *srcPtr;
	}

	dstBeg = (U64*) dstPtr;
	srcBeg = (const U64*) srcPtr;

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
		return Error_nullPointer(0, 0);

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0);

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, 0, offset, Buffer_length(buf) << 3);

	buf.ptr[offset >> 3] |= 1 << (offset & 7);
	return Error_none();
}

Error Buffer_resetBit(Buffer buf, U64 offset) {

	if(!buf.ptr)
		return Error_nullPointer(0, 0);

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0);

	if((offset >> 3) >= Buffer_length(buf))
		return Error_outOfBounds(1, 0, offset, Buffer_length(buf) << 3);

	buf.ptr[offset >> 3] &= ~(1 << (offset & 7));
	return Error_none();
}

#define BitOp(x, dst, src) {															\
																						\
	if(!dst.ptr || !src.ptr)															\
		return Error_nullPointer(!dst.ptr ? 0 : 1, 0);									\
																						\
	if(Buffer_isConstRef(dst))															\
		return Error_constData(0, 0);													\
																						\
	U64 l = U64_min(Buffer_length(dst), Buffer_length(src));							\
																						\
	for(U64 i = 0, j = l >> 3; i < j; ++i)												\
		*((U64*)dst.ptr + i) x *((const U64*)src.ptr + i);								\
																						\
	for (U64 i = l >> 3 << 3; i < l; ++i)												\
		dst.ptr[i] x src.ptr[i];														\
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

Buffer Buffer_createConstRef(const void *v, U64 length) { 

	if(!length || !v)
		return Buffer_createNull();

	if(length >> 48 || (U64)v >> 48)		//Invalid addresses (unsupported by CPUs)
		return Buffer_createNull();

	return (Buffer) { .ptr = (U8*) v, .lengthAndRefBits = length | ((U64)3 << 62) };
}

Error Buffer_eq(Buffer buf0, Buffer buf1, Bool *result) {

	if (!buf0.ptr && !buf1.ptr) {
		if(result) *result = true;
		return Error_none();
	}
	
	if(!buf0.ptr || !buf1.ptr)
		return Error_nullPointer(!buf0.ptr ? 0 : 1, 0);

	if(!result)
		return Error_nullPointer(2, 0);

	*result = false;

	U64 len0 = Buffer_length(buf0);

	if(len0 != Buffer_length(buf1))
		return Error_none();

	for (U64 i = 0, j = len0 >> 3; i < j; ++i)
		if (*((const U64*)buf0.ptr + i) != *((const U64*)buf1.ptr + i))
			return Error_none();

	for (U64 i = len0 >> 3 << 3; i < len0; ++i)
		if (buf0.ptr[i] != buf1.ptr[i])
			return Error_none();

	*result = true;
	return Error_none();
}

Error Buffer_neq(Buffer buf0, Buffer buf1, Bool *result) {

	Error e = Buffer_eq(buf0, buf1, result);

	if(e.genericError)
		return e;

	*result = !*result;
	return Error_none();
}

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return Error_nullPointer(0, 0);

	if(Buffer_isConstRef(dst))
		return Error_constData(0, 0);

	if(!bits)
		return Error_invalidParameter(2, 0, 0);

	if(dstOff + bits > (Buffer_length(dst) << 3))
		return Error_outOfBounds(1, 0, dstOff + bits, Buffer_length(dst) << 3);

	U64 dstOff8 = (dstOff + 7) >> 3;
	U64 bitEnd = dstOff + bits;

	//Bits, begin

	for (U64 i = dstOff; i < bitEnd && i < dstOff8 << 3; ++i)
		dst.ptr[i >> 3] |= (1 << (i & 7));

	//Bytes, until U64 aligned

	U64 dstOff64 = (dstOff8 + 7) >> 3;
	U64 bitEnd8 = bitEnd >> 3;

	for (U64 i = dstOff8; i < bitEnd8 && i < dstOff64 << 3; ++i)
		dst.ptr[i] = U8_MAX;

	//U64 aligned

	U64 bitEnd64 = bitEnd8 >> 3;

	for(U64 i = dstOff64; i < bitEnd64; ++i)
		*((U64*)dst.ptr + i) = U64_MAX;

	//Bytes unaligned at end

	for(U64 i = bitEnd64 << 3; i < bitEnd8; ++i)
		dst.ptr[i] = U8_MAX;

	//Bits unaligned at end

	for (U64 i = bitEnd8 << 3; i < bitEnd; ++i)
		dst.ptr[i >> 3] |= (1 << (i & 7));

	return Error_none();
}

Error Buffer_unsetBitRange(Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return Error_nullPointer(0, 0);

	if(Buffer_isConstRef(dst))
		return Error_constData(0, 0);

	if(!bits)
		return Error_invalidParameter(2, 0, 0);

	if(dstOff + bits > (Buffer_length(dst) << 3))
		return Error_outOfBounds(1, 0, dstOff + bits, Buffer_length(dst) << 3);

	U64 dstOff8 = (dstOff + 7) >> 3;
	U64 bitEnd = dstOff + bits;

	//Bits, begin

	for (U64 i = dstOff; i < bitEnd && i < dstOff8 << 3; ++i)
		dst.ptr[i >> 3] &=~ (1 << (i & 7));

	//Bytes, until U64 aligned

	U64 dstOff64 = (dstOff8 + 7) >> 3;
	U64 bitEnd8 = bitEnd >> 3;

	for (U64 i = dstOff8; i < bitEnd8 && i < dstOff64 << 3; ++i)
		dst.ptr[i] = 0;

	//U64 aligned

	U64 bitEnd64 = bitEnd8 >> 3;

	for(U64 i = dstOff64; i < bitEnd64; ++i)
		*((U64*)dst.ptr + i) = 0;

	//Bytes unaligned at end

	for(U64 i = bitEnd64 << 3; i < bitEnd8; ++i)
		dst.ptr[i] = 0;

	//Bits unaligned at end

	for (U64 i = bitEnd8 << 3; i < bitEnd; ++i)
		dst.ptr[i >> 3] &=~ (1 << (i & 7));

	return Error_none();
}

inline Error Buffer_setAllToInternal(Buffer buf, U64 b64, U8 b8) {

	if(!buf.ptr)
		return Error_nullPointer(0, 0);

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0);

	U64 l = Buffer_length(buf);

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)buf.ptr + i) = b64;

	for (U64 i = l >> 3 << 3; i < l; ++i)
		buf.ptr[i] = b8;

	return Error_none();
}

Error Buffer_createBits(U64 length, Bool value, Allocator alloc, Buffer *result) {
	return !value ? Buffer_createZeroBits(length, alloc, result) : Buffer_createOneBits(length, alloc, result);
}

Error Buffer_setAllBits(Buffer buf) {
	return Buffer_setAllToInternal(buf, U64_MAX, U8_MAX);
}

Error Buffer_unsetAllBits(Buffer buf) {
	return Buffer_setAllToInternal(buf, 0, 0);
}

Error Buffer_allocBitsInternal(U64 length, Allocator alloc, Buffer *result) {

	if(!length)
		return Error_invalidParameter(0, 0, 0);

	if(!alloc.alloc)
		return Error_nullPointer(1, 0);

	if(!result)
		return Error_nullPointer(0, 0);

	if(result->ptr)
		return Error_invalidParameter(2, 0, 0);

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

	if(!Buffer_length(buf)) {

		if(result)
			*result = Buffer_createNull();

		return Error_none();
	}

	U64 l = Buffer_length(buf);
	Error e = Buffer_allocBitsInternal(l << 3, alloc, result);

	if(e.genericError)
		return e;

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)result->ptr + i) = *((const U64*)buf.ptr + i);

	for (U64 i = l >> 3 << 3; i < l; ++i)
		result->ptr[i] = buf.ptr[i];

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

	Bool success = alloc.free(alloc.ptr, *buf);
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
		return Error_nullPointer(0, 0);

	if(!Buffer_isRef(*buf))								//Ensure we don't accidentally call this and leak memory
		return Error_invalidOperation(0);

	U64 bufLen = Buffer_length(*buf);

	if(length > bufLen)
		return Error_outOfBounds(1, 0, length, bufLen);

	buf->ptr += length;

	//Maintain constness

	buf->lengthAndRefBits = (bufLen - length) | (buf->lengthAndRefBits >> 62 << 62);

	if(!bufLen)
		*buf = Buffer_createNull();

	return Error_none();
}

inline void Buffer_copyBytesInternal(U8 *ptr, const void *v, U64 length) {

	for (U64 i = 0, j = length >> 3; i < j; ++i)
		*((U64*)ptr + i) = *((const U64*)v + i);

	for (U64 i = length >> 3 << 3; i < length; ++i)
		ptr[i] = *((const U8*)v + i);
}

Error Buffer_appendBuffer(Buffer *buf, Buffer append) {

	if(!append.ptr)
		return Error_nullPointer(1, 0);

	if(buf && Buffer_isConstRef(*buf))					//We need write access
		return Error_constData(0, 0);

	void *ptr = buf ? buf->ptr : NULL;

	Error e = Buffer_offset(buf, Buffer_length(append));

	if(e.genericError)
		return e;

	Buffer_copyBytesInternal(ptr, append.ptr, Buffer_length(append));
	return Error_none();
}

Error Buffer_append(Buffer *buf, const void *v, U64 length) {
	return Buffer_appendBuffer(buf, Buffer_createConstRef(v, length));
}

Error Buffer_createSubset(Buffer buf, U64 offset, U64 length, Bool isConst, Buffer *output) {

	//Since our buffer was passed here, it's safe to make a ref (to ensure Buffer_offset doesn't fail)

	buf = Buffer_createRefFromBuffer(buf, isConst);
	Error e = Buffer_offset(&buf, offset);

	if(e.genericError)
		return e;

	if(length > Buffer_length(buf))
		return Error_outOfBounds(2, 0, length, Buffer_length(buf));

	buf.lengthAndRefBits = length | (buf.lengthAndRefBits >> 62 << 62);
	*output = buf;
	return Error_none();
}

Error Buffer_combine(Buffer a, Buffer b, Allocator alloc, Buffer *output) {

	U64 alen = Buffer_length(a), blen = Buffer_length(b);

	if(alen + blen < alen)
		return Error_overflow(0, 0, alen + blen, alen);

	Error err = Buffer_createUninitializedBytes(alen + blen, alloc, output);

	if(err.genericError)
		return err;

	Buffer_copy(*output, a);
	Buffer_copy(Buffer_createRef(output->ptr + alen, blen), b);
	return Error_none();
}

Error Buffer_appendU64(Buffer *buf, U64 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendU32(Buffer *buf, U32 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendU16(Buffer *buf, U16 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendU8(Buffer *buf,  U8 v)  { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendC8(Buffer *buf,  C8 v)  { return Buffer_append(buf, &v, sizeof(v)); }

Error Buffer_appendI64(Buffer *buf, I64 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendI32(Buffer *buf, I32 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendI16(Buffer *buf, I16 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendI8(Buffer *buf,  I8 v)  { return Buffer_append(buf, &v, sizeof(v)); }

Error Buffer_appendF32(Buffer *buf, F32 v) { return Buffer_append(buf, &v, sizeof(v)); }

Error Buffer_appendF32x4(Buffer *buf, F32x4 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendF32x2(Buffer *buf, I32x2 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendI32x4(Buffer *buf, I32x4 v) { return Buffer_append(buf, &v, sizeof(v)); }
Error Buffer_appendI32x2(Buffer *buf, I32x2 v) { return Buffer_append(buf, &v, sizeof(v)); }

//UTF-8
//https://en.wikipedia.org/wiki/UTF-8

Error Buffer_readAsUTF8(Buffer buf, U64 i, UTF8CodePointInfo *codepoint) {

	if(!codepoint)
		return Error_nullPointer(3, 0);

	if(i == U64_MAX || i + 1 > Buffer_length(buf))
		return Error_outOfBounds(0, 0, i, Buffer_length(buf));

	//Ascii

	U8 v0 = buf.ptr[i++];

	if (!(v0 >> 7)) {

		if(!C8_isValidAscii((C8)v0))
			return Error_invalidParameter(0, 0, 0);

		*codepoint = (UTF8CodePointInfo) { .size = 1, .index = v0 };
		return Error_none();
	}

	if(v0 < 0xC0)
		return Error_invalidParameter(0, 0, 1);

	//2-4 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(1, 0, i, Buffer_length(buf));

	U8 v1 = buf.ptr[i++];

	if(v1 < 0x80 || v1 >= 0xC0)
		return Error_invalidParameter(0, 0, 2);

	//2 bytes

	if (v0 < 0xE0) {
		*codepoint = (UTF8CodePointInfo) { .size = 2, .index = (((U16)v0 & 0x1F) << 6) | (v1 & 0x3F) };
		return Error_none();
	}

	//3 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(2, 0, i, Buffer_length(buf));

	U8 v2 = buf.ptr[i++];

	if(v2 < 0x80 || v2 >= 0xC0)
		return Error_invalidParameter(0, 0, 3);

	if (v0 < 0xF0) {
		*codepoint = (UTF8CodePointInfo) { .size = 3, .index = (((U32)v0 & 0xF) << 12) | (((U32)v1 & 0x3F) << 6) | (v2 & 0x3F) };
		return Error_none();
	}

	//4 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(3, 0, i, Buffer_length(buf));

	U8 v3 = buf.ptr[i++];

	if(v3 < 0x80 || v3 >= 0xC0)
		return Error_invalidParameter(0, 0, 4);

	*codepoint = (UTF8CodePointInfo) { 
		.size = 4, 
		.index = (((U32)v0 & 0x7) << 18) | (((U32)v1 & 0x3F) << 12) | (((U32)v2 & 0x3F) << 6) | (v3 & 0x3F) 
	};

	return Error_none();
}

Error Buffer_writeAsUTF8(Buffer buf, U64 i, UTF8CodePoint codepoint) {

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0);

	if ((codepoint & 0x7F) == codepoint)
		return Buffer_appendU8(&buf, (U8) codepoint);

	if ((codepoint & 0x7FF) == codepoint) {

		if(i + 2 > Buffer_length(buf))
			return Error_outOfBounds(0, 0, i + 2, Buffer_length(buf));

		buf.ptr[i]		= 0xC0 | (U8)(codepoint >> 6);
		buf.ptr[i + 1]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	if ((codepoint & 0xFFFF) == codepoint) {

		if(i + 3 > Buffer_length(buf))
			return Error_outOfBounds(0, 0, i + 3, Buffer_length(buf));

		buf.ptr[i]		= 0xE0 | (U8)(codepoint >> 12);
		buf.ptr[i + 1]	= 0x80 | (U8)((codepoint >> 6) & 0x3F);
		buf.ptr[i + 2]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	if (codepoint <= 0x10FFFF) {

		if(i + 4 > Buffer_length(buf))
			return Error_outOfBounds(0, 0, i + 4, Buffer_length(buf));

		buf.ptr[i]		= 0xF0 | (U8)(codepoint >> 18);
		buf.ptr[i + 1]	= 0x80 | (U8)((codepoint >> 12) & 0x3F);
		buf.ptr[i + 2]	= 0x80 | (U8)((codepoint >> 6) & 0x3F);
		buf.ptr[i + 3]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	return Error_invalidParameter(2, 0, 0);
}

Bool Buffer_isUTF8(Buffer buf, F32 threshold) {

	threshold = 1 - threshold;

	U64 i = 0;
	F32 counter = 0;

	while (i < Buffer_length(buf)) {

		UTF8CodePointInfo info;

		if((Buffer_readAsUTF8(buf, i, &info)).genericError) {
			
			counter += 1.f / Buffer_length(buf);

			if(counter >= threshold)
				return false;

			++i;
			continue;
		}

		i += info.size;
	}

	return i;
}
