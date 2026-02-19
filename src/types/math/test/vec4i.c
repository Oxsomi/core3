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

#include "types/math/vec4i.h"
#include "types/math/u128.h"
#include "shared.h"

void Test_vec4i(Test *test) {

	Test_setModule(test, "I32x4");
	
	//Create, accessors

	I32x4 v4 = I32x4_create4(1, 2, 3, 4);
	Test_assert(test, "I32x4 accessors",  I32x4_x(v4) == 1 && I32x4_y(v4) == 2 && I32x4_z(v4) == 3 && I32x4_w(v4) == 4);

	I32x4 v1 = I32x4_create1(5);
	Test_assert(test, "I32x4_create1",    I32x4_x(v1) == 5 && I32x4_y(v1) == 0 && I32x4_z(v1) == 0 && I32x4_w(v1) == 0);

	I32x4_setZRef(&v4, 9);
	I32x4_setWRef(&v4, 10);
	Test_assert(test, "I32x4_setRef/get", I32x4_get(v4, 2) == 9 && I32x4_get(v4, 3) == 10);

	//Comparisons and masks

	I32x4 a = I32x4_create4(1, 2, 3, 4);
	I32x4 b = I32x4_create4(1, 3, 2, 4);
	Test_assert(test, "I32x4_eq4",   I32x4_eq4(a, I32x4_create4(1, 2, 3, 4)));
	Test_assert(test, "I32x4_neq4",  I32x4_neq4(a, b));
	Test_assert(test, "I32x4_lt",    I32x4_eq4(I32x4_lt(a, b),  I32x4_create4(0, 1, 0, 0)));
	Test_assert(test, "I32x4_geq",   I32x4_eq4(I32x4_geq(a, b), I32x4_create4(1, 0, 1, 1)));
	Test_assert(test, "I32x4_gt",    I32x4_eq4(I32x4_gt(a, b),  I32x4_create4(0, 0, 1, 0)));
	Test_assert(test, "I32x4_leq",   I32x4_eq4(I32x4_leq(a, b), I32x4_create4(1, 1, 0, 1)));
	Test_assert(test, "I32x4_all",   I32x4_all(I32x4_create4(1, -2, 3, 4)));
	Test_assert(test, "I32x4_any",   !I32x4_any(I32x4_zero()));

	//Arithmetic

	I32x4 mod = I32x4_mod(I32x4_create4(5, -5, 9, -9), I32x4_xxxx4(4));
	Test_assert(test, "I32x4_mod", !I32x4_neq4(mod, I32x4_create4(1, 3, 1, 3)));

	a = I32x4_create4(1, 2, 3, 4);
	b = I32x4_create4(5, 6, 7, 8);
	Test_assert(test, "I32x4_add",   !I32x4_neq4(I32x4_add(a, b), I32x4_create4(6, 8, 10, 12)));
	Test_assert(test, "I32x4_sub",   !I32x4_neq4(I32x4_sub(b, a), I32x4_create4(4, 4, 4, 4)));
	Test_assert(test, "I32x4_mul",   !I32x4_neq4(I32x4_mul(a, b), I32x4_create4(5, 12, 21, 32)));

	Test_assert(test, "I32x4_abs",   !I32x4_neq4(I32x4_abs(I32x4_create4(-3, 4, -1, 0)),  I32x4_create4(3, 4, 1, 0)));
	Test_assert(test, "I32x4_sign",  !I32x4_neq4(I32x4_sign(I32x4_create4(-3, 4, 0, 1)),  I32x4_create4(-1, 1, 1, 1)));
	Test_assert(test, "I32x4_pow2",  I32x4_eq4(I32x4_pow2(I32x4_create4(2, -3, 4, -5)),  I32x4_create4(4, 9, 16, 25)));

	//min/max/clamp

	a = I32x4_create4(-2, 5, 2, 10);
	Test_assert(test, "I32x4_min",   !I32x4_neq4(I32x4_min(a, I32x4_zero()), I32x4_create4(-2, 0, 0, 0)));
	Test_assert(test, "I32x4_max",   !I32x4_neq4(I32x4_max(a, I32x4_one()),  I32x4_create4(1, 5, 2, 10)));

	I32x4 clamped = I32x4_clamp(I32x4_create4(-1, 5, 1, 2), I32x4_zero(), I32x4_two());
	Test_assert(test, "I32x4_clamp", !I32x4_neq4(clamped, I32x4_create4(0, 2, 1, 2)));

	//Bitwise

	a = I32x4_create4(0x0F0F0F0F, 0xAAAAAAAA, 0x12345678, 0xFFFFFFFF);
	b = I32x4_create4(0x33333333, 0x55555555, 0x87654321, 0x0);
	Test_assert(test, "I32x4_and", !I32x4_neq4(I32x4_and(a, b), I32x4_create4(0x03030303, 0x00000000, 0x02244220, 0x0)));
	Test_assert(test, "I32x4_or",  !I32x4_neq4(I32x4_or (a, b), I32x4_create4(0x3F3F3F3F, 0xFFFFFFFF, 0x97755779, 0xFFFFFFFF)));
	Test_assert(test, "I32x4_xor", !I32x4_neq4(I32x4_xor(a, b), I32x4_create4(0x3C3C3C3C, 0xFFFFFFFF, 0x95511559, 0xFFFFFFFF)));
	Test_assert(test, "I32x4_not", !I32x4_neq4(I32x4_not(I32x4_zero()), I32x4_create4(-1, -1, -1, -1)));

	a = I32x4_create4(0xFFFF0000, 0xAAAAAAAA, 0x00000000, 0xFFFFFFFF);
	b = I32x4_create4(0x12345678, 0x55555555, 0xFFFFFFFF, 0x0F0F0F0F);
	I32x4 c = I32x4_create4(0x00005678, 0x55555555, 0xFFFFFFFF, 0x00000000);

	Test_assert(test, "I32x4_andnot", I32x4_eq4(I32x4_andnot(a, b), c));

	//Shifts and rotates

	v4 = I32x4_create4(1, 2, 4, 8);
	Test_assert(test, "I32x4_lsh32", !I32x4_neq4(I32x4_lsh32(v4, 1),                        I32x4_create4(2, 4, 8, 16)));
	Test_assert(test, "I32x4_rsh32", !I32x4_neq4(I32x4_rsh32(I32x4_create4(8, 4, 2, 1), 1), I32x4_create4(4, 2, 1, 0)));

	I32x4 rol = I32x4_rol(I32x4_create4((I32)0x80000001, 0, 0, 0), 1);
	Test_assert(test, "I32x4_rol",   I32x4_x(rol) == 0x00000003);

	I32x4 ror = I32x4_ror(I32x4_create4(0x00000003, 0, 0, 0), 1);
	Test_assert(test, "I32x4_ror",   I32x4_x(ror) == (I32)0x80000001);

	Test_assert(test, "I32x4_lshElements", I32x4_eq4(I32x4_lshElements(v4, 1), I32x4_create4(0, 1, 2, 4)));
	Test_assert(test, "I32x4_rshElements", I32x4_eq4(I32x4_rshElements(v4, 1), I32x4_create4(2, 4, 8, 0)));

	I32x4 one = I32x4_create4(0x00000001, 0, 0, 0);
	I32x4 l   = I32x4_lsh128(one, 32);
	Test_assert(test, "I32x4_lsh128",      I32x4_x(l) == 0 && I32x4_y(l) == 1);
	Test_assert(test, "I32x4_rsh128",      I32x4_eq4(I32x4_rsh128(l, 32), one));

	one = I32x4_create4(0x00000001, 0, 0x00000001, 0);
	l   = I32x4_lsh64(one, 1);
	Test_assert(test, "I32x4_lsh64",       I32x4_x(l) == 2 && I32x4_z(l) == 2);
	Test_assert(test, "I32x4_rsh64",       I32x4_eq4(I32x4_rsh64(l, 1), one));

	//reduce, trunc

	v4 = I32x4_create4(1, 2, 3, 4);
	Test_assert(test, "I32x4_reduce", I32x4_reduce(v4) == 10);
	Test_assert(test, "I32x4_trunc3", !I32x4_neq4(I32x4_trunc3(v4), I32x4_create4(1, 2, 3, 0)));
	Test_assert(test, "I32x4_trunc2", !I32x4_neq4(I32x4_trunc2(v4), I32x4_create4(1, 2, 0, 0)));

	//loads

	I32 vals[4] = { 7, 8, 9, 10 };
	Test_assert(test, "I32x4_load1", !I32x4_neq4(I32x4_load1(vals), I32x4_create4(7, 0, 0, 0)));
	Test_assert(test, "I32x4_load2", !I32x4_neq4(I32x4_load2(vals), I32x4_create4(7, 8, 0, 0)));
	Test_assert(test, "I32x4_load3", !I32x4_neq4(I32x4_load3(vals), I32x4_create4(7, 8, 9, 0)));
	Test_assert(test, "I32x4_load4", !I32x4_neq4(I32x4_load4(vals), I32x4_create4(7, 8, 9, 10)));

	//combineRightShift, blend, shuffleBytes

	a = I32x4_create4(1, 2, 3, 4);
	b = I32x4_create4(5, 6, 7, 8);
	Test_assert(test, "I32x4_combineRightShift", I32x4_eq4(I32x4_combineRightShift(a, b, 1), I32x4_create4(6, 7, 8, 1)));

	a = I32x4_create4(1,  2,  3,  4);
	b = I32x4_create4(10, 20, 30, 40);
	Test_assert(test, "I32x4_blend", I32x4_eq4(I32x4_blend(a, b, 0b0101), I32x4_create4(10, 2, 30, 4)));

	v4 = I32x4_create4(0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00);
	I32x4 mask = I32x4_create4(0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F);

	c = I32x4_create4(0x44332211, 0x88776655, 0xCCBBAA99, 0x00FFEEDD);
	Test_assert(test, "I32x4_shuffleBytes", I32x4_eq4(I32x4_shuffleBytes(v4, mask), c));

	//swapEndianness

	//TODO: Needs I32x2_swapEndianness to behave similarly or to add I32x4_reverse and I32x2_reverse

	I32x4 endian = I32x4_swapEndianness(I32x4_create4(0x11223344, 0xAABBCCDD, 0x11223344, 0xAABBCCDD));
	I32x4 endianResult = I32x4_create4(0xDDCCBBAA, 0x44332211, 0xDDCCBBAA, 0x44332211);
	Test_assert(test, "I32x4_swapEndianness", !I32x4_neq4(endian, endianResult));

	//clmul64

	const I32x4 clmulA[] = {
		I32x4_create4(0x89ABCDEF, 0x01234567, 0x0FEDCBA9, 0x76543210),
		I32x4_create4(0x89ABCDEF, 0x01234567, 0x0FEDCBA9, 0x76543210),
		I32x4_create4(0, 0, 1, 0),
		I32x4_create4(1 << 3, 0, 0, 0),
		I32x4_create4(0b0011, 0, 0, 0),
		I32x4_create4(2, 0, 4, 0),
		I32x4_create4(2, 0, 4, 0),
		I32x4_create4(2, 0, 4, 0),
		I32x4_create4(2, 0, 4, 0),
	};

	const I32x4 clmulB[] = {
		I32x4_create4(1, 0, 0, 0),
		I32x4_create4(0, 0, 1, 0),
		I32x4_create4(0x89ABCDEF, 0x01234567, 0x0FEDCBA9, 0x76543210),
		I32x4_create4(1 << 5, 0, 0, 0),
		I32x4_create4(0b0101, 0, 0, 0),
		I32x4_create4(8, 0, 16, 0),
		I32x4_create4(8, 0, 16, 0),
		I32x4_create4(8, 0, 16, 0),
		I32x4_create4(8, 0, 16, 0),
	};

	static const U8 clmulImm[] = { 0x00, 0x10, 0x11, 0x00, 0x00, 0x00, 0x10, 0x01, 0x11 };

	const I32x4 clmulExpect[] = {
		I32x4_create4(0x89ABCDEF, 0x01234567, 0, 0),
		I32x4_create4(0x89ABCDEF, 0x01234567, 0, 0),
		I32x4_create4(0x0FEDCBA9, 0x76543210, 0, 0),
		I32x4_create4(1 << 8, 0, 0, 0),
		I32x4_create4(0b1111, 0, 0, 0),
		I32x4_create4(16, 0, 0, 0),
		I32x4_create4(32, 0, 0, 0),
		I32x4_create4(32, 0, 0, 0),
		I32x4_create4(64, 0, 0, 0),
	};

	for (U32 i = 0; i < sizeof(clmulA) / sizeof(clmulA[0]); ++i)
		Test_assert(test, "I32x4_clmul64", I32x4_eq4(I32x4_clmul64(clmulA[i], clmulB[i], clmulImm[i]), clmulExpect[i]));
}
