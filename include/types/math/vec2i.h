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

//types/math/vec2i.h

#pragma once
#include "types/base/buffer_base.h"
#include "types/base/mathi.h"
#include "types/base/endianness.h"
#include "types/math/vec_base.h"
#include "types/math/vec2.h"

#ifdef __cplusplus
	extern "C" {
#endif

BUFFER_OP_IMPL(I32x2);

//Constants

static const I32x2 I32x2_zero = { { 0, 0 } };
static const I32x2 I32x2_one = { { 1, 1 } };
static const I32x2 I32x2_two = { { 2, 2 } };
static const I32x2 I32x2_negOne = { { -1, -1 } };
static const I32x2 I32x2_negTwo = { { -2, -2 } };

//Creates and loads

static inline I32x2 I32x2_create2(I32 x, I32 y) { I32x2 v = { { x, y } }; return v; }
static inline I32x2 I32x2_create1(I32 x) { return I32x2_create2(x, 0); }

static inline I32x2 I32x2_xx2(I32 x) { return I32x2_create2(x, x); }

static inline I32x2 I32x2_load1(const void *arr) {        //Misaligned load 1 I32
	I32x2 result = I32x2_zero;
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32)), Buffer_createRefConst(arr, sizeof(I32)));
	return result;
}

static inline I32x2 I32x2_load2(const void *arr) {        //Misaligned load 2 I32s
	I32x2 result = I32x2_zero;
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32) * 2), Buffer_createRefConst(arr, sizeof(I32) * 2));
	return result;
}

//Swizzles

static inline I32 I32x2_x(I32x2 a) { return a.v[0]; }
static inline I32 I32x2_y(I32x2 a) { return a.v[1]; }

static inline I32x2 I32x2_xx(I32x2 a) { return I32x2_xx2(I32x2_x(a)); }
static inline I32x2 I32x2_xy(I32x2 a) { return a; }
static inline I32x2 I32x2_yx(I32x2 a) { return I32x2_create2(I32x2_y(a), I32x2_x(a)); }
static inline I32x2 I32x2_yy(I32x2 a) { return I32x2_xx2(I32x2_y(a)); }

static inline void I32x2_setRefX(I32x2 *a, I32 v) { if (a) a->v[0] = v; }
static inline void I32x2_setRefY(I32x2 *a, I32 v) { if (a) a->v[1] = v; }

static inline void I32x2_setRef(I32x2 *a, U8 i, I32 v) {
	switch (i) {
		case 0:        I32x2_setRefX(a, v);    break;
		default:    I32x2_setRefY(a, v);    break;
	}
}

static inline I32 I32x2_get(I32x2 a, U8 i) {
	switch (i) {
		case 0:        return I32x2_x(a);
		default:    return I32x2_y(a);
	}
}

//Arithmetic

static inline I32x2 I32x2_add(I32x2 a, I32x2 b) { NONE_OP2I(a.v[i] + b.v[i]); }
static inline I32x2 I32x2_sub(I32x2 a, I32x2 b) { NONE_OP2I(a.v[i] - b.v[i]); }
static inline I32x2 I32x2_mul(I32x2 a, I32x2 b) { NONE_OP2I(a.v[i] * b.v[i]); }
static inline I32x2 I32x2_div(I32x2 a, I32x2 b) { NONE_OP2I(a.v[i] / b.v[i]); }        //Undefined if any b[x] == 0

static inline I32x2 I32x2_complement(I32x2 a) { return I32x2_sub(I32x2_one, a); }
static inline I32x2 I32x2_negate(I32x2 a) { return I32x2_sub(I32x2_zero, a); }

static inline I32x2 I32x2_pow2(I32x2 a) { return I32x2_mul(a, a); }

static inline I32 I32x2_reduce(I32x2 a) { return I32x2_x(a) + I32x2_y(a); }

//Clamp

static inline I32x2 I32x2_min(I32x2 a, I32x2 b) { NONE_OP2I(I32_min(a.v[i], b.v[i])); }
static inline I32x2 I32x2_max(I32x2 a, I32x2 b) { NONE_OP2I(I32_max(a.v[i], b.v[i])); }

static inline I32x2 I32x2_clamp(I32x2 a, I32x2 mi, I32x2 ma) { return I32x2_max(mi, I32x2_min(ma, a)); }

//Boolean

static inline I32x2 I32x2_eq(I32x2 a, I32x2 b) { NONE_OP2I((I32)(a.v[i] == b.v[i])); }
static inline I32x2 I32x2_neq(I32x2 a, I32x2 b) { NONE_OP2I((I32)(a.v[i] != b.v[i])); }
static inline I32x2 I32x2_geq(I32x2 a, I32x2 b) { NONE_OP2I((I32)(a.v[i] >= b.v[i])); }
static inline I32x2 I32x2_gt(I32x2 a, I32x2 b) { NONE_OP2I((I32)(a.v[i] > b.v[i])); }
static inline I32x2 I32x2_leq(I32x2 a, I32x2 b) { NONE_OP2I((I32)(a.v[i] <= b.v[i])); }
static inline I32x2 I32x2_lt(I32x2 a, I32x2 b) { NONE_OP2I((I32)(a.v[i] < b.v[i])); }

static inline Bool I32x2_all(I32x2 a) { return I32x2_reduce(I32x2_neq(a, I32x2_zero)) == 2; }
static inline Bool I32x2_any(I32x2 a) { return I32x2_reduce(I32x2_neq(a, I32x2_zero)); }

static inline Bool I32x2_eq2(I32x2 a, I32x2 b) { return I32x2_all(I32x2_eq(a, b)); }
static inline Bool I32x2_neq2(I32x2 a, I32x2 b) { return !I32x2_eq2(a, b); }

//Obtain sign (-1 if <0, otherwise 1)
static inline I32x2 I32x2_sign(I32x2 v) { return I32x2_add(I32x2_mul(I32x2_lt(v, I32x2_zero), I32x2_negTwo), I32x2_one); }

static inline I32x2 I32x2_swapEndianness(I32x2 v) {
	return I32x2_create2(I32_swapEndianness(I32x2_x(v)), I32_swapEndianness(I32x2_y(v)));
}

static inline F32x2 F32x2_bitsI32x2(I32x2 a) {
	const void *aptr = &a;
	return *(const F32x2*)aptr;
}

static inline I32x2 I32x2_bitsF32x2(F32x2 a) {
	const void *aptr = &a;
	return *(const I32x2 *)aptr;
}

//Arithmetic

static inline I32x2 I32x2_abs(I32x2 v) { return I32x2_mul(I32x2_sign(v), v); }
static inline I32x2 I32x2_mod(I32x2 v, I32x2 d) {                //UB for any d[x] == 0
	I32x2 r = I32x2_sub(v, I32x2_mul(I32x2_div(v, d), d));
	I32x2 mask = I32x2_lt(r, I32x2_zero);
	return I32x2_add(r, I32x2_mul(mask, d));
}

//Boolean / bitwise

static inline I32x2 I32x2_or(I32x2 a, I32x2 b) { NONE_OP2I(a.v[i] | b.v[i]); }
static inline I32x2 I32x2_and(I32x2 a, I32x2 b) { NONE_OP2I(a.v[i] & b.v[i]); }
static inline I32x2 I32x2_xor(I32x2 a, I32x2 b) { NONE_OP2I(a.v[i] ^ b.v[i]); }
static inline I32x2 I32x2_andnot(I32x2 a, I32x2 b) { NONE_OP2I(~a.v[i] & b.v[i]); }        //~a & b
static inline I32x2 I32x2_not(I32x2 a) { NONE_OP2I(~a.v[i]); }

static inline I32x2 I32x2_lsh32(I32x2 a, U8 bits) { return I32x2_create2(I32x2_x(a) << bits, I32x2_y(a) << bits); }
static inline I32x2 I32x2_rsh32(I32x2 a, U8 bits) {
	return I32x2_create2((I32)((U32)I32x2_x(a) >> bits), (I32)((U32)I32x2_y(a) >> bits));
}

#ifdef __cplusplus
	}
#endif
