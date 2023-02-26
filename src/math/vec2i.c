/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "math/vec.h"
#include "types/type_cast.h"

F32x2 F32x2_bitsI32x2(I32x2 a) { return *(const F32x2*) &a; }		//Convert raw bits to data type
I32x2 I32x2_bitsF32x2(F32x2 a) { return *(const I32x2*) &a; }		//Convert raw bits to data type

I32x2 I32x2_complement(I32x2 a) { return I32x2_sub(I32x2_one(), a); }
I32x2 I32x2_negate(I32x2 a) { return I32x2_sub(I32x2_zero(), a); }

I32x2 I32x2_pow2(I32x2 a) { return I32x2_mul(a, a); }

I32x2 I32x2_mod(I32x2 v, I32x2 d) { return I32x2_sub(v, I32x2_mul(I32x2_div(v, d), d)); }

I32x2 I32x2_clamp(I32x2 a, I32x2 mi, I32x2 ma) { return I32x2_max(mi, I32x2_min(ma, a)); }

Bool I32x2_eq2(I32x2 a, I32x2 b) { return I32x2_all(I32x2_eq(a, b)); }
Bool I32x2_neq2(I32x2 a, I32x2 b) { return !I32x2_eq2(a, b); }

I32x2 I32x2_xy(I32x2 a) { return a; }

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

I32x2 I32x2_sign(I32x2 v) {
	return I32x2_add(
		I32x2_mul(I32x2_lt(v, I32x2_zero()), I32x2_two()), 
		I32x2_one()
	);
}

I32x2 I32x2_abs(I32x2 v){ return I32x2_mul(I32x2_sign(v), v); }

I32x2 I32x2_one() { return I32x2_xx2(1); }
I32x2 I32x2_two() { return I32x2_xx2(2); }
I32x2 I32x2_negOne() { return I32x2_xx2(-1); }
I32x2 I32x2_negTwo() { return I32x2_xx2(-2); }

Bool I32x2_all(I32x2 b) { return I32x2_reduce(I32x2_neq(b, I32x2_zero())) == 4; }
Bool I32x2_any(I32x2 b) { return I32x2_reduce(I32x2_neq(b, I32x2_zero())); }

I32x2 I32x2_load1(const I32 *arr) { return arr ? I32x2_create1(*arr) : I32x2_zero(); }
I32x2 I32x2_load2(const I32 *arr) { return arr ? I32x2_create2(*arr, arr[1]) : I32x2_zero(); }

I32x2 I32x2_swapEndianness(I32x2 v) {
	return I32x2_create2(I32_swapEndianness(I32x2_x(v)), I32_swapEndianness(I32x2_y(v)));
}

void I32x2_setX(I32x2 *a, I32 v) { if(a) *a = I32x2_create2(v, I32x2_y(*a)); }
void I32x2_setY(I32x2 *a, I32 v) { if(a) *a = I32x2_create2(I32x2_x(*a), v); }

void I32x2_set(I32x2 *a, U8 i, I32 v) {
	switch (i & 1) {
		case 0:		I32x2_setX(a, v);	break;
		default:	I32x2_setY(a, v);	break;
	}
}

I32 I32x2_get(I32x2 a, U8 i) {
	switch (i & 1) {
		case 0:		return I32x2_x(a);
		default:	return I32x2_y(a);
	}
}

I32x2 I32x2_fromI32x4(I32x4 a) { return I32x2_load2((const I32*) &a); }
I32x4 I32x4_fromI32x2(I32x2 a) { return I32x4_load2((const I32*) &a); }

//Cast from vec2f to vec4

I32x4 I32x4_create2_2(I32x2 a, I32x2 b) { return I32x4_create4(I32x2_x(a), I32x2_y(a), I32x2_x(b), I32x2_y(b)); }

I32x4 I32x4_create2_1_1(I32x2 a, I32 b, I32 c) { return I32x4_create4(I32x2_x(a), I32x2_y(a), b, c); }
I32x4 I32x4_create1_2_1(I32 a, I32x2 b, I32 c) { return I32x4_create4(a, I32x2_x(b), I32x2_y(b), c); }
I32x4 I32x4_create1_1_2(I32 a, I32 b, I32x2 c) { return I32x4_create4(a, b, I32x2_x(c), I32x2_y(c)); }

I32x4 I32x4_create2_1(I32x2 a, I32 b) { return I32x4_create3(I32x2_x(a), I32x2_y(a), b); }
I32x4 I32x4_create1_2(I32 a, I32x2 b) { return I32x4_create3(a, I32x2_x(b), I32x2_y(b)); }