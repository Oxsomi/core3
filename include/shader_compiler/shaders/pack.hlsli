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

//shader_compiler/shaders/pack.hlsli
//
//Narrow float and normal packing, for the places a value has to survive a trip through storage it does not fit:
// a ray payload, a G-buffer texel, an accumulator.
//Everything here is plain 32-bit integer math,
// so none of it needs 16BitTypes and all of it works on any shader model this compiler targets.
//
//Rounding is ROUND-TO-NEAREST-EVEN throughout,
// done by biasing the discarded bits before the shift so a carry ripples into the exponent for free.
//That matters more than it sounds:
// a truncating pack of a running mean drifts,
// because what rounds away is the small corrections while the rare large ones still land.

#pragma once
#include "@types.hlsli"

//---------------------------------------------------------------- F16

//Upcast/downcast without 16BitTypes:
// f32tof16/f16tof32 are U32-in, U32-out intrinsics, so the half only ever exists as bits.
//Use these when a struct field must stay a U32.

F32 unpackF16(U32 v)            { return f16tof32(v & 0xFFFF); }
U32 packF16(F32 v)              { return f32tof16(v); }

F32x2 unpackF16x2(U32 v)        { return F32x2(f16tof32(v & 0xFFFF), f16tof32(v >> 16)); }
U32 packF16x2(F32x2 v)          { return f32tof16(v.x) | (f32tof16(v.y) << 16); }

//---------------------------------------------------------------- F21, unsigned

//21 bits:
// 6 exponent (bias 31), 15 mantissa, NO sign.
//Three fit in a U64 with one bit spare, which is what an RG32u accumulator wants.
//The exponent spans 2^-30..2^32, wider than F16 both ways,
// so neither an emissive value nor a deep-shadow one leaves the range.
//Denormals, NaN and Inf are not represented; input is clamped non-negative.
//Matches EFloatType_UF21 on the CPU side (flp.h), EXCEPT that flp.c truncates ties where this rounds to nearest even,
// an exact tie can differ by one ulp between the two.
//
//F16 is not an alternative here:
// its 10-bit mantissa cannot hold a progressive mean.
//Once the increment falls under half an ulp the downward corrections round away while the upward ones still land,
// and a non-negative heavy-tailed signal then drifts UP by tens of percent.

static const U32 F21_MANT = 15, F21_BIAS = 31, F21_MAX = (63u << F21_MANT) | 0x7FFFu;

U32 packF21(F32 v) {

	U32 u = asuint(max(v, 0)) + 0x7F;
	u += (u >> 8) & 1;

	const I32 e = I32((u >> 23) & 0xFF) - 127 + I32(F21_BIAS);

	if(e <= 0)
		return 0;

	if(e >= 63)
		return F21_MAX;

	return (U32(e) << F21_MANT) | ((u >> 8) & 0x7FFFu);
}

F32 unpackF21(U32 p) {

	const U32 e = p >> F21_MANT;

	if(!e)
		return 0;

	return asfloat(((e - F21_BIAS + 127) << 23) | ((p & 0x7FFFu) << 8));
}

//Three F21 in the 64 bits of an RG32u texel:
// x in w0[0:20], y across w0[21:31] and w1[0:9], z in w1[10:30].
//Bit 63 is free.

U32x2 packF21x3(F32x3 v) {
	const U32 x = packF21(v.r), y = packF21(v.g), z = packF21(v.b);
	return U32x2(x | (y << 21), (y >> 11) | (z << 10));
}

F32x3 unpackF21x3(U32x2 v) {
	return F32x3(
		unpackF21(v.x & 0x1FFFFF),
		unpackF21(((v.x >> 21) & 0x7FF) | ((v.y & 0x3FF) << 11)),
		unpackF21((v.y >> 10) & 0x1FFFFF)
	);
}

//---------------------------------------------------------------- RGB9E5

//DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
// three 9-bit mantissas over one 5-bit exponent. 32 bits for an HDR triple whose channels are within a few stops of
// each other, emission, irradiance, a probe.
//Channels far apart lose the dim ones, since they all pay for the brightest one's exponent.

static const F32 rgb9e5Max = (511.0 / 512) * 65536;      //((2^9-1)/2^9) * 2^(31-15)

U32 packRGB9E5(F32x3 v) {

	v = clamp(v, 0.xxx, rgb9e5Max.xxx);

	const F32 maxC = max(v.r, max(v.g, v.b));

	I32 e = maxC > 0 ? max((I32) floor(log2(maxC)) + 1, -15) : -15;

	F32 denom = exp2((F32)(e - 9));

	//A mantissa that rounds up to 2^9 needs one more exponent step, so re-derive once
	if((I32) floor(maxC / denom + 0.5) >= 512) {
		denom *= 2;
		++e;
	}

	const U32x3 m = (U32x3) floor(v / denom + 0.5.xxx);

	return m.r | (m.g << 9) | (m.b << 18) | ((U32)(e + 15) << 27);
}

F32x3 unpackRGB9E5(U32 p) {
	const F32 scale = exp2((F32)((I32)(p >> 27) - 15 - 9));
	return F32x3(p & 0x1FF, (p >> 9) & 0x1FF, (p >> 18) & 0x1FF) * scale;
}

//---------------------------------------------------------------- oct18

//A unit vector as an 18 bit octahedral in the low 18 bits, 9 an axis, the top 14 left to the caller.
//Mirrors U32_packOct18 in types/math/pack.h, which is what writes the per triangle word a mesh reader emits:
// the geometric normal a hit loads instead of three positions and a cross product, with the triangle's material
// index above it. About a third of a degree, uniform over the sphere, which is plenty for deciding which side of a
// surface a ray is on.

U32 packOct18(F32x3 n) {

	const F32x2 p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
	const F32x2 e = n.z >= 0 ? p : (1.xx - abs(p.yx)) * select(p >= 0, 1.xx, (-1).xx);
	const U32x2 q = U32x2((I32x2) round(clamp(e, -1, 1) * 255) + 256);

	return q.x | (q.y << 9);
}

F32x3 unpackOct18(U32 v) {

	const F32x2 e = F32x2(I32x2(v & 0x1FF, (v >> 9) & 0x1FF) - 256) / 255;
	F32x3 n = F32x3(e, 1 - abs(e.x) - abs(e.y));

	if(n.z < 0)
		n.xy = (1.xx - abs(n.yx)) * select(n.xy >= 0, 1.xx, (-1).xx);

	return normalize(n);
}

//---------------------------------------------------------------- oct32

//A unit vector as two snorm16 octahedral axes, x | y<<16, each biased by 32768. Mirrors U32_packOct32 in
// types/math/pack.h, which is what a mesh reader packs its shading normals with. About 0.005 degrees.

U32 packOct32(F32x3 n) {
	const F32x2 p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
	const F32x2 e = n.z >= 0 ? p : (1.xx - abs(p.yx)) * select(p >= 0, 1.xx, (-1).xx);
	const U32x2 q = U32x2((I32x2) round(clamp(e, -1, 1) * 32767) + 32768);
	return q.x | (q.y << 16);
}

F32x3 unpackOct32(U32 v) {

	const F32x2 e = F32x2(I32x2(v & 0xFFFF, v >> 16) - 32768) / 32767;
	F32x3 n = F32x3(e, 1 - abs(e.x) - abs(e.y));

	if(n.z < 0)
		n.xy = (1.xx - abs(n.yx)) * select(n.xy >= 0, 1.xx, (-1).xx);

	return normalize(n);
}

//---------------------------------------------------------------- normals

//17 bits:
// 8:8 for x/y plus one sign bit for the reconstructed z. Coarse for SHADING, roughly a degree,
// but exact enough for what a geometric normal is used for, which is deciding which side of a surface a ray starts on.
//Prefer octahedral for a shading normal at the same bit count.

U32 packNormal17(F32x3 n) {

	const U32x2 xy = (U32x2) round(saturate(n.xy * 0.5 + 0.5.xx) * 255);

	return xy.x | (xy.y << 8) | (n.z < 0 ? (1u << 16) : 0);
}

F32x3 unpackNormal17(U32 p) {

	const F32x2 xy = F32x2(p & 0xFF, (p >> 8) & 0xFF) / 255 * 2 - 1.xx;

	//Clamped before the sqrt:
	// the 8-bit quantisation can push x^2 + y^2 just past 1
	const F32 z = sqrt(saturate(1 - dot(xy, xy)));

	return F32x3(xy, (p >> 16) & 1 ? -z : z);
}
