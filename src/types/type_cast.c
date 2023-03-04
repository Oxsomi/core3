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

#include "types/type_cast.h"

//Conversions

#define _FLP_FROMBITS(T, TInt)											\
Error T##_fromBits(U64 v, T *res) {										\
																		\
	if(!res)															\
		return Error_nullPointer(1);									\
																		\
	if(v > TInt##_MAX)													\
		return Error_overflow(0, v, TInt##_MAX);						\
																		\
	TInt bits = (TInt) v;												\
	T r = *(const T*) &bits;											\
																		\
	if(!T##_isValid(r))													\
		return Error_NaN(0);											\
																		\
	if(v == ((TInt)1 << (sizeof(T) * 8 - 1)))	/* Signed zero */		\
		r = 0;															\
																		\
	*res = r;															\
	return Error_none();												\
}

_FLP_FROMBITS(F32, U32);
_FLP_FROMBITS(F64, U64);

//Cast to ints

#define _CastFromU(type, toBits, ...) {											\
																				\
	if(!res)																	\
		return Error_nullPointer(1);											\
																				\
	__VA_ARGS__																	\
																				\
	if(v > type##_MAX)															\
		return Error_overflow(0, toBits, (U64) type##_MAX);						\
																				\
	*res = (type) v;															\
	return Error_none();														\
}

#define _CastFromI(type, castType, toBits, ...) _CastFromU(type, toBits, __VA_ARGS__ 		\
	if(v < type##_MIN)																		\
		return Error_underflow(0, toBits, (castType) type##_MIN);							\
)

#define _CastFromF(type) _CastFromI(type, type, *(const U32*)&v)
#define _CastFromD(type) _CastFromI(type, type, *(const U64*)&v)

#define _ITOF(T, TUint)																		\
Error T##_fromUInt(U64 v, T *res)		_CastFromU(T, (U64)v)								\
Error T##_fromInt(I64 v, T *res)		_CastFromI(T, TUint, (U64)v)						\
Error T##_fromFloat(F32 v, T *res)		_CastFromF(T) 										\
Error T##_fromDouble(F64 v, T *res)		_CastFromD(T) 

_ITOF(I8, U8);
_ITOF(I16, U16);
_ITOF(I32, U32);

Error I64_fromUInt(U64 v, I64 *res) {

	if(!res)
		return Error_nullPointer(1);

	if(v > (U64) I64_MAX)
		return Error_overflow(0, (U64)v, (U64) I64_MAX);

	return Error_none();
}

Error I64_fromFloat(F32 v, I64 *res)	_CastFromF(I64)
Error I64_fromDouble(F64 v, I64 *res)	_CastFromD(I64)

//Cast to uints

#define _UTOF(T)																			\
Error T##_fromUInt(U64 v, T *res)		_CastFromU(T, (U64)v)								\
Error T##_fromInt(I64 v, T *res)		_CastFromI(T, T, (U64)v)							\
Error T##_fromFloat(F32 v, T *res)		_CastFromF(T) 										\
Error T##_fromDouble(F64 v, T *res)		_CastFromD(T) 

_UTOF(U8);
_UTOF(U16);
_UTOF(U32);

Error U64_fromInt(I64 v, U64 *res) {

	if(!res)
		return Error_nullPointer(1);

	if(v < (I64) U64_MAX)
		return Error_underflow(0, (U64)v, 0);

	return Error_none();
}


Error U64_fromFloat(F32 v, U64 *res)	_CastFromF(U64)
Error U64_fromDouble(F64 v, U64 *res)	_CastFromD(U64)

//Cast to floats

#define CastTo(T) {															\
																			\
	if(!res)																\
		return Error_nullPointer(1);										\
																			\
	T r = (T) v;															\
																			\
	if(!T##_isValid(r))														\
		return Error_NaN(0);												\
																			\
	if(r != v)																\
		return Error_notFound(0, 0);										\
																			\
	*res = r;																\
	return Error_none();													\
}

Error F32_fromInt(I64 v, F32 *res)    CastTo(F32)
Error F32_fromUInt(U64 v, F32 *res)   CastTo(F32)
Error F32_fromDouble(F64 v, F32 *res) CastTo(F32)

Error F64_fromInt(I64 v, F64 *res)    CastTo(F64)
Error F64_fromUInt(U64 v, F64 *res)   CastTo(F64)
Error F32_fromFloat(F32 v, F64 *res)  CastTo(F64)

//Endianness, because sometimes it's needed

U16 U16_swapEndianness(U16 v) { return (v >> 8) | (v << 8); }
U32 U32_swapEndianness(U32 v) { return ((U32)U16_swapEndianness((U16)v) << 16) | U16_swapEndianness((U16)(v >> 16)); }
U64 U64_swapEndianness(U64 v) { return ((U64)U32_swapEndianness((U32)v) << 32) | U32_swapEndianness((U32)(v >> 32)); }

I16 I16_swapEndianness(I16 v) { return (I16) U16_swapEndianness((U16)v); }
I32 I32_swapEndianness(I32 v) { return (I32) U32_swapEndianness((U32)v); }
I64 I64_swapEndianness(I64 v) { return (I64) U64_swapEndianness((U64)v); }
