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
#include "types/base/algorithm.h"
#include "types/base/string.h"
#include "types/container/big_int_predeclare.h"
#include "types/math/vec4i.h"

#ifdef __cplusplus
	extern "C" {
#endif

#if _PLATFORM_TYPE == PLATFORM_WINDOWS

	static inline U128 U128_create(const void *data) { return I32x4_load4(data); }
	static inline U128 U128_createU64x2(U64 a, U64 b) { return I32x4_createFromU64x2(a, b); }

	static inline U128 U128_zero() { return I32x4_zero(); }
	static inline U128 U128_one() { return I32x4_create4(1, 0, 0, 0); }

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

	static inline U8 U128_bitScan(U128 a) {

		unsigned long index = 0;
		const Bool hasFirstBit = _BitScanReverse64(&index, ((const U64 *)&a)[1]);

		if (hasFirstBit)
			return (U8)index + 64;

		const Bool hasLastBit = _BitScanReverse64(&index, ((const U64 *)&a)[0]);
		return hasLastBit ? (U8)index : U8_MAX;
	}

	static inline U8 U128_bitScanReverse(U128 a) {

		unsigned long index = 0;
		const Bool hasFirstBit = _BitScanForward64(&index, ((const U64 *)&a)[0]);

		if (hasFirstBit)
			return (U8)index;

		const Bool hasLastBit = _BitScanForward64(&index, ((const U64 *)&a)[1]);
		return hasLastBit ? (U8)index + 64 : U8_MAX;
	}

	static inline ECompareResult U128_cmp(U128 a, U128 b) {

		if (U128_eq(a, b))
			return ECompareResult_Eq;

		const U64 *a64 = (const U64 *)&a;
		const U64 *b64 = (const U64 *)&b;

		if (a64[1] > b64[1] || (a64[1] == b64[1] && a64[0] > b64[0]))
			return ECompareResult_Gt;

		return ECompareResult_Lt;
	}

#else

	static inline U128 U128_create(const void *data) {
		U128 result = U128_zero();
		Buffer_memcpy(Buffer_createRef(&result, sizeof(result)), Buffer_createRefConst(data, sizeof(result)));
		return result;
	}

	typedef union U128_U64x2 {
		U128 v;
		U64 v2[2];
	} U128_U64x2;

	static inline U128 U128_createU64x2(U64 a, U64 b) {
		U128_U64x2 data = (U128_U64x2){ .v2 = { a, b } };
		return data.v;
	}

	static inline U128 U128_zero() { return (__uint128_t)0; }
	static inline U128 U128_one() { return (__uint128_t)1; }

	//Bitwise
	
	static inline U128 U128_xor(U128 a, U128 b) { return a ^ b; }
	static inline U128 U128_or(U128 a, U128 b) { return a | b; }
	static inline U128 U128_and(U128 a, U128 b) { return a & b; }

	static inline U128_not(U128 a) { return ~a; }

	static inline U128_lsh(U128 a, U8 x) { return a << x; }
	static inline U128_rsh(U128 a, U8 x) { return a >> x; }

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

	//Helpers

	static inline U8 U128_bitScan(U128 a) {
		U64 arr[2] = { a & U64_MAX, a >> 64 };
		BigInt b = { 0 };
		BigInt_createRefConst(arr, 2, &b);
		return (U8) BigInt_bitScan(b);
	}

	static inline U8 U128_bitScanReverse(U128 a) {
		U64 arr[2] = { a & U64_MAX, a >> 64 };
		BigInt b = { 0 };
		BigInt_createRefConst(arr, 2, &b);
		return (U8) BigInt_bitScanReverse(b);
	}

#endif

static inline U128 U128_createFromBase2(CharString text, EIntEncoding type, Error *e_rr) {

	U128 result = U128_zero();
	BigInt asBigInt = { 0 };

	if (!BigInt_createRef((U64*) &result, 2, &asBigInt, e_rr))
		return result;

	BigIntCreate bigCreate = { &text, U16_MAX, NULL, &asBigInt };
	BigInt_createFromBase2Type(&bigCreate, type, e_rr);
	return result;
}

static inline U128 U128_createFromHex(CharString text, Error *e_rr) {
	return U128_createFromBase2(text, EIntEncoding_Hex, e_rr);
}

static inline U128 U128_createFromOct(CharString text, Error *e_rr) {
	return U128_createFromBase2(text, EIntEncoding_Oct, e_rr);
}

static inline U128 U128_createFromBin(CharString text, Error *e_rr) {
	return U128_createFromBase2(text, EIntEncoding_Bin, e_rr);
}

static inline U128 U128_createFromNyto(CharString text, Error *e_rr) {
	return U128_createFromBase2(text, EIntEncoding_Nyto, e_rr);
}

static inline U128 U128_createFromDec(CharString text, const Allocator *alloc, Error *e_rr) {

	U128 result = U128_zero();
	BigInt asBigInt = { 0 };

	//TODO: Optimize with U128_mul when available!

	if (!BigInt_createRef((U64*) &result, 2, &asBigInt, e_rr))
		return result;

	BigIntCreate bigCreate = { &text, U16_MAX, alloc, &asBigInt };
	BigInt_createFromDec(&bigCreate, e_rr);
	return result;
}

static inline U128 U128_createFromString(CharString text, const Allocator *alloc, Error *e_rr) {

	U128 v = U128_zero();

	if (CharString_length(text) > 2)
		switch (Buffer_readU16(CharString_bufferConst(text), 0, NULL, NULL)) {

			case C8x2('0', 'b'): case C8x2('0', 'B'):
			case C8x2('0', 'x'): case C8x2('0', 'X'):
			case C8x2('0', 'o'): case C8x2('0', 'O'):
			case C8x2('0', 'n'): case C8x2('0', 'N'): {

				BigInt bigInt = { 0 };
				if (!BigInt_createRef((U64*) &v, 2, &bigInt, e_rr))
					return v;

				BigIntCreate bigCreate = { &text, U16_MAX, NULL, &bigInt };
				BigInt_createFromString(&bigCreate, e_rr);
				return v;
			}
		}

	return U128_createFromDec(text, alloc, e_rr);
}

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

//Helpers

static inline Bool U128_isBase2(U128 a) {

	const U8 v = U128_bitScan(a);

	if (v == U8_MAX)
		return false;

	return U128_eq(U128_lsh(U128_one(), v), a);
}

//Transform to string

static inline Bool U128_base2(const BigIntStringify *stringify, EIntEncoding encoding, U128 a, Error *e_rr) {

	BigInt b = { 0 };
	if (!BigInt_createRefConst((const U64*) &a, 2, &b, e_rr))
		return false;

	return BigInt_base2(stringify, encoding, b, e_rr);
}

//Bool U128_dec(const BigIntStringify *stringify, U128 a, Error *e_rr);

static inline Bool U128_hex(const BigIntStringify *stringify, U128 a, Error *e_rr) {
	return U128_base2(stringify, EIntEncoding_Hex, a, e_rr);
}

static inline Bool U128_oct(const BigIntStringify *stringify, U128 a, Error *e_rr) {
	return U128_base2(stringify, EIntEncoding_Oct, a, e_rr);
}

static inline Bool U128_bin(const BigIntStringify *stringify, U128 a, Error *e_rr) {
	return U128_base2(stringify, EIntEncoding_Bin, a, e_rr);
}

static inline Bool U128_nyto(const BigIntStringify *stringify, U128 a, Error *e_rr) {
	return U128_base2(stringify, EIntEncoding_Nyto, a, e_rr);
}

static inline Bool U128_toString(const BigIntStringify *stringify, EIntEncoding enc, U128 a, Error *e_rr) {

	BigInt b = { 0 };
	if (!BigInt_createRefConst((const U64*) &a, 2, &b, e_rr))
		return false;

	return BigInt_toString(stringify, enc, b, e_rr);
}

#ifdef __cplusplus
	}
#endif
