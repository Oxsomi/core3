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

//types/base/test/test_types_base_mathf.c

#include "test_types_base_shared.h"
#include "types/base/mathf.h"

void Test_mathf(Test *test) {

	Test_setModule(test, "Mathf");

	static const F32 eps = 1e-7f;

	//Constants
	
	Test_assert(test, "F32_PI", F32_approxEq(F32_PI, 3.14159265f, eps));
	Test_assert(test, "F32_E", F32_approxEq(F32_E, 2.71828182f, eps));
	Test_assert(test, "F32_DEG_TO_RAD * 180", F32_approxEq(F32_DEG_TO_RAD * 180.0f, F32_PI, eps));
	Test_assert(test, "F32_RAD_TO_DEG * PI", F32_approxEq(F32_RAD_TO_DEG * F32_PI, 180.0f, eps));

	//Abs

	Test_assert(test, "F32_abs", F32_approxEq(F32_abs(0), 0, eps));
	Test_assert(test, "F32_abs", F32_approxEq(F32_abs(3.5f), 3.5f, eps));
	Test_assert(test, "F32_abs", F32_approxEq(F32_abs(-3.5f), 3.5f, eps));

	//Saturate

	Test_assert(test, "F32_saturate", F32_approxEq(F32_saturate(0.5f), 0.5f, eps));
	Test_assert(test, "F32_saturate", F32_approxEq(F32_saturate(-1), 0, eps));
	Test_assert(test, "F32_saturate", F32_approxEq(F32_saturate(2), 1, eps));

	//Lerp
	
	Test_assert(test, "F32_lerp", F32_approxEq(F32_lerp(0, 10, 0), 0, eps));
	Test_assert(test, "F32_lerp", F32_approxEq(F32_lerp(0, 10, 1), 10, eps));
	Test_assert(test, "F32_lerp", F32_approxEq(F32_lerp(0, 10, 0.5f), 5, eps));
	Test_assert(test, "F32_lerp", F32_approxEq(F32_lerp(-4, 4, 0.5f), 0, eps));

	//Sign, signInc
	
	Test_assert(test, "F32_sign", F32_approxEq(F32_sign(-5), -1, eps));
	Test_assert(test, "F32_sign", F32_approxEq(F32_sign( 0),  0, eps));
	Test_assert(test, "F32_sign", F32_approxEq(F32_sign( 5),  1, eps));

	Test_assert(test, "F32_signInc", F32_approxEq(F32_signInc(-5), -1, eps));
	Test_assert(test, "F32_signInc", F32_approxEq(F32_signInc( 0),  1, eps));
	Test_assert(test, "F32_signInc", F32_approxEq(F32_signInc( 5),  1, eps));

	//Sqrt
	
	Test_assert(test, "F32_sqrt", F32_approxEq(F32_sqrt(0), 0, eps));
	Test_assert(test, "F32_sqrt", F32_approxEq(F32_sqrt(1), 1, eps));
	Test_assert(test, "F32_sqrt", F32_approxEq(F32_sqrt(4), 2, eps));
	Test_assert(test, "F32_sqrt", F32_approxEq(F32_sqrt(9), 3, eps));
	Test_assert(test, "F32_sqrt", F32_approxEq(F32_sqrt(2), 1.41421356f, eps));

	//Pow, exp

	Test_assert(test, "F32_pow", F32_approxEq(F32_pow(2, 10), 1024, eps));
	Test_assert(test, "F32_exp2", F32_approxEq(F32_exp2(8), 256, eps));
	Test_assert(test, "F32_exp10", F32_approxEq(F32_exp10(3), 1000, eps));
	Test_assert(test, "F32_expe", F32_approxEq(F32_expe(1), F32_E, eps));
	Test_assert(test, "F32_expe", F32_approxEq(F32_expe(0), 1, eps));

	//Log

	Test_assert(test, "F32_log10", F32_approxEq(F32_log10(1), 0, eps));
	Test_assert(test, "F32_log10", F32_approxEq(F32_log10(10), 1, eps));
	Test_assert(test, "F32_log10", F32_approxEq(F32_log10(100), 2, eps));
	Test_assert(test, "F32_log2", F32_approxEq(F32_log2(1), 0, eps));
	Test_assert(test, "F32_log2", F32_approxEq(F32_log2(2), 1, eps));
	Test_assert(test, "F32_log2", F32_approxEq(F32_log2(8), 3, eps));
	Test_assert(test, "F32_loge", F32_approxEq(F32_loge(F32_E), 1, eps));
	Test_assert(test, "F32_loge", F32_approxEq(F32_loge(1), 0, eps));

	//Trig: (a)sin/cas/tan/atan2

	Test_assert(test, "F32_sin", F32_approxEq(F32_sin(0), 0, eps));
	Test_assert(test, "F32_sin", F32_approxEq(F32_sin(F32_PI / 2), 1, eps));
	Test_assert(test, "F32_sin", F32_approxEq(F32_sin(F32_PI), 0, eps));
	Test_assert(test, "F32_cos", F32_approxEq(F32_cos(0), 1, eps));
	Test_assert(test, "F32_cos", F32_approxEq(F32_cos(F32_PI / 2), 0, eps));
	Test_assert(test, "F32_cos", F32_approxEq(F32_cos(F32_PI), -1, eps));
	Test_assert(test, "F32_tan", F32_approxEq(F32_tan(0), 0, eps));
	Test_assert(test, "F32_tan", F32_approxEq(F32_tan(F32_PI / 4), 1, eps));
	Test_assert(test, "F32_asin", F32_approxEq(F32_asin(0), 0, eps));
	Test_assert(test, "F32_asin", F32_approxEq(F32_asin(1), F32_PI / 2, eps));
	Test_assert(test, "F32_acos", F32_approxEq(F32_acos(1), 0, eps));
	Test_assert(test, "F32_acos", F32_approxEq(F32_acos(0), F32_PI / 2, eps));
	Test_assert(test, "F32_atan", F32_approxEq(F32_atan(0), 0, eps));
	Test_assert(test, "F32_atan", F32_approxEq(F32_atan(1), F32_PI / 4, eps));
	Test_assert(test, "F32_atan2", F32_approxEq(F32_atan2(1, 1), F32_PI / 4, eps));
	Test_assert(test, "F32_atan2", F32_approxEq(F32_atan2(0, -1), F32_PI, eps));

	//Round, ceil, floor, fract

	Test_assert(test, "F32_ceil", F32_approxEq(F32_ceil(1.1f), 2, eps));
	Test_assert(test, "F32_ceil", F32_approxEq(F32_ceil(-1.1f), -1, eps));
	Test_assert(test, "F32_floor", F32_approxEq(F32_floor(1.9f), 1, eps));
	Test_assert(test, "F32_floor", F32_approxEq(F32_floor(-1.1f), -2, eps));
	Test_assert(test, "F32_fract", F32_approxEq(F32_fract(1.75f), 0.75f, eps));
	Test_assert(test, "F32_fract", F32_approxEq(F32_fract(-1.75f), 0.25f, eps));

	Test_assert(test, "F32_round", F32_approxEq(F32_round(0.5f), 0, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(1.5f), 2, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(2.5f), 2, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(3.5f), 4, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(4.5f), 4, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(5.5f), 6, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(6.5f), 6, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(7.5f), 8, eps));

	Test_assert(test, "F32_round", F32_approxEq(F32_round(-0.5f), 0, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-1.5f), -2, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-2.5f), -2, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-3.5f), -4, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-4.5f), -4, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-5.5f), -6, eps));

	Test_assert(test, "F32_round", F32_approxEq(F32_round(1.4f), 1, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(1.6f), 2, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-1.4f), -1, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-1.6f), -2, eps));

	Test_assert(test, "F32_round", F32_approxEq(F32_round(2), 2, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(-2), -2, eps));
	Test_assert(test, "F32_round", F32_approxEq(F32_round(0), 0, eps));

	//Mod

	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(5, 3), 2, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(7, 2), 1, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(1, 1), 0, eps));

	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(-1, 3), -1, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(-5, 3), -2, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(-6, 3), 0, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(-0.5f, 1), -0.5f, eps));

	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(5, -3), 2, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(1, -3), 1, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(6, -3), 0, eps));

	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(-5, -3), -2, eps));
	Test_assert(test, "F32_mod", F32_approxEq(F32_mod(-1, -3), -1, eps));

	//isNaN / isInf / isValid

	volatile F32 zero = 0;        //Supress errors

	Test_assert(test, "F32_isNaN", F32_isNaN(0 / zero));
	Test_assert(test, "F32_isNaN", !F32_isNaN(1));
	Test_assert(test, "F32_isInf", F32_isInf(1 / zero));
	Test_assert(test, "F32_isInf", !F32_isInf(1));
	Test_assert(test, "F32_isValid", F32_isValid(1));
	Test_assert(test, "F32_isValid", !F32_isValid(0 / zero));
	Test_assert(test, "F32_isValid", !F32_isValid(1 / zero));
}

void Test_mathd(Test *test) {

	Test_setModule(test, "Mathd");

	static const F64 eps = 1e-10;

	//Constants

	Test_assert(test, "F64_PI", F64_approxEq(F64_PI, 3.14159265358979, eps));
	Test_assert(test, "F64_E", F64_approxEq(F64_E, 2.71828182845904, eps));

	Test_assert(test, "F64_DEG_TO_RAD * 180", F64_approxEq(F64_DEG_TO_RAD * 180.0, F64_PI, eps));
	Test_assert(test, "F64_RAD_TO_DEG * PI", F64_approxEq(F64_RAD_TO_DEG * F64_PI, 180.0, eps));

	//Abs

	Test_assert(test, "F64_abs", F64_approxEq(F64_abs(0), 0, eps));
	Test_assert(test, "F64_abs", F64_approxEq(F64_abs(3.5), 3.5, eps));
	Test_assert(test, "F64_abs", F64_approxEq(F64_abs(-3.5), 3.5, eps));

	//Saturate

	Test_assert(test, "F64_saturate", F64_approxEq(F64_saturate(0.5), 0.5, eps));
	Test_assert(test, "F64_saturate", F64_approxEq(F64_saturate(-1), 0, eps));
	Test_assert(test, "F64_saturate", F64_approxEq(F64_saturate(2), 1, eps));

	//Lerp

	Test_assert(test, "F64_lerp", F64_approxEq(F64_lerp(0, 10, 0), 0, eps));
	Test_assert(test, "F64_lerp", F64_approxEq(F64_lerp(0, 10, 1), 10, eps));
	Test_assert(test, "F64_lerp", F64_approxEq(F64_lerp(0, 10, 0.5), 5, eps));
	Test_assert(test, "F64_lerp", F64_approxEq(F64_lerp(-4, 4, 0.5), 0, eps));

	//Sign, signInc

	Test_assert(test, "F64_sign", F64_approxEq(F64_sign(-5), -1, eps));
	Test_assert(test, "F64_sign", F64_approxEq(F64_sign( 0),  0, eps));
	Test_assert(test, "F64_sign", F64_approxEq(F64_sign( 5),  1, eps));

	Test_assert(test, "F64_signInc", F64_approxEq(F64_signInc(-5), -1, eps));
	Test_assert(test, "F64_signInc", F64_approxEq(F64_signInc( 0),  1, eps));
	Test_assert(test, "F64_signInc", F64_approxEq(F64_signInc( 5),  1, eps));

	//Sqrt

	Test_assert(test, "F64_sqrt", F64_approxEq(F64_sqrt(0), 0, eps));
	Test_assert(test, "F64_sqrt", F64_approxEq(F64_sqrt(1), 1, eps));
	Test_assert(test, "F64_sqrt", F64_approxEq(F64_sqrt(4), 2, eps));
	Test_assert(test, "F64_sqrt", F64_approxEq(F64_sqrt(9), 3, eps));
	Test_assert(test, "F64_sqrt", F64_approxEq(F64_sqrt(2), 1.41421356237310, eps));

	//Pow, exp

	Test_assert(test, "F64_pow", F64_approxEq(F64_pow(2, 10), 1024, eps));
	Test_assert(test, "F64_exp2", F64_approxEq(F64_exp2(8), 256, eps));
	Test_assert(test, "F64_exp10", F64_approxEq(F64_exp10(3), 1000, eps));
	Test_assert(test, "F64_expe", F64_approxEq(F64_expe(1), F64_E, eps));
	Test_assert(test, "F64_expe", F64_approxEq(F64_expe(0), 1, eps));

	//Log

	Test_assert(test, "F64_log10", F64_approxEq(F64_log10(1), 0, eps));
	Test_assert(test, "F64_log10", F64_approxEq(F64_log10(10), 1, eps));
	Test_assert(test, "F64_log10", F64_approxEq(F64_log10(100), 2, eps));
	Test_assert(test, "F64_log2", F64_approxEq(F64_log2(1), 0, eps));
	Test_assert(test, "F64_log2", F64_approxEq(F64_log2(2), 1, eps));
	Test_assert(test, "F64_log2", F64_approxEq(F64_log2(8), 3, eps));
	Test_assert(test, "F64_loge", F64_approxEq(F64_loge(F64_E), 1, eps));
	Test_assert(test, "F64_loge", F64_approxEq(F64_loge(1), 0, eps));

	//Trig: (a)sin/cos/tan/atan2

	Test_assert(test, "F64_sin", F64_approxEq(F64_sin(0), 0, eps));
	Test_assert(test, "F64_sin", F64_approxEq(F64_sin(F64_PI / 2), 1, eps));
	Test_assert(test, "F64_sin", F64_approxEq(F64_sin(F64_PI), 0, eps));
	Test_assert(test, "F64_cos", F64_approxEq(F64_cos(0), 1, eps));
	Test_assert(test, "F64_cos", F64_approxEq(F64_cos(F64_PI / 2), 0, eps));
	Test_assert(test, "F64_cos", F64_approxEq(F64_cos(F64_PI), -1, eps));
	Test_assert(test, "F64_tan", F64_approxEq(F64_tan(0), 0, eps));
	Test_assert(test, "F64_tan", F64_approxEq(F64_tan(F64_PI / 4), 1, eps));
	Test_assert(test, "F64_asin", F64_approxEq(F64_asin(0), 0, eps));
	Test_assert(test, "F64_asin", F64_approxEq(F64_asin(1), F64_PI / 2, eps));
	Test_assert(test, "F64_acos", F64_approxEq(F64_acos(1), 0, eps));
	Test_assert(test, "F64_acos", F64_approxEq(F64_acos(0), F64_PI / 2, eps));
	Test_assert(test, "F64_atan", F64_approxEq(F64_atan(0), 0, eps));
	Test_assert(test, "F64_atan", F64_approxEq(F64_atan(1), F64_PI / 4, eps));
	Test_assert(test, "F64_atan2", F64_approxEq(F64_atan2(1, 1), F64_PI / 4, eps));
	Test_assert(test, "F64_atan2", F64_approxEq(F64_atan2(0, -1), F64_PI, eps));

	//Round, ceil, floor, fract

	Test_assert(test, "F64_ceil", F64_approxEq(F64_ceil(1.1), 2, eps));
	Test_assert(test, "F64_ceil", F64_approxEq(F64_ceil(-1.1), -1, eps));
	Test_assert(test, "F64_floor", F64_approxEq(F64_floor(1.9), 1, eps));
	Test_assert(test, "F64_floor", F64_approxEq(F64_floor(-1.1), -2, eps));
	Test_assert(test, "F64_fract", F64_approxEq(F64_fract(1.75), 0.75, eps));
	Test_assert(test, "F64_fract", F64_approxEq(F64_fract(-1.75), 0.25, eps));

	Test_assert(test, "F64_round", F64_approxEq(F64_round(0.5), 0, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(1.5), 2, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(2.5), 2, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(3.5), 4, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(4.5), 4, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(5.5), 6, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(6.5), 6, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(7.5), 8, eps));

	Test_assert(test, "F64_round", F64_approxEq(F64_round(-0.5), 0, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-1.5), -2, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-2.5), -2, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-3.5), -4, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-4.5), -4, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-5.5), -6, eps));

	Test_assert(test, "F64_round", F64_approxEq(F64_round(1.4), 1, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(1.6), 2, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-1.4), -1, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-1.6), -2, eps));

	Test_assert(test, "F64_round", F64_approxEq(F64_round(2), 2, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(-2), -2, eps));
	Test_assert(test, "F64_round", F64_approxEq(F64_round(0), 0, eps));

	//Mod

	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(5, 3), 2, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(7, 2), 1, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(1, 1), 0, eps));

	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(-1, 3), -1, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(-5, 3), -2, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(-6, 3), 0, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(-0.5, 1), -0.5, eps));

	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(5, -3), 2, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(1, -3), 1, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(6, -3), 0, eps));

	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(-5, -3), -2, eps));
	Test_assert(test, "F64_mod", F64_approxEq(F64_mod(-1, -3), -1, eps));

	//isNaN / isInf / isValid

	volatile F64 zero = 0;

	Test_assert(test, "F64_isNaN", F64_isNaN(0 / zero));
	Test_assert(test, "F64_isNaN", !F64_isNaN(1));
	Test_assert(test, "F64_isInf", F64_isInf(1 / zero));
	Test_assert(test, "F64_isInf", !F64_isInf(1));
	Test_assert(test, "F64_isValid", F64_isValid(1));
	Test_assert(test, "F64_isValid", !F64_isValid(0 / zero));
	Test_assert(test, "F64_isValid", !F64_isValid(1 / zero));
}
