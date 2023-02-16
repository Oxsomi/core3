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

	String str = String_createConstRefUnsafe(EGenericError_TO_STRING[err.genericError]);
	String strCpy = String_createNull();

	if(String_createCopyx(str, &strCpy).genericError)
		return;

	str = strCpy;			//Safe because it's not allocated.

	if(err.errorSubId) {

		String prefix = String_createConstRefUnsafe(" sub id: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0 = String_createNull();

			if(!String_createHexx(err.errorSubId, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	if(err.paramId) {

		String prefix = String_createConstRefUnsafe(" param id: ");

		if (!String_appendStringx(&str, prefix).genericError) {

			String str0 = String_createNull();

			if(!String_createDecx(err.paramId, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	if(err.paramSubId) {

		String prefix = String_createConstRefUnsafe(" param sub id: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0 = String_createNull();

			if(!String_createDecx(err.paramSubId, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	EErrorParamValues val = EGenericError_HAS_PARAM_VALUES[err.genericError];

	if(val & EErrorParamValues_V0) {

		String prefix = String_createConstRefUnsafe(" param0: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0 = String_createNull();

			if(!String_createHexx(err.paramValue0, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	if(val & EErrorParamValues_V1) {

		String prefix = String_createConstRefUnsafe(" param1: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0 = String_createNull();

			if(!String_createHexx(err.paramValue1, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	if(err.genericError == EGenericError_PlatformError) {

		String suffix0 = String_createConstRefUnsafe(": ");

		if(!String_appendStringx(&str, suffix0).genericError) {
			String suffix1 = Error_formatPlatformError(err);
			String_appendStringx(&str, suffix1);
			String_free(&suffix1, Platform_instance.alloc);
		}
	}

	LogArgs args = (LogArgs) { .argc = 1, .args = &str };
	Log_log(logLevel == ELogLevel_Fatal ? ELogLevel_Error : logLevel, options, args);

	String_freex(&str);

	Log_printCapturedStackTraceCustom(err.stackTrace, ERROR_STACKTRACE, logLevel, options);

	if(logLevel == ELogLevel_Fatal)
		exit(1);
}
