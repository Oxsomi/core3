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

extern int errno;

Error Error_stderr(U32 subId) {
	_Error_base(.genericError = EGenericError_Stderr, .errorSubId = subId, .paramValue0 = (U64)(U32)errno);
}

Error Error_none() { return (Error) { 0 }; }
