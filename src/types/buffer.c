#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/type_cast.h"
#include "math/math.h"
#include "math/vec.h"

#include <intrin.h>

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
		*(U8*)dstPtr = *(const U8*)srcPtr;

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

	if(length >> 48 || (U64)v >> 48)		//Invalid addresses (unsupported by CPUs)
		return Buffer_createNull();

	return (Buffer) { .ptr = (U8*) v, .lengthAndRefBits = length | ((U64)1 << 63) };
}

Buffer Buffer_createConstRef(const void *v, U64 length) { 

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

//SHA state

static const U32 SHA256_STATE[8] = {
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

//First 32 bits of cube roots of first 64 primes 2..311 (see amosnier/sha-2)

static const U32 SHA256_K[64] = {
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5, 
	0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174, 
	0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA, 
	0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967, 
	0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85, 
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070, 
	0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
	0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

//Fallback for SHA256 if the native instruction isn't available
//Thanks to https://codereview.stackexchange.com/questions/182812/self-contained-sha-256-implementation-in-c
//https://github.com/amosnier/sha-2/blob/master/sha-256.c
//https://en.wikipedia.org/wiki/SHA-2

//Arm7's ror instruction aka Java's >>>; shift that maintains right side of the bits into left side

inline U32 ror(U32 v, U32 amount) {
	amount &= 31;								//Avoid undefined behavior (<< 32 is undefined)
	return amount ? ((v >> amount) | (v << (32 - amount))) : v;
}

inline void Buffer_sha256Internal(Buffer buf, U32 *output) {

	I32x4 state[2] = {
		I32x4_load4((const I32*)SHA256_STATE),
		I32x4_load4((const I32*)SHA256_STATE + 4)
	};

	U64 ptr = (U64)(void*) buf.ptr, len = Buffer_length(buf);
	U8 block[64];

	Bool padded = false;
	Bool wasPaddingBlock = false;
	Bool wasPerfectlyAligned = false;

	if(!len) {
		wasPaddingBlock = wasPerfectlyAligned = true;
		len = 64;
	}

	while (len) {

		//We reached an unfilled block. We gotta make a block on the stack and point to it

		if (len < 64 || wasPaddingBlock) {

			//Point to stack

			U64 realLen = len;
			U64 realPtr = ptr;

			ptr = (U64)(void*) block;
			len = 64;
			padded = true;

			//Last block was padding block
			//This means we can put the length at the end

			if (wasPaddingBlock) {

				Buffer_unsetAllBits(Buffer_createRef(block, 64 - sizeof(U64)));

				if(wasPerfectlyAligned)
					block[0] = 0x80;

				U8 *lenPtr = block + 64 - sizeof(U64);

				//Keep 5 bits from the index in the block at lenPtr[7]
				//Keep the others as big endian in [0-6]

				U64 currLen = Buffer_length(buf);

				lenPtr[7] = (U8)(currLen << 3);
				currLen >>= 5;

				for(U64 k = 6; k != U64_MAX; --k) {
					lenPtr[k] = (U8) currLen;
					currLen >>= 8;
				}
			}

			//We reached the block with 0x80, 0x00....
			//With possibly length in the end

			else {

				Buffer_copy(Buffer_createRef(block, 64), Buffer_createConstRef((const void*)realPtr, realLen));

				*((U8*)(void*)block + realLen) = 0x80;

				if(realLen <= 62)
					Buffer_unsetAllBits(Buffer_createRef(block + realLen + 1, 64 - realLen - 1));

				//We need one more block just to contain the length at the end

				if(realLen >= 64 - sizeof(U64)) {
					wasPaddingBlock = true;
					len += 64;
				}

				//We can insert length in this block :)

				else {

					U8 *lenPtr = block + 64 - sizeof(U64);

					U64 currLen = Buffer_length(buf);

					//Keep 5 bits from the index in the block at lenPtr[7]
					//Keep the others as big endian in [0-6]

					lenPtr[7] = (U8)(currLen << 3);
					currLen >>= 5;

					for(U64 k = 6; k != U64_MAX; --k) {
						lenPtr[k] = (U8) currLen;
						currLen >>= 8;
					}
				}
			}
		}

		//Store state

		I32x4 currState0 = state[0], currState1 = state[1];

		//Perform SHA256

		U32 w[16];

		for(U64 i = 0; i < 4; ++i)
			for (U64 j = 0; j < 16; ++j) {

				//Initialize w

				if(!i) {

					U32 v = 0;

					for(U64 k = 0; k < 4; ++k)
						v |= ((U32)*(U8*)(void*)(ptr + (j << 2) + k)) << (24 - k * 8);

					w[j] = v;
				}

				//Scramble w

				else {

					U32 wj1 = w[(j + 1) & 0xF], wj14 = w[(j + 14) & 0xF];

					U32 s0 = ror(wj1, 7) ^ ror(wj1, 18) ^ (wj1 >> 3);
					U32 s1 = ror(wj14, 17) ^ ror(wj14, 19) ^ (wj14 >> 10);

					w[j] += s0 + w[(j + 9) & 0xF] + s1;
				}

				//Calculate s1 and ch

				U32 ah4 = (U32) I32x4_x(state[1]);
				U32 ah5 = (U32) I32x4_y(state[1]);
				U32 ah6 = (U32) I32x4_z(state[1]);

				U32 s1 = ror(ah4, 6) ^ ror(ah4, 11) ^ ror(ah4, 25);
				U32 ch = (ah4 & ah5) ^ (~ah4 & ah6);

				//Calculate temp1 and temp2

				U32 ah0 = (U32) I32x4_x(state[0]);
				U32 ah1 = (U32) I32x4_y(state[0]);
				U32 ah2 = (U32) I32x4_z(state[0]);

				U32 temp1 = (U32) I32x4_w(state[1]) + s1 + ch + SHA256_K[(i << 4) | j] + w[j];
				U32 s0 = ror(ah0, 2) ^ ror(ah0, 13) ^ ror(ah0, 22);
				U32 maj = (ah0 & ah1) ^ (ah0 & ah2) ^ (ah1 & ah2);
				U32 temp2 = s0 + maj;
				
				//Swizzle

				U32 state0w = I32x4_w(state[0]);

				state[0] = I32x4_xxyz(state[0]);
				state[1] = I32x4_xxyz(state[1]);

				I32x4_setX(&state[0], temp1 + temp2);
				I32x4_setX(&state[1], state0w + temp1);
			}

		//Combine two states

		state[0] = I32x4_add(state[0], currState0);
		state[1] = I32x4_add(state[1], currState1);

		//Update block pos

		ptr += 64;
		len -= 64;

		//If we haven't padded and length is zero, that means we're perfectly aligned!
		//This means we're still missing a padding U64 for the encoded length

		if (!len && !padded) {
			wasPerfectlyAligned = true;
			wasPaddingBlock = true;
			len = 64;
		}
	}

	//Store output

	Buffer_copy(Buffer_createRef(output, sizeof(U32) * 8), Buffer_createConstRef(state, sizeof(U32) * 8));
}

//

#if _SIMD == SIMD_SSE

	#include <nmmintrin.h>

	//Implementation of hardware CRC32C but ported back to C and restructured a bit
	//https://github.com/rurban/smhasher/blob/master/crc32c.cpp

	extern const U32 CRC32C_LONG_SHIFTS[4][256];
	extern const U32 CRC32C_SHORT_SHIFTS[4][256];

	inline U32 CRC32C_shiftShort(U32 crc0) {
		return 
			CRC32C_SHORT_SHIFTS[0][(U8) crc0] ^ CRC32C_SHORT_SHIFTS[1][(U8)(crc0 >> 8)] ^ 
			CRC32C_SHORT_SHIFTS[2][(U8)(crc0 >> 16)] ^ CRC32C_SHORT_SHIFTS[3][(U8)(crc0 >> 24)];
	}

	inline U32 CRC32C_shiftLong(U32 crc0) {
		return 
			CRC32C_LONG_SHIFTS[0][(U8) crc0] ^ CRC32C_LONG_SHIFTS[1][(U8)(crc0 >> 8)] ^ 
			CRC32C_LONG_SHIFTS[2][(U8)(crc0 >> 16)] ^ CRC32C_LONG_SHIFTS[3][(U8)(crc0 >> 24)];
	}

	U32 Buffer_crc32c(Buffer buf) {

		U32 crc = U32_MAX;
	
		U64 bufLen = Buffer_length(buf);

		if(!bufLen)
			return crc ^ U32_MAX;

		U64 off = (U64)(void*)buf.ptr;
		
		U64 len = bufLen;
		U64 offNear8 = U64_min((U64)buf.ptr + bufLen, (off + 7) & ~7);

		//Go to nearest 8 byte boundary

		if (off & 7) {

			if (off + sizeof(U32) <= offNear8) {
				crc = _mm_crc32_u32(crc, *(U32*)(void*)off);
				off += sizeof(U32);
				len -= sizeof(U32);
			}

			if (off + sizeof(U16) <= offNear8) {
				crc = _mm_crc32_u16(crc, *(U16*)(void*)off);
				off += sizeof(U16);
				len -= sizeof(U16);
			}

			if (off + sizeof(U8) <= offNear8) {
				crc = _mm_crc32_u8(crc, *(U8*)(void*)off);
				off += sizeof(U8);
				len -= sizeof(U8);
			}
		}

		//Compute 3 CRCs at once, this is beneficial because the hardware is able to schedule these at once
		//Giving us better perf (up to 15x faster!)
		//We always run 64-bit, so we can assume U64 to be properly present

		U64 crc0 = crc;

		static const U64 LONG_SHIFT = 8192, LONG_SHIFT_U64 = 8192 / sizeof(U64);

		while(len >= LONG_SHIFT * 3) {

			U64 crc1 = 0, crc2 = 0;

			for (U64 i = 0; i < LONG_SHIFT_U64; ++i) {
				crc0 = _mm_crc32_u64(crc0, *(U64*)(void*)off + i);
				crc1 = _mm_crc32_u64(crc1, *((U64*)(void*)off + LONG_SHIFT_U64 + i));
				crc2 = _mm_crc32_u64(crc2, *((U64*)(void*)off + LONG_SHIFT_U64 * 2 + i));
			}

			crc0 = CRC32C_shiftLong((U32) crc0) ^ crc1;
			crc0 = CRC32C_shiftLong((U32) crc0) ^ crc2;

			off += LONG_SHIFT * 3;
			len -= LONG_SHIFT * 3;
		}

		//Do the same thing but for smaller blocks

		static const U64 SHORT_SHIFT = 256, SHORT_SHIFT_U64 = 256 / sizeof(U64);

		while(len >= SHORT_SHIFT * 3) {

			U64 crc1 = 0, crc2 = 0;

			for (U64 i = 0; i < SHORT_SHIFT_U64; ++i) {
				crc0 = _mm_crc32_u64(crc0, *(U64*)(void*)off + i);
				crc1 = _mm_crc32_u64(crc1, *((U64*)(void*)off + SHORT_SHIFT_U64 + i));
				crc2 = _mm_crc32_u64(crc2, *((U64*)(void*)off + SHORT_SHIFT_U64 * 2 + i));
			}

			crc0 = CRC32C_shiftShort((U32) crc0) ^ crc1;
			crc0 = CRC32C_shiftShort((U32) crc0) ^ crc2;

			off += SHORT_SHIFT * 3;
			len -= SHORT_SHIFT * 3;
		}

		//Process remaining U64s

		while (len >= sizeof(U64)) {
			crc0 = _mm_crc32_u64(crc0, *(U64*)(void*)off);
			off += sizeof(U64);
			len -= sizeof(U64);
		}

		if (len >= sizeof(U32)) {
			crc0 = _mm_crc32_u32((U32) crc0, *(U32*)(void*)off);
			off += sizeof(U32);
			len -= sizeof(U32);
		}

		if (len >= sizeof(U16)) {
			crc0 = _mm_crc32_u16((U32) crc0, *(U16*)(void*)off);
			off += sizeof(U16);
			len -= sizeof(U16);
		}

		if (len >= sizeof(U8))
			crc0 = _mm_crc32_u8((U32) crc0, *(U8*)(void*)off);

		return (U32) crc0 ^ U32_MAX;
	}

	//Ported + cleaned up from https://github.com/noloader/SHA-Intrinsics/blob/master/sha256-x86.c

	void Buffer_sha256(Buffer buf, U32 output[8]) {

		if(!output)
			return;

		//Fallback if the hardware doesn't support SHA extension

		int cpuInfo1[4];
		__cpuidex(cpuInfo1, 7, 0);

		if(!((cpuInfo1[1] >> 29) & 1)) {
			Buffer_sha256Internal(buf, output);
			return;
		}

		//Consts

		const I32x4 MASK = _mm_set_epi64x(0x0C'0D'0E'0F'08'09'0A'0B, 0x04'05'06'07'00'01'02'03);

		const I32x4 ROUNDS[16] = {
			_mm_set_epi64x(0xE9B5DBA5B5C0FBCF, 0x71374491428A2F98),		//0-3
			_mm_set_epi64x(0xAB1C5ED5923F82A4, 0x59F111F13956C25B),		//4-7
			_mm_set_epi64x(0x550C7DC3243185BE, 0x12835B01D807AA98),		//8-11
			_mm_set_epi64x(0xC19BF1749BDC06A7, 0x80DEB1FE72BE5D74),		//12-15
			_mm_set_epi64x(0x240CA1CC0FC19DC6, 0xEFBE4786E49B69C1),		//16-19
			_mm_set_epi64x(0x76F988DA5CB0A9DC, 0x4A7484AA2DE92C6F),		//20-23
			_mm_set_epi64x(0xBF597FC7B00327C8, 0xA831C66D983E5152),		//24-27
			_mm_set_epi64x(0x1429296706CA6351, 0xD5A79147C6E00BF3),		//28-31
			_mm_set_epi64x(0x53380D134D2C6DFC, 0x2E1B213827B70A85),		//32-35
			_mm_set_epi64x(0x92722C8581C2C92E, 0x766A0ABB650A7354),		//36-39
			_mm_set_epi64x(0xC76C51A3C24B8B70, 0xA81A664BA2BFE8A1),		//40-43
			_mm_set_epi64x(0x106AA070F40E3585, 0xD6990624D192E819),		//44-47
			_mm_set_epi64x(0x34B0BCB52748774C, 0x1E376C0819A4C116),		//48-51
			_mm_set_epi64x(0x682E6FF35B9CCA4F, 0x4ED8AA4A391C0CB3),		//52-55
			_mm_set_epi64x(0x8CC7020884C87814, 0x78A5636F748F82EE),		//56-59
			_mm_set_epi64x(0xC67178F2BEF9A3F7, 0xA4506CEB90BEFFFA)		//60-63
		};

		//Initialize state

		I32x4 tmp = I32x4_yxwz(I32x4_load4((const I32*) SHA256_STATE));			//_mm_shuffle_epi32(tmp, 0xB1)
		I32x4 state1 = I32x4_wzyx(I32x4_load4((const I32*) SHA256_STATE + 4));	//_mm_shuffle_epi32(state1, 0x1B)
		I32x4 state0 = _mm_alignr_epi8(tmp, state1, 8);
		state1 = _mm_blend_epi16(state1, tmp, 0xF0);

		//64-byte blocks

		U64 ptr = (U64)(void*) buf.ptr, len = Buffer_length(buf);
		U8 block[64];

		Bool padded = false;
		Bool wasPaddingBlock = false;
		Bool wasPerfectlyAligned = false;

		if(!len) {
			wasPaddingBlock = wasPerfectlyAligned = true;
			len = 64;
		}

		while(len) {

			//We reached an unfilled block. We gotta make a block on the stack and point to it

			if (len < 64 || wasPaddingBlock) {

				//Point to stack

				U64 realLen = len;
				U64 realPtr = ptr;

				ptr = (U64)(void*) block;
				len = 64;
				padded = true;

				//Last block was padding block
				//This means we can put the length at the end

				if (wasPaddingBlock) {

					Buffer_unsetAllBits(Buffer_createRef(block, 64 - sizeof(U64)));

					if(wasPerfectlyAligned)
						block[0] = 0x80;

					U8 *lenPtr = block + 64 - sizeof(U64);

					//Keep 5 bits from the index in the block at lenPtr[7]
					//Keep the others as big endian in [0-6]

					U64 currLen = Buffer_length(buf);

					lenPtr[7] = (U8)(currLen << 3);
					currLen >>= 5;

					for(U64 k = 6; k != U64_MAX; --k) {
						lenPtr[k] = (U8) currLen;
						currLen >>= 8;
					}
				}

				//We reached the block with 0x80, 0x00....
				//With possibly length in the end

				else {

					Buffer_copy(
						Buffer_createRef(block, 64), 
						Buffer_createConstRef((const void*)realPtr, realLen)
					);

					*((U8*)(void*)block + realLen) = 0x80;

					if(realLen <= 62)
						Buffer_unsetAllBits(Buffer_createRef(block + realLen + 1, 64 - realLen - 1));

					//We need one more block just to contain the length at the end

					if(realLen >= 64 - sizeof(U64)) {
						wasPaddingBlock = true;
						len += 64;
					}

					//We can insert length in this block :)

					else {

						U8 *lenPtr = block + 64 - sizeof(U64);

						U64 currLen = Buffer_length(buf);

						//Keep 5 bits from the index in the block at lenPtr[7]
						//Keep the others as big endian in [0-6]

						lenPtr[7] = (U8)(currLen << 3);
						currLen >>= 5;

						for(U64 k = 6; k != U64_MAX; --k) {
							lenPtr[k] = (U8) currLen;
							currLen >>= 8;
						}
					}
				}
			}

			//Store previous state

			I32x4 currState0 = state0, currState1 = state1;
			
			//Init messages

			I32x4 msgs[4];

			for(U64 i = 0; i < 4; ++i)
				msgs[i] = _mm_shuffle_epi8(I32x4_load4((const I32*)(void*)ptr + i * 4), MASK);

			//3 iterations: Rounds 0-11 (two different iterations; 0-3, 4-7, 8-11)

			for (U64 i = 0; i < 3; ++i) {

				I32x4 msg = I32x4_add(msgs[i], ROUNDS[i]);
				state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
				msg = I32x4_zwxx(msg);										//_mm_shuffle_epi32(msg, 0xE);
				state0 = _mm_sha256rnds2_epu32(state0, state1, msg);

				if(i)
					msgs[i - 1] = _mm_sha256msg1_epu32(msgs[i - 1], msgs[i]);
			}

			//12 iterations; Rounds 12-59

			for(U64 i = 0; i < 12; ++i) {

				I32x4 msg = I32x4_add(msgs[(i + 3) & 3], ROUNDS[i + 3]);
				state1 = _mm_sha256rnds2_epu32(state1, state0, msg);

				tmp = _mm_alignr_epi8(msgs[(i + 3) & 3], msgs[(i + 2) & 3], 4);
				I32x4 msgTmp = I32x4_add(msgs[i & 3], tmp);
				msgs[i & 3] = _mm_sha256msg2_epu32(msgTmp, msgs[(i + 3) & 3]);
				msg = I32x4_zwxx(msg);										//_mm_shuffle_epi32(msg, 0xE);
				state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
				msgs[(i + 2) & 3] = _mm_sha256msg1_epu32(msgs[(i + 2) & 3], msgs[(i + 3) & 3]);
			}

			//1 iteration; Rounds 60-63

			I32x4 msg = I32x4_add(msgs[3], ROUNDS[15]);
			state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
			msg = I32x4_zwxx(msg);	
			state0 = _mm_sha256rnds2_epu32(state0, state1, msg);

			//Combine state

			state0 = I32x4_add(state0, currState0);
			state1 = I32x4_add(state1, currState1);

			//Next block

			len -= 64;
			ptr += 64;

			//If we haven't padded and length is zero, that means we're perfectly aligned!
			//This means we're still missing a padding U64 for the encoded length

			if (!len && !padded) {
				wasPerfectlyAligned = true;
				wasPaddingBlock = true;
				len = 64;
			}
		}

		//Post process 

		tmp = I32x4_wzyx(state0);			//_mm_shuffle_epi32(state0, 0x1B)
		state1 = I32x4_yxwz(state1);		//_mm_shuffle_epi32(state1, 0xB1)
		state0 = _mm_blend_epi16(tmp, state1, 0xF0);
		state1 = _mm_alignr_epi8(state1, tmp, 8);

		//Store output

		*(I32x4*) output = state0;
		*((I32x4*)output + 1) = state1;
	}

	//Key expansion for AES256
	//Implemented from the official intel AES-NI paper + Additional paper by S. Gueron appendix A
	//https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf
	//https://link.springer.com/content/pdf/10.1007/978-3-642-03317-9_4.pdf
	//https://www.samiam.org/key-schedule.html

	inline I32x4 aesExpandKey1(I32x4 im1, I32x4 im2) {

		im2 = I32x4_wwww(im2);

		I32x4 im4 = im1;

		for(U8 i = 0; i < 3; ++i) {
			im4 = _mm_slli_si128(im4, 0x4);
			im1 = I32x4_xor(im1, im4);
		}

		return I32x4_xor(im1, im2);
	}

	inline I32x4 aesExpandKey2(I32x4 im1, I32x4 im3) {

		I32x4 im4 = _mm_aeskeygenassist_si128(im1, 0x0);
		I32x4 im2 = I32x4_zzzz(im4);

		im4 = im3;

		for(U8 i = 0; i < 3; ++i) {
			im4 = _mm_slli_si128(im4, 0x4);
			im3 = I32x4_xor(im3, im4);
		}

		return I32x4_xor(im3, im2);
	}

	inline void aesExpandKey(const U32 key[8], I32x4 k[15]) {

		I32x4 im1 = (k[0] = *(const I32x4*) key);
		I32x4 im2;
		I32x4 im3 = (k[1] = ((const I32x4*) key)[1]);

		for (U8 i = 0, j = 2; i < 7; ++i, j += 2) {

			//Unfortunately can't do 1 << i. It needs constexpr

			switch (i) {
				case 0:		im2 = _mm_aeskeygenassist_si128(im3, 0x01);	break;
				case 1:		im2 = _mm_aeskeygenassist_si128(im3, 0x02);	break;
				case 2:		im2 = _mm_aeskeygenassist_si128(im3, 0x04);	break;
				case 3:		im2 = _mm_aeskeygenassist_si128(im3, 0x08);	break;
				case 4:		im2 = _mm_aeskeygenassist_si128(im3, 0x10);	break;
				case 5:		im2 = _mm_aeskeygenassist_si128(im3, 0x20);	break;
				default:	im2 = _mm_aeskeygenassist_si128(im3, 0x40);	break;
			}

			k[j] = (im1 = aesExpandKey1(im1, im2));

			if(j + 1 < 15)
				k[j + 1] = (im3 = aesExpandKey2(im1, im3));
		}
	}

	//Aes block encryption. Don't use this plainly, it's a part of the larger AES256-CTR algorithm

	inline I32x4 aesBlock(I32x4 block, const I32x4 k[15]) {

		block = I32x4_xor(block, k[0]);

		for(U8 i = 1; i < 14; ++i)				//AES256 uses 14 rounds
			block = _mm_aesenc_si128(block, k[i]);

		return _mm_aesenclast_si128(block, k[14]);
	}

	#define _I32x4_rsh(n, a)										\
																	\
		I32x4 mask = I32x4_create4(0, 0, (1 << n) - 1, 0);			\
																	\
		/* Shift to left, but gets rid of U64[1]'s lower N bits */	\
																	\
		I32x4 b = _mm_srli_epi64(a, n);								\
																	\
		/* Mask the lost bits */									\
																	\
		I32x4 lost = I32x4_and(a, mask);							\
		lost = _mm_slli_epi64(I32x4_zwxy(lost), 64 - n);			\
																	\
		/* Combine lost bits */										\
																	\
		return I32x4_or(b, lost)									\

	inline I32x4 I32x4_rsh4(I32x4 a) { _I32x4_rsh(4, a); }
	inline I32x4 I32x4_rsh1(I32x4 a) { _I32x4_rsh(1, a); }

	Bool Buffer_csprng(Buffer target) {

		if(!Buffer_length(target) || Buffer_isConstRef(target))
			return false;

		U8 *ptr = target.ptr;
		U64 len = Buffer_length(target);

		while(len) {

			U64 rng = 0;
			while(!_rdrand64_step(&rng)) { }

			U64 siz = 1;

			if(len >= sizeof(U64)) {
				*(U64*)ptr = rng;
				siz = sizeof(U64);
			}

			else if(len >= sizeof(U32)) {
				*(U32*)ptr = (U32) rng;
				siz = sizeof(U32);
			}

			else if(len >= sizeof(U16)) {
				*(U16*)ptr = (U16) rng;
				siz = sizeof(U16);
			}

			else {
				*(U8*)ptr = (U8) rng;
				siz = sizeof(U8);
			}

			ptr += siz;
			len -= siz;
		}

		return true;
	}

#elif _SIMD == SIMD_NEON

	#error Neon currently unsupported for native CRC32C, AES256 or SHA256 operation

#else

	//Fallback CRC32 implementation

	//CRC32C ported from:
	//https://github.com/rurban/smhasher/blob/master/crc32c.cpp

	extern const U32 CRC32C_TABLE[16][256];

	U32 Buffer_crc32c(Buffer buf) {

		U64 crc = U32_MAX;

		U64 bufLen = Buffer_length(buf);

		if(!bufLen)
			return (U32) crc ^ U32_MAX;

		U64 len = bufLen;
		U64 it = (U64)(void*)buf.ptr;
		U64 align8 = U64_min(it + len, (it + 7) & ~7);

		while (it < align8) {
			crc = CRC32C_TABLE[0][(U8)(crc ^ *(const U8*)it)] ^ (crc >> 8);
			++it;
			--len;
		}

		while (len >= sizeof(U64) * 2) {

			crc ^= *(const U64*) it;
			U64 next = *((const U64*) it + 1);

			U64 res = 0;

			for(U64 i = 0; i < sizeof(U64); ++i)		//Compiler will unroll for us
				res ^= CRC32C_TABLE[15 - i][(U8)(crc >> (i * 8))];

			for(U64 i = 0; i < sizeof(U64); ++i)		//Compiler will unroll for us
				res ^= CRC32C_TABLE[7 - i][(U8)(next >> (i * 8))];

			crc = res;

			it += sizeof(U64) * 2;
			len -= sizeof(U64) * 2;
		}

		while (len > 0) {
			crc = CRC32C_TABLE[0][(U8)(crc ^ *(const U8*)it)] ^ (crc >> 8);
			++it;
			--len;
		}

		return (U32)crc ^ U32_MAX;
	}

	void Buffer_sha256(Buffer buf, U32 output[8]) {

		if(!output)
			return;

		Buffer_sha256Internal(buf, output);
	}

	inline void aesExpandKey(U32 key[8], I32x4 k[15]);
	inline I32x4 aesBlock(I32x4 block, I32x4 k[15]);
	inline I32x4 I32x4_rsh4(I32x4 a);

#endif

//ghash computes the Galois field multiplication GF(2^128) with the current H (hash of AES256 encrypted zero block)
//for AES256 GCM + GMAC

//LUT creation from https://github.com/mko-x/SharedAES-GCM/blob/master/Sources/gcm.c#L207

inline void ghashPrepare(I32x4 H, I32x4 ghashLut[16]) {

	H = I32x4_swapEndianness(H);

	ghashLut[0] = I32x4_zero();		//0 = 0 in GF(2^128)
	ghashLut[8] = H;				//8 (0b1000) corresponds to 1 in GF (2^128)

	for (U8 i = 4; i > 0; i >>= 1) {

		I32x4 T = I32x4_create4(0, 0, 0, I32x4_x(H) & 1 ? 0xE1000000 : 0);
		H = I32x4_rsh1(H);
		H = I32x4_xor(H, T);

		ghashLut[i] = H;
	}

	for (U8 i = 2; i < 16; i <<= 1) {

		H = ghashLut[i];

		for(U8 j = 1; j < i; ++j)
			ghashLut[j + i] = I32x4_xor(H, ghashLut[j]);
	}
}

//Implemented and optimized to SSE from https://github.com/mko-x/SharedAES-GCM/blob/master/Sources/gcm.c#L131

const U16 GHASH_LAST4[16] = {
	0x0000, 0x1C20, 0x3840, 0x2460, 0x7080, 0x6CA0, 0x48C0, 0x54E0,
	0xE100, 0xFD20, 0xD940, 0xC560, 0x9180, 0x8DA0, 0xA9C0, 0xB5E0
};

inline I32x4 ghash(I32x4 a, const I32x4 ghashLut[16]) {

	U8 lo = ((U8*)&a)[15] & 0xF;
	U8 hi = ((U8*)&a)[15] >> 4;

	I32x4 zlZh = ghashLut[lo];

	for(U8 i = 15; i != U8_MAX; i--) {

		if(i != 15) {

			U8 rem = (U8)I32x4_x(zlZh) & 0xF;

			zlZh = I32x4_rsh4(zlZh);
			zlZh = I32x4_xor(zlZh, I32x4_create4(0, 0, 0, (U32)GHASH_LAST4[rem] << 16));

			zlZh = I32x4_xor(zlZh, ghashLut[lo]);
		}

		U8 rem = (U8)I32x4_x(zlZh) & 0xF;
		zlZh = I32x4_rsh4(zlZh);

		zlZh = I32x4_xor(zlZh, I32x4_create4(0, 0, 0, (U32)GHASH_LAST4[rem] << 16));

		zlZh = I32x4_xor(zlZh, ghashLut[hi]);

		if(!i)
			break;

		lo = ((U8*)&a)[i - 1] & 0x0f;
		hi = ((U8*)&a)[i - 1] >> 4;
	}

	return I32x4_swapEndianness(zlZh);
}

//Explanation of algorithm; AES256 GCM + GMAC
//https://www.alexeyshmalko.com/20200319144641/ / https://www.youtube.com/watch?v=V2TlG3JbGp0
//https://www.youtube.com/watch?v=g_eY7JXOc8U
//
//The final algorithm is basically the following:
//
//- Init H: aes256(0, key)
//- Init GHASH table
//
//- Tag: 0
//- Foreach additional data block padded to 16-byte with 0s:
//	- tag = GHASH(tag XOR additional data block)
// 
//- IV (Initial vector) = Generate CSPRNG of 12-bytes
//- Store iv in result
// 
//- Foreach plaindata block at i padded to 16-byte with 0s:
//	- Eki = encrypt(IV append U32BE(i + 2))
//	- store (cyphertext[i] = plainText[i] XOR Eki) in result
//	- tag = GHASH(tag XOR cyphertext[i])
// 
//- tag = GHASH(combine(U64BE(additionalDataBits), U64BE(plainTextBits)) XOR tag)
//- tag = tag XOR aes256(IV with U32BE(1) appended)
// 
//- Store tag in result
//
//For "encrypt" we use AES CTR as explained by the intel paper:
//https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf
//
inline Error aes256Encrypt(
	Buffer target, 
	Buffer additionalData,
	const U32 realKey[8], 
	const I32x4 *ivPtr,
	Allocator alloc,
	Buffer *output
) {

	//Validate inputs

	if(!output)
		return Error_nullPointer(3, 0);

	if(output->ptr)
		return Error_invalidParameter(3, 0, 0);

	if(!realKey)
		return Error_nullPointer(1, 0);

	//Since we have a 12-byte IV, we have a 4-byte block counter.
	//This block counter runs out in (4Gi - 2) * sizeof(Block) aka ~4Gi * 16 = ~64GiB.
	//When the IV block counter runs out it would basically repeat the same block xor pattern again.
	//-2 because we start at 2 since 1 is used at the end for verification (and 0 is skipped).

	if(Buffer_length(target) > (4 * GIBI - 2) * sizeof(I32x4))
		return Error_unsupportedOperation(0);

	//Create output (IV, Cypher text, tag)

	Error err = Buffer_createUninitializedBytes(
		12 +										//Iv
		Buffer_length(target) +						//Cipher text
		16,											//Tag
		alloc, 
		output
	);

	if(err.genericError)
		return err;

	//Get key that's gonna be used for aes blocks

	I32x4 key[15];
	aesExpandKey(realKey, key);

	I32x4 H = aesBlock(I32x4_zero(), key);

	I32x4 ghashLut[16];
	ghashPrepare(H, ghashLut);

	//Generate iv

	I32x4 iv = I32x4_zero();

	if(!ivPtr)
		Buffer_csprng(Buffer_createRef(&iv, 12));

	else Buffer_copy(
		Buffer_createRef(&iv, 12),
		Buffer_createConstRef(ivPtr, 12)
	);

	//Compute initial tag

	I32x4 Y0 = iv;
	I32x4_setW(&Y0, I32_swapEndianness(1));

	I32x4 EKY0 = aesBlock(Y0, key);

	//Hash in the additional data
	//This could be something like sender + receiver ip address
	//This data could allow the dev to discard invalid packets for example
	//And verify that this is the data the original message was signed with

	I32x4 tag = I32x4_zero();

	for (U64 i = 0, j = (Buffer_length(additionalData) + 15) >> 4; i < j; ++i) {

		I32x4 ADi;	//Additional data

		//Avoid out of bounds read, by simply filling additional data by zero

		if (i == j - 1 && Buffer_length(additionalData) & 15) {

			ADi = I32x4_zero();
			Buffer_copy(
				Buffer_createRef(&ADi, sizeof(ADi)),
				Buffer_createConstRef(
					(const I32x4*)additionalData.ptr + i, 
					Buffer_length(additionalData) & 15
				)
			);
		}

		else ADi = ((const I32x4*)additionalData.ptr)[i];

		tag = ghash(I32x4_xor(tag, ADi), ghashLut);
	}

	//Prepend iv

	U8 *outputPtr = output->ptr;

	Buffer_copy(Buffer_createRef(outputPtr, 12), Buffer_createConstRef(&iv, 12));
	outputPtr += 12;

	//Handle blocks
	//TODO: We might wanna multithread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	U64 targetLen = Buffer_length(target), targetLeftOver = targetLen & 15;

	for (U64 i = 0, j = (targetLen + 15) >> 4; i < j; ++i) {

		I32x4 PTi;	//Plain text

		//Avoid out of bounds read, by simply filling additional data by zero

		if (i == j - 1 && targetLeftOver) {

			PTi = I32x4_zero();
			Buffer_copy(
				Buffer_createRef(&PTi, sizeof(PTi)),
				Buffer_createConstRef(
					(const I32x4*)target.ptr + i, 
					targetLeftOver
				)
			);
		}

		else PTi = ((const I32x4*)target.ptr)[i];

		//Encrypt IV + blockId

		I32x4 ivi = iv;
		I32x4_setW(&ivi, I32_swapEndianness((U32)i + 2));

		I32x4 CTi = I32x4_xor(PTi, aesBlock(ivi, key));

		//A special property of unaligned blocks is that the bytes that are added as padding
		//shouldn't be stored and so they have to be zero-ed in CTi, otherwise the tag will mess up

		if (i == j - 1 && targetLeftOver) {

			Buffer_unsetAllBits(Buffer_createRef((U8*)&CTi + targetLeftOver, sizeof(I32x4) - targetLeftOver));

			Buffer_copy(
				Buffer_createRef(outputPtr, targetLeftOver), 
				Buffer_createConstRef(&CTi, sizeof(CTi))
			);

			outputPtr += targetLeftOver;
		}

		//Aligned blocks

		else {
			*(I32x4*)outputPtr = CTi;
			outputPtr += sizeof(I32x4);
		}

		tag = ghash(I32x4_xor(CTi, tag), ghashLut);
	}

	//Add length of inputs into the message too (lengths are in bits)

	I32x4 lengths = I32x4_zero();
	*(U64*)&lengths = U64_swapEndianness(Buffer_length(additionalData) << 3);
	*((U64*)&lengths + 1) = U64_swapEndianness(Buffer_length(target) << 3);

	tag = ghash(I32x4_xor(tag, lengths), ghashLut);

	//Finish up by adding the iv into the key (this is already has blockId 1 in it)

	tag = I32x4_xor(tag, EKY0);

	//Finish encryption by appending tag for authentication / verification that the data isn't messed with

	*(I32x4*)outputPtr = tag;

	return Error_none();
}

Error Buffer_encrypt(
	Buffer target, 
	Buffer additionalData,
	BufferEncryptionType type, 
	const U32 key[8], 
	const I32x4 *iv,
	Allocator alloc, 
	Buffer *output
) {

	switch (type) {

		case BufferEncryptionType_AES256GCM:
			return aes256Encrypt(target, additionalData, key, iv, alloc, output);

		default:
			return Error_invalidEnum(1, 0, (U64) type, BufferEncryptionType_Count);
	}
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
