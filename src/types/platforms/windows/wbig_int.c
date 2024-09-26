/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/big_int.h"

U128 U128_create(const U8 data[16]) {
	return I32x4_createFromU64x2(((const U64*)data)[0], ((const U64*)data)[1]);
}

U128 U128_createU64x2(U64 a, U64 b) {
	return I32x4_createFromU64x2(a, b);
}

U128 U128_mul64(U64 au, U64 bu) {
	const U64 hiProd = 0;
	const U64 loProd = _umul128(bu, au, &hiProd);
	return U128_createU64x2(loProd, hiProd);
}

U128 U128_zero() { return I32x4_zero(); }
U128 U128_one() { return I32x4_create4(1, 0, 0, 0); }

U128 U128_xor(U128 a, U128 b) { return I32x4_xor(a, b); }
U128 U128_or(U128 a, U128 b) { return I32x4_or(a, b); }
U128 U128_and(U128 a, U128 b) { return I32x4_and(a, b); }
U128 U128_not(U128 a) {  return I32x4_not(a); }
Bool U128_eq(U128 a, U128 b) { return I32x4_eq4(a, b); }

U128 U128_add64(U64 a, U64 b) {
	const U64 c = a + b;
	return I32x4_create4((U32)c, (U32)(c >> 32), c < a, 0);
}

U128 U128_add(U128 av, U128 bv) {

	const U64 *a = (const U64*)&av;
	const U64 *b = (const U64*)&bv;

	const U64 lo = a[0] + b[0];
	U64 hi = lo < a[0];
	hi += a[1] + b[1];

	return U128_createU64x2(lo, hi);
}

U128 U128_sub(U128 a, U128 b) {
	return U128_add(a, U128_add(U128_not(b), U128_one()));
}

U128 U128_lsh(U128 a, U8 x) { return I32x4_lsh128(a, x); }
U128 U128_rsh(U128 a, U8 x) { return I32x4_rsh128(a, x); }

U8 U128_bitScan(U128 a) {

	unsigned long index = 0;
	const Bool hasFirstBit = _BitScanReverse64(&index, ((const U64*)&a)[0]);

	if (hasFirstBit)
		return (U8)index;

	const Bool hasLastBit = _BitScanReverse64(&index, ((const U64*)&a)[1]);
	return hasLastBit ? (U8) index + 64 : U8_MAX;
}

ECompareResult U128_cmp(U128 a, U128 b) {

	if(U128_eq(a, b))
		return ECompareResult_Eq;

	const U64 *a64 = (const U64*)&a;
	const U64 *b64 = (const U64*)&b;

	if(a64[1] > b64[1] || (a64[1] == b64[1] && a64[0] > b64[0]))
		return ECompareResult_Gt;

	return ECompareResult_Lt;
}
