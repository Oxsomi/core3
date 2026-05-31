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

//types/math/u128.h

#pragma once
#include "types/base/platform_types.h"
#include "types/base/algorithm.h"
#include "types/math/vec4i.h"

#ifdef __cplusplus
	extern "C" {
#endif
		
//U128

#if _PLATFORM_TYPE != PLATFORM_WINDOWS
	typedef __uint128_t U128;
#else
	typedef I32x4 U128;
#endif

#if _PLATFORM_TYPE == PLATFORM_WINDOWS

	static inline U128 U128_create(const void *data) { return I32x4_load4(data); }
	static inline U128 U128_createU64x2(U64 a, U64 b) { return I32x4_createFromU64x2(a, b); }

	static inline U128 U128_zero() { return I32x4_zero(); }
	static inline U128 U128_one() { return I32x4_create4(1, 0, 0, 0); }

	static inline U128 U128_createI32x4(I32x4 a) { return a; }
	static inline I32x4 I32x4_fromU128(U128 a) { return a; }

	//Bitwise

	static inline U128 U128_xor(U128 a, U128 b) { return I32x4_xor(a, b); }
	static inline U128 U128_or(U128 a, U128 b) { return I32x4_or(a, b); }
	static inline U128 U128_and(U128 a, U128 b) { return I32x4_and(a, b); }

	static inline U128 U128_not(U128 a) { return I32x4_not(a); }

	static inline U128 U128_lsh(U128 a, U8 x) { return I32x4_lsh128(a, x); }
	static inline U128 U128_rsh(U128 a, U8 x) { return I32x4_rsh128(a, x); }

	//Arithmetic

	static inline U128 U128_add64(U64 a, U64 b) {
		const U64 c = a + b;
		return I32x4_create4((U32)c, (U32)(c >> 32), c < a, 0);
	}

	static inline U128 U128_add(U128 av, U128 bv) {

		const U64 *a = (const U64*)&av;
		const U64 *b = (const U64*)&bv;

		const U64 lo = a[0] + b[0];
		U64 hi = lo < a[0];
		hi += a[1] + b[1];

		return U128_createU64x2(lo, hi);
	}

	static inline U128 U128_sub(U128 a, U128 b) {
		return U128_add(a, U128_add(U128_not(b), U128_one()));
	}

	//Comparison

	static inline Bool U128_eq(U128 a, U128 b) { return I32x4_eq4(a, b); }

	#if _ARCH == ARCH_ARM64

		uint64_t __umulh(uint64_t a, uint64_t b);

		static inline U128 U128_mul64(U64 au, U64 bu) {
			U64 hiProd = __umulh(au, bu);
			U64 loProd = au * bu;
			return U128_createU64x2(loProd, hiProd);
		}

	#else
		static inline U128 U128_mul64(U64 au, U64 bu) {
			U64 hiProd = 0;
			const U64 loProd = _umul128(bu, au, &hiProd);
			return U128_createU64x2(loProd, hiProd);
		}
	#endif

	static inline ECompareResult U128_cmp(U128 a, U128 b) {

		if (U128_eq(a, b))
			return ECompareResult_Eq;

		const U64 *a64 = (const U64 *)&a;
		const U64 *b64 = (const U64 *)&b;

		if (a64[1] > b64[1] || (a64[1] == b64[1] && a64[0] > b64[0]))
			return ECompareResult_Gt;

		return ECompareResult_Lt;
	}

	static inline U64 U128_lo(U128 a) { return ((U64)(U32)I32x4_y(a) << 32) | (U32)I32x4_x(a); }
	static inline U64 U128_hi(U128 a) { return ((U64)(U32)I32x4_w(a) << 32) | (U32)I32x4_z(a); }

#else

	static inline U128 U128_zero() { return (__uint128_t)0; }

	static inline U128 U128_create(const void *data) {
		U128 result = U128_zero();
		Buffer_memcpy(Buffer_createRef(&result, sizeof(result)), Buffer_createRefConst(data, sizeof(result)));
		return result;
	}

	static inline U128 U128_createI32x4(I32x4 a) { const void *avoid = &a; return *(const U128*)avoid; }
	static inline I32x4 I32x4_fromU128(U128 a) { const void *avoid = &a; return *(const I32x4*)avoid; }

	typedef union U128_U64x2 {
		U128 v;
		U64 v2[2];
	} U128_U64x2;

	static inline U128 U128_createU64x2(U64 a, U64 b) {
		U128_U64x2 data = (U128_U64x2){ .v2 = { a, b } };
		return data.v;
	}

	static inline U128 U128_one() { return (__uint128_t)1; }

	//Bitwise
	
	static inline U128 U128_xor(U128 a, U128 b) { return a ^ b; }
	static inline U128 U128_or(U128 a, U128 b) { return a | b; }
	static inline U128 U128_and(U128 a, U128 b) { return a & b; }

	static inline U128 U128_not(U128 a) { return ~a; }

	static inline U128 U128_lsh(U128 a, U8 x) { return a << x; }
	static inline U128 U128_rsh(U128 a, U8 x) { return a >> x; }

	//Arithmetic

	static inline U128 U128_add(U128 a, U128 b) { return a + b; }
	static inline U128 U128_sub(U128 a, U128 b) { return a - b; }

	//Add two 64-bit numbers but keep the overflow bit
	static inline U128 U128_add64(U64 a, U64 b) { return (__uint128_t)a + b; }

	//Multiply two 64-bit numbers to generate a 128-bit number
	static inline U128 U128_mul64(U64 au, U64 bu) {
		return (__uint128_t)au * (__uint128_t)bu;
	}

	//Comparison

	static inline Bool U128_eq(U128 a, U128 b) { return a == b; }

	static inline ECompareResult U128_cmp(U128 a, U128 b) {
		return a < b ? ECompareResult_Lt : (a == b ? ECompareResult_Eq : ECompareResult_Gt);
	}

	static inline U64 U128_lo(U128 a) { return (U64)a; }
	static inline U64 U128_hi(U128 a) { return (U64)(a >> 64); }

#endif

//Comparison

static inline Bool U128_neq(U128 a, U128 b) { return !U128_eq(a, b); }
static inline Bool U128_lt(U128 a, U128 b) { return U128_cmp(a, b) < ECompareResult_Eq; }
static inline Bool U128_leq(U128 a, U128 b) { return U128_cmp(a, b) <= ECompareResult_Eq; }
static inline Bool U128_gt(U128 a, U128 b) { return U128_cmp(a, b) > ECompareResult_Eq; }
static inline Bool U128_geq(U128 a, U128 b) { return U128_cmp(a, b) >= ECompareResult_Eq; }

static inline U128 U128_min(U128 a, U128 b) { return U128_leq(a, b) ? a : b; }
static inline U128 U128_max(U128 a, U128 b) { return U128_geq(a, b) ? a : b; }
static inline U128 U128_clamp(U128 a, U128 mi, U128 ma) { return U128_max(U128_min(a, ma), mi); }

//Arithmetic

//U128 U128_div(U128 a, U128 b);
//U128 U128_mod(U128 a, U128 b);
//U128 U128_mul(U128 a, U128 b);

//clmul64 and clmul64Fallback need imm to be a compile time constant, else it's UB.
static inline I32x4 I32x4_clmul64Fallback(I32x4 avec, I32x4 bvec, U8 imm) {

	U128 a = U128_createI32x4(avec);
	U128 b = U128_createI32x4(bvec);

	U64 A = (imm & 0x01) ? U128_hi(a) : U128_lo(a);
	U64 B = (imm & 0x10) ? U128_hi(b) : U128_lo(b);

	U128 r = U128_zero();

	for (U8 i = 0; i < 64; ++i)
		if ((B >> i) & 1)
			r = U128_xor(r, U128_lsh(U128_createU64x2(A, 0), i));

	return I32x4_fromU128(r);
}

#if _SIMD == SIMD_NONE
	static inline I32x4 I32x4_clmul64(I32x4 avec, I32x4 bvec, U8 imm) {
		return I32x4_clmul64Fallback(avec, bvec, imm);
	}
#elif _SIMD == SIMD_NEON
	static inline I32x4 I32x4_clmul64(I32x4 a, I32x4 b, U8 imm) {

		uint64x2_t va = vreinterpretq_u64_s32(a);
		uint64x2_t vb = vreinterpretq_u64_s32(b);

		uint64x1_t halfa, halfb;
		switch (imm) {
			case 0x00:     halfa = vget_low_u64(va);  halfb = vget_low_u64(vb);  break;
			case 0x10:     halfa = vget_low_u64(va);  halfb = vget_high_u64(vb); break;
			case 0x01:     halfa = vget_high_u64(va); halfb = vget_low_u64(vb);  break;
			default:       halfa = vget_high_u64(va); halfb = vget_high_u64(vb); break;
		}

		return vreinterpretq_s32_p128(vmull_p64(vreinterpret_u64_p64(halfa), vreinterpret_u64_p64(halfb)));
	}
#else
	static inline I32x4 I32x4_clmul64(I32x4 a, I32x4 b, U8 imm) {
		switch(imm) {
			case 0x00:    return _mm_clmulepi64_si128(a, b, 0x00);
			case 0x01:    return _mm_clmulepi64_si128(a, b, 0x01);
			case 0x10:    return _mm_clmulepi64_si128(a, b, 0x10);
			default:    return _mm_clmulepi64_si128(a, b, 0x11);
		}
	}
#endif

#ifdef __cplusplus
	}
#endif
