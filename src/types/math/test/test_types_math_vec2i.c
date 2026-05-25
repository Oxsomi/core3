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

//types/math/test/test_types_math_vec2i.c

#include "test_types_math_shared.h"
#include "types/math/vec2i.h"

void Test_vec2i(Test *test) {

	Test_setModule(test, "I32x2");
	
	//Create, accessors, swizzle

	I32x2 v2 = I32x2_create2(1, -2);
	Test_assert(test, "I32x2_x/y",        I32x2_x(v2) == 1 && I32x2_y(v2) == -2);

	I32x2_setRefX(&v2, 3);
	I32x2_setRefY(&v2, -4);
	Test_assert(test, "I32x2_setRef/get", I32x2_get(v2, 0) == 3 && I32x2_get(v2, 1) == -4);

	I32x2 yx = I32x2_yx(v2);
	Test_assert(test, "I32x2_yx",         I32x2_x(yx) == -4 && I32x2_y(yx) == 3);

	//Comparisons

	I32x2 a = I32x2_create2(2, 5);
	I32x2 b = I32x2_create2(3, 5);
	Test_assert(test, "I32x2_leq",        I32x2_all(I32x2_leq(a, b)));
	Test_assert(test, "I32x2_geq",        I32x2_all(I32x2_geq(b, a)));
	Test_assert(test, "I32x2_lt",         I32x2_eq2(I32x2_lt(a, b), I32x2_create2(1, 0)));
	Test_assert(test, "I32x2_gt",         I32x2_eq2(I32x2_gt(b, a), I32x2_create2(1, 0)));
	Test_assert(test, "I32x2_eq",         I32x2_eq2(a, I32x2_create2(2, 5)));
	Test_assert(test, "I32x2_neq",        I32x2_neq2(a, b));

	//Arithmetic and bitwise

	a = I32x2_create2(2, -3);
	b = I32x2_create2(5, 4);
	Test_assert(test, "I32x2_add",        !I32x2_neq2(I32x2_add(a, b), I32x2_create2(7, 1)));
	Test_assert(test, "I32x2_sub",        !I32x2_neq2(I32x2_sub(a, b), I32x2_create2(-3, -7)));
	Test_assert(test, "I32x2_mul",        !I32x2_neq2(I32x2_mul(a, b), I32x2_create2(10, -12)));
	Test_assert(test, "I32x2_div",        !I32x2_neq2(I32x2_div(b, a), I32x2_create2(2, -1)));

	I32x2 i3_6 = I32x2_create2(3, 6);
	Test_assert(test, "I32x2_and",        !I32x2_neq2(I32x2_and(i3_6,  I32x2_create2(5, 3)), I32x2_create2(1, 2)));
	Test_assert(test, "I32x2_or",         !I32x2_neq2(I32x2_or (i3_6,  I32x2_create2(5, 3)), I32x2_create2(7, 7)));
	Test_assert(test, "I32x2_xor",        !I32x2_neq2(I32x2_xor(i3_6,  I32x2_create2(5, 3)), I32x2_create2(6, 5)));
	Test_assert(test, "I32x2_not",        !I32x2_neq2(I32x2_not(I32x2_create2(0, -1)),       I32x2_create2(-1, 0)));

	I32x2 andnot = I32x2_andnot(I32x2_create2(0b1100, 0b1010), I32x2_create2(0b1010, 0b1100));
	Test_assert(test, "I32x2_andnot",     !I32x2_neq2(andnot, I32x2_create2(0b0010, 0b0100)));

	a = I32x2_create2(1, -8);
	Test_assert(test, "I32x2_lsh32",      !I32x2_neq2(I32x2_lsh32(a, 2), I32x2_create2(4, -32)));
	Test_assert(test, "I32x2_rsh32",      !I32x2_neq2(I32x2_rsh32(a, 1), I32x2_create2(0, (U32)(I32)-8 >> 1)));

	Test_assert(test, "I32x2_mod",        !I32x2_neq2(I32x2_mod(I32x2_create2(13, -13), I32x2_xx2(5)), I32x2_create2(3, 2)));
	Test_assert(test, "I32x2_pow2",       !I32x2_neq2(I32x2_pow2(I32x2_create2(3, 5)),  I32x2_create2(9, 25)));

	//min/max/clamp/sign/abs

	I32x2 clamped = I32x2_clamp(I32x2_create2(-1, 5), I32x2_zero, I32x2_two);
	Test_assert(test, "I32x2_clamp", !I32x2_neq2(clamped, I32x2_create2(0, 2)));

	a = I32x2_create2(3, -5);
	b = I32x2_create2(2,  2);
	Test_assert(test, "I32x2_min",        !I32x2_neq2(I32x2_min(a, b),                                 I32x2_create2(2, -5)));
	Test_assert(test, "I32x2_max",        !I32x2_neq2(I32x2_max(a, b),                                 I32x2_create2(3, 2)));
	Test_assert(test, "I32x2_sign",       !I32x2_neq2(I32x2_sign(I32x2_create2(-3, 4)),                I32x2_create2(-1, 1)));
	Test_assert(test, "I32x2_abs",        !I32x2_neq2(I32x2_abs(I32x2_create2(-3, 4)),                 I32x2_create2(3, 4)));

	//swapEndianness

	I32x2 endian = I32x2_swapEndianness(I32x2_create2(0x11223344, 0xAABBCCDD));
	Test_assert(test, "I32x2_swapEndianness", !I32x2_neq2(endian, I32x2_create2(0x44332211, 0xDDCCBBAA)));
}
