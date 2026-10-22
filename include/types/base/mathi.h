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

//types/base/mathi.h

#pragma once
#include "types/base/platform_types.h"
#include "types/base/math_common.h"
#include "types/base/algorithm.h"
#include "types/base/constants.h"

#if _COMPILER == COMPILER_MSVC
	#include <intrin0.h>        //_BitScan*64 and __popcnt64
#endif

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

#define XINT_OP(T, TUint, TLimit, TSigned)                                                              \
ARIT_OP(T);                                                                                             \
static inline T T##_exp10(T v) {                                                                        \
	return                                                                                              \
		(TUint)v >= (TUint)sizeof(U64_EXP10) / sizeof(U64) || U64_EXP10[v] >= (TLimit) ?                \
		(T)-1 : (T) U64_EXP10[v];                                                                       \
}                                                                                                       \
																										\
static inline T T##_exp2(T v) {                                                                         \
	return (TUint)v >= (T)(sizeof(T) * 8 - TSigned) ? (T)-1 : (T) ((U64)1 << v);                        \
}                                                                                                       \
																										\
static inline T T##_safeDiv(T a, T b) { return b == 0 ? 0 : a / b; }                                    \
																										\
/* The arguments point at the values. */                                                                \
static inline ECompareResult T##_compare(const void *a, const void *b, void *context) {                 \
	const T x = *(const T*) a, y = *(const T*) b;                                                       \
	(void) context;                                                                                     \
	return x == y ? ECompareResult_Eq : (x < y ? ECompareResult_Lt : ECompareResult_Gt);                \
}

//rol/ror below mask the second shift as well as the first.
//With bits == 0 the complement is the full width, and shifting a T by sizeof(T) * 8 is undefined.
//U8/U16 escape that by promoting to int, U32/U64 don't.
//x86 gave the right answer anyway, but on arm64 the poisoned value reaches the caller and the optimizer does as it
// likes with it, which is how rol(x, 0) != x got through.
//Masking both is the idiom compilers pattern match, so it still comes out as a single rotate instruction.

#define UINT_OP(T)                                                                                      \
XINT_OP(T, T, (U64)(T)-1, 0);                                                                           \
																										\
static inline T T##_rol(T a, U8 bits) {                                                                 \
	bits &= sizeof(T) * 8 - 1;                                                                          \
	return (a << bits) | (a >> ((sizeof(T) * 8 - bits) & (sizeof(T) * 8 - 1)));                         \
}                                                                                                       \
																										\
static inline T T##_ror(T a, U8 bits) {                                                                 \
	bits &= sizeof(T) * 8 - 1;                                                                          \
	return (a >> bits) | (a << ((sizeof(T) * 8 - bits) & (sizeof(T) * 8 - 1)));                         \
}

UINT_OP(U64);
UINT_OP(U32);
UINT_OP(U16);
UINT_OP(U8);

//The index of the highest set bit (bitScan) and of the lowest (bitScanReverse), U8_MAX when none is set,
// and how many are set (bitCount): HLSL's firstbithigh, firstbitlow and countbits.
//MSVC uses its intrinsics, every other compiler (clang-cl too) the GNU builtins.
//__popcnt64 needs POPCNT: always there on arm64, and on x64 only in SIMD builds, through their SSE4.2 baseline.

static inline U8 U64_bitCount(U64 v) {
#if _COMPILER != COMPILER_MSVC
	return (U8) __builtin_popcountll(v);
#elif _ARCH == ARCH_ARM64 || _SIMD == SIMD_SSE
	return (U8) __popcnt64(v);
#else
	v = v - ((v >> 1) & 0x5555555555555555);
	v = (v & 0x3333333333333333) + ((v >> 2) & 0x3333333333333333);
	v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0F;
	return (U8) ((v * 0x0101010101010101) >> 56);
#endif
}

static inline U8 U64_bitScan(U64 v) {

	if(!v)
		return U8_MAX;

#if _COMPILER != COMPILER_MSVC
	return (U8) (63 - __builtin_clzll(v));
#else
	unsigned long i = 0;
	_BitScanReverse64(&i, v);
	return (U8) i;
#endif
}

static inline U8 U64_bitScanReverse(U64 v) {

	if(!v)
		return U8_MAX;

#if _COMPILER != COMPILER_MSVC
	return (U8) __builtin_ctzll(v);
#else
	unsigned long i = 0;
	_BitScanForward64(&i, v);
	return (U8) i;
#endif
}

//The narrower types widen without changing any bit's index.

static inline U8 U32_bitCount(U32 v) { return U64_bitCount(v); }
static inline U8 U16_bitCount(U16 v) { return U64_bitCount(v); }
static inline U8 U8_bitCount(U8 v) { return U64_bitCount(v); }

static inline U8 U32_bitScan(U32 v) { return U64_bitScan(v); }
static inline U8 U16_bitScan(U16 v) { return U64_bitScan(v); }
static inline U8 U8_bitScan(U8 v) { return U64_bitScan(v); }

static inline U8 U32_bitScanReverse(U32 v) { return U64_bitScanReverse(v); }
static inline U8 U16_bitScanReverse(U16 v) { return U64_bitScanReverse(v); }
static inline U8 U8_bitScanReverse(U8 v) { return U64_bitScanReverse(v); }

//Int

#define INT_IOP(T, TUint)                                                                            \
XINT_OP(T, TUint, (U64)1 << (sizeof(T) * 8 - 1), 1);                                                 \
static inline T T##_abs(T v) {                                                                       \
	const TUint mask = (TUint)0 - ((TUint)v >> (sizeof(T) * 8 - 1));                                 \
	return (T)(((TUint)v ^ mask) - mask);                                                            \
}

INT_IOP(I64, U64);
INT_IOP(I32, U32);
INT_IOP(I16, U16);
INT_IOP(I8, U8);

#undef INT_IOP
#undef XINT_OP

#ifdef __cplusplus
	}
#endif
