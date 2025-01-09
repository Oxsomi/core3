/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "types/container/big_int.h"
#include "types/base/error.h"

U128 U128_create(const void *data) {
	U128 result = U128_zero();
	Buffer_memcpy(Buffer_createRef(&result, sizeof(result)), Buffer_createRefConst(data, sizeof(result)));
	return result;
}

typedef union U128_U64x2 {
	U128 v;
	U64 v2[2];
} U128_U64x2;

U128 U128_createU64x2(U64 a, U64 b) {
	U128_U64x2 data = (U128_U64x2) { .v2 = { a, b } };
	return data.v;
}

U128 U128_mul64(U64 au, U64 bu) {
	return (__uint128_t) au * (__uint128_t) bu;
}

U128 U128_zero() { return (__uint128_t) 0; }
U128 U128_one() { return (__uint128_t) 1; }

U128 U128_xor(U128 a, U128 b) { return a ^ b; }
U128 U128_or(U128 a, U128 b) { return a | b; }
U128 U128_and(U128 a, U128 b) { return a & b; }
U128 U128_not(U128 a) {  return ~a; }
U128 U128_add64(U64 a, U64 b) { return (__uint128_t)a + b; }
Bool U128_eq(U128 a, U128 b) { return a == b; }
U128 U128_add(U128 a, U128 b) { return a + b; }
U128 U128_sub(U128 a, U128 b) {  return a - b; }

U128 U128_lsh(U128 a, U8 x) { return a << x; }
U128 U128_rsh(U128 a, U8 x) { return a >> x; }

U8 U128_bitScan(U128 a) {
	U64 arr[2] = { a & U64_MAX, a >> 64 };
	BigInt b = (BigInt) { 0 };
	BigInt_createRefConst(arr, 2, &b);
	return (U8) BigInt_bitScan(b);
}

U8 U128_bitScanReverse(U128 a) {
	U64 arr[2] = { a & U64_MAX, a >> 64 };
	BigInt b = (BigInt) { 0 };
	BigInt_createRefConst(arr, 2, &b);
	return (U8) BigInt_bitScanReverse(b);
}

ECompareResult U128_cmp(U128 a, U128 b) {
	return a < b ? ECompareResult_Lt : (a == b ? ECompareResult_Eq : ECompareResult_Gt);
}
