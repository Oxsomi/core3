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

#pragma once
#include "types/base/error.h"

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
