#pragma once
#include "types/types.h"
#include "types/error.h"

extern const f32 f32_e;
extern const f32 f32_pi;
extern const f32 f32_radToDeg;
extern const f32 f32_degToRad;

//Math errors assume inputs aren't nan or inf
//Ensure it's true with extra validation
//But math operations can't create them

//Uint

inline u64 u64_min(u64 v0, u64 v1) { return v0 <= v1 ? v0 : v1; }
inline u64 u64_max(u64 v0, u64 v1) { return v0 >= v1 ? v0 : v1; }
inline u64 u64_clamp(u64 v, u64 mi, u64 ma) { return u64_max(mi, u64_min(ma, v)); }

inline struct Error u64_pow2(u64 *res, u64 v) {

	if(!res)
		return Error_base(GenericError_NullPointer, 0, 0, 0, 0, 0);

	*res = v * v; 

	if(*res < v)
		return Error_base(GenericError_Overflow, 0, 1, 0, v, *res);

	return Error_none();
}

//TODO: Int, uint %/^*+-

//Int

inline i64 i64_min(i64 v0, i64 v1) { return v0 <= v1 ? v0 : v1; }
inline i64 i64_max(i64 v0, i64 v1) { return v0 >= v1 ? v0 : v1; }
inline i64 i64_clamp(i64 v, i64 mi, i64 ma) { return i64_max(mi, i64_min(ma, v)); }

inline i64 i64_abs(i64 v) { return v < 0 ? -v : v; }

inline struct Error i64_pow2(i64 v, i64 *res) { 

	if(!res)
		return Error_base(GenericError_NullPointer, 0, 0, 0, 0, 0);

	*res = v * v; 

	if(*res < i64_abs(v))
		return Error_base(GenericError_Overflow, 0, 1, 0, v, *res);

	return Error_none();
}

//Float
//TODO: %/^+-
//		Should also check if the ++ and -- actually increased the float. If not, throw! +- etc can check on lost precision (e.g. 1% of value)
//TODO: Proper error checking!

inline f32 f32_min(f32 v0, f32 v1) { return v0 <= v1 ? v0 : v1; }
inline f32 f32_max(f32 v0, f32 v1) { return v0 >= v1 ? v0 : v1; }
inline f32 f32_clamp(f32 v, f32 mi, f32 ma) { return f32_max(mi, f32_min(ma, v)); }
inline f32 f32_saturate(f32 v) { return f32_clamp(v, 0, 1); }

//TODO: Lerp perc should be 0,1

inline f32 f32_lerp(f32 a, f32 b, f32 perc) { return a + (b - a) * perc; }
inline f32 f32_abs(f32 v) { return v < 0 ? -v : v; }
f32 f32_sqrt(f32 v);

//Inputs should always return false for the runtime, since floats should be validated before they're made
//As such, these won't be exposed to any user, only natively

bool f32_isnan(f32 v);
bool f32_isinf(f32 v);

//

inline struct Error f32_pow2(f32 v, f32 *res) { 

	if(!res)
		return Error_base(GenericError_NullPointer, 0, 0, 0, 0, 0);

	*res = v * v; 

	if(f32_isinf(*res))
		return Error_base(GenericError_Overflow, 0, 1, 0, v, *res);

	return Error_none();
}

struct Error f32_pow(f32 v, f32 exp, f32 *res);
f32 f32_expe(f32 v);
f32 f32_exp2(f32 v);
f32 f32_exp10(f32 v);

f32 f32_log10(f32 v);
f32 f32_loge(f32 v);
f32 f32_log2(f32 v);

f32 f32_asin(f32 v);
f32 f32_sin(f32 v);
f32 f32_cos(f32 v);
f32 f32_acos(f32 v);
f32 f32_tan(f32 v);
f32 f32_atan(f32 v);
f32 f32_atan2(f32 y, f32 x);

f32 f32_round(f32 v);
f32 f32_ceil(f32 v);
f32 f32_floor(f32 v);
inline f32 f32_fract(f32 v) { return v - f32_floor(v); }

struct Error f32_mod(f32 v, f32 mod, f32 *result);

inline f32 f32_sign(f32 v) { return v < 0 ? -1.f : (v > 0 ? 1.f : 0.f); }
inline f32 f32_signInc(f32 v) { return v < 0 ? -1.f : 1.f; }

bool f32_isnan(f32 v);
bool f32_isinf(f32 v);
