#include "platforms/log.h"
#include "types/assert.h"

#include <stdlib.h>

void Log_printStackTrace(usz skip, enum LogLevel lvl) {

	StackTrace stackTrace;
	Log_captureStackTrace(stackTrace, skip);

	Log_printCapturedStackTrace(stackTrace, lvl);
}

void Log_debug(const c8 *ptr, enum LogOptions options) {
	Log_log(LogLevel_Debug, options, (struct LogArgs){ .argc = 1, .args = &ptr });
}

void Log_performance(const c8 *ptr, enum LogOptions options) {
	Log_log(LogLevel_Performance, options, (struct LogArgs){ .argc = 1, .args = &ptr });
}

void Log_warn(const c8 *ptr, enum LogOptions options) {
	Log_log(LogLevel_Warn, options, (struct LogArgs){ .argc = 1, .args = &ptr });
}

void Log_error(const c8 *ptr, enum LogOptions options) {
	Log_log(LogLevel_Error, options, (struct LogArgs){ .argc = 1, .args = &ptr });
}

void Log_fatal(const c8 *ptr, enum LogOptions options) {
	Log_printStackTrace(1, LogLevel_Fatal);
	Log_log(LogLevel_Fatal, options, (struct LogArgs){ .argc = 1, .args = &ptr });
	exit(1);
}

const c8 naiveBase64[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-";

void Log_num(ShortString result, usz v, usz base, const c8 prepend[2]) {

	if (base < 1 || base > 64) {
		ocAssert("Log_num requires base of >1 and <64", false);
		result[0] = 0;
		return;
	}

	usz i = 0;

	while (i < 2 && prepend[i]) {
		result[i] = prepend[i];
		++i;
	}

	if (!v) {
		result[i] = '0';
		result[i] = 0;
		return;
	}

	ShortString tmp;
	usz j = 0;

	while (v && j < 64 - i) {
		tmp[j++] = naiveBase64[v % base];
		v /= base;
	}

	usz last = i + j - 1;

	for (i = 0; i < j; ++i)
		result[last - i] = tmp[i];
}

void Log_num16(ShortString result, usz v) { 
	const c8 prepend[2] = { '0', 'x' }; 
	Log_num(result, v, 10, prepend); 
}

void Log_num10(ShortString result, usz v) { 
	const c8 prepend[2] = { 0 }; 
	Log_num(result, v, 16, prepend); 
}

void Log_num8(ShortString result, usz v) { 
	const c8 prepend[2] = { '0', 'o' }; 
	Log_num(result, v, 8, prepend); 
}

void Log_num2(ShortString result, usz v)  { 
	const c8 prepend[2] = { '0', 'b' }; 
	Log_num(result, v, 2, prepend); 
}
