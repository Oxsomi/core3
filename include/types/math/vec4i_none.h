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

//types/math/vec4i_none.h

#pragma once
#ifndef VEC4I_NONE_GUARD
	#error Vec4i None guard was undefined, this likely indicates include of vec4i_none.h was attempted instead of vec4i.h
#endif

#include "types/base/mathi.h"

//Loads

typedef union I32x4_U64x2 {
	I32x4 v;
	U64 v2[2];
} I32x4_U64x2;

static inline I32x4 I32x4_createFromU64x2(U64 i0, U64 i1) {
	I32x4_U64x2 v;
	v.v2[0] = i0;
	v.v2[1] = i1;
	return v.v;
}

//Swizzles

static inline I32 I32x4_x(I32x4 a) { return a.v[0]; }
static inline I32 I32x4_y(I32x4 a) { return a.v[1]; }
static inline I32 I32x4_z(I32x4 a) { return a.v[2]; }
static inline I32 I32x4_w(I32x4 a) { return a.v[3]; }

//TODO:
static inline I32x4 I32x4_setXCopy(I32x4 a, I32 v) { a.v[0] = v; return a; }
static inline I32x4 I32x4_setYCopy(I32x4 a, I32 v) { a.v[1] = v; return a; }
static inline I32x4 I32x4_setZCopy(I32x4 a, I32 v) { a.v[2] = v; return a; }
static inline I32x4 I32x4_setWCopy(I32x4 a, I32 v) { a.v[3] = v; return a; }

static inline void I32x4_setXRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(v, I32x4_y(*a), I32x4_z(*a), I32x4_w(*a)); }
static inline void I32x4_setYRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(I32x4_x(*a), v, I32x4_z(*a), I32x4_w(*a)); }
static inline void I32x4_setZRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(I32x4_x(*a), I32x4_y(*a), v, I32x4_w(*a)); }
static inline void I32x4_setWRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(I32x4_x(*a), I32x4_y(*a), I32x4_z(*a), v); }

static inline I32x4 I32x4_setCopy(I32x4 a, U8 i, I32 v) {
	switch (i & 3) {
		case 0:		return I32x4_setXCopy(a, v);
		case 1:		return I32x4_setYCopy(a, v);
		case 2:		return I32x4_setZCopy(a, v);
		default:	return I32x4_setWCopy(a, v);
	}
}

static inline void I32x4_setRef(I32x4 *a, U8 i, I32 v) {
	switch (i & 3) {
		case 0:		I32x4_setXRef(a, v);	break;
		case 1:		I32x4_setYRef(a, v);	break;
		case 2:		I32x4_setZRef(a, v);	break;
		default:	I32x4_setWRef(a, v);
	}
}

static inline I32 I32x4_get(I32x4 a, U8 i) {
	switch (i & 3) {
		case 0:		return I32x4_x(a);
		case 1:		return I32x4_y(a);
		case 2:		return I32x4_z(a);
		default:	return I32x4_w(a);
	}
}

static inline I32x4 I32x4_fromF32x4(F32x4 a) { NONE_OP4I((I32)a.v[i]); }

//Trunc & reduce

static inline I32x4 I32x4_trunc2(I32x4 a) { return I32x4_create2(I32x4_x(a), I32x4_y(a)); }
static inline I32x4 I32x4_trunc3(I32x4 a) { return I32x4_create3(I32x4_x(a), I32x4_y(a), I32x4_z(a)); }

static inline I32 I32x4_reduce(I32x4 a) { return I32x4_x(a) + I32x4_y(a) + I32x4_z(a) + I32x4_w(a); }

//Arithmetic

static inline I32x4 I32x4_add(I32x4 a, I32x4 b) { NONE_OP4I((I32)((U32)a.v[i] + (U32)b.v[i])); }
static inline I32x4 I32x4_sub(I32x4 a, I32x4 b) { NONE_OP4I((I32)((U32)a.v[i] - (U32)b.v[i])); }
static inline I32x4 I32x4_mul(I32x4 a, I32x4 b) { NONE_OP4I((I32)((I64)a.v[i] * (I64)b.v[i])); }

//Used for big ints
//64-bit add but stored in 32-bit int
//64-bit mul but fetched as 32-bit int

static inline I32x4 I32x4_addI64x2(I32x4 a, I32x4 b) {

	I32x4 res = a;

	for (U64 i = 0; i < 2; ++i)
		((I64 *)&res)[i] += ((const I64*)&b)[i];

	return res;
}

static inline I32x4 I32x4_mulU32x2AsU64x2(I32x4 a, I32x4 b) {
	NONE_OP4I(
		(I32)(U32)(((U64)(U32)a.v[i & ~1] * (U32)b.v[i & ~1]) >> ((i & 1) << 5))
	);
}

//Clamps

static inline I32x4 I32x4_min(I32x4 a, I32x4 b) { NONE_OP4I((I32)I64_min(a.v[i], b.v[i])); }
static inline I32x4 I32x4_max(I32x4 a, I32x4 b) { NONE_OP4I((I32)I64_max(a.v[i], b.v[i])); }

//Comparison

static inline I32x4 I32x4_eq(I32x4 a, I32x4 b)  { NONE_OP4I(a.v[i] == b.v[i]); }
static inline I32x4 I32x4_neq(I32x4 a, I32x4 b) { NONE_OP4I(a.v[i] != b.v[i]); }
static inline I32x4 I32x4_geq(I32x4 a, I32x4 b) { NONE_OP4I(a.v[i] >= b.v[i]); }
static inline I32x4 I32x4_gt(I32x4 a, I32x4 b)  { NONE_OP4I(a.v[i] > b.v[i]); }
static inline I32x4 I32x4_leq(I32x4 a, I32x4 b) { NONE_OP4I(a.v[i] <= b.v[i]); }
static inline I32x4 I32x4_lt(I32x4 a, I32x4 b)  { NONE_OP4I(a.v[i] < b.v[i]); }

//Bitwise

static inline I32x4 I32x4_or(I32x4 a, I32x4 b)  { NONE_OP4I(a.v[i] | b.v[i]); }
static inline I32x4 I32x4_and(I32x4 a, I32x4 b) { NONE_OP4I(a.v[i] & b.v[i]); }
static inline I32x4 I32x4_andnot(I32x4 a, I32x4 b) { NONE_OP4I((~a.v[i]) & b.v[i]); }
static inline I32x4 I32x4_xor(I32x4 a, I32x4 b) { NONE_OP4I(a.v[i] ^ b.v[i]); }
static inline I32x4 I32x4_not(I32x4 a) { NONE_OP4I(~a.v[i]); }
	
static inline I32x4 I32x4_lshElements(I32x4 a, U8 elements) {

	if (!elements)
		return a;

	if (elements >= 4)
		return I32x4_zero();

	const U8 bytes = elements * 4;

	I32x4 result = I32x4_zero();
	Buffer_memcpy(Buffer_createRef((U8*)&result + bytes, sizeof(result) - bytes), Buffer_createRefConst(&a, sizeof(a)));

	return result;
}

static inline I32x4 I32x4_rshElements(I32x4 a, U8 elements) {

	if (!elements)
		return a;

	if(elements >= 4)
		return I32x4_zero();

	const U8 bytes = elements * 4;

	I32x4 result = I32x4_zero();
	Buffer_memcpy(Buffer_createRef(&result, sizeof(result)), Buffer_createRefConst((U8*)&a + bytes, sizeof(a) - bytes));

	return result;
}

static inline I32x4 I32x4_lsh32(I32x4 a, U8 bits) { NONE_OP4I((I32)(bits >= 32 ? 0 : (U32)a.v[i] << bits)); }
static inline I32x4 I32x4_rsh32(I32x4 a, U8 bits) { NONE_OP4I((I32)(bits >= 32 ? 0 : (U32)a.v[i] >> bits)); }

static inline I32x4 I32x4_lsh64(I32x4 a, U8 bits) {

	if (!bits)
		return a;

	if (bits >= 64)
		return I32x4_zero();

	I32x4 res = a;

	for (U64 i = 0; i < 2; ++i)
		((U64 *)&res)[i] <<= bits;

	return res;
}

static inline I32x4 I32x4_rsh64(I32x4 a, U8 bits) {

	if (!bits)
		return a;

	if (bits >= 64)
		return I32x4_zero();

	I32x4 res = a;

	for (U64 i = 0; i < 2; ++i)
		((U64 *)&res)[i] >>= bits;

	return res;
}


//SHA256 helper functions

typedef union I32x4_U8x8 {
	I32x4 vec;
	U8 uc[16];
} I32x4_U8x8;

static inline I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b) {

	const U8 *ua = (U8 *)&a;
	const U8 *ub = (U8 *)&b;
	I32x4_U8x8 c = (I32x4_U8x8){ 0 };

	for (U8 i = 0; i < 16; ++i) {

		if (ub[i] >> 7)
			c.uc[i] = 0;

		else c.uc[i] = ua[ub[i] & 0xF];
	}

	return c.vec;
}

static inline I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw) {

	for (U8 i = 0; i < 4; ++i)
		if ((xyzw >> i) & 1)
			I32x4_setRef(&a, i, I32x4_get(b, i));

	return a;
}

static inline I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v) {

	switch (v) {
		default: return b;
		case 1:  return I32x4_create4(I32x4_y(b), I32x4_z(b), I32x4_w(b), I32x4_x(a));
		case 2:  return I32x4_create4(I32x4_z(b), I32x4_w(b), I32x4_x(a), I32x4_y(a));
		case 3:  return I32x4_create4(I32x4_w(b), I32x4_x(a), I32x4_y(a), I32x4_z(a));
	}
}

static inline I32x4 I32x4_swapEndianness(I32x4 v) {

	I32 v0 = v.v[0];		//Basically wzyx, but don't need to include it.
	I32 v1 = v.v[1];
	v.v[0] = v.v[3];
	v.v[1] = v.v[2];
	v.v[2] = v1;
	v.v[3] = v0;

	v = I32x4_or(I32x4_lsh32(v, 16), I32x4_rsh32(v, 16));

	const I32x4 m = I32x4_xxxx4(0x00FF00FF);
	return I32x4_or(
		I32x4_lsh32(I32x4_and(v, m), 8),
		I32x4_and(I32x4_rsh32(v, 8), m)
	);
}
