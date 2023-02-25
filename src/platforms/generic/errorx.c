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

#include "types/buffer.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "platforms/platform.h"

#include <stdlib.h>

void Error_fillStackTrace(Error *err) {

	//Skip Error_fillStackTrace (skip=1), Error_x (skip=2)

	if(err)
		Log_captureStackTrace(err->stackTrace, ERROR_STACKTRACE, 2);
}

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options) {

	if(!err.genericError)
		return;

	String result = String_createNull();
	String platformErr = Error_formatPlatformError(err);

	if(
		!String_formatx(

			&result, 

			"%s: sub id: %X, param id: %u, param0: %08X, param1: %08X.\nPlatform error: %.*s", 

			EGenericError_TO_STRING[err.genericError],

			err.errorSubId,
			err.paramId,
			err.paramValue0,
			err.paramValue1,
			String_length(platformErr), platformErr.ptr

		).genericError
	)
		Log_log(logLevel == ELogLevel_Fatal ? ELogLevel_Error : logLevel, options, result);

	String_freex(&result);
	String_freex(&platformErr);

	Log_printCapturedStackTraceCustom(err.stackTrace, ERROR_STACKTRACE, logLevel, options);

	if(logLevel == ELogLevel_Fatal)
		exit(1);
}
