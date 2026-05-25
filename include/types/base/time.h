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

//types/base/time.h

#pragma once
#include "types/base/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef ShortString TimeFormat;

Ns Time_now();
U64 Time_clocks();

//e.g. year: 1970, month: 1, day: 1, hour: 0, minute: 0, seconds: 0, ns: 0 = (Ns)0
typedef struct Date {
	U32 ns;
	U16 year;
	U8 month;
	U8 day;
	U8 hour;
	U8 minute;
	U8 second;
	U8 padding;
} Date;

Ns Time_date(const Date *date, Bool isLocalTime);
Bool Time_getDate(Ns timestamp, Date *date, Bool isLocalTime);

static inline DNs Time_dns(Ns timeStamp0, Ns timeStamp1) {
	return (DNs)(timeStamp1 - timeStamp0);
}

static inline F64 Time_dt(Ns timeStamp0, Ns timeStamp1) {
	return (F64)Time_dns(timeStamp0, timeStamp1) / SECOND;
}

static inline I64 Time_clocksElapsed(U64 prevClocks) { return Time_dns(prevClocks, Time_clocks()); }
static inline DNs Time_elapsed(Ns prev) { return Time_dns(prev, Time_now()); }

void Time_format(Ns time, TimeFormat timeString, Bool isLocalTime);
Bool Time_parseFormat(Ns *time, TimeFormat format, Bool isLocalTime);

#ifdef __cplusplus
	}
#endif
