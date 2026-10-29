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

//formats/oiBC/test/test_oiBC_main.c

#include "types/test/test.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/math/vec4f.h"
#include "types/math/type_cast.h"
#include "formats/oiBC/chimera.h"

void Test_chimera(Test *t) {

	Test_setModule(t, "Chimera");

	Chimera chimera = (Chimera) {
		.v4f = { F32x4_create4(1, 2, 3, 4) },
		.f   = { 2, 1, 3, 4, 1 }
	};

	chimera.v4f[4] = F32x4_create4(1, 2, 3, 4);

	//FidiA per-register ops (0x00 - 0x1F)

	Test_setModule(t, "Chimera/FidiA");

	for (U8 i = 0; i < 4; ++i) {

		//add

		F32 expected = chimera.f[4] + chimera.f[i];
		Chimera_stepFidiA(&chimera, EFidiA_add(i));
		Test_assert(t, "add", chimera.f[4] == expected);

		//sub

		expected = chimera.f[4] - chimera.f[i];
		Chimera_stepFidiA(&chimera, EFidiA_sub(i));
		Test_assert(t, "sub", chimera.f[4] == expected);

		//mul

		expected = chimera.f[4] * chimera.f[i];
		Chimera_stepFidiA(&chimera, EFidiA_mul(i));
		Test_assert(t, "mul", chimera.f[4] == expected);

		//swap

		F32 expectedSwap = chimera.f[i];
		F32 old          = chimera.f[4];
		Chimera_stepFidiA(&chimera, EFidiA_swap(i));
		Test_assert(t, "swap f[4]", chimera.f[4] == expectedSwap);
		Test_assert(t, "swap f[i]", chimera.f[i] == old);

		//cmp

		ECompareResult expectedCmp = chimera.f[4] < chimera.f[i] ? ECompareResult_Lt : (
			chimera.f[4] > chimera.f[i] ? ECompareResult_Gt : ECompareResult_Eq
		);

		Chimera_stepFidiA(&chimera, EFidiA_cmp(i));
		Test_assert(t, "cmp", expectedCmp == Chimera_getLastCompare(&chimera));

		//load

		Chimera_stepFidiA(&chimera, EFidiA_load(i));
		Test_assert(t, "load", chimera.f[i] == chimera.f[4]);
	}

	//FidiA scalar ops

	{
		F32 expected = 0;

		//max

		expected = F32_max(chimera.f[4], chimera.f[0]);
		Chimera_stepFidiA(&chimera, EFidiA_max);
		Test_assert(t, "max", chimera.f[4] == expected);

		//div

		expected = chimera.f[4] / chimera.f[0];
		Chimera_stepFidiA(&chimera, EFidiA_div);
		Test_assert(t, "div", chimera.f[4] == expected);

		//mod

		expected = F32_mod(chimera.f[4], chimera.f[0]);
		Chimera_stepFidiA(&chimera, EFidiA_mod);
		Test_assert(t, "mod", chimera.f[4] == expected);

		//min

		expected = F32_min(chimera.f[4], chimera.f[0]);
		Chimera_stepFidiA(&chimera, EFidiA_min);
		Test_assert(t, "min", chimera.f[4] == expected);

		//isfinite

		ECompareResult expectedCmp = F32_isValid(chimera.f[4]) ? ECompareResult_Gt : ECompareResult_Eq;
		Chimera_stepFidiA(&chimera, EFidiA_isfinite);
		Test_assert(t, "isfinite", expectedCmp == Chimera_getLastCompare(&chimera));

		//isnan

		expectedCmp = F32_isNaN(chimera.f[4]) ? ECompareResult_Gt : ECompareResult_Eq;
		Chimera_stepFidiA(&chimera, EFidiA_isnan);
		Test_assert(t, "isnan", expectedCmp == Chimera_getLastCompare(&chimera));

		//any

		expectedCmp = F32x4_any(chimera.vf[4]) ? ECompareResult_Gt : ECompareResult_Eq;
		Chimera_stepFidiA(&chimera, EFidiA_anyFv);
		Test_assert(t, "any", expectedCmp == Chimera_getLastCompare(&chimera));

		//all

		expectedCmp = F32x4_all(chimera.vf[4]) ? ECompareResult_Gt : ECompareResult_Eq;
		Chimera_stepFidiA(&chimera, EFidiA_allFv);
		Test_assert(t, "all", expectedCmp == Chimera_getLastCompare(&chimera));
	}
}

//A compare with a NaN is unordered, never equal, and compares of equal infinities are equal

void Test_chimeraCompare(Test *t) {

	Test_setModule(t, "Chimera/Compare");

	Test_assert(
		t, "compare register encodings",
		ECompareResult_Lt == 0 && ECompareResult_Eq == 1 && ECompareResult_Gt == 2 && ECompareResult_Unordered == 3
	);

	const F32 nan = F32_fromU32Bits(0x7FC00000);
	const F32 inf = F32_fromU32Bits(0x7F800000);

	Chimera chimera = (Chimera) { 0 };

	typedef struct CmpCase {
		const C8 *name;
		F32 a, b;
		ECompareResult expected;
	} CmpCase;

	const CmpCase cases[] = {
		{ "1 < 2", 1, 2, ECompareResult_Lt },
		{ "2 > 1", 2, 1, ECompareResult_Gt },
		{ "2 == 2", 2, 2, ECompareResult_Eq },
		{ "-0 == 0", -0.0f, 0.0f, ECompareResult_Eq },
		{ "nan vs 1", nan, 1, ECompareResult_Unordered },
		{ "1 vs nan", 1, nan, ECompareResult_Unordered },
		{ "nan vs nan", nan, nan, ECompareResult_Unordered },
		{ "nan vs inf", nan, inf, ECompareResult_Unordered },
		{ "inf == inf", inf, inf, ECompareResult_Eq },
		{ "-inf == -inf", -inf, -inf, ECompareResult_Eq },
		{ "inf > -inf", inf, -inf, ECompareResult_Gt },
		{ "-inf < inf", -inf, inf, ECompareResult_Lt },
		{ "max > -max", 3e38f, -3e38f, ECompareResult_Gt },
	};

	for (U64 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {

		chimera.f[4] = cases[i].a;
		chimera.f[1] = cases[i].b;
		chimera.effects = ~(U64) 3;

		Chimera_stepFidiA(&chimera, EFidiA_cmp(1));

		Test_assert(t, cases[i].name, Chimera_getLastCompare(&chimera) == cases[i].expected);
		Test_assert(t, "other effects bits kept", (chimera.effects >> 2) == (~(U64) 0 >> 2));
	}

	//The sign comparison also returns Unordered for NaN

	Chimera_cmp(&chimera, nan);
	Test_assert(t, "cmp nan", Chimera_getLastCompare(&chimera) == ECompareResult_Unordered);
	Chimera_cmp(&chimera, -1);
	Test_assert(t, "cmp -1", Chimera_getLastCompare(&chimera) == ECompareResult_Lt);
	Chimera_cmp(&chimera, nan);
	Chimera_cmp(&chimera, 0);
	Test_assert(t, "cmp 0 replaces unordered", Chimera_getLastCompare(&chimera) == ECompareResult_Eq);
	Chimera_cmp(&chimera, 3);
	Test_assert(t, "cmp 3", Chimera_getLastCompare(&chimera) == ECompareResult_Gt);

	//Boolean operations replace the comparison with Eq for false or Gt for true

	chimera.f[4] = nan;
	Chimera_stepFidiA(&chimera, EFidiA_isnan);
	Test_assert(t, "isnan(nan) is true", Chimera_getLastCompare(&chimera) == ECompareResult_Gt);
	Chimera_stepFidiA(&chimera, EFidiA_isfinite);
	Test_assert(t, "isfinite(nan) is false", Chimera_getLastCompare(&chimera) == ECompareResult_Eq);
}

OXC3_TEST_MAIN(formats_oiBC) {
	Test t = (Test) { 0 };
	Test_chimera(&t);
	Test_chimeraCompare(&t);
	return Test_end(&t);
}
