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

typedef struct LogArgs {
	U64 argc;
	String const *args;
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

	ELogOptions_None		= 0,

	ELogOptions_Timestamp	= 1 << 0,
	ELogOptions_NewLine		= 1 << 1,
	ELogOptions_Thread		= 1 << 2,

	ELogOptions_Default		= ELogOptions_Timestamp | ELogOptions_NewLine | ELogOptions_Thread

} ELogOptions;

impl void Log_captureStackTrace(void **stackTrace, U64 stackSize, U64 skip);

impl void Log_printCapturedStackTraceCustom(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions options);
impl void Log_log(ELogLevel lvl, ELogOptions options, LogArgs args);

void Log_printCapturedStackTrace(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options);

void Log_printStackTrace(U64 skip, ELogLevel lvl, ELogOptions options);

void Log_debug(String s, ELogOptions options);
void Log_performance(String s, ELogOptions options);
void Log_warn(String s, ELogOptions options);
void Log_error(String s, ELogOptions options);
void Log_fatal(String s, ELogOptions options);

void Log_num(LongString result, U64 v, U64 base, const C8 prepend[2]);
void Log_num64(LongString result, U64 v);		//0n[0-9a-zA-Z_$]+ aka Nytodecimal. $ is ASCII and allowed in var name
void Log_num16(LongString result, U64 v);		//0x[0-9a-f]+
void Log_num10(LongString result, U64 v);		//[0-9]+
void Log_num8(LongString result, U64 v);		//[0-7]+
void Log_num2(LongString result, U64 v);		//[0-1]+
