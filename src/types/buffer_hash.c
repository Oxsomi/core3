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

//SHA state

const U32 SHA256_STATE[8] = {
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

U32 ror(U32 v, U32 amount) {
	amount &= 31;								//Avoid undefined behavior (<< 32 is undefined)
	return amount ? ((v >> amount) | (v << (32 - amount))) : v;
}

void Buffer_sha256Internal(Buffer buf, U32 *output) {

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

				Buffer_copy(Buffer_createRef(block, 64), Buffer_createRefConst((const void*)realPtr, realLen));

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

	Buffer_copy(Buffer_createRef(output, sizeof(U32) * 8), Buffer_createRefConst(state, sizeof(U32) * 8));
}
