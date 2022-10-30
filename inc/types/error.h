#pragma once
#include "types.h"

#define StackTrace_SIZE 128
typedef void *StackTrace[StackTrace_SIZE];

typedef enum GenericError {
	GenericError_None,
	GenericError_OutOfMemory,
	GenericError_OutOfBounds,
	GenericError_NullPointer,
	GenericError_Unauthorized,			//For example if the local permissions or remote server disallow it
	GenericError_NotFound,
	GenericError_DivideByZero,
	GenericError_Overflow,
	GenericError_Underflow,
	GenericError_NaN,
	GenericError_InvalidEnum,
	GenericError_InvalidParameter,
	GenericError_InvalidOperation,
	GenericError_InvalidCast,
	GenericError_InvalidState,
	GenericError_RateLimit,
	GenericError_LoopLimit,				//If the platform decides that loop limit is reached, this will be thrown
	GenericError_AlreadyDefined,
	GenericError_UnsupportedOperation,
	GenericError_TimedOut,
	GenericError_ConstData				//If an operation is done on data that is supposed to be const
} GenericError;

typedef enum ErrorParamValues {
	ErrorParamValues_None,
	ErrorParamValues_V0,
	ErrorParamValues_V1,
	ErrorParamValues_V0_1
} ErrorParamValues;

extern const C8 *GenericError_toString[];
extern const ErrorParamValues GenericError_hasParamValues[];

//Only direct caller preserved to save space in release mode

#ifdef NDEBUG
	#define ERROR_STACKTRACE 1
#else
	#define ERROR_STACKTRACE StackTrace_SIZE
#endif

//

typedef struct Error {

	GenericError genericError;
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
	GenericError err, U32 subId, 
	U32 paramId, U32 paramSubId, 
	U64 paramValue0, U64 paramValue1
);

Error Error_none();
