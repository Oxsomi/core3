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

#include "types/math/vec.h"
#include "types/math/type_cast.h"
#include "types/base/error.h"

BUFFER_OP_IMPL(I32x4);

//Convert raw bits to data type

F32x4 F32x4_bitsI32x4(I32x4 a) {
	const void *aptr = &a;
	return *(const F32x4*) aptr;
}

I32x4 I32x4_bitsF32x4(F32x4 a) {
	const void *aptr = &a;
	return *(const I32x4*) aptr;
}

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

I32x4 I32x4_sign(I32x4 v) {
	return I32x4_add(
		I32x4_mul(I32x4_lt(v, I32x4_zero()), I32x4_two()),
		I32x4_one()
	);
}

I32x4 I32x4_abs(I32x4 v){ return I32x4_mul(I32x4_sign(v), v); }

//Simple definitions

I32x4 I32x4_one() { return I32x4_xxxx4(1); }
I32x4 I32x4_two() { return I32x4_xxxx4(2); }
I32x4 I32x4_negOne() { return I32x4_xxxx4(-1); }
I32x4 I32x4_negTwo() { return I32x4_xxxx4(-2); }

Bool I32x4_all(I32x4 a) { return I32x4_reduce(I32x4_neq(a, I32x4_zero())) == 4; }
Bool I32x4_any(I32x4 a) { return I32x4_reduce(I32x4_neq(a, I32x4_zero())); }

I32x4 I32x4_load1(const void *arr) {
	I32x4 result = I32x4_zero();
	if(arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32)), Buffer_createRefConst(arr, sizeof(I32)));
	return result;
}

I32x4 I32x4_load2(const void *arr) {
	I32x4 result = I32x4_zero();
	if(arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32) * 2), Buffer_createRefConst(arr, sizeof(I32) * 2));
	return result;
}

I32x4 I32x4_load3(const void *arr) {
	I32x4 result = I32x4_zero();
	if(arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32) * 3), Buffer_createRefConst(arr, sizeof(I32) * 3));
	return result;
}

I32x4 I32x4_load4(const void *arr) {
	I32x4 result = I32x4_zero();
	if(arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32) * 4), Buffer_createRefConst(arr, sizeof(I32) * 4));
	return result;
}

I32x4 I32x4_swapEndianness(I32x4 v) {

	//TODO: This could also just be 1 swizzle if U8x16 was available

	I32x4 swapped = I32x4_wzyx(v);

	swapped = I32x4_or(I32x4_lsh32(swapped, 16), I32x4_rsh32(swapped, 16));		//Swap low 16 with hi 16

	const I32x4 byteMask = I32x4_xxxx4(0x00FF00FF);
	const I32x4 lo = I32x4_and(swapped, byteMask);
	const I32x4 hi = I32x4_and(I32x4_rsh32(swapped, 8), byteMask);

	return I32x4_or(I32x4_lsh32(lo, 8), hi);
}

void I32x4_setX(I32x4 *a, I32 v) { if(a) *a = I32x4_create4(v,				I32x4_y(*a),	I32x4_z(*a),	I32x4_w(*a)); }
void I32x4_setY(I32x4 *a, I32 v) { if(a) *a = I32x4_create4(I32x4_x(*a),	v,				I32x4_z(*a),	I32x4_w(*a)); }
void I32x4_setZ(I32x4 *a, I32 v) { if(a) *a = I32x4_create4(I32x4_x(*a),	I32x4_y(*a),	v,				I32x4_w(*a)); }
void I32x4_setW(I32x4 *a, I32 v) { if(a) *a = I32x4_create4(I32x4_x(*a),	I32x4_y(*a),	I32x4_z(*a),	v); }

void I32x4_set(I32x4 *a, U8 i, I32 v) {
	switch (i & 3) {
		case 0:		I32x4_setX(a, v);	break;
		case 1:		I32x4_setY(a, v);	break;
		case 2:		I32x4_setZ(a, v);	break;
		default:	I32x4_setW(a, v);
	}
}

I32 I32x4_get(I32x4 a, U8 i) {
	switch (i & 3) {
		case 0:		return I32x4_x(a);
		case 1:		return I32x4_y(a);
		case 2:		return I32x4_z(a);
		default:	return I32x4_w(a);
	}
}

Bool I32x4_eq4(I32x4 a, I32x4 b) { return I32x4_all(I32x4_eq(a, b)); }
Bool I32x4_neq4(I32x4 a, I32x4 b) { return !I32x4_eq4(a, b); }

I32x4 I32x4_complement(I32x4 a) { return I32x4_sub(I32x4_one(), a); }
I32x4 I32x4_negate(I32x4 a) { return I32x4_sub(I32x4_zero(), a); }

I32x4 I32x4_rol(I32x4 a, U8 bits) {
	bits &= 31;
	return I32x4_or(I32x4_lsh32(a, bits), I32x4_rsh32(a, 32 - bits));
}

I32x4 I32x4_ror(I32x4 a, U8 bits) {
	bits &= 31;
	return I32x4_or(I32x4_rsh32(a, bits), I32x4_lsh32(a, 32 - bits));
}

I32x4 I32x4_pow2(I32x4 a) { return I32x4_mul(a, a); }

I32x4 I32x4_mod(I32x4 v, I32x4 d) { return I32x4_sub(v, I32x4_mul(I32x4_div(v, d), d)); }
I32x4 I32x4_clamp(I32x4 a, I32x4 mi, I32x4 ma) { return I32x4_max(mi, I32x4_min(ma, a)); }

//4D Swizzles

#undef I32x4_expand4
#undef I32x4_expand3
#undef I32x4_expand2
#undef I32x4_expand

#define I32x4_expand4(xv, x0, yv, y0, zv, z0, wv, w0)							\
I32x4 I32x4_##xv##yv##zv##wv(I32x4 a) { return vecShufflei(a, x0, y0, z0, w0); }

#define I32x4_expand3(...)												\
I32x4_expand4(__VA_ARGS__, x, 0); I32x4_expand4(__VA_ARGS__, y, 1);	\
I32x4_expand4(__VA_ARGS__, z, 2); I32x4_expand4(__VA_ARGS__, w, 3);

#define I32x4_expand2(...)												\
I32x4_expand3(__VA_ARGS__, x, 0); I32x4_expand3(__VA_ARGS__, y, 1);	\
I32x4_expand3(__VA_ARGS__, z, 2); I32x4_expand3(__VA_ARGS__, w, 3);

#define I32x4_expand(...)												\
I32x4_expand2(__VA_ARGS__, x, 0); I32x4_expand2(__VA_ARGS__, y, 1);	\
I32x4_expand2(__VA_ARGS__, z, 2); I32x4_expand2(__VA_ARGS__, w, 3);

I32x4_expand(x, 0);
I32x4_expand(y, 1);
I32x4_expand(z, 2);
I32x4_expand(w, 3);

//3D swizzles

#undef I32x3_expand3
#undef I32x3_expand2
#undef I32x3_expand

#define I32x3_expand3(xv, yv, zv) I32x4 I32x4_##xv##yv##zv(I32x4 a) { return I32x4_trunc3(I32x4_##xv##yv##zv##x(a)); }

#define I32x3_expand2(...)										\
I32x3_expand3(__VA_ARGS__, x); I32x3_expand3(__VA_ARGS__, y); \
I32x3_expand3(__VA_ARGS__, z); I32x3_expand3(__VA_ARGS__, w);

#define I32x3_expand(...)										\
I32x3_expand2(__VA_ARGS__, x); I32x3_expand2(__VA_ARGS__, y); \
I32x3_expand2(__VA_ARGS__, z); I32x3_expand2(__VA_ARGS__, w);

I32x3_expand(x);
I32x3_expand(y);
I32x3_expand(z);
I32x3_expand(w);

//2D swizzles

#undef I32x2_expand2
#undef I32x2_expand

#define I32x2_expand2(xv, yv)													 \
I32x4 I32x4_##xv##yv##4(I32x4 a) { return I32x4_trunc2(I32x4_##xv##yv##xx(a)); } \
I32x2 I32x4_##xv##yv(I32x4 a) { return I32x2_fromI32x4(I32x4_##xv##yv##xx(a)); }

#define I32x2_expand(...)										\
I32x2_expand2(__VA_ARGS__, x); I32x2_expand2(__VA_ARGS__, y); \
I32x2_expand2(__VA_ARGS__, z); I32x2_expand2(__VA_ARGS__, w);

I32x2_expand(x);
I32x2_expand(y);
I32x2_expand(z);
I32x2_expand(w);

//Generic helper functions
//Adapted from https://stackoverflow.com/questions/17610696/shift-a-m128i-of-n-bits

I32x4 I32x4_lsh128(I32x4 a, U8 bits) {

	const I32x4 b = a;
	a = I32x4_lshByte(a, 8);

	if (bits >= 64)
		return I32x4_lsh64(a, bits - 64);

	a = I32x4_rsh64(a, 64 - bits);
	return I32x4_or(I32x4_lsh64(b, bits), a);
}

I32x4 I32x4_rsh128(I32x4 a, U8 bits) {

	const I32x4 b = a;
	a = I32x4_rshByte(a, 8);

	if (bits >= 64)
		return I32x4_rsh64(a, bits - 64);

	a = I32x4_lsh64(a, 64 - bits);
	return I32x4_or(I32x4_rsh64(b, bits), a);
}
