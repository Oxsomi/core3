/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/error.h"
#include "types/string.h"
#include "types/log.h"
#include "types/allocator.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

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
	"Unimplemented",
	"Std error"
};

Error Error_platformError(U32 subId, U64 platformError, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_PlatformError, .errorSubId = subId, .paramValue0 = platformError, .errorStr = errorStr
	);
}

Error Error_outOfMemory(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_OutOfMemory, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_outOfBounds(U32 paramId, U64 id, U64 limit, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_OutOfBounds, .paramId = paramId,
		.paramValue0 = id, .paramValue1 = limit,
		.errorStr = errorStr
	);
}

Error Error_nullPointer(U32 paramId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_NullPointer, .paramId = paramId, .errorStr = errorStr);
}

Error Error_unauthorized(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_Unauthorized, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_notFound(U32 subId, U32 paramId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_NotFound, .errorSubId = subId, .paramId = paramId, .errorStr = errorStr);
}

Error Error_divideByZero(U32 subId, U64 a, U64 b, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_DivideByZero, .errorSubId = subId,
		.paramValue0 = a, .paramValue1 = b,
		.errorStr = errorStr
	);
}

Error Error_overflow(U32 paramId, U64 a, U64 b, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_Overflow, .paramId = paramId,
		.paramValue0 = a, .paramValue1 = b,
		.errorStr = errorStr
	);
}

Error Error_underflow(U32 paramId, U64 a, U64 b, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_Underflow, .paramId = paramId,
		.paramValue0 = a, .paramValue1 = b,
		.errorStr = errorStr
	);
}

Error Error_NaN(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_NaN, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_invalidEnum(U32 paramId, U64 value, U64 maxValue, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_InvalidEnum, .paramId = paramId,
		.paramValue0 = value, .paramValue1 = maxValue,
		.errorStr = errorStr
	);
}

Error Error_invalidParameter(U32 paramId, U32 subId, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_InvalidParameter, .paramId = paramId,
		.errorSubId = subId,
		.errorStr = errorStr
	);
}

Error Error_invalidOperation(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_InvalidOperation, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_invalidCast(U32 subId, U32 typeId, U32 castTypeId, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_InvalidCast, .errorSubId = subId,
		.paramValue0 = typeId, .paramValue1 = castTypeId,
		.errorStr = errorStr
	);
}

Error Error_invalidState(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_InvalidState, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_rateLimit(U32 subId, U64 limit, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_RateLimit, .errorSubId = subId, .paramValue0 = limit, .errorStr = errorStr);
}

Error Error_loopLimit(U32 subId, U64 limit, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_LoopLimit, .errorSubId = subId, .paramValue0 = limit, .errorStr = errorStr);
}

Error Error_alreadyDefined(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_AlreadyDefined, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_unimplemented(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_Unimplemented, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_unsupportedOperation(U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_UnsupportedOperation, .errorSubId = subId, .errorStr = errorStr);
}

Error Error_timedOut(U32 subId, U64 limit, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_TimedOut, .errorSubId = subId, .paramValue0 = limit, .errorStr = errorStr);
}

Error Error_constData(U32 paramId, U32 subId, const C8 *errorStr) {
	Error_base(.genericError = EGenericError_ConstData, .errorSubId = subId, .paramId = paramId, .errorStr = errorStr);
}

Error Error_stderr(U32 subId, const C8 *errorStr) {
	Error_base(
		.genericError = EGenericError_Stderr, .errorSubId = subId, .paramValue0 = (U64)(U32)errno, .errorStr = errorStr
	);
}

Error Error_none() { return (Error) { 0 }; }

impl CharString Error_formatPlatformError(Allocator alloc, Error err);

void Error_print(Allocator alloc, Error err, ELogLevel logLevel, ELogOptions options) {

	if(!err.genericError)
		return;

	CharString result = CharString_createNull();
	CharString platformErr = Error_formatPlatformError(alloc, err);

	if(err.genericError == EGenericError_Stderr)
		platformErr = CharString_createRefCStrConst(strerror((int)err.paramValue0));

	Log_printCapturedStackTraceCustom(alloc, (const void**)err.stackTrace, ERROR_STACKTRACE, ELogLevel_Error, options);

	if(
		!CharString_format(

			alloc,
			&result,

			"%s (%s)\nsub id: %"PRIu32"X, param id: %"PRIu32", param0: %08X, param1: %08X.\nPlatform/std error: %.*s.",

			err.errorStr,
			EGenericError_TO_STRING[err.genericError],

			err.errorSubId,
			err.paramId,
			err.paramValue0,
			err.paramValue1,
			CharString_length(platformErr), platformErr.ptr

		).genericError
	)
		Log_log(alloc, logLevel, options, result);

	CharString_free(&result, alloc);
	CharString_free(&platformErr, alloc);
}
