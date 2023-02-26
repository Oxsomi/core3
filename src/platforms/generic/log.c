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

void Log_printStackTrace(U64 skip, ELogLevel lvl, ELogOptions options) {

	StackTrace stackTrace;
	Log_captureStackTrace(stackTrace, _STACKTRACE_SIZE, skip);

	Log_printCapturedStackTrace(stackTrace, lvl, options);
}

#define Log_level(lvl) 													\
																		\
	if(!format)															\
		return;															\
																		\
	String res = String_createNull();									\
																		\
	va_list arg1;														\
	va_start(arg1, format);												\
	Error err = String_formatVariadicx(&res, format, arg1);				\
	va_end(arg1);														\
																		\
	if(!err.genericError)												\
		Log_log(lvl, opt, res);											\
																		\
	String_freex(&res);

void Log_debug(ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Debug);
}

void Log_performance(ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Performance);
}

void Log_warn(ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Warn);
}

void Log_error(ELogOptions opt, const C8 *format, ...) {
	Log_level(ELogLevel_Error);
}

void Log_fatal(ELogOptions opt, const C8 *format, ...) {

	if(!format)
		return;

	Log_printStackTrace(1, ELogLevel_Fatal, opt);

	String res = String_createNull();

	va_list arg1;
	va_start(arg1, format);
	Error err = String_formatVariadicx(&res, format, arg1);
	va_end(arg1);

	if(err.genericError)
		return;

	Log_log(ELogLevel_Fatal, opt, res);
	String_freex(&res);

	exit(1);
}
