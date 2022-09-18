#include "types/timer.h"
#include "types/string.h"
#include "math/math.h"
#include <time.h>
#include <assert.h>
#include <string.h>

#ifdef _WIN32
	#include <intrin.h>
	#define timegm _mkgmtime
#else
	#include <x86intrin.h>
#endif

ns Timer_now() {
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	return (ns)ts.tv_sec * seconds + (ns)ts.tv_nsec;
}

dns Timer_dns(ns timeStamp0, ns timeStamp1) {

	if (timeStamp0 > timeStamp1) {

		ns diff = timeStamp0 - timeStamp1;

		if (diff >> 63)
			return i64_MAX;

		return -(dns) diff;
	}

	ns diff = timeStamp1 - timeStamp0;

	if (diff >> 63)
		return i64_MAX;

	return (dns) diff;
}

f64 Timer_dt(ns timeStamp0, ns timeStamp1) {
	return (f64)Timer_dns(timeStamp0, timeStamp1) / seconds;
}

u64 Timer_clocks() { return __rdtsc(); }	//TODO: Arm?
i64 Timer_clocksElapsed(u64 prevClocks) { return Timer_dns(prevClocks, Timer_clocks()); }
dns Timer_elapsed(ns prev) { return Timer_dns(prev, Timer_now()); }

//ISO 8601 e.g. 2022-02-26T21:08:45.000000000Z
//The standard functions strp don't work properly cross platform

void setNum(TimerFormat format, u64 offset, u64 siz, u64 v) {

	i64 off = (i64)(offset + siz) - 1;

	while (v && off >= offset) {
		format[off] = '0' + v % 10;
		--off;
		v /= 10;
	}
}

const c8 formatStr[] = "0000-00-00T00:00:00.000000000Z";
const u8 offsets[] =	{ 0, 5, 8, 11, 14, 17, 20 };
const u8 sizes[] =		{ 4, 2, 2,  2,  2,  2, 9 };

void Timer_format(ns time, TimerFormat timeString) {

	time_t inSecs = (time_t)(time / seconds);
	ns inNs = time % seconds;

	struct tm *t = gmtime(&inSecs);

	memcpy(timeString, formatStr, sizeof(formatStr));

	setNum(timeString, offsets[0], sizes[0], (u64)t->tm_year + 1900);
	setNum(timeString, offsets[1], sizes[1], (u64)t->tm_mon + 1);
	setNum(timeString, offsets[2], sizes[2], (u64)t->tm_mday);
	setNum(timeString, offsets[3], sizes[3], (u64)t->tm_hour);
	setNum(timeString, offsets[4], sizes[4], (u64)t->tm_min);
	setNum(timeString, offsets[5], sizes[5], (u64)t->tm_sec);
	setNum(timeString, offsets[6], sizes[6], inNs);
}

const c8 separators[] = "--T::.Z";

enum EFormatStatus Timer_parseFormat(ns *time, TimerFormat format) {

	if (!time)
		return FormatStatus_InvalidInput;

	u64 len = String_calcStrLen(format, 32);

	struct tm tm = { 0 };
	u64 curr = 0, currSep = 0, prevI = u64_MAX;

	u64 nsAdd = 0;

	for (u64 i = 0; i < len; ++i) {

		c8 c = format[i];

		if (c >= '0' && c <= '9') {

			if(curr == u64_MAX)
				return FormatStatus_Overflow;

			curr *= 10;

			u64 prev = curr;
			curr += (c - '0');

			if(curr < prev)
				return FormatStatus_Overflow;

			if (curr * 10 < curr)
				curr = u64_MAX;

			continue;
		}

		if (currSep == sizeof(separators) - 1 || c != separators[currSep])
			return FormatStatus_InvalidFormat;

		switch (currSep) {

			case 0:		//Year

				if (curr < 1970 || (curr - 1970) > i32_MAX)
					return FormatStatus_InvalidValue;

				tm.tm_year = (int)(curr - 1900);
				break;

			case 1:		//Month

				if (curr < 1 || curr > 12)
					return FormatStatus_InvalidValue;

				tm.tm_mon = (int)curr - 1;
				break;

			case 2:		//Day

				if (curr < 1 || curr > 31)
					return FormatStatus_InvalidValue;

				tm.tm_mday = (int)curr;
				break;

			case 3:		//Hour

				if (curr >= 24)
					return FormatStatus_InvalidValue;

				tm.tm_hour = (int)curr;
				break;

			case 4:		//Minute

				if (curr >= hours / mins)
					return FormatStatus_InvalidValue;

				tm.tm_min = (int)curr;
				break;

			case 5:		//Seconds

				if (curr >= mins / seconds)
					return FormatStatus_InvalidValue;

				tm.tm_sec = (int)curr;
				break;

			case 6: {	//Nanoseconds

				if (curr >= seconds)
					return FormatStatus_InvalidValue;

				int dif = (int)(i - prevI);

				if(dif == 0)
				   return FormatStatus_InvalidFormat;

				u64 mul = u64_pow10(9 - dif);

				if(mul == u64_MAX)
					return FormatStatus_InvalidFormat;

				nsAdd = curr * mul;
				break;
			}

			default:
				return FormatStatus_InvalidFormat;
		}

		curr = 0;
		++currSep;
		prevI = i + 1;
	}

	time_t ts = timegm(&tm);

	if (ts == (time_t)-1)
		return FormatStatus_InvalidTime;

	*time = ts * seconds + nsAdd;
	return FormatStatus_Success;
}
