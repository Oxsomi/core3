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

//types/container/log.h

#pragma once
#include "types/base/error.h"
#include <inttypes.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct Allocator Allocator;

typedef enum ELogLevel {
	ELogLevel_Debug,
	ELogLevel_Performance,
	ELogLevel_Warn,
	ELogLevel_Error,
	ELogLevel_Count
} ELogLevel;

typedef enum ELogOptions {

	ELogOptions_None        = 0,

	ELogOptions_Timestamp    = 1 << 0,
	ELogOptions_NewLine        = 1 << 1,
	ELogOptions_Thread        = 1 << 2,
	ELogOptions_NoBreak        = 1 << 3,        //No debug break

	ELogOptions_Default        = ELogOptions_Timestamp | ELogOptions_NewLine | ELogOptions_Thread

} ELogOptions;

impl void Log_printCapturedStackTraceCustom(
	const Allocator *alloc,
	const void **stackTrace,
	U64 stackSize,
	ELogLevel lvl,
	ELogOptions options
);

//TODO: Proper logging system where you put them on a message queue unformatted so it doesn't impact perf at record time.

impl void Log_log(const Allocator *alloc, ELogLevel lvl, ELogOptions options, const CharString *arg);

void Log_printCapturedStackTrace(const Allocator *alloc, const StackTrace stackTrace, ELogLevel lvl, ELogOptions options);
void Log_printStackTrace(const Allocator *alloc, U8 skip, ELogLevel lvl, ELogOptions options);

//IMPORTANT:
//NEVER! Supply user generated content into format.
//Instead, use "%.*s".
//When displaying strings, use "%.*s", (int) args.length, arg.ptr instead of args.ptr, because strings aren't null terminated.
//(Only exception is if the strings are safely generated from code and are determined to be null terminated, then use %s)

void Log_logFormat(const Allocator *alloc, ELogLevel lvl, ELogOptions options, const C8 *format, ...);
void Log_debug(const Allocator *alloc, ELogOptions options, const C8 *format, ...);
void Log_performance(const Allocator *alloc, ELogOptions options, const C8 *format, ...);
void Log_warn(const Allocator *alloc, ELogOptions options, const C8 *format, ...);
void Log_error(const Allocator *alloc, ELogOptions options, const C8 *format, ...);

void Error_print(const Allocator *alloc, const Error *e_rr, ELogLevel logLevel, ELogOptions options);

#define Log_logFormatLn(alloc, lvl, ...)    Log_logFormat(alloc, lvl, ELogOptions_NewLine, __VA_ARGS__)
#define Log_debugLn(alloc, ...)                Log_debug(alloc, ELogOptions_NewLine, __VA_ARGS__)
#define Log_performanceLn(alloc, ...)        Log_performance(alloc, ELogOptions_NewLine, __VA_ARGS__)
#define Log_warnLn(alloc, ...)                Log_warn(alloc, ELogOptions_NewLine, __VA_ARGS__)
#define Log_errorLn(alloc, ...)                Log_error(alloc, ELogOptions_NewLine, __VA_ARGS__)

#ifdef __cplusplus
	}
#endif
