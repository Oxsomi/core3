/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/vec.h"
#include "types/type_cast.h"
#include "types/buffer.h"

//Cast

F32x4 F32x4_fromI32x4(I32x4 a) {
	return (F32x4) { .v = {
		(F32) I32x4_x(a), (F32) I32x4_y(a), (F32) I32x4_z(a), (F32) I32x4_w(a)
	} };
}

I32x4 I32x4_fromF32x4(F32x4 a) {
	return (I32x4) { .v = {
		(I32) F32x4_x(a), (I32) F32x4_y(a), (I32) F32x4_z(a), (I32) F32x4_w(a)
	} };
}

F32x2 F32x2_fromI32x2(I32x2 a) { return (F32x2) { .v = { (F32) I32x2_x(a), (F32) I32x2_y(a) } }; }
I32x2 I32x2_fromF32x2(F32x2 a) { return (I32x2) { .v = { (I32) F32x2_x(a), (I32) F32x2_y(a) } }; }

//Arithmetic

I32x4 I32x4_add(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] + b.v[i])
F32x4 F32x4_add(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] + b.v[i])
I32x2 I32x2_add(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] + b.v[i])
F32x2 F32x2_add(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] + b.v[i])

I32x4 I32x4_sub(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] - b.v[i])
F32x4 F32x4_sub(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] - b.v[i])
I32x2 I32x2_sub(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] - b.v[i])
F32x2 F32x2_sub(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] - b.v[i])

I32x4 I32x4_mul(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] * b.v[i])

F32x4 F32x4_mul(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] * b.v[i])
I32x2 I32x2_mul(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] * b.v[i])
F32x2 F32x2_mul(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] * b.v[i])

I32x4 I32x4_div(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] / b.v[i])
F32x4 F32x4_div(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] / b.v[i])
I32x2 I32x2_div(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] / b.v[i])
F32x2 F32x2_div(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] / b.v[i])

//Used for big ints
//64-bit add but stored in 32-bit int
//64-bit mul but fetched as 32-bit int

I32x4 I32x4_addI64x2(I32x4 a, I32x4 b) {

	I32x4 res = a;

	for(U64 i = 0; i < 2; ++i)
		((I64*)&res)[i] += ((const I64*)&b)[i];

	return res;
}

I32x4 I32x4_mulU32x2AsU64x2(I32x4 a, I32x4 b) _NONE_OP4I(
	(I32)(U32)(((U64)(U32)a.v[i & ~1] * (U32)b.v[i & ~1]) >> ((i & 1) << 5))
)

//Swizzle

I32x4 I32x4_trunc2(I32x4 a) { return I32x4_create2(I32x4_x(a), I32x4_y(a)); }
F32x4 F32x4_trunc2(F32x4 a) { return F32x4_create2(F32x4_x(a), F32x4_y(a)); }

I32x4 I32x4_trunc3(I32x4 a) { return I32x4_create3(I32x4_x(a), I32x4_y(a), I32x4_z(a)); }
F32x4 F32x4_trunc3(F32x4 a) { return F32x4_create3(F32x4_x(a), F32x4_y(a), F32x4_z(a)); }

I32 I32x4_x(I32x4 a) { return a.v[0]; }
F32 F32x4_x(F32x4 a) { return a.v[0]; }
I32 I32x2_x(I32x2 a) { return a.v[0]; }
F32 F32x2_x(F32x2 a) { return a.v[0]; }

I32 I32x4_y(I32x4 a) { return a.v[1]; }
F32 F32x4_y(F32x4 a) { return a.v[1]; }
I32 I32x2_y(I32x2 a) { return a.v[1]; }
F32 F32x2_y(F32x2 a) { return a.v[1]; }

I32 I32x4_z(I32x4 a) { return a.v[2]; }
F32 F32x4_z(F32x4 a) { return a.v[2]; }

I32 I32x4_w(I32x4 a) { return a.v[3]; }
F32 F32x4_w(F32x4 a) { return a.v[3]; }

I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { return (I32x4) { .v = { x, y, z, w } }; }
F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { return (F32x4) { .v = { x, y, z, w } }; }

I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return I32x4_create4(x, y, z, 0); }
F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return F32x4_create4(x, y, z, 0); }

I32x4 I32x4_create2(I32 x, I32 y) { return I32x4_create4(x, y, 0, 0); }
F32x4 F32x4_create2(F32 x, F32 y) { return F32x4_create4(x, y, 0, 0); }
I32x2 I32x2_create2(I32 x, I32 y) { return (I32x2) { .v = { x, y } }; }
F32x2 F32x2_create2(F32 x, F32 y) { return (F32x2) { .v = { x, y } }; }

I32x4 I32x4_create1(I32 x) { return I32x4_create4(x, 0, 0, 0); }
F32x4 F32x4_create1(F32 x) { return F32x4_create4(x, 0, 0, 0); }
I32x2 I32x2_create1(I32 x) { return I32x2_create2(x, 0); }
F32x2 F32x2_create1(F32 x) { return F32x2_create2(x, 0); }

I32x4 I32x4_createFromU64x2(U64 i0, U64 i1) {
	I32x4 v;
	((U64*)&v)[0] = i0;
	((U64*)&v)[1] = i1;
	return v;
}

I32x4 I32x4_xxxx4(I32 x) { return I32x4_create4(x, x, x, x); }
F32x4 F32x4_xxxx4(F32 x) { return F32x4_create4(x, x, x, x); }
I32x2 I32x2_xx2(I32 x) { return I32x2_create2(x, x); }
F32x2 F32x2_xx2(F32 x) { return F32x2_create2(x, x); }

I32x4 I32x4_zero() { return I32x4_xxxx4(0); }
I32x2 I32x2_zero() { return I32x2_xx2(0); }
F32x4 F32x4_zero() { return F32x4_xxxx4(0); }
F32x2 F32x2_zero() { return F32x2_xx2(0); }

//Comparison

I32x4 I32x4_eq(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] == b.v[i])
I32x2 I32x2_eq(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] == b.v[i])
F32x4 F32x4_eq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] == b.v[i]))
F32x2 F32x2_eq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] == b.v[i]))

I32x4 I32x4_neq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] != b.v[i])
I32x2 I32x2_neq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] != b.v[i])
F32x4 F32x4_neq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] != b.v[i]))
F32x2 F32x2_neq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] != b.v[i]))

I32x4 I32x4_geq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] >= b.v[i])
I32x2 I32x2_geq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] >= b.v[i])
F32x4 F32x4_geq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] >= b.v[i]))
F32x2 F32x2_geq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] >= b.v[i]))

I32x4 I32x4_gt(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] > b.v[i])
I32x2 I32x2_gt(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] > b.v[i])
F32x4 F32x4_gt(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] > b.v[i]))
F32x2 F32x2_gt(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] > b.v[i]))

I32x4 I32x4_leq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] <= b.v[i])
I32x2 I32x2_leq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] <= b.v[i])
F32x4 F32x4_leq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] <= b.v[i]))
F32x2 F32x2_leq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] <= b.v[i]))

I32x4 I32x4_lt(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] < b.v[i])
I32x2 I32x2_lt(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] < b.v[i])
F32x4 F32x4_lt(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] < b.v[i]))
F32x2 F32x2_lt(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] < b.v[i]))

//Bitwise

I32x4 I32x4_or(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] | b.v[i])
I32x2 I32x2_or(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] | b.v[i])

I32x4 I32x4_and(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] & b.v[i])
I32x2 I32x2_and(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] & b.v[i])

I32x4 I32x4_xor(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] ^ b.v[i])
I32x2 I32x2_xor(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] ^ b.v[i])

I32x2 I32x2_not(I32x2 a) _NONE_OP2I(~a.v[i])
I32x4 I32x4_not(I32x4 a) _NONE_OP4I(~a.v[i])

//Min/max

I32x4 I32x4_min(I32x4 a, I32x4 b) _NONE_OP4I((I32) I64_min(a.v[i], b.v[i]))
I32x2 I32x2_min(I32x2 a, I32x2 b) _NONE_OP2I((I32) I64_min(a.v[i], b.v[i]))
F32x4 F32x4_min(F32x4 a, F32x4 b) _NONE_OP4F(F32_min(a.v[i], b.v[i]))
F32x2 F32x2_min(F32x2 a, F32x2 b) _NONE_OP2F(F32_min(a.v[i], b.v[i]))

I32x4 I32x4_max(I32x4 a, I32x4 b) _NONE_OP4I((I32) I64_max(a.v[i], b.v[i]))
I32x2 I32x2_max(I32x2 a, I32x2 b) _NONE_OP2I((I32) I64_max(a.v[i], b.v[i]))
F32x4 F32x4_max(F32x4 a, F32x4 b) _NONE_OP4F(F32_max(a.v[i], b.v[i]))
F32x2 F32x2_max(F32x2 a, F32x2 b) _NONE_OP2F(F32_max(a.v[i], b.v[i]))

//Reduce

I32 I32x4_reduce(I32x4 a) { return I32x4_x(a) + I32x4_y(a) + I32x4_z(a) + I32x4_w(a); }
F32 F32x4_reduce(F32x4 a) { return F32x4_x(a) + F32x4_y(a) + F32x4_z(a) + F32x4_w(a); }

I32 I32x2_reduce(I32x2 a) { return I32x2_x(a) + I32x2_y(a); }
F32 F32x2_reduce(F32x2 a) { return F32x2_x(a) + F32x2_y(a); }

//Simple swizzle

I32x2 I32x2_xx(I32x2 a) { return I32x2_xx2(I32x2_x(a)); }
F32x2 F32x2_xx(F32x2 a) { return F32x2_xx2(F32x2_x(a)); }

I32x2 I32x2_yx(I32x2 a) { return I32x2_create2(I32x2_y(a), I32x2_x(a)); }
F32x2 F32x2_yx(F32x2 a) { return F32x2_create2(F32x2_y(a), F32x2_x(a)); }

F32x2 F32x2_yy(F32x2 a) { return F32x2_xx2(F32x2_y(a)); }
I32x2 I32x2_yy(I32x2 a) { return I32x2_xx2(I32x2_y(a)); }

//Float arithmetic

F32x4 F32x4_ceil(F32x4 a) _NONE_OP4F(F32_ceil(a.v[i]))
F32x2 F32x2_ceil(F32x2 a) _NONE_OP2F(F32_ceil(a.v[i]))

F32x4 F32x4_floor(F32x4 a) _NONE_OP4F(F32_floor(a.v[i]))
F32x2 F32x2_floor(F32x2 a) _NONE_OP2F(F32_floor(a.v[i]))

F32x4 F32x4_round(F32x4 a) _NONE_OP4F(F32_round(a.v[i]))
F32x2 F32x2_round(F32x2 a) _NONE_OP2F(F32_round(a.v[i]))

F32x4 F32x4_sqrt(F32x4 a) _NONE_OP4F(F32_sqrt(a.v[i]))
F32x2 F32x2_sqrt(F32x2 a) _NONE_OP2F(F32_sqrt(a.v[i]))

F32x4 F32x4_rsqrt(F32x4 a) _NONE_OP4F(1 / F32_sqrt(a.v[i]))
F32x2 F32x2_rsqrt(F32x2 a) _NONE_OP2F(1 / F32_sqrt(a.v[i]))

F32x4 F32x4_acos(F32x4 a) _NONE_OP4F(F32_acos(a.v[i]))
F32x2 F32x2_acos(F32x2 a) _NONE_OP2F(F32_acos(a.v[i]))

F32x4 F32x4_cos(F32x4 a) _NONE_OP4F(F32_cos(a.v[i]))
F32x2 F32x2_cos(F32x2 a) _NONE_OP2F(F32_cos(a.v[i]))

F32x4 F32x4_asin(F32x4 a) _NONE_OP4F(F32_asin(a.v[i]))
F32x2 F32x2_asin(F32x2 a) _NONE_OP2F(F32_asin(a.v[i]))

F32x4 F32x4_sin(F32x4 a) _NONE_OP4F(F32_sin(a.v[i]))
F32x2 F32x2_sin(F32x2 a) _NONE_OP2F(F32_sin(a.v[i]))

F32x4 F32x4_atan(F32x4 a) _NONE_OP4F(F32_atan(a.v[i]))
F32x2 F32x2_atan(F32x2 a) _NONE_OP2F(F32_atan(a.v[i]))

F32x4 F32x4_atan2(F32x4 a, F32x4 x) _NONE_OP4F(F32_atan2(a.v[i], x.v[i]))
F32x2 F32x2_atan2(F32x2 a, F32x2 x) _NONE_OP2F(F32_atan2(a.v[i], x.v[i]))

F32x4 F32x4_tan(F32x4 a)  _NONE_OP4F(F32_tan(a.v[i]))
F32x2 F32x2_tan(F32x2 a)  _NONE_OP2F(F32_tan(a.v[i]))

F32x4 F32x4_loge(F32x4 a)  _NONE_OP4F(F32_loge(a.v[i]))
F32x2 F32x2_loge(F32x2 a)  _NONE_OP2F(F32_loge(a.v[i]))

F32x4 F32x4_log10(F32x4 a)  _NONE_OP4F(F32_log10(a.v[i]))
F32x2 F32x2_log10(F32x2 a)  _NONE_OP2F(F32_log10(a.v[i]))

F32x4 F32x4_log2(F32x4 a)  _NONE_OP4F(F32_log2(a.v[i]))
F32x2 F32x2_log2(F32x2 a)  _NONE_OP2F(F32_log2(a.v[i]))

//These return 0 if an invalid value was returned. TODO: Make this conform better!

F32 F32_expeInternal(F32 v) { F32 v0 = 0; F32_expe(v, &v0); return v0; }
F32 F32_exp10Internal(F32 v) { F32 v0 = 0; F32_exp10(v, &v0); return v0; }
F32 F32_exp2Internal(F32 v) { F32 v0 = 0; F32_exp2(v, &v0); return v0; }

//

F32x4 F32x4_exp(F32x4 a) _NONE_OP4F(F32_expeInternal(a.v[i]))
F32x2 F32x2_exp(F32x2 a) _NONE_OP2F(F32_expeInternal(a.v[i]))

F32x4 F32x4_exp10(F32x4 a) _NONE_OP4F(F32_exp10Internal(a.v[i]))
F32x2 F32x2_exp10(F32x2 a) _NONE_OP2F(F32_exp10Internal(a.v[i]))

F32x2 F32x2_exp2(F32x2 a) _NONE_OP2F(F32_exp2Internal(a.v[i]))
F32x4 F32x4_exp2(F32x4 a) _NONE_OP4F(F32_exp2Internal(a.v[i]))

//Dot products

F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(a) * F32x4_x(b) + F32x4_y(a) * F32x4_y(b); }
F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_dot2(a, b) + F32x4_z(a) * F32x4_z(b); }
F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_dot3(a, b) + F32x4_w(a) * F32x4_w(b); }

F32 F32x2_dot(F32x2 a, F32x2 b) { return F32x4_dot2(F32x4_fromF32x2(a), F32x4_fromF32x2(b)); }

//Loading other byte

I32x4 I32x4_lshByte(I32x4 a, U8 bytes) {

	if(!bytes)
		return a;

	if(bytes >= 16)
		return I32x4_zero();

	I32x4 result = I32x4_zero();
	Buffer_copy(Buffer_createRef((U8*)&result + bytes, sizeof(result) - bytes), Buffer_createRefConst(&a, sizeof(a)));

	return result;
}

I32x4 I32x4_rshByte(I32x4 a, U8 bytes) {

	if(!bytes)
		return a;

	if(bytes >= 16)
		return I32x4_zero();

	I32x4 result = I32x4_zero();
	Buffer_copy(Buffer_createRef(&result, sizeof(result)), Buffer_createRefConst((U8*)&a + bytes, sizeof(a) - bytes));

	return result;
}

I32x4 I32x4_lsh32(I32x4 a, U8 bits) _NONE_OP4I((I32)(bits >= 32 ? 0 : (U32)a.v[i] << bits))
I32x4 I32x4_rsh32(I32x4 a, U8 bits) _NONE_OP4I((I32)(bits >= 32 ? 0 : (U32)a.v[i] >> bits))

I32x4 I32x4_lsh64(I32x4 a, U8 bits) {

	if(!bits)
		return a;

	if(bits >= 64)
		return I32x4_zero();

	I32x4 res = a;

	for(U64 i = 0; i < 2; ++i)
		((U64*)&res)[i] <<= bits;

	return res;
}

I32x4 I32x4_rsh64(I32x4 a, U8 bits)  {

	if(!bits)
		return a;

	if(bits >= 64)
		return I32x4_zero();

	I32x4 res = a;

	for(U64 i = 0; i < 2; ++i)
		((U64*)&res)[i] >>= bits;

	return res;
}

I32x4 I32x4_lshByte32(I32x4 a) {
	return I32x4_create4(
		0,
		I32x4_x(a),
		I32x4_y(a),
		I32x4_z(a)
	);
}

I32x4 I32x4_lshByte64(I32x4 a) {
	return I32x4_create4(
		0,
		0,
		I32x4_x(a),
		I32x4_y(a)
	);
}

I32x4 I32x4_lshByte96(I32x4 a) {
	return I32x4_create4(
		0,
		0,
		0,
		I32x4_x(a)
	);
}

//SHA256 helper functions

I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b) {

	U8 *ua = (U8*)&a;
	U8 *ub = (U8*)&b;
	U8 uc[16];

	for (U8 i = 0; i < 16; ++i) {

		if(ub[i] >> 7)
			uc[i] = 0;

		else uc[i] = ua[ub[i] & 0xF];
	}

	return *(const I32x4*)uc;
}

I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw) {

	for(U8 i = 0; i < 4; ++i)
		if((xyzw >> i) & 1)
			I32x4_set(&a, i, I32x4_get(b, i));

	return a;
}

I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v) {

	switch (v) {

		case 0:		return b;
		case 1:		return I32x4_create4(I32x4_w(a), I32x4_x(b), I32x4_y(b), I32x4_z(b));
		case 2:		return I32x4_create4(I32x4_z(a), I32x4_w(a), I32x4_x(b), I32x4_y(b));
		case 3:		return I32x4_create4(I32x4_y(a), I32x4_z(a), I32x4_w(a), I32x4_x(b));

		case 4:		return a;
		case 5:		return I32x4_create4(0, I32x4_x(a), I32x4_y(a), I32x4_z(a));
		case 6:		return I32x4_create4(0, 0, I32x4_x(a), I32x4_y(a));
		case 7:		return I32x4_create4(0, 0, 0, I32x4_x(a));

		default:	return I32x4_zero();
	}
}
