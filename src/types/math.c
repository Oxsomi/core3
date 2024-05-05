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

#include "types/math.h"
#include "types/error.h"

#include <math.h>

#define XINT_OP_IMPL(T)													\
T T##_min(T v0, T v1) { return v0 <= v1 ? v0 : v1; }					\
T T##_max(T v0, T v1) { return v0 >= v1 ? v0 : v1; }					\
T T##_clamp(T v, T mi, T ma) { return T##_max(mi, T##_min(ma, v)); }

const U64 U64_EXP10[] = {
	1,
	10,
	100,
	1000,
	10000,
	100000,
	1000000,
	10000000,
	100000000,
	1000000000,
	10000000000,
	100000000000,
	1000000000000,
	10000000000000,
	100000000000000,
	1000000000000000,
	10000000000000000,
	100000000000000000,
	1000000000000000000,
	UINT64_C(10000000000000000000)
};

//UInts

#define UINT_OP_IMPL(T)													\
XINT_OP_IMPL(T);														\
																		\
T T##_pow2(T v) {														\
	if(!v) return 0;													\
	T res = v * v; 														\
	return res / v != v ? T##_MAX : res;								\
}																		\
																		\
T T##_pow3(T v) {														\
	if(!v) return 0;													\
	T res = T##_pow2(v), res2 = res * v;								\
	return res == T##_MAX || res2 / v != res ? T##_MAX : res2;			\
}																		\
																		\
T T##_pow4(T v) {														\
	if(!v) return 0;													\
	T res = T##_pow2(v), res2 = T##_pow2(res);							\
	return res == T##_MAX || res2 == T##_MAX ? T##_MAX : res2;			\
}																		\
																		\
T T##_pow5(T v) {														\
	if(!v) return 0;													\
	T res = T##_pow4(v), res2 = res * v;								\
	return res == T##_MAX || res2 / v != res ? T##_MAX : res2;			\
}																		\
																		\
T T##_exp10(T v) {														\
																		\
	if(v >= sizeof(U64_EXP10) / sizeof(U64_EXP10[0]))					\
		return T##_MAX;													\
																		\
	U64 r = U64_EXP10[v];												\
	return r >= (U64)T##_MAX ? T##_MAX : (T)r;							\
}																		\
																		\
T T##_exp2(T v) {														\
																		\
	if(v < 0 || v >= sizeof(T) * 8 - 1)									\
		return T##_MAX;													\
																		\
	return (T)1 << v;													\
}

UINT_OP_IMPL(U64);
UINT_OP_IMPL(U32);
UINT_OP_IMPL(U16);
UINT_OP_IMPL(U8);

//TODO: Properly check Ixx_pow

#define INT_OP_IMPL(T)																		\
XINT_OP_IMPL(T);																			\
																							\
T T##_abs(T v) { return v < 0 ? -v : v; }													\
																							\
T T##_pow2(T v) { 																			\
	T res = v * v; 																			\
	return res < T##_abs(v) ? T##_MAX : res;												\
}																							\
																							\
T T##_pow3(T v) { 																			\
	T res = v * v * v; 																		\
	return res < T##_abs(v) ? T##_MAX : res;												\
}																							\
																							\
T T##_pow4(T v) { 																			\
	T res = v * v; res *= res;																\
	return res < T##_abs(v) ? T##_MAX : res;												\
}																							\
																							\
T T##_pow5(T v) { 																			\
	T res = v * v; res *= res * v;															\
	return res < T##_abs(v) ? T##_MAX : res;												\
}																							\
																							\
T T##_exp10(T v) {																			\
																							\
	if(v < 0 || v >= (T)(sizeof(U64_EXP10) / sizeof(U64_EXP10[0]) - 1))						\
		return T##_MAX;																		\
																							\
	I64 r = (I64) U64_EXP10[v];																\
	return r < 0 || r >= (I64) T##_MAX ? T##_MAX : (T)r;									\
}																							\
																							\
T T##_exp2(T v) {																			\
																							\
	if(v >= (T)(sizeof(T) * 8))																\
		return T##_MAX;																		\
																							\
	return (T)1 << v;																		\
}

INT_OP_IMPL(I64);
INT_OP_IMPL(I32);
INT_OP_IMPL(I16);
INT_OP_IMPL(I8);

#define FLP_OP_IMPL(T, TInt, suffix)														\
T T##_min(T v0, T v1) { return v0 <= v1 ? v0 : v1; }										\
T T##_max(T v0, T v1) { return v0 >= v1 ? v0 : v1; }										\
T T##_clamp(T v, T mi, T ma) { return T##_max(mi, T##_min(ma, v)); }						\
T T##_saturate(T v) { return T##_clamp(v, 0, 1); }											\
																							\
T T##_lerp(T a, T b, T perc) { return a + (b - a) * perc; }									\
T T##_abs(T v) { return v < 0 ? -v : v; }													\
																							\
Error T##_mod(T v, T mod, T *res) { 														\
																							\
	if(!res)																				\
		return Error_nullPointer(1, #T "_mod()::res is required");							\
																							\
	if(!mod)																				\
		return Error_divideByZero(0, *(const TInt*) &v, 0, #T "_mod() division by zero");	\
																							\
	T r = fmod##suffix(v, mod); 															\
																							\
	if(!T##_isValid(r))																		\
		return Error_NaN(0, #T "_mod() generated NaN or Inf");								\
																							\
	*res = r;																				\
	return Error_none();																	\
}																							\
																							\
Bool T##_isNaN(T v) { return isnan(v); }													\
Bool T##_isInf(T v) { return isinf(v); }													\
Bool T##_isValid(T v) { return isfinite(v); }												\
																							\
T T##_fract(T v) { return v - T##_floor(v); }												\
																							\
T T##_sign(T v) { return v < 0 ? -1.##suffix : (v > 0 ? 1.##suffix : 0.##suffix); }			\
T T##_signInc(T v) { return v < 0 ? -1.##suffix : 1.##suffix; }								\
																							\
T T##_sqrt(T v) { return sqrt##suffix(v); }													\
																							\
T T##_log10(T v) { return log10##suffix(v); }												\
T T##_loge(T v) { return log##suffix(v); }													\
T T##_log2(T v) { return log2##suffix(v); }													\
																							\
T T##_asin(T v) { return asin##suffix(v); }													\
T T##_sin(T v) { return sin##suffix(v); }													\
T T##_cos(T v) { return cos##suffix(v); }													\
T T##_acos(T v) { return acos##suffix(v); }													\
T T##_tan(T v) { return tan##suffix(v); }													\
T T##_atan(T v) { return atan##suffix(v); }													\
T T##_atan2(T y, T x) { return atan2##suffix(y, x); }										\
																							\
T T##_round(T v) { return round##suffix(v); }												\
T T##_ceil(T v) { return ceil##suffix(v); }													\
T T##_floor(T v) { return floor##suffix(v); }												\
																							\
Error T##_pow2(T v, T *res) { 																\
																							\
	if(!res)																				\
		return Error_nullPointer(1, #T "_pow2()::res is required");							\
																							\
	*res = v * v; 																			\
	return !T##_isValid(*res) ? Error_overflow(												\
		0, *(const TInt*)&v, *(const TInt*)res, #T "_pow2() generated an inf"				\
	) : Error_none();																		\
}																							\
																							\
Error T##_pow3(T v, T *res) { 																\
																							\
	if(!res)																				\
		return Error_nullPointer(1, #T "_pow3()::res is required");							\
																							\
	*res = v * v * v;																		\
	return !T##_isValid(*res) ? Error_overflow(												\
		0, *(const TInt*)&v, *(const TInt*)res, #T "_pow3() generated an inf"				\
	) : Error_none();																		\
}																							\
																							\
Error T##_pow4(T v, T *res) { 																\
																							\
	if(!res)																				\
		return Error_nullPointer(1, #T "_pow4()::res is required");							\
																							\
	*res = v * v;																			\
	*res *= *res;																			\
																							\
	return !T##_isValid(*res) ? Error_overflow(												\
		0, *(const TInt*)&v, *(const TInt*)res, #T "_pow4() generated an inf"				\
	) : Error_none();																		\
}																							\
																							\
Error T##_pow5(T v, T *res) { 																\
																							\
	if(!res)																				\
		return Error_nullPointer(1, #T "_pow5()::res is required");							\
																							\
	*res = v * v; 																			\
	*res *= *res; 																			\
	*res *= v;																				\
																							\
	return !T##_isValid(*res) ? Error_overflow(												\
		0, *(const TInt*)&v, *(const TInt*)res, #T "_pow5() generated an inf"				\
	) : Error_none();																		\
}																							\
																							\
Error T##_pow(T v, T exp, T *res) { 														\
																							\
	T r = pow##suffix(v, exp); 																\
																							\
	if(!T##_isValid(r))																		\
		return Error_overflow(																\
			0, *(const TInt*)&v, *(const TInt*)res, #T "_pow() generated an inf"			\
		);																					\
																							\
	*res = r;																				\
	return Error_none();																	\
}																							\
																							\
Error T##_expe(T v, T *res) { return T##_pow(T##_E, v, res); }								\
Error T##_exp2(T v, T *res) { return T##_pow(2, v, res); }									\
Error T##_exp10(T v, T *res) { return T##_pow(10, v, res); }

FLP_OP_IMPL(F32, U32, f);
FLP_OP_IMPL(F64, U64, );
