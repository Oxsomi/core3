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

#include "types/base/mathi.h"
#include "shared.h"

void Test_mathu(Test *test) {

	Test_setModule(test, "Mathu");

	//U32

	//Exps

	Test_assert(test, "U32_exp10", U32_exp10(0) == 1);
	Test_assert(test, "U32_exp10", U32_exp10(1) == 10);
	Test_assert(test, "U32_exp10", U32_exp10(9) == 1000000000);
	Test_assert(test, "U32_exp10 out of bounds", U32_exp10(10) == (U32)-1);

	Test_assert(test, "U32_exp2", U32_exp2(0) == 1);
	Test_assert(test, "U32_exp2", U32_exp2(1) == 2);
	Test_assert(test, "U32_exp2", U32_exp2(31) == 0x80000000);
	Test_assert(test, "U32_exp2 out of bounds", U32_exp2(32) == (U32)-1);

	//Safe div
	
	Test_assert(test, "U32_safeDiv", U32_safeDiv(10, 2) == 5);
	Test_assert(test, "U32_safeDiv", U32_safeDiv(7, 0) == 0);
	Test_assert(test, "U32_safeDiv", U32_safeDiv(0, 5) == 0);

	//ROR / ROL (Rotate right/left)

	Test_assert(test, "U32_rol", U32_rol(1, 1) == 2);
	Test_assert(test, "U32_ror", U32_ror(2, 1) == 1);

	Test_assert(test, "U32_rol", U32_rol(0xDEADBEEF, 16) == 0xBEEFDEAD);
	Test_assert(test, "U32_ror", U32_ror(0xDEADBEEF, 16) == 0xBEEFDEAD);
	Test_assert(test, "U32_rol", U32_rol(0xDEADBEEF, 8) == 0xADBEEFDE);
	Test_assert(test, "U32_ror", U32_ror(0xDEADBEEF, 8) == 0xEFDEADBE);

	Test_assert(test, "U32_rol round", U32_ror(U32_rol(0xDEADBEEF, 7), 7) == 0xDEADBEEF);
	Test_assert(test, "U32_ror round", U32_rol(U32_ror(0xDEADBEEF, 13), 13) == 0xDEADBEEF);

	Test_assert(test, "U32_rol", U32_rol(0xABCDu, 0) == 0xABCDu);
	Test_assert(test, "U32_ror", U32_ror(0xABCDu, 0) == 0xABCDu);

	//U64

	Test_assert(test, "U64_exp10", U64_exp10(0) == 1);
	Test_assert(test, "U64_exp10", U64_exp10(19) == 10000000000000000000);
	Test_assert(test, "U64_exp10", U64_exp10(20) == (U64)-1);

	Test_assert(test, "U64_exp2", U64_exp2(0) == 1);
	Test_assert(test, "U64_exp2", U64_exp2(63) == 0x8000000000000000);
	Test_assert(test, "U64_exp2", U64_exp2(64) == (U64)-1);

	Test_assert(test, "U64_safeDiv", U64_safeDiv(100, 10) == 10);
	Test_assert(test, "U64_safeDiv", U64_safeDiv(1, 0) == 0);

	Test_assert(test, "U64_rol", U64_rol(0xDEADBEEFCAFEBABE, 8) == 0xADBEEFCAFEBABEDE);
	Test_assert(test, "U64_ror", U64_ror(0xDEADBEEFCAFEBABE, 8) == 0xBEDEADBEEFCAFEBA);

	//U16

	Test_assert(test, "U16_exp10", U16_exp10(0) == 1);
	Test_assert(test, "U16_exp10", U16_exp10(4) == 10000);
	Test_assert(test, "U16_exp10", U16_exp10(5) == (U16)-1);

	Test_assert(test, "U16_exp2", U16_exp2(0) == 1);
	Test_assert(test, "U16_exp2", U16_exp2(15) == 0x8000u);
	Test_assert(test, "U16_exp2", U16_exp2(16) == (U16)-1);

	Test_assert(test, "U16_safeDiv", U16_safeDiv(100, 4) == 25);
	Test_assert(test, "U16_safeDiv", U16_safeDiv(100, 0) == 0);

	//U8

	Test_assert(test, "U8_exp10", U8_exp10(0) == 1);
	Test_assert(test, "U8_exp10", U8_exp10(2) == 100);
	Test_assert(test, "U8_exp10", U8_exp10(3) == (U8)-1);

	Test_assert(test, "U8_exp2", U8_exp2(0) == 1);
	Test_assert(test, "U8_exp2", U8_exp2(7) == 128);
	Test_assert(test, "U8_exp2", U8_exp2(8) == (U8)-1);

	Test_assert(test, "U8_safeDiv", U8_safeDiv(200, 4) == 50);
	Test_assert(test, "U8_safeDiv", U8_safeDiv(200, 0) == 0);
}

void Test_mathi(Test *test) {

	Test_setModule(test, "Mathi");

	//I32

	//Exp

	Test_assert(test, "I32_exp10", I32_exp10(0) == 1);
	Test_assert(test, "I32_exp10", I32_exp10(9) == 1000000000);
	Test_assert(test, "I32_exp10", I32_exp10(10) == -1);
	Test_assert(test, "I32_exp10", I32_exp10(-1) == -1);

	Test_assert(test, "I32_exp2", I32_exp2(0) == 1);
	Test_assert(test, "I32_exp2", I32_exp2(30) == 0x40000000);
	Test_assert(test, "I32_exp2", I32_exp2(31) == -1);
	Test_assert(test, "I32_exp2", I32_exp2(-1) == -1);

	//Abs
	
	Test_assert(test, "I32_abs", I32_abs(0) == 0);
	Test_assert(test, "I32_abs", I32_abs(5) == 5);
	Test_assert(test, "I32_abs", I32_abs(-5) == 5);
	Test_assert(test, "I32_abs", I32_abs(-100) == 100);

	//Safe div
	
	Test_assert(test, "I32_safeDiv", I32_safeDiv(9, 3) == 3);
	Test_assert(test, "I32_safeDiv", I32_safeDiv(-9, -3) == 3);
	Test_assert(test, "I32_safeDiv", I32_safeDiv(-9, 3) == -3);
	Test_assert(test, "I32_safeDiv", I32_safeDiv(9, -3) == -3);
	Test_assert(test, "I32_safeDiv", I32_safeDiv(9, 0) == 0);

	//I64

	Test_assert(test, "I64_exp10", I64_exp10(0) == 1);
	Test_assert(test, "I64_exp10", I64_exp10(18) == 1000000000000000000);
	Test_assert(test, "I64_exp10", I64_exp10(19) == -1);
	Test_assert(test, "I64_exp10", I64_exp10(-1) == -1);

	Test_assert(test, "I64_exp2", I64_exp2(0) == 1);
	Test_assert(test, "I64_exp2", I64_exp2(62) == 0x4000000000000000);
	Test_assert(test, "I64_exp2", I64_exp2(63) == -1);
	Test_assert(test, "I64_exp2", I64_exp2(-1) == -1);

	Test_assert(test, "I64_abs", I64_abs(0) == 0);
	Test_assert(test, "I64_abs", I64_abs(1) == 1);
	Test_assert(test, "I64_abs", I64_abs(-1) == 1);
	Test_assert(test, "I64_abs", I64_abs(-1000000000) == 1000000000);

	Test_assert(test, "I64_safeDiv", I64_safeDiv(100, 7) == 14);
	Test_assert(test, "I64_safeDiv", I64_safeDiv(100, 0) == 0);

	//I16

	Test_assert(test, "I16_exp10", I16_exp10(0) == 1);
	Test_assert(test, "I16_exp10", I16_exp10(4) == 10000);
	Test_assert(test, "I16_exp10", I16_exp10(5) == -1);
	Test_assert(test, "I16_exp10", I16_exp10(-1) == -1);

	Test_assert(test, "I16_exp2", I16_exp2(0) == 1);
	Test_assert(test, "I16_exp2", I16_exp2(14) == 0x4000);
	Test_assert(test, "I16_exp2", I16_exp2(15) == -1);
	Test_assert(test, "I16_exp2", I16_exp2(-1) == -1);

	Test_assert(test, "I16_abs", I16_abs(0) == 0);
	Test_assert(test, "I16_abs", I16_abs(32767) == 32767);
	Test_assert(test, "I16_abs", I16_abs(-32767) == 32767);
	Test_assert(test, "I16_safeDiv", I16_safeDiv(100, 5) == 20);
	Test_assert(test, "I16_safeDiv", I16_safeDiv(100, 0) == 0);

	//I8

	Test_assert(test, "I8_exp10", I8_exp10(0) == 1);
	Test_assert(test, "I8_exp10", I8_exp10(2) == 100);
	Test_assert(test, "I8_exp10", I8_exp10(3) == -1);
	Test_assert(test, "I8_exp10", I8_exp10(-1) == -1);

	Test_assert(test, "I8_exp2", I8_exp2(0) == 1);
	Test_assert(test, "I8_exp2", I8_exp2(6) == 0x40);
	Test_assert(test, "I8_exp2", I8_exp2(7) == -1);
	Test_assert(test, "I8_exp2", I8_exp2(-1) == -1);

	Test_assert(test, "I8_abs", I8_abs(0) == 0);
	Test_assert(test, "I8_abs", I8_abs(127) == 127);
	Test_assert(test, "I8_abs", I8_abs(-127) == 127);
	Test_assert(test, "I8_safeDiv", I8_safeDiv(100, 5) == 20);
	Test_assert(test, "I8_safeDiv", I8_safeDiv(100, 0) == 0);
}
