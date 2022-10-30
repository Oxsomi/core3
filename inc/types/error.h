#pragma once
#include "types.h"

#define StackTrace_SIZE 128
typedef void *StackTrace[StackTrace_SIZE];

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
	EGenericError_ConstData				//If an operation is done on data that is supposed to be const
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
	//Stacktraces have to be done through platform dependent Error_traced to also save performance.
	//When not present; stackTrace[0] will be null.

	void *stackTrace[ERROR_STACKTRACE];

} Error;

//Base error doesn't fill stacktrace,
//But the one in platform does

Error Error_base(
	EGenericError err, U32 subId, 
	U32 paramId, U32 paramSubId, 
	U64 paramValue0, U64 paramValue1
);

Error Error_none();
