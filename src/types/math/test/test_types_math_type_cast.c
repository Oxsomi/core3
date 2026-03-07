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
#include "types/math/type_cast.h"
#include "types/math/type_cast_safe.h"

static const U32 nanBits  = 0x7F800001;
static const U64 nanBitsd = 0x7FF0000000000001;

void Test_typeCastFloatBits(Test *test) {

	Test_setModule(test, "Float bit reinterpret");

	static const U32 pointFive  = 0x3F000000;
	static const U64 pointFived = 0x3FE0000000000000;

	Test_assert(test, "U32_fromF32Bits",     U32_fromF32Bits(0.5f) == pointFive);
	Test_assert(test, "F32_fromU32Bits",     F32_fromU32Bits(pointFive) == 0.5f);

	Test_assert(test, "U64_fromF64Bits",     U64_fromF64Bits(0.5) == pointFived);
	Test_assert(test, "F64_fromU64Bits",     F64_fromU64Bits(pointFived) == 0.5);

	F32 v = 0;
	Test_assert(test, "F32_fromU32BitsSafe", F32_fromU32BitsSafe(pointFive, &v, true, NULL) && v == 0.5f);

	Error tempErr = Error_none();
	Test_assert(test, "F32_fromU32BitsSafe", !F32_fromU32BitsSafe(nanBits, &v, true, &tempErr));
	Test_assert(test, "F32_fromU32BitsSafe", tempErr.genericError == EGenericError_NaN);

	Test_assert(test, "F32_fromU32BitsSafe", F32_fromU32BitsSafe(nanBits, &v, false, NULL) && U32_fromF32Bits(v) == nanBits);

	F64 d = 0;
	Test_assert(test, "F64_fromU64BitsSafe", F64_fromU64BitsSafe(pointFived, &d, true, NULL) && d == 0.5);

	tempErr = Error_none();
	Test_assert(test, "F64_fromU64BitsSafe", !F64_fromU64BitsSafe(nanBitsd, &d, true, &tempErr));
	Test_assert(test, "F64_fromU64BitsSafe", tempErr.genericError == EGenericError_NaN);

	Test_assert(test, "F64_fromU64BitsSafe", F64_fromU64BitsSafe(nanBitsd, &d, false, NULL) && U64_fromF64Bits(d) == nanBitsd);
}

void Test_typeCastSafeInt(Test *test) {

	Test_setModule(test, "Safe int casts");

	I8 i8 = 0;
	Test_assert(test, "I8_fromInt",          I8_fromInt(127, &i8, NULL)  && i8 == 127);
	Test_assert(test, "I8_fromInt",          !I8_fromInt(128, &i8, NULL) && i8 == 127);
	Test_assert(test, "I8_fromUInt",         !I8_fromUInt(128, &i8, NULL) && i8 == 127);
	Test_assert(test, "I8_fromInt",          I8_fromInt(-128, &i8, NULL)  && i8 == -128);
	Test_assert(test, "I8_fromInt",          !I8_fromInt(-129, &i8, NULL) && i8 == -128);

	U8 u8 = 0;
	Test_assert(test, "U8_fromInt",          U8_fromInt(127, &u8, NULL)  && u8 == 127);
	Test_assert(test, "U8_fromInt",          U8_fromInt(128, &u8, NULL)  && u8 == 128);
	Test_assert(test, "U8_fromUInt",         U8_fromUInt(255, &u8, NULL) && u8 == 255);
	Test_assert(test, "U8_fromUInt",         !U8_fromUInt(256, &u8, NULL) && u8 == 255);
	Test_assert(test, "U8_fromInt",          !U8_fromInt(-1, &u8, NULL)  && u8 == 255);
	Test_assert(test, "U8_fromInt",          !U8_fromInt(256, &u8, NULL) && u8 == 255);
}

void Test_typeCastFloatToInt(Test *test) {

	Test_setModule(test, "float -> int");

	I32 i32 = 0;
	Test_assert(test, "I32_fromFloat",       I32_fromFloat(123.0f, &i32, NULL) && i32 == 123);
	Test_assert(test, "I32_fromFloat",       !I32_fromFloat(122.5f, &i32, NULL) && i32 == 123);
	Test_assert(test, "I32_fromFloat",       I32_fromFloat((F32)(1 << 25), &i32, NULL) && i32 == (I32)(1 << 25));
	Test_assert(test, "I32_fromFloat",       !I32_fromFloat(F32_fromU32Bits(nanBits), &i32, NULL));
	Test_assert(test, "I32_fromFloat",       !I32_fromFloat(F32_fromU32Bits(nanBits - 1), &i32, NULL));

	U32 u32 = 0;
	Test_assert(test, "U32_fromFloat",       U32_fromFloat(42.0f, &u32, NULL) && u32 == 42);
	Test_assert(test, "U32_fromFloat",       !U32_fromFloat(-1.0f, &u32, NULL));
	Test_assert(test, "U32_fromFloat",       !U32_fromFloat(4294967296.0f, &u32, NULL));
}

void Test_typeCastDoubleToInt(Test *test) {

	Test_setModule(test, "double -> int");

	static const I64 maxExact = (I64)9007199254740991;	//2^53 - 1, last exactly representable integer in F64

	I64 i64 = 0;
	Test_assert(test, "I64_fromDouble",      I64_fromDouble(9007199254740991.0, &i64, NULL) && i64 == maxExact);
	Test_assert(test, "I64_fromDouble",      !I64_fromDouble(9223372036854775808.0, &i64, NULL));
	Test_assert(test, "I64_fromDouble",      !I64_fromDouble(F64_fromU64Bits(nanBitsd), &i64, NULL));
}

void Test_typeCastIntToFloat(Test *test) {

	Test_setModule(test, "int -> float");

	//F32: 2^24 = 16777216 is the last exactly representable integer; 2^24 + 1 isn't

	F32 f32 = 0;
	Test_assert(test, "F32_fromInt",         F32_fromInt(16777216, &f32, NULL)  && f32 == 16777216.0f);
	Test_assert(test, "F32_fromInt",         !F32_fromInt(16777217, &f32, NULL) && f32 == 16777216.0f);

	//F64: 2^53 is the boundary

	F64 f64 = 0;
	Test_assert(test, "F64_fromInt",         F64_fromInt(9007199254740992LL, &f64, NULL)  && f64 == 9007199254740992.0);
	Test_assert(test, "F64_fromInt",         !F64_fromInt(9007199254740993LL, &f64, NULL) && f64 == 9007199254740992.0);
}

void Test_typeCastFloatDouble(Test *test) {

	Test_setModule(test, "float <-> double");

	//F32_fromDouble

	F32 f32 = 0;
	Test_assert(test, "F32_fromDouble",      F32_fromDouble(1.0, &f32, NULL) && f32 == 1.0f);
	Test_assert(test, "F32_fromDouble",      !F32_fromDouble(1.0 + 1e-12, &f32, NULL) && f32 == 1.0f);
	Test_assert(test, "F32_fromDouble",      !F32_fromDouble(F64_fromU64Bits(nanBitsd - 1), &f32, NULL));

	//F64_fromFloat

	F64 f64 = 0;
	Test_assert(test, "F64_fromFloat",       F64_fromFloat(1.0f, &f64, NULL) && f64 == 1.0);
	Test_assert(test, "F64_fromFloat",       !F64_fromFloat(F32_fromU32Bits(nanBits), &f64, NULL));
}

void Test_typeCast(Test *test) {
	Test_typeCastFloatBits(test);
	Test_typeCastSafeInt(test);
	Test_typeCastFloatToInt(test);
	Test_typeCastDoubleToInt(test);
	Test_typeCastIntToFloat(test);
	Test_typeCastFloatDouble(test);
}