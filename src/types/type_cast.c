/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

U64 F32_fromBits(F32 v) {
	U32 u = *(const U32*)&v;
	return u;
}

U64 F64_fromBits(F64 v) { return *(const U64*) &v; }

#define FLP_FROMBITS(T, TInt)																			\
Error T##_from##T##Bits(U64 v, T *res) {																\
																										\
	if(!res)																							\
		return Error_nullPointer(1, #T "_fromBits()::res is required");									\
																										\
	if(v > TInt##_MAX)																					\
		return Error_overflow(0, v, TInt##_MAX, #T "_fromBits()::v is out of bounds");					\
																										\
	TInt bits = (TInt) v;																				\
	T r = *(const T*) &bits;																			\
																										\
	if(!T##_isValid(r))																					\
		return Error_NaN(0, #T "_fromBits()::v generated NaN or Inf");									\
																										\
	if(v == ((TInt)1 << (sizeof(T) * 8 - 1)))	/* Signed zero */										\
		r = 0;																							\
																										\
	*res = r;																							\
	return Error_none();																				\
}

FLP_FROMBITS(F32, U32);
FLP_FROMBITS(F64, U64);

//Cast to ints

#define CastFromU(type, toBits, ...) {																	\
																										\
	if(!res)																							\
		return Error_nullPointer(1, "CastFromU()::res is required");									\
																										\
	__VA_ARGS__																							\
																										\
	if(v > type##_MAX)																					\
		return Error_overflow(0, toBits, (U64) type##_MAX, "CastFromU()::v out of bounds");				\
																										\
	*res = (type) v;																					\
	return Error_none();																				\
}

#define CastFromI(type, castType, toBits, ...) CastFromU(type, toBits, __VA_ARGS__ 						\
	if(v < type##_MIN)																					\
		return Error_underflow(0, toBits, (castType) type##_MIN, "CastFromI()::v is out of bounds");	\
)

#define CastFromF(type) CastFromI(type, type, *(const U32*)&v)
#define CastFromD(type) CastFromI(type, type, *(const U64*)&v)

#define ITOF(T, TUint)																				\
Error T##_fromUInt(U64 v, T *res){																		\
																										\
	if(!res)																							\
		return Error_nullPointer(1, #T "_fromUInt()::res is required");									\
																										\
	if(v > (U64)T##_MAX)																				\
		return Error_overflow(0, (U64)v, (U64) T##_MAX, #T "_fromUInt()::v out of bounds");				\
																										\
	*res = (T) v;																						\
	return Error_none();																				\
}																										\
Error T##_fromInt(I64 v, T *res)		CastFromI(T, TUint, (U64)v)										\
Error T##_fromFloat(F32 v, T *res)		CastFromF(T) 													\
Error T##_fromDouble(F64 v, T *res)		CastFromD(T)

ITOF(I8, U8);
ITOF(I16, U16);
ITOF(I32, U32);

Error I64_fromUInt(U64 v, I64 *res) {

	if(!res)
		return Error_nullPointer(1, "I64_fromUInt()::res is required");

	if(v > (U64) I64_MAX)
		return Error_overflow(0, (U64)v, (U64) I64_MAX, "I64_fromUInt() overflow");

	return Error_none();
}

Error I64_fromFloat(F32 v, I64 *res)	CastFromF(I64)
Error I64_fromDouble(F64 v, I64 *res)	CastFromD(I64)

//Cast to uints

#define _UTOF(T)																			\
Error T##_fromUInt(U64 v, T *res)		CastFromU(T, (U64)v)								\
Error T##_fromInt(I64 v, T *res)		CastFromI(T, T, (U64)v)							\
Error T##_fromFloat(F32 v, T *res)		CastFromF(T) 										\
Error T##_fromDouble(F64 v, T *res)		CastFromD(T)

_UTOF(U8);
_UTOF(U16);
_UTOF(U32);

Error U64_fromInt(I64 v, U64 *res) {

	if(!res)
		return Error_nullPointer(1, "U64_fromInt()::res is required");

	if(v < 0)
		return Error_underflow(0, (U64)v, 0, "U64_fromInt() underflow");

	return Error_none();
}


Error U64_fromFloat(F32 v, U64 *res)	CastFromF(U64)
Error U64_fromDouble(F64 v, U64 *res)	CastFromD(U64)

//Cast to floats

#define CastTo(T) {																			\
																							\
	if(!res)																				\
		return Error_nullPointer(1, "CastTo()::res is required");							\
																							\
	T r = (T) v;																			\
																							\
	if(!T##_isValid(r))																		\
		return Error_NaN(0, "CastTo()::res truncated to NaN or Inf");						\
																							\
	if(r != v)																				\
		return Error_notFound(0, 0, "CastTo()::res didn't properly resolve");				\
																							\
	*res = r;																				\
	return Error_none();																	\
}

Error F32_fromInt(I64 v, F32 *res)    CastTo(F32)
Error F32_fromUInt(U64 v, F32 *res)   CastTo(F32)
Error F32_fromDouble(F64 v, F32 *res) CastTo(F32)

Error F64_fromInt(I64 v, F64 *res)    CastTo(F64)
Error F64_fromUInt(U64 v, F64 *res)   CastTo(F64)
Error F64_fromFloat(F32 v, F64 *res)  CastTo(F64)

//Endianness, because sometimes it's needed

U16 U16_swapEndianness(U16 v) { return (v >> 8) | (v << 8); }
U32 U32_swapEndianness(U32 v) { return ((U32)U16_swapEndianness((U16)v) << 16) | U16_swapEndianness((U16)(v >> 16)); }
U64 U64_swapEndianness(U64 v) { return ((U64)U32_swapEndianness((U32)v) << 32) | U32_swapEndianness((U32)(v >> 32)); }

I16 I16_swapEndianness(I16 v) { return (I16) U16_swapEndianness((U16)v); }
I32 I32_swapEndianness(I32 v) { return (I32) U32_swapEndianness((U32)v); }
I64 I64_swapEndianness(I64 v) { return (I64) U64_swapEndianness((U64)v); }
