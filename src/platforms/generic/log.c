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
