/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#pragma once
#include "types.h"

#define _STACKTRACE_SIZE 128
typedef void *StackTrace[_STACKTRACE_SIZE];

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
	EGenericError_Unimplemented,
	EGenericError_Stderr
} EGenericError;

extern const C8 *EGenericError_TO_STRING[];

//Only direct caller preserved to save space in release mode

#ifdef NDEBUG
	#define ERROR_STACKTRACE 1
#else
	#define ERROR_STACKTRACE _STACKTRACE_SIZE
#endif

//

typedef struct Error {

	EGenericError genericError;
	U32 errorSubId;

	U32 paramId;

	U64 paramValue0;
	U64 paramValue1;

	//These are optional for functions that don't know about any platform dependent stuff
	//Code calling that should include their own stack trace if necessary.
	//When not present; stackTrace[0] will be null.

	void *stackTrace[ERROR_STACKTRACE];

} Error;

#define _gotoIfError(x, ...) { err = __VA_ARGS__; if(err.genericError) goto x; }		//Shortcut to handle cleanup on error

impl void Error_fillStackTrace(Error *err);

#define _Error_base(...) Error err = (Error) { __VA_ARGS__ }; Error_fillStackTrace(&err); return err

Error Error_platformError(U32 subId, U64 platformError);
Error Error_outOfMemory(U32 subId);
Error Error_outOfBounds(U32 paramId, U64 id, U64 limit);
Error Error_nullPointer(U32 paramId);
Error Error_unauthorized(U32 subId);
Error Error_notFound(U32 subId, U32 paramId);
Error Error_divideByZero(U32 subId, U64 a, U64 b);
Error Error_overflow(U32 paramId, U64 a, U64 b);
Error Error_underflow(U32 paramId, U64 a, U64 b);
Error Error_NaN(U32 subId);
Error Error_invalidEnum(U32 paramId, U64 value, U64 maxValue);
Error Error_invalidParameter(U32 paramId, U32 subId);
Error Error_invalidOperation(U32 subId);
Error Error_invalidCast(U32 subId, U32 typeId, U32 castTypeId);
Error Error_invalidState(U32 subId);
Error Error_rateLimit(U32 subId, U64 limit);
Error Error_loopLimit(U32 subId, U64 limit);
Error Error_alreadyDefined(U32 subId);
Error Error_unimplemented(U32 subId);
Error Error_unsupportedOperation(U32 subId);
Error Error_timedOut(U32 subId, U64 limit);
Error Error_constData(U32 paramId, U32 subId);
Error Error_stderr(U32 subId);
Error Error_none();
