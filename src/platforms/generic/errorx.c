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

	String str = String_createRefUnsafeConst(EGenericError_toString[err.genericError]);

	if(err.errorSubId) {

		String prefix = String_createRefUnsafeConst(" sub id: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0;

			if(!String_createHexx(err.errorSubId, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	if(err.paramId) {

		String prefix = String_createRefUnsafeConst(" param id: ");

		if (!String_appendStringx(&str, prefix).genericError) {

			String str0;

			if(!String_createDecx(err.paramId, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	if(err.paramSubId) {

		String prefix = String_createRefUnsafeConst(" param sub id: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0;

			if(!String_createDecx(err.paramSubId, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	EErrorParamValues val = EGenericError_hasParamValues[err.genericError];

	if(val & EErrorParamValues_V0) {

		String prefix = String_createRefUnsafeConst(" param0: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0;

			if(!String_createHexx(err.paramValue0, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	if(val & EErrorParamValues_V1) {

		String prefix = String_createRefUnsafeConst(" param1: ");

		if(!String_appendStringx(&str, prefix).genericError) {

			String str0;

			if(!String_createHexx(err.paramValue1, false, &str0).genericError) {
				String_appendStringx(&str, str0);
				String_freex(&str0);
			}

			else String_appendx(&str, '?');
		}
	}

	LogArgs args = (LogArgs) { .argc = 1, .args = &str };
	Log_log(logLevel == ELogLevel_Fatal ? ELogLevel_Error : logLevel, options, args);

	String_freex(&str);

	Log_printCapturedStackTraceCustom(err.stackTrace, ERROR_STACKTRACE, logLevel, options);

	if(logLevel == ELogLevel_Fatal)
		exit(1);
}