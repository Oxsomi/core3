#include "math/math.h"
#include <math.h>

struct Error Math_powf(f32 v, f32 exp, f32 *res) { 

	f32 r = powf(v, exp); 

	if(Math_isnan(r) || Math_isinf(r))
		return Error_base(
			GenericError_Overflow, 0,
			0, 0, 
			*(const u32*) &v, *(const u32*) &exp
		);

	*res = r;
	return Error_none();
}

//TODO: Error check this

f32 Math_expef(f32 v) { return powf(Math_e, v); }
f32 Math_exp2f(f32 v) { return powf(2, v); }
f32 Math_exp10f(f32 v) { return powf(10, v); }

f32 Math_sqrtf(f32 v) { return sqrtf(v); }

f32 Math_log10f(f32 v) { return log10f(v); }
f32 Math_logef(f32 v) { return logf(v); }
f32 Math_log2f(f32 v) { return log2f(v); }

f32 Math_asin(f32 v) { return asinf(v); }
f32 Math_sin(f32 v) { return sinf(v); }
f32 Math_cos(f32 v) { return cosf(v); }
f32 Math_acos(f32 v) { return acosf(v); }
f32 Math_tan(f32 v) { return tanf(v); }
f32 Math_atan(f32 v) { return atanf(v); }
f32 Math_atan2(f32 y, f32 x) { return atan2f(y, x); }

f32 Math_round(f32 v) { return roundf(v); }
f32 Math_ceil(f32 v) { return ceilf(v); }
f32 Math_floor(f32 v) { return floorf(v); }

struct Error Math_mod(f32 v, f32 mod, f32 *res) { 

	if(!mod)
		return Error_base(
			GenericError_DivideByZero,
			0, 0, 0,
			*(const u32*) &v, *(const u32*) &mod
		);

	f32 r = fmodf(v, mod); 

	if(Math_isinf(r) || Math_isnan(r))
		return Error_base(
			GenericError_Overflow,
			0, 0, 0,
			*(const u32*) &v, *(const u32*) &mod
		);

	*res = r;
	return Error_none();
}

bool Math_isnan(f32 v) { return isnan(v); }
bool Math_isinf(f32 v) { return isinf(v); }

//Conversions

struct Error Math_f32FromBits(u64 v, f32 *res) {

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

struct Error Math_i8FromUInt(u64 v, i64 *res) {

	if(v > i8_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i8FromInt(i64 v, i64 *res) {

	if(v > i8_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	if(v < i8_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i8FromFloat(f32 v, i64 *res) {

	v = Math_floor(v);

	if(v > i8_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < i8_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i16FromUInt(u64 v, i64 *res) {

	if(v > i16_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i16FromInt(i64 v, i64 *res) {

	if(v > i16_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	if(v < i16_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i16FromFloat(f32 v, i64 *res) {

	v = Math_floor(v);

	if(v > i16_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < i16_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i32FromUInt(u64 v, i64 *res) {

	if(v > i32_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i32FromInt(i64 v, i64 *res) {

	if(v > i32_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	if(v < i32_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i32FromFloat(f32 v, i64 *res) {

	v = Math_floor(v);

	if(v > i32_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < i32_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i64FromUInt(u64 v, i64 *res) {

	if(v > i64_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (i64) v;
	return Error_none();
}

struct Error Math_i64FromFloat(f32 v, i64 *res) {

	v = Math_floor(v);

	if(v > i64_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < i64_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (i64) v;
	return Error_none();
}

//Cast to uints

struct Error Math_u8FromUInt(u64 v, u64 *res) {

	if(v > u8_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u8FromInt(i64 v, u64 *res) {

	if(v > u8_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	if(v < u8_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u8FromFloat(f32 v, u64 *res) {

	v = Math_floor(v);

	if(v > u8_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < u8_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u16FromUInt(u64 v, u64 *res) {

	if(v > u16_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u16FromInt(i64 v, u64 *res) {

	if(v > u16_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	if(v < u16_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u16FromFloat(f32 v, u64 *res) {

	v = Math_floor(v);

	if(v > u16_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < u16_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u32FromUInt(u64 v, u64 *res) {

	if(v > u32_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u32FromInt(i64 v, u64 *res) {

	if(v > u32_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	if(v < u32_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u32FromFloat(f32 v, u64 *res) {

	v = Math_floor(v);

	if(v > u32_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < u32_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u64FromUInt(u64 v, u64 *res) {

	if(v > u64_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, v, 0);

	*res = (u64) v;
	return Error_none();
}

struct Error Math_u64FromFloat(f32 v, u64 *res) {

	v = Math_floor(v);

	if(v > u64_MAX)
		return Error_base(GenericError_Overflow, 0, 0, 0, *(const u32*)&v, 0);

	if(v < u64_MIN)
		return Error_base(GenericError_Underflow, 0, 0, 0, *(const u32*)&v, 0);

	*res = (u64) v;
	return Error_none();
}

//Cast to floats

struct Error Math_f32FromInt(i64 v, f32 *res) {

	f32 r = (f32) v;

	if(Math_isinf(r))
		return Error_base(
			v >= 0 ? GenericError_Overflow : GenericError_Underflow, 0, 
			0, 0, v, 0
		);

	if(r != v)
		return Error_base(
			GenericError_NotFound, 0, 
			0, 0, v, 0
		);

	*res = r;
	return Error_none();
}

struct Error Math_f32FromUInt(u64 v, f32 *res) {

	f32 r = (f32) v;

	if(Math_isinf(r))
		return Error_base(
			GenericError_Overflow, 0, 
			0, 0, v, 0
		);

	if(r != v)
		return Error_base(
			GenericError_NotFound, 0, 
			0, 0, v, 0
		);

	*res = r;
	return Error_none();
}
