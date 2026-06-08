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

//types/container/big_int.h

#pragma once
#include "types/container/big_int_predeclare.h"
#include "types/base/c8.h"
#include "types/base/algorithm.h"
#include "types/base/constants.h"
#include "types/base/string_base.h"
#include "types/base/mathi.h"

#ifdef __cplusplus
	extern "C" {
#endif

static inline Bool BigInt_createFromHex(const BigIntCreate *bigIntCreate, Error *e_rr) {
	return BigInt_createFromBase2Type(bigIntCreate, EIntEncoding_Hex, e_rr);
}

static inline Bool BigInt_createFromOct(const BigIntCreate *bigIntCreate, Error *e_rr) {
	return BigInt_createFromBase2Type(bigIntCreate, EIntEncoding_Oct, e_rr);
}

static inline Bool BigInt_createFromBin(const BigIntCreate *bigIntCreate, Error *e_rr) {
	return BigInt_createFromBase2Type(bigIntCreate, EIntEncoding_Bin, e_rr);
}

static inline Bool BigInt_createFromNyto(const BigIntCreate *bigIntCreate, Error *e_rr) {
	return BigInt_createFromBase2Type(bigIntCreate, EIntEncoding_Nyto, e_rr);
}

//Arithmetic

Bool BigInt_mul(BigInt *a, BigInt b, const Allocator *allocator, Error *e_rr);    //Multiply on self and keep bit count
Bool BigInt_add(BigInt *a, BigInt b);                                            //Add on self and keep bit count
Bool BigInt_sub(BigInt *a, BigInt b);                                            //Subtract on self and keep bit count

//Bool BigInt_mod(BigInt *a, BigInt b);
//Bool BigInt_div(BigInt *a, BigInt b);

//Bitwise

static inline Bool BigInt_xor(BigInt *a, BigInt b) {

	if (!a || a->isConst)
		return false;

	for (U64 i = 0; i < a->length && i < b.length; ++i)
		a->dataNonConst[i] ^= b.data[i];

	return true;
}

static inline Bool BigInt_or(BigInt *a, BigInt b) {

	if (!a || a->isConst)
		return false;

	for (U64 i = 0; i < a->length && i < b.length; ++i)
		a->dataNonConst[i] |= b.data[i];

	return true;
}

static inline Bool BigInt_and(BigInt *a, BigInt b) {

	if (!a || a->isConst)
		return false;

	const U64 j = U64_min(a->length, b.length);

	for (U64 i = 0; i < j; ++i)
		a->dataNonConst[i] &= b.data[i];

	for (U64 i = b.length; i < a->length; ++i)
		a->dataNonConst[i] = 0;

	return true;
}

static inline Bool BigInt_not(BigInt *a) {

	if (!a || a->isConst)
		return false;

	for (U64 i = 0; i < a->length; ++i)
		a->dataNonConst[i] = ~a->data[i];

	return true;
}

Bool BigInt_lsh(BigInt *a, U16 bits);
Bool BigInt_rsh(BigInt *a, U16 bits);

//Compare

static inline ECompareResult BigInt_cmp(BigInt a, BigInt b) {

	const U64 biggestLen = U64_max(a.length, b.length);

	for (U64 i = biggestLen - 1; i != U64_MAX; --i) {

		const U64 ai = i >= a.length ? 0 : a.data[i];
		const U64 bi = i >= b.length ? 0 : b.data[i];

		if (ai > bi)
			return ECompareResult_Gt;

		else if (ai < bi)
			return ECompareResult_Lt;
	}

	return ECompareResult_Eq;
}

static inline Bool BigInt_eq(BigInt a, BigInt b) { return BigInt_cmp(a, b) == ECompareResult_Eq; }
static inline Bool BigInt_neq(BigInt a, BigInt b) { return BigInt_cmp(a, b) != ECompareResult_Eq; }
static inline Bool BigInt_lt(BigInt a, BigInt b) { return BigInt_cmp(a, b) < ECompareResult_Eq; }
static inline Bool BigInt_leq(BigInt a, BigInt b) { return BigInt_cmp(a, b) <= ECompareResult_Eq; }
static inline Bool BigInt_gt(BigInt a, BigInt b) { return BigInt_cmp(a, b) > ECompareResult_Eq; }
static inline Bool BigInt_geq(BigInt a, BigInt b) { return BigInt_cmp(a, b) >= ECompareResult_Eq; }

//Returns one of two passed pointers

static inline BigInt *BigInt_min(BigInt *a, BigInt *b) { return !a ? b : (!b ? a : (BigInt_leq(*a, *b) ? a : b)); }
static inline BigInt *BigInt_max(BigInt *a, BigInt *b) { return !a ? b : (!b ? a : (BigInt_geq(*a, *b) ? a : b)); }
static inline BigInt *BigInt_clamp(BigInt *a, BigInt *mi, BigInt *ma) { return BigInt_max(BigInt_min(a, ma), mi); }

//Helpers

Bool BigInt_resize(BigInt *a, U8 newSize, const Allocator *alloc, Error *e_rr);    //newSize in U64s

//Set all bits to b, resize if allowResize
Bool BigInt_set(BigInt *a, BigInt b, Bool allowResize, const Allocator *alloc, Error *e_rr);

//Gets rid of all hi bits that are unset
static inline Bool BigInt_trunc(BigInt *big, const Allocator *allocator, Error *e_rr) {

	if (!big)
		return false;

	U8 i = big->length - 1;

	for (; i != U8_MAX; --i)
		if (big->data[i])
			break;

	return BigInt_resize(big, i + 1, allocator, e_rr);
}

static inline U16 BigInt_byteCount(BigInt b) { return (U16)(b.length * sizeof(U64)); }
static inline U16 BigInt_bitCount(BigInt b) { return BigInt_byteCount(b) * 8; }

static inline Buffer BigInt_buffer(BigInt b) {
	return b.isConst ? Buffer_createNull() : Buffer_createRef(b.dataNonConst, BigInt_byteCount(b));
}

static inline Buffer BigInt_bufferConst(BigInt b) { return Buffer_createRefConst(b.data, BigInt_byteCount(b)); }

static inline Bool BigInt_isBase2(BigInt a) {
	return a.length && BigInt_bitScan(a) == BigInt_bitScanReverse(a);
}

//Transform to string

static inline Bool BigInt_hex(const BigIntStringify *stringify, BigInt b, Error *e_rr) {
	return BigInt_base2(stringify, EIntEncoding_Hex, b, e_rr);
}

static inline Bool BigInt_oct(const BigIntStringify *stringify, BigInt b, Error *e_rr) {
	return BigInt_base2(stringify, EIntEncoding_Oct, b, e_rr);
}

static inline Bool BigInt_bin(const BigIntStringify *stringify, BigInt b, Error *e_rr) {
	return BigInt_base2(stringify, EIntEncoding_Bin, b, e_rr);
}

static inline Bool BigInt_nyto(const BigIntStringify *stringify, BigInt b, Error *e_rr) {
	return BigInt_base2(stringify, EIntEncoding_Nyto, b, e_rr);
}

#ifdef __cplusplus
	}
#endif
