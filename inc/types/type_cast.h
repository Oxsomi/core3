#pragma once
#include "types.h"
#include "error.h"

//Conversions

inline struct Error f32_fromBits(u64 v, f32 *res) {

	if(v > u32_MAX)
		return Error_base(GenericError_OutOfBounds, 0, 0, 0, v, 0);

	u32 bits = (u32) v;
	f32 r = *(const f32*) &v;

	if(Math_isnan(r))
		return Error_base(GenericError_NaN, 0, 0, 0, v, 0);

	if(Math_isinf(r))
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	if(v == (1 << 31))	//Signed zero
		return Error_base(GenericError_InvalidParameter, 0, 0, 0, v, 0);

	*res = r;
	return Error_none();
}

//Cast to ints

#define CastFromU(type, ...) {										\
																	\
	__VA_ARGS__														\
																	\
	if(v > type##_MAX)												\
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);	\
																	\
	*res = (type) v;												\
	return Error_none();											\
}																	\

#define CastFromI(type, ...) CastFromU(type, __VA_ARGS__ 			\
	if(v < type##_MIN)												\
		return Error_base(GenericError_Underflow, 0, 0, 0, v, 0);	\
)

#define CastFromF(type) CastFromI(type, v = Math_floor(v);)

inline struct Error i8_fromUInt(u64 v, i8 *res)		CastFromU(i8)
inline struct Error i8_fromInt(i64 v, i8 *res)		CastFromI(i8)
inline struct Error i8_fromFloat(f32 v, i8 *res)	CastFromF(i8)

inline struct Error i16_fromUInt(u64 v, i16 *res)	CastFromU(i16)
inline struct Error i16_fromInt(i64 v, i16 *res)	CastFromI(i16)
inline struct Error i16_fromFloat(f32 v, i16 *res)	CastFromF(i16) 

inline struct Error i32_fromUInt(u64 v, i32 *res)	CastFromU(i32)
inline struct Error i32_fromInt(i64 v, i32 *res) 	CastFromI(i32)
inline struct Error i32_fromFloat(f32 v, i32 *res)	CastFromF(i32)

inline struct Error i64_fromUInt(u64 v, i64 *res)	CastFromU(i64)
inline struct Error i64_fromFloat(f32 v, i64 *res)	CastFromF(i64)

//Cast to uints

inline struct Error u8_fromUInt(u64 v, u8 *res)		CastFromU(u8)
inline struct Error u8_fromInt(i64 v, u8 *res)		CastFromI(u8)
inline struct Error u8_fromFloat(f32 v, u8 *res)	CastFromF(u8)

inline struct Error u16_fromUInt(u64 v, u16 *res)	CastFromU(u16)
inline struct Error u16_fromInt(i64 v, u16 *res) 	CastFromI(u16)
inline struct Error u16_fromFloat(f32 v, u16 *res)	CastFromF(u16)

inline struct Error u32_fromUInt(u64 v, u32 *res)	CastFromU(u32)
inline struct Error u32_fromInt(i64 v, u32 *res)	CastFromI(u32)
inline struct Error u32_fromFloat(f32 v, u32 *res)	CastFromF(u32)

inline struct Error u64_fromInt(i64 v, u64 *res) 	CastFromI(u64)
inline struct Error u64_fromFloat(f32 v, u64 *res)  CastFromF(u64)

//Cast to floats

#define CastToF32 {															\
																			\
	f32 r = (f32) v;														\
																			\
	if(Math_isinf(r))														\
		return Error_base(													\
			v >= 0 ? GenericError_Overflow : GenericError_Underflow, 0,		\
			0, 0, v, 0														\
		);																	\
																			\
	if(r != v)																\
		return Error_base(													\
			GenericError_NotFound, 0, 										\
			0, 0, v, 0														\
		);																	\
																			\
	*res = r;																\
	return Error_none();													\
}

inline struct Error f32_fromInt(i64 v, f32 *res)  CastToF32
inline struct Error f32_fromUInt(u64 v, f32 *res) CastToF32