#include "types/allocator.h"
#include "types/error.h"
#include "types/bit.h"
#include "types/hash.h"
#include "math/math.h"

struct Error Bit_get(struct Buffer buf, U64 offset, Bool *output) {

	if(!output)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 2
		};

	if(!buf.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if((offset >> 3) >= buf.siz)
		return (struct Error) {
			.genericError = GenericError_OutOfBounds,
			.paramId = 0,
			.paramValue0 = offset >> 3,
			.paramValue1 = buf.siz
		};

	*output = (buf.ptr[offset >> 3] >> (offset & 7)) & 1;
	return (struct Error) { 0 };
}

//Copy forwards

void Bit_copy(struct Buffer dst, struct Buffer src) {

	U64 *dstPtr = (U64*)dst.ptr, *dstEnd = dstPtr + (dst.siz >> 3);
	const U64 *srcPtr = (const U64*)src.ptr, *srcEnd = srcPtr + (src.siz >> 3);

	for(; dstPtr < dstEnd && srcPtr < srcEnd; ++dstPtr, ++srcPtr)
		*dstPtr = *srcPtr;

	if((U64)dstPtr + 4 <= (U64)dstEnd && (U64)srcPtr + 4 <= (U64)srcEnd) {

		*(U32*)dstPtr = *(const U32*)srcPtr;

		dstPtr = (const U64*)((const U32*)dstPtr + 1);
		srcPtr = (const U64*)((const U32*)srcPtr + 1);
	}

	if ((U64)dstPtr + 2 <= (U64)dstEnd && (U64)srcPtr + 2 <= (U64)srcEnd) {

		*(U16*)dstPtr = *(const U16*)srcPtr;

		dstPtr = (const U64*)((const U16*)dstPtr + 1);
		srcPtr = (const U64*)((const U16*)srcPtr + 1);
	}

	if ((U64)dstPtr + 1 <= (U64)dstEnd && (U64)srcPtr + 1 <= (U64)srcEnd)
		*(U8*)dstPtr = *(const U8*)srcPtr;
}

//Copy backwards; if ranges are overlapping this might be important

void Bit_revCopy(struct Buffer dst, struct Buffer src) {

	if(!dst.ptr || !src.ptr || !dst.siz || !src.siz)
		return;

	U64 *dstPtr = (U64*)(dst.ptr + dst.siz), *dstBeg = (U64*)(dstPtr - (dst.siz >> 3));
	const U64 *srcPtr = (const U64*)(src.ptr + src.siz), *srcBeg = (U64*)(srcPtr - (src.siz >> 3));

	for(; dstPtr > dstBeg && srcPtr > srcBeg; )
		*(dstPtr--) = *(srcPtr--);

	if((U64)dstPtr - 4 >= (U64)dst.ptr && (U64)srcPtr - 4 >= (U64)src.ptr ) {

		*(U32*)dstPtr = *(const U32*)srcPtr;

		dstPtr = (const U64*)((const U32*)dstPtr - 1);
		srcPtr = (const U64*)((const U32*)srcPtr - 1);
	}

	if ((U64)dstPtr - 2 >= (U64)dst.ptr && (U64)srcPtr - 2 >= (U64)src.ptr) {

		*(U16*)dstPtr = *(const U16*)srcPtr;

		dstPtr = (const U64*)((const U16*)dstPtr - 1);
		srcPtr = (const U64*)((const U16*)srcPtr - 1);
	}

	if ((U64)dstPtr - 1 >= (U64)dst.ptr && (U64)srcPtr - 1 >= (U64)src.ptr)
		*(U8*)dstPtr = *(const U8*)srcPtr;
}

//

struct Error Bit_set(struct Buffer buf, U64 offset) {

	if(!buf.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if((offset >> 3) >= buf.siz)
		return (struct Error) {
			.genericError = GenericError_OutOfBounds,
			.paramId = 0,
			.paramValue0 = offset >> 3,
			.paramValue1 = buf.siz
		};

	buf.ptr[offset >> 3] |= 1 << (offset & 7);
	return (struct Error) { 0 };
}

struct Error Bit_reset(struct Buffer buf, U64 offset) {

	if(!buf.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if((offset >> 3) >= buf.siz)
		return (struct Error) {
			.genericError = GenericError_OutOfBounds,
			.paramId = 0,
			.paramValue0 = offset >> 3,
			.paramValue1 = buf.siz
		};

	buf.ptr[offset >> 3] &= ~(1 << (offset & 7));
	return (struct Error) { 0 };
}

#define BitOp(x, dst, src) {															\
																						\
	if(!dst.ptr)																		\
		return (struct Error) {															\
			.genericError = GenericError_NullPointer,									\
			.paramId = 0																\
		};																				\
																						\
	if(!src.ptr)																		\
		return (struct Error) {															\
			.genericError = GenericError_NullPointer,									\
			.paramId = 1																\
		};																				\
																						\
	if(!dst.siz)																		\
		return (struct Error) {															\
			.genericError = GenericError_InvalidParameter,								\
			.paramId = 0,																\
			.paramSubId = 1																\
		};																				\
																						\
	if(!src.siz)																		\
		return (struct Error) {															\
			.genericError = GenericError_InvalidParameter,								\
			.paramId = 1,																\
			.paramSubId = 1																\
		};																				\
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

struct Error Bit_or(struct Buffer dst, struct Buffer src)  BitOp(|=, dst, src)
struct Error Bit_xor(struct Buffer dst, struct Buffer src) BitOp(^=, dst, src)
struct Error Bit_and(struct Buffer dst, struct Buffer src) BitOp(&=, dst, src)
struct Error Bit_not(struct Buffer dst) BitOp(=~, dst, dst)

struct Error Bit_cmp(struct Buffer buf0, struct Buffer buf1, I8 *result) {

	if(!buf0.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if(!buf1.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 1
		};

	if(!result)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 2
		};

	if(!buf0.siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 0,
			.paramSubId = 1
		};

	if(!buf1.siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 1,
			.paramSubId = 1
		};

	*result = 0;

	U64 l = buf0.siz;
	U64 m = buf1.siz;
																					
	for (U64 i = 0, j = l >> 3, k = m >> 3; i < j && i < k; ++i) {

		U64 v0 = *((const U64*)buf0.ptr + i);
		U64 v1 = *((const U64*)buf1.ptr + i);

		if (v0 != v1) {
			*result = v0 < v1 ? -1 : 1;
			return Error_none();
		}
	}
																					
	for (U64 i = l >> 3 << 3, j = m >> 3 << 3; i < l && i < m; ++i)
		if (buf0.ptr[i] != buf1.ptr[i])
			return Error_none();

	return Error_none();
}

struct Error Bit_eq(struct Buffer buf0, struct Buffer buf1, Bool *result) {

	if(buf0.siz != buf1.siz) {

		if(result)
			*result = false;

		return Error_none();
	}

	I8 i = 0;
	struct Error e = Bit_cmp(buf0, buf1, result);

	if(e.genericError)
		return e;

	*result = i == 0;
	return Error_none();
}

struct Error Bit_neq(struct Buffer buf0, struct Buffer buf1, Bool *result) {

	struct Error e = Bit_eq(buf0, buf1, result);

	if(e.genericError)
		return e;

	*result = !*result;
	return Error_none();
}

U64 Bit_hash(struct Buffer buf) {

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

struct Error Bit_setRange(struct Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramSubId = 0
		};

	if(!dst.siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramSubId = 1
		};

	if(((dstOff + bits - 1) >> 3) >= dst.siz)
		return (struct Error) {
			.genericError = GenericError_OutOfBounds,
			.paramId = 1
		};

	if(!bits)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 2
		};

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

	return (struct Error) { 0 };
}

struct Error Bit_unsetRange(struct Buffer dst, U64 dstOff, U64 bits) {

	if(!dst.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramSubId = 0
		};

	if(!dst.siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramSubId = 1
		};

	if(((dstOff + bits - 1) >> 3) >= dst.siz)
		return (struct Error) {
			.genericError = GenericError_OutOfBounds,
			.paramId = 1
		};

	if(!bits)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 2
		};

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

	return (struct Error) { 0 };
}

inline struct Error Bit_setAllToInternal(struct Buffer buf, U64 b64, U8 b8) {

	if(!buf.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer
		};

	if(!buf.siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramSubId = 1
		};

	U64 l = buf.siz;

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)buf.ptr + i) = b64;

	for (U64 i = l >> 3 << 3; i < l; ++i)
		buf.ptr[i] = b8;

	return (struct Error) { 0 };
}

struct Error Bit_setAll(struct Buffer buf) {
	return Bit_setAllToInternal(buf, U64_MAX, U8_MAX);
}

struct Error Bit_unsetAll(struct Buffer buf) {
	return Bit_setAllToInternal(buf, 0, 0);
}

struct Error Bit_alloc(U64 siz, struct Allocator alloc, struct Buffer *result) {

	if(!siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 0
		};

	if(!alloc.alloc)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 1,
			.paramSubId = 1
		};

	if(!result)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 2
		};

	siz = (siz + 7) >> 3;	//Align to bytes

	struct Error err = alloc.alloc(alloc.ptr, siz, result);

	if(err.genericError)
		return err;

	if(!result->ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.errorSubId = 1
		};

	return (struct Error) { 0 };
}

struct Error Bit_createEmpty(U64 siz, struct Allocator alloc, struct Buffer *result) {

	struct Error e = Bit_alloc(siz, alloc, result);

	if(e.genericError)
		return e;

	Bit_unsetAll(*result);
	return (struct Error) { 0 };
}

struct Error Bit_createFull(U64 siz, struct Allocator alloc, struct Buffer *result) {

	struct Error e = Bit_alloc(siz, alloc, result);

	if(e.genericError)
		return e;

	Bit_setAll(*result);
	return (struct Error) { 0 };
}

struct Error Bit_createDuplicate(struct Buffer buf, struct Allocator alloc, struct Buffer *result) {

	if(!buf.ptr || !buf.siz) {
		*result = (struct Buffer) { 0 };
		return (struct Error) { 0 };
	}

	U64 l = buf.siz;
	struct Error e = Bit_alloc(l, alloc, result);

	if(e.genericError)
		return e;

	for(U64 i = 0, j = l >> 3; i < j; ++i)
		*((U64*)result->ptr + i) |= *((const U64*)buf.ptr + i);

	for (U64 i = l >> 3 << 3; i < l; ++i)
		result->ptr[i] |= buf.ptr[i];

	return (struct Error) { 0 };
}

struct Error Bit_free(struct Buffer *buf, struct Allocator alloc) {

	if(!buf)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if(!buf->ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if(!buf->siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 0,
			.paramSubId = 1
		};

	if(!alloc.free)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 1,
			.paramSubId = 2
		};

	alloc.free(alloc.ptr, *buf);
	*buf = (struct Buffer){ 0 };
	return (struct Error) { 0 };
}

struct Error Bit_createEmptyBytes(U64 siz, struct Allocator alloc, struct Buffer *output) {
	return Bit_createEmpty(siz << 3, alloc, output);
}

struct Error Bit_createBytes(U64 siz, struct Allocator alloc, struct Buffer *result) {
	return Bit_alloc(siz << 3, alloc, result);
}

struct Error Bit_offset(struct Buffer *buf, U64 siz) {

	if(!buf)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if(!buf->ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 0
		};

	if(!buf->siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 0,
			.paramSubId = 1
		};

	if(!siz)
		return (struct Error) {
			.genericError = GenericError_InvalidParameter,
			.paramId = 1
		};

	if(siz > buf->siz)
		return (struct Error) {
			.genericError = GenericError_OutOfBounds
		};

	buf->ptr += siz;
	buf->siz -= siz;

	if(!buf->siz)
		*buf = (struct Buffer){ 0 };

	return (struct Error) { 0 };
}

inline void Bit_copyBytes(U8 *ptr, const void *v, U64 siz) {

	for (U64 i = 0, j = siz >> 3; i < j; ++i)
		*((U64*)ptr + i) = *((const U64*)v + i);

	for (U64 i = siz & ~7; i < siz; ++i)
		ptr[i] = *((const U8*)v + i);
}

struct Error Bit_appendBuffer(struct Buffer *buf, struct Buffer append) {

	if(!append.ptr)
		return (struct Error) {
			.genericError = GenericError_NullPointer,
			.paramId = 1
		};

	void *ptr = buf ? buf->ptr : NULL;

	struct Error e = Bit_offset(buf, append.siz);

	if(e.genericError)
		return e;

	Bit_copyBytes(ptr, append.ptr, append.siz);
	return (struct Error) { 0 };
}

struct Error Bit_append(struct Buffer *buf, const void *v, U64 siz) {
	return Bit_appendBuffer(buf, (struct Buffer) { .ptr = (U8*) v, siz });
}

struct Error Bit_createSubset(struct Buffer buf, U64 offset, U64 siz, struct Buffer *output) {

	struct Error e = Bit_offset(&buf, offset);

	if(e.genericError)
		return e;
		
	if(siz >= buf.siz)
		return (struct Error) {
			.genericError = GenericError_OutOfBounds
		};

	buf.siz = siz;
	*output = buf;
	return (struct Error) { 0 };
}
