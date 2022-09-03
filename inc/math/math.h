#pragma once
#include "types/types.h"
#include "types/error.h"

extern const f32 Math_e;
extern const f32 Math_pi;
extern const f32 Math_radToDeg;
extern const f32 Math_degToRad;

//Math errors assume inputs aren't nan or inf
//Ensure it's true with extra validation
//But math operations can't create them

//Uint

inline u64 Math_minu(u64 v0, u64 v1) { return v0 <= v1 ? v0 : v1; }
inline u64 Math_maxu(u64 v0, u64 v1) { return v0 >= v1 ? v0 : v1; }
inline u64 Math_clampu(u64 v, u64 mi, u64 ma) { return Math_maxu(mi, Math_minu(ma, v)); }

inline struct Error Math_pow2u(u64 *res, u64 v) {

	if(!res)
		return Error_base(GenericError_NullPointer, 0, 0, 0, 0, 0);

	*res = v * v; 

	if(*res < v)
		return Error_base(GenericError_Overflow, 0, 1, 0, v, *res);

	return Error_none();
}

//TODO: Int, uint %/^*+-

//Int

inline i64 Math_mini(i64 v0, i64 v1) { return v0 <= v1 ? v0 : v1; }
inline i64 Math_maxi(i64 v0, i64 v1) { return v0 >= v1 ? v0 : v1; }
inline i64 Math_clampi(i64 v, i64 mi, i64 ma) { return Math_maxi(mi, Math_mini(ma, v)); }

inline i64 Math_absi(i64 v) { return v < 0 ? -v : v; }

inline struct Error Math_pow2i(i64 v, i64 *res) { 

	if(!res)
		return Error_base(GenericError_NullPointer, 0, 0, 0, 0, 0);

	*res = v * v; 

	if(*res < Math_absi(v))
		return Error_base(GenericError_Overflow, 0, 1, 0, v, *res);

	return Error_none();
}

//Float
//TODO: %/^+-
//		Should also check if the ++ and -- actually increased the float. If not, throw!
//TODO: Proper error checking!

inline f32 Math_minf(f32 v0, f32 v1) { return v0 <= v1 ? v0 : v1; }
inline f32 Math_maxf(f32 v0, f32 v1) { return v0 >= v1 ? v0 : v1; }
inline f32 Math_clampf(f32 v, f32 mi, f32 ma) { return Math_maxf(mi, Math_minf(ma, v)); }
inline f32 Math_saturate(f32 v) { return Math_clampf(v, 0, 1); }

//TODO: Lerp perc should be 0,1

inline f32 Math_lerp(f32 a, f32 b, f32 perc) { return a + (b - a) * perc; }
inline f32 Math_absf(f32 v) { return v < 0 ? -v : v; }
f32 Math_sqrtf(f32 v);

//Inputs should always return false for the runtime, since floats should be validated before they're made
//As such, these won't be exposed to any user, only natively

bool Math_isnan(f32 v);
bool Math_isinf(f32 v);

//

inline struct Error Math_pow2f(f32 v, f32 *res) { 

	if(!res)
		return Error_base(GenericError_NullPointer, 0, 0, 0, 0, 0);

	*res = v * v; 

	if(Math_isinf(*res))
		return Error_base(GenericError_Overflow, 0, 1, 0, v, *res);

	return Error_none();
}

struct Error Math_powf(f32 v, f32 exp, f32 *res);
f32 Math_expef(f32 v);
f32 Math_exp2f(f32 v);
f32 Math_exp10f(f32 v);

f32 Math_log10f(f32 v);
f32 Math_logef(f32 v);
f32 Math_log2f(f32 v);

f32 Math_asin(f32 v);
f32 Math_sin(f32 v);
f32 Math_cos(f32 v);
f32 Math_acos(f32 v);
f32 Math_tan(f32 v);
f32 Math_atan(f32 v);
f32 Math_atan2(f32 y, f32 x);

f32 Math_round(f32 v);
f32 Math_ceil(f32 v);
f32 Math_floor(f32 v);
inline f32 Math_fract(f32 v) { return v - Math_floor(v); }

struct Error Math_mod(f32 v, f32 mod, f32 *result);

inline f32 Math_sign(f32 v) { return v < 0 ? -1.f : (v > 0 ? 1.f : 0.f); }
inline f32 Math_signInc(f32 v) { return v < 0 ? -1.f : 1.f; }

//Conversions between types

struct Error Math_f32FromBits(u64 v, f32 *res);

struct Error Math_i8FromUInt(u64 v, i64 *res);
struct Error Math_i8FromInt(i64 v, i64 *res);
struct Error Math_i8FromFloat(f32 v, i64 *res);

struct Error Math_i16FromUInt(u64 v, i64 *res);
struct Error Math_i16FromInt(i64 v, i64 *res);
struct Error Math_i16FromFloat(f32 v, i64 *res);

struct Error Math_i32FromUInt(u64 v, i64 *res);
struct Error Math_i32FromInt(i64 v, i64 *res);
struct Error Math_i32FromFloat(f32 v, i64 *res);

struct Error Math_i64FromUInt(u64 v, i64 *res);
struct Error Math_i64FromFloat(f32 v, i64 *res);

struct Error Math_u8FromUInt(u64 v, u64 *res);
struct Error Math_u8FromInt(i64 v, u64 *res);
struct Error Math_u8FromFloat(f32 v, u64 *res);

struct Error Math_u16FromUInt(u64 v, u64 *res);
struct Error Math_u16FromInt(i64 v, u64 *res);
struct Error Math_u16FromFloat(f32 v, u64 *res);

struct Error Math_u32FromUInt(u64 v, u64 *res);
struct Error Math_u32FromInt(i64 v, u64 *res);
struct Error Math_u32FromFloat(f32 v, u64 *res);

struct Error Math_u64FromInt(i64 v, u64 *res);
struct Error Math_u64FromFloat(f32 v, u64 *res);

struct Error Math_f32FromInt(i64 v, f32 *res);
struct Error Math_f32FromUInt(u64 v, f32 *res);
