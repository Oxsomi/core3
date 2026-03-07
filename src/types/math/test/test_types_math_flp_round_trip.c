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

#include "test_types_math_shared.h"
#include "types/math/flp.h"
#include "types/math/type_cast.h"

void Test_flpRoundTrip(Test *test) {

	Test_setModule(test, "Flp round trip");

	//F32 -> F64 -> F32 should be lossless for finite normals

	static const U32 normals[] = {
		0x3F800000,		//1
		0x3F000000,		//0.5
		0x40000000,		//2
		0x42F60000,		//123
		0x7F7FFFFF,		//Float max
		0x00800000,		//Smallest non-DeN
		0x80000000,		//-0
		0x00000000,		//+0
		0xFF800000,		//-Inf
		0x7F800000,		//+Inf
	};

	for (U64 i = 0; i < sizeof(normals) / sizeof(normals[0]); ++i) {
		const F32 original = F32_fromU32Bits(normals[i]);
		const F32 roundTrip = F64_castF32(F32_castF64(original));
		Test_assert(test, "F32->F64->F32", U32_fromF32Bits(roundTrip) == normals[i]);
	}

	//F16 -> F32 -> F16 and F16 -> F64 -> F16 for values that fit cleanly

	static const F16 f16normals[] = {
		0x3C00,		//1
		0x3800,		//0.5
		0x4000,		//2
		0x57B0,		//123
		0x7BFF,		//Float max
		0x7C00,		//Inf
		0x0000,		//0
	};

	for (U64 i = 0; i < sizeof(f16normals) / sizeof(f16normals[0]); ++i) {
		Test_assert(test, "F16->F32->F16", F32_castF16(F16_castF32(f16normals[i])) == f16normals[i]);
		Test_assert(test, "F16->F64->F16", F64_castF16(F16_castF64(f16normals[i])) == f16normals[i]);
	}
}
