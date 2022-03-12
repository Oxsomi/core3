#pragma once
#include "types.h"

typedef c8 TimerFormat[32];

enum FormatStatus {
	FormatStatus_Success,
	FormatStatus_Overflow,
	FormatStatus_InvalidValue,
	FormatStatus_InvalidFormat,
	FormatStatus_InvalidInput,
	FormatStatus_InvalidTime
};

ns Timer_now();
dns Timer_elapsed(ns prev);

dns Timer_dns(ns timeStamp0, ns timeStamp1);
f64 Timer_dt(ns timeStamp0, ns timeStamp1);

u64 Timer_clocks();
i64 Timer_clocksElapsed(u64 prevClocks);

void Timer_format(ns time, TimerFormat timer);
enum FormatStatus Timer_parseFormat(ns* time, TimerFormat format);