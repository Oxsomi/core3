/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//types/container/log.c

#include "types/container/log.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/lock.h"

#include <stdlib.h>
#include <string.h>

//Reserved memory for error printing.
//A small static pool so logging (and thus error reporting) still works when the normal allocator is failing
// (e.g. the process is out of memory); the pool is reset before each fallback print.
//32KiB fits a long message, its UTF16 conversion for the console and a symbolicated stack trace.

static U8 Log_reservedMem[32768];
static U64 Log_reservedOff = 0;
static SpinLock Log_reservedLock = { 0 };

static Bool Log_reservedAlloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	(void)allocator;

	if(!output)
		retError(clean, Error_nullPointer(2, "Log_reservedAlloc()::output is required"));

	if(!length)
		retError(clean, Error_invalidParameter(1, 0, "Log_reservedAlloc()::length is required"));

	//16-byte align every block so SIMD paths behave the same as with the real allocators

	const U64 aligned = (length + 15) &~ (U64)15;

	if(aligned < length || Log_reservedOff + aligned > sizeof(Log_reservedMem))
		retError(clean, Error_outOfMemory(0, "Log_reservedAlloc() reserved pool exhausted"));

	*output = Buffer_createManagedPtr(Log_reservedMem + Log_reservedOff, length);
	Log_reservedOff += aligned;

clean:
	return s_uccess;
}

static void Log_reservedFree(void *allocator, Buffer buf) {

	(void)allocator;

	//LIFO reclaim keeps alloc/free ping-pong (e.g. one string per stack trace line) from exhausting the pool.
	//Out of order frees are simply left for the reset before the next fallback print.

	const U64 aligned = (Buffer_length(buf) + 15) &~ (U64)15;

	if(buf.ptr + aligned == Log_reservedMem + Log_reservedOff)
		Log_reservedOff -= aligned;
}

static const Allocator Log_reservedAllocator = {
	.ptr = NULL,
	.alloc = Log_reservedAlloc,
	.free = Log_reservedFree
};

void Log_printCapturedStackTrace(const Allocator *alloc, const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom(alloc, (const void**) stackTrace, STACKTRACE_SIZE, lvl, options);
}

void Log_printStackTrace(const Allocator *alloc, U8 skip, ELogLevel lvl, ELogOptions options) {

	StackTrace stackTrace;
	Error_captureStackTrace(stackTrace, STACKTRACE_SIZE, skip);

	Log_printCapturedStackTrace(alloc, stackTrace, lvl, options);
}

#define Log_level(lvl)                                                             \
																				\
	if(!format)                                                                    \
		return;                                                                    \
																				\
	CharString res = CharString_createNull();                                    \
																				\
	va_list arg1;                                                                \
	va_start(arg1, format);                                                        \
	Bool s_uccess = CharString_formatVariadic(alloc, &res, NULL, format, arg1);    \
	va_end(arg1);                                                                \
																				\
	if(s_uccess)                                                                \
		Log_log(alloc, lvl, opt, &res);                                            \
																				\
	/*The allocator is failing (e.g. OOM); retry from the reserved pool so the message still prints*/    \
	else {                                                                        \
																				\
		const ELockAcquire acq = SpinLock_lock(&Log_reservedLock, U64_MAX);        \
																				\
		Log_reservedOff = 0;                                                    \
		CharString_free(&res, alloc);        /*In case the failed attempt left a partial string*/    \
																				\
		va_start(arg1, format);                                                    \
		if(CharString_formatVariadic(&Log_reservedAllocator, &res, NULL, format, arg1))    \
			Log_log(&Log_reservedAllocator, lvl, opt, &res);                    \
		va_end(arg1);                                                            \
																				\
		res = CharString_createNull();        /*Pool memory; reclaimed by the next reset, so nothing to free*/    \
																				\
		if(acq == ELockAcquire_Acquired)                                        \
			SpinLock_unlock(&Log_reservedLock);                                    \
	}                                                                            \
																				\
	CharString_free(&res, alloc);

void Log_logFormat(const Allocator *alloc, ELogLevel level, ELogOptions opt, const C8 *format, ...) {
	Log_level(level);
}

void Log_debug(const Allocator *alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Debug);
}

void Log_performance(const Allocator *alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Performance);
}

void Log_warn(const Allocator *alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Warn);
}

void Log_error(const Allocator *alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Error);
}

impl CharString Error_formatPlatformError(const Allocator *alloc, const Error *e_rr);

static Bool Error_printFormat(const Allocator *alloc, const Error *e_rr, CharString platformErr, CharString *result) {

	//TODO: Replace this with something less annoying.

	return CharString_format(
		alloc, result,
		NULL,
		"%s (%s)\nsub id: %"PRIu32"X, param id: %"PRIu32", param0: %08X, param1: %08X.\nPlatform/std error: %.*s.",
		e_rr->errorStr,
		EGenericError_TO_STRING[e_rr->genericError],
		e_rr->errorSubId,
		e_rr->paramId,
		e_rr->paramValue0,
		e_rr->paramValue1,
		CharString_length(platformErr), platformErr.ptr
	);
}

void Error_print(const Allocator *alloc, const Error *e_rr, ELogLevel logLevel, ELogOptions options) {

	if(!e_rr || !e_rr->genericError)
		return;

	CharString result = CharString_createNull();
	CharString platformErr = CharString_createNull();

	Bool usedReserved = false;
	ELockAcquire acq = ELockAcquire_Invalid;

	if (e_rr->genericError == EGenericError_PlatformError)
		platformErr = Error_formatPlatformError(alloc, e_rr);

	if(e_rr->genericError == EGenericError_Stderr)
		platformErr = CharString_createRefCStrConst(strerror((int)e_rr->paramValue0));

	if(Error_printFormat(alloc, e_rr, platformErr, &result))
		Log_log(alloc, logLevel, options | ELogOptions_NoBreak, &result);

	//The allocator is failing (e.g. OOM); retry from the reserved pool so the error still prints.
	//The stack trace below then also prints from the pool, since the real allocator can't service it either.
	else {

		usedReserved = true;
		acq = SpinLock_lock(&Log_reservedLock, U64_MAX);

		Log_reservedOff = 0;
		CharString_free(&result, alloc);        //In case the failed attempt left a partial string

		if(Error_printFormat(&Log_reservedAllocator, e_rr, platformErr, &result))
			Log_log(&Log_reservedAllocator, logLevel, options | ELogOptions_NoBreak, &result);
	}

	Log_printCapturedStackTraceCustom(
		usedReserved ? &Log_reservedAllocator : alloc,
		(const void**)e_rr->stackTrace, ERROR_STACKTRACE, ELogLevel_Error, options
	);

	if (usedReserved) {

		result = CharString_createNull();        //Pool memory; reclaimed by the next reset, so nothing to free

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&Log_reservedLock);
	}

	CharString_free(&result, alloc);
	CharString_free(&platformErr, alloc);
}
