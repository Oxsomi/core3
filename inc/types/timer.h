#pragma once
#include "string.h"

typedef ShortString TimerFormat;

enum EFormatStatus {
	FormatStatus_Success,
	FormatStatus_Overflow,
	FormatStatus_InvalidValue,
	FormatStatus_InvalidFormat,
	FormatStatus_InvalidInput,
	FormatStatus_InvalidTime
};

Ns Timer_now();
DNs Timer_elapsed(Ns prev);

DNs Timer_dns(Ns timeStamp0, Ns timeStamp1);
F32 Timer_dt(Ns timeStamp0, Ns timeStamp1);

U64 Timer_clocks();
I64 Timer_clocksElapsed(U64 prevClocks);

void Timer_format(Ns time, TimerFormat timer);
enum EFormatStatus Timer_parseFormat(Ns *time, TimerFormat format);
