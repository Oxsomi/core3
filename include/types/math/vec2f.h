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

#pragma once
#include "types/base/buffer.h"
#include "types/base/mathf.h"
#include "types/math/vec_base.h"
#include "types/math/vec2.h"

#ifdef __cplusplus
	extern "C" {
#endif

BUFFER_OP_IMPL(F32x2);

//Constants

static const F32x2 F32x2_zero = { { 0, 0 } };
static const F32x2 F32x2_one = { { 1, 1 } };
static const F32x2 F32x2_two = { { 2, 2 } };
static const F32x2 F32x2_negOne = { { -1, -1 } };
static const F32x2 F32x2_negTwo = { { -2, -2 } };

//Creates and loads

static inline F32x2 F32x2_create2(F32 x, F32 y) { F32x2 v = { { x, y } }; return v; }
static inline F32x2 F32x2_create1(F32 x) { return F32x2_create2(x, 0); }

static inline F32x2 F32x2_xx2(F32 x) { return F32x2_create2(x, x); }

static inline F32x2 F32x2_load1(const void *arr) {		//Misaligned load 1 F32
	F32x2 result = F32x2_zero;
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(F32)), Buffer_createRefConst(arr, sizeof(F32)));
	return result;
}

static inline F32x2 F32x2_load2(const void *arr) {		//Misaligned load 2 F32s
	F32x2 result = F32x2_zero;
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(F32) * 2), Buffer_createRefConst(arr, sizeof(F32) * 2));
	return result;
}

//Swizzles

static inline F32 F32x2_x(F32x2 a) { return a.v[0]; }
static inline F32 F32x2_y(F32x2 a) { return a.v[1]; }

static inline F32x2 F32x2_xx(F32x2 a) { return F32x2_xx2(F32x2_x(a)); }
static inline F32x2 F32x2_xy(F32x2 a) { return a; }
static inline F32x2 F32x2_yx(F32x2 a) { return F32x2_create2(F32x2_y(a), F32x2_x(a)); }
static inline F32x2 F32x2_yy(F32x2 a) { return F32x2_xx2(F32x2_y(a)); }

static inline void F32x2_setRefX(F32x2 *a, F32 v) { if (a) a->v[0] = v; }
static inline void F32x2_setRefY(F32x2 *a, F32 v) { if (a) a->v[1] = v; }

static inline void F32x2_setRef(F32x2 *a, U8 i, F32 v) {
	switch (i) {
		case 0:		F32x2_setRefX(a, v);	break;
		default:	F32x2_setRefY(a, v);	break;
	}
}

static inline F32 F32x2_get(F32x2 a, U8 i) {
	switch (i) {
		case 0:		return F32x2_x(a);
		default:	return F32x2_y(a);
	}
}

//Arithmetic

static inline F32x2 F32x2_add(F32x2 a, F32x2 b) { NONE_OP2F(a.v[i] + b.v[i]); }
static inline F32x2 F32x2_sub(F32x2 a, F32x2 b) { NONE_OP2F(a.v[i] - b.v[i]); }
static inline F32x2 F32x2_mul(F32x2 a, F32x2 b) { NONE_OP2F(a.v[i] * b.v[i]); }
static inline F32x2 F32x2_div(F32x2 a, F32x2 b) { NONE_OP2F(a.v[i] / b.v[i]); }

//Rounding

static inline F32x2 F32x2_ceil(F32x2 v) { NONE_OP2F(F32_ceil(v.v[i])); }
static inline F32x2 F32x2_floor(F32x2 v) { NONE_OP2F(F32_floor(v.v[i])); }
static inline F32x2 F32x2_round(F32x2 v) { NONE_OP2F(F32_round(v.v[i])); }

static inline F32x2 F32x2_fract(F32x2 v) { return F32x2_sub(v, F32x2_floor(v)); }

//Arithmetic (more complex)

static inline F32x2 F32x2_complement(F32x2 a) { return F32x2_sub(F32x2_one, a); }
static inline F32x2 F32x2_negate(F32x2 a) { return F32x2_sub(F32x2_zero, a); }
static inline F32x2 F32x2_inverse(F32x2 a) { return F32x2_div(F32x2_one, a); }

static inline F32x2 F32x2_pow2(F32x2 a) { return F32x2_mul(a, a); }
static inline F32x2 F32x2_mod(F32x2 v, F32x2 d) { return F32x2_sub(v, F32x2_mul(F32x2_fract(F32x2_div(v, d)), d)); }

static inline F32 F32x2_reduce(F32x2 a) { return F32x2_x(a) + F32x2_y(a); }
static inline F32 F32x2_dot(F32x2 a, F32x2 b) { return F32x2_reduce(F32x2_mul(a, b)); }
static inline F32 F32x2_satDot(F32x2 a, F32x2 b) { return F32_saturate(F32x2_dot(a, b)); }

static inline F32 F32x2_sqLen(F32x2 v) { return F32x2_dot(v, v); }
static inline F32 F32x2_len(F32x2 v) { return F32_sqrt(F32x2_sqLen(v)); }
static inline F32x2 F32x2_normalize(F32x2 v) { return F32x2_mul(v, F32x2_xx2(1 / F32x2_len(v))); }

//Clamp

static inline F32x2 F32x2_min(F32x2 a, F32x2 b) { NONE_OP2F(F32_min(a.v[i], b.v[i])); }
static inline F32x2 F32x2_max(F32x2 a, F32x2 b) { NONE_OP2F(F32_max(a.v[i], b.v[i])); }

static inline F32x2 F32x2_clamp(F32x2 a, F32x2 mi, F32x2 ma) { return F32x2_max(mi, F32x2_min(ma, a)); }
static inline F32x2 F32x2_saturate(F32x2 a) { return F32x2_clamp(a, F32x2_zero, F32x2_one); }

//Transcedendals

static inline F32x2 F32x2_acos(F32x2 v) { NONE_OP2F(F32_acos(v.v[i])); }
static inline F32x2 F32x2_cos(F32x2 v) { NONE_OP2F(F32_cos(v.v[i])); }
static inline F32x2 F32x2_asin(F32x2 v) { NONE_OP2F(F32_asin(v.v[i])); }
static inline F32x2 F32x2_sin(F32x2 v) { NONE_OP2F(F32_sin(v.v[i])); }
static inline F32x2 F32x2_atan(F32x2 v) { NONE_OP2F(F32_atan(v.v[i])); }
static inline F32x2 F32x2_atan2(F32x2 y, F32x2 x) { NONE_OP2F(F32_atan2(y.v[i], x.v[i])); }
static inline F32x2 F32x2_tan(F32x2 v) { NONE_OP2F(F32_tan(v.v[i])); }
static inline F32x2 F32x2_sqrt(F32x2 v) { NONE_OP2F(F32_sqrt(v.v[i])); }
static inline F32x2 F32x2_rsqrt(F32x2 v) { NONE_OP2F(1 / F32_sqrt(v.v[i])); }

static inline F32x2 F32x2_pow(F32x2 v, F32x2 e) { NONE_OP2F(F32_pow(v.v[i], e.v[i])); }

static inline F32x2 F32x2_loge(F32x2 v) { NONE_OP2F(F32_loge(v.v[i])); }
static inline F32x2 F32x2_log10(F32x2 v) { NONE_OP2F(F32_log10(v.v[i])); }
static inline F32x2 F32x2_log2(F32x2 v) { NONE_OP2F(F32_log2(v.v[i])); }

static inline F32x2 F32x2_expe(F32x2 v) { NONE_OP2F(F32_expe(v.v[i])); }
static inline F32x2 F32x2_exp10(F32x2 v) { NONE_OP2F(F32_exp10(v.v[i])); }
static inline F32x2 F32x2_exp2(F32x2 v) { NONE_OP2F(F32_exp2(v.v[i])); }

//Boolean

static inline F32x2 F32x2_eq(F32x2 a, F32x2 b) { NONE_OP2F((F32)(a.v[i] == b.v[i])); }
static inline F32x2 F32x2_neq(F32x2 a, F32x2 b) { NONE_OP2F((F32)(a.v[i] != b.v[i])); }
static inline F32x2 F32x2_geq(F32x2 a, F32x2 b) { NONE_OP2F((F32)(a.v[i] >= b.v[i])); }
static inline F32x2 F32x2_gt(F32x2 a, F32x2 b) { NONE_OP2F((F32)(a.v[i] > b.v[i])); }
static inline F32x2 F32x2_leq(F32x2 a, F32x2 b) { NONE_OP2F((F32)(a.v[i] <= b.v[i])); }
static inline F32x2 F32x2_lt(F32x2 a, F32x2 b) { NONE_OP2F((F32)(a.v[i] < b.v[i])); }

static inline Bool F32x2_all(F32x2 a) { return F32x2_reduce(F32x2_neq(a, F32x2_zero)) == 2; }
static inline Bool F32x2_any(F32x2 a) { return F32x2_reduce(F32x2_neq(a, F32x2_zero)); }

static inline Bool F32x2_eq2(F32x2 a, F32x2 b) { return F32x2_all(F32x2_eq(a, b)); }
static inline Bool F32x2_neq2(F32x2 a, F32x2 b) { return !F32x2_eq2(a, b); }

//Obtain sign (-1 if <0, otherwise 1)
static inline F32x2 F32x2_sign(F32x2 v) { return F32x2_add(F32x2_mul(F32x2_lt(v, F32x2_zero), F32x2_negTwo), F32x2_one); }

//Misc functions, used for shading for example

//Reflect incident direction around normal
//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml
static inline F32x2 F32x2_reflect(F32x2 i, F32x2 n) {
	return F32x2_sub(i, F32x2_mul(n, F32x2_xx2(2 * F32x2_dot(n, i))));
}

static inline F32x2 F32x2_abs(F32x2 v) { return F32x2_mul(F32x2_sign(v), v); }

//Matrix

static inline F32x2 F32x2_mul2x2(F32x2 v2, F32x2 v2x2[2]) {
	return F32x2_add(
		F32x2_mul(v2x2[0], F32x2_xx(v2)),
		F32x2_mul(v2x2[1], F32x2_yy(v2))
	);
}

static inline F32x2 F32x2_mul2x3(F32x2 v2, F32x2 v2x3[3]) {
	return F32x2_add(F32x2_add(
		F32x2_mul(v2x3[0], F32x2_xx(v2)),
		F32x2_mul(v2x3[1], F32x2_yy(v2))
	), v2x3[2]);
}

#ifdef __cplusplus
	}
#endif
