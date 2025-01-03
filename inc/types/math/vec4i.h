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

F32x4 F32x4_bitsI32x4(I32x4 a);
I32x4 I32x4_bitsF32x4(F32x4 a);

//Arithmetic

impl I32x4 I32x4_zero();
I32x4 I32x4_one();
I32x4 I32x4_two();
I32x4 I32x4_negTwo();
I32x4 I32x4_negOne();
impl I32x4 I32x4_xxxx4(I32 x);

impl I32x4 I32x4_add(I32x4 a, I32x4 b);
impl I32x4 I32x4_addI64x2(I32x4 a, I32x4 b);			//Add two U64x2s
impl I32x4 I32x4_sub(I32x4 a, I32x4 b);
impl I32x4 I32x4_mul(I32x4 a, I32x4 b);
impl I32x4 I32x4_mulU32x2AsU64x2(I32x4 a, I32x4 b);		//U32x4 as U64x2 * U64x2 (clear upper 32-bit)
impl I32x4 I32x4_div(I32x4 a, I32x4 b);

I32x4 I32x4_complement(I32x4 a);
I32x4 I32x4_negate(I32x4 a);

I32x4 I32x4_rol(I32x4 a, U8 bits);	//Rotate left (<<<)
I32x4 I32x4_ror(I32x4 a, U8 bits);	//Rotate right (>>>)

I32x4 I32x4_pow2(I32x4 a);

I32x4 I32x4_sign(I32x4 v);			//Zero counts as signed too
I32x4 I32x4_abs(I32x4 v);
I32x4 I32x4_mod(I32x4 v, I32x4 d);

impl I32 I32x4_reduce(I32x4 a);		//ax+ay+az+aw

impl I32x4 I32x4_min(I32x4 a, I32x4 b);
impl I32x4 I32x4_max(I32x4 a, I32x4 b);
I32x4 I32x4_clamp(I32x4 a, I32x4 mi, I32x4 ma);

//Boolean

Bool I32x4_all(I32x4 a);
Bool I32x4_any(I32x4 a);

impl I32x4 I32x4_eq(I32x4 a, I32x4 b);
impl I32x4 I32x4_neq(I32x4 a, I32x4 b);
impl I32x4 I32x4_geq(I32x4 a, I32x4 b);
impl I32x4 I32x4_gt(I32x4 a, I32x4 b);
impl I32x4 I32x4_leq(I32x4 a, I32x4 b);
impl I32x4 I32x4_lt(I32x4 a, I32x4 b);

impl I32x4 I32x4_or(I32x4 a, I32x4 b);
impl I32x4 I32x4_and(I32x4 a, I32x4 b);
impl I32x4 I32x4_andnot(I32x4 a, I32x4 b);		//~a & b
impl I32x4 I32x4_xor(I32x4 a, I32x4 b);

impl I32x4 I32x4_not(I32x4 a);

Bool I32x4_eq4(I32x4 a, I32x4 b);
Bool I32x4_neq4(I32x4 a, I32x4 b);

//Swizzles and shizzle

I32 I32x4_x(I32x4 a);
I32 I32x4_y(I32x4 a);
I32 I32x4_z(I32x4 a);
I32 I32x4_w(I32x4 a);
I32 I32x4_get(I32x4 a, U8 i);

void I32x4_setX(I32x4 *a, I32 v);
void I32x4_setY(I32x4 *a, I32 v);
void I32x4_setZ(I32x4 *a, I32 v);
void I32x4_setW(I32x4 *a, I32 v);
void I32x4_set(I32x4 *a, U8 i, I32 v);

//Construction

impl I32x4 I32x4_create1(I32 x);
impl I32x4 I32x4_create2(I32 x, I32 y);
impl I32x4 I32x4_create3(I32 x, I32 y, I32 z);
impl I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w);

impl I32x4 I32x4_createFromU64x2(U64 a, U64 b);
impl I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw);				//xyzw: 4-bit selector. x as b0, w as b3. 1 means b, 0 means 0.
impl I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v);		//Appends a before b and shifts with v I32s and truncates.

impl I32x4 I32x4_lshByte(I32x4 a, U8 bytes);	//Left shifting 8 bits per i as U128
impl I32x4 I32x4_rshByte(I32x4 a, U8 bytes);	//Right shifting 8 bits per i as U128

impl I32x4 I32x4_lsh32(I32x4 a, U8 bits);		//Left shifting each I32 individually per bit
impl I32x4 I32x4_rsh32(I32x4 a, U8 bits);		//Right shifting each I32 individually per bit

impl I32x4 I32x4_lsh64(I32x4 a, U8 bits);		//Left shifting each I64 individually per bit
impl I32x4 I32x4_rsh64(I32x4 a, U8 bits);		//Right shifting each I64 individually per bit

I32x4 I32x4_lsh128(I32x4 a, U8 bits);
I32x4 I32x4_rsh128(I32x4 a, U8 bits);

I32x4 I32x4_load1(const void *arr);
I32x4 I32x4_load2(const void *arr);
I32x4 I32x4_load3(const void *arr);
I32x4 I32x4_load4(const void *arr);

I32x4 I32x4_swapEndianness(I32x4 v);

//4D swizzles

#define I32x4_expand4(xv, yv, zv, wv) I32x4 I32x4_##xv##yv##zv##wv(I32x4 a)

#define I32x4_expand3(a, b, c)							\
I32x4_expand4(a, b, c, x); I32x4_expand4(a, b, c, y);	\
I32x4_expand4(a, b, c, z); I32x4_expand4(a, b, c, w)

#define I32x4_expand2(a, b)								\
I32x4_expand3(a, b, x); I32x4_expand3(a, b, y);			\
I32x4_expand3(a, b, z); I32x4_expand3(a, b, w)

#define I32x4_expand(a)									\
I32x4_expand2(a, x); I32x4_expand2(a, y);				\
I32x4_expand2(a, z); I32x4_expand2(a, w)

I32x4_expand(x);
I32x4_expand(y);
I32x4_expand(z);
I32x4_expand(w);

impl I32x4 I32x4_trunc2(I32x4 a);
impl I32x4 I32x4_trunc3(I32x4 a);

//2D swizzles

#define I32x2_expand2(xv, yv) I32x4 I32x4_##xv##yv##4(I32x4 a); I32x2 I32x4_##xv##yv(I32x4 a);

#define I32x2_expand(a)									\
I32x2_expand2(a, x); I32x2_expand2(a, y);				\
I32x2_expand2(a, z); I32x2_expand2(a, w);

I32x2_expand(x);
I32x2_expand(y);
I32x2_expand(z);
I32x2_expand(w);

//3D swizzles

#define I32x3_expand3(xv, yv, zv) I32x4 I32x4_##xv##yv##zv(I32x4 a);

#define I32x3_expand2(a, b)							\
I32x3_expand3(a, b, x); I32x3_expand3(a, b, y);		\
I32x3_expand3(a, b, z); I32x3_expand3(a, b, w);

#define I32x3_expand(a)								\
I32x3_expand2(a, x); I32x3_expand2(a, y);			\
I32x3_expand2(a, z); I32x3_expand2(a, w);

I32x3_expand(x);
I32x3_expand(y);
I32x3_expand(z);
I32x3_expand(w);

//Shuffling bytes

I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b);		//Shuffle bytes around. Useful for changing endianness for example

#ifdef __cplusplus
	}
#endif
