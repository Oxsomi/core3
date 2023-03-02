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

#pragma once
#include "string.h"

typedef ShortString TimerFormat;

typedef enum EFormatStatus {
	EFormatStatus_Success,
	EFormatStatus_Overflow,
	EFormatStatus_InvalidValue,
	EFormatStatus_InvalidFormat,
	EFormatStatus_InvalidInput,
	EFormatStatus_InvalidTime
} EFormatStatus;

Ns Time_now();
DNs Time_elapsed(Ns prev);

//e.g. year: 1970, month: 1, day: 1, hour: 0, minute: 0, seconds: 0, ns: 0 = (Ns)0

Ns Time_date(U16 year, U8 month, U8 day, U8 hour, U8 minute, U8 second, U32 ns);		//U64_MAX on invalid
Bool Time_getDate(Ns timestamp, U16 *year, U8 *month, U8 *day, U8 *hour, U8 *minute, U8 *second, U32 *ns);

DNs Time_dns(Ns timeStamp0, Ns timeStamp1);
F64 Time_dt(Ns timeStamp0, Ns timeStamp1);

U64 Time_clocks();
I64 Time_clocksElapsed(U64 prevClocks);

void Time_format(Ns time, TimerFormat timer);
EFormatStatus Time_parseFormat(Ns *time, TimerFormat format);
