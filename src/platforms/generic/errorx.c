#include "platforms/errorx.h"
#include "platforms/log.h"
#include "platforms/platform.h"

#include <stdlib.h>

Error Error_traced(EGenericError err, U32 subId, U32 paramId, U32 paramSubId, U64 paramValue0, U64 paramValue1) {
	Error res = Error_base(err, subId, paramId, paramSubId, paramValue0, paramValue1);
	Log_captureStackTrace(res.stackTrace, ERROR_STACKTRACE, 0);
	return res;
}

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options) {

	if(!err.genericError)
		return;

	String str = String_createRefUnsafeConst(EGenericError_toString[err.genericError]);
	Allocator alloc = Platform_instance.alloc;

	if(err.errorSubId) {

		String prefix = String_createRefUnsafeConst(" sub id: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			String str0;

			if(!String_createHex(err.errorSubId, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	if(err.paramId) {

		String prefix = String_createRefUnsafeConst(" param id: ");

		if (!String_appendString(&str, prefix, alloc).genericError) {

			String str0;

			if(!String_createDec(err.paramId, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	if(err.paramSubId) {

		String prefix = String_createRefUnsafeConst(" param sub id: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			String str0;

			if(!String_createDec(err.paramSubId, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	EErrorParamValues val = EGenericError_hasParamValues[err.genericError];

	if(val & EErrorParamValues_V0) {

		String prefix = String_createRefUnsafeConst(" param0: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			String str0;

			if(!String_createHex(err.paramValue0, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	if(val & EErrorParamValues_V1) {

		String prefix = String_createRefUnsafeConst(" param1: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			String str0;

			if(!String_createHex(err.paramValue1, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	LogArgs args = (LogArgs) { .argc = 1, .args = &str };
	Log_log(logLevel == ELogLevel_Fatal ? ELogLevel_Error : logLevel, options, args);

	String_free(&str, alloc);

	Log_printCapturedStackTraceCustom(err.stackTrace, ERROR_STACKTRACE, logLevel, options);

	if(logLevel == ELogLevel_Fatal)
		exit(1);
}