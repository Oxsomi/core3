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

//types/base/test/test_types_base_time.c

#include "test_types_base_shared.h"
#include "types/base/time.h"

void Test_time(Test *test) {

	Test_setModule(test, "Time");

	Ns now = Time_now();
	TimeFormat nowStr;

	Time_format(now, nowStr, false);

	Ns now2 = 0;

	Test_assert(test, "parseFormat", NULL, Time_parseFormat(&now2, nowStr, false));
	Test_assert(test, "parseFormat", "Parsed timestamp invalid", now2 == now);

	Time_format(now, nowStr, true);
	Test_assert(test, "parseFormat (local)", NULL, Time_parseFormat(&now2, nowStr, true));
	Test_assert(test, "parseFormat (local)", "Parsed timestamp invalid", now2 == now);
}
