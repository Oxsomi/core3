#pragma once
#include "types/types.h"
#include "types/error.h"

extern const F32 F32_e;
extern const F32 F32_pi;
extern const F32 F32_radToDeg;
extern const F32 F32_degToRad;

//Math errors assume inputs aren't nan or inf
//Ensure it's true with extra validation
//But math operations can't create them

//Uint

inline U64 U64_min(U64 v0, U64 v1) { return v0 <= v1 ? v0 : v1; }
inline U64 U64_max(U64 v0, U64 v1) { return v0 >= v1 ? v0 : v1; }
inline U64 U64_clamp(U64 v, U64 mi, U64 ma) { return U64_max(mi, U64_min(ma, v)); }

inline U64 U64_pow2(U64 v) {
	if(!v) return 0;
	U64 res = v * v; 
	return res / v != v ? U64_MAX : res;
}

inline U64 U64_pow3(U64 v) {
	if(!v) return 0;
	U64 res = U64_pow2(v), res2 = res * v;
	return res == U64_MAX || res2 / v != v ? U64_MAX : res2;
}

inline U64 U64_pow4(U64 v) {
	if(!v) return 0;
	U64 res = U64_pow2(v), res2 = U64_pow2(res);
	return res == U64_MAX || res2 == U64_MAX ? U64_MAX : res2;
}

inline U64 U64_pow5(U64 v) {
	if(!v) return 0;
	U64 res = U64_pow4(v), res2 = res * v;
	return res == U64_MAX || res2 / v != v ? U64_MAX : res2;
}

inline U64 U64_pow10(U64 v) {
	switch (v) {
		case 0:		return 1;
		case 1:		return 10;
		case 2:		return 100;
		case 3:		return 1'000;
		case 4:		return 10'000;
		case 5:		return 100'000;
		case 6:		return 1'000'000;
		case 7:		return 10'000'000;
		case 8:		return 100'000'000;
		case 9:		return 1'000'000'000;
		case 10:	return 10'000'000'000;
		case 11:	return 100'000'000'000;
		case 12:	return 1'000'000'000'000;
		case 13:	return 10'000'000'000'000;
		case 14:	return 100'000'000'000'000;
		case 15:	return 1'000'000'000'000'000;
		case 16:	return 10'000'000'000'000'000;
		case 17:	return 100'000'000'000'000'000;
		case 18:	return 1'000'000'000'000'000'000;
		case 19:	return 10'000'000'000'000'000'000;
		default:	return U64_MAX;
	}
}

//TODO: Int, uint %/^*+-

//Int

inline I64 I64_min(I64 v0, I64 v1) { return v0 <= v1 ? v0 : v1; }
inline I64 I64_max(I64 v0, I64 v1) { return v0 >= v1 ? v0 : v1; }
inline I64 I64_clamp(I64 v, I64 mi, I64 ma) { return I64_max(mi, I64_min(ma, v)); }

inline I64 I64_abs(I64 v) { return v < 0 ? -v : v; }

//TODO: proper safety checks, because this doesn't properly check overflow!

inline I64 I64_pow2(I64 v) { 
	I64 res = v * v; 
	return res < I64_abs(v) ? I64_MAX : res;
}

inline I64 I64_pow3(I64 v) { 
	I64 res = v * v * v; 
	return res < I64_abs(v) ? I64_MAX : res;
}

inline I64 I64_pow4(I64 v) { 
	I64 res = v * v; res *= res;
	return res < I64_abs(v) ? I64_MAX : res;
}

inline I64 I64_pow5(I64 v) { 
	I64 res = v * v; res *= res * v;
	return res < I64_abs(v) ? I64_MAX : res;
}

//Float
//TODO: %/^+-
//		Should also check if the ++ and -- actually increased the float. If not, throw! +- etc can check on lost precision (e.g. 1% of value)
//TODO: Proper error checking!

inline F32 F32_min(F32 v0, F32 v1) { return v0 <= v1 ? v0 : v1; }
inline F32 F32_max(F32 v0, F32 v1) { return v0 >= v1 ? v0 : v1; }
inline F32 F32_clamp(F32 v, F32 mi, F32 ma) { return F32_max(mi, F32_min(ma, v)); }
inline F32 F32_saturate(F32 v) { return F32_clamp(v, 0, 1); }

//TODO: Lerp perc should be 0,1

inline F32 F32_lerp(F32 a, F32 b, F32 perc) { return a + (b - a) * perc; }
inline F32 F32_abs(F32 v) { return v < 0 ? -v : v; }
F32 F32_sqrt(F32 v);

//Inputs should always return false for the runtime, since floats should be validated before they're made
//As such, these won't be exposed to any user, only natively

Bool F32_isNaN(F32 v);
Bool F32_isInf(F32 v);
Bool F32_isValid(F32 v);

//

inline Error F32_pow2(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v; 
	return F32_isInf(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

inline Error F32_pow3(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v * v;
	return F32_isInf(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

inline Error F32_pow4(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v; *res *= *res;
	return F32_isInf(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

inline Error F32_pow5(F32 v, F32 *res) { 

	if(!res)
		return Error_nullPointer(1, 0);

	*res = v * v; *res *= *res * v;
	return F32_isInf(*res) ? Error_overflow(0, 0, *(const U32*)&v, *(const U32*)res) : 
		Error_none();
}

Error F32_pow(F32 v, F32 exp, F32 *res);
Error F32_expe(F32 v, F32 *res);
Error F32_exp2(F32 v, F32 *res);
Error F32_exp10(F32 v, F32 *res);

F32 F32_log10(F32 v);
F32 F32_loge(F32 v);
F32 F32_log2(F32 v);

F32 F32_asin(F32 v);
F32 F32_sin(F32 v);
F32 F32_cos(F32 v);
F32 F32_acos(F32 v);
F32 F32_tan(F32 v);
F32 F32_atan(F32 v);
F32 F32_atan2(F32 y, F32 x);

F32 F32_round(F32 v);
F32 F32_ceil(F32 v);
F32 F32_floor(F32 v);
inline F32 F32_fract(F32 v) { return v - F32_floor(v); }

Error F32_mod(F32 v, F32 mod, F32 *result);

inline F32 F32_sign(F32 v) { return v < 0 ? -1.f : (v > 0 ? 1.f : 0.f); }
inline F32 F32_signInc(F32 v) { return v < 0 ? -1.f : 1.f; }
