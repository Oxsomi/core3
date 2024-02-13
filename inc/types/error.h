/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#pragma once
#include "types.h"

#define _STACKTRACE_SIZE 32
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
	U32 pad;

	U64 paramValue0;
	U64 paramValue1;

	const C8 *errorStr;

	//These are optional for functions that don't know about any platform dependent stuff
	//Code calling that should include their own stack trace if necessary.
	//When not present; stackTrace[0] will be null.

	void *stackTrace[ERROR_STACKTRACE];

} Error;

//Shortcut to handle cleanup on error, can be disabled to save CPU time

#ifndef _DISABLE_ERRORS
	#define _gotoIfError(x, ...) { err = __VA_ARGS__; if(err.genericError) goto x; }
#else
	#define _gotoIfError(x, ...) { x; } /* suppress unused goto label */
#endif

impl void Error_fillStackTrace(Error *err);

#define _Error_base(...) Error err = (Error) { __VA_ARGS__ }; Error_fillStackTrace(&err); return err

Error Error_platformError(U32 subId, U64 platformError, const C8 *errorStr);
Error Error_outOfMemory(U32 subId, const C8 *errorStr);
Error Error_outOfBounds(U32 paramId, U64 id, U64 limit, const C8 *errorStr);
Error Error_nullPointer(U32 paramId, const C8 *errorStr);
Error Error_unauthorized(U32 subId, const C8 *errorStr);
Error Error_notFound(U32 subId, U32 paramId, const C8 *errorStr);
Error Error_divideByZero(U32 subId, U64 a, U64 b, const C8 *errorStr);
Error Error_overflow(U32 paramId, U64 a, U64 b, const C8 *errorStr);
Error Error_underflow(U32 paramId, U64 a, U64 b, const C8 *errorStr);
Error Error_NaN(U32 subId, const C8 *errorStr);
Error Error_invalidEnum(U32 paramId, U64 value, U64 maxValue, const C8 *errorStr);
Error Error_invalidParameter(U32 paramId, U32 subId, const C8 *errorStr);
Error Error_invalidOperation(U32 subId, const C8 *errorStr);
Error Error_invalidCast(U32 subId, U32 typeId, U32 castTypeId, const C8 *errorStr);
Error Error_invalidState(U32 subId, const C8 *errorStr);
Error Error_rateLimit(U32 subId, U64 limit, const C8 *errorStr);
Error Error_loopLimit(U32 subId, U64 limit, const C8 *errorStr);
Error Error_alreadyDefined(U32 subId, const C8 *errorStr);
Error Error_unimplemented(U32 subId, const C8 *errorStr);
Error Error_unsupportedOperation(U32 subId, const C8 *errorStr);
Error Error_timedOut(U32 subId, U64 limit, const C8 *errorStr);
Error Error_constData(U32 paramId, U32 subId, const C8 *errorStr);
Error Error_stderr(U32 subId, const C8 *errorStr);
Error Error_none();
