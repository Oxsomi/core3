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

//types/math/vec4f_none.h

#ifndef VEC4F_NONE_GUARD
	#error Vec4f NONE guard was undefined, this likely indicates include of vec4f_none.h was attempted instead of vec4f.h
#endif

//Swizzles

static inline F32 F32x4_x(F32x4 a) { return a.v[0]; }
static inline F32 F32x4_y(F32x4 a) { return a.v[1]; }
static inline F32 F32x4_z(F32x4 a) { return a.v[2]; }
static inline F32 F32x4_w(F32x4 a) { return a.v[3]; }

static inline F32x4 F32x4_setXCopy(F32x4 a, F32 v) { return F32x4_create4(v, F32x4_y(a), F32x4_z(a), F32x4_w(a)); }
static inline F32x4 F32x4_setYCopy(F32x4 a, F32 v) { return F32x4_create4(F32x4_x(a), v, F32x4_z(a), F32x4_w(a)); }
static inline F32x4 F32x4_setZCopy(F32x4 a, F32 v) { return F32x4_create4(F32x4_x(a), F32x4_y(a), v, F32x4_w(a)); }
static inline F32x4 F32x4_setWCopy(F32x4 a, F32 v) { return F32x4_create4(F32x4_x(a), F32x4_y(a), F32x4_z(a), v); }

static inline void F32x4_setXRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setXCopy(*a, v); }
static inline void F32x4_setYRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setYCopy(*a, v); }
static inline void F32x4_setZRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setZCopy(*a, v); }
static inline void F32x4_setWRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setWCopy(*a, v); }

static inline F32x4 F32x4_setCopy(F32x4 a, U8 i, F32 v) {
	switch (i & 3) {
		case 0:		return F32x4_setXCopy(a, v);
		case 1:		return F32x4_setYCopy(a, v);
		case 2:		return F32x4_setZCopy(a, v);
		default:	return F32x4_setWCopy(a, v);
	}
}

static inline void F32x4_setRef(F32x4 *a, U8 i, F32 v) {
	switch (i & 3) {
		case 0:		F32x4_setXRef(a, v);	break;
		case 1:		F32x4_setYRef(a, v);	break;
		case 2:		F32x4_setZRef(a, v);	break;
		default:	F32x4_setWRef(a, v);
	}
}

static inline F32x4 F32x4_fromI32x4(I32x4 a) { NONE_OP4F((F32)a.v[i]); }

//Arithmetic
	
static inline F32x4 F32x4_add(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] + b.v[i]); }
static inline F32x4 F32x4_sub(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] - b.v[i]); }
static inline F32x4 F32x4_mul(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] * b.v[i]); }
static inline F32x4 F32x4_div(F32x4 a, F32x4 b) { NONE_OP4F(a.v[i] / b.v[i]); }

static inline F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(a) * F32x4_x(b) + F32x4_y(a) * F32x4_y(b); }
static inline F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_dot2(a, b) + F32x4_z(a) * F32x4_z(b); }
static inline F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_dot3(a, b) + F32x4_w(a) * F32x4_w(b); }

//Clamps
	
static inline F32x4 F32x4_min(F32x4 a, F32x4 b) { NONE_OP4F(F32_min(a.v[i], b.v[i])); }
static inline F32x4 F32x4_max(F32x4 a, F32x4 b) { NONE_OP4F(F32_max(a.v[i], b.v[i])); }

//Rounding
	
static inline F32x4 F32x4_ceil(F32x4 a) { NONE_OP4F(F32_ceil(a.v[i])); }
static inline F32x4 F32x4_floor(F32x4 a) { NONE_OP4F(F32_floor(a.v[i])); }
static inline F32x4 F32x4_round(F32x4 a) { NONE_OP4F(F32_round(a.v[i])); }

//Transcendentals
	
static inline F32x4 F32x4_sqrt(F32x4 a) { NONE_OP4F(F32_sqrt(a.v[i])); }
static inline F32x4 F32x4_rsqrt(F32x4 a) { NONE_OP4F(1 / F32_sqrt(a.v[i])); }

//Boolean

static inline F32x4 F32x4_eqExact(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] == b.v[i])); }
static inline F32x4 F32x4_neqExact(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] != b.v[i])); }
static inline F32x4 F32x4_geq(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] >= b.v[i])); }
static inline F32x4 F32x4_gt(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] > b.v[i])); }
static inline F32x4 F32x4_leq(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] <= b.v[i])); }
static inline F32x4 F32x4_lt(F32x4 a, F32x4 b) { NONE_OP4F((F32)(a.v[i] < b.v[i])); }

//Trunc & reduce

static inline F32x4 F32x4_trunc2(F32x4 a) { return F32x4_create2(F32x4_x(a), F32x4_y(a)); }
static inline F32x4 F32x4_trunc3(F32x4 a) { return F32x4_create3(F32x4_x(a), F32x4_y(a), F32x4_z(a)); }

static inline F32 F32x4_reduce(F32x4 a) { return F32x4_x(a) + F32x4_y(a) + F32x4_z(a) + F32x4_w(a); }
