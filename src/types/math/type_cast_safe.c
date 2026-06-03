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

//types/math/type_cast_safe.c

#include "types/math/type_cast.h"
#include "types/base/constants.h"

//Cast to ints

#define CastFromU(type, toBits, ...)                                                                            \
																												\
	Bool s_uccess = true;                                                                                       \
																												\
	if(!res)                                                                                                    \
		retError(clean, Error_nullPointer(1, "CastFromU()::res is required"));                                  \
																												\
	__VA_ARGS__                                                                                                 \
																												\
	if(v > type##_MAX)                                                                                          \
		retError(clean, Error_overflow(0, toBits, (U64) type##_MAX, "CastFromU()::v out of bounds"));           \
																												\
	*res = (type) v;                                                                                            \
clean:                                                                                                          \
	return s_uccess

#define CastFromI(type, castType, toBits, ...) CastFromU(type, toBits, __VA_ARGS__                              \
	if(v < type##_MIN)                                                                                          \
		retError(clean, Error_underflow(0, toBits, (castType) type##_MIN, "CastFromI()::v is out of bounds"));  \
)

//We can't reuse CastFromU/CastFromI, since floats are a bit special:
// U32_MAX will round to U32_MAX + 1 with a float, so the > check will actually fail for 4Gi.
// This produces UB because (U32)f32_above_u32_max will depend on the arch.
// On x64 this seems to work as you expect (0) but on aarch64 this seems to clamp (U32_MAX).
// To prevent this issue, we instead compare it with >= (floatType)intLimit + 1.
// This results in either >= 0x100, 0x10000 and with
// float/int (due to float precision) it'll produce 4Gi + 1 = 4Gi so it'll work.

#define CastFromUForF(type, toBits, floatType, ...)                                                             \
																												\
	Bool s_uccess = true;                                                                                       \
																												\
	if(!res)                                                                                                    \
		retError(clean, Error_nullPointer(1, "CastFromUForF()::res is required"));                              \
																												\
	__VA_ARGS__                                                                                                 \
																												\
	if(v >= ((floatType)type##_MAX + 1))                                                                        \
		retError(clean, Error_overflow(0, toBits, (U64) type##_MAX, "CastFromUForF()::v out of bounds"));       \
																												\
	*res = (type) v;                                                                                            \
clean:                                                                                                          \
	return s_uccess

#define CastFromIForF(type, castType, toBits, floatType, ...)                                                   \
	CastFromUForF(type, toBits, floatType, __VA_ARGS__                                                          \
		if(v < type##_MIN)                                                                                      \
			retError(clean, Error_underflow(                                                                    \
				0, toBits, (castType) type##_MIN, "CastFromIForF()::v is out of bounds"                         \
			));                                                                                                 \
	)

#define CastFromF(type)                                                                                         \
	const void *vptr = &v;                                                                                      \
	CastFromIForF(type, type, *(const U32*)vptr, F32,                                                           \
		if(v != (type)v)                                                                                        \
			retError(clean, Error_notFound(0, 0, "CastTo()::res didn't properly resolve"));                     \
)

#define CastFromD(type)                                                                                         \
	const void *vptr = &v;                                                                                      \
	CastFromIForF(type, type, *(const U64*)vptr, F64,                                                           \
		if(v != (type)v)                                                                                        \
			retError(clean, Error_notFound(0, 0, "CastTo()::res didn't properly resolve"));                     \
)

#define ITOF(T, TUint)                                                                                          \
Bool T##_fromUInt(U64 v, T *res, Error *e_rr){                                                                  \
																												\
	Bool s_uccess = true;                                                                                       \
																												\
	if(!res)                                                                                                    \
		retError(clean, Error_nullPointer(1, #T "_fromUInt()::res is required"));                               \
																												\
	if(v > (U64)T##_MAX)                                                                                        \
		retError(clean, Error_overflow(0, (U64)v, (U64) T##_MAX, #T "_fromUInt()::v out of bounds"));           \
																												\
	*res = (T) v;                                                                                               \
clean:                                                                                                          \
	return s_uccess;                                                                                            \
}                                                                                                               \
Bool T##_fromInt(I64 v, T *res, Error *e_rr) { CastFromI(T, TUint, (U64)v); }                                   \
Bool T##_fromFloat(F32 v, T *res, Error *e_rr) { CastFromF(T); }                                                \
Bool T##_fromDouble(F64 v, T *res, Error *e_rr) { CastFromD(T); }

ITOF(I8, U8);
ITOF(I16, U16);
ITOF(I32, U32);

Bool I64_fromUInt(U64 v, I64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(1, "I64_fromUInt()::res is required"));

	if(v > (U64) I64_MAX)
		retError(clean, Error_overflow(0, (U64)v, (U64) I64_MAX, "I64_fromUInt() overflow"));

	*res = (I64)v;
clean:
	return s_uccess;
}

Bool I64_fromFloat(F32 v, I64 *res, Error *e_rr) { CastFromF(I64); }
Bool I64_fromDouble(F64 v, I64 *res, Error *e_rr) { CastFromD(I64); }

//Cast to uints

#define _UTOF(T)                                                                                                \
Bool T##_fromUInt(U64 v, T *res, Error *e_rr) { CastFromU(T, (U64)v); }                                         \
Bool T##_fromInt(I64 v, T *res, Error *e_rr) { CastFromI(T, T, (U64)v); }                                       \
Bool T##_fromFloat(F32 v, T *res, Error *e_rr) { CastFromF(T); }                                                \
Bool T##_fromDouble(F64 v, T *res, Error *e_rr) { CastFromD(T); }

_UTOF(U8);
_UTOF(U16);
_UTOF(U32);

Bool U64_fromInt(I64 v, U64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(1, "U64_fromInt()::res is required"));

	if(v < 0)
		retError(clean, Error_underflow(0, (U64)v, 0, "U64_fromInt() underflow"));

	*res = (U64) v;
clean:
	return s_uccess;
}

//Cast to floats

#define CastToFloatType(T, PrevType)                                                        \
																							\
	Bool s_uccess = true;                                                                   \
																							\
	if(!res)                                                                                \
		retError(clean, Error_nullPointer(1, "CastTo()::res is required"));                 \
																							\
	T r = (T) v;                                                                            \
																							\
	if(!T##_isValid(r))                                                                     \
		retError(clean, Error_NaN(0, "CastTo()::res truncated to NaN or Inf"));             \
																							\
	if((PrevType)r != v)                                                                    \
		retError(clean, Error_notFound(0, 0, "CastTo()::res didn't properly resolve"));     \
																							\
	*res = r;                                                                               \
clean:                                                                                      \
	return s_uccess

Bool F32_fromInt(I64 v, F32 *res, Error *e_rr) { CastToFloatType(F32, I64); }
Bool F32_fromUInt(U64 v, F32 *res, Error *e_rr) { CastToFloatType(F32, U64); }
Bool F32_fromDouble(F64 v, F32 *res, Error *e_rr) { CastToFloatType(F32, F64); }

Bool F64_fromInt(I64 v, F64 *res, Error *e_rr) { CastToFloatType(F64, I64); }
Bool F64_fromUInt(U64 v, F64 *res, Error *e_rr) { CastToFloatType(F64, U64); }
Bool F64_fromFloat(F32 v, F64 *res, Error *e_rr) { CastToFloatType(F64, F32); }
