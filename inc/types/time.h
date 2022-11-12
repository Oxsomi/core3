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
F32 Time_dt(Ns timeStamp0, Ns timeStamp1);

U64 Time_clocks();
I64 Time_clocksElapsed(U64 prevClocks);

void Time_format(Ns time, TimerFormat timer);
EFormatStatus Time_parseFormat(Ns *time, TimerFormat format);
