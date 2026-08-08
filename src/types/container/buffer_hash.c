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

//types/container/buffer_hash.c

#include "types/container/buffer.h"
#include "types/math/vec4i_swizzle.h"
#include "types/math/type_cast.h"
#include "types/base/endianness.h"
#include "types/base/constants.h"

//SHA state

const U32 SHA256_STATE[8] = {
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

static inline U32 sigma0(U32 x) {
	return (U32_ror(x, 7) ^ U32_ror(x, 18)) ^ (x >> 3);
}

static inline U32 sigma1(U32 x) {
	return (U32_ror(x, 17) ^ U32_ror(x, 19)) ^ (x >> 10);
}

static inline U32 Sigma0(U32 x) {
	return U32_ror(x, 2) ^ U32_ror(x, 13) ^ U32_ror(x, 22);
}

static inline U32 Sigma1(U32 x) {
	return U32_ror(x, 6) ^ U32_ror(x, 11) ^ U32_ror(x, 25);
}

static inline U32 Ch(U32 x, U32 y, U32 z) {
	return (x & y) ^ (~x & z);
}

static inline U32 Maj(U32 x, U32 y, U32 z) {
	return (x & y) ^ (x & z) ^ (y & z);
}

//TODO: Replace with U32x4_add if available
//(W0, W1, W2, W3) = a
//(W4) = bx
//return (W0, W1, W2, W3) + (W1, W2, W3, W4)
static inline I32x4 I32x4_sha256msg1(I32x4 a, I32x4 b) {
	U32 W0 = (U32) I32x4_x(a), W1 = (U32)I32x4_y(a), W2 = (U32)I32x4_z(a), W3 = (U32)I32x4_w(a);
	U32 W4 = (U32) I32x4_x(b);
	return I32x4_create4(
		(I32)(W0 + sigma0(W1)),
		(I32)(W1 + sigma0(W2)),
		(I32)(W2 + sigma0(W3)),
		(I32)(W3 + sigma0(W4))
	);
}

//TODO: Replace with U32x4_add if available
//(W14, 15) = zw(b)
//(W16, W17) = xy(a) + sigma1(W14, 15)
//(W18, W19) = zw(a) + sigma1(W16, W17)
//a = (W16, W17, W18, W19)
static inline I32x4 I32x4_sha256msg2(I32x4 i0, I32x4 i2, I32x4 i3) {

	I32x4 tmp = I32x4_combineRightShift(i3, i2, 1);        //_mm_alignr_epi8(a, b, 4)
	I32x4 msgTmp = I32x4_add(i0, tmp);

	I32x4 a = msgTmp;
	I32x4 b = i3;

	U32 W14 = (U32)I32x4_z(b), W15 = (U32)I32x4_w(b);

	U32 W16 = (U32)I32x4_x(a) + sigma1(W14);
	U32 W17 = (U32)I32x4_y(a) + sigma1(W15);

	return I32x4_create4(
		(I32)W16,
		(I32)W17,
		(I32)((U32)I32x4_z(a) + sigma1(W16)),
		(I32)((U32)I32x4_w(a) + sigma1(W17))
	);
}

//Literal port from https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=sha256rnds2
static inline I32x4 I32x4_sha256rnds2(I32x4 a, I32x4 b, I32x4 k) {

	U32 A = (U32)I32x4_w(b);
	U32 B = (U32)I32x4_z(b);
	U32 C = (U32)I32x4_w(a);
	U32 D = (U32)I32x4_z(a);

	U32 E = (U32)I32x4_y(b);
	U32 F = (U32)I32x4_x(b);
	U32 G = (U32)I32x4_y(a);
	U32 H = (U32)I32x4_x(a);

	for (U8 i = 0; i < 2; ++i) {

		U32 Wki = (U32)I32x4_get(k, i);

		U32 T = Ch(E, F, G) + Sigma1(E) + Wki + H;
		U32 A1 = T + Maj(A, B, C) + Sigma0(A);
		U32 E1 = T + D;

		U32 H1 = G;
		U32 G1 = F;
		U32 F1 = E;

		U32 D1 = C;
		U32 C1 = B;
		U32 B1 = A;

		A = A1; B = B1; C = C1; D = D1;
		E = E1; F = F1; G = G1; H = H1;
	}

	return I32x4_create4(F, E, B, A);
}

static inline void I32x4_sha256rnds4(I32x4 msgOriginal, I32x4 round, I32x4 *state0, I32x4 *state1) {
	I32x4 msg = I32x4_add(msgOriginal, round);
	*state1 = I32x4_sha256rnds2(*state1, *state0, msg);
	msg = I32x4_zwxx(msg);                                                        //_mm_shuffle_epi32(msg, 0xE);
	*state0 = I32x4_sha256rnds2(*state0, *state1, msg);
}

#define SIMD_SHA256_LINKING
#define SIMD_SHA256_RNDS4 I32x4_sha256rnds4
#define SIMD_SHA256_MSG1 I32x4_sha256msg1
#define SIMD_SHA256_MSG2 I32x4_sha256msg2
#define SIMD_SHA256_SUFFIX(x) x##Fallback
#include "types/container/simd/buffer_simd_sha.inc.h"

//Fallback CRC32 implementation

//CRC32C ported from:
//https://github.com/rurban/smhasher/blob/master/crc32c.cpp

extern const U32 CRC32C_TABLE[16][256];

U32 Buffer_crc32cFallbackChained(const Buffer buf, U32 prevCrc) {

	U64 crc = prevCrc ^ U32_MAX;

	const U64 bufLen = Buffer_length(buf);

	if(!bufLen)
		return (U32) crc ^ U32_MAX;

	U64 len = bufLen;
	U64 it = (U64)(void*)buf.ptr;
	const U64 align8 = U64_min(it + len, (it + 7) & ~7);

	while (it < align8) {
		crc = CRC32C_TABLE[0][(U8)(crc ^ *(const U8*)it)] ^ (crc >> 8);
		++it;
		--len;
	}

	while (len >= sizeof(U64) * 2) {

		crc ^= *(const U64*) it;
		const U64 next = *((const U64*) it + 1);

		U64 res = 0;

		for(U64 i = 0; i < sizeof(U64); ++i)        //Compiler will unroll for us
			res ^= CRC32C_TABLE[15 - i][(U8)(crc >> (i * 8))];

		for(U64 i = 0; i < sizeof(U64); ++i)        //Compiler will unroll for us
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

//Ported from https://github.com/krisprice/simd_md5/blob/master/simd_md5/md5_sse.c#L9
//But removed SIMD, since it was incredibly (and I can't stress this enough!) badly done.
//Found locally that:
//    Their SIMD version does 1GiB in 52s (Debug) and 22s (Release)
//    My non-SIMD version does 1GiB in 12s (Debug) and 4s (Release).
//    It is clear that due to the data dependencies in MD5, it's just not a good fit for SSE.
//    Moving to and from SIMD registers and wasting cycles computing data that is duplicated makes no sense.
//    The only way is if we compute multiple MD5 hashes of different data at the same time.
//    Or if we could split up in blocks (impossible with MD5).

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
	{ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },        //i
	{ 1,  6, 11,  0,  5, 10, 15,  4,  9, 14,  3,  8, 13,  2,  7, 12 },        //(1 + 5 * i) & 15
	{ 5,  8, 11, 14,  1,  4,  7, 10, 13,  0,  3,  6,  9, 12, 15,  2 },        //(5 + 3 * i) & 15
	{ 0,  7, 14,  5, 12,  3, 10,  1,  8, 15,  6, 13,  4, 11,  2,  9 }        //(0 + 7 * i) & 15
};

typedef union MD5State {
	I32x4 vec;
	U32 v[4];
} MD5State;

static inline void MD5State_update(MD5State *stateOut, const Buffer buf) {

	MD5State state = *stateOut;
	U32 data[16];
	Buffer_memcpy(Buffer_createRef(data, sizeof(data)), buf);

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
				case 0:        e = (b & c) | ((~b) & d);    break;
				case 1:        e = (b & d) | ((~d) & c);    break;
				case 2:        e = b ^ c ^ d;                break;
				default:    e = c ^ (b | (~d));            break;
			}

			a = a + e + f;
			a = U32_rol(a, MD5_rotateLeft[j][i & 3]);
			state.v[(16 - i) & 3] = a + b;
		}
	}

	for(U8 i = 0; i < 4; ++i)
		stateOut->v[i] += state.v[i];
}

static inline void Buffer_md5Generic(const Buffer buf, MD5State *state) {

	//Create state for first perfectly filled blocks

	*state = (MD5State) { .v = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 } };

	U64 bufLen = Buffer_length(buf);
	U64 lastBlock = bufLen >> 6;

	for(U64 i = 0; i < lastBlock; ++i)
		MD5State_update(state, Buffer_createRefConst(buf.ptr + (i << 6), 64));
}

MD5Hash Buffer_md5(const Buffer buf) {

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
		Buffer_memcpy(Buffer_createRef(tmp, 64), bufTmp);
	}

	tmp[off] = (U8)'\x80';

	if (off >= 56) {
		MD5State_update(&state, Buffer_createRefConst(tmp, 64));
		Buffer_unsetAllBits(Buffer_createRefConst(tmp, 64), NULL);
	}

	*(U64*)(tmp + 56) = bufLen << 3;
	MD5State_update(&state, Buffer_createRefConst(tmp, 64));

	//Finish

	MD5Hash hash = (MD5Hash) { 0 };

	for(U8 i = 0; i < 4; ++i)
		hash.v[i] = U32_swapEndianness(state.v[i]);

	return hash;
}

U64 Buffer_fnv1a64Single(U64 a, U64 hash) {
	return (a ^ hash) * Buffer_fnv1a64Prime;
}

U64 Buffer_fnv1a64(const Buffer buf, U64 hash) {

	U64 len = Buffer_length(buf);
	U64 off = 0;

	for(; off < (len &~ 7); off += sizeof(U64)) {
		U64 tmp = Buffer_readU64(buf, off, NULL, NULL);
		hash = Buffer_fnv1a64Single(tmp, hash);
	}

	if (len & 4) {
		U32 tmp = Buffer_readU32(buf, off, NULL, NULL);
		hash = Buffer_fnv1a64Single(tmp, hash);
		off += 4;
	}

	if (len & 2) {
		U16 tmp = Buffer_readU16(buf, off, NULL, NULL);
		hash = Buffer_fnv1a64Single(tmp, hash);
		off += 2;
	}

	if (len & 1)
		hash = Buffer_fnv1a64Single(buf.ptr[off], hash);

	return hash;
}
