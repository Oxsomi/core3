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

//tools/oxc3_cli/test/test_cli_operations.c
//
//The parameter masks of the CLI: a set of parameters is a U64 with one bit per parameter, 64 at most.
//No parameter of the CLI itself is past bit 31, so the table is driven with parameters at bits a 32 bit mask cannot hold.

#include "tools/oxc3_cli/operations.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"
#include "types/test/test.h"

//The argument ParsedArgs_getArg hands out for one shift, compared to what was pushed for it

static Bool Test_cliArg(Test *t, const ParsedArgs *args, U64 shift, const C8 *expected) {

	CharString arg = CharString_createNull();
	const CharString expectedArg = CharString_createRefCStrConst(expected);

	return
		Test_assert(t, "getArg", ParsedArgs_getArg(args, (EOperationHasParameter) shift, &arg, &t->err)) &&
		Test_assert(t, "getArgValue", CharString_equalsStringSensitive(&arg, &expectedArg));
}

OXC3_TEST_MAIN(tools_cli_operations) {

	const Allocator alloc = BasicAllocator_instance;

	Test t = (Test) { 0 };
	t.alloc = &alloc;

	Test_setModule(&t, "CLI/parameterMasks");

	//The masks are U64 and follow their shifts

	Test_assert(&t, "maskIsU64", sizeof(EOperationHasParameter_Exposure) == sizeof(U64));

	Test_assert(&t, "maskFollowsShift",
		EOperationHasParameter_Input == ((U64)1 << EOperationHasParameter_InputShift) &&
		EOperationHasParameter_Exposure == ((U64)1 << EOperationHasParameter_ExposureShift) &&
		EOperationHasParameter_FileFormat == 1
	);

	Test_assert(&t, "anyAES",
		EOperationHasParameter_AnyAES == (EOperationHasParameter_AES | EOperationHasParameter_AESFile)
	);

	Test_assert(&t, "countFitsMask", EOperationHasParameter_CountEnum <= 64);

	//The sets an operation and a format hold keep every bit

	Operation operation = (Operation) { 0 };
	operation.requiredParameters = EOperationHasParameter_Input | ((U64)1 << 40);
	operation.optionalParameters = (U64)1 << 63;

	Format format = (Format) { 0 };
	format.requiredParameters = (U64)1 << 33;
	format.optionalParameters = (U64)1 << 63 | EOperationHasParameter_Output;

	Test_assert(&t, "operationKeepsHighBits",
		((operation.requiredParameters >> 40) & 1) && ((operation.optionalParameters >> 63) & 1) &&
		(operation.requiredParameters & EOperationHasParameter_Input)
	);

	Test_assert(&t, "formatKeepsHighBits",
		((format.requiredParameters >> 33) & 1) && ((format.optionalParameters >> 63) & 1) &&
		(format.optionalParameters & EOperationHasParameter_Output)
	);

	//ParsedArgs_getArg counts the parameters present below a shift to find its argument, and has to see all 64 bits

	ParsedArgs parsed = (ParsedArgs) { 0 };
	parsed.parameters = EOperationHasParameter_Input | ((U64)1 << 33) | ((U64)1 << 40) | ((U64)1 << 63);

	Test_assert(&t, "parsedKeepsHighBits", ((parsed.parameters >> 63) & 1) && ((parsed.parameters >> 33) & 1));

	const C8 *values[4] = { "low", "thirtyThree", "forty", "sixtyThree" };
	Bool pushed = true;

	for(U64 i = 0; i < 4; ++i)
		pushed &= ListCharString_pushBack(&parsed.args, CharString_createRefCStrConst(values[i]), &alloc, &t.err);

	if(Test_assert(&t, "pushArgs", pushed)) {

		Test_cliArg(&t, &parsed, EOperationHasParameter_InputShift, values[0]);
		Test_cliArg(&t, &parsed, 33, values[1]);
		Test_cliArg(&t, &parsed, 40, values[2]);
		Test_cliArg(&t, &parsed, 63, values[3]);

		//A shift that is not set has no argument, however high it is

		CharString none = CharString_createNull();

		Test_assert(&t, "absentLow", !ParsedArgs_getArg(&parsed, EOperationHasParameter_OutputShift, &none, NULL));
		Test_assert(&t, "absentHigh", !ParsedArgs_getArg(&parsed, (EOperationHasParameter) 34, &none, NULL));
		Test_assert(&t, "absentTop", !ParsedArgs_getArg(&parsed, (EOperationHasParameter) 62, &none, NULL));
	}

	ListCharString_free(&parsed.args, &alloc);

	BasicAllocator_checkLeakedMem(&t);

	return Test_end(&t);
}
