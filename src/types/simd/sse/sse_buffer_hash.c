/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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
#include "types/vec.h"
#include "types/platform_types.h"
#include <nmmintrin.h>

extern const U32 SHA256_STATE[8];

void Buffer_sha256Internal(Buffer buf, U32 *output);

//Implementation of hardware CRC32C but ported back to C and restructured a bit
//https://github.com/rurban/smhasher/blob/master/crc32c.cpp

extern const U32 CRC32C_LONG_SHIFTS[4][256];
extern const U32 CRC32C_SHORT_SHIFTS[4][256];

U32 CRC32C_shiftShort(U32 crc0) {
	return
		CRC32C_SHORT_SHIFTS[0][(U8) crc0] ^ CRC32C_SHORT_SHIFTS[1][(U8)(crc0 >> 8)] ^
		CRC32C_SHORT_SHIFTS[2][(U8)(crc0 >> 16)] ^ CRC32C_SHORT_SHIFTS[3][(U8)(crc0 >> 24)];
}

U32 CRC32C_shiftLong(U32 crc0) {
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

static I8 hasSHA256 = -1;

void Buffer_sha256(Buffer buf, U32 output[8]) {

	if(!output)
		return;

	//Fallback if the hardware doesn't support SHA extension

	if(hasSHA256 < 0) {
		U32 cpuInfo1[4];
		Platform_getCPUId(7, cpuInfo1);
		hasSHA256 = (cpuInfo1[1] >> 29) & 1;
	}

	if(!hasSHA256) {
		Buffer_sha256Internal(buf, output);
		return;
	}

	//Consts

	const I32x4 MASK = I32x4_createFromU64x2(0x0405060700010203, 0x0C0D0E0F08090A0B);

	const I32x4 ROUNDS[16] = {
		I32x4_createFromU64x2(0x71374491428A2F98, 0xE9B5DBA5B5C0FBCF),		//0-3
		I32x4_createFromU64x2(0x59F111F13956C25B, 0xAB1C5ED5923F82A4),		//4-7
		I32x4_createFromU64x2(0x12835B01D807AA98, 0x550C7DC3243185BE),		//8-11
		I32x4_createFromU64x2(0x80DEB1FE72BE5D74, 0xC19BF1749BDC06A7),		//12-15
		I32x4_createFromU64x2(0xEFBE4786E49B69C1, 0x240CA1CC0FC19DC6),		//16-19
		I32x4_createFromU64x2(0x4A7484AA2DE92C6F, 0x76F988DA5CB0A9DC),		//20-23
		I32x4_createFromU64x2(0xA831C66D983E5152, 0xBF597FC7B00327C8),		//24-27
		I32x4_createFromU64x2(0xD5A79147C6E00BF3, 0x1429296706CA6351),		//28-31
		I32x4_createFromU64x2(0x2E1B213827B70A85, 0x53380D134D2C6DFC),		//32-35
		I32x4_createFromU64x2(0x766A0ABB650A7354, 0x92722C8581C2C92E),		//36-39
		I32x4_createFromU64x2(0xA81A664BA2BFE8A1, 0xC76C51A3C24B8B70),		//40-43
		I32x4_createFromU64x2(0xD6990624D192E819, 0x106AA070F40E3585),		//44-47
		I32x4_createFromU64x2(0x1E376C0819A4C116, 0x34B0BCB52748774C),		//48-51
		I32x4_createFromU64x2(0x4ED8AA4A391C0CB3, 0x682E6FF35B9CCA4F),		//52-55
		I32x4_createFromU64x2(0x78A5636F748F82EE, 0x8CC7020884C87814),		//56-59
		I32x4_createFromU64x2(0xA4506CEB90BEFFFA, 0xC67178F2BEF9A3F7)		//60-63
	};

	//Initialize state

	I32x4 tmp = I32x4_yxwz(I32x4_load4((const I32*) SHA256_STATE));				//_mm_shuffle_epi32(tmp, 0xB1)
	I32x4 state1 = I32x4_wzyx(I32x4_load4((const I32*) SHA256_STATE + 4));		//_mm_shuffle_epi32(state1, 0x1B)
	I32x4 state0 =	I32x4_combineRightShift(tmp, state1, 2);					//_mm_alignr_epi8(tmp, state1, 8);
	state1 = I32x4_blend(state1, tmp, 0b1100);									//_mm_blend_epi16(state1, tmp, 0xF0)

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
					Buffer_createRefConst((const void*)realPtr, realLen)
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
			msgs[i] = I32x4_shuffleBytes(I32x4_load4((const I32*)(void*)ptr + i * 4), MASK);

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

			tmp = I32x4_combineRightShift(msgs[(i + 3) & 3], msgs[(i + 2) & 3], 1);		//_mm_alignr_epi8(a, b, 4)
			I32x4 msgTmp = I32x4_add(msgs[i & 3], tmp);
			msgs[i & 3] = _mm_sha256msg2_epu32(msgTmp, msgs[(i + 3) & 3]);
			msg = I32x4_zwxx(msg);														//_mm_shuffle_epi32(msg, 0xE);
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

	tmp = I32x4_wzyx(state0);							//_mm_shuffle_epi32(state0, 0x1B)
	state1 = I32x4_yxwz(state1);						//_mm_shuffle_epi32(state1, 0xB1)
	state0 = I32x4_blend(tmp, state1, 0b1100);			//_mm_blend_epi16(tmp, state1, 0xF0);
	state1 = I32x4_combineRightShift(state1, tmp, 2);	//_mm_alignr_epi8(a, b, 2)

	//Store output

	*(I32x4*) output = state0;
	*((I32x4*)output + 1) = state1;
}