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

//types/math/test/test_types_math_flp_f16.c

#include "test_types_math_shared.h"
#include "types/math/flp.h"
#include "types/math/type_cast.h"

void Test_flpF16(Test *test) {

	Test_setModule(test, "F16_castF32/F64");

	static const F16 halfs[] = {
		0x7C00,        //Inf
		0x0000,        //0
		0x7C01,        //NaN #1
		0x7E00,        //NaN (about half)
		0x7E01,        //NaN (about half + 1)
		0x7FFF,        //NaN full
		0x0003,        //DeN
		0x0040,        //DeN bit comparison
		0x0001,        //Smallest DeN
		0x03FF,        //Biggest DeN
		0x0400,        //Smallest non DeN
		0x7BFF,        //Float max
		0x3C00,        //1
		0x3800,        //0.5
		0x4000,        //2
		0x57B0,        //123
		0x3CEB,        //1.23
		0x3BFF,        //Almost 1
	};

	static const U32 expectedF32[] = {
		0x7F800000,        //Inf
		0x00000000,        //0
		0x7FC02000,        //NaN #1
		0x7FC00000,        //NaN (about half)
		0x7FC02000,        //NaN (about half + 1)
		0x7FFFE000,        //NaN full
		0x34400000,        //1.7881393e-7
		0x36800000,        //0.0000038146972
		0x33800000,        //5.9604644e-8
		0x387FC000,        //0.00006097555
		0x38800000,        //0.000061035156
		0x477FE000,        //65504
		0x3F800000,        //1
		0x3F000000,        //0.5
		0x40000000,        //2
		0x42F60000,        //123
		0x3F9D6000,        //1.2294922
		0x3F7FE000,        //0.9995117
	};

	static const U64 expectedF64[] = {
		0x7FF0000000000000,        //Inf
		0x0000000000000000,        //0
		0x7FF8040000000000,        //NaN #1
		0x7FF8000000000000,        //NaN (about half)
		0x7FF8040000000000,        //NaN (about half + 1)
		0x7FFFFC0000000000,        //NaN full
		0x3E88000000000000,        //1.7881393432617188e-7
		0x3ED0000000000000,        //0.000003814697265625
		0x3E70000000000000,        //5.960464477539063e-8
		0x3f0ff80000000000,        //0.00006097555160522461
		0x3F10000000000000,        //0.00006103515625
		0x40EFFC0000000000,        //65504
		0x3FF0000000000000,        //1
		0x3FE0000000000000,        //0.5
		0x4000000000000000,        //2
		0x405EC00000000000,        //123
		0x3FF3AC0000000000,        //1.2294921875
		0x3FEFFC0000000000,        //0.99951171875
	};

	for (U64 i = 0; i < sizeof(halfs) / sizeof(halfs[0]); ++i) {
		for (U64 j = 0; j < 2; ++j) {

			F16 fh = halfs[i];
			if (j) fh |= (U16)1 << 15;

			U32 expF32 = expectedF32[i];
			if (j) expF32 |= (U32)1 << 31;

			U64 expF64 = expectedF64[i];
			if (j) expF64 |= (U64)1 << 63;

			Test_assert(test, "F16_castF32", U32_fromF32Bits(F16_castF32(fh)) == expF32);
			Test_assert(test, "F16_castF64", U64_fromF64Bits(F16_castF64(fh)) == expF64);
		}
	}
	
	Test_setModule(test, "F32_castF16 / F64_castF16");

	static const U32 inputF32[] = {
		0x7F800000,        //Inf
		0x4781E480,        //66505 (collapses to Inf)
		0x7E967699,        //1e38 (collapses to Inf)
		0x00000000,        //0
		0x34400000,        //1.7881393e-7
		0x36800000,        //0.0000038146972
		0x33800000,        //5.9604644e-8
		0x387FC000,        //0.00006097555
		0x38800000,        //0.000061035156
		0x477FE000,        //65504
		0x3F800000,        //1
		0x3F000000,        //0.5
		0x40000000,        //2
		0x42F60000,        //123
		0x3F9D6000,        //1.2294922
		0x3F7FE000,        //0.9995117
		0x7FC02000,        //NaN #1
		0x7FC00000,        //NaN (about half)
		0x7FC02000,        //NaN (about half + 1)
		0x7FFFE000,        //NaN full
		0x3F801000,        //Collapse to 1
		0x3F802000,        //Almost collapse to 1
		0x006CE3EE,        //1e-38 collapse to 0
		0x33802000,        //Doesn't collapse to 0
		0x33801FFF,        //Doesn't collapse to 0
		0x33800001,        //Doesn't collapse to 0
		0x33000000,        //Collapses to 0
	};

	static const U64 inputF64[] = {
		0x7FF0000000000000,        //Inf
		0x40F03C9000000000,        //65505 (collapses to Inf)
		0x7FE1CCF385EBC8A0,        //1e308 (collapses to Inf)
		0x0000000000000000,        //0
		0x3E88000000000000,        //1.7881393432617188e-7
		0x3ED0000000000000,        //0.000003814697265625
		0x3E70000000000000,        //5.960464477539063e-8
		0x3f0ff80000000000,        //0.00006097555160522461
		0x3F10000000000000,        //0.00006103515625
		0x40EFFC0000000000,        //65504
		0x3FF0000000000000,        //1
		0x3FE0000000000000,        //0.5
		0x4000000000000000,        //2
		0x405EC00000000000,        //123
		0x3FF3AC0000000000,        //1.2294921875
		0x3FEFFC0000000000,        //0.99951171875
		0x7FF8040000000000,        //NaN #1
		0x7FF8000000000000,        //NaN (about half)
		0x7FF8040000000000,        //NaN (about half + 1)
		0x7FFFFC0000000000,        //NaN full
		0x3FF0020000000000,        //Collapse to 1
		0x3FF0040000000000,        //Almost collapse to 1
		0x000730D67819E8D2,        //Collapse to 0 (1e-308)
		0x3E70040000000000,        //Doesn't collapse to 0
		0x3E7003FFFFFFFFFF,        //Doesn't collapse to 0
		0x3E70000000000001,        //Doesn't collapse to 0
		0x3E60000000000000,        //Collapses to 0
	};

	static const F16 expectedF16[] = {
		0x7C00,        //Inf
		0x7C00,        //Inf
		0x7C00,        //Inf
		0x0000,        //0
		0x0003,        //DeN
		0x0040,        //DeN bit comparison
		0x0001,        //Smallest DeN
		0x03FF,        //Biggest DeN
		0x0400,        //Smallest non DeN
		0x7BFF,        //Float max
		0x3C00,        //1
		0x3800,        //0.5
		0x4000,        //2
		0x57B0,        //123
		0x3CEB,        //1.23
		0x3BFF,        //Almost 1
		0x7E01,        //NaN #1
		0x7E00,        //NaN (about half)
		0x7E01,        //NaN (about half + 1)
		0x7FFF,        //NaN full
		0x3C00,        //Collapse to 1
		0x3C01,        //Almost collapse to 1
		0x0000,        //Collapse to 0
		0x0001,        //Doesn't collapse to 0
		0x0001,        //Doesn't collapse to 0
		0x0001,        //Doesn't collapse to 0
		0x0000,        //Collapses to 0
	};

	for (U64 i = 0; i < sizeof(expectedF16) / sizeof(expectedF16[0]); ++i) {
		for (U64 j = 0; j < 2; ++j) {

			U32 f32bits = inputF32[i];
			U64 f64bits = inputF64[i];
			F16 exp     = expectedF16[i];

			if (j) {
				f32bits |= (U32)1 << 31;
				f64bits |= (U64)1 << 63;
				exp     |= (U16)1 << 15;
			}

			Test_assert(test, "F32_castF16", F32_castF16(F32_fromU32Bits(f32bits)) == exp);
			Test_assert(test, "F64_castF16", F64_castF16(F64_fromU64Bits(f64bits)) == exp);
		}
	}
}
