/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/error.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Buffer (more functions in types/container/buffer.h)

U64 Buffer_length(Buffer buf);

Bool Buffer_isRef(Buffer buf);
Bool Buffer_isConstRef(Buffer buf);

Buffer Buffer_createManagedPtr(void *ptr, U64 length);

//These should never be Buffer_free-d because Buffer doesn't know if it's allocated

Buffer Buffer_createNull();

Buffer Buffer_createRef(void *v, U64 length);
Buffer Buffer_createRefConst(const void *v, U64 length);

//Bit manipulation

Bool Buffer_memcpy(Buffer dst, Buffer src);			//Copies bytes from two separate ranges
Bool Buffer_memmove(Buffer dst, Buffer src);		//Copies bytes from two overlapping ranges

Error Buffer_setAllBits(Buffer dst);
Error Buffer_unsetAllBits(Buffer dst);

Error Buffer_setAllBitsTo(Buffer buf, Bool isOn);

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output);

Error Buffer_setBit(Buffer buf, U64 offset);
Error Buffer_resetBit(Buffer buf, U64 offset);

Error Buffer_setBitTo(Buffer buf, U64 offset, Bool value);

Error Buffer_bitwiseOr(Buffer dst, Buffer src);
Error Buffer_bitwiseAnd(Buffer dst, Buffer src);
Error Buffer_bitwiseXor(Buffer dst, Buffer src);
Error Buffer_bitwiseNot(Buffer dst);

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits);
Error Buffer_unsetBitRange(Buffer dst, U64 dstOff, U64 bits);

//Comparison

Bool Buffer_eq(Buffer buf0, Buffer buf1);			//Also compares size
Bool Buffer_neq(Buffer buf0, Buffer buf1);			//Also compares size

#define BUFFER_OP(T)									\
Error Buffer_append##T(Buffer *buf, T v);				\
Error Buffer_consume##T(Buffer *buf, T *v);				\
T Buffer_read##T(Buffer buf, U64 off,  Bool *success);	\
Bool Buffer_write##T(Buffer buf, U64 off, T v)

#define BUFFER_OP_IMPL(T)																	\
Error Buffer_append##T(Buffer *buf, T v)	{ return Buffer_append(buf, &v, sizeof(v)); }	\
Error Buffer_consume##T(Buffer *buf, T *v)	{ return Buffer_consume(buf, v, sizeof(*v)); }	\
																							\
T Buffer_read##T(Buffer buf, U64 off, Bool *success) {										\
																							\
	buf = Buffer_createRefFromBuffer(buf, true);											\
	T v = (T) { 0 };																		\
	Error err = Buffer_offset(&buf, off);													\
																							\
	if(err.genericError) {																	\
		if(success) *success = false;														\
		return v;																			\
	}																						\
																							\
	Bool ok = !Buffer_consume##T(&buf, &v).genericError;									\
	if(success) *success = ok;																\
	return v;																				\
}																							\
																							\
Bool Buffer_write##T(Buffer buf, U64 off, T v) {											\
	buf = Buffer_createRefFromBuffer(buf, false);											\
	Error err = Buffer_offset(&buf, off);													\
	if(err.genericError) return false;														\
	return !Buffer_append##T(&buf, v).genericError;											\
}

BUFFER_OP(U64);
BUFFER_OP(U32);
BUFFER_OP(U16);
BUFFER_OP(U8);

BUFFER_OP(I64);
BUFFER_OP(I32);
BUFFER_OP(I16);
BUFFER_OP(I8);

BUFFER_OP(F64);
BUFFER_OP(F32);

Error Buffer_offset(Buffer *buf, U64 length);

Error Buffer_append(Buffer *buf, const void *v, U64 length);
Error Buffer_appendBuffer(Buffer *buf, Buffer append);

Error Buffer_consume(Buffer *buf, void *v, U64 length);

//Use this instead of simply copying the Buffer to a new location
//A copy like this is only fine if the other doesn't get freed.
//In all other cases, createRefFromBuffer should be called on the one that shouldn't be freeing.
//If it needs to be refcounted, RefPtr should be used.
Buffer Buffer_createRefFromBuffer(Buffer buf, Bool isConst);

#ifdef __cplusplus
	}
#endif
