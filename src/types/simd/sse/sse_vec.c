/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//Cast it

I32x4 I32x4_fromF32x4(F32x4 a) { return _mm_cvtps_epi32(a); }
F32x4 F32x4_fromI32x4(I32x4 a) { return _mm_cvtepi32_ps(a); }
I32x2 I32x2_fromF32x2(F32x2 a) { return (I32x2) { .v = { (I32) F32x2_x(a), (I32) F32x2_y(a) } }; }
F32x2 F32x2_fromI32x2(I32x2 a) { return (F32x2) { .v = { (F32) I32x2_x(a), (F32) I32x2_y(a) } }; }

//Arithmetic

I32x4 I32x4_add(I32x4 a, I32x4 b) { return _mm_add_epi32(a, b); }
I32x4 I32x4_addI64x2(I32x4 a, I32x4 b) { return _mm_add_epi64(a, b); }
F32x4 F32x4_add(F32x4 a, F32x4 b) { return _mm_add_ps(a, b); }
I32x2 I32x2_add(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] + b.v[i])
F32x2 F32x2_add(F32x2 a, F32x2 b) NONE_OP2F(a.v[i] + b.v[i])

I32x4 I32x4_sub(I32x4 a, I32x4 b) { return _mm_sub_epi32(a, b); }
F32x4 F32x4_sub(F32x4 a, F32x4 b) { return _mm_sub_ps(a, b); }
I32x2 I32x2_sub(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] - b.v[i])
F32x2 F32x2_sub(F32x2 a, F32x2 b) NONE_OP2F(a.v[i] - b.v[i])

I32x4 I32x4_mul(I32x4 a, I32x4 b) { return _mm_mullo_epi32(a, b); }
I32x4 I32x4_mulU32x2AsU64x2(I32x4 a, I32x4 b) { return _mm_mul_epu32(a, b); }
F32x4 F32x4_mul(F32x4 a, F32x4 b) { return _mm_mul_ps(a, b); }
I32x2 I32x2_mul(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] * b.v[i])
F32x2 F32x2_mul(F32x2 a, F32x2 b) NONE_OP2F(a.v[i] * b.v[i])

I32x4 I32x4_div(I32x4 a, I32x4 b) { return _mm_div_epi32(a, b); }
F32x4 F32x4_div(F32x4 a, F32x4 b) { return _mm_div_ps(a, b); }
I32x2 I32x2_div(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] / b.v[i])
F32x2 F32x2_div(F32x2 a, F32x2 b) NONE_OP2F(a.v[i] / b.v[i])

//Swizzle

I32x4 _movelh_epi32(I32x4 a, I32x4 b) {
	return I32x4_bitsF32x4(_mm_movelh_ps(
		F32x4_bitsI32x4(a), F32x4_bitsI32x4(b)
	));
}

I32x4 I32x4_trunc2(I32x4 a) { return _movelh_epi32(a, I32x4_zero()); }
F32x4 F32x4_trunc2(F32x4 a) { return _mm_movelh_ps(a, F32x4_zero()); }

I32x4 I32x4_trunc3(I32x4 a) {
	const I32x4 z0 = I32x4_xzzz(_movelh_epi32(I32x4_zzzz(a), I32x4_zero()));
	return _movelh_epi32(a, z0);
}

F32x4 F32x4_trunc3(F32x4 a) {
	const F32x4 z0 = F32x4_xzzz(_mm_movelh_ps(F32x4_zzzz(a), F32x4_zero()));
	return _mm_movelh_ps(a, z0);
}

//Swizzle

I32 I32x4_x(I32x4 a) { return _mm_extract_epi32(a, 0); }
F32 F32x4_x(F32x4 a) { return _mm_cvtss_f32(a); }
I32 I32x2_x(I32x2 a) { return a.v[0]; }
F32 F32x2_x(F32x2 a) { return a.v[0]; }

I32 I32x4_y(I32x4 a) { return _mm_extract_epi32(a, 1);  }
F32 F32x4_y(F32x4 a) { return _mm_cvtss_f32(F32x4_yyyy(a)); ; }
I32 I32x2_y(I32x2 a) { return a.v[1]; }
F32 F32x2_y(F32x2 a) { return a.v[1]; }

I32 I32x4_z(I32x4 a) { return _mm_extract_epi32(a, 2);  }
F32 F32x4_z(F32x4 a) { return _mm_cvtss_f32(F32x4_zzzz(a)); }

I32 I32x4_w(I32x4 a) { return _mm_extract_epi32(a, 3); }
F32 F32x4_w(F32x4 a) { return _mm_cvtss_f32(F32x4_wwww(a)); }

I32x4 I32x4_create2(I32 x, I32 y) { return _mm_set_epi32(0, 0, y, x); }
F32x4 F32x4_create2(F32 x, F32 y) { return _mm_set_ps(0, 0, y, x); }
I32x2 I32x2_create2(I32 x, I32 y) { return (I32x2){ .v = { x, y } }; }
F32x2 F32x2_create2(F32 x, F32 y) { return (F32x2){ .v = { x, y } }; }

I32x4 I32x4_create1(I32 x) { return _mm_set_epi32(0, 0, 0, x); }
F32x4 F32x4_create1(F32 x) { return _mm_set_ps(0, 0, 0, x); }
I32x2 I32x2_create1(I32 x) { return I32x2_create2(x, 0); }
F32x2 F32x2_create1(F32 x) { return F32x2_create2(x, 0); }

F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return _mm_set_ps(0, z, y, x); }
I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return _mm_set_epi32(0, z, y, x); }

F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { return _mm_set_ps(w, z, y, x); }
I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { return _mm_set_epi32(w, z, y, x); }
I32x4 I32x4_createFromU64x2(U64 i0, U64 i1) { return _mm_set_epi64x(i1, i0); }

F32x4 F32x4_xxxx4(F32 x) { return _mm_set1_ps(x); }
I32x4 I32x4_xxxx4(I32 x) { return _mm_set1_epi32(x); }
F32x2 F32x2_xx2(F32 x) { return F32x2_create2(x, x); }
I32x2 I32x2_xx2(I32 x) { return I32x2_create2(x, x); }

I32x4 I32x4_zero() { return I32x4_xxxx4(0); }
F32x4 F32x4_zero() { return _mm_setzero_ps(); }
I32x2 I32x2_zero() { return (I32x2){ 0 }; }
F32x2 F32x2_zero() { return (F32x2){ 0 }; }

//Comparison

F32x4 F32x4_recastI32x4Internal(F32x4 a) { return F32x4_fromI32x4(*(const I32x4*) &a); }
F32x4 F32x4_negateRecastiInternal(F32x4 a) { return F32x4_negate(F32x4_recastI32x4Internal(a)); }

I32x4 I32x4_eq(I32x4 a, I32x4 b)  { return I32x4_negate(_mm_cmpeq_epi32(a, b)); }
F32x4 F32x4_eq(F32x4 a, F32x4 b)  { return F32x4_negateRecastiInternal(_mm_cmpeq_ps(a, b)); }
I32x2 I32x2_eq(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] == b.v[i])
F32x2 F32x2_eq(F32x2 a, F32x2 b) NONE_OP2F((F32)(a.v[i] == b.v[i]))

I32x4 I32x4_neq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmpeq_epi32(a, b)); }
F32x4 F32x4_neq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpneq_ps(a, b)); }
I32x2 I32x2_neq(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] != b.v[i])
F32x2 F32x2_neq(F32x2 a, F32x2 b) NONE_OP2F((F32)(a.v[i] != b.v[i]))

I32x4 I32x4_geq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmplt_epi32(a, b)); }
F32x4 F32x4_geq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpge_ps(a, b)); }
I32x2 I32x2_geq(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] >= b.v[i])
F32x2 F32x2_geq(F32x2 a, F32x2 b) NONE_OP2F((F32)(a.v[i] >= b.v[i]))

I32x4 I32x4_gt(I32x4 a, I32x4 b)  { return I32x4_negate(_mm_cmpgt_epi32(a, b)); }
F32x4 F32x4_gt(F32x4 a, F32x4 b)  { return F32x4_negateRecastiInternal(_mm_cmpgt_ps(a, b)); }
I32x2 I32x2_gt(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] > b.v[i])
F32x2 F32x2_gt(F32x2 a, F32x2 b) NONE_OP2F((F32)(a.v[i] > b.v[i]))

I32x4 I32x4_leq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmpgt_epi32(a, b)); }
F32x4 F32x4_leq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmple_ps(a, b)); }
I32x2 I32x2_leq(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] <= b.v[i])
F32x2 F32x2_leq(F32x2 a, F32x2 b) NONE_OP2F((F32)(a.v[i] <= b.v[i]))

I32x4 I32x4_lt(I32x4 a, I32x4 b)  { return I32x4_negate(_mm_cmplt_epi32(a, b)); }
F32x4 F32x4_lt(F32x4 a, F32x4 b)  { return F32x4_negateRecastiInternal(_mm_cmplt_ps(a, b)); }
I32x2 I32x2_lt(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] < b.v[i])
F32x2 F32x2_lt(F32x2 a, F32x2 b) NONE_OP2F((F32)(a.v[i] < b.v[i]))

//Bitwise

I32x4 I32x4_or(I32x4 a, I32x4 b) { return _mm_or_si128(a, b); }
I32x2 I32x2_or(I32x2 a, I32x2 b)  NONE_OP2I(a.v[i] | b.v[i])

I32x4 I32x4_and(I32x4 a, I32x4 b) { return _mm_and_si128(a, b); }
I32x2 I32x2_and(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] & b.v[i])

I32x4 I32x4_andnot(I32x4 a, I32x4 b) { return _mm_andnot_si128(a, b); }
I32x2 I32x2_andnot(I32x2 a, I32x2 b) NONE_OP2I((~a.v[i]) & b.v[i])

I32x4 I32x4_xor(I32x4 a, I32x4 b) { return _mm_xor_si128(a, b); }
I32x2 I32x2_xor(I32x2 a, I32x2 b) NONE_OP2I(a.v[i] ^ b.v[i])

I32x4 I32x4_not(I32x4 a) { return _mm_xor_si128(a, I32x4_xxxx4((I32)U32_MAX)); }
I32x2 I32x2_not(I32x2 a) NONE_OP2I(~a.v[i])

//Min/max

I32x4 I32x4_min(I32x4 a, I32x4 b) { return _mm_min_epi32(a, b); }
F32x4 F32x4_min(F32x4 a, F32x4 b) { return _mm_min_ps(a, b); }
I32x2 I32x2_min(I32x2 a, I32x2 b) { return I32x2_fromI32x4(I32x4_min(I32x4_fromI32x2(a), I32x4_fromI32x2(b))); }
F32x2 F32x2_min(F32x2 a, F32x2 b) { return F32x2_fromF32x4(F32x4_min(F32x4_fromF32x2(a), F32x4_fromF32x2(b))); }

I32x4 I32x4_max(I32x4 a, I32x4 b) { return _mm_max_epi32(a, b); }
F32x4 F32x4_max(F32x4 a, F32x4 b) { return _mm_max_ps(a, b); }
I32x2 I32x2_max(I32x2 a, I32x2 b) { return I32x2_fromI32x4(I32x4_max(I32x4_fromI32x2(a), I32x4_fromI32x2(b))); }
F32x2 F32x2_max(F32x2 a, F32x2 b) { return F32x2_fromF32x4(F32x4_max(F32x4_fromF32x2(a), F32x4_fromF32x2(b))); }

//Reduce

I32 I32x4_reduce(I32x4 a) {
	return I32x4_x(_mm_hadd_epi32(_mm_hadd_epi32(a, I32x4_zero()), I32x4_zero()));
}

F32 F32x4_reduce(F32x4 a) {
	return F32x4_x(_mm_hadd_ps(_mm_hadd_ps(a, F32x4_zero()), F32x4_zero()));
}

I32 I32x2_reduce(I32x2 a) { return I32x2_x(a) + I32x2_y(a); }
F32 F32x2_reduce(F32x2 a) { return F32x2_x(a) + F32x2_y(a); }

//Swizzle I32x2

I32x2 I32x2_xx(I32x2 a) { return I32x2_xx2(I32x2_x(a)); }
F32x2 F32x2_xx(F32x2 a) { return F32x2_xx2(F32x2_x(a)); }

I32x2 I32x2_yy(I32x2 a) { return I32x2_xx2(I32x2_y(a)); }
F32x2 F32x2_yy(F32x2 a) { return F32x2_xx2(F32x2_y(a)); }

I32x2 I32x2_yx(I32x2 a) { return I32x2_create2(I32x2_y(a), I32x2_x(a)); }
F32x2 F32x2_yx(F32x2 a) { return F32x2_create2(F32x2_y(a), F32x2_x(a)); }

//Float specific

//Rounding

F32x4 F32x4_ceil(F32x4 a) { return _mm_ceil_ps(a); }
F32x4 F32x4_floor(F32x4 a) { return _mm_floor_ps(a); }
F32x4 F32x4_round(F32x4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }

//Expensive math

F32x4 F32x4_pow(F32x4 v, F32x4 e) { return _mm_pow_ps(v, e); }
F32x4 F32x4_sqrt(F32x4 a) { return _mm_sqrt_ps(a); }
F32x4 F32x4_rsqrt(F32x4 a) { return _mm_rsqrt_ps(a); }

F32x4 F32x4_loge(F32x4 v) { return _mm_log_ps(v); }
F32x4 F32x4_log10(F32x4 v) { return _mm_log10_ps(v); }
F32x4 F32x4_log2(F32x4 v) { return _mm_log2_ps(v); }

F32x4 F32x4_exp(F32x4 v) { return _mm_exp_ps(v); }
F32x4 F32x4_exp10(F32x4 v) { return _mm_exp10_ps(v); }
F32x4 F32x4_exp2(F32x4 v)  { return _mm_exp2_ps(v); }

//Trig

F32x4 F32x4_acos(F32x4 v) { return _mm_acos_ps(v); }
F32x4 F32x4_cos(F32x4 v) { return _mm_cos_ps(v); }
F32x4 F32x4_asin(F32x4 v) { return _mm_asin_ps(v); }
F32x4 F32x4_sin(F32x4 v) { return _mm_sin_ps(v); }
F32x4 F32x4_atan(F32x4 v) { return _mm_atan_ps(v); }
F32x4 F32x4_atan2(F32x4 y, F32x4 x) { return _mm_atan2_ps(y, x); }
F32x4 F32x4_tan(F32x4 v) { return _mm_tan_ps(v); }

//Version for vec2

#define _F32x2_wrap1(x) { return F32x2_fromF32x4(x(F32x4_fromF32x2(a))); }
#define _F32x2_wrap2(x) { return F32x2_fromF32x4(x(F32x4_fromF32x2(a), F32x4_fromF32x2(b))); }

F32x2 F32x2_ceil(F32x2 a)  _F32x2_wrap1(F32x4_ceil)
F32x2 F32x2_floor(F32x2 a) _F32x2_wrap1(F32x4_floor)
F32x2 F32x2_round(F32x2 a) _F32x2_wrap1(F32x4_round)

F32x2 F32x2_pow(F32x2 a, F32x2 b) _F32x2_wrap2(F32x4_pow)
F32x2 F32x2_sqrt(F32x2 a)  _F32x2_wrap1(F32x4_sqrt)
F32x2 F32x2_rsqrt(F32x2 a) _F32x2_wrap1(F32x4_rsqrt)

F32x2 F32x2_loge(F32x2 a)  _F32x2_wrap1(F32x4_loge)
F32x2 F32x2_log10(F32x2 a) _F32x2_wrap1(F32x4_log10)
F32x2 F32x2_log2(F32x2 a)  _F32x2_wrap1(F32x4_log2)

F32x2 F32x2_exp(F32x2 a)   _F32x2_wrap1(F32x4_exp)
F32x2 F32x2_exp10(F32x2 a) _F32x2_wrap1(F32x4_exp10)
F32x2 F32x2_exp2(F32x2 a)  _F32x2_wrap1(F32x4_exp2)

F32x2 F32x2_acos(F32x2 a) _F32x2_wrap1(F32x4_acos)
F32x2 F32x2_cos(F32x2 a)  _F32x2_wrap1(F32x4_cos)
F32x2 F32x2_asin(F32x2 a) _F32x2_wrap1(F32x4_asin)
F32x2 F32x2_sin(F32x2 a)  _F32x2_wrap1(F32x4_sin)
F32x2 F32x2_atan(F32x2 a) _F32x2_wrap1(F32x4_atan)
F32x2 F32x2_tan(F32x2 a)  _F32x2_wrap1(F32x4_tan)
F32x2 F32x2_atan2(F32x2 a, F32x2 b) _F32x2_wrap2(F32x4_atan2)

//Dot products only need to trunc 1 of the two

F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(a, b, 0xFF)); }

F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(a, F32x4_xy4(b), 0xFF)); }
F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(a, F32x4_xyz(b), 0xFF)); }

F32 F32x2_dot(F32x2 a, F32x2 b) { return F32x4_dot2(F32x4_fromF32x2(a), F32x4_fromF32x2(b)); }

//Used for AES256 and shifting operations in math

I32x4 I32x4_lshByte(I32x4 a, U8 bytes) {
	switch (bytes) {
		case 0:		return a;
		FUNC_EXPAND8(1, _mm_slli_si128, a);
		FUNC_EXPAND4(9, _mm_slli_si128, a);
		FUNC_EXPAND2(13, _mm_slli_si128, a);
		case 15:	return _mm_slli_si128(a, 15);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_rshByte(I32x4 a, U8 bytes) {
	switch (bytes) {
		case 0:		return a;
		FUNC_EXPAND8(1, _mm_srli_si128, a);
		FUNC_EXPAND4(9, _mm_srli_si128, a);
		FUNC_EXPAND2(13, _mm_srli_si128, a);
		case 15:	return _mm_srli_si128(a, 15);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_lsh32(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm_slli_epi32, a);
		FUNC_EXPAND8(17, _mm_slli_epi32, a);
		FUNC_EXPAND4(25, _mm_slli_epi32, a);
		FUNC_EXPAND2(29, _mm_slli_epi32, a);
		case 31:	return _mm_slli_epi32(a, 31);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_rsh32(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm_srli_epi32, a);
		FUNC_EXPAND8(17, _mm_srli_epi32, a);
		FUNC_EXPAND4(25, _mm_srli_epi32, a);
		FUNC_EXPAND2(29, _mm_srli_epi32, a);
		case 31:	return _mm_srli_epi32(a, 31);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_lsh64(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND32(1, _mm_slli_epi64, a);
		FUNC_EXPAND16(33, _mm_slli_epi64, a);
		FUNC_EXPAND8(49, _mm_slli_epi64, a);
		FUNC_EXPAND4(57, _mm_slli_epi64, a);
		FUNC_EXPAND2(61, _mm_slli_epi64, a);
		case 63:	return _mm_slli_epi64(a, 63);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_rsh64(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND32(1, _mm_srli_epi64, a);
		FUNC_EXPAND16(33, _mm_srli_epi64, a);
		FUNC_EXPAND8(49, _mm_srli_epi64, a);
		FUNC_EXPAND4(57, _mm_srli_epi64, a);
		FUNC_EXPAND2(61, _mm_srli_epi64, a);
		case 63:	return _mm_srli_epi64(a, 63);
		default:	return I32x4_zero();
	}
}

//SHA256 helper functions

I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b) { return _mm_shuffle_epi8(a, b); }

I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw) {

	switch (xyzw & 0xF) {

		default:		return a;
		case 0b1111:	return b;

		case 0b0001:	return _mm_blend_epi16(a, b, 0x03);
		case 0b0010:	return _mm_blend_epi16(a, b, 0x0C);
		case 0b0011:	return _mm_blend_epi16(a, b, 0x0F);

		case 0b0100:	return _mm_blend_epi16(a, b, 0x30);
		case 0b0101:	return _mm_blend_epi16(a, b, 0x33);
		case 0b0110:	return _mm_blend_epi16(a, b, 0x3C);
		case 0b0111:	return _mm_blend_epi16(a, b, 0x3F);

		case 0b1000:	return _mm_blend_epi16(a, b, 0xC0);
		case 0b1001:	return _mm_blend_epi16(a, b, 0xC3);
		case 0b1010:	return _mm_blend_epi16(a, b, 0xCC);
		case 0b1011:	return _mm_blend_epi16(a, b, 0xCF);

		case 0b1100:	return _mm_blend_epi16(a, b, 0xF0);
		case 0b1101:	return _mm_blend_epi16(a, b, 0xF3);
		case 0b1110:	return _mm_blend_epi16(a, b, 0xFC);
	}
}

I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v) {

	switch (v) {

		case 0:		return b;
		case 1:		return _mm_alignr_epi8(a, b, 4);
		case 2:		return _mm_alignr_epi8(a, b, 8);
		case 3:		return _mm_alignr_epi8(a, b, 12);

		case 4:		return a;
		case 5:		return _mm_alignr_epi8(a, b, 20);
		case 6:		return _mm_alignr_epi8(a, b, 24);
		case 7:		return _mm_alignr_epi8(a, b, 28);

		default:	return I32x4_zero();
	}
}
