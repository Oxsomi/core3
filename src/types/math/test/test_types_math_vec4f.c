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

//types/math/test/test_types_math_vec4f.c

#include "test_types_math_shared.h"
#include "types/math/vec4f_swizzle.h"
#include "types/math/u128.h"

void Test_vec4f(Test *test) {

	Test_setModule(test, "F32x4");

	//Create, accessors, swizzle

	F32x4 v4 = F32x4_create4(1, 2, 3, 4);
	Test_assert(test, "F32x4 x/y/z/w", F32x4_x(v4) == 1 && F32x4_y(v4) == 2 && F32x4_z(v4) == 3 && F32x4_w(v4) == 4);

	F32x4 v1 = F32x4_create1(5);
	Test_assert(test, "F32x4_create1", F32x4_x(v1) == 5 && F32x4_y(v1) == 0 && F32x4_z(v1) == 0 && F32x4_w(v1) == 0);

	F32x4_setZRef(&v4, 9);
	F32x4_setWRef(&v4, 10);
	Test_assert(test, "F32x4_set/get", F32x4_get(v4, 2) == 9 && F32x4_get(v4, 3) == 10);

	F32x4 xxxx = F32x4_xxxx(v4);
	Test_assert(test, "F32x4_xxxx",    F32x4_x(xxxx) == 1 && F32x4_y(xxxx) == 1 && F32x4_z(xxxx) == 1 && F32x4_w(xxxx) == 1);

	F32x4 wzyx = F32x4_wzyx(v4);
	Test_assert(test, "F32x4_wzyx",    F32x4_x(wzyx) == 10 && F32x4_y(wzyx) == 9 && F32x4_z(wzyx) == 2 && F32x4_w(wzyx) == 1);

	//Comparisons

	F32x4 a = F32x4_create4(1, 2, 3, 4);
	F32x4 b = F32x4_create4(1, 3, 2, 5);
	Test_assert(test, "F32x4_eq",         F32x4_eqExact4(a, F32x4_create4(1, 2, 3, 4)));
	Test_assert(test, "F32x4_neq",        F32x4_neqExact4(a, b));
	Test_assert(test, "F32x4_leq",        F32x4_eqExact4(F32x4_leq(a, b), F32x4_create4(1, 1, 0, 1)));
	Test_assert(test, "F32x4_gt",         F32x4_eqExact4(F32x4_gt(b, a),  F32x4_create4(0, 1, 0, 1)));
	Test_assert(test, "F32x4_lt",         F32x4_eqExact4(F32x4_lt(a, b),  F32x4_create4(0, 1, 0, 1)));
	Test_assert(test, "F32x4_geq",        F32x4_eqExact4(F32x4_geq(a, b), F32x4_create4(1, 0, 1, 0)));

	//Arithmetic

	a = F32x4_create4(1, 2, 3, 4);
	b = F32x4_create4(5, 6, 7, 8);
	Test_assert(test, "F32x4_add",        !F32x4_neqExact4(F32x4_add(a, b), F32x4_create4(6, 8, 10, 12)));
	Test_assert(test, "F32x4_sub",        !F32x4_neqExact4(F32x4_sub(b, a), F32x4_create4(4, 4, 4, 4)));
	Test_assert(test, "F32x4_mul",        !F32x4_neqExact4(F32x4_mul(a, b), F32x4_create4(5, 12, 21, 32)));
	Test_assert(test, "F32x4_div",        !F32x4_neqExact4(F32x4_div(b, a), F32x4_create4(5, 3, 7.0f / 3.0f, 2)));

	F32x4 mod = F32x4_mod(F32x4_create4(5.5f, -2.25f, 1, 2), F32x4_xxxx4(2));
	Test_assert(test, "F32x4_mod",        !F32x4_neqExact4(mod, F32x4_create4(1.5f, 1.75f, 1, 0)));

	//Length, normalize, dot, reduce, sqrt, lerp

	v4 = F32x4_create4(2, 3, 6, 1);    //sqLen4=50, sqLen3=49, sqLen2=13
	Test_assert(test, "F32x4_sqLen4",     F32_abs(F32x4_sqLen4(v4) - 50) <= 1e-6f);
	Test_assert(test, "F32x4_sqLen3",     F32_abs(F32x4_sqLen3(v4) - 49) <= 1e-6f);
	Test_assert(test, "F32x4_sqLen2",     F32_abs(F32x4_sqLen2(v4) - 13) <= 1e-6f);
	Test_assert(test, "F32x4_normalize4", F32_abs(F32x4_len4(F32x4_normalize4(v4)) - 1) <= 3e-3f);
	Test_assert(test, "F32x4_normalize3", F32_abs(F32x4_len3(F32x4_normalize3(v4)) - 1) <= 3e-3f);
	Test_assert(test, "F32x4_normalize2", F32_abs(F32x4_len2(F32x4_normalize2(v4)) - 1) <= 3e-3f);
	Test_assert(test, "F32x4_dot4",       F32_abs(F32x4_dot4(v4, v4) - 50) <= 1e-6f);
	Test_assert(test, "F32x4_dot3",       F32_abs(F32x4_dot3(v4, v4) - 49) <= 1e-6f);
	Test_assert(test, "F32x4_dot2",       F32_abs(F32x4_dot2(v4, v4) - 13) <= 1e-6f);
	Test_assert(test, "F32x4_satDot4",    F32x4_satDot4(F32x4_create4(0, 1, 0, 0), F32x4_create4(0, -1, 0, 0)) >= 0);
	Test_assert(test, "F32x4_satDot3",    F32x4_satDot3(F32x4_create3(0, 1, 0),    F32x4_create3(0, -1, 0))    >= 0);
	Test_assert(test, "F32x4_satDot2",    F32x4_satDot2(F32x4_create2(0, 1),       F32x4_create2(0, -1))       >= 0);
	Test_assert(test, "F32x4_trunc3",     !F32x4_neqExact4(F32x4_trunc3(v4), F32x4_create3(2, 3, 6)));
	Test_assert(test, "F32x4_trunc2",     !F32x4_neqExact4(F32x4_trunc2(v4), F32x4_create2(2, 3)));
	Test_assert(test, "F32x4_reduce",     F32_abs(F32x4_reduce(v4) - 12) <= 1e-6f);

	F32x4 c = F32x4_create4(6, 5, 4, 3);
	Test_assert(test, "F32x4_sqrt",       !F32x4_neqExact4(F32x4_sqrt(F32x4_create4(36, 25, 16, 9)),  c));

	c = F32x4_create4(1 / 6.f, 0.2f, 0.25f, 1 / 3.f);
	Test_assert(test, "F32x4_rsqrt",      !F32x4_neqApproxAdv4(F32x4_rsqrt(F32x4_create4(36, 25, 16, 9)), c, 1e-3f, 1e-3f));

	c = F32x4_create4(6, 6.5f, 8, 5.5f);
	Test_assert(test, "F32x4_lerp",       !F32x4_neqExact4(F32x4_lerp(v4, F32x4_xxxx4(10), 0.5f), c));

	//min/max/clamp/saturate/sign/abs

	a = F32x4_create4(-2, 0.5f, 2, 10);
	Test_assert(test, "F32x4_min",        !F32x4_neqExact4(F32x4_min(a, F32x4_zero()),  F32x4_create4(-2, 0, 0, 0)));
	Test_assert(test, "F32x4_max",        !F32x4_neqExact4(F32x4_max(a, F32x4_one()),   F32x4_create4(1, 1, 2, 10)));
	Test_assert(test, "F32x4_saturate",   !F32x4_neqExact4(F32x4_saturate(a),            F32x4_create4(0, 0.5f, 1, 1)));
	Test_assert(test, "F32x4_sign",       !F32x4_neqExact4(F32x4_sign(F32x4_create4(-3, 4, 0, 1)), F32x4_create4(-1, 1, 1, 1)));
	Test_assert(test, "F32x4_abs",        !F32x4_neqExact4(F32x4_abs(F32x4_create4(-3, 4, -1, 0)), F32x4_create4(3, 4, 1, 0)));

	c = F32x4_clamp(F32x4_create4(-1, 5, 1, 2), F32x4_zero(), F32x4_two());
	Test_assert(test, "F32x4_clamp",      !F32x4_neqExact4(c, F32x4_create4(0, 2, 1, 2)));

	//fract/floor/ceil/round

	v4 = F32x4_create4(1.25f, -1.75f, 2.5f, -2.5f);
	Test_assert(test, "F32x4_fract",      !F32x4_neqExact4(F32x4_fract(v4), F32x4_create4(0.25f, 0.25f, 0.5f, 0.5f)));
	Test_assert(test, "F32x4_floor",      !F32x4_neqExact4(F32x4_floor(v4), F32x4_create4(1, -2, 2, -3)));
	Test_assert(test, "F32x4_ceil",       !F32x4_neqExact4(F32x4_ceil(v4),  F32x4_create4(2, -1, 3, -2)));
	Test_assert(test, "F32x4_round",      !F32x4_neqExact4(F32x4_round(v4), F32x4_create4(1, -2, 2, -2)));

	//exp/log

	v4 = F32x4_create4(1, 2, 3, 4);
	Test_assert(test, "F32x4_exp2",  !F32x4_neqExact4(F32x4_exp2(v4),                         F32x4_create4(2, 4, 8, 16)));
	Test_assert(test, "F32x4_log2",  !F32x4_neqExact4(F32x4_log2(F32x4_create4(2, 4, 8, 16)), v4));
	Test_assert(test, "F32x4_exp",   !F32x4_neqExact4(F32x4_exp(F32x4_zero()),                F32x4_one()));
	Test_assert(test, "F32x4_loge",  !F32x4_neqExact4(F32x4_loge(F32x4_one()),                F32x4_zero()));
	c = F32x4_create4(10, 100, 1000, 10000);
	Test_assert(test, "F32x4_exp10", !F32x4_neqApprox4(F32x4_exp10(v4),                       c));
	Test_assert(test, "F32x4_log10", !F32x4_neqApprox4(F32x4_log10(F32x4_create4(10, 100, 1000, 10000)), v4));

	//cross3, reflect2/3

	a = F32x4_create3(1, 2, 3);
	b = F32x4_create3(4, 5, 6);
	Test_assert(test, "F32x4_cross3",   !F32x4_neqExact4(F32x4_cross3(a, b), F32x4_create3(-3, 6, -3)));

	F32x4 inc = F32x4_create3(1, -2, 0);
	F32x4 nrm = F32x4_create3(0,  1, 0);
	Test_assert(test, "F32x4_reflect2", !F32x4_neqApprox4(F32x4_reflect2(inc, nrm), F32x4_create3(1, 2, 0)));
	Test_assert(test, "F32x4_reflect3", !F32x4_neqApprox4(F32x4_reflect3(inc, nrm), F32x4_create3(1, 2, 0)));

	//rgb8 pack/unpack and srgb8 pack/unpack

	const F32x4 valuesRGBA[] = {
		F32x4_create4(0, 0, 0, 0), F32x4_create4(0, 0, 1, 0), F32x4_create4(0, 1, 0, 0),
		F32x4_create4(0, 1, 1, 0), F32x4_create4(1, 0, 0, 0), F32x4_create4(1, 0, 1, 0),
		F32x4_create4(1, 1, 0, 0), F32x4_create4(1, 1, 1, 0),
		F32x4_trunc3(F32x4_xxxx4(0.25f)),
		F32x4_trunc3(F32x4_xxxx4(0.5f)),
		F32x4_trunc3(F32x4_xxxx4(0.75f))
	};

	static const U32 valuesRGBA8[] = {
		0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF,
		0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
		0x003F3F3F, 0x007F7F7F, 0x00BFBFBF
	};

	static const U32 valuesSRGBA8[] = {
		0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF,
		0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
		0x00888888, 0x00BBBBBB, 0x00E0E0E0
	};

	for (U64 i = 0; i < sizeof(valuesRGBA) / sizeof(valuesRGBA[0]); ++i) {
		Test_assert(test, "F32x4_rgb8Pack",    F32x4_rgb8Pack(valuesRGBA[i]) == valuesRGBA8[i]);

		c = F32x4_rgb8Unpack(valuesRGBA8[i]);
		Test_assert(test, "F32x4_rgb8Unpack",  !F32x4_neqApproxAdv4(c, valuesRGBA[i], 1e-2f, 1e-2f));

		Test_assert(test, "F32x4_srgb8Pack",   F32x4_srgb8Pack(valuesRGBA[i]) == valuesSRGBA8[i]);

		c = F32x4_srgb8Unpack(valuesSRGBA8[i]);
		Test_assert(test, "F32x4_srgb8Unpack", !F32x4_neqApproxAdv4(c, valuesRGBA[i], 1e-2f, 1e-2f));
	}

	//mul3x3 / mul4x4 / mul3x4

	F32x4 v3 = F32x4_create3(1, 2, 3);
	F32x4 mat3[3] = { F32x4_create3(1, 4, 7), F32x4_create3(2, 5, 8), F32x4_create3(3, 6, 9) };
	Test_assert(test, "F32x4_mul3x3", !F32x4_neqExact4(F32x4_mul3x3(v3, mat3), F32x4_create3(14, 32, 50)));

	F32x4 mat4[4] = {
		F32x4_create4(0, 3, 6, 9), F32x4_create4(1, 4, 7, 0),
		F32x4_create4(2, 5, 8, 1), F32x4_create4(3, 6, 9, 2)
	};

	c = F32x4_create4(20, 50, 80, 20);
	Test_assert(test, "F32x4_mul4x4", !F32x4_neqExact4(F32x4_mul4x4(F32x4_create4(1, 2, 3, 4), mat4), c));

	c = F32x4_create3(20, 50, 80);
	Test_assert(test, "F32x4_mul3x4", !F32x4_neqExact4(F32x4_mul3x4(F32x4_create4(1, 2, 3, 4), mat4), c));
}
