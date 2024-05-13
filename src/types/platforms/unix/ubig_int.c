/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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
	return *(const U128*)data;
}

U128 U128_createU64x2(U64 a, U64 b) {
	U64 data[2] = { a, b };
	return *(const U128*)data;
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

	//Keep subdividing by 2x until the first bit is found

	U64 offset = (Bool)((const U64*)&a)[1];
	offset <<= 1;

	offset |= (Bool)((const U32*)&a)[offset + 1];
	offset <<= 1;

	offset |= (Bool)((const U16*)&a)[offset + 1];
	offset <<= 1;

	offset |= (Bool)((const U8*)&a)[offset + 1];
	U8 b = ((const U8*)&a)[offset];
	offset <<= 1;

	offset |= (Bool)(b >> 4);
	offset <<= 1;

	offset |= (Bool)((b >> (((offset & 2) + 1) * 2)) & 3);
	offset <<= 1;

	offset |= (Bool)((b >> ((offset & 6) + 1)) & 1);

	return offset;
}

ECompareResult U128_cmp(U128 a, U128 b) {
	return a < b ? ECompareResult_Lt : (a == b ? ECompareResult_Eq : ECompareResult_Gt);
}
