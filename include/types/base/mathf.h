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

//types/base/mathf.h

#pragma once
#include "types/base/math_common.h"

#ifdef __cplusplus
	extern "C" {
#endif

#define FLP_CONSTS(T, suffix)                                                                        \
static const T T##_E                = 2.718281828459045##suffix;                                    \
static const T T##_PI                = 3.141592653589793##suffix;                                    \
static const T T##_RAD_TO_DEG        = 57.29577951308232##suffix;                                    \
static const T T##_DEG_TO_RAD        = 0.017453292519943295##suffix;

FLP_CONSTS(F32, f);
FLP_CONSTS(F64, );

#undef FLP_CONSTS

#define FLP_OP(T, suffix)                                                                            \
																									\
ARIT_OP(T);                                                                                            \
																									\
static inline T T##_saturate(T v) { return T##_clamp(v, 0, 1); }                                    \
																									\
static inline T T##_lerp(T a, T b, T perc) { return a + (b - a) * perc; }                            \
static inline T T##_abs(T v) { return v < 0 ? -v : v; }                                                \
T T##_sqrt(T v);                                                                                    \
																									\
Bool T##_isNaN(T v);                                                                                \
Bool T##_isInf(T v);                                                                                \
Bool T##_isValid(T v);                                                                                \
																									\
T T##_pow(T v, T exp);                                                                                \
																									\
static inline T T##_expe(T v) { return T##_pow(T##_E, v); }                                            \
static inline T T##_exp2(T v) { return T##_pow(2, v); }                                                \
static inline T T##_exp10(T v) { return T##_pow(10, v); }                                            \
																									\
T T##_log10(T v);                                                                                    \
T T##_loge(T v);                                                                                    \
T T##_log2(T v);                                                                                    \
																									\
T T##_asin(T v);                                                                                    \
T T##_sin(T v);                                                                                        \
T T##_cos(T v);                                                                                        \
T T##_acos(T v);                                                                                    \
T T##_tan(T v);                                                                                        \
T T##_atan(T v);                                                                                    \
T T##_atan2(T y, T x);                                                                                \
																									\
T T##_round(T v);                                                                                    \
T T##_ceil(T v);                                                                                    \
T T##_floor(T v);                                                                                    \
T T##_fract(T v);                                                                                    \
																									\
T T##_mod(T v, T mod);                                                                                \
																									\
static inline T T##_sign(T v) { return v < 0 ? -1.##suffix : (v > 0 ? 1.##suffix : 0.##suffix); }    \
static inline T T##_signInc(T v) { return v < 0 ? -1.##suffix : 1.##suffix; }                        \
static inline Bool T##_approxEq(T a, T b, T eps) { return T##_abs(a - b) <= eps; }

FLP_OP(F32, f);
FLP_OP(F64, );

#ifdef __cplusplus
	}
#endif
