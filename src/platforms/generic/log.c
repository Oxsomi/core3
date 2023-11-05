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

#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "types/string.h"
#include "math/math.h"

#include <stdlib.h>

void Log_printStackTrace(Allocator alloc, U64 skip, ELogLevel lvl, ELogOptions options) {

	StackTrace stackTrace;
	Log_captureStackTrace(stackTrace, _STACKTRACE_SIZE, skip);

	Log_printCapturedStackTrace(alloc, stackTrace, lvl, options);
}

void Log_printStackTracex(U64 skip, ELogLevel lvl, ELogOptions options) {
	Log_printStackTrace(Platform_instance.alloc, skip + 1, lvl, options);
}

void Log_printCapturedStackTraceCustomx(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom(Platform_instance.alloc, stackTrace, stackSize, lvl, options);
}

void Log_logx(ELogLevel lvl, ELogOptions options, CharString arg) {
	Log_log(Platform_instance.alloc, lvl, options, arg);
}

#define Log_level(lvl) 													\
																		\
	if(!format)															\
		return;															\
																		\
	CharString res = CharString_createNull();							\
																		\
	va_list arg1;														\
	va_start(arg1, format);												\
	Error err = CharString_formatVariadic(alloc, &res, format, arg1);	\
	va_end(arg1);														\
																		\
	if(!err.genericError)												\
		Log_log(alloc, lvl, opt, res);									\
																		\
	CharString_free(&res, alloc);

void Log_debug(Allocator alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Debug);
}

void Log_performance(Allocator alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Performance);
}

void Log_warn(Allocator alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Warn);
}

void Log_error(Allocator alloc, ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Error);
}

void Log_fatal(Allocator alloc, ELogOptions opt, const C8 *format, ...) {

	if(!format)
		return;

	Log_printStackTrace(alloc, 1, ELogLevel_Fatal, opt);

	CharString res = CharString_createNull();

	va_list arg1;
	va_start(arg1, format);
	Error err = CharString_formatVariadic(alloc, &res, format, arg1);
	va_end(arg1);

	if(err.genericError)
		return;

	Log_log(alloc, ELogLevel_Fatal, opt, res);
	CharString_free(&res, alloc);

	exit(1);
}

//Default allocator. Sometimes they can't be safely used

void Log_debugx(ELogOptions opt, const C8 *format, ...) {
	Allocator alloc = Platform_instance.alloc;
	Log_level(ELogLevel_Debug);
}

void Log_performancex(ELogOptions opt, const C8 *format, ...) {
	Allocator alloc = Platform_instance.alloc;
	Log_level(ELogLevel_Performance);
}

void Log_warnx(ELogOptions opt, const C8 *format, ...) {
	Allocator alloc = Platform_instance.alloc;
	Log_level(ELogLevel_Warn);
}

void Log_errorx(ELogOptions opt, const C8 *format, ...) {
	Allocator alloc = Platform_instance.alloc;
	Log_level(ELogLevel_Error);
}

void Log_fatalx(ELogOptions opt, const C8 *format, ...) {

	if(!format)
		return;

	Log_printStackTracex(1, ELogLevel_Fatal, opt);

	CharString res = CharString_createNull();

	va_list arg1;
	va_start(arg1, format);
	Error err = CharString_formatVariadicx(&res, format, arg1);
	va_end(arg1);

	if(err.genericError)
		return;

	Log_logx(ELogLevel_Fatal, opt, res);
	CharString_freex(&res);

	exit(1);
}
