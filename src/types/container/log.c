/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/allocator.h"
#include "types/base/error.h"

#include <stdlib.h>
#include <string.h>

void Log_printCapturedStackTrace(const Allocator *alloc, const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom(alloc, (const void**) stackTrace, STACKTRACE_SIZE, lvl, options);
}

void Log_printStackTrace(const Allocator *alloc, U8 skip, ELogLevel lvl, ELogOptions options) {

	StackTrace stackTrace;
	Error_captureStackTrace(stackTrace, STACKTRACE_SIZE, skip);

	Log_printCapturedStackTrace(alloc, stackTrace, lvl, options);
}

#define Log_level(lvl) 															\
																				\
	if(!format)																	\
		return;																	\
																				\
	CharString res = CharString_createNull();									\
																				\
	va_list arg1;																\
	va_start(arg1, format);														\
	Bool s_uccess = CharString_formatVariadic(alloc, &res, NULL, format, arg1);	\
	va_end(arg1);																\
																				\
	if(s_uccess)																\
		Log_log(alloc, lvl, opt, &res);											\
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

void Error_print(const Allocator *alloc, const Error *e_rr, ELogLevel logLevel, ELogOptions options) {

	if(!e_rr || !e_rr->genericError)
		return;

	CharString result = CharString_createNull();
	CharString platformErr = CharString_createNull();

	if (e_rr->genericError == EGenericError_PlatformError)
		platformErr = Error_formatPlatformError(alloc, e_rr);

	if(e_rr->genericError == EGenericError_Stderr)
		platformErr = CharString_createRefCStrConst(strerror((int)e_rr->paramValue0));

	//TODO: Replace this with something less annoying.

	if(CharString_format(
		alloc, &result,
		NULL,
		"%s (%s)\nsub id: %"PRIu32"X, param id: %"PRIu32", param0: %08X, param1: %08X.\nPlatform/std error: %.*s.",
		e_rr->errorStr,
		EGenericError_TO_STRING[e_rr->genericError],
		e_rr->errorSubId,
		e_rr->paramId,
		e_rr->paramValue0,
		e_rr->paramValue1,
		CharString_length(platformErr), platformErr.ptr
	))
		Log_log(alloc, logLevel, options | ELogOptions_NoBreak, &result);

	Log_printCapturedStackTraceCustom(alloc, (const void**)e_rr->stackTrace, ERROR_STACKTRACE, ELogLevel_Error, options);

	CharString_free(&result, alloc);
	CharString_free(&platformErr, alloc);
}
