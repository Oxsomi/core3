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

#include "types/container/log.h"
#include "types/container/string.h"
#include "types/math/math.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

#include <stdlib.h>
#include <string.h>

void Log_printCapturedStackTrace(Allocator alloc, const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom(alloc, (const void**) stackTrace, STACKTRACE_SIZE, lvl, options);
}

void Log_printStackTrace(Allocator alloc, U8 skip, ELogLevel lvl, ELogOptions options) {

	StackTrace stackTrace;
	Log_captureStackTrace(alloc, stackTrace, STACKTRACE_SIZE, skip);

	Log_printCapturedStackTrace(alloc, stackTrace, lvl, options);
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

impl CharString Error_formatPlatformError(Allocator alloc, Error err);

void Error_print(Allocator alloc, Error err, ELogLevel logLevel, ELogOptions options) {

	if(!err.genericError)
		return;

	CharString result = CharString_createNull();
	CharString platformErr = Error_formatPlatformError(alloc, err);

	if(err.genericError == EGenericError_Stderr)
		platformErr = CharString_createRefCStrConst(strerror((int)err.paramValue0));

	if(
		!CharString_format(

			alloc,
			&result,

			"%s (%s)\nsub id: %"PRIu32"X, param id: %"PRIu32", param0: %08X, param1: %08X.\nPlatform/std error: %.*s.",

			err.errorStr,
			EGenericError_TO_STRING[err.genericError],

			err.errorSubId,
			err.paramId,
			err.paramValue0,
			err.paramValue1,
			CharString_length(platformErr), platformErr.ptr

		).genericError
		)
		Log_log(alloc, logLevel, options | ELogOptions_NoBreak, result);

	Log_printCapturedStackTraceCustom(alloc, (const void**)err.stackTrace, ERROR_STACKTRACE, ELogLevel_Error, options);

	CharString_free(&result, alloc);
	CharString_free(&platformErr, alloc);
}
