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

//types/test/test.h

#pragma once
#include "types/base/error.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;

typedef struct Test {
	U64 tests, succeeded;
	U64 totalTests, totalSucceeded;
	const Allocator *alloc;
	const C8 *currentModule;
	Error err;
} Test;

Bool Test_assert2(Test *test, const C8 *section, Bool value, const C8 *file, U64 line, const C8 *source);

#define Test_assert(test, section, ...) Test_assert2(test, section, (__VA_ARGS__), __FILE__, __LINE__, #__VA_ARGS__)

void Test_print(Test *test, const C8 *str);

void Test_setModule(Test *test, const C8 *moduleName);

int Test_end(Test *test);

//Every suite is its own executable on desktop, but android has no exec: the whole set runs as one .so inside an APK.
//Under _OXC3_TEST_BUNDLED each "main" becomes a named int (*)(void*) that the bundle's android_main calls in turn;
// otherwise it's the entry point it has always been.
//OXC3_TEST_MAIN is for suites that only need an allocator,
//OXC3_TEST_ENTRY for the ones that bring up a Platform and therefore need the entry argument android_native_app_glue hands us.

#ifdef _OXC3_TEST_BUNDLED

	//A MAIN suite is written without the entry argument, so it gets a trampoline that forwards it under the same
	// name the ENTRY suites use.
	//It has to reach the body rather than be discarded: every suite here brackets itself with Platform_create and
	// Platform_cleanup, so one running after another has no platform left, and on android Platform_getData() is the
	// entry argument - it's the only way back to the android_app.
	//Marked unused because most suites never look at it, and the bundle builds with -Wextra -Werror.
	//This branch is android only (only src/test/android defines _OXC3_TEST_BUNDLED), so the attribute is safe.

	#define OXC3_TEST_MAIN(name)                                                                      \
		static int OxC3_testBody_##name(void *state);                                                 \
		int OxC3_test_##name(void *state) { return OxC3_testBody_##name(state); }                     \
		static int OxC3_testBody_##name(void *state __attribute__((unused)))

	#define OXC3_TEST_ENTRY(name) int OxC3_test_##name(void *state)

#else
	#define OXC3_TEST_MAIN(name) int main()
	#define OXC3_TEST_ENTRY(name) Platform_defineEntrypoint()
#endif

#ifdef __cplusplus
	}
#endif
