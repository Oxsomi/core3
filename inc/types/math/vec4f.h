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

//Arithmetic

impl F32x4 F32x4_zero();
F32x4 F32x4_one();
F32x4 F32x4_two();
F32x4 F32x4_negTwo();
F32x4 F32x4_negOne();
impl F32x4 F32x4_xxxx4(F32 x);

impl F32x4 F32x4_add(F32x4 a, F32x4 b);
impl F32x4 F32x4_sub(F32x4 a, F32x4 b);
impl F32x4 F32x4_mul(F32x4 a, F32x4 b);
impl F32x4 F32x4_div(F32x4 a, F32x4 b);

F32x4 F32x4_srgba8Unpack(U32 v);
U32   F32x4_srgba8Pack(F32x4 v);
F32x4 F32x4_rgb8Unpack(U32 v);
U32   F32x4_rgb8Pack(F32x4 v);

F32x4 F32x4_complement(F32x4 a);
F32x4 F32x4_negate(F32x4 a);
F32x4 F32x4_inverse(F32x4 a);

F32x4 F32x4_pow2(F32x4 a);

F32x4 F32x4_sign(F32x4 v);			//Zero counts as signed too
F32x4 F32x4_abs(F32x4 v);
impl F32x4 F32x4_ceil(F32x4 v);
impl F32x4 F32x4_floor(F32x4 v);
impl F32x4 F32x4_round(F32x4 v);
impl F32x4 F32x4_pow(F32x4 v, F32x4 e);
F32x4 F32x4_fract(F32x4 v);
F32x4 F32x4_mod(F32x4 v, F32x4 d);

impl F32 F32x4_reduce(F32x4 a);		//ax+ay+az+aw

impl F32 F32x4_dot2(F32x4 a, F32x4 b);
impl F32 F32x4_dot3(F32x4 a, F32x4 b);
impl F32 F32x4_dot4(F32x4 a, F32x4 b);

F32 F32x4_satDot2(F32x4 x, F32x4 y);
F32 F32x4_satDot3(F32x4 x, F32x4 y);
F32 F32x4_satDot4(F32x4 x, F32x4 y);

F32x4 F32x4_reflect2(F32x4 i, F32x4 n);
F32x4 F32x4_reflect3(F32x4 i, F32x4 n);

F32 F32x4_sqLen2(F32x4 v);
F32 F32x4_sqLen3(F32x4 v);
F32 F32x4_sqLen4(F32x4 v);

F32 F32x4_len2(F32x4 v);
F32 F32x4_len3(F32x4 v);
F32 F32x4_len4(F32x4 v);

impl F32x4 F32x4_acos(F32x4 v);
impl F32x4 F32x4_cos(F32x4 v);
impl F32x4 F32x4_asin(F32x4 v);
impl F32x4 F32x4_sin(F32x4 v);
impl F32x4 F32x4_atan(F32x4 v);
impl F32x4 F32x4_atan2(F32x4 y, F32x4 x);
impl F32x4 F32x4_tan(F32x4 v);
impl F32x4 F32x4_sqrt(F32x4 a);
impl F32x4 F32x4_rsqrt(F32x4 a);

F32x4 F32x4_normalize2(F32x4 v);
F32x4 F32x4_normalize3(F32x4 v);
F32x4 F32x4_normalize4(F32x4 v);

impl F32x4 F32x4_loge(F32x4 v);
impl F32x4 F32x4_log10(F32x4 v);
impl F32x4 F32x4_log2(F32x4 v);

impl F32x4 F32x4_exp(F32x4 v);
impl F32x4 F32x4_exp10(F32x4 v);
impl F32x4 F32x4_exp2(F32x4 v);

F32x4 F32x4_cross3(F32x4 a, F32x4 b);
F32x4 F32x4_lerp(F32x4 a, F32x4 b, F32 perc);

impl F32x4 F32x4_min(F32x4 a, F32x4 b);
impl F32x4 F32x4_max(F32x4 a, F32x4 b);
F32x4 F32x4_clamp(F32x4 a, F32x4 mi, F32x4 ma);
F32x4 F32x4_saturate(F32x4 a);

//Boolean

Bool F32x4_all(F32x4 a);
Bool F32x4_any(F32x4 a);

impl F32x4 F32x4_eq(F32x4 a, F32x4 b);
impl F32x4 F32x4_neq(F32x4 a, F32x4 b);
impl F32x4 F32x4_geq(F32x4 a, F32x4 b);
impl F32x4 F32x4_gt(F32x4 a, F32x4 b);
impl F32x4 F32x4_leq(F32x4 a, F32x4 b);
impl F32x4 F32x4_lt(F32x4 a, F32x4 b);

Bool F32x4_eq4(F32x4 a, F32x4 b);
Bool F32x4_neq4(F32x4 a, F32x4 b);

//Swizzles and shizzle

impl F32 F32x4_x(F32x4 a);
impl F32 F32x4_y(F32x4 a);
impl F32 F32x4_z(F32x4 a);
impl F32 F32x4_w(F32x4 a);
F32 F32x4_get(F32x4 a, U8 i);

void F32x4_setX(F32x4 *a, F32 v);
void F32x4_setY(F32x4 *a, F32 v);
void F32x4_setZ(F32x4 *a, F32 v);
void F32x4_setW(F32x4 *a, F32 v);
void F32x4_set(F32x4 *a, U8 i, F32 v);

//Construction

impl F32x4 F32x4_create1(F32 x);
impl F32x4 F32x4_create2(F32 x, F32 y);
impl F32x4 F32x4_create3(F32 x, F32 y, F32 z);
impl F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w);

F32x4 F32x4_load1(const void *arr);
F32x4 F32x4_load2(const void *arr);
F32x4 F32x4_load3(const void *arr);
F32x4 F32x4_load4(const void *arr);

//Shuffle and extracting values

#define vecShufflef1(a, x) vecShufflef(a, x, x, x, x)

//4D swizzles

#define F32x4_expand4(xv, yv, zv, wv) F32x4 F32x4_##xv##yv##zv##wv(F32x4 a);

#define F32x4_expand3(a, b, c)							\
F32x4_expand4(a, b, c, x); F32x4_expand4(a, b, c, y);	\
F32x4_expand4(a, b, c, z); F32x4_expand4(a, b, c, w);

#define F32x4_expand2(a, b)								\
F32x4_expand3(a, b, x); F32x4_expand3(a, b, y);			\
F32x4_expand3(a, b, z); F32x4_expand3(a, b, w);

#define F32x4_expand(a)									\
F32x4_expand2(a, x); F32x4_expand2(a, y);				\
F32x4_expand2(a, z); F32x4_expand2(a, w);

F32x4_expand(x);
F32x4_expand(y);
F32x4_expand(z);
F32x4_expand(w);

impl F32x4 F32x4_trunc2(F32x4 a);
impl F32x4 F32x4_trunc3(F32x4 a);

//2D swizzles

#define F32x2_expand2(xv, yv) F32x4 F32x4_##xv##yv##4(F32x4 a); F32x2 F32x4_##xv##yv(F32x4 a);

#define F32x2_expand(a)									\
F32x2_expand2(a, x); F32x2_expand2(a, y);				\
F32x2_expand2(a, z); F32x2_expand2(a, w);

F32x2_expand(x);
F32x2_expand(y);
F32x2_expand(z);
F32x2_expand(w);

//3D swizzles

#define F32x3_expand3(xv, yv, zv) F32x4 F32x4_##xv##yv##zv(F32x4 a);

#define F32x3_expand2(a, b)								\
F32x3_expand3(a, b, x); F32x3_expand3(a, b, y);			\
F32x3_expand3(a, b, z); F32x3_expand3(a, b, w);

#define F32x3_expand(a)									\
F32x3_expand2(a, x); F32x3_expand2(a, y);				\
F32x3_expand2(a, z); F32x3_expand2(a, w);

F32x3_expand(x);
F32x3_expand(y);
F32x3_expand(z);
F32x3_expand(w);

impl F32x4 F32x4_fromI32x4(I32x4 a);
impl I32x4 I32x4_fromF32x4(F32x4 a);

F32x4 F32x4_mul3x3(F32x4 v3, F32x4 v3x3[3]);
F32x4 F32x4_mul3x4(F32x4 v3, F32x4 v3x4[4]);

#ifdef __cplusplus
	}
#endif
