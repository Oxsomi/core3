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

//types/base/test/shared/shared.c

#include "types/test/test.h"
#include <stdio.h>
#include <inttypes.h>

static inline const C8 *Test_prefix(Test *test) {
	return !test ? "???" : (test->currentModule ? test->currentModule : "End");
}

void Test_print(Test *test, const C8 *str) {
	printf("-- %s: %s\n", Test_prefix(test), str);            //No access to Log, that's in container
}

void Test_printPlatformCreateFail(const Error *e) {

	if (!e || !e->genericError) {
		printf("-- Fatal: Platform_create failed without error info\n");
		return;
	}

	printf(
		"-- Fatal: Platform_create failed: %s (%s, sub %"PRIu32", param %"PRIu32": %"PRIX64" %"PRIX64")\n",
		e->errorStr ? e->errorStr : "???",
		EGenericError_TO_STRING[e->genericError],
		e->errorSubId, e->paramId, e->paramValue0, e->paramValue1
	);
}

void Test_setModule(Test *test, const C8 *moduleName) {

	if (test->currentModule) {

		if (test->succeeded == test->tests)
			printf("-- %s: Success (%"PRIu64")\n", Test_prefix(test), test->succeeded);

		else printf("-- %s: Failed (%"PRIu64"/%"PRIu64")\n", Test_prefix(test), test->succeeded, test->tests);
	}

	test->currentModule = moduleName;
	test->tests = 0;
	test->succeeded = 0;
}

int Test_end(Test *test) {

	Test_setModule(test, NULL);

	if (test->totalSucceeded == test->totalTests)
		printf("-- %s: Success (%"PRIu64")\n", Test_prefix(test), test->totalSucceeded);

	else printf("-- %s: Failed (%"PRIu64"/%"PRIu64")\n", Test_prefix(test), test->totalSucceeded, test->totalTests);

	return test->totalSucceeded != test->totalTests;
}

Bool Test_assert2(Test *test, const C8 *section, Bool value, const C8 *file, U64 line, const C8 *source) {

	if (!test)
		return false;

	++test->tests;
	++test->totalTests;

	if (value && !test->err.genericError) {
		++test->succeeded;
		++test->totalSucceeded;
	}

	else printf(
		"-- %s (%s): Failed at %s:%"PRIu64" (%s)\n",
		Test_prefix(test),
		!section ? "???" : section,
		!file ? "???" : file,
		line,
		!source ? "???" : source
	);

	//Guarded and newline terminated because this runs while a failure is already being reported.
	//A NULL errorStr would otherwise crash the harness mid-report and take the rest of the suite's output
	// with it, and without the newline android's log pump merges this into the next assert and truncates it.

	if (test->err.genericError)
		printf(
			"-- Error: %s (%s)\n",
			test->err.genericError < EGenericError_Stderr + 1 ?
				EGenericError_TO_STRING[test->err.genericError] : "???",
			test->err.errorStr ? test->err.errorStr : "???"
		);

	test->err = Error_none();
	return value;
}
