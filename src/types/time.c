/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/time.h"
#include "types/string.h"
#include "types/buffer.h"
#include "types/math.h"
#include <time.h>

#ifdef _WIN32
	#include <intrin.h>
	#define timegm _mkgmtime
#else
	#include <x86intrin.h>
#endif

Ns Time_now() {

	struct timespec ts;

	if(!timespec_get(&ts, TIME_UTC))
		return (Ns) U64_MAX;

	return (Ns)ts.tv_sec * SECOND + (Ns)ts.tv_nsec;
}

DNs Time_dns(Ns timeStamp0, Ns timeStamp1) {

	if (timeStamp0 > timeStamp1) {

		Ns diff = timeStamp0 - timeStamp1;

		if (diff >> 63)
			return I64_MAX;

		return -(DNs) diff;
	}

	Ns diff = timeStamp1 - timeStamp0;

	if (diff >> 63)
		return I64_MAX;

	return (DNs) diff;
}

F64 Time_dt(Ns timeStamp0, Ns timeStamp1) {
	return (F64)Time_dns(timeStamp0, timeStamp1) / SECOND;
}

U64 Time_clocks() { return __rdtsc(); }	//TODO: Arm?
I64 Time_clocksElapsed(U64 prevClocks) { return Time_dns(prevClocks, Time_clocks()); }
DNs Time_elapsed(Ns prev) { return Time_dns(prev, Time_now()); }

//ISO 8601 e.g. 2022-02-26T21:08:45.000000000Z
//The standard functions strp don't work properly cross platform.
//This is not done via our CharString functions because format is called at important moments.
//At these moments there might not be any space left on the heap to allocate or there might be corruption there,
//as such, using string would be problematic. (This also includes error handling with logging and signals such as segfault).
//Our stack is less likely to be corrupted, if it is then we can't properly handle it.

void setNum(TimeFormat format, I64 offset, U64 length, U64 v) {

	I64 off = (I64)(offset + length) - 1;

	while (v && off >= offset) {
		format[off] = '0' + v % 10;
		--off;
		v /= 10;
	}
}

const C8 FORMAT_STR[] = "0000-00-00T00:00:00.000000000Z";
const U8 OFFSETS[] =	{ 0, 5, 8, 11, 14, 17, 20 };
const U8 SIZES[] =		{ 4, 2, 2,  2,  2,  2, 9 };

void Time_format(Ns time, TimeFormat timeString, Bool isLocalTime) {

	time_t inSecs = (time_t)(time / SECOND);
	Ns inNs = time % SECOND;

	struct tm *t = isLocalTime ? localtime(&inSecs) : gmtime(&inSecs);

	Buffer_copy(
		Buffer_createRef(timeString, _SHORTSTRING_LEN), 
		Buffer_createRefConst(FORMAT_STR, sizeof(FORMAT_STR))
	);

	if(!t)
		return;

	setNum(timeString, OFFSETS[0], SIZES[0], (U64)t->tm_year + 1900);
	setNum(timeString, OFFSETS[1], SIZES[1], (U64)t->tm_mon + 1);
	setNum(timeString, OFFSETS[2], SIZES[2], (U64)t->tm_mday);
	setNum(timeString, OFFSETS[3], SIZES[3], (U64)t->tm_hour);
	setNum(timeString, OFFSETS[4], SIZES[4], (U64)t->tm_min);
	setNum(timeString, OFFSETS[5], SIZES[5], (U64)t->tm_sec);
	setNum(timeString, OFFSETS[6], SIZES[6], inNs);
}

const C8 separators[] = "--T::.Z";

Bool Time_parseFormat(Ns *time, TimeFormat format, Bool isLocalTime) {

	if (!time)
		return false;

	U64 length = CharString_calcStrLen(format, _SHORTSTRING_LEN - 1);

	U64 curr = 0, currSep = 0, prevI = U64_MAX;

	U32 ns = 0;
	U16 year = 0;
	U8 month = 0, day = 0, hour = 0, minute = 0, second = 0;

	for (U64 i = 0; i < length; ++i) {

		C8 c = format[i];

		if (C8_isDec(c)) {

			if(curr == U64_MAX)
				return false;

			curr *= 10;

			U64 prev = curr;
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

			if (currSep && curr > U8_MAX)			//Everything but year is 8-bit
				return false;
		}

		switch (currSep) {

			case 0:		year = (U16) curr;		break;
			case 1:		month = (U8) curr;		break;
			case 2:		day = (U8) curr;		break;
			case 3:		hour = (U8) curr;		break;
			case 4:		minute = (U8) curr;		break;
			case 5:		second = (U8) curr;		break;

			case 6: {	//Nanoseconds

				int dif = (int)(i - prevI);

				if(dif == 0)
					return false;

				U64 mul = U64_exp10(9 - dif);

				if(mul == U64_MAX)
					return false;

				if (curr * mul >= SECOND)
					return false;

				ns = (U32)(curr * mul);
				break;
			}

			default:
				return false;
		}

		curr = 0;
		++currSep;
		prevI = i + 1;
	}

	Ns res = Time_date(year, month, day, hour, minute, second, ns, isLocalTime);

	if (res == U64_MAX)
		return false;

	*time = res;
	return true;
}

Ns Time_date(U16 year, U8 month, U8 day, U8 hour, U8 minute, U8 second, U32 ns, Bool isLocalTime) {

	if(
		year < 1970 || 
		month < 1 || month > 12 || 
		day > 31 || 
		hour >= 24 || minute >= 60 || second >= 60 ||
		ns >= SECOND
	)
		return U64_MAX;

	struct tm tm = { 
		.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day,
		.tm_hour = hour, .tm_min = minute, .tm_sec = second
	};

	time_t ts = isLocalTime ? mktime(&tm) : timegm(&tm);

	if (ts == (time_t)-1 || (U64)ts * SECOND + ns < (U64)ts)
		return U64_MAX;

	return (Ns)ts * SECOND + ns;
}

Bool Time_getDate(Ns timestamp, U16 *year, U8 *month, U8 *day, U8 *hour, U8 *minute, U8 *second, U32 *ns, Bool isLocalTime) {

	time_t inSecs = (time_t)(timestamp / SECOND);

	struct tm *t = isLocalTime ? localtime(&inSecs) : gmtime(&inSecs);

	if(!t)
		return false;

	if(t->tm_year > 1900 + U16_MAX)
		return false;

	if(ns)		*ns = (U32)(timestamp % SECOND);
	if(year)	*year = (U16)(t->tm_year + 1900);
	if(month)	*month = (U8)(t->tm_mon + 1);
	if(day)		*day = (U8)t->tm_mday;
	if(hour)	*hour = (U8)t->tm_hour;
	if(minute)	*minute = (U8)t->tm_min;
	if(second)	*second = (U8)t->tm_sec;

	return true;
}
