/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/error.h"
#include "types/string.h"

typedef enum ELogLevel {
	ELogLevel_Debug,
	ELogLevel_Performance,
	ELogLevel_Warn,
	ELogLevel_Error,
	ELogLevel_Fatal,
	ELogLevel_Count
} ELogLevel;

typedef enum ELogOptions {

	ELogOptions_None		= 0,

	ELogOptions_Timestamp	= 1 << 0,
	ELogOptions_NewLine		= 1 << 1,
	ELogOptions_Thread		= 1 << 2,

	ELogOptions_Default		= ELogOptions_Timestamp | ELogOptions_NewLine | ELogOptions_Thread

} ELogOptions;

impl void Log_captureStackTrace(void **stackTrace, U64 stackSize, U64 skip);

impl void Log_printCapturedStackTraceCustom(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions options);
impl void Log_log(ELogLevel lvl, ELogOptions options, String arg);

void Log_printCapturedStackTrace(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options);

void Log_printStackTrace(U64 skip, ELogLevel lvl, ELogOptions options);

//IMPORTANT:
//NEVER! Supply user generated content into format. Instead use "%.*s".
//When displaying strings, use "%.*s", args.length, arg.ptr instead of args.ptr, because strings aren't null terminated.
//(Only exception is if the strings are safely generated from code and are determined to be null terminated, then use %s)

void Log_debug(ELogOptions options, const C8 *format, ...);
void Log_performance(ELogOptions options, const C8 *format, ...);
void Log_warn(ELogOptions options, const C8 *format, ...);
void Log_error(ELogOptions options, const C8 *format, ...);
void Log_fatal(ELogOptions options, const C8 *format, ...);

#define Log_debugLn(...)		Log_debug(ELogOptions_NewLine, __VA_ARGS__)
#define Log_performanceLn(...)	Log_performance(ELogOptions_NewLine, __VA_ARGS__)
#define Log_warnLn(...)			Log_warn(ELogOptions_NewLine, __VA_ARGS__)
#define Log_errorLn(...)		Log_error(ELogOptions_NewLine, __VA_ARGS__)
#define Log_fatalLn(...)		Log_fatal(ELogOptions_NewLine, __VA_ARGS__)
