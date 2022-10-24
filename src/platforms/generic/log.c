#include "platforms/log.h"

#include <stdlib.h>

void Log_printStackTrace(U64 skip, enum LogLevel lvl) {

	StackTrace stackTrace;
	Log_captureStackTrace(stackTrace, StackTrace_SIZE, skip);

	Log_printCapturedStackTrace(stackTrace, lvl);
}

void Log_debug(struct String s, enum LogOptions options) {
	Log_log(LogLevel_Debug, options, (struct LogArgs){ .argc = 1, .args = &s });
}

void Log_performance(struct String s, enum LogOptions options) {
	Log_log(LogLevel_Performance, options, (struct LogArgs){ .argc = 1, .args = &s });
}

void Log_warn(struct String s, enum LogOptions options) {
	Log_log(LogLevel_Warn, options, (struct LogArgs){ .argc = 1, .args = &s });
}

void Log_error(struct String s, enum LogOptions options) {
	Log_log(LogLevel_Error, options, (struct LogArgs){ .argc = 1, .args = &s });
}

void Log_fatal(struct String s, enum LogOptions options) {
	Log_printStackTrace(1, LogLevel_Fatal);
	Log_log(LogLevel_Fatal, options, (struct LogArgs){ .argc = 1, .args = &s });
	exit(1);
}

const C8 nytoBase64[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$";

void Log_num(LongString result, U64 v, U64 base, const C8 prepend[2]) {

	if (base < 1 || base > 64) {
		result[0] = 0;
		return;
	}

	U64 i = 0;

	while (i < 2 && prepend[i]) {
		result[i] = prepend[i];
		++i;
	}

	if (!v) {
		result[i] = '0';
		result[i + 1] = '\0';
		return;
	}

	LongString tmp;
	U64 j = 0;

	while (v && j < 64 - i) {
		tmp[j++] = nytoBase64[v % base];
		v /= base;
	}

	U64 last = i + j - 1;

	for (i = 0; i < j; ++i)
		result[last - i] = tmp[i];
}

void Log_num64(LongString result, U64 v) {
	const C8 prepend[2] = { '0', 'n' }; 
	Log_num(result, v, 64, prepend); 
}

void Log_num16(LongString result, U64 v) { 
	const C8 prepend[2] = { '0', 'x' }; 
	Log_num(result, v, 16, prepend); 
}

void Log_num10(LongString result, U64 v) { 
	const C8 prepend[2] = { 0 }; 
	Log_num(result, v, 10, prepend); 
}

void Log_num8(LongString result, U64 v) { 
	const C8 prepend[2] = { '0', 'o' }; 
	Log_num(result, v, 8, prepend); 
}

void Log_num2(LongString result, U64 v)  { 
	const C8 prepend[2] = { '0', 'b' }; 
	Log_num(result, v, 2, prepend); 
}
