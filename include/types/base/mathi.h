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
#include "types/base/math_common.h"

#ifdef __cplusplus
	extern "C" {
#endif

static const U64 U64_EXP10[] = {
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

//Uint

#define XINT_OP(T, TUint)																			\
ARIT_OP(T);																							\
static inline T T##_exp10(T v) {																	\
	return (TUint)v >= (TUint)sizeof(U64_EXP10) / sizeof(U64) ? (T)-1 : (T) U64_EXP10[v];			\
}																									\
																									\
static inline T T##_exp2(T v) {																		\
	return (TUint)v >= (T)(sizeof(T) * 8) ? (T)-1 : (T) ((U64)1 << v);								\
}																									\
																									\
static inline T T##_safeDiv(T a, T b) { return b == 0 ? 0 : a / b; }

#define UINT_OP(T)																					\
XINT_OP(T, T);																						\
																									\
static inline T T##_rol(T a, U8 bits) {																\
	bits &= sizeof(T) * 8 - 1;																		\
	return (a << bits) | (a >> (32 - bits));														\
}																									\
																									\
static inline T T##_ror(T a, U8 bits) {																\
	bits &= sizeof(T) * 8 - 1;																		\
	return (a >> bits) | (a << (32 - bits));														\
}

UINT_OP(U64);
UINT_OP(U32);
UINT_OP(U16);
UINT_OP(U8);

//Int

#define INT_IOP(T, TUint)																			\
XINT_OP(T, TUint);																					\
static inline T T##_abs(T v) { return v < 0 ? -v : v; }		/*UB for INT_MIN*/

INT_IOP(I64, U64);
INT_IOP(I32, U32);
INT_IOP(I16, U16);
INT_IOP(I8, U8);

#undef INT_IOP
#undef XINT_OP

#ifdef __cplusplus
	}
#endif
