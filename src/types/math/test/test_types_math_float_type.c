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

//types/math/test/test_types_math_float_type.c

#include "test_types_math_shared.h"
#include "types/math/flp.h"

void Test_floatType(Test *test) {

	Test_setModule(test, "EFloatType");

	//Sizes / shifts / masks for common types

	Test_assert(test, "F16 bytes",         EFloatType_bytes(EFloatType_F16) == 2);
	Test_assert(test, "F32 bytes",         EFloatType_bytes(EFloatType_F32) == 4);
	Test_assert(test, "F64 bytes",         EFloatType_bytes(EFloatType_F64) == 8);

	Test_assert(test, "F16 exponentBits",  EFloatType_exponentBits(EFloatType_F16) == 5);
	Test_assert(test, "F32 exponentBits",  EFloatType_exponentBits(EFloatType_F32) == 8);
	Test_assert(test, "F64 exponentBits",  EFloatType_exponentBits(EFloatType_F64) == 11);

	Test_assert(test, "F16 mantissaBits",  EFloatType_mantissaBits(EFloatType_F16) == 10);
	Test_assert(test, "F32 mantissaBits",  EFloatType_mantissaBits(EFloatType_F32) == 23);
	Test_assert(test, "F64 mantissaBits",  EFloatType_mantissaBits(EFloatType_F64) == 52);

	//signShift = mantissaBits + exponentBits

	Test_assert(test, "F16 signShift",     EFloatType_signShift(EFloatType_F16) == 15);
	Test_assert(test, "F32 signShift",     EFloatType_signShift(EFloatType_F32) == 31);
	Test_assert(test, "F64 signShift",     EFloatType_signShift(EFloatType_F64) == 63);

	//signMask

	Test_assert(test, "F32 signMask",      EFloatType_signMask(EFloatType_F32) == 0x80000000);
	Test_assert(test, "F64 signMask",      EFloatType_signMask(EFloatType_F64) == 0x8000000000000000);
	Test_assert(test, "F16 signMask",      EFloatType_signMask(EFloatType_F16) == 0x8000);

	//exponentMask (not shifted)

	Test_assert(test, "F32 exponentMask",  EFloatType_exponentMask(EFloatType_F32) == 0xFF);
	Test_assert(test, "F64 exponentMask",  EFloatType_exponentMask(EFloatType_F64) == 0x7FF);
	Test_assert(test, "F16 exponentMask",  EFloatType_exponentMask(EFloatType_F16) == 0x1F);

	//mantissaMask (not shifted)

	Test_assert(test, "F32 mantissaMask",  EFloatType_mantissaMask(EFloatType_F32) == 0x7FFFFF);
	Test_assert(test, "F64 mantissaMask",  EFloatType_mantissaMask(EFloatType_F64) == 0xFFFFFFFFFFFFF);
	Test_assert(test, "F16 mantissaMask",  EFloatType_mantissaMask(EFloatType_F16) == 0x3FF);

	//sign / abs / negate on a known F32

	const U64 negOne32 = 0xBF800000;        //-1
	const U64 posOne32 = 0x3F800000;        //+1

	Test_assert(test, "F32 sign",          EFloatType_sign(EFloatType_F32, negOne32));
	Test_assert(test, "F32 sign",          !EFloatType_sign(EFloatType_F32, posOne32));
	Test_assert(test, "F32 abs",           EFloatType_abs(EFloatType_F32, negOne32) == posOne32);
	Test_assert(test, "F32 negate",        EFloatType_negate(EFloatType_F32, posOne32) == negOne32);
	Test_assert(test, "F32 negate",        EFloatType_negate(EFloatType_F32, negOne32) == posOne32);

	//exponent / mantissa on known F32 values

	Test_assert(test, "F32 exponent",      EFloatType_exponent(EFloatType_F32, posOne32) == 127);
	Test_assert(test, "F32 exponent",      EFloatType_exponent(EFloatType_F32, 0x40000000) == 128);
	Test_assert(test, "F32 exponent",      EFloatType_exponent(EFloatType_F32, 0x3F000000) == 126);
	Test_assert(test, "F32 mantissa",      EFloatType_mantissa(EFloatType_F32, posOne32) == 0);
	Test_assert(test, "F32 mantissa",      EFloatType_mantissa(EFloatType_F32, 0x3FC00000) == 0x400000);

	//isFinite / isDeN / isNaN / isInf / isZero on known bit patterns

	Test_assert(test, "F32 isFinite",      EFloatType_isFinite(EFloatType_F32, posOne32));
	Test_assert(test, "F32 isFinite",      !EFloatType_isFinite(EFloatType_F32, 0x7F800000));
	Test_assert(test, "F32 isFinite",      !EFloatType_isFinite(EFloatType_F32, 0x7FC00000));

	Test_assert(test, "F32 isDeN",         EFloatType_isDeN(EFloatType_F32, 0x00000001));
	Test_assert(test, "F32 isDeN",         !EFloatType_isDeN(EFloatType_F32, posOne32));
	Test_assert(test, "F32 isDeN",         EFloatType_isDeN(EFloatType_F32, 0));

	Test_assert(test, "F32 isNaN",         EFloatType_isNaN(EFloatType_F32, 0x7FC00000));
	Test_assert(test, "F32 isNaN",         !EFloatType_isNaN(EFloatType_F32, 0x7F800000));
	Test_assert(test, "F32 isNaN",         !EFloatType_isNaN(EFloatType_F32, posOne32));

	Test_assert(test, "F32 isInf",         EFloatType_isInf(EFloatType_F32, 0x7F800000));
	Test_assert(test, "F32 isInf",         EFloatType_isInf(EFloatType_F32, 0xFF800000));
	Test_assert(test, "F32 isInf",         !EFloatType_isInf(EFloatType_F32, 0x7FC00000));
	Test_assert(test, "F32 isInf",         !EFloatType_isInf(EFloatType_F32, posOne32));

	Test_assert(test, "F32 isZero",        EFloatType_isZero(EFloatType_F32, 0));
	Test_assert(test, "F32 isZero",        EFloatType_isZero(EFloatType_F32, 0x80000000));
	Test_assert(test, "F32 isZero",        !EFloatType_isZero(EFloatType_F32, posOne32));

	//BF16 metadata

	Test_assert(test, "BF16 bytes",        EFloatType_bytes(EFloatType_BF16) == 2);
	Test_assert(test, "BF16 exponentBits", EFloatType_exponentBits(EFloatType_BF16) == 8);
	Test_assert(test, "BF16 mantissaBits", EFloatType_mantissaBits(EFloatType_BF16) == 7);
}
