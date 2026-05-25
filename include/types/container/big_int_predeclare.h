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

//types/container/big_int_predeclare.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct Allocator Allocator;
typedef struct CharString CharString;

//BigInt allow up to 16320 bit ints but isn't well optimized.
//For optimized versions please use U128 (and U256 in the future) since they don't dynamically allocate,
//and because big int can be varying length so no SIMD optimizations are present.
typedef struct BigInt {

	union {
		const U64 *data;	//Aligned
		U64 *dataNonConst;	//Only if !isConst
	};

	Bool isConst;
	Bool isRef;
	U8 length;		//In U64s
	U8 pad;

} BigInt;

static inline BigInt BigInt_createNull() { BigInt bi = { 0 }; return bi; }

Bool BigInt_create(U16 bitCount, const Allocator *allocator, BigInt *big, Error *e_rr);		//Aligns bitCount to 64.
Bool BigInt_createRef(U64 *ptr, U64 ptrCount, BigInt *big, Error *e_rr);					//ref U64 ptr[ptrCount]
Bool BigInt_createRefConst(const U64 *ptr, U64 ptrCount, BigInt *big, Error *e_rr);			//const ref U64 ptr[ptrCount]
Bool BigInt_createCopy(BigInt *a, const Allocator *alloc, BigInt *b, Error *e_rr);

//bitCount set to 0 indicates "auto".
//bitCount set to U16_MAX indicates "already allocated"
typedef struct BigIntCreate {
	const CharString *text;
	U16 bitCount;
	const Allocator *alloc;
	BigInt *big;
} BigIntCreate;

Bool BigInt_createFromDec(const BigIntCreate *bigIntCreate, Error *e_rr);
Bool BigInt_createFromString(const BigIntCreate *bigIntCreate, Error *e_rr);

void BigInt_free(BigInt *a, const Allocator *allocator);
	
typedef enum EIntEncoding {
	EIntEncoding_Hex,
	EIntEncoding_Bin,
	EIntEncoding_Oct,
	EIntEncoding_Nyto,
	EIntEncoding_Count,
	EIntEncoding_Base2End = EIntEncoding_Count		//TODO: If dec is added, keep this in mind!
} EIntEncoding;

Bool BigInt_createFromBase2Type(const BigIntCreate *bigIntCreate, EIntEncoding type, Error *e_rr);

typedef struct BigIntStringify {
	Bool leadingZeros;
	const Allocator *alloc;
	CharString *result;
} BigIntStringify;

Bool BigInt_base2(const BigIntStringify *stringify, EIntEncoding type, BigInt b, Error *e_rr);

//Exists for the whole purpose of non base2 (like dec)
Bool BigInt_toString(const BigIntStringify *stringify, EIntEncoding encoding, BigInt b, Error *e_rr);

#ifdef __cplusplus
	}
#endif
