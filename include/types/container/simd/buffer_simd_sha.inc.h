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

//types/container/simd/buffer_simd_sha.inc.h

//SHA256
//Ported + cleaned up from https://github.com/noloader/SHA-Intrinsics/blob/master/sha256-x86.c

#include "types/container/log.h"

#ifndef SIMD_SHA256_RNDS4
	#error Define SIMD_SHA256_RNDS4 for this architecture (2x _mm_sha256rnds2_epu32)
#endif

#ifndef SIMD_SHA256_MSG1
	#error Define SIMD_SHA256_MSG1 for this architecture; e.g. _mm_sha256msg1_epu32
#endif

#ifndef SIMD_SHA256_MSG2
	#error Define SIMD_SHA256_MSG2 for this architecture; e.g. _mm_sha256msg2_epu32
#endif

#ifndef SIMD_SHA256_SUFFIX
	#error Define SIMD_SHA256_SUFFIX for this architecture; e.g. #define SIMD_SHA256_SUFFIX(x) x##SSE
#endif

#ifndef SIMD_SHA256_LINKING
	#define SIMD_SHA256_LINKING static inline
#endif

extern const U32 SHA256_STATE[8];

SIMD_SHA256_LINKING void SIMD_SHA256_SUFFIX(Buffer_sha256)(const Buffer buf, U32 *output)  {

	//Consts

	const I32x4 MASK = I32x4_createFromU64x2(0x0405060700010203, 0x0C0D0E0F08090A0B);

	const I32x4 ROUNDS[16] = {
		I32x4_createFromU64x2(0x71374491428A2F98, 0xE9B5DBA5B5C0FBCF),        //0-3
		I32x4_createFromU64x2(0x59F111F13956C25B, 0xAB1C5ED5923F82A4),        //4-7
		I32x4_createFromU64x2(0x12835B01D807AA98, 0x550C7DC3243185BE),        //8-11
		I32x4_createFromU64x2(0x80DEB1FE72BE5D74, 0xC19BF1749BDC06A7),        //12-15
		I32x4_createFromU64x2(0xEFBE4786E49B69C1, 0x240CA1CC0FC19DC6),        //16-19
		I32x4_createFromU64x2(0x4A7484AA2DE92C6F, 0x76F988DA5CB0A9DC),        //20-23
		I32x4_createFromU64x2(0xA831C66D983E5152, 0xBF597FC7B00327C8),        //24-27
		I32x4_createFromU64x2(0xD5A79147C6E00BF3, 0x1429296706CA6351),        //28-31
		I32x4_createFromU64x2(0x2E1B213827B70A85, 0x53380D134D2C6DFC),        //32-35
		I32x4_createFromU64x2(0x766A0ABB650A7354, 0x92722C8581C2C92E),        //36-39
		I32x4_createFromU64x2(0xA81A664BA2BFE8A1, 0xC76C51A3C24B8B70),        //40-43
		I32x4_createFromU64x2(0xD6990624D192E819, 0x106AA070F40E3585),        //44-47
		I32x4_createFromU64x2(0x1E376C0819A4C116, 0x34B0BCB52748774C),        //48-51
		I32x4_createFromU64x2(0x4ED8AA4A391C0CB3, 0x682E6FF35B9CCA4F),        //52-55
		I32x4_createFromU64x2(0x78A5636F748F82EE, 0x8CC7020884C87814),        //56-59
		I32x4_createFromU64x2(0xA4506CEB90BEFFFA, 0xC67178F2BEF9A3F7)        //60-63
	};

	//Initialize state

	I32x4 tmp = I32x4_yxwz(I32x4_load4((const I32*) SHA256_STATE));                //_mm_shuffle_epi32(tmp, 0xB1)
	I32x4 state1 = I32x4_wzyx(I32x4_load4((const I32*) SHA256_STATE + 4));        //_mm_shuffle_epi32(state1, 0x1B)
	I32x4 state0 =    I32x4_combineRightShift(tmp, state1, 2);                    //_mm_alignr_epi8(tmp, state1, 8);
	state1 = I32x4_blend(state1, tmp, 0b1100);                                    //_mm_blend_epi16(state1, tmp, 0xF0)

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

		//We reached an unfilled block.
		//We gotta make a block on the stack and point to it

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

				Buffer_unsetAllBits(Buffer_createRef(block, 64 - sizeof(U64)), NULL);

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

				realLen &= 63; //Always true here (len < 64), but helps GCC's -Warray-bounds prove it

				Buffer_memcpy(
					Buffer_createRef(block, 64),
					Buffer_createRefConst((const void*)realPtr, realLen)
				);

				block[realLen] = 0x80;

				if(realLen <= 62)
					Buffer_unsetAllBits(Buffer_createRef(block + realLen + 1, 64 - realLen - 1), NULL);

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

			SIMD_SHA256_RNDS4(msgs[i], ROUNDS[i], &state0, &state1);

			if(i)
				msgs[i - 1] = SIMD_SHA256_MSG1(msgs[i - 1], msgs[i]);
		}

		//12 iterations; Rounds 12-59

		for(U64 i = 0; i < 12; ++i) {
			SIMD_SHA256_RNDS4(msgs[(i + 3) & 3], ROUNDS[i + 3], &state0, &state1);
			msgs[i & 3] = SIMD_SHA256_MSG2(msgs[i & 3], msgs[(i + 2) & 3], msgs[(i + 3) & 3]);
			msgs[(i + 2) & 3] = SIMD_SHA256_MSG1(msgs[(i + 2) & 3], msgs[(i + 3) & 3]);
		}

		//1 iteration; Rounds 60-63

		SIMD_SHA256_RNDS4(msgs[3], ROUNDS[15], &state0, &state1);

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

	tmp = I32x4_wzyx(state0);                            //_mm_shuffle_epi32(state0, 0x1B)
	state1 = I32x4_yxwz(state1);                        //_mm_shuffle_epi32(state1, 0xB1)
	state0 = I32x4_blend(tmp, state1, 0b1100);            //_mm_blend_epi16(tmp, state1, 0xF0);
	state1 = I32x4_combineRightShift(state1, tmp, 2);    //_mm_alignr_epi8(a, b, 2)

	//Store output

	for(U8 i = 0; i < 4; ++i)
		output[i] = (U32) I32x4_get(state0, i);

	for(U8 i = 0; i < 4; ++i)
		output[4 + i] = (U32) I32x4_get(state1, i);

	Buffer_clearAllSecure(Buffer_createRef(block, sizeof(block)));
}
