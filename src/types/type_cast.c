#include "types/type_cast.h"

//Conversions

Error F32_fromBits(U64 v, F32 *res) {

	if(v > U32_MAX)
		return Error_overflow(0, 0, v, U32_MAX);

	U32 bits = (U32) v;
	F32 r = *(const F32*) &bits;

	if(!F32_isValid(r))
		return Error_NaN(0);

	if(v == (1 << 31))	//Signed zero
		return Error_invalidParameter(0, 0, 0);

	*res = r;
	return Error_none();
}

//Cast to ints

#define _CastFromU(type, toBits, ...) {											\
																				\
	if(!res)																	\
		return Error_nullPointer(1, 0);											\
																				\
	__VA_ARGS__																	\
																				\
	if(v > type##_MAX)															\
		return Error_overflow(0, 0, toBits, (U64) type##_MAX);					\
																				\
	*res = (type) v;															\
	return Error_none();														\
}

#define _CastFromI(type, castType, toBits, ...) _CastFromU(type, toBits, __VA_ARGS__ 		\
	if(v < type##_MIN)																		\
		return Error_underflow(0, 0, toBits, (castType) type##_MIN);						\
)

#define _CastFromF(type) _CastFromI(type, type, *(const U32*)&v)

Error I8_fromUInt(U64 v, I8 *res)		_CastFromU(I8, (U64)v)
Error I8_fromInt(I64 v, I8 *res)		_CastFromI(I8, U8, (U64)(I64)v)
Error I8_fromFloat(F32 v, I8 *res)		_CastFromF(I8) 

Error I16_fromUInt(U64 v, I16 *res)		_CastFromU(I16, (U64)v)
Error I16_fromInt(I64 v, I16 *res)		_CastFromI(I16, U16, (U64)(I64)v)
Error I16_fromFloat(F32 v, I16 *res)	_CastFromF(I16) 

Error I32_fromUInt(U64 v, I32 *res)		_CastFromU(I32, (U64)v)
Error I32_fromInt(I64 v, I32 *res) 		_CastFromI(I32, U32, (U64)(I64)v)
Error I32_fromFloat(F32 v, I32 *res)	_CastFromF(I32)

Error I64_fromUInt(U64 v, I64 *res) {

	if(!res)
		return Error_nullPointer(1, 0);

	if(v > (U64) I64_MAX)
		return Error_overflow(0, 0, (U64)v, (U64) I64_MAX);

	return Error_none();
}

Error I64_fromFloat(F32 v, I64 *res)	_CastFromF(I64)

//Cast to uints

Error U8_fromUInt(U64 v, U8 *res)		_CastFromU(U8, (U64)v)
Error U8_fromInt(I64 v, U8 *res)		_CastFromI(U8, U8, (U64)(I64)v)
Error U8_fromFloat(F32 v, U8 *res)		_CastFromF(U8)

Error U16_fromUInt(U64 v, U16 *res)		_CastFromU(U16, (U64)v)
Error U16_fromInt(I64 v, U16 *res) 		_CastFromI(U16, U16, (U64)(I64)v)
Error U16_fromFloat(F32 v, U16 *res)	_CastFromF(U16)

Error U32_fromUInt(U64 v, U32 *res)		_CastFromU(U32, (U64)v)
Error U32_fromInt(I64 v, U32 *res)		_CastFromI(U32, U32, (U64)(I64)v)
Error U32_fromFloat(F32 v, U32 *res)	_CastFromF(U32)

Error U64_fromInt(I64 v, U64 *res) {

	if(!res)
		return Error_nullPointer(1, 0);

	if(v < (I64) U64_MAX)
		return Error_underflow(0, 0, (U64)v, 0);

	return Error_none();
}


Error U64_fromFloat(F32 v, U64 *res)	_CastFromF(U64)

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

Error F32_fromInt(I64 v, F32 *res)  CastToF32
Error F32_fromUInt(U64 v, F32 *res) CastToF32
