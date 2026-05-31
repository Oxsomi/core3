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

//types/base/time.c

#include "types/base/platform_types.h"
#include "types/base/time.h"
#include "types/base/c8.h"
#include "types/base/constants.h"
#include "types/base/string.h"
#include "types/base/buffer.h"
#include "types/base/mathi.h"
#include <time.h>

#ifdef _WIN32
	#include <intrin.h>
	#define timegm _mkgmtime
	#define localtime_r(a, b) (!localtime_s(b, a))
	#define gmtime_r(a, b) (!gmtime_s(b, a))
#elif !defined(__clang__)
	#include <x86intrin.h>
#endif

Ns Time_now() {

	struct timespec ts;

	if(!timespec_get(&ts, TIME_UTC))
		return (Ns) U64_MAX;

	return (Ns)ts.tv_sec * SECOND + (Ns)ts.tv_nsec;
}

#ifdef __clang__
	U64 Time_clocks() {
		return __builtin_readcyclecounter();
	}
#elif _ARCH == ARCH_ARM64

	//Windows doesn't support inline ASM with arm64,
	//To support this anyways, we have time_clocks_arm64.s
	//This will then turn into time_clocks_arm64.obj and we will link this.
	
	#ifndef _WIN32
		U64 Time_clocks() {
			U64 result;
			asm volatile("mrs %0, cntvct_el0" : "=r"(result));
			return result;
		}
	#else
		extern U64 Time_clocksAsm();
		U64 Time_clocks() { return Time_clocksAsm(); }
	#endif

#else
	U64 Time_clocks() {
		return __rdtsc();
	}
#endif

//ISO 8601 e.g. 2022-02-26T21:08:45.000000000Z
//The standard functions strp don't work properly cross-platform.

static inline void setNum(TimeFormat format, I64 offset, U64 length, U64 v) {

	I64 off = (I64)(offset + length) - 1;

	while (v && off >= offset) {
		format[off] = '0' + v % 10;
		--off;
		v /= 10;
	}
}

const C8 FORMAT_STR[] = "0000-00-00T00:00:00.000000000Z";
const U8 OFFSETS[] =    { 0, 5, 8, 11, 14, 17, 20 };
const U8 SIZES[] =        { 4, 2, 2,  2,  2,  2, 9 };

void Time_format(Ns time, TimeFormat timeString, Bool isLocalTime) {

	const time_t inSecs = (time_t)(time / SECOND);
	const Ns inNs = time % SECOND;

	struct tm t = (struct tm) { 0 };

	Bool success = false;

	if(isLocalTime)
		success = localtime_r(&inSecs, &t);

	else success = gmtime_r(&inSecs, &t);

	Buffer_memcpy(
		Buffer_createRef(timeString, SHORTSTRING_LEN),
		Buffer_createRefConst(FORMAT_STR, sizeof(FORMAT_STR))
	);

	if(!success)
		return;

	setNum(timeString, OFFSETS[0], SIZES[0], (U64)t.tm_year + 1900);
	setNum(timeString, OFFSETS[1], SIZES[1], (U64)t.tm_mon + 1);
	setNum(timeString, OFFSETS[2], SIZES[2], (U64)t.tm_mday);
	setNum(timeString, OFFSETS[3], SIZES[3], (U64)t.tm_hour);
	setNum(timeString, OFFSETS[4], SIZES[4], (U64)t.tm_min);
	setNum(timeString, OFFSETS[5], SIZES[5], (U64)t.tm_sec);
	setNum(timeString, OFFSETS[6], SIZES[6], inNs);
}

static const C8 separators[] = "--T::.Z";

Bool Time_parseFormat(Ns *time, TimeFormat format, Bool isLocalTime) {

	if (!time)
		return false;

	const U64 length = CharString_calcStrLen(format, SHORTSTRING_LEN - 1);

	U64 curr = 0, currSep = 0, prevI = U64_MAX;

	Date date = { 0 };

	for (U64 i = 0; i < length; ++i) {

		const C8 c = format[i];

		if (C8_isDec(c)) {

			if(curr == U64_MAX)
				return false;

			curr *= 10;

			const U64 prev = curr;
			curr += C8_dec(c);

			if(curr < prev)
				return false;

			if (curr * 10 < curr)
				curr = U64_MAX;

			continue;
		}

		if (currSep == sizeof(separators) - 1 || c != separators[currSep])
			return false;

		if(currSep < 6) {

			if (curr > U16_MAX)
				return false;

			if (currSep && curr > U8_MAX)            //Everything but year is 8-bit
				return false;
		}

		switch (currSep) {

			case 0:        date.year = (U16) curr;        break;
			case 1:        date.month = (U8) curr;        break;
			case 2:        date.day = (U8) curr;        break;
			case 3:        date.hour = (U8) curr;        break;
			case 4:        date.minute = (U8) curr;    break;
			case 5:        date.second = (U8) curr;    break;

			case 6: {    //Nanoseconds

				const int dif = (int)(i - prevI);

				if(dif == 0)
					return false;

				const U64 mul = U64_exp10(9 - dif);

				if(mul == U64_MAX)
					return false;

				if (curr * mul >= SECOND)
					return false;

				date.ns = (U32)(curr * mul);
				break;
			}

			default:
				return false;
		}

		curr = 0;
		++currSep;
		prevI = i + 1;
	}

	const Ns res = Time_date(&date, isLocalTime);

	if (res == U64_MAX)
		return false;

	*time = res;
	return true;
}

Ns Time_date(const Date *date, Bool isLocalTime) {

	if(
		!date ||
		date->year < 1970 ||
		date->month < 1 || date->month > 12 ||
		date->day > 31 ||
		date->hour >= 24 || date->minute >= 60 || date->second >= 60 ||
		date->ns >= SECOND
	)
		return U64_MAX;

	struct tm tm = {
		.tm_year = date->year - 1900, .tm_mon = date->month - 1, .tm_mday = date->day,
		.tm_hour = date->hour, .tm_min = date->minute, .tm_sec = date->second
	};

	if(isLocalTime)
		tm.tm_isdst = -1;        //Lookup if daylight savings time is active

	const time_t ts = isLocalTime ? mktime(&tm) : timegm(&tm);

	if (ts == (time_t)-1 || (U64)ts * SECOND + date->ns < (U64)ts)
		return U64_MAX;

	return (Ns)ts * SECOND + date->ns;
}

Bool Time_getDate(Ns timestamp, Date *date, Bool isLocalTime) {

	const time_t inSecs = (time_t)(timestamp / SECOND);

	struct tm t = (struct tm) { 0 };

	Bool success = false;

	if(isLocalTime)
		success = localtime_r(&inSecs, &t);

	else success = gmtime_r(&inSecs, &t);

	if(!success)
		return false;

	if(t.tm_year > 1900 + U16_MAX)
		return false;

	if (!date)
		return false;

	date->ns = (U32)(timestamp % SECOND);
	date->year = (U16)(t.tm_year + 1900);
	date->month = (U8)(t.tm_mon + 1);
	date->day = (U8)t.tm_mday;
	date->hour = (U8)t.tm_hour;
	date->minute = (U8)t.tm_min;
	date->second = (U8)t.tm_sec;
	return true;
}
