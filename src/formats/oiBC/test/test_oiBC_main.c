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

OXC3_TEST_MAIN(formats_oiBC) {
	Test t = (Test) { 0 };
	Test_chimera(&t);
	return Test_end(&t);
}
