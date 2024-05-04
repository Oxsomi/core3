/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/vec.h"
#include "types/platform_types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct CharString CharString;

//BigInt allow up to 16320 bit ints but isn't well optimized.
//For optimized versions please use U128 and U256 since they don't dynamically allocate,
//and because big int can be varying length so no SIMD optimizations are present.

typedef struct BigInt {

	const U64 *data;

	Bool isConst;
	Bool isRef;
	U8 length;		//In U64s
	U8 pad;

} BigInt;

BigInt BigInt_createNull();
Error BigInt_create(U16 bitCount, Allocator allocator, BigInt *big);			//Aligns bitCount to 64.
Error BigInt_createRef(U64 *ptr, U64 ptrCount, BigInt *big);					//ref U64 ptr[ptrCount]
Error BigInt_createRefConst(const U64 *ptr, U64 ptrCount, BigInt *big);			//const ref U64 ptr[ptrCount]
Error BigInt_createCopy(BigInt *a, Allocator alloc, BigInt *b);

Bool BigInt_free(BigInt *a, Allocator allocator);

//bitCount set to 0 indicates "auto".
//bitCount set to U16_MAX indicates "already allocated"

Error BigInt_createFromHex(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromDec(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromOct(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromBin(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromNyto(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromString(CharString text, U16 bitCount, Allocator allocator, BigInt *big);

//Arithmetic

Bool BigInt_mul(BigInt *a, BigInt b, Allocator allocator);						//Multiply on self and keep bit count
Bool BigInt_add(BigInt *a, BigInt b);											//Add on self and keep bit count
Bool BigInt_sub(BigInt *a, BigInt b);											//Subtract on self and keep bit count

//Bool BigInt_mod(BigInt *a, BigInt b);
//Bool BigInt_div(BigInt *a, BigInt b);

//Bitwise

Bool BigInt_xor(BigInt *a, BigInt b);
Bool BigInt_or(BigInt *a, BigInt b);
Bool BigInt_and(BigInt *a, BigInt b);
Bool BigInt_not(BigInt *a);

Bool BigInt_lsh(BigInt *a, U16 bits);
Bool BigInt_rsh(BigInt *a, U16 bits);

//Compare

ECompareResult BigInt_cmp(BigInt a, BigInt b);
Bool BigInt_eq(BigInt a, BigInt b);
Bool BigInt_neq(BigInt a, BigInt b);
Bool BigInt_lt(BigInt a, BigInt b);
Bool BigInt_leq(BigInt a, BigInt b);
Bool BigInt_gt(BigInt a, BigInt b);
Bool BigInt_geq(BigInt a, BigInt b);

BigInt *BigInt_min(BigInt *a, BigInt *b);										//Returns one of two passed pointers
BigInt *BigInt_max(BigInt *a, BigInt *b);										//Returns one of two passed pointers
BigInt *BigInt_clamp(BigInt *a, BigInt *mi, BigInt *ma);						//Returns one of two passed pointers

//Helpers

Error BigInt_resize(BigInt *a, U8 newSize, Allocator alloc);					//newSize in U64s
Bool BigInt_set(BigInt *a, BigInt b, Bool allowResize, Allocator alloc);		//Set all bits to b, resize if allowResize

Bool BigInt_trunc(BigInt *big, Allocator allocator);							//Gets rid of all hi bits that are unset

Buffer BigInt_bufferConst(BigInt b);
Buffer BigInt_buffer(BigInt b);

U16 BigInt_byteCount(BigInt b);
U16 BigInt_bitCount(BigInt b);

U16 BigInt_bitScan(BigInt a);						//Find highest bit that was on. Returns U16_MAX if 0
Error BigInt_isBase2(BigInt a, Allocator allocator, Bool *isBase2);

//Transform to string

typedef enum EIntegerEncoding {
	EIntegerEncoding_Hex,
	EIntegerEncoding_Bin,
	EIntegerEncoding_Oct,
	EIntegerEncoding_Nyto,
	//EIntegerEncoding_Dec,
	EIntegerEncoding_Count
} EIntegerEncoding;

Error BigInt_hex(BigInt b, Allocator allocator, CharString *result, Bool leadingZeros);
Error BigInt_oct(BigInt b, Allocator allocator, CharString *result, Bool leadingZeros);
//Error BigInt_dec(BigInt b, Allocator allocator, CharString *result, Bool leadingZeros);
Error BigInt_bin(BigInt b, Allocator allocator, CharString *result, Bool leadingZeros);
Error BigInt_nyto(BigInt b, Allocator allocator, CharString *result, Bool leadingZeros);
Error BigInt_toString(
	BigInt b,
	Allocator allocator,
	CharString *result,
	EIntegerEncoding encoding,
	Bool leadingZeros
);

//U128

#if _PLATFORM_TYPE != PLATFORM_WINDOWS
	typedef __uint128_t U128;
#else
	typedef I32x4 U128;
#endif

//Create

U128 U128_create(const U8 data[16]);
U128 U128_createU64x2(U64 a, U64 b);

U128 U128_zero();
U128 U128_one();

U128 U128_createFromHex(CharString text, Error *failed);
U128 U128_createFromOct(CharString text, Error *failed);
U128 U128_createFromBin(CharString text, Error *failed);
U128 U128_createFromNyto(CharString text, Error *failed);
U128 U128_createFromDec(CharString text, Error *failed, Allocator alloc);
U128 U128_createFromString(CharString text, Error *failed, Allocator alloc);

//Bitwise

U128 U128_xor(U128 a, U128 b);
U128 U128_or(U128 a, U128 b);
U128 U128_and(U128 a, U128 b);

U128 U128_not(U128 a);

U128 U128_lsh(U128 a, U8 x);
U128 U128_rsh(U128 a, U8 x);

//Comparison

ECompareResult U128_cmp(U128 a, U128 b);
Bool U128_eq(U128 a, U128 b);
Bool U128_neq(U128 a, U128 b);
Bool U128_lt(U128 a, U128 b);
Bool U128_leq(U128 a, U128 b);
Bool U128_gt(U128 a, U128 b);
Bool U128_geq(U128 a, U128 b);

U128 U128_min(U128 a, U128 b);
U128 U128_max(U128 a, U128 b);
U128 U128_clamp(U128 a, U128 mi, U128 ma);

//Arithmetic

U128 U128_mul64(U64 a, U64 b);			//Multiply two 64-bit numbers to generate a 128-bit number
U128 U128_add64(U64 a, U64 b);			//Add two 64-bit numbers but keep the overflow bit
//U128 U128_div(U128 a, U128 b);
//U128 U128_mod(U128 a, U128 b);
U128 U128_mul(U128 a, U128 b);
U128 U128_add(U128 a, U128 b);
U128 U128_sub(U128 a, U128 b);

//Helpers

U8 U128_bitScan(U128 a);						//Find highest bit that was on. Returns U8_MAX if 0
Bool U128_isBase2(U128 a);

//Transform to string

Error U128_hex(U128 a, Allocator allocator, CharString *result, Bool leadingZeros);
Error U128_oct(U128 a, Allocator allocator, CharString *result, Bool leadingZeros);
//Error U128_dec(BigInt a, Allocator allocator, CharString *result, Bool leadingZeros);
Error U128_bin(U128 a, Allocator allocator, CharString *result, Bool leadingZeros);
Error U128_nyto(U128 a, Allocator allocator, CharString *result, Bool leadingZeros);
Error U128_toString(
	U128 a,
	Allocator allocator,
	CharString *result,
	EIntegerEncoding encoding,
	Bool leadingZeros
);

#ifdef __cplusplus
	}
#endif
