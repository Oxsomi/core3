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

#include "types/error.h"
#include "types/string.h"

#include <errno.h>

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

Error Error_platformError(U32 subId, U64 platformError) {
	_Error_base(.genericError = EGenericError_PlatformError, .errorSubId = subId, .paramValue0 = platformError);
}

Error Error_outOfMemory(U32 subId) {
	_Error_base(.genericError = EGenericError_OutOfMemory, .errorSubId = subId);
}

Error Error_outOfBounds(U32 paramId, U64 id, U64 limit) {
	_Error_base(
		.genericError = EGenericError_OutOfBounds, .paramId = paramId, 
		.paramValue0 = id, .paramValue1 = limit
	);
}

Error Error_nullPointer(U32 paramId) {
	_Error_base(.genericError = EGenericError_NullPointer, .paramId = paramId);
}

Error Error_unauthorized(U32 subId) {
	_Error_base(.genericError = EGenericError_Unauthorized, .errorSubId = subId);
}

Error Error_notFound(U32 subId, U32 paramId) {
	_Error_base(.genericError = EGenericError_NotFound, .errorSubId = subId, .paramId = paramId);
}

Error Error_divideByZero(U32 subId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_DivideByZero, .errorSubId = subId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

Error Error_overflow(U32 paramId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_Overflow, .paramId = paramId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

Error Error_underflow(U32 paramId, U64 a, U64 b) {
	_Error_base(
		.genericError = EGenericError_Underflow, .paramId = paramId, 
		.paramValue0 = a, .paramValue1 = b
	);
}

Error Error_NaN(U32 subId) {
	_Error_base(.genericError = EGenericError_NaN, .errorSubId = subId);
}

Error Error_invalidEnum(U32 paramId, U64 value, U64 maxValue) {
	_Error_base(
		.genericError = EGenericError_InvalidEnum, .paramId = paramId, 
		.paramValue0 = value, .paramValue1 = maxValue
	);
}

Error Error_invalidParameter(U32 paramId, U32 subId) {
	_Error_base(
		.genericError = EGenericError_InvalidParameter, .paramId = paramId, 
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

Error Error_stderr(U32 subId) {
	_Error_base(.genericError = EGenericError_Stderr, .errorSubId = subId, .paramValue0 = (U64)(U32)errno);
}

Error Error_none() { return (Error) { 0 }; }
