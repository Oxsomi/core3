#include "types/error.h"
#include "types/string.h"

Error Error_base(
	enum GenericError err, U32 subId, 
	U32 paramId, U32 paramSubId, 
	U64 paramValue0, U64 paramValue1
) {
	return (Error) {
		.genericError = err,
		.errorSubId = subId,
		.paramId = paramId,
		.paramSubId = paramSubId,
		.paramValue0 = paramValue0,
		.paramValue1 = paramValue1
	};
}

Error Error_none() {
	return (Error) { 0 };
}

const C8 *GenericError_toString[] = {
	"None",
	"Out of memory",
	"Out of bounds",
	"Null pointer",
	"Unauthorized",
	"Not found",
	"Divide by zero",
	"Overflow",
	"Underflow",
	"NaN",
	"Invalid enum",
	"Invalid parameter",
	"Invalid operator",
	"Invalid cast",
	"Invalid state",
	"Rate limit",
	"Loop limit",
	"Already defined",
	"Unsupported operation",
	"Timed out",
	"Const data"
};

const ErrorParamValues GenericError_hasParamValues[] = {
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_V0_1,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_V0_1,
	ErrorParamValues_V0_1,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_V1,
	ErrorParamValues_V1,
	ErrorParamValues_None,
	ErrorParamValues_None,
	ErrorParamValues_V1,
	ErrorParamValues_None
};
