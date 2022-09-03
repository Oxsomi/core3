#pragma once
#include "types.h"

#define STACKTRACE_MAX_SIZE 128
typedef void *StackTrace[STACKTRACE_MAX_SIZE];

enum GenericError {
	GenericError_None,
	GenericError_OutOfMemory,
	GenericError_OutOfBounds,
	GenericError_NullPointer,
	GenericError_Unauthorized,
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
	GenericError_LoopLimit				/// If the platform decides that loop limit is reached, this will be thrown
};

//Only direct caller preserved to save space in release mode

#ifdef NDEBUG
	#define ERROR_STACKTRACE 1
#else
	#define ERROR_STACKTRACE STACKTRACE_MAX_SIZE
#endif

//

struct Error {

	enum GenericError genericError;
	u32 errorSubId;

	u32 paramId;
	u32 paramSubId;

	u64 paramValue0;
	u64 paramValue1;

	//These are optional for functions that don't know about any platform dependent stuff
	//Code calling that should include their own stack trace if necessary.
	//Stacktraces have to be done through platform dependent Error_traced to also save performance.
	//When not present; stackTrace[0] will be null.

	void *stackTrace[ERROR_STACKTRACE];
};

//Base error doesn't fill stacktrace,
//But the one in platform does

struct Error Error_base(
	enum GenericError err, u32 subId, 
	u32 paramId, u32 paramSubId, 
	u64 paramValue0, u64 paramValue1
);

struct Error Error_none();