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

#pragma once

//TODO: Error checking

F32x2 F32x2_bitsI32x2(I32x2 a);
I32x2 I32x2_bitsF32x2(F32x2 a);

//Arithmetic

impl I32x2 I32x2_zero();
I32x2 I32x2_one();
I32x2 I32x2_two();
I32x2 I32x2_negTwo();
I32x2 I32x2_negOne();
impl I32x2 I32x2_xx2(I32 x);

impl I32x2 I32x2_add(I32x2 a, I32x2 b);
impl I32x2 I32x2_sub(I32x2 a, I32x2 b);
impl I32x2 I32x2_mul(I32x2 a, I32x2 b);
impl I32x2 I32x2_div(I32x2 a, I32x2 b);

I32x2 I32x2_complement(I32x2 a);
I32x2 I32x2_negate(I32x2 a);

I32x2 I32x2_pow2(I32x2 a);

I32x2 I32x2_sign(I32x2 v);			//Zero counts as signed too
I32x2 I32x2_abs(I32x2 v);
I32x2 I32x2_mod(I32x2 v, I32x2 d);

impl I32 I32x2_reduce(I32x2 a);		//ax+ay

impl I32x2 I32x2_min(I32x2 a, I32x2 b);
impl I32x2 I32x2_max(I32x2 a, I32x2 b);
I32x2 I32x2_clamp(I32x2 a, I32x2 mi, I32x2 ma);

//Boolean / bitwise

Bool I32x2_all(I32x2 a);
Bool I32x2_any(I32x2 a);

impl I32x2 I32x2_eq(I32x2 a, I32x2 b);
impl I32x2 I32x2_neq(I32x2 a, I32x2 b);
impl I32x2 I32x2_geq(I32x2 a, I32x2 b);
impl I32x2 I32x2_gt(I32x2 a, I32x2 b);
impl I32x2 I32x2_leq(I32x2 a, I32x2 b);
impl I32x2 I32x2_lt(I32x2 a, I32x2 b);

impl I32x2 I32x2_or(I32x2 a, I32x2 b);
impl I32x2 I32x2_and(I32x2 a, I32x2 b);
impl I32x2 I32x2_xor(I32x2 a, I32x2 b);

impl I32x2 I32x2_not(I32x2 a);

Bool I32x2_eq2(I32x2 a, I32x2 b);
Bool I32x2_neq2(I32x2 a, I32x2 b);

//Swizzles and shizzle

impl I32 I32x2_x(I32x2 a);
impl I32 I32x2_y(I32x2 a);
I32 I32x2_get(I32x2 a, U8 i);

void I32x2_setX(I32x2 *a, I32 v);
void I32x2_setY(I32x2 *a, I32 v);
void I32x2_set(I32x2 *a, U8 i, I32 v);

//Construction

I32x2 I32x2_create1(I32 x);
I32x2 I32x2_create2(I32 x, I32 y);

I32x2 I32x2_load1(const I32 *arr);
I32x2 I32x2_load2(const I32 *arr);

I32x2 I32x2_swapEndianness(I32x2 v);

I32x2 I32x2_xx(I32x2 a);
I32x2 I32x2_xy(I32x2 a);
I32x2 I32x2_yx(I32x2 a);
I32x2 I32x2_yy(I32x2 a);

//Casts from vec4i

I32x2 I32x2_fromI32x4(I32x4 a);
I32x4 I32x4_fromI32x2(I32x2 a);

//Cast from vec2f to vec4

I32x4 I32x4_create2_2(I32x2 a, I32x2 b);

I32x4 I32x4_create2_1_1(I32x2 a, I32 b, I32 c);
I32x4 I32x4_create1_2_1(I32 a, I32x2 b, I32 c);
I32x4 I32x4_create1_1_2(I32 a, I32 b, I32x2 c);

I32x4 I32x4_create2_1(I32x2 a, I32 b);
I32x4 I32x4_create1_2(I32 a, I32x2 b);
