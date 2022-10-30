#include "types/timer.h"
#include "types/string.h"
#include "math/math.h"
#include <time.h>

#ifdef _WIN32
	#include <intrin.h>
	#define timegm _mkgmtime
#else
	#include <x86intrin.h>
#endif

Ns Timer_now() {
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	return (Ns)ts.tv_sec * seconds + (Ns)ts.tv_nsec;
}

DNs Timer_dns(Ns timeStamp0, Ns timeStamp1) {

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

F32 Timer_dt(Ns timeStamp0, Ns timeStamp1) {
	return (F32)Timer_dns(timeStamp0, timeStamp1) / seconds;
}

U64 Timer_clocks() { return __rdtsc(); }	//TODO: Arm?
I64 Timer_clocksElapsed(U64 prevClocks) { return Timer_dns(prevClocks, Timer_clocks()); }
DNs Timer_elapsed(Ns prev) { return Timer_dns(prev, Timer_now()); }

//ISO 8601 e.g. 2022-02-26T21:08:45.000000000Z
//The standard functions strp don't work properly cross platform

void setNum(TimerFormat format, I64 offset, U64 siz, U64 v) {

	I64 off = (I64)(offset + siz) - 1;

	while (v && off >= offset) {
		format[off] = '0' + v % 10;
		--off;
		v /= 10;
	}
}

const C8 formatStr[] = "0000-00-00T00:00:00.000000000Z";
const U8 offsets[] =	{ 0, 5, 8, 11, 14, 17, 20 };
const U8 sizes[] =		{ 4, 2, 2,  2,  2,  2, 9 };

void Timer_format(Ns time, TimerFormat timeString) {

	time_t inSecs = (time_t)(time / seconds);
	Ns inNs = time % seconds;

	struct tm *t = gmtime(&inSecs);

	Buffer_copy(
		Buffer_createRef(timeString, ShortString_LEN), 
		Buffer_createRef((void*) formatStr, sizeof(formatStr))
	);

	setNum(timeString, offsets[0], sizes[0], (U64)t->tm_year + 1900);
	setNum(timeString, offsets[1], sizes[1], (U64)t->tm_mon + 1);
	setNum(timeString, offsets[2], sizes[2], (U64)t->tm_mday);
	setNum(timeString, offsets[3], sizes[3], (U64)t->tm_hour);
	setNum(timeString, offsets[4], sizes[4], (U64)t->tm_min);
	setNum(timeString, offsets[5], sizes[5], (U64)t->tm_sec);
	setNum(timeString, offsets[6], sizes[6], inNs);
}

const C8 separators[] = "--T::.Z";

EFormatStatus Timer_parseFormat(Ns *time, TimerFormat format) {

	if (!time)
		return FormatStatus_InvalidInput;

	U64 len = String_calcStrLen(format, ShortString_LEN - 1);

	struct tm tm = { 0 };
	U64 curr = 0, currSep = 0, prevI = U64_MAX;

	U64 nsAdd = 0;

	for (U64 i = 0; i < len; ++i) {

		C8 c = format[i];

		if (C8_isDec(c)) {

			if(curr == U64_MAX)
				return FormatStatus_Overflow;

			curr *= 10;

			U64 prev = curr;
			curr += C8_dec(c);

			if(curr < prev)
				return FormatStatus_Overflow;

			if (curr * 10 < curr)
				curr = U64_MAX;

			continue;
		}

		if (currSep == sizeof(separators) - 1 || c != separators[currSep])
			return FormatStatus_InvalidFormat;

		switch (currSep) {

			case 0:		//Year

				if (curr < 1970 || (curr - 1970) > I32_MAX)
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

				U64 mul = U64_pow10(9 - dif);

				if(mul == U64_MAX)
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

	*time = (Ns)ts * seconds + nsAdd;
	return FormatStatus_Success;
}
