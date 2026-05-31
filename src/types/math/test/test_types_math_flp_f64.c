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

//types/math/test/test_types_math_flp_f64.c

#include "test_types_math_shared.h"
#include "types/math/flp.h"
#include "types/math/type_cast.h"

void Test_flpF64(Test *test) {

	Test_setModule(test, "F64_castF32");

	static const U64 truncTests[] = {
		0x7FFFFFFFFFFFFFFF,        //NaN
		0x0000000000000000,        //0
		0x7FF0000000000000,        //Inf

		0x7FF0000000000001,        //NaN that mostly collapses to 0 mantissa (but top bit 1)
		0x7FF8000000000000,        //NaN with top bit set
		0x7FF1000000000001,        //NaN other
		0x7FF8000000000001,        //NaN with top and bottom bit
		0x7FFC000000000003,        //NaN with two top and bottom bits

		//Normal numbers

		0x3FF0000000000000,        //1
		0x3FE0000000000000,        //0.5
		0x4000000000000000,        //2
		0x405EC00000000000,        //123
		0x3FF3AE147AE147AE,        //1.23
		0x3FEFFFFFFFFFFFFF,        //Almost 1 (rounds to 1)

		0x7FEFFFFFFFFFFFFF,        //Double max

		//Float test numbers

		0x36E4FFFFFFFFEF66,        //float 0x00000015
		0x47EFFFFFE00030B7,        //Float min
		0x36B7FFFFFFFFE40F,        //float 0x00000003 DeN
		0x37B00000000001AF,        //float 0x00020000 DeN bit comparison
		0x369FFFFFFFFF870D,        //float 0x00000001 Smallest DeN
		0x368FFFFFFFFF870D,        //float 0x00000000.5 (collapses to 0)
		0x3690000000000000,        //float 0x00000000.5 nearest (collapses to 0)
		0x3690000000000001,        //float 0x00000000.5 nearest+ (rounds up)
		0x380FFFFFBFFFB6EF,        //float 0x007FFFFF Biggest DeN
		0x380FFFFFFFFFBB88,        //float 0x00800000 Smallest non DeN

		//DeNs

		0x0000000000000003,        //DeN
		0x0000000400000003,        //DeN bit comparison
		0x0000000000000001,        //Smallest DeN
		0x000FFFFFFFFFFFFF,        //Biggest DeN
		0x0010000000000000,        //Smallest non DeN
		0x37FFFFFFFFFFF765,        //Resulting in a DeN (float)

		//Rounding with DeN

		0x0000000020000000,
		0x0000000010000000,
		0x0000000008000000,

		//Collapse to 1

		0x3FF0000020000000,        //1.00000011920928955078125
		0x3FF0000020000001,        //1.00000011920928977...
		0x3FF000003FFFFFFF,        //1.00000023841857887...
		0x3FF0000010000000,        //1.000000059604644775390625
		0x3FF0000010000001,        //1.00000005960464499...
		0x3FF000001FFFFFFF,        //1.00000011920928932...
		0x3FF0000008000000,        //1.0000000298023223876953125
		0x3FF000000FFFFFFF,        //1.00000005960464455...

		//High exponents

		0x47D2CED32A16A1B1,        //1e38
		0x48078287F49C4A1D,        //1e39
		0x483D6329F1C35CA5,        //1e40

		//Low exponents

		0x366244CE242C5561,        //1e-46
		0x3696D601AD376AB9,        //1e-45
		0x37A16C262777579C,        //1e-40
		0x37D5C72FB1552D83,        //1e-39
		0x380B38FB9DAA78E4,        //1e-38
		0x3841039D428A8B8F,        //1e-37
		0x3E112E0BE826D695,        //1e-9
		0x3BC79CA10C924223,        //1e-20

		//Previously failing

		0x369202F02C8DEC27
	};

	for (U64 i = 0; i < sizeof(truncTests) / sizeof(truncTests[0]); ++i) {
		for (U64 j = 0; j < 2; ++j) {

			U64 bits = truncTests[i];
			if (j) bits |= (U64)1 << 63;

			const F64 fd       = F64_fromU64Bits(bits);
			const U32 expected = U32_fromF32Bits((F32)fd);
			const U32 got      = U32_fromF32Bits(F64_castF32(fd));

			Test_assert(test, "F64_castF32", got == expected);
		}
	}
}
