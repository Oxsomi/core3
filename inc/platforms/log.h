/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
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
