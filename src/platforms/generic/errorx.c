#include "platforms/errorx.h"
#include "platforms/log.h"
#include "platforms/platform.h"

#include <stdlib.h>

struct Error Error_traced(enum GenericError err, U32 subId, U32 paramId, U32 paramSubId, U64 paramValue0, U64 paramValue1) {
	struct Error res = Error_base(err, subId, paramId, paramSubId, paramValue0, paramValue1);
	Log_captureStackTrace(res.stackTrace, ERROR_STACKTRACE, 0);
	return res;
}

void Error_printx(struct Error err, enum LogLevel logLevel, enum LogOptions options) {

	if(!err.genericError)
		return;

	struct String str = String_createRefUnsafeConst(GenericError_toString[err.genericError]);
	struct Allocator alloc = Platform_instance.alloc;

	if(err.errorSubId) {

		struct String prefix = String_createRefUnsafeConst(" sub id: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			struct String str0;

			if(!String_createHex(err.errorSubId, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	if(err.paramId) {

		struct String prefix = String_createRefUnsafeConst(" param id: ");

		if (!String_appendString(&str, prefix, alloc).genericError) {

			struct String str0;

			if(!String_createDec(err.paramId, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	if(err.paramSubId) {

		struct String prefix = String_createRefUnsafeConst(" param sub id: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			struct String str0;

			if(!String_createDec(err.paramSubId, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	enum ErrorParamValues val = GenericError_hasParamValues[err.genericError];

	if(val & ErrorParamValues_V0) {

		struct String prefix = String_createRefUnsafeConst(" param0: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			struct String str0;

			if(!String_createHex(err.paramValue0, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	if(val & ErrorParamValues_V1) {

		struct String prefix = String_createRefUnsafeConst(" param1: ");

		if(!String_appendString(&str, prefix, alloc).genericError) {

			struct String str0;

			if(!String_createHex(err.paramValue1, false, alloc, &str0).genericError) {
				String_appendString(&str, str0, alloc);
				String_free(&str0, alloc);
			}

			else String_append(&str, '?', alloc);
		}
	}

	struct LogArgs args = (struct LogArgs) { .argc = 1, .args = &str };
	Log_log(logLevel == LogLevel_Fatal ? LogLevel_Error : logLevel, options, args);

	String_free(&str, alloc);

	Log_printCapturedStackTraceCustom(err.stackTrace, ERROR_STACKTRACE, logLevel, options);

	if(logLevel == LogLevel_Fatal)
		exit(1);
}