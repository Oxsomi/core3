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

#include "math/big_int.h"

U128 U128_create(const U8 data[16]) {
	return I32x4_createFromU64x2(((const U64*)data)[0], ((const U64*)data)[1]);
}

U128 U128_mul64(U64 au, U64 bu) {

	//We interpret au and bu as a U32[2] interleaved with an empty U32 each.
	// 
	//a0 a1 a2 a3 00 00 00 00 a4 a5 a6 a7 00 00 00 00
	//b0 b1 b2 b3 00 00 00 00 b4 b5 b6 b7 00 00 00 00
	//=
	//xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
	// 
	//This basically means we can use it as a multiply two U32s and not overflow.
	//However, this is not all that a multiplication has to do.
	//It has to do this operation for every single element and merge them together,
	//Respecting baseN offset.
	// 
	//For example:
	//1 2
	//3 4 x
	//=
	//--->1
	//0 2 
	//3 4 x
	//=
	//6 8
	//--->2
	//0 1
	//3 4 x
	//=
	//3 4 (shift e+1)
	//= 3 4 0
	//---> 4
	//340 + 68 = 408.
	//
	//a0 = 12 % 10 = 2
	//a1 = 12 / 10 = 1
	//
	//b0 = 34 % 10 = 4
	//b1 = 34 / 10 = 3
	//
	//From this we can understand that:
	//(a0 * b0) % 10 = (2 * 4) % 10 = 8 % 10 = 8 = digit0
	// c = ((a0 * b0) / 10 + (a0 * b1) + (a1 * b0))
	//   = (8 / 10 (int) + 6 + 4) % 10 = (0 + 6 + 4) = 10
	//c % 10 = 10 % 10 = 0 = digit1
	//((a1 * b1) + c / 10) % 10 = (3 + 10 / 10) = 4 = digit2
	//((a1 * b1) + c / 10) / 10 = 3 / 10 = 0 = digit3
	//0 4 0 8 = 1 2 * 3 4
	//
	// % 10 in baseU32 context is just a simple trunc from U64 to U32
	// / 10 in baseU32 context is just a simple shift by 32 from the U64

	#if _PLATFORM_TYPE == EPlatform_Linux

		return (__int128) au * (__int128) bu;

	#elif _SIMD == SIMD_NONE

		const U32 *a = (const U32*) &au;
		const U32 *b = (const U32*) &bu;

		U64 mul0[2] = { (U64)a[0] * b[0], (U64)a[0] * b[1] };				//digit 0, digit 1
		U64 mul1[2] = { (U64)a[1] * b[0], (U64)a[1] * b[1] };				//digit 1, digit 2

		//Digit 0 is always unmodified because if one of the bases grows it shifts it 1 digit

		U32 digit0 = (U32) mul0[0];

		//Add overflow from digit 0 and digit 1 from both

		U64 add0 = (U64)(mul0[0] >> 32) + (U32)mul0[1] + (U32)mul1[0];
		U32 digit1 = (U32) add0;

		//Add overflow from the last operation and the last mul

		U64 add1 = (U64)(mul0[1] >> 32) + (U64)(mul1[0] >> 32) + (U32)mul1[1] + (add0 >> 32);
		U32 digit2 = (U32) add1;

		//Do the same thing but once more

		U64 add2 = (U64)(mul1[1] >> 32) + (add1 >> 32);		//We can use the overflow if we want to, but ignore for now.
		U32 digit3 = (U32) add2;

		return I32x4_create4(digit0, digit1, digit2, digit3);

	#else

		//"Digits"

		I32x4 b = I32x4_create4((U16)(bu >>  0), (U16)(bu >> 16), (U16)(bu >> 32), (U16)(bu >> 48));

		I32x4 c0 = I32x4_mul(b, I32x4_create1((U16)(au >>  0)));
		I32x4 c1 = I32x4_mul(b, I32x4_create1((U16)(au >> 16)));
		I32x4 c2 = I32x4_mul(b, I32x4_create1((U16)(au >> 32)));
		I32x4 c3 = I32x4_mul(b, I32x4_create1((U16)(au >> 48)));

		//Reduce 

		//Generate final result

		return I32x4_create4(I32x4_x(c0), 0, 0, 0);

	#endif
}

U128 U128_add64(U64 a, U64 b) {
	U64 c = a + b;
	return I32x4_create4((U32)c, (U32)(c >> 32), c < a, 0);
}

U128 U128_zero() { return I32x4_zero(); }
U128 U128_one() { return I32x4_create4(0, 0, 0, 1); }

U128 U128_xor(U128 a, U128 b) { return I32x4_xor(a, b); }
U128 U128_or(U128 a, U128 b) { return I32x4_or(a, b); }
U128 U128_and(U128 a, U128 b) { return I32x4_and(a, b); }

//U128 U128_not(U128 a) { return I32x4_not(a); }

Bool U128_eq(U128 a, U128 b) { return I32x4_eq4(a, b); }
Bool U128_neq(U128 a, U128 b) { return !U128_eq(a, b); }
Bool U128_lt(U128 a, U128 b);
Bool U128_leq(U128 a, U128 b);
Bool U128_gt(U128 a, U128 b);
Bool U128_geq(U128 a, U128 b);

//U128 U128_div(U128 a, U128 b);
//U128 U128_mod(U128 a, U128 b);
U128 U128_mul(U128 a, U128 b);
U128 U128_add(U128 a, U128 b);
U128 U128_sub(U128 a, U128 b);

//U128 U128_negate(U128 a) { return U128_sub(U128_zero(), a); }
//U128 U128_complement(U128 a) { return U128_sub(U128_one(), a); }

U128 U128_min(U128 a, U128 b);
U128 U128_max(U128 a, U128 b);
U128 U128_clamp(U128 a, U128 mi, U128 ma);
