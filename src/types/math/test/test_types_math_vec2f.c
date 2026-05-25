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

//types/math/test/test_types_math_vec2f.c

#include "test_types_math_shared.h"
#include "types/math/vec2f.h"

void Test_vec2f(Test *test) {

	Test_setModule(test, "F32x2");
	
	//Create, accessors, swizzle, set/get

	F32x2 v2 = F32x2_create2(1, 2);
	Test_assert(test, "F32x2_x/y",        F32x2_x(v2) == 1 && F32x2_y(v2) == 2);

	F32x2 v0 = F32x2_create1(1);
	Test_assert(test, "F32x2_create1",    F32x2_x(v0) == 1 && F32x2_y(v0) == 0);

	F32x2_setRefX(&v2, 3);
	F32x2_setRefY(&v2, 4);
	Test_assert(test, "F32x2_setRef/get", F32x2_get(v2, 0) == 3 && F32x2_get(v2, 1) == 4);

	F32x2 xx = F32x2_xx(v2);
	F32x2 yy = F32x2_yy(v2);
	Test_assert(test, "F32x2_xx",         F32x2_x(xx) == 3 && F32x2_y(xx) == 3);
	Test_assert(test, "F32x2_yy",         F32x2_x(yy) == 4 && F32x2_y(yy) == 4);

	//Comparisons

	F32x2 a = F32x2_create2(2, 3);
	F32x2 b = F32x2_create2(2, 4);

	Test_assert(test, "F32x2_eq",         F32x2_all(F32x2_eq(a, F32x2_create2(2, 3))));
	Test_assert(test, "F32x2_neq",        F32x2_any(F32x2_neq(a, b)));
	Test_assert(test, "F32x2_leq",        F32x2_all(F32x2_leq(a, b)));
	Test_assert(test, "F32x2_geq",        F32x2_all(F32x2_geq(b, a)));
	Test_assert(test, "F32x2_lt",         F32x2_eq2(F32x2_lt(a, b), F32x2_create2(0, 1)));
	Test_assert(test, "F32x2_gt",         F32x2_eq2(F32x2_gt(b, a), F32x2_create2(0, 1)));

	//Arithmetic

	a = F32x2_create2(1.5f, -2);
	b = F32x2_create2(2, 4);
	F32x2 c = F32x2_create2(1, 2);

	Test_assert(test, "F32x2_add",        !F32x2_neq2(F32x2_add(a, b), F32x2_create2(3.5f, 2)));
	Test_assert(test, "F32x2_sub",        !F32x2_neq2(F32x2_sub(a, b), F32x2_create2(-0.5f, -6)));
	Test_assert(test, "F32x2_mul",        !F32x2_neq2(F32x2_mul(a, b), F32x2_create2(3, -8)));
	Test_assert(test, "F32x2_div",        !F32x2_neq2(F32x2_div(b, c), F32x2_create2(2, 2)));

	//Length, normalize, dot, reduce

	v2 = F32x2_create2(3, 4);
	Test_assert(test, "F32x2_len",        F32_abs(F32x2_len(v2) - 5) <= 1e-6f);
	Test_assert(test, "F32x2_normalize",  F32_abs(F32x2_len(F32x2_normalize(v2)) - 1) <= 1e-6f);
	Test_assert(test, "F32x2_dot",        F32_abs(F32x2_dot(v2, v2) - 25) <= 1e-6f);
	Test_assert(test, "F32x2_reduce",     F32_abs(F32x2_reduce(v2) - 7) <= 1e-6f);

	//min/max/clamp/saturate/sign/abs

	F32x2 clamped = F32x2_clamp(F32x2_create2(-2, 10), F32x2_negate(F32x2_one), F32x2_one);
	Test_assert(test, "F32x2_clamp",      !F32x2_neq2(clamped,                                  F32x2_create2(-1, 1)));

	a = F32x2_create2(1, 5);
	b = F32x2_create2(3, 4);

	Test_assert(test, "F32x2_min",        !F32x2_neq2(F32x2_min(a, b),                          F32x2_create2(1, 4)));
	Test_assert(test, "F32x2_max",        !F32x2_neq2(F32x2_max(a, b),                          F32x2_create2(3, 5)));
	Test_assert(test, "F32x2_saturate",   !F32x2_neq2(F32x2_saturate(F32x2_create2(2, -1)),     F32x2_create2(1, 0)));
	Test_assert(test, "F32x2_sign",       !F32x2_neq2(F32x2_sign(F32x2_create2(-3, 4)),         F32x2_create2(-1, 1)));
	Test_assert(test, "F32x2_abs",        !F32x2_neq2(F32x2_abs(F32x2_create2(-3, 4)),          F32x2_create2(3, 4)));

	//negate, pow2, reflect

	v2 = F32x2_create2(-3.5f, 4.5f);
	Test_assert(test, "F32x2_negate",     !F32x2_neq2(F32x2_negate(v2),                         F32x2_create2(3.5f, -4.5f)));
	Test_assert(test, "F32x2_pow2",       !F32x2_neq2(F32x2_pow2(F32x2_create2(2, -3)),         F32x2_create2(4, 9)));

	F32x2 refl = F32x2_reflect(F32x2_create2(1, 1), F32x2_create2(0, 1));
	Test_assert(test, "F32x2_reflect",    !F32x2_neq2(refl, F32x2_create2(1, -1)));

	//mul2x2

	const F32x2 mat2x2[2] = { F32x2_create2(1, 2), F32x2_create2(3, 4) };
	Test_assert(test, "F32x2_mul2x2",     !F32x2_neq2(F32x2_mul2x2(F32x2_create2(5, 6), mat2x2), F32x2_create2(23, 34)));
	Test_assert(test, "F32x2_mul2x2",     !F32x2_neq2(F32x2_mul2x2(F32x2_create2(7, 8), mat2x2), F32x2_create2(31, 46)));

	//mul2x3

	F32x2 mat2x3[] = { F32x2_create2(1, 0), F32x2_create2(0, 1), F32x2_create2(5, 7) };
	Test_assert(test, "F32x2_mul2x3",     !F32x2_neq2(F32x2_mul2x3(F32x2_create2(2, 3), mat2x3), F32x2_create2(7, 10)));

	//any/all

	v2 = F32x2_create2(-2, 3);
	Test_assert(test, "F32x2_sign",       !F32x2_neq2(F32x2_sign(v2), F32x2_create2(-1, 1)));
	Test_assert(test, "F32x2_any",        F32x2_any(v2));
	Test_assert(test, "F32x2_all",        F32x2_all(v2));

	//mod/fract/floor/ceil/round

	v2 = F32x2_create2(5.5f, -2.25f);
	Test_assert(test, "F32x2_mod",        !F32x2_neq2(F32x2_mod(v2, F32x2_create2(2, 2)),          F32x2_create2(1.5f, 1.75f)));
	Test_assert(test, "F32x2_fract",      !F32x2_neq2(F32x2_fract(v2),                             F32x2_create2(0.5f, 0.75f)));
	Test_assert(test, "F32x2_floor",      !F32x2_neq2(F32x2_floor(v2),                             F32x2_create2(5, -3)));
	Test_assert(test, "F32x2_ceil",       !F32x2_neq2(F32x2_ceil(v2),                              F32x2_create2(6, -2)));
	Test_assert(test, "F32x2_round",      !F32x2_neq2(F32x2_round(F32x2_create2(2.4f, 2.6f)),      F32x2_create2(2, 3)));

	//exp/log

	v2 = F32x2_create2(1, 2);
	Test_assert(test, "F32x2_exp2",       !F32x2_neq2(F32x2_exp2(v2),                              F32x2_create2(2, 4)));
	Test_assert(test, "F32x2_log2",       !F32x2_neq2(F32x2_log2(F32x2_create2(2, 4)),             v2));
	Test_assert(test, "F32x2_expe",       !F32x2_neq2(F32x2_expe(F32x2_zero),                      F32x2_one));
	Test_assert(test, "F32x2_loge",       !F32x2_neq2(F32x2_loge(F32x2_one),                       F32x2_zero));
	Test_assert(test, "F32x2_exp10",      !F32x2_neq2(F32x2_exp10(F32x2_create2(1, 2)),            F32x2_create2(10, 100)));
	Test_assert(test, "F32x2_log10",      !F32x2_neq2(F32x2_log10(F32x2_create2(10, 100)),         F32x2_create2(1, 2)));
}
