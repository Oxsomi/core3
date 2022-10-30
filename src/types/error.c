#include "types/error.h"
#include "types/string.h"

Error Error_base(
	EGenericError err, U32 subId, 
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

const C8 *EGenericError_toString[] = {
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

const EErrorParamValues EGenericError_hasParamValues[] = {
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V0_1,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V0_1,
	EErrorParamValues_V0_1,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V1,
	EErrorParamValues_V1,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V1,
	EErrorParamValues_None
};
