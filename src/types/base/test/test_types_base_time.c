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
	Test_assert(test, "UTC format has Z suffix", nowStr[29] == 'Z' && nowStr[30] == '\0');

	Ns now2 = 0;

	Test_assert(test, "parseFormat", Time_parseFormat(&now2, nowStr, false));
	Test_assert(test, "parseFormat validity", now2 == now);

	Time_format(now, nowStr, true);
	Test_assert(test, "local format has no Z suffix", nowStr[29] == '\0');
	Test_assert(test, "parseFormat (local)", Time_parseFormat(&now2, nowStr, true));
	Test_assert(test, "parseFormat validity (local)", now2 == now);
	Test_assert(test, "local format is not UTC", !Time_parseFormat(&now2, nowStr, false));
	Time_format(now, nowStr, false);
	Test_assert(test, "UTC format is not local", !Time_parseFormat(&now2, nowStr, true));

	const Ns seasons[] = { 1768478400ULL * SECOND, 1784116800ULL * SECOND };

	for(U64 i = 0; i < sizeof(seasons) / sizeof(seasons[0]); ++i) {
		Time_format(seasons[i], nowStr, true);
		Test_assert(test, "local seasonal roundtrip", Time_parseFormat(&now2, nowStr, true) && now2 == seasons[i]);
	}

	TimeFormat invalid[] = {
		"2026-10-22T12:34:",
		"2026-10-22T12:34:00.1",
		"2026-10-22T12:34:00.1Z9",
		"2026-10-22T12:34:00.Z",
		"2026-10-22T12:34:00.1234567890Z",
		"2026-2-22T12:34:00.1Z",
		"2026-02-29T00:00:00.0Z",
		"2026-04-31T00:00:00.0Z",
		"2026-01-00T00:00:00.0Z",
		"2600-01-01T00:00:00.0Z"
	};

	for(U64 i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {

		now2 = now;
		Test_assert(test, "reject invalid timestamp without changing result", !Time_parseFormat(&now2, invalid[i], false));
		Test_assert(test, "invalid timestamp preserves output", now2 == now);
	}

	TimeFormat fractional = "1970-01-01T00:00:00.1Z";

	Test_assert(test, "one fractional digit", Time_parseFormat(&now2, fractional, false) && now2 == SECOND / 10);
	Test_assert(test, "null format", !Time_parseFormat(&now2, NULL, false));

	Date date = { .year = 2000, .month = 2, .day = 29 };

	Test_assert(test, "leap century", Time_date(&date, false) != U64_MAX);
	date.year = 2100;
	Test_assert(test, "nonleap century", Time_date(&date, false) == U64_MAX);
	date = (Date) { .year = 2600, .month = 1, .day = 1 };
	Test_assert(test, "date nanosecond overflow", Time_date(&date, false) == U64_MAX);
}
