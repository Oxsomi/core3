#include "types/time.h"
#include "types/string.h"
#include "math/math.h"
#include <time.h>

#ifdef _WIN32
	#include <intrin.h>
	#define timegm _mkgmtime
#else
	#include <x86intrin.h>
#endif

Ns Time_now() {
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	return (Ns)ts.tv_sec * seconds + (Ns)ts.tv_nsec;
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

F32 Time_dt(Ns timeStamp0, Ns timeStamp1) {
	return (F32)Time_dns(timeStamp0, timeStamp1) / seconds;
}

U64 Time_clocks() { return __rdtsc(); }	//TODO: Arm?
I64 Time_clocksElapsed(U64 prevClocks) { return Time_dns(prevClocks, Time_clocks()); }
DNs Time_elapsed(Ns prev) { return Time_dns(prev, Time_now()); }

//ISO 8601 e.g. 2022-02-26T21:08:45.000000000Z
//The standard functions strp don't work properly cross platform

void setNum(TimerFormat format, I64 offset, U64 length, U64 v) {

	I64 off = (I64)(offset + length) - 1;

	while (v && off >= offset) {
		format[off] = '0' + v % 10;
		--off;
		v /= 10;
	}
}

const C8 formatStr[] = "0000-00-00T00:00:00.000000000Z";
const U8 offsets[] =	{ 0, 5, 8, 11, 14, 17, 20 };
const U8 sizes[] =		{ 4, 2, 2,  2,  2,  2, 9 };

void Time_format(Ns time, TimerFormat timeString) {

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

EFormatStatus Time_parseFormat(Ns *time, TimerFormat format) {

	if (!time)
		return EFormatStatus_InvalidInput;

	U64 length = String_calcStrLen(format, ShortString_LEN - 1);

	U64 curr = 0, currSep = 0, prevI = U64_MAX;

	U32 ns = 0;
	U16 year = 0;
	U8 month = 0, day = 0, hour = 0, minute = 0, second = 0;

	for (U64 i = 0; i < length; ++i) {

		C8 c = format[i];

		if (C8_isDec(c)) {

			if(curr == U64_MAX)
				return EFormatStatus_Overflow;

			curr *= 10;

			U64 prev = curr;
			curr += C8_dec(c);

			if(curr < prev)
				return EFormatStatus_Overflow;

			if (curr * 10 < curr)
				curr = U64_MAX;

			continue;
		}

		if (currSep == sizeof(separators) - 1 || c != separators[currSep])
			return EFormatStatus_InvalidFormat;

		if(currSep < 6) {

			if (curr > U16_MAX)
				return EFormatStatus_InvalidValue;

			if (currSep && curr > U8_MAX)			//Everything but year is 8-bit
				return EFormatStatus_InvalidValue;
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
				   return EFormatStatus_InvalidFormat;

				U64 mul = U64_pow10(9 - dif);

				if(mul == U64_MAX)
					return EFormatStatus_InvalidFormat;

				if (curr * mul >= seconds)
					return EFormatStatus_InvalidValue;

				ns = (U32)(curr * mul);
				break;
			}

			default:
				return EFormatStatus_InvalidFormat;
		}

		curr = 0;
		++currSep;
		prevI = i + 1;
	}

	Ns res = Time_date(year, month, day, hour, minute, second, ns);

	if (res == U64_MAX)
		return EFormatStatus_InvalidTime;

	*time = res;
	return EFormatStatus_Success;
}

Ns Time_date(U16 year, U8 month, U8 day, U8 hour, U8 minute, U8 second, U32 ns) {

	if(
		year < 1970 || 
		month < 1 || month > 12 || 
		day > 31 || 
		hour >= 24 || minute >= 60 || second >= 60 ||
		ns >= seconds
	)
		return U64_MAX;

	struct tm tm = { 
		.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day,
		.tm_hour = hour, .tm_min = minute, .tm_sec = second
	};

	time_t ts = timegm(&tm);

	if (ts == (time_t)-1)
		return U64_MAX;

	return (Ns)ts * seconds + ns;
}

Bool Time_getDate(Ns timestamp, U16 *year, U8 *month, U8 *day, U8 *hour, U8 *minute, U8 *second, U32 *ns) {

	time_t inSecs = (time_t)(timestamp / seconds);

	struct tm *t = gmtime(&inSecs);

	if(!t)
		return false;

	if(t->tm_year > 1900 + U16_MAX)
		return false;

	if(ns)		*ns = (U32)(timestamp % seconds);
	if(year)	*year = (U16)(t->tm_year + 1900);
	if(month)	*month = (U8)(t->tm_mon + 1);
	if(day)		*day = (U8)t->tm_mday;
	if(hour)	*hour = (U8)t->tm_hour;
	if(minute)	*minute = (U8)t->tm_min;
	if(second)	*second = (U8)t->tm_sec;

	return true;
}
