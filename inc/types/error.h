#pragma once
#include "types.h"

#define StackTrace_SIZE 128
typedef void *StackTrace[StackTrace_SIZE];

//TODO: Make errors extendible like TypeId

typedef enum EGenericError {
	EGenericError_None,
	EGenericError_OutOfMemory,
	EGenericError_OutOfBounds,
	EGenericError_NullPointer,
	EGenericError_Unauthorized,			//For example if the local permissions or remote server disallow it
	EGenericError_NotFound,
	EGenericError_DivideByZero,
	EGenericError_Overflow,
	EGenericError_Underflow,
	EGenericError_NaN,
	EGenericError_InvalidEnum,
	EGenericError_InvalidParameter,
	EGenericError_InvalidOperation,
	EGenericError_InvalidCast,
	EGenericError_InvalidState,
	EGenericError_RateLimit,
	EGenericError_LoopLimit,				//If the platform decides that loop limit is reached, this will be thrown
	EGenericError_AlreadyDefined,
	EGenericError_UnsupportedOperation,
	EGenericError_TimedOut,
	EGenericError_ConstData,			//If an operation is done on data that is supposed to be const
	EGenericError_PlatformError,
	EGenericError_Unimplemented
} EGenericError;

typedef enum EErrorParamValues {
	EErrorParamValues_None,
	EErrorParamValues_V0,
	EErrorParamValues_V1,
	EErrorParamValues_V0_1
} EErrorParamValues;

extern const C8 *EGenericError_toString[];
extern const EErrorParamValues EGenericError_hasParamValues[];

//Only direct caller preserved to save space in release mode

#ifdef NDEBUG
	#define ERROR_STACKTRACE 1
#else
	#define ERROR_STACKTRACE StackTrace_SIZE
#endif

//

typedef struct Error {

	EGenericError genericError;
	U32 errorSubId;

	U32 paramId;
	U32 paramSubId;

	U64 paramValue0;
	U64 paramValue1;

	//These are optional for functions that don't know about any platform dependent stuff
	//Code calling that should include their own stack trace if necessary.
	//When not present; stackTrace[0] will be null.

	void *stackTrace[ERROR_STACKTRACE];

} Error;

#define gotoIfError(x, ...) { err = __VA_ARGS__; if(err.genericError) goto x; }		//Shortcut to handle cleanup on error

impl void Error_fillStackTrace(Error *err);

#define _Error_base(...) Error err = (Error) { __VA_ARGS__ }; Error_fillStackTrace(&err); return err

inline Error Error_platformError(U32 subId, U64 platformError) {
	_Error_base(.genericError = EGenericError_PlatformError, .errorSubId = subId, .paramValue0 = platformError);
}

inline Error Error_outOfMemory(U32 subId) {
	_Error_base(.genericError = EGenericError_OutOfMemory, .errorSubId = subId);
}

inline Error Error_outOfBounds(U32 paramId, U32 paramSubId, U64 id, U64 limit) {
	_Error_base(
		.genericError = EGenericError_OutOfBounds, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = id, .paramValue1 = limit
	);
}

inline Error Error_nullPointer(U32 paramId, U32 paramSubId) {
	_Error_base(.genericError = EGenericError_NullPointer, .paramSubId = paramSubId, .paramId = paramId );
}

inline Error Error_unauthorized(U32 subId) {
	_Error_base(.genericError = EGenericError_Unauthorized, .errorSubId = subId);
}

inline Error Error_notFound(U32 subId, U32 paramId, U32 paramSubId) {
	_Error_base(.genericError = EGenericError_NotFound, .errorSubId = subId, .paramId = paramId, .paramSubId = paramSubId );
}

inline Error Error_divideByZero(U32 subId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_DivideByZero, .errorSubId = subId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

inline Error Error_overflow(U32 paramId, U32 paramSubId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_Overflow, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

inline Error Error_underflow(U32 paramId, U32 paramSubId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_Underflow, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

inline Error Error_NaN(U32 subId) {
	_Error_base(.genericError = EGenericError_NaN, .errorSubId = subId);
}

inline Error Error_invalidEnum(U32 paramId, U32 paramSubId, U64 value, U64 maxValue) {
	_Error_base(
		.genericError = EGenericError_InvalidEnum, .paramSubId = paramSubId, .paramId = paramId, 
		.paramValue0 = value, .paramValue1 = maxValue
	);
}

inline Error Error_invalidParameter(U32 paramId, U32 paramSubId, U32 subId) {
	_Error_base(
		.genericError = EGenericError_InvalidParameter, .paramSubId = paramSubId, .paramId = paramId, 
		.errorSubId = subId
	);
}

inline Error Error_invalidOperation(U32 subId) {
	_Error_base(.genericError = EGenericError_InvalidOperation, .errorSubId = subId);
}

inline Error Error_invalidCast(U32 subId, U32 typeId, U32 castTypeId) {
	_Error_base(
		.genericError = EGenericError_InvalidCast, .errorSubId = subId,
		.paramValue0 = typeId, .paramValue1 = castTypeId
	);
}

inline Error Error_invalidState(U32 subId) {
	_Error_base(.genericError = EGenericError_InvalidState, .errorSubId = subId);
}

inline Error Error_rateLimit(U32 subId, U64 limit) {
	_Error_base(.genericError = EGenericError_RateLimit, .errorSubId = subId, .paramValue0 = limit);
}

inline Error Error_loopLimit(U32 subId, U64 limit) {
	_Error_base(.genericError = EGenericError_LoopLimit, .errorSubId = subId, .paramValue0 = limit);
}

inline Error Error_alreadyDefined(U32 subId) {
	_Error_base(.genericError = EGenericError_AlreadyDefined, .errorSubId = subId);
}

inline Error Error_unimplemented(U32 subId) {
	_Error_base(.genericError = EGenericError_Unimplemented, .errorSubId = subId);
}

inline Error Error_unsupportedOperation(U32 subId) {
	_Error_base(.genericError = EGenericError_UnsupportedOperation, .errorSubId = subId);
}

inline Error Error_timedOut(U32 subId, U64 limit) {
	_Error_base(.genericError = EGenericError_TimedOut, .errorSubId = subId, .paramValue0 = limit);
}

inline Error Error_constData(U32 paramId, U32 subId) {
	_Error_base(.genericError = EGenericError_ConstData, .errorSubId = subId, .paramId = paramId );
}

inline Error Error_none() { return (Error) { 0 }; }
