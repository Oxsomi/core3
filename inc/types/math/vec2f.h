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

#ifdef __cplusplus
	extern "C" {
#endif

//TODO: Error checking

impl F32x2 F32x2_zero();
F32x2 F32x2_one();
F32x2 F32x2_two();
F32x2 F32x2_negTwo();
F32x2 F32x2_negOne();
impl F32x2 F32x2_xx2(F32 x);

//Arithmetic

impl F32x2 F32x2_add(F32x2 a, F32x2 b);
impl F32x2 F32x2_sub(F32x2 a, F32x2 b);
impl F32x2 F32x2_mul(F32x2 a, F32x2 b);
impl F32x2 F32x2_div(F32x2 a, F32x2 b);

F32x2 F32x2_complement(F32x2 a);
F32x2 F32x2_negate(F32x2 a);
F32x2 F32x2_inverse(F32x2 a);

F32x2 F32x2_pow2(F32x2 a);

impl F32x2 F32x2_min(F32x2 a, F32x2 b);
impl F32x2 F32x2_max(F32x2 a, F32x2 b);
impl F32x2 F32x2_clamp(F32x2 a, F32x2 mi, F32x2 ma);
impl F32x2 F32x2_saturate(F32x2 a);

impl F32x2 F32x2_acos(F32x2 v);
impl F32x2 F32x2_cos(F32x2 v);
impl F32x2 F32x2_asin(F32x2 v);
impl F32x2 F32x2_sin(F32x2 v);
impl F32x2 F32x2_atan(F32x2 v);
impl F32x2 F32x2_atan2(F32x2 y, F32x2 x);
impl F32x2 F32x2_tan(F32x2 v);
impl F32x2 F32x2_sqrt(F32x2 a);
impl F32x2 F32x2_rsqrt(F32x2 a);

F32x2 F32x2_sign(F32x2 v);			//Zero counts as signed too
F32x2 F32x2_abs(F32x2 v);
impl F32x2 F32x2_ceil(F32x2 v);
impl F32x2 F32x2_floor(F32x2 v);
impl F32x2 F32x2_round(F32x2 v);
F32x2 F32x2_fract(F32x2 v);
F32x2 F32x2_mod(F32x2 v, F32x2 d);

F32x2 F32x2_reflect(F32x2 i, F32x2 n);
impl F32 F32x2_dot(F32x2 a, F32x2 b);
F32 F32x2_satDot(F32x2 a, F32x2 b);

F32 F32x2_sqLen(F32x2 v);
F32 F32x2_len(F32x2 v);
F32x2 F32x2_normalize(F32x2 v);

impl F32x2 F32x2_pow(F32x2 v, F32x2 e);

impl F32x2 F32x2_loge(F32x2 v);
impl F32x2 F32x2_log10(F32x2 v);
impl F32x2 F32x2_log2(F32x2 v);

impl F32x2 F32x2_exp(F32x2 v);
impl F32x2 F32x2_exp10(F32x2 v);
impl F32x2 F32x2_exp2(F32x2 v);

impl F32 F32x2_reduce(F32x2 a);		//ax+ay

//Boolean / bitwise

Bool F32x2_all(F32x2 a);
Bool F32x2_any(F32x2 a);

impl F32x2 F32x2_eq(F32x2 a, F32x2 b);
impl F32x2 F32x2_neq(F32x2 a, F32x2 b);
impl F32x2 F32x2_geq(F32x2 a, F32x2 b);
impl F32x2 F32x2_gt(F32x2 a, F32x2 b);
impl F32x2 F32x2_leq(F32x2 a, F32x2 b);
impl F32x2 F32x2_lt(F32x2 a, F32x2 b);

Bool F32x2_eq2(F32x2 a, F32x2 b);
Bool F32x2_neq2(F32x2 a, F32x2 b);

impl F32x2 F32x2_fromI32x2(I32x2 a);
impl I32x2 I32x2_fromF32x2(F32x2 a);

F32x2 F32x2_fromF32x4(F32x4 a);
F32x4 F32x4_fromF32x2(F32x2 a);

//Swizzles and shizzle

impl F32 F32x2_x(F32x2 a);
impl F32 F32x2_y(F32x2 a);
F32 F32x2_get(F32x2 a, U8 i);

void F32x2_setX(F32x2 *a, F32 v);
void F32x2_setY(F32x2 *a, F32 v);
void F32x2_set(F32x2 *a, U8 i, F32 v);

//Construction

impl F32x2 F32x2_create1(F32 x);
F32x2 F32x2_create2(F32 x, F32 y);

F32x2 F32x2_load1(const void *arr);		//F32[1]
F32x2 F32x2_load2(const void *arr);		//F32[2]

impl F32x2 F32x2_xx(F32x2 a);
F32x2 F32x2_xy(F32x2 a);
impl F32x2 F32x2_yx(F32x2 a);
impl F32x2 F32x2_yy(F32x2 a);

F32x2 F32x2_mul2x2(F32x2 v2, F32x2 v2x2[2]);
F32x2 F32x2_mul2x3(F32x2 v2, F32x2 v2x3[3]);

//Cast from vec2 to vec4

F32x4 F32x4_create2_2(F32x2 a, F32x2 b);

F32x4 F32x4_create2_1_1(F32x2 a, F32 b, F32 c);
F32x4 F32x4_create1_2_1(F32 a, F32x2 b, F32 c);
F32x4 F32x4_create1_1_2(F32 a, F32 b, F32x2 c);

F32x4 F32x4_create2_1(F32x2 a, F32 b);
F32x4 F32x4_create1_2(F32 a, F32x2 b);

#ifdef __cplusplus
	}
#endif
