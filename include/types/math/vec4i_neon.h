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

//types/math/vec4i_neon.h

#pragma once
#ifndef VEC4I_NEON_GUARD
	#error Vec4i NEON guard was undefined, this likely indicates include of vec4i_neon.h was attempted instead of vec4i.h
#endif

//Loads

static inline I32x4 I32x4_fromF32x4(F32x4 a) { return vcvtq_s32_f32(vrndq_f32(a)); }
static inline I32x4 I32x4_createFromU64x2(U64 i0, U64 i1) {
	uint64x2_t v = vdupq_n_u64(0);
	v = vsetq_lane_u64(i0, v, 0);
	v = vsetq_lane_u64(i1, v, 1);
	return vreinterpretq_s32_u64(v);
}

//Swizzles

static inline I32 I32x4_x(I32x4 a) { return vgetq_lane_s32(a, 0); }
static inline I32 I32x4_y(I32x4 a) { return vgetq_lane_s32(a, 1); }
static inline I32 I32x4_z(I32x4 a) { return vgetq_lane_s32(a, 2); }
static inline I32 I32x4_w(I32x4 a) { return vgetq_lane_s32(a, 3); }

static inline I32x4 I32x4_setXCopy(I32x4 a, I32 v) { return vsetq_lane_s32(v, a, 0); }
static inline I32x4 I32x4_setYCopy(I32x4 a, I32 v) { return vsetq_lane_s32(v, a, 1); }
static inline I32x4 I32x4_setZCopy(I32x4 a, I32 v) { return vsetq_lane_s32(v, a, 2); }
static inline I32x4 I32x4_setWCopy(I32x4 a, I32 v) { return vsetq_lane_s32(v, a, 3); }

//Trunc & reduce

static inline I32x4 I32x4_trunc3(I32x4 a) { return I32x4_setWCopy(a, 0); }
static inline I32x4 I32x4_trunc2(I32x4 a) { return I32x4_setZCopy(I32x4_trunc3(a), 0); }

static inline I32 I32x4_reduce(I32x4 a) {
	int32x2_t low = vget_low_s32(a);
	int32x2_t high = vget_high_s32(a);
	int32x2_t sum = vadd_s32(low, high);
	sum = vpadd_s32(sum, sum);
	return vget_lane_s32(sum, 0);
}

//Arithmetic

static inline I32x4 I32x4_add(I32x4 a, I32x4 b) { return vaddq_s32(a, b); }
static inline I32x4 I32x4_sub(I32x4 a, I32x4 b) { return vsubq_s32(a, b); }
static inline I32x4 I32x4_mul(I32x4 a, I32x4 b) { return vmulq_s32(a, b); }

static inline I32x4 I32x4_addI64x2(I32x4 a, I32x4 b) {
	int64x2_t a64 = vreinterpretq_s64_s32(a);
	int64x2_t b64 = vreinterpretq_s64_s32(b);
	return vreinterpretq_s32_s64(vaddq_s64(a64, b64));
}

static inline I32x4 I32x4_mulU32x2AsU64x2(I32x4 a, I32x4 b) {
	uint64x2_t result = vmull_u32(vget_low_u32(vreinterpretq_u32_s32(a)), vget_low_u32(vreinterpretq_u32_s32(b)));
	return vreinterpretq_s32_u64(result);
}

//Clamps

static inline I32x4 I32x4_min(I32x4 a, I32x4 b) { return vminq_s32(a, b); }
static inline I32x4 I32x4_max(I32x4 a, I32x4 b) { return vmaxq_s32(a, b); }

//Comparison

static inline I32x4 I32x4_eq(I32x4 a, I32x4 b) { return vreinterpretq_s32_u32(vandq_s32(vceqq_s32(a, b), vdupq_n_s32(1))); }
static inline I32x4 I32x4_geq(I32x4 a, I32x4 b) { return vreinterpretq_s32_u32(vandq_s32(vcgeq_s32(a, b), vdupq_n_s32(1))); }
static inline I32x4 I32x4_gt(I32x4 a, I32x4 b) { return vreinterpretq_s32_u32(vandq_s32(vcgtq_s32(a, b), vdupq_n_s32(1))); }
static inline I32x4 I32x4_leq(I32x4 a, I32x4 b) { return vreinterpretq_s32_u32(vandq_s32(vcleq_s32(a, b), vdupq_n_s32(1))); }
static inline I32x4 I32x4_lt(I32x4 a, I32x4 b) { return vreinterpretq_s32_u32(vandq_s32(vcltq_s32(a, b), vdupq_n_s32(1))); }
static inline I32x4 I32x4_neq(I32x4 a, I32x4 b) {
	return vreinterpretq_s32_u32(vandq_s32(vmvnq_u32(vceqq_f32(a, b)), vdupq_n_s32(1)));
}

//Bitwise

static inline I32x4 I32x4_or(I32x4 a, I32x4 b) { return vorrq_s32(a, b); }
static inline I32x4 I32x4_and(I32x4 a, I32x4 b) { return vandq_s32(a, b); }
static inline I32x4 I32x4_andnot(I32x4 a, I32x4 b) { return vandq_s32(vmvnq_s32(a), b); }
static inline I32x4 I32x4_xor(I32x4 a, I32x4 b) { return veorq_s32(a, b); }
static inline I32x4 I32x4_not(I32x4 a) { return veorq_s32(a, I32x4_xxxx4(-1)); }

static inline I32x4 I32x4_lshElements(I32x4 a, U8 elements) {
	switch (elements) {
		case 0:        return a;
		case 1:        return vextq_s32(I32x4_zero(), a, 3);
		case 2:        return vextq_s32(I32x4_zero(), a, 2);
		case 3:        return vextq_s32(I32x4_zero(), a, 1);
		default:    return I32x4_zero();
	}
}

static inline I32x4 I32x4_rshElements(I32x4 a, U8 elements) {
	switch (elements) {
		case 0:        return a;
		case 1:        return vextq_s32(a, I32x4_zero(), 1);
		case 2:        return vextq_s32(a, I32x4_zero(), 2);
		case 3:        return vextq_s32(a, I32x4_zero(), 3);
		default:    return I32x4_zero();
	}
}

static inline I32x4 I32x4_lsh32(I32x4 a, U8 bits) {
	if (!bits) return a;
	if (bits >= 32) return I32x4_zero();
	return vshlq_u32(a, vdupq_n_s32(bits));
}

static inline I32x4 I32x4_rsh32(I32x4 a, U8 bits) {
	if (!bits) return a;
	if (bits >= 32) return I32x4_zero();
	return vshlq_u32(a, vdupq_n_s32(-(I32)bits));
}

static inline I32x4 I32x4_lsh64(I32x4 a, U8 bits) {
	if (!bits) return a;
	if (bits >= 64) return I32x4_zero();
	uint64x2_t v = vreinterpretq_u64_s32(a);
	v = vshlq_u64(v, vdupq_n_s64(bits));
	return vreinterpretq_s32_u64(v);
}

static inline I32x4 I32x4_rsh64(I32x4 a, U8 bits) {
	if (!bits) return a;
	if (bits >= 64) return I32x4_zero();
	uint64x2_t v = vreinterpretq_u64_s32(a);
	v = vshlq_u64(v, vdupq_n_s64(-(I64)bits));
	return vreinterpretq_s32_u64(v);
}

//SHA256 helper functions

static inline I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b) {
	uint8x16_t tbl = vreinterpretq_u8_s32(a);
	uint8x16_t idx = vreinterpretq_u8_s32(b);
	return vreinterpretq_s32_u8(vqtbl1q_u8(tbl, idx));
}

static inline I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw) {
	I32x4 mask = I32x4_create4(-(xyzw & 1), -((xyzw >> 1) & 1), -((xyzw >> 2) & 1), -((xyzw >> 3) & 1));
	return vbslq_s32(vreinterpretq_u32_s32(mask), b, a);
}

static inline I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v) {

	uint8x16_t va = vreinterpretq_u8_s32(a);
	uint8x16_t vb = vreinterpretq_u8_s32(b);

	switch (v) {
		default:  return b;
		case 1:   va = vextq_u8(vb, va, 4);  break;
		case 2:   va = vextq_u8(vb, va, 8);  break;
		case 3:   va = vextq_u8(vb, va, 12); break;
	}

	return vreinterpretq_s32_u8(va);
}

static inline I32x4 I32x4_swapEndianness(I32x4 v) {
	v = vrev64q_s32(vreinterpretq_s32_u8(vrev32q_u8(vreinterpretq_u8_s32(v))));
	return vextq_s32(v, v, 2);
}
