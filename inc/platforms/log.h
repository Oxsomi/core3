#pragma once
#include "types/string.h"
#include "types/error.h"

struct LogArgs {
	u64 argc;
	struct String const *args;
};

enum LogLevel {
	LogLevel_Debug,
	LogLevel_Performance,
	LogLevel_Warn,
	LogLevel_Error,
	LogLevel_Fatal
};

enum LogOptions {
	LogOptions_None			= 0,
	LogOptions_Timestamp	= 1 << 0,
	LogOptions_NewLine		= 1 << 1,
	LogOptions_Thread		= 1 << 2,
	LogOptions_Default		= LogOptions_Timestamp | LogOptions_NewLine | LogOptions_Thread
};

impl void Log_captureStackTrace(void **stackTrace, u64 stackSize, u64 skip);

impl void Log_printCapturedStackTrace(const StackTrace stackTrace, enum LogLevel lvl);
impl void Log_log(enum LogLevel lvl, enum LogOptions options, struct LogArgs args);

void Log_printStackTrace(u64 skip, enum LogLevel lvl);

void Log_debug(struct String s, enum LogOptions options);
void Log_performance(struct String s, enum LogOptions options);
void Log_warn(struct String s, enum LogOptions options);
void Log_error(struct String s, enum LogOptions options);
void Log_fatal(struct String s, enum LogOptions options);

void Log_num(LongString result, u64 v, u64 base, const c8 prepend[2]);
void Log_num64(LongString result, u64 v);		//0n[0-9a-zA-Z_$]+ aka Nytodecimal. $ is chosen because it's ASCII and allowed in var definition
void Log_num16(LongString result, u64 v);		//0x[0-9a-f]+
void Log_num10(LongString result, u64 v);		//[0-9]+
void Log_num8(LongString result, u64 v);		//[0-7]+
void Log_num2(LongString result, u64 v);		//[0-1]+
