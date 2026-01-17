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

#ifndef VEC4_NONE_GUARD
	#error Vec4 NONE guard was undefined, this likely indicates include of vec4f_none.h was attempted instead of vec4f.h
#endif

//Swizzles

static inline F32x4 F32x4_setXCopy(F32x4 a, F32 v) { return F32x4_create4(v, F32x4_y(*a), F32x4_z(*a), F32x4_w(*a)); }
static inline F32x4 F32x4_setYCopy(F32x4 a, F32 v) { return F32x4_create4(F32x4_x(*a), v, F32x4_z(*a), F32x4_w(*a)); }
static inline F32x4 F32x4_setZCopy(F32x4 a, F32 v) { return F32x4_create4(F32x4_x(*a), F32x4_y(*a), v, F32x4_w(*a)); }
static inline F32x4 F32x4_setWCopy(F32x4 a, F32 v) { return F32x4_create4(F32x4_x(*a), F32x4_y(*a), F32x4_z(*a), v); }

//Arithmetic
	
static inline F32x4 F32x4_add(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] + b.v[i]); }
static inline F32x4 F32x4_sub(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] - b.v[i]); }
static inline F32x4 F32x4_mul(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] * b.v[i]); }
static inline F32x4 F32x4_div(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] / b.v[i]); }

static inline F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(a) * F32x4_x(b) + F32x4_y(a) * F32x4_y(b); }
static inline F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_dot2(a, b) + F32x4_z(a) * F32x4_z(b); }
static inline F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_dot3(a, b) + F32x4_w(a) * F32x4_w(b); }

//Clamps
	
static inline F32x4 F32x4_min(F32x4 a, F32x4 b) { NONE_OP4F(F32_min(a.v[i], b.v[i])) }
static inline F32x4 F32x4_max(F32x4 a, F32x4 b) { NONE_OP4F(F32_max(a.v[i], b.v[i])) }

//Rounding
	
static inline F32x4 F32x4_ceil(F32x4 a) { NONE_OP4F(F32_ceil(a.v[i])); }
static inline F32x4 F32x4_floor(F32x4 a) { NONE_OP4F(F32_floor(a.v[i])); }
static inline F32x4 F32x4_round(F32x4 a) { NONE_OP4F(F32_round(a.v[i])); }

//Transcendentals
	
static inline F32x4 F32x4_sqrt(F32x4 a) { NONE_OP4F(F32_sqrt(a.v[i])); }
static inline F32x4 F32x4_rsqrt(F32x4 a) { NONE_OP4F(1 / F32_sqrt(a.v[i])); }

//Boolean

static inline F32x4 F32x4_eq(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] == b.v[i])); }
static inline F32x4 F32x4_neq(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] != b.v[i])); }
static inline F32x4 F32x4_geq(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] >= b.v[i])); }
static inline F32x4 F32x4_gt(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] > b.v[i])); }
static inline F32x4 F32x4_leq(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] <= b.v[i])); }
static inline F32x4 F32x4_lt(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] < b.v[i])); }

//Trunc & reduce

static inline F32 F32x4_reduce(F32x4 a) { return F32x4_x(a) + F32x4_y(a) + F32x4_z(a) + F32x4_w(a); }
