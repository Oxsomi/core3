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

//types/math/pack.h

#pragma once
#include "types/math/quat.h"
#include "types/math/flp.h"
#include "types/math/vec4i.h"
#include "types/base/fixed_point.h"
#include "types/math/vec4f.h"
#include "types/base/constants.h"

#ifdef __cplusplus
	extern "C" {
#endif

//U64 packing

static inline U64 U64_pack21x3(U32 x, U32 y, U32 z) {

	if ((x >> 21) || (y >> 21) || (z >> 21))
		return U64_MAX;                                    //Returns U64_MAX on invalid

	return x | ((U64)y << 21) | ((U64)z << 42);
}

static inline Bool U64_pack20x3u4(U64 *dst, U32 x, U32 y, U32 z, U8 u4) {

	if (!dst || (x >> 20) || (y >> 20) || (z >> 20) || (u4 >> 4))
		return false;

	*dst = x | ((U64)y << 20) | ((U64)z << 40) | ((U64)u4 << 60);
	return true;
}

static inline U32 U64_unpack21x3(U64 packed, U8 off) {

	if ((packed >> 63) || (off >= 3))
		return U32_MAX;                                    //Returns U32_MAX on invalid

	return (U32)((packed >> (21 * off)) & ((1 << 21) - 1));
}

static inline U32 U64_unpack20x3u4(U64 packed, U8 off) {

	if (off > 3)
		return U32_MAX;                                    //Returns U32_MAX on invalid

	if (off == 3)
		return packed >> 60;

	return (U32)((packed >> (20 * off)) & ((1 << 20) - 1));
}

static inline Bool U64_setPacked20x3u4(U64 *packed, U8 off, U32 v) {

	if (off == 3) {

		if(!packed || v >> 4)
			return false;

		*packed &= ~((U64)0xF << 60);
		*packed |= (U64)v << 60;
		return true;
	}

	if (v >> 20 || off > 3 || !packed)
		return false;

	off *= 20;
	*packed &= ~((U64)((1 << 20) - 1) << off);        //Reset bits
	*packed |= (U64)v << off;                        //Set bits
	return true;
}

static inline Bool U64_setPacked21x3(U64 *packed, U8 off, U32 v) {

	if (v >> 21 || off >= 3 || !packed)
		return false;

	off *= 21;
	*packed &= ~((U64)((1 << 21) - 1) << off);        //Reset bits
	*packed |= (U64)v << off;                        //Set bits
	return true;
}

#define GET_BIT_OP(T)                                       \
static inline Bool T##_getBit(T packed, U8 off) {           \
															\
	if (off >= sizeof(T) * 8)                               \
		return false;                                       \
															\
	return (packed >> off) & 1;                             \
}                                                           \
															\
static inline Bool T##_setBit(T *packed, U8 off, Bool b) {  \
															\
	if (off >= sizeof(T) * 8)                               \
		return false;                                       \
															\
	T shift = (T)1 << off;                                  \
															\
	if (b)                                                  \
		*packed |= shift;                                   \
															\
	else *packed &= ~shift;                                 \
															\
	return true;                                            \
}

GET_BIT_OP(U64);
GET_BIT_OP(U32);
GET_BIT_OP(U16);
GET_BIT_OP(U8);

//UF21x3

//Three UF21 in the 64 bits of an RG32u texel, the layout @pack.hlsli reads and writes:
// x in bits 0..20, y ACROSS 21..41, z in 42..62, and bit 63 spare.
//An accumulator wants exactly this. F16's 10-bit mantissa cannot hold a progressive mean:
// once the increment falls under half an ulp the downward corrections round away while the upward ones still land,
// and a non-negative heavy-tailed signal then drifts UP by tens of percent.
//UF21 keeps a wider exponent than F16 both ways, so neither an emissive value nor a deep-shadow one leaves the range.
//
//flp.c truncates ties where the shader rounds to nearest even, so an exact tie can differ by one ulp between the two.
//Nothing else does; every other value round trips bit for bit.

static inline U64 U64_packF21x3(F32x4 v) {

	const U64 x = F32_castUF21(F32_max(0, F32x4_x(v)));
	const U64 y = F32_castUF21(F32_max(0, F32x4_y(v)));
	const U64 z = F32_castUF21(F32_max(0, F32x4_z(v)));

	return x | (y << 21) | (z << 42);
}

static inline F32x4 F32x4_unpackF21x3(U64 packed) {
	return F32x4_create3(
		UF21_castF32((U32) ( packed        & 0x1FFFFF)),
		UF21_castF32((U32) ((packed >> 21) & 0x1FFFFF)),
		UF21_castF32((U32) ((packed >> 42) & 0x1FFFFF))
	);
}

//RGB9E5

//DXGI_FORMAT_R9G9B9E5_SHAREDEXP, the layout @pack.hlsli reads:
// r | g<<9 | b<<18 | (e+15)<<27, scale 2^(e-24).
//Three 9-bit mantissas over one shared 5-bit exponent, 32 bits for an HDR triple whose channels sit within a few
// stops of each other, emission, irradiance, a probe.
//Channels far apart lose the dim ones, since they all pay for the brightest one's exponent.
//
//Written out rather than approximated: a value written here is read back by the shader through unpackRGB9E5,
// so a disagreement between the two is a BIAS rather than a rounding error.

static inline U32 U32_packRGB9E5(F32x4 v) {

	const F32 maxV = (511.0f / 512) * 65536;               //((2^9-1)/2^9) * 2^(31-15)

	const F32 r = F32_clamp(F32x4_x(v), 0, maxV);
	const F32 g = F32_clamp(F32x4_y(v), 0, maxV);
	const F32 b = F32_clamp(F32x4_z(v), 0, maxV);

	const F32 maxC = F32_max(r, F32_max(g, b));

	I32 e = maxC > 0 ? (I32) F32_floor(F32_log2(maxC)) + 1 : -15;

	if (e < -15)
		e = -15;

	F32 denom = F32_exp2((F32)(e - 9));

	//A mantissa that rounds up to 2^9 needs one more exponent step, so re-derive once

	if ((I32) F32_floor(maxC / denom + 0.5f) >= 512) {
		denom *= 2;
		++e;
	}

	return
		(U32) F32_floor(r / denom + 0.5f) |
		((U32) F32_floor(g / denom + 0.5f) << 9) |
		((U32) F32_floor(b / denom + 0.5f) << 18) |
		((U32) (e + 15) << 27);
}

static inline F32x4 F32x4_unpackRGB9E5(U32 packed) {

	const F32 scale = F32_exp2((F32)((I32)(packed >> 27) - 15 - 9));

	return F32x4_create3(
		(F32) ( packed        & 0x1FF) * scale,
		(F32) ((packed >>  9) & 0x1FF) * scale,
		(F32) ((packed >> 18) & 0x1FF) * scale
	);
}

//Oct18

//A unit vector as an 18 bit octahedral, 9 bits an axis in the low 18 bits, the top 14 left ZERO for the caller.
//The layout @pack.hlsli reads back through unpackOct18.
//
//Octahedral rather than two axes and a sign for the third: the same bits, but no square root to rebuild z
// and no cliff at the equator where sqrt(1 - x^2 - y^2) loses everything. Its error is uniform over the sphere,
// about a third of a degree here, which is coarse for shading and plenty for deciding which side of a surface
// a ray is on. It exists so a mesh can carry one word per TRIANGLE rather than have every hit fetch three positions
// and take a cross product, and the fourteen spare bits are for whatever else a triangle wants to say about itself.
//
//Snorm per axis is symmetric on purpose: -1, 0 and 1 all land exactly, so an axis aligned face normal survives
// untouched, which is what the walls of any box are.

static inline U32 U32_packOct18(F32x4 n) {

	const F32 x = F32x4_x(n), y = F32x4_y(n), z = F32x4_z(n);
	const F32 inv = 1 / (F32_abs(x) + F32_abs(y) + F32_abs(z));

	F32 e[2] = { x * inv, y * inv };

	if(z < 0) {
		const F32 ox = (1 - F32_abs(e[1])) * (e[0] >= 0 ? 1 : -1);
		const F32 oy = (1 - F32_abs(e[0])) * (e[1] >= 0 ? 1 : -1);
		e[0] = ox;
		e[1] = oy;
	}

	U32 packed = 0;

	//Rounded half away from zero on both sides, so -1 and 1 land on the ends of the range rather than one of them
	// falling a step outside it.

	for(U8 i = 0; i < 2; ++i) {
		const F32 c = F32_clamp(e[i], -1, 1) * 255;
		const I32 q = (I32) (c >= 0 ? F32_floor(c + 0.5f) : -F32_floor(-c + 0.5f)) + 256;
		packed |= (U32) q << (9 * i);
	}

	return packed;
}

static inline F32x4 F32x4_unpackOct18(U32 packed) {

	const F32 ex = (F32) ((I32) (packed & 0x1FF) - 256) / 255;
	const F32 ey = (F32) ((I32) ((packed >> 9) & 0x1FF) - 256) / 255;

	F32 x = ex, y = ey;
	const F32 z = 1 - F32_abs(ex) - F32_abs(ey);

	if(z < 0) {
		x = (1 - F32_abs(ey)) * (ex >= 0 ? 1 : -1);
		y = (1 - F32_abs(ex)) * (ey >= 0 ? 1 : -1);
	}

	//Divided by the real length rather than F32x4_normalize3, which is the approximate rsqrt on SSE and hands back
	// 0.9998 for an axis. A pack helper is read back by a shader that did the exact thing, so it has to be exact.

	const F32x4 n = F32x4_create3(x, y, z);
	return F32x4_div(n, F32x4_xxxx4(F32x4_len3(n)));
}

//Oct32

//A unit vector as two snorm16 octahedral axes, x | y<<16, each stored biased by 32768 so the word is unsigned.
//The layout @pack.hlsli's unpackOct32 reads, and what a SHADING normal wants: about 0.005 degrees, which no lobe
// can tell from exact, in the four bytes a shading normal should cost.

static inline U32 U32_packOct32(F32x4 n) {

	const F32 x = F32x4_x(n), y = F32x4_y(n), z = F32x4_z(n);
	const F32 inv = 1 / (F32_abs(x) + F32_abs(y) + F32_abs(z));

	F32 e[2] = { x * inv, y * inv };

	if(z < 0) {
		const F32 ox = (1 - F32_abs(e[1])) * (e[0] >= 0 ? 1 : -1);
		const F32 oy = (1 - F32_abs(e[0])) * (e[1] >= 0 ? 1 : -1);
		e[0] = ox;
		e[1] = oy;
	}

	U32 packed = 0;

	for(U8 i = 0; i < 2; ++i) {
		const F32 c = F32_clamp(e[i], -1, 1) * 32767;
		const I32 q = (I32) (c >= 0 ? F32_floor(c + 0.5f) : -F32_floor(-c + 0.5f)) + 32768;
		packed |= (U32) q << (16 * i);
	}

	return packed;
}

static inline F32x4 F32x4_unpackOct32(U32 packed) {

	const F32 ex = (F32) ((I32) (packed & 0xFFFF) - 32768) / 32767;
	const F32 ey = (F32) ((I32) (packed >> 16) - 32768) / 32767;

	F32 x = ex, y = ey;
	const F32 z = 1 - F32_abs(ex) - F32_abs(ey);

	if(z < 0) {
		x = (1 - F32_abs(ey)) * (ex >= 0 ? 1 : -1);
		y = (1 - F32_abs(ex)) * (ey >= 0 ? 1 : -1);
	}

	const F32x4 n = F32x4_create3(x, y, z);
	return F32x4_div(n, F32x4_xxxx4(F32x4_len3(n)));
}

//F16x2

//Two halves in one word, x low, the layout @pack.hlsli's unpackF16x2 reads. What a uv should cost.

static inline U32 U32_packF16x2(F32 x, F32 y) {
	return (U32) F32_castF16(x) | ((U32) F32_castF16(y) << 16);
}

//Fixed point positions

//Three FP37f4 in a U32x4: x in bits 0..41, y in 42..83, z in 84..125, and bits 126..127 spare.
//
//Fixed point rather than F32 because a POSITION wants uniform absolute precision, where F32's is
//proportional to magnitude. 42 bits of 1 sign, 37 integer and 4 fractional cover +-1.4M km at 1/16 cm,
//so one representation serves a site and its geographic context without the far end going coarse.
//
//The intended use is a world REBASED against a moving anchor: the fixed point value is the truth and the
//instance's F32 transform is derived from it. Both the scale and a whole-unit anchor step are powers of
//two, so that subtraction is exact in F32 and object-space traversal comes out bit identical across a
//rebase. An acceleration structure over the result may be REFIT rather than rebuilt: a global rebase
//translates every instance rigidly, and the surface area heuristic is translation invariant.

static inline Bool I32x4_packFP37f4x3(I32x4 *dst, FP37f4 x, FP37f4 y, FP37f4 z) {

	const I64 lo = -((I64)1 << 41), hi = ((I64)1 << 41) - 1;

	if (!dst || x < lo || x > hi || y < lo || y > hi || z < lo || z > hi)
		return false;                                      //Returns false on invalid, like the packers above

	const U64 ux = (U64) x & 0x3FFFFFFFFFF;
	const U64 uy = (U64) y & 0x3FFFFFFFFFF;
	const U64 uz = (U64) z & 0x3FFFFFFFFFF;

	const U64 lo64 = ux | (uy << 42);
	const U64 hi64 = (uy >> 22) | (uz << 20);

	*dst = I32x4_create4(
		(I32)(U32) lo64, (I32)(U32)(lo64 >> 32),
		(I32)(U32) hi64, (I32)(U32)(hi64 >> 32)
	);

	return true;
}

static inline FP37f4 I32x4_unpackFP37f4x3(I32x4 packed, U8 off) {

	if (off >= 3)
		return 0;

	const U64 lo64 = (U32) I32x4_x(packed) | ((U64)(U32) I32x4_y(packed) << 32);
	const U64 hi64 = (U32) I32x4_z(packed) | ((U64)(U32) I32x4_w(packed) << 32);

	const U64 v = !off ? lo64 : (off == 1 ? ((lo64 >> 42) | (hi64 << 22)) : (hi64 >> 20));

	//Sign extend from the 42nd bit: the field is two's complement in 42 bits, not in 64.

	return (FP37f4)(((v & 0x3FFFFFFFFFF) ^ ((U64)1 << 41)) - ((U64)1 << 41));
}

//Compressing quaternions

typedef struct QuatS16 {
	U64 packed;            //21x3 + sign of w
} QuatS16;

static inline QuatF32 QuatS16_unpack(QuatS16 q) {

	U64 mask = (1 << 21) - 1;
	U64 x = q.packed & mask;
	U64 y = (q.packed >> 21) & mask;
	U64 z = (q.packed >> 42) & mask;
	Bool sign = q.packed >> 63;

	F32x4 v = F32x4_create4((F32)x, (F32)y, (F32)z, 0);
	v = F32x4_div(v, F32x4_xxxx4((F32)mask));
	v = F32x4_sub(F32x4_mul(v, F32x4_two()), F32x4_one());

	F32x4_setWRef(&v, F32_max(0, F32_sqrt(1 - F32x4_sqLen3(v))) * (sign ? -1 : 1));
	return v;
}

static inline QuatS16 QuatF32_pack(QuatF32 q) {

	q = QuatF32_normalize(q);
	U64 sign = F32x4_w(q) < 0;

	q = F32x4_saturate(F32x4_fma(q, F32x4_xxxx4(0.5), F32x4_xxxx4(0.5)));
	const F32x4 asI16 = F32x4_mul(q, F32x4_xxxx4((1 << 21) - 1));

	sign <<= 63;

	const QuatS16 ret = { sign | (U64)(F32x4_x(asI16)) | ((U64)(F32x4_y(asI16)) << 21) | ((U64)(F32x4_z(asI16)) << 42) };
	return ret;
}

#ifdef __cplusplus
	}
#endif
