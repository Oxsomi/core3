/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;

#define FLP_CONSTS(T)							\
extern const T T##_E;							\
extern const T T##_PI;							\
extern const T T##_RAD_TO_DEG;					\
extern const T T##_DEG_TO_RAD;

FLP_CONSTS(F32);
FLP_CONSTS(F64);

//Math errors assume inputs aren't nan or inf
//Ensure it's true with extra validation
//But math operations can't create them

//Uint
//TODO: Errors

#define ARIT_OP(T)								\
T T##_min(T v0, T v1);							\
T T##_max(T v0, T v1);							\
T T##_clamp(T v, T mi, T ma)

#define XINT_OP(T)								\
ARIT_OP(T);										\
T T##_pow2(T v);								\
T T##_pow3(T v);								\
T T##_pow4(T v);								\
T T##_pow5(T v);								\
T T##_exp10(T v);								\
T T##_exp2(T v)

//TODO: Int, uint %/^*+-

XINT_OP(U64);
XINT_OP(U32);
XINT_OP(U16);
XINT_OP(U8);

//Int

#define INT_IOP(T)								\
XINT_OP(T);										\
T T##_abs(T v)

INT_IOP(I64);
INT_IOP(I32);
INT_IOP(I16);
INT_IOP(I8);

//Float
//TODO: %/^+-
//		Should also check if the ++ and -- actually increased the float.
//		If not, throw! +- etc can check on lost precision (e.g. 1% of value)
//TODO: Proper error checking!

#define FLP_OP(T)								\
												\
ARIT_OP(T);										\
												\
Error T##_pow2(T v, T *res);					\
Error T##_pow3(T v, T *res);					\
Error T##_pow4(T v, T *res);					\
Error T##_pow5(T v, T *res);					\
Error T##_exp10(T v, T *res);					\
Error T##_exp2(T v, T *res);					\
												\
T T##_saturate(T v);							\
												\
T T##_lerp(T a, T b, T perc);					\
T T##_abs(T v);									\
T T##_sqrt(T v);								\
												\
Bool T##_isNaN(T v);							\
Bool T##_isInf(T v);							\
Bool T##_isValid(T v);							\
												\
Error T##_pow(T v, T exp, T *res);				\
Error T##_expe(T v, T *res);					\
												\
T T##_log10(T v);								\
T T##_loge(T v);								\
T T##_log2(T v);								\
												\
T T##_asin(T v);								\
T T##_sin(T v);									\
T T##_cos(T v);									\
T T##_acos(T v);								\
T T##_tan(T v);									\
T T##_atan(T v);								\
T T##_atan2(T y, T x);							\
												\
T T##_round(T v);								\
T T##_ceil(T v);								\
T T##_floor(T v);								\
T T##_fract(T v);								\
												\
Error T##_mod(T v, T mod, T *result);			\
												\
T T##_sign(T v);								\
T T##_signInc(T v)

FLP_OP(F32);
FLP_OP(F64);

#ifdef __cplusplus
	}
#endif
