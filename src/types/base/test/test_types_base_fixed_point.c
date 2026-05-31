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

#include "test_types_base_shared.h"
#include "types/base/fixed_point.h"

void Test_fixedPoint(Test *test) {

	Test_setModule(test, "FP37f4 (fixed point)");

	{
		const FP37f4 a = FP37f4_fromDouble(123.25);            //123.25
		const FP37f4 b = FP37f4_fromDouble(0.75);            //0.75

		Test_assert(test, "FP37f4_fromDouble", NULL, a == (FP37f4)(0b0100 | (123 << 4)));
		Test_assert(test, "FP37f4_toDouble", NULL, FP37f4_toDouble(a) == 123.25);

		const FP37f4 sum = FP37f4_add(a, b);                //124.0
		Test_assert(test, "FP37f4_add", NULL, FP37f4_toDouble(sum) == 124);

		const FP37f4 diff = FP37f4_sub(a, b);                //122.5
		Test_assert(test, "FP37f4_sub", NULL, FP37f4_toDouble(diff) == 122.5);
	}

	Test_setModule(test, "FP46f6 (fixed point)");

	{
		const FP46f6 a = FP46f6_fromDouble(1000000.0625);    //1e6 + 1/16
		const FP46f6 b = FP46f6_fromDouble(0.9375);            //15/16

		Test_assert(test, "FP46f6_fromDouble", NULL, a == (FP46f6)(0b000100 | (1000000 << 6)));
		Test_assert(test, "FP46f6_toDouble", NULL, FP46f6_toDouble(a) == 1000000.0625);

		const FP46f6 sum = FP46f6_add(a, b);                //1000001.0
		Test_assert(test, "FP46f6_add", NULL, FP46f6_toDouble(sum) == 1000001.0);

		const FP46f6 diff = FP46f6_sub(a, b);                //999999.125
		Test_assert(test, "FP46f6_sub", NULL, FP46f6_toDouble(diff) == 999999.125);
	}
}
