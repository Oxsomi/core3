/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//types/base/error.h

#pragma once
#include "types/base/platform_types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Default stacktrace

#define STACKTRACE_SIZE 32
typedef void *StackTrace[STACKTRACE_SIZE];

//TODO: Make errors extendable like TypeId

typedef enum EGenericError {
	EGenericError_None,
	EGenericError_OutOfMemory,
	EGenericError_OutOfBounds,
	EGenericError_NullPointer,
	EGenericError_Unauthorized,            //For example if the local permissions or remote server disallow it
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
	EGenericError_LoopLimit,                //If the platform decides that loop limit is reached, this will be thrown
	EGenericError_AlreadyDefined,
	EGenericError_UnsupportedOperation,
	EGenericError_TimedOut,
	EGenericError_ConstData,            //If an operation is done on data that is supposed to be const
	EGenericError_PlatformError,
	EGenericError_Unimplemented,
	EGenericError_Stderr
} EGenericError;

extern const C8 *EGenericError_TO_STRING[];

//Only direct caller preserved to save space in release mode.
//Web spends one entry on the trace ring generation rather than a frame (see the web branch of
// types/base/platforms/unix/uerror.c), so it needs one more entry to keep the same frame count.

#if _PLATFORM_TYPE == PLATFORM_WEB
	#define ERROR_STACKTRACE_RESERVED 1
#else
	#define ERROR_STACKTRACE_RESERVED 0
#endif

#ifdef NDEBUG
	#define ERROR_STACKTRACE (1 + ERROR_STACKTRACE_RESERVED)
#else
	#define ERROR_STACKTRACE (STACKTRACE_SIZE + ERROR_STACKTRACE_RESERVED)
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

//Error handling; functions return Bool success and set an optional Error.
//(void) 0 is a trick to require a ; after the statement

#define gotoIfError3(x, ...) {        \
	if(!(__VA_ARGS__)) {            \
		s_uccess = false;            \
		goto x;                        \
	}                                \
} (void) 0

#define retError(x, ...) {            \
	if(e_rr) *e_rr = __VA_ARGS__;    \
	s_uccess = false;                \
	goto x;                            \
} (void) 0

impl void Error_captureStackTrace(void **stackTrace, U8 stackSize, U8 skip);    //May fail if (stackSize + skip + 1 > 128)

#if _PLATFORM_TYPE == PLATFORM_WEB

	//Web keeps the trace text in a small recycled ring and stores the owning generation in stackTrace[0],
	// with the frames from stackTrace[1] on (see types/base/platforms/unix/uerror.c).
	//False means the ring has since reused that slot, so the frames now point at an unrelated stack.

	Bool Error_webStackTraceIsLive(const void *const *stackTrace);

	//Anything that prints a trace long after capturing it has to copy the text out of the ring, point the
	// frames at that copy, and stamp stackTrace[0] with this so a reader knows the frames are owned rather
	// than a generation to check.
	//The tracked allocator does exactly that: it captures per allocation and prints at shutdown, by which
	// point no ring size could still hold the text.

	#define ERROR_WEB_STACKTRACE_OWNED ((void*)(U64) U64_MAX)

#endif

void Error_fillStackTrace(Error *err);

#define Error_base(...) Error err = { __VA_ARGS__ }; Error_fillStackTrace(&err); return err

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

#ifdef __cplusplus
	}
#endif
