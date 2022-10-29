#pragma once
#include "types/string.h"
#include "types/error.h"

struct LogArgs {
	U64 argc;
	struct String const *args;
};

enum LogLevel {
	LogLevel_Debug,
	LogLevel_Performance,
	LogLevel_Warn,
	LogLevel_Error,
	LogLevel_Fatal,
	LogLevel_Count
};

enum LogOptions {
	LogOptions_None			= 0,
	LogOptions_Timestamp	= 1 << 0,
	LogOptions_NewLine		= 1 << 1,
	LogOptions_Thread		= 1 << 2,
	LogOptions_Default		= LogOptions_Timestamp | LogOptions_NewLine | LogOptions_Thread
};

impl void Log_captureStackTrace(void **stackTrace, U64 stackSize, U64 skip);

impl void Log_printCapturedStackTraceCustom(const void **stackTrace, U64 stackSize, enum LogLevel lvl, enum LogOptions options);
impl void Log_log(enum LogLevel lvl, enum LogOptions options, struct LogArgs args);

inline void Log_printCapturedStackTrace(const StackTrace stackTrace, enum LogLevel lvl, enum LogOptions options) {
	Log_printCapturedStackTraceCustom((const void**) stackTrace, StackTrace_SIZE, lvl, options);
}

void Log_printStackTrace(U64 skip, enum LogLevel lvl, enum LogOptions options);

void Log_debug(struct String s, enum LogOptions options);
void Log_performance(struct String s, enum LogOptions options);
void Log_warn(struct String s, enum LogOptions options);
void Log_error(struct String s, enum LogOptions options);
void Log_fatal(struct String s, enum LogOptions options);

void Log_num(LongString result, U64 v, U64 base, const C8 prepend[2]);
void Log_num64(LongString result, U64 v);		//0n[0-9a-zA-Z_$]+ aka Nytodecimal. $ is chosen because it's ASCII and allowed in var definition
void Log_num16(LongString result, U64 v);		//0x[0-9a-f]+
void Log_num10(LongString result, U64 v);		//[0-9]+
void Log_num8(LongString result, U64 v);		//[0-7]+
void Log_num2(LongString result, U64 v);		//[0-1]+
