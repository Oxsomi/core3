#pragma once
#include "math/math.h"
#include "error.h"

//Conversions

inline Error F32_fromBits(U64 v, F32 *res) {

	if(v > U32_MAX)
		return Error_overflow(0, 0, v, U32_MAX);

	U32 bits = (U32) v;
	F32 r = *(const F32*) &v;

	if(!F32_isValid(r))
		return Error_NaN(0);

	if(v == (1 << 31))	//Signed zero
		return Error_invalidParameter(0, 0, 0);

	*res = r;
	return Error_none();
}

//Cast to ints

#define CastFromU(type, ...) {										\
																	\
	__VA_ARGS__														\
																	\
	if(v > type##_MAX)												\
		return Error_overflow(0, 0, v, (U64) type##_MAX);			\
																	\
	*res = (type) v;												\
	return Error_none();											\
}																	\

#define CastFromI(type, castType, ...) CastFromU(type, __VA_ARGS__ 			\
	if(v < type##_MIN)														\
		return Error_underflow(0, 0, (castType) v, (castType) type##_MIN);	\
)

#define CastFromF(type) CastFromI(type, type, v = F32_floor(v);)

inline Error I8_fromUInt(U64 v, I8 *res)	CastFromU(I8)
inline Error I8_fromInt(I64 v, I8 *res)		CastFromI(I8, U8)
inline Error I8_fromFloat(F32 v, I8 *res)	CastFromF(I8)

inline Error I16_fromUInt(U64 v, I16 *res)	CastFromU(I16)
inline Error I16_fromInt(I64 v, I16 *res)	CastFromI(I16, U16)
inline Error I16_fromFloat(F32 v, I16 *res)	CastFromF(I16) 

inline Error I32_fromUInt(U64 v, I32 *res)	CastFromU(I32)
inline Error I32_fromInt(I64 v, I32 *res) 	CastFromI(I32, U32)
inline Error I32_fromFloat(F32 v, I32 *res)	CastFromF(I32)

inline Error I64_fromUInt(U64 v, I64 *res)	CastFromU(I64)
inline Error I64_fromFloat(F32 v, I64 *res)	CastFromF(I64)

//Cast to uints

inline Error U8_fromUInt(U64 v, U8 *res)	CastFromU(U8)
inline Error U8_fromInt(I64 v, U8 *res)		CastFromI(U8, U8)
inline Error U8_fromFloat(F32 v, U8 *res)	CastFromF(U8)

inline Error U16_fromUInt(U64 v, U16 *res)	CastFromU(U16)
inline Error U16_fromInt(I64 v, U16 *res) 	CastFromI(U16, U16)
inline Error U16_fromFloat(F32 v, U16 *res)	CastFromF(U16)

inline Error U32_fromUInt(U64 v, U32 *res)	CastFromU(U32)
inline Error U32_fromInt(I64 v, U32 *res)	CastFromI(U32, U32)
inline Error U32_fromFloat(F32 v, U32 *res)	CastFromF(U32)

inline Error U64_fromInt(I64 v, U64 *res) 	CastFromI(U64, U64)
inline Error U64_fromFloat(F32 v, U64 *res)  CastFromF(U64)

//Cast to floats

#define CastToF32 {															\
																			\
	F32 r = (F32) v;														\
																			\
	if(!F32_isValid(r))														\
		return Error_NaN(0);												\
																			\
	if(r != v)																\
		return Error_notFound(0, 0, 0);										\
																			\
	*res = r;																\
	return Error_none();													\
}

inline Error F32_fromInt(I64 v, F32 *res)  CastToF32
inline Error F32_fromUInt(U64 v, F32 *res) CastToF32
