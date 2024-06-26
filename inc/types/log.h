/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
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

	ELogOptions_None		= 0,

	ELogOptions_Timestamp	= 1 << 0,
	ELogOptions_NewLine		= 1 << 1,
	ELogOptions_Thread		= 1 << 2,

	ELogOptions_Default		= ELogOptions_Timestamp | ELogOptions_NewLine | ELogOptions_Thread

} ELogOptions;

impl void Log_captureStackTrace(Allocator alloc, void **stackTrace, U64 stackSize, U8 skip);

impl void Log_printCapturedStackTraceCustom(
	Allocator alloc, const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions options
);

impl void Log_log(Allocator alloc, ELogLevel lvl, ELogOptions options, CharString arg);

void Log_printCapturedStackTrace(Allocator alloc, const StackTrace stackTrace, ELogLevel lvl, ELogOptions options);
void Log_printStackTrace(Allocator alloc, U8 skip, ELogLevel lvl, ELogOptions options);

//IMPORTANT:
//NEVER! Supply user generated content into format. Instead, use "%.*s".
//When displaying strings, use "%.*s", (int) args.length, arg.ptr instead of args.ptr, because strings aren't null terminated.
//(Only exception is if the strings are safely generated from code and are determined to be null terminated, then use %s)

void Log_debug(Allocator alloc, ELogOptions options, const C8 *format, ...);
void Log_performance(Allocator alloc, ELogOptions options, const C8 *format, ...);
void Log_warn(Allocator alloc, ELogOptions options, const C8 *format, ...);
void Log_error(Allocator alloc, ELogOptions options, const C8 *format, ...);

#define Log_debugLn(alloc, ...)			Log_debug(alloc, ELogOptions_NewLine, __VA_ARGS__)
#define Log_performanceLn(alloc, ...)	Log_performance(alloc, ELogOptions_NewLine, __VA_ARGS__)
#define Log_warnLn(alloc, ...)			Log_warn(alloc, ELogOptions_NewLine, __VA_ARGS__)
#define Log_errorLn(alloc, ...)			Log_error(alloc, ELogOptions_NewLine, __VA_ARGS__)

#ifdef __cplusplus
	}
#endif
