#pragma once
#include "types/string.h"
#include "types/error.h"

typedef struct LogArgs {
	U64 argc;
	struct String const *args;
} LogArgs;

typedef enum ELogLevel {
	ELogLevel_Debug,
	ELogLevel_Performance,
	ELogLevel_Warn,
	ELogLevel_Error,
	ELogLevel_Fatal,
	ELogLevel_Count
} ELogLevel;

typedef enum ELogOptions {
	ELogOptions_None			= 0,
	ELogOptions_Timestamp	= 1 << 0,
	ELogOptions_NewLine		= 1 << 1,
	ELogOptions_Thread		= 1 << 2,
	ELogOptions_Default		= ELogOptions_Timestamp | ELogOptions_NewLine | ELogOptions_Thread
} ELogOptions;

impl void Log_captureStackTrace(void **stackTrace, U64 stackSize, U64 skip);

impl void Log_printCapturedStackTraceCustom(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions options);
impl void Log_log(ELogLevel lvl, ELogOptions options, LogArgs args);

inline void Log_printCapturedStackTrace(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom((const void**) stackTrace, StackTrace_SIZE, lvl, options);
}

void Log_printStackTrace(U64 skip, ELogLevel lvl, ELogOptions options);

void Log_debug(String s, ELogOptions options);
void Log_performance(String s, ELogOptions options);
void Log_warn(String s, ELogOptions options);
void Log_error(String s, ELogOptions options);
void Log_fatal(String s, ELogOptions options);

void Log_num(LongString result, U64 v, U64 base, const C8 prepend[2]);
void Log_num64(LongString result, U64 v);		//0n[0-9a-zA-Z_$]+ aka Nytodecimal. $ is chosen because it's ASCII and allowed in var definition
void Log_num16(LongString result, U64 v);		//0x[0-9a-f]+
void Log_num10(LongString result, U64 v);		//[0-9]+
void Log_num8(LongString result, U64 v);		//[0-7]+
void Log_num2(LongString result, U64 v);		//[0-1]+
