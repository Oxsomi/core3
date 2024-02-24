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

#include "types/buffer.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "platforms/platform.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

void Error_fillStackTrace(Error *err) {

	//Skip Error_fillStackTrace (skip=1), Error_x (skip=2)

	if(err)
		Log_captureStackTrace(err->stackTrace, ERROR_STACKTRACE, 2);
}

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options) {
	Error_print(Platform_instance.alloc, err, logLevel, options);
}

CharString Error_formatPlatformErrorx(Error err) {
	return Error_formatPlatformError(Platform_instance.alloc, err);
}

void Error_print(Allocator alloc, Error err, ELogLevel logLevel, ELogOptions options) {

	if(!err.genericError)
		return;

	CharString result = CharString_createNull();
	CharString platformErr = Error_formatPlatformError(alloc, err);

	if(err.genericError == EGenericError_Stderr)
		platformErr = CharString_createRefCStrConst(strerror((int)err.paramValue0));

	Log_printCapturedStackTraceCustom(alloc, (const void**)err.stackTrace, ERROR_STACKTRACE, ELogLevel_Error, options);

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
		Log_log(alloc, logLevel, options, result);

	CharString_free(&result, alloc);
	CharString_free(&platformErr, alloc);
}
