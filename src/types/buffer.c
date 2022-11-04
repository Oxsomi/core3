#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/hash.h"
#include "math/math.h"

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output) {

	if(!output)
		return Error_nullPointer(2, 0);

	if(!buf.ptr)
		return Error_nullPointer(0, 0);

	if((offset >> 3) >= buf.siz)
		return Error_outOfBounds(1, 0, offset >> 3, buf.siz);

	*output = (buf.ptr[offset >> 3] >> (offset & 7)) & 1;
	return Error_none();
}

//Copy forwards

void Buffer_copy(Buffer dst, Buffer src) {

	U64 *dstPtr = (U64*)dst.ptr, *dstEnd = dstPtr + (dst.siz >> 3);
	const U64 *srcPtr = (const U64*)src.ptr, *srcEnd = srcPtr + (src.siz >> 3);

	for(; dstPtr < dstEnd && srcPtr < srcEnd; ++dstPtr, ++srcPtr)
		*dstPtr = *srcPtr;

	dstEnd = (U64*)(dst.ptr + dst.siz);
	srcEnd = (const U64*)(src.ptr + src.siz);

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
}

//Copy backwards; if ranges are overlapping this might be important

void Buffer_revCopy(Buffer dst, Buffer src) {

	if(!dst.ptr || !src.ptr || !dst.siz || !src.siz)
		return;

	U64 *dstPtr = (U64*)(dst.ptr + dst.siz), *dstBeg = (U64*)(dstPtr - (dst.siz >> 3));
	const U64 *srcPtr = (const U64*)(src.ptr + src.siz), *srcBeg = (U64*)(srcPtr - (src.siz >> 3));

	for(; dstPtr > dstBeg && srcPtr > srcBeg; )
		*(dstPtr--) = *(srcPtr--);

	dstBeg = (U64*) dstPtr;
	srcBeg = (const U64*) srcPtr;

	if((U64)dstPtr - 4 >= (U64)dst.ptr && (U64)srcPtr - 4 >= (U64)src.ptr ) {

		*(U32*)dstPtr = *(const U32*)srcPtr;

		dstPtr = (U64*)((U32*)dstPtr - 1);
		srcPtr = (const U64*)((const U32*)srcPtr - 1);
	}

	if ((U64)dstPtr - 2 >= (U64)dst.ptr && (U64)srcPtr - 2 >= (U64)src.ptr) {

		*(U16*)dstPtr = *(const U16*)srcPtr;

		dstPtr = (U64*)((U16*)dstPtr - 1);
		srcPtr = (const U64*)((const U16*)srcPtr - 1);
	}

	if ((U64)dstPtr - 1 >= (U64)dst.ptr && (U64)srcPtr - 1 >= (U64)src.ptr)
		*(U8*)dstPtr = *(const U8*)srcPtr;
}

//

Error Buffer_setBit(Buffer buf, U64 offset) {

	if(!buf.ptr)
		return Error_nullPointer(0, 0);

	if((offset >> 3) >= buf.siz)
		return Error_outOfBounds(1, 0, offset, buf.siz << 3);

	buf.ptr[offset >> 3] |= 1 << (offset & 7);
	return Error_none();
}

Error Buffer_resetBit(Buffer buf, U64 offset) {

	if(!buf.ptr)
		return Error_nullPointer(0, 0);

	if((offset >> 3) >= buf.siz)
		return Error_outOfBounds(1, 0, offset, buf.siz << 3);

	buf.ptr[offset >> 3] &= ~(1 << (offset & 7));
	return Error_none();
}

#define BitOp(x, dst, src) {															\
																						\
	if(!dst.ptr || !src.ptr)															\
		return Error_nullPointer(!dst.ptr ? 0 : 1, 0);									\
																						\
	U64 l = U64_min(dst.siz, src.siz);													\
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

Error Buffer_eq(Buffer buf0, Buffer buf1, Bool *result) {
	
	if(!buf0.ptr || !buf1.ptr)
		return Error_nullPointer(!buf0.ptr ? 0 : 1, 0);

	if(!result)
		return Error_nullPointer(2, 0);

	*result = false;

	if(buf0.siz != buf1.siz)
		return Error_none();

	U64 l = buf0.siz;
	U64 m = buf1.siz;
																					
	for (U64 i = 0, j = l >> 3, k = m >> 3; i < j && i < k; ++i) {

		U64 v0 = *((const U64*)buf0.ptr + i);
		U64 v1 = *((const U64*)buf1.ptr + i);

		if (v0 != v1)
			return Error_none();
	}
																					
	for (U64 i = l >> 3 << 3, j = m >> 3 << 3; i < l && j < m; ++i, ++j)
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

U64 Buffer_hash(Buffer buf) {

	U64 h = FNVHash_create();
	h = FNVHash_apply(h, buf.siz);

	if(buf.ptr) {

		for(U64 i = 0, j = buf.siz >> 3; i < j; ++i)
			h = FNVHash_apply(h, *((const U64*)buf.ptr + i));

		for(U64 i = buf.siz >> 3; i < buf.siz; ++i)
			h = FNVHash_apply(h, buf.ptr[i]);
	}

	return h;
}

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return Error_nullPointer(0, 0);

	if(((dstOff + bits - 1) >> 3) >= dst.siz)
		return Error_outOfBounds(1, 0, dstOff + bits, dst.siz << 3);

	if(!bits)
		return Error_invalidParameter(2, 0, 0);

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

	if(((dstOff + bits - 1) >> 3) >= dst.siz)
		return Error_outOfBounds(1, 0, dstOff + bits, dst.siz << 3);

	if(!bits)
		return Error_invalidParameter(2, 0, 0);

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

	U64 l = buf.siz;

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)buf.ptr + i) = b64;

	for (U64 i = l >> 3 << 3; i < l; ++i)
		buf.ptr[i] = b8;

	return Error_none();
}

Error Buffer_setAllBits(Buffer buf) {
	return Buffer_setAllToInternal(buf, U64_MAX, U8_MAX);
}

Error Buffer_unsetAllBits(Buffer buf) {
	return Buffer_setAllToInternal(buf, 0, 0);
}

Error Buffer_allocBitsInternal(U64 siz, Allocator alloc, Buffer *result) {

	if(!siz)
		return Error_invalidParameter(0, 0, 0);

	if(!alloc.alloc)
		return Error_nullPointer(1, 0);

	if(!result)
		return Error_nullPointer(0, 0);

	siz = (siz + 7) >> 3;	//Align to bytes
	return alloc.alloc(alloc.ptr, siz, result);
}

Error Buffer_createZeroBits(U64 siz, Allocator alloc, Buffer *result) {

	Error e = Buffer_allocBitsInternal(siz, alloc, result);

	if(e.genericError)
		return e;

	Buffer_unsetAllBits(*result);
	return Error_none();
}

Error Buffer_createOneBits(U64 siz, Allocator alloc, Buffer *result) {

	Error e = Buffer_allocBitsInternal(siz, alloc, result);

	if(e.genericError)
		return e;

	Buffer_setAllBits(*result);
	return Error_none();
}

Error Buffer_createCopy(Buffer buf, Allocator alloc, Buffer *result) {

	if(!buf.ptr || !buf.siz) {
		*result = Buffer_createNull();
		return Error_none();
	}

	U64 l = buf.siz;
	Error e = Buffer_allocBitsInternal(l << 3, alloc, result);

	if(e.genericError)
		return e;

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)result->ptr + i) = *((const U64*)buf.ptr + i);

	for (U64 i = l >> 3 << 3; i < l; ++i)
		result->ptr[i] = buf.ptr[i];

	return Error_none();
}

Error Buffer_free(Buffer *buf, Allocator alloc) {

	if(!buf)
		return Error_none();

	if(!buf->ptr)
		return Error_nullPointer(0, 0);

	if(!alloc.free)
		return Error_nullPointer(1, 0);

	Error err = alloc.free(alloc.ptr, *buf);
	*buf = Buffer_createNull();
	return err;
}

Error Buffer_createEmptyBytes(U64 siz, Allocator alloc, Buffer *output) {
	return Buffer_createZeroBits(siz << 3, alloc, output);
}

Error Buffer_createUninitializedBytes(U64 siz, Allocator alloc, Buffer *result) {
	return Buffer_allocBitsInternal(siz << 3, alloc, result);
}

Error Buffer_offset(Buffer *buf, U64 siz) {

	if(!buf || !buf->ptr)
		return Error_nullPointer(0, 0);

	if(siz > buf->siz)
		return Error_outOfBounds(1, 0, siz, buf->siz);

	buf->ptr += siz;
	buf->siz -= siz;

	if(!buf->siz)
		*buf = Buffer_createNull();

	return Error_none();
}

inline void Buffer_copyBytesInternal(U8 *ptr, const void *v, U64 siz) {

	for (U64 i = 0, j = siz >> 3; i < j; ++i)
		*((U64*)ptr + i) = *((const U64*)v + i);

	for (U64 i = siz >> 3 << 3; i < siz; ++i)
		ptr[i] = *((const U8*)v + i);
}

Error Buffer_appendBuffer(Buffer *buf, Buffer append) {

	if(!append.ptr)
		return Error_nullPointer(1, 0);

	void *ptr = buf ? buf->ptr : NULL;

	Error e = Buffer_offset(buf, append.siz);

	if(e.genericError)
		return e;

	Buffer_copyBytesInternal(ptr, append.ptr, append.siz);
	return Error_none();
}

Error Buffer_append(Buffer *buf, const void *v, U64 siz) {
	return Buffer_appendBuffer(buf, Buffer_createRef((void*) v, siz));
}

Error Buffer_createSubset(Buffer buf, U64 offset, U64 siz, Buffer *output) {

	Error e = Buffer_offset(&buf, offset);

	if(e.genericError)
		return e;
		
	if(siz > buf.siz)
		return Error_outOfBounds(2, 0, siz, buf.siz);

	buf.siz = siz;
	*output = buf;
	return Error_none();
}
