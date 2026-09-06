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

//types/math/test/test_types_math_flp_f32.c

#include "test_types_math_shared.h"
#include "types/math/flp.h"
#include "types/math/type_cast.h"

void Test_flpF32(Test *test) {

	Test_setModule(test, "F32_castF64");

	static const U32 expansionTests[] = {
		0x7FFFFFFF,        //NaN
		0x00000000,        //0
		0x7F800000,        //Inf
		0x7F800001,        //Another NaN
		0x00000003,        //DeN
		0x00020000,        //DeN that tests bit comparison in expansion function
		0x00000001,        //Smallest DeN
		0x007FFFFF,        //Biggest DeN
		0x00800000,        //Smallest non DeN
		0x7F7FFFFF,        //Float max
		0x3F800000,        //1
		0x3F000000,        //0.5
		0x40000000,        //2
		0x42F60000,        //123
		0x3F9D70A4,        //1.23
		0x3F7FFFFF,        //Almost 1
		0x00000015         //DeN that was failing in the tests
	};

	for (U64 i = 0; i < sizeof(expansionTests) / sizeof(expansionTests[0]); ++i) {
		for (U64 j = 0; j < 2; ++j) {

			U32 bits = expansionTests[i];
			if (j) bits |= (U32)1 << 31;

			const F32 fi       = F32_fromU32Bits(bits);
			const U64 expected = U64_fromF64Bits((F64)fi);
			const U64 got      = U64_fromF64Bits(F32_castF64(fi));

			Test_assert(test, "F32_castF64", got == expected);
		}
	}
}
