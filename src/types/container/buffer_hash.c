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

#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/math/vec.h"
#include "types/math/type_cast.h"

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

U32 U32_ror(const U32 v, U32 amount) {
	amount &= 31;								//Avoid undefined behavior (<< 32 is undefined)
	return amount ? ((v >> amount) | (v << (32 - amount))) : v;
}

U32 U32_rol(const U32 v, U32 amount) {
	amount &= 31;								//Avoid undefined behavior (<< 32 is undefined)
	return amount ? ((v << amount) | (v >> (32 - amount))) : v;
}

void Buffer_sha256Internal(Buffer buf, U32 *output) {

	I32x4 state[2] = {
		I32x4_load4(SHA256_STATE),
		I32x4_load4(SHA256_STATE + 4)
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

			const U64 realLen = len;
			const U64 realPtr = ptr;

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

		const I32x4 currState0 = state[0], currState1 = state[1];

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

					const U32 wj1 = w[(j + 1) & 0xF], wj14 = w[(j + 14) & 0xF];

					const U32 s0 = U32_ror(wj1, 7) ^ U32_ror(wj1, 18) ^ (wj1 >> 3);
					const U32 s1 = U32_ror(wj14, 17) ^ U32_ror(wj14, 19) ^ (wj14 >> 10);

					w[j] += s0 + w[(j + 9) & 0xF] + s1;
				}

				//Calculate s1 and ch

				const U32 ah4 = (U32) I32x4_x(state[1]);
				const U32 ah5 = (U32) I32x4_y(state[1]);
				const U32 ah6 = (U32) I32x4_z(state[1]);

				const U32 s1 = U32_ror(ah4, 6) ^ U32_ror(ah4, 11) ^ U32_ror(ah4, 25);
				const U32 ch = (ah4 & ah5) ^ (~ah4 & ah6);

				//Calculate temp1 and temp2

				const U32 ah0 = (U32) I32x4_x(state[0]);
				const U32 ah1 = (U32) I32x4_y(state[0]);
				const U32 ah2 = (U32) I32x4_z(state[0]);

				const U32 temp1 = (U32) I32x4_w(state[1]) + s1 + ch + SHA256_K[(i << 4) | j] + w[j];
				const U32 s0 = U32_ror(ah0, 2) ^ U32_ror(ah0, 13) ^ U32_ror(ah0, 22);
				const U32 maj = (ah0 & ah1) ^ (ah0 & ah2) ^ (ah1 & ah2);
				const U32 temp2 = s0 + maj;

				//Swizzle

				const U32 state0w = I32x4_w(state[0]);

				state[0] = I32x4_xxyz(state[0]);
				state[1] = I32x4_xxyz(state[1]);

				I32x4_setX(&state[0], (I32)(temp1 + temp2));
				I32x4_setX(&state[1], (I32)(state0w + temp1));
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

//Ported from https://github.com/krisprice/simd_md5/blob/master/simd_md5/md5_sse.c#L9
//But removed SIMD, since it was incredibly (and I can't stress this enough!) badly done. Found locally that:
//	Their SIMD version does 1GiB in 52s (Debug) and 22s (Release)
//	My non-SIMD version does 1GiB in 12s (Debug) and 4s (Release).
//	It is clear that due to the data dependencies in MD5, it's just not a good fit for SSE.
//	Moving to and from SIMD registers and wasting cycles computing data that is duplicated makes no sense.
//	The only way is if we compute multiple MD5 hashes of different data at the same time.
//	Or if we could split up in blocks (impossible with MD5).

//This is layed out as constants per step per round.
//As each step per round has their different constants.

static const U32 MD5_consts[][16] = {
	{
		0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
		0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
	},
	{
		0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
		0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
	},
	{
		0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
		0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
	},
	{
		0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
		0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391
	}
};
static const U8 MD5_rotateLeft[][4] = {
	{  7, 12, 17, 22 },
	{  5,  9, 14, 20 },
	{  4, 11, 16, 23 },
	{  6, 10, 15, 21 }
};

static const U8 MD5_offsets[][16] = {
	{ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },		//i
	{ 1,  6, 11,  0,  5, 10, 15,  4,  9, 14,  3,  8, 13,  2,  7, 12 },		//(1 + 5 * i) & 15
	{ 5,  8, 11, 14,  1,  4,  7, 10, 13,  0,  3,  6,  9, 12, 15,  2 },		//(5 + 3 * i) & 15
	{ 0,  7, 14,  5, 12,  3, 10,  1,  8, 15,  6, 13,  4, 11,  2,  9 }		//(0 + 7 * i) & 15
};

typedef union MD5State {
	I32x4 vec;
	U32 v[4];
} MD5State;

void MD5State_update(MD5State *stateOut, Buffer buf) {

	MD5State state = *stateOut;
	U32 data[16];
	Buffer_copy(Buffer_createRef(data, sizeof(data)), buf);

	//This contains all rounds.
	//Since j and i are constant, it will magically unroll for us.
	//No need to complicate this any further.

	for (U8 j = 0; j < 4; ++j) {

		for (U8 i = 0; i < 16; ++i) {

			U32 f = data[MD5_offsets[j][i]] + MD5_consts[j][i];

			U32 a = state.v[(16 - i) & 3];
			U32 b = state.v[(17 - i) & 3];
			U32 c = state.v[(18 - i) & 3];
			U32 d = state.v[(19 - i) & 3];

			U32 e;

			switch (j) {
				case 0:		e = (b & c) | ((~b) & d);	break;
				case 1:		e = (b & d) | ((~d) & c);	break;
				case 2:		e = b ^ c ^ d;				break;
				default:	e = c ^ (b | (~d));			break;
			}

			a = a + e + f;
			a = U32_rol(a, MD5_rotateLeft[j][i & 3]);
			state.v[(16 - i) & 3] = a + b;
		}
	}

	for(U8 i = 0; i < 4; ++i)
		stateOut->v[i] += state.v[i];
}

void Buffer_md5Generic(Buffer buf, MD5State *state) {

	//Create state for first perfectly filled blocks

	*state = (MD5State) { .v = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 } };

	U64 bufLen = Buffer_length(buf);
	U64 lastBlock = bufLen >> 6;

	for(U64 i = 0; i < lastBlock; ++i)
		MD5State_update(state, Buffer_createRefConst(buf.ptr + (i << 6), 64));
}

I32x4 Buffer_md5(Buffer buf) {

	MD5State state;
	Buffer_md5Generic(buf, &state);

	//Final block is located with padding of \x80 and \0s filling the remaining space.
	//After that, we have a U8 bit count.

	U64 bufLen = Buffer_length(buf);
	U64 lastBlock = bufLen >> 6;
	U64 blocks = (bufLen + 63) >> 6;

	U8 tmp[64] = { 0 };
	U8 off = 0;

	if(lastBlock != blocks) {
		Buffer bufTmp = Buffer_createRefConst(buf.ptr + (lastBlock << 6), (off += (bufLen & 63)));
		Buffer_copy(Buffer_createRef(tmp, 64), bufTmp);
	}

	tmp[off] = (U8)'\x80';

	if (off >= 56) {
		MD5State_update(&state, Buffer_createRefConst(tmp, 64));
		Buffer_unsetAllBits(Buffer_createRefConst(tmp, 64));
	}

	*(U64*)(tmp + 56) = bufLen << 3;
	MD5State_update(&state, Buffer_createRefConst(tmp, 64));

	//Finish

	for(U8 i = 0; i < 4; ++i)
		state.v[i] = U32_swapEndianness(state.v[i]);

	return state.vec;
}

U64 Buffer_fnv1a64Single(U64 a, U64 hash) {
	return (a ^ hash) * Buffer_fnv1a64Prime;
}

U64 Buffer_fnv1a64(Buffer buf, U64 hash) {

	U64 len = Buffer_length(buf);
	U64 off = 0;

	for(; off < (len &~ 7); off += sizeof(U64)) {
		U64 tmp = Buffer_readU64(buf, off, NULL);
		hash = Buffer_fnv1a64Single(tmp, hash);
	}

	if (len & 4) {
		U32 tmp = Buffer_readU32(buf, off, NULL);
		hash = Buffer_fnv1a64Single(tmp, hash);
		off += 4;
	}

	if (len & 2) {
		U16 tmp = Buffer_readU16(buf, off, NULL);
		hash = Buffer_fnv1a64Single(tmp, hash);
		off += 2;
	}

	if (len & 1)
		hash = Buffer_fnv1a64Single(buf.ptr[off], hash);

	return hash;
}
