#include "types/error.h"
#include "types/string.h"

const C8 *EGenericError_TO_STRING[] = {
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
	"Const data",
	"Platform error",
	"Unimplemented"
};

const EErrorParamValues EGenericError_HAS_PARAM_VALUES[] = {
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V0N1,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_V0N1,
	EErrorParamValues_V0N1,
	EErrorParamValues_V0N1,
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
	EErrorParamValues_None,
	EErrorParamValues_None,
	EErrorParamValues_None
};

Error Error_platformError(U32 subId, U64 platformError) {
	_Error_base(.genericError = EGenericError_PlatformError, .errorSubId = subId, .paramValue0 = platformError);
}

Error Error_outOfMemory(U32 subId) {
	_Error_base(.genericError = EGenericError_OutOfMemory, .errorSubId = subId);
}

Error Error_outOfBounds(U32 paramId, U32 paramSubId, U64 id, U64 limit) {
	_Error_base(
		.genericError = EGenericError_OutOfBounds, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = id, .paramValue1 = limit
	);
}

Error Error_nullPointer(U32 paramId, U32 paramSubId) {
	_Error_base(.genericError = EGenericError_NullPointer, .paramSubId = paramSubId, .paramId = paramId );
}

Error Error_unauthorized(U32 subId) {
	_Error_base(.genericError = EGenericError_Unauthorized, .errorSubId = subId);
}

Error Error_notFound(U32 subId, U32 paramId, U32 paramSubId) {
	_Error_base(.genericError = EGenericError_NotFound, .errorSubId = subId, .paramId = paramId, .paramSubId = paramSubId );
}

Error Error_divideByZero(U32 subId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_DivideByZero, .errorSubId = subId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

Error Error_overflow(U32 paramId, U32 paramSubId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_Overflow, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

Error Error_underflow(U32 paramId, U32 paramSubId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_Underflow, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

Error Error_NaN(U32 subId) {
	_Error_base(.genericError = EGenericError_NaN, .errorSubId = subId);
}

Error Error_invalidEnum(U32 paramId, U32 paramSubId, U64 value, U64 maxValue) {
	_Error_base(
		.genericError = EGenericError_InvalidEnum, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = value, .paramValue1 = maxValue
	);
}

Error Error_invalidParameter(U32 paramId, U32 paramSubId, U32 subId) {
	_Error_base(
		.genericError = EGenericError_InvalidParameter, .paramSubId = paramSubId, .paramId = paramId, 
		.errorSubId = subId
	);
}

Error Error_invalidOperation(U32 subId) {
	_Error_base(.genericError = EGenericError_InvalidOperation, .errorSubId = subId);
}

Error Error_invalidCast(U32 subId, U32 typeId, U32 castTypeId) {
	_Error_base(
		.genericError = EGenericError_InvalidCast, .errorSubId = subId,
		.paramValue0 = typeId, .paramValue1 = castTypeId
	);
}

Error Error_invalidState(U32 subId) {
	_Error_base(.genericError = EGenericError_InvalidState, .errorSubId = subId);
}

Error Error_rateLimit(U32 subId, U64 limit) {
	_Error_base(.genericError = EGenericError_RateLimit, .errorSubId = subId, .paramValue0 = limit);
}

Error Error_loopLimit(U32 subId, U64 limit) {
	_Error_base(.genericError = EGenericError_LoopLimit, .errorSubId = subId, .paramValue0 = limit);
}

Error Error_alreadyDefined(U32 subId) {
	_Error_base(.genericError = EGenericError_AlreadyDefined, .errorSubId = subId);
}

Error Error_unimplemented(U32 subId) {
	_Error_base(.genericError = EGenericError_Unimplemented, .errorSubId = subId);
}

Error Error_unsupportedOperation(U32 subId) {
	_Error_base(.genericError = EGenericError_UnsupportedOperation, .errorSubId = subId);
}

Error Error_timedOut(U32 subId, U64 limit) {
	_Error_base(.genericError = EGenericError_TimedOut, .errorSubId = subId, .paramValue0 = limit);
}

Error Error_constData(U32 paramId, U32 subId) {
	_Error_base(.genericError = EGenericError_ConstData, .errorSubId = subId, .paramId = paramId );
}

Error Error_none() { return (Error) { 0 }; }
