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

//types/math/vec4f_neon.inc.h

#ifndef VEC4F_NEON_GUARD
	#error Vec4f NEON guard was undefined, this likely indicates include of vec4f_neon.h was attempted instead of vec4f.h
#endif

static inline F32x4 F32x4_fromI32x4(I32x4 a) { return vcvtq_f32_s32(a); }

//Swizzles

static inline F32 F32x4_x(F32x4 a) { return vgetq_lane_f32(a, 0); }
static inline F32 F32x4_y(F32x4 a) { return vgetq_lane_f32(a, 1); }
static inline F32 F32x4_z(F32x4 a) { return vgetq_lane_f32(a, 2); }
static inline F32 F32x4_w(F32x4 a) { return vgetq_lane_f32(a, 3); }

static inline F32x4 F32x4_setXCopy(F32x4 a, F32 v) { return vsetq_lane_f32(v, a, 0); }
static inline F32x4 F32x4_setYCopy(F32x4 a, F32 v) { return vsetq_lane_f32(v, a, 1); }
static inline F32x4 F32x4_setZCopy(F32x4 a, F32 v) { return vsetq_lane_f32(v, a, 2); }
static inline F32x4 F32x4_setWCopy(F32x4 a, F32 v) { return vsetq_lane_f32(v, a, 3); }

//Trunc & reduce

static inline F32x4 F32x4_trunc3(F32x4 a) { return F32x4_setWCopy(a, 0); }
static inline F32x4 F32x4_trunc2(F32x4 a) { return F32x4_setZCopy(F32x4_trunc3(a), 0); }

static inline F32 F32x4_reduce(F32x4 a) {
	float32x2_t low = vget_low_f32(a);
	float32x2_t high = vget_high_f32(a);
	float32x2_t sum = vadd_f32(low, high);
	sum = vpadd_f32(sum, sum);
	return vget_lane_f32(sum, 0);
}

//Arithmetic

static inline F32x4 F32x4_add(F32x4 a, F32x4 b) { return vaddq_f32(a, b); }
static inline F32x4 F32x4_sub(F32x4 a, F32x4 b) { return vsubq_f32(a, b); }
static inline F32x4 F32x4_mul(F32x4 a, F32x4 b) { return vmulq_f32(a, b); }
static inline F32x4 F32x4_div(F32x4 a, F32x4 b) { return vdivq_f32(a, b); }

static inline F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_reduce(F32x4_mul(a, F32x4_trunc2(b))); }
static inline F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_reduce(F32x4_mul(a, F32x4_trunc3(b))); }
static inline F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_reduce(F32x4_mul(a, b)); }

//Clamps

static inline F32x4 F32x4_min(F32x4 a, F32x4 b) { return vminq_f32(a, b); }
static inline F32x4 F32x4_max(F32x4 a, F32x4 b) { return vmaxq_f32(a, b); }

//Rounding

static inline F32x4 F32x4_ceil(F32x4 a) { return vrndpq_f32(a); }
static inline F32x4 F32x4_floor(F32x4 a) { return vrndmq_f32(a); }
static inline F32x4 F32x4_round(F32x4 a) { return vrndnq_f32(a); }

//Transcendentals

static inline F32x4 F32x4_sqrt(F32x4 a) { return vsqrtq_f32(a); }
static inline F32x4 F32x4_rsqrt(F32x4 a) { return vrsqrteq_f32(a); }

//Boolean
		
static inline F32x4 F32x4_recastI32x4Internal(F32x4 a) { return vcvtq_f32_s32(vnegq_s32(vreinterpretq_s32_u32(a))); }
static inline F32x4 F32x4_eqExact(F32x4 a, F32x4 b) { return F32x4_recastI32x4Internal(vceqq_f32(a, b)); }
static inline F32x4 F32x4_neqExact(F32x4 a, F32x4 b) { return F32x4_recastI32x4Internal(vmvnq_u32(vceqq_f32(a, b))); }
static inline F32x4 F32x4_geq(F32x4 a, F32x4 b) { return F32x4_recastI32x4Internal(vcgeq_f32(a, b)); }
static inline F32x4 F32x4_gt(F32x4 a, F32x4 b) { return F32x4_recastI32x4Internal(vcgtq_f32(a, b)); }
static inline F32x4 F32x4_leq(F32x4 a, F32x4 b) { return F32x4_recastI32x4Internal(vcleq_f32(a, b)); }
static inline F32x4 F32x4_lt(F32x4 a, F32x4 b) { return F32x4_recastI32x4Internal(vcltq_f32(a, b)); }
