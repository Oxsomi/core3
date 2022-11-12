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

	for(; dstPtr > dstBeg && srcPtr > srcBeg; ) {
		--srcPtr; --dstPtr;
		*dstPtr = *srcPtr;
	}

	dstBeg = (U64*) dstPtr;
	srcBeg = (const U64*) srcPtr;

	if((U64)dstPtr - 4 >= (U64)dst.ptr && (U64)srcPtr - 4 >= (U64)src.ptr ) {

		dstPtr = (U64*)((U32*)dstPtr - 1);
		srcPtr = (const U64*)((const U32*)srcPtr - 1);

		*(U32*)dstPtr = *(const U32*)srcPtr;
	}

	if ((U64)dstPtr - 2 >= (U64)dst.ptr && (U64)srcPtr - 2 >= (U64)src.ptr) {

		dstPtr = (U64*)((U16*)dstPtr - 1);
		srcPtr = (const U64*)((const U16*)srcPtr - 1);

		*(U16*)dstPtr = *(const U16*)srcPtr;
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

//CRC32 (0xDEBB20E3)
//Source code: https://web.mit.edu/freebsd/head/sys/libkern/crc32.c
//Better explanation: https://lxp32.github.io/docs/a-simple-example-crc32-calculation/
//This table basically performs the inner loop:
//for(U64 j = 0; j < 8; ++j)
//	U32 b = (c8 ^ crc) & 1
//	crc >>= 1
//	if(b) crc ^= 0xEDB88320;
//	crc >>= 1

static const U32 crc32Table[] = {
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
	0xE963A535, 0x9E6495A3,	0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
	0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
	0xF3B97148, 0x84BE41DE,	0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,	0x14015C4F, 0x63066CD9,
	0xFA0F3D63, 0x8D080DF5,	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,	0x35B5A8FA, 0x42B2986C,
	0xDBBBC9D6, 0xACBCF940,	0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
	0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,	0x76DC4190, 0x01DB7106,
	0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
	0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
	0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
	0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
	0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
	0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
	0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
	0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
	0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
	0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
	0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
	0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
	0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
	0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
	0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
	0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
	0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
	0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
	0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
	0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
	0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

U32 Buffer_crc32(Buffer buf, U32 crcSeed) {

	const U8 *p = buf.ptr;
	U32 crc = U32_MAX;

	while(buf.siz--)
		crc = crc32Table[(U8)(crc ^ *p++)] ^ (crc >> 8);

	return crc ^ U32_MAX;
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
