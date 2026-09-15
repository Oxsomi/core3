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

//types/base/math.c

#include "types/base/mathf.h"

#include <math.h>

#define FLP_OP_IMPL(T, TInt, suffix)                                                        \
																							\
T T##_mod(T v, T mod) { return fmod##suffix(v, mod); }                                      \
																							\
Bool T##_isNaN(T v) { return isnan(v); }                                                    \
Bool T##_isInf(T v) { return isinf(v); }                                                    \
Bool T##_isValid(T v) { return isfinite(v); }                                               \
																							\
T T##_fract(T v) { return v - T##_floor(v); }                                               \
																							\
T T##_sqrt(T v) { return sqrt##suffix(v); }                                                 \
																							\
T T##_log10(T v) { return log10##suffix(v); }                                               \
T T##_loge(T v) { return log##suffix(v); }                                                  \
T T##_log2(T v) { return log2##suffix(v); }                                                 \
																							\
T T##_asin(T v) { return asin##suffix(v); }                                                 \
T T##_sin(T v) { return sin##suffix(v); }                                                   \
T T##_cos(T v) { return cos##suffix(v); }                                                   \
T T##_acos(T v) { return acos##suffix(v); }                                                 \
T T##_tan(T v) { return tan##suffix(v); }                                                   \
T T##_atan(T v) { return atan##suffix(v); }                                                 \
T T##_atan2(T y, T x) { return atan2##suffix(y, x); }                                       \
																							\
T T##_round(T v) {                                                                          \
	T fl = T##_floor(v);                                                                    \
	T diff = v - fl;                                                                        \
	if (diff < 0.5##suffix) return fl;                                                      \
	if (diff > 0.5##suffix) return fl + 1.##suffix;                                         \
	/* exactly 0.5, round to even (banker's) */                                                \
	return T##_mod(fl, 2.##suffix) == 0.##suffix ? fl : fl + 1.##suffix;                    \
}                                                                                           \
																							\
T T##_ceil(T v) { return ceil##suffix(v); }                                                 \
T T##_floor(T v) { return floor##suffix(v); }                                               \
																							\
T T##_pow(T v, T exp) { return pow##suffix(v, exp); }

FLP_OP_IMPL(F32, U32, f);
FLP_OP_IMPL(F64, U64, );
