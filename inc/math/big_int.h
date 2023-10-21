/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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
#include "math/vec.h"
#include "types/platform_types.h"

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
Error BigInt_createConstRef(const U64 *ptr, U64 ptrCount, BigInt *big);			//const ref U64 ptr[ptrCount]
Error BigInt_createFromHex(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromDec(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromOct(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromBin(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromNyto(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromText(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Bool BigInt_free(BigInt *b, Allocator allocator);

Bool BigInt_mul(BigInt *a, BigInt b, Allocator allocator);		//Multiply on self and keep bit count
Bool BigInt_add(BigInt *a, BigInt b, Allocator allocator);		//Add on self and keep bit count
Bool BigInt_sub(BigInt *a, BigInt b, Allocator allocator);		//Subtract on self and keep bit count

Bool BigInt_xor(BigInt *a, BigInt b);
Bool BigInt_or(BigInt *a, BigInt b);
Bool BigInt_and(BigInt *a, BigInt b);

Bool BigInt_not(BigInt *a);

I8 BigInt_cmp(BigInt a, BigInt b);
Bool BigInt_eq(BigInt a, BigInt b);
Bool BigInt_neq(BigInt a, BigInt b);
Bool BigInt_lt(BigInt a, BigInt b);
Bool BigInt_leq(BigInt a, BigInt b);
Bool BigInt_gt(BigInt a, BigInt b);
Bool BigInt_geq(BigInt a, BigInt b);

//TODO: div and mod

Bool BigInt_trunc(BigInt *big, Allocator allocator);							//Gets rid of all hi bits that are unset

Buffer BigInt_bufferConst(BigInt b);
Buffer BigInt_buffer(BigInt b);

U16 BigInt_byteCount(BigInt b);
U16 BigInt_bitCount(BigInt b);

Error BigInt_hex(BigInt b, Allocator allocator, CharString *result);
Error BigInt_oct(BigInt b, Allocator allocator, CharString *result);
Error BigInt_dec(BigInt b, Allocator allocator, CharString *result);
Error BigInt_bin(BigInt b, Allocator allocator, CharString *result);
Error BigInt_nyto(BigInt b, Allocator allocator, CharString *result);

//U128

#if _PLATFORM_TYPE == EPlatform_Linux
	typedef __int128 U128;
#else
	typedef I32x4 U128;
#endif

U128 U128_create(const U8 data[16]);
U128 U128_createU64x2(U64 a, U64 b);
U128 U128_mul64(U64 a, U64 b);			//Multiply two 64-bit numbers to generate a 128-bit number
U128 U128_add64(U64 a, U64 b);			//Add two 64-bit numbers but keep the overflow bit

U128 U128_zero();
U128 U128_one();

U128 U128_xor(U128 a, U128 b);
U128 U128_or(U128 a, U128 b);
U128 U128_and(U128 a, U128 b);

U128 U128_not(U128 a);

I8 U128_cmp(U128 a, U128 b);
Bool U128_eq(U128 a, U128 b);
Bool U128_neq(U128 a, U128 b);
Bool U128_lt(U128 a, U128 b);
Bool U128_leq(U128 a, U128 b);
Bool U128_gt(U128 a, U128 b);
Bool U128_geq(U128 a, U128 b);

//U128 U128_div(U128 a, U128 b);
//U128 U128_mod(U128 a, U128 b);
U128 U128_mul(U128 a, U128 b);
U128 U128_add(U128 a, U128 b);
U128 U128_sub(U128 a, U128 b);

U128 U128_min(U128 a, U128 b);
U128 U128_max(U128 a, U128 b);
U128 U128_clamp(U128 a, U128 mi, U128 ma);
