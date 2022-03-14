#pragma once
#include "types/string.h"

#define STACKTRACE_MAX_SIZE 128
typedef void *StackTrace[STACKTRACE_MAX_SIZE];

struct LogArgs {
	usz argc;
	const c8* const *args;
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

void Log_captureStackTrace(StackTrace stackTrace, usz skip);

void Log_printCapturedStackTrace(const StackTrace stackTrace, enum LogLevel lvl);
void Log_printStackTrace(usz skip, enum LogLevel lvl);

void Log_log(enum LogLevel lvl, enum LogOptions options, struct LogArgs args);

void Log_debug(const c8 *ptr, enum LogOptions options);
void Log_performance(const c8 *ptr, enum LogOptions options);
void Log_warn(const c8 *ptr, enum LogOptions options);
void Log_error(const c8 *ptr, enum LogOptions options);
void Log_fatal(const c8 *ptr, enum LogOptions options);

void Log_num(ShortString result, usz v, usz base, const c8 prepend[2]);
void Log_num16(ShortString result, usz v);
void Log_num10(ShortString result, usz v);
void Log_num8(ShortString result, usz v);
void Log_num2(ShortString result, usz v);
