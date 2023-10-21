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

#include "math/big_int.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/time.h"

//BigInt

BigInt BigInt_createNull() { return (BigInt) { .isRef = true, .isConst = true }; }

Error BigInt_create(U16 bitCount, Allocator alloc, BigInt *big) {

	U64 u64s = (bitCount + 63) >> 6;

	if(u64s >> 8)
		return Error_outOfBounds(0, bitCount, (U64)U8_MAX << 6);

	Buffer buffer = Buffer_createNull();
	Error err = Buffer_createEmptyBytes(u64s * sizeof(U64), alloc, &buffer);

	if(err.genericError)
		return err;

	*big = (BigInt) { .data = (const U64*) buffer.ptr, .length = (U8) u64s };
	return err;
}

Error BigInt_createRef(U64 *ptr, U64 ptrCount, BigInt *big) {

	if(!big)
		return Error_nullPointer(2);

	if(big->data)
		return Error_invalidParameter(2, 0);

	if(ptrCount >> 8)
		return Error_outOfBounds(1, ptrCount, U8_MAX);

	*big = (BigInt) { .data = ptr, .isConst = false, .isRef = true, .length = (U8) ptrCount };
	return Error_none();
}

Error BigInt_createConstRef(const U64 *ptr, U64 ptrCount, BigInt *big) {

	if(!big)
		return Error_nullPointer(2);

	if(big->data)
		return Error_invalidParameter(2, 0);

	if(ptrCount >> 8)
		return Error_outOfBounds(1, 0, U8_MAX);

	*big = (BigInt) { .data = ptr, .isConst = true, .isRef = true, .length = (U8) ptrCount };
	return Error_none();
}

Error BigInt_createFromHex(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromDec(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromOct(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromBin(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromNyto(CharString text, U16 bitCount, Allocator allocator, BigInt *big);
Error BigInt_createFromText(CharString text, U16 bitCount, Allocator allocator, BigInt *big);

Bool BigInt_free(BigInt *b, Allocator allocator) {

	if(!b)
		return false;

	if(!b->isRef && b->length) {
		Buffer buf = Buffer_createManagedPtr((U8*)b->data, b->length * sizeof(U64));
		Buffer_free(&buf, allocator);
	}

	*b = (BigInt) { 0 };
	return true;
}

Bool BigInt_mul(BigInt *a, BigInt b, Allocator allocator) {

	if(!a || a->isConst || !a->length)
		return false;

	if(!b.length)
		return BigInt_and(a, b);

	BigInt temp = (BigInt) { 0 };
	Error err = BigInt_create(BigInt_bitCount(*a), allocator, &temp);

	if(err.genericError)
		return false;

	U32 digitsA = (U32)a->length * 2;
	U32 digitsB = (U32)b.length * 2;

	U32 *dst = (U32*) temp.data;
	const U32 *aptr = (const U32*) a->data;
	const U32 *bptr = (const U32*) b.data;

	for (U32 i = 0; i < digitsA; ++i) {

		U64 mul = dst[i];

		//Shoot ray (diagonally) through a Brune matrix.
		//The concept is as follows:
		// 
		//   a0 a1 a2 a3
		//b0 0  1  2  3
		//b1 1  2  3  4
		//b2 2  3  4  5
		//b3 3  4  5  6
		//
		//With any number system, (a0 * b0) % base will result into the smallest digit.
		//While (a0 * b0) / base will result into the overflow.
		//This overflow is then added to the next which will calculate the next row.
		//In this case diagonal 1 would be (a1 * b0 + b1 * a0 + overflow).
		//The digit would be obtained through % base and then overflow is computed again.
		//This process is continued until it hits the desired number of digits.
		//
		//If we use base as 2^32 we can essentially process per U32 and then use a U64 to catch the overflow.
		//Truncating it is a simple U32 cast and/or shift.
		//When 128 bit numbers are available (hardware accelerated) this could be extended the same way to speed it up.

		U32 startX = (U32) U64_min(i, digitsA - 1);
		U32 startRayT = i - startX;
		U32 endRayT = (U32) U64_min(i, digitsB - 1) - startRayT;

		for (U32 t = startRayT; t <= endRayT; ++t) {

			U64 x = i - t;
			U64 y = t;

			U64 prevMul = mul;
			mul += (U64) aptr[x] * bptr[y];

			if(mul < prevMul && i + 2 < digitsA) {			//Overflow in our overflow.

				U64 j = i + 2, v = 0;

				do {										//Keep on adding overflow
					v = ++dst[j++];
				} while(!v && j < digitsA);
			}
		}

		dst[i] = (U32) mul;

		if(i + 1 < digitsA) {

			U64 prev = dst[i + 1];
			dst[i + 1] += (U32) (mul >> 32);

			if (dst[i + 1] < prev && i + 2 < digitsA) {		//Overflow in our overflow.

				U64 j = i + 2, v = 0;

				do {										//Keep on adding overflow
					v = ++dst[j++];
				} while(!v && j < digitsA);
			}
		}
	}

	Bool res = BigInt_and(a, BigInt_createNull()) && BigInt_or(a, temp);
	BigInt_free(&temp, allocator);
	return res;
}

Bool BigInt_add(BigInt *a, BigInt b, Allocator alloc);		//Add on self and keep bit count
Bool BigInt_sub(BigInt *a, BigInt b, Allocator alloc);		//Subtract on self and keep bit count

Bool BigInt_xor(BigInt *a, BigInt b) {

	if(!a || a->isConst)
		return false;

	for(U64 i = 0; i < a->length && i < b.length; ++i)
		((U64*)a->data)[i] ^= b.data[i];

	return true;
}

Bool BigInt_or(BigInt *a, BigInt b) {

	if(!a || a->isConst)
		return false;

	for(U64 i = 0; i < a->length && i < b.length; ++i)
		((U64*)a->data)[i] |= b.data[i];

	return true;
}

Bool BigInt_and(BigInt *a, BigInt b) {

	if(!a || a->isConst)
		return false;

	U64 j = U64_min(a->length, b.length);

	for(U64 i = 0; i < j; ++i)
		((U64*)a->data)[i] &= b.data[i];

	for(U64 i = b.length; i < a->length; ++i)
		((U64*)a->data)[i] = 0;

	return true;
}

Bool BigInt_not(BigInt *a) {

	if(!a || a->isConst)
		return false;

	for(U64 i = 0; i < a->length; ++i)
		((U64*)a->data)[i] = ~a->data[i];

	return true;
}

I8 BigInt_cmp(BigInt a, BigInt b) {

	U64 biggestLen = U64_max(a.length, b.length);

	for (U64 i = biggestLen - 1; i != U64_MAX; --i) {

		U64 ai = i >= a.length ? 0 : a.data[i];
		U64 bi = i >= b.length ? 0 : b.data[i];

		if(ai > bi)
			return 1;		//Greater

		else if(ai < bi)
			return -1;		//Less
	}

	return 0;				//Equal
}

Bool BigInt_eq(BigInt a, BigInt b) { return !BigInt_cmp(a, b); }
Bool BigInt_neq(BigInt a, BigInt b) { return BigInt_cmp(a, b); }
Bool BigInt_lt(BigInt a, BigInt b) { return BigInt_cmp(a, b) < 0; }
Bool BigInt_leq(BigInt a, BigInt b) { return BigInt_cmp(a, b) <= 0; }
Bool BigInt_gt(BigInt a, BigInt b) { return BigInt_cmp(a, b) > 0; }
Bool BigInt_geq(BigInt a, BigInt b) { return BigInt_cmp(a, b) >= 0; }

//TODO: div and mod

Bool BigInt_trunc(BigInt *big, Allocator allocator);							//Gets rid of all hi bits that are unset

Buffer BigInt_bufferConst(BigInt b) { 
	return b.isConst ? Buffer_createNull() : Buffer_createRef((U64*)b.data, BigInt_byteCount(b));
}

Buffer BigInt_buffer(BigInt b) { return Buffer_createConstRef(b.data, BigInt_byteCount(b)); }

U16 BigInt_byteCount(BigInt b) { return (U16)(b.length * sizeof(U64)); }
U16 BigInt_bitCount(BigInt b) { return (U16)(BigInt_byteCount(b) * 8); }

Error BigInt_hex(BigInt b, Allocator allocator, CharString *result);
Error BigInt_oct(BigInt b, Allocator allocator, CharString *result);
Error BigInt_dec(BigInt b, Allocator allocator, CharString *result);
Error BigInt_bin(BigInt b, Allocator allocator, CharString *result);
Error BigInt_nyto(BigInt b, Allocator allocator, CharString *result);

//U128

U128 U128_create(const U8 data[16]) {
	return I32x4_createFromU64x2(((const U64*)data)[0], ((const U64*)data)[1]);
}

U128 U128_createU64x2(U64 a, U64 b) {
	return I32x4_createFromU64x2(a, b);
}

#include <stdio.h>

U128 U128_mul64(U64 au, U64 bu, Allocator alloc) {

	alloc;

	#if _PLATFORM_TYPE == EPlatform_Linux

		return (__int128) au * (__int128) bu;

	#elif _SIMD == SIMD_NONE

		U64 clocks = Time_clocks();
		
		U64 a[2] = { au, 0 };
		U64 b[2] = { bu, 0 };

		BigInt aBig = (BigInt) { 0 }, bBig = (BigInt) { 0 };

		if(
			(BigInt_createRef(a, 2, &aBig)).genericError || 
			(BigInt_createRef(b, 2, &bBig)).genericError || 
			!BigInt_mul(&aBig, bBig, alloc)
		)
			return U128_zero();
			
		U128 result = U128_createU64x2(a[0], a[1]);
		U64 deltaClocks = Time_clocksElapsed(clocks);
		printf("%llu\n", deltaClocks);
		return result;

	#else

		U64 clocks = Time_clocks();
		I32x4 mask4 = I32x4_xxxx4(U16_MAX);

		//TODO: Write this in a more simplified way, currently this doesn't use mul_epu32, 
		//		but that'd allow to use 64-bit int mul, greatly simplifying the process.
		//		Currently the compiler understands this anyways on optimized mode.
		//https://stackoverflow.com/questions/28807341/simd-signed-with-unsigned-multiplication-for-64-bit-64-bit-to-128-bit

		U16 D[8];

		//Split up a and b as the following:
		//a0 a1 00 00 a4 a5 00 00 b0 b1 00 00 b4 b5 00 00
		//a2 a3 00 00 a6 a7 00 00 b2 b3 00 00 b6 b7 00 00

		I32x4 low = I32x4_createFromU64x2(au, bu);

		I32x4 hi = I32x4_and(I32x4_rshByte(low, 2), mask4);
		low = I32x4_and(low, mask4);

		//Merge the two together so it becomes
		//a0 a1 00 00 a2 a3 00 00 a4 a5 00 00 a6 a7 00 00
		//b0 b1 00 00 b2 b3 00 00 b4 b5 00 00 b6 b7 00 00

		I32x4 hiSwiz = I32x4_zwxy(hi);
		I32x4 a = I32x4_xzyw(I32x4_blend(low, hiSwiz, 0b1100));
		I32x4 b = I32x4_blend(low, hiSwiz, 0b0011);					//zxwy = xyzw. Not done here to avoid swizzle

		//Then multiply the U32x4 with the other U32x4.

		I32x4 aXbx = I32x4_mul(a, I32x4_zzzz(b));
		I32x4 aXby = I32x4_mul(a, I32x4_xxxx(b));
		I32x4 aXbz = I32x4_mul(a, I32x4_wwww(b));
		I32x4 aXbw = I32x4_mul(a, I32x4_yyyy(b));

		U32 tempD6 = I32x4_w(aXbw);
		D[0] = (U16)I32x4_x(aXbx);

		//Swizzle Brune matrix to allow adding in parallel
		// 
		//aXbx: 0 1 2 3
		//aXby: 1 2 3 4
		//aXbz: 2 3 4 5
		//aXbw: 3 4 5 6
		// 
		//Gets swizzled to:
		//aXbx: 0 1 2 3 (no change)
		//aXby: 4 1 2 3
		//aXbz: 4 5 2 3
		//aXbw: 4 5 6 3

		aXby = I32x4_wxyz(aXby);
		aXbz = I32x4_zwxy(aXbz);
		aXbw = I32x4_yzwx(aXbw);

		//Grab hi and low of mul of each type

		I32x4 aXbxHi = I32x4_and(I32x4_rshByte(aXbx, 2), mask4);
		aXbx = I32x4_and(aXbx, mask4);

		I32x4 aXbyHi = I32x4_and(I32x4_rshByte(aXby, 2), mask4);
		aXby = I32x4_and(aXby, mask4);

		I32x4 aXbzHi = I32x4_and(I32x4_rshByte(aXbz, 2), mask4);
		aXbz = I32x4_and(aXbz, mask4);

		I32x4 aXbwHi = I32x4_and(I32x4_rshByte(aXbw, 2), mask4);
		aXbw = I32x4_and(aXbw, mask4);

		//We store hi and lo separately because 32 bit isn't enough to prevent overflow.
		//We actually use 48 bit (32-bit lo overflows after 16-bit into 32-bit hi)
		//Do two 1, 2 and 3 additions.
		//aXbx: 0 1 2 3
		//aXby: X 1 2 3 +	(X = unused)

		I32x4 tmp = I32x4_wxww(I32x4_trunc2(aXbxHi));
		I32x4 hiAddr = I32x4_add(aXbxHi, aXbyHi);												//High addition
		I32x4 loAddr = I32x4_add(I32x4_add(aXbx, aXby), tmp);		//Low addition (add overflow)

		hiAddr = I32x4_add(hiAddr, I32x4_and(I32x4_rshByte(loAddr, 2), mask4));				//Handle low overflow
		loAddr = I32x4_and(loAddr, mask4);

		D[1] = (U16) I32x4_y(loAddr);

		//Do two 4, 2 and 3 additions.
		//adder: 4 X 2 3
		//aXbz:  4 X 2 3 +	(X = unused)

		tmp = I32x4_wwyw(I32x4_trunc2(hiAddr));
		hiAddr = I32x4_blend(hiAddr, aXbyHi, 0b0001);		//Blend in D4
		loAddr = I32x4_blend(loAddr, aXby,   0b0001);

		hiAddr = I32x4_add(hiAddr, aXbzHi);
		loAddr = I32x4_add(loAddr, I32x4_add(aXbz, tmp));

		hiAddr = I32x4_add(hiAddr, I32x4_and(I32x4_rshByte(loAddr, 2), mask4));				//Handle low overflow
		loAddr = I32x4_and(loAddr, mask4);

		D[2] = (U16) I32x4_z(loAddr);

		//Do two 4, 5 and 3 additions.
		//adder: 4 5 X 3
		//aXbz:  4 5 X 3 +	(X = unused)

		tmp = I32x4_wwwz(I32x4_trunc3(hiAddr));
		hiAddr = I32x4_blend(hiAddr, aXbzHi, 0b0010);		//Blend in D5
		loAddr = I32x4_blend(loAddr, aXbz,   0b0010);

		hiAddr = I32x4_add(hiAddr, aXbwHi);
		loAddr = I32x4_add(loAddr, I32x4_add(aXbw, tmp));

		hiAddr = I32x4_add(hiAddr, I32x4_and(I32x4_rshByte(loAddr, 2), mask4));				//Handle low overflow
		loAddr = I32x4_and(loAddr, mask4);

		D[3] = (U16) I32x4_w(loAddr);

		//Add tail (D4 + OD3)
		//adder: 4 5 X X

		tmp = I32x4_create4(0, 0, 0, (I32) U32_MAX);
		tmp = I32x4_wxxx(I32x4_and(hiAddr, tmp));
		loAddr = I32x4_add(loAddr, tmp);

		hiAddr = I32x4_add(hiAddr, I32x4_and(I32x4_rshByte(loAddr, 2), mask4));				//Handle low overflow
		loAddr = I32x4_and(loAddr, mask4);

		D[4] = (U16) I32x4_x(loAddr);

		//Add tail (D5 + OD4)
		//adder: X 5 X X

		U64 tail = ((U64)I32x4_y(hiAddr) << 16) + I32x4_y(loAddr) + I32x4_x(hiAddr);
		D[5] = (U16) tail;
		tail >>= 16;

		//Add tail (D5 + OD4)
		//adder: X 5 X X

		tail += tempD6;
		D[6] = (U16) tail;
		tail >>= 16;

		D[7] = (U16) tail;

		//Grab resulting digits

		U64 deltaClocks = Time_clocksElapsed(clocks);
		printf(
			"%llu clocks %04X%04X%04X%04X%04X%04X%04X%04X\n", 
			deltaClocks,
			D[0], D[1], D[2], D[3], D[4], D[5], D[6], D[7]
		);

		return I32x4_createFromU64x2(*(const U64*)&D[0], *(const U64*)&D[4]);

	#endif
}

U128 U128_add64(U64 a, U64 b) {
	U64 c = a + b;
	return I32x4_create4((U32)c, (U32)(c >> 32), c < a, 0);
}

U128 U128_zero() { return I32x4_zero(); }
U128 U128_one() { return I32x4_create4(0, 0, 0, 1); }

U128 U128_xor(U128 a, U128 b) { return I32x4_xor(a, b); }
U128 U128_or(U128 a, U128 b) { return I32x4_or(a, b); }
U128 U128_and(U128 a, U128 b) { return I32x4_and(a, b); }

U128 U128_not(U128 a) { return I32x4_not(a); }

I8 U128_cmp(U128 a, U128 b) {

	//Early exit

	if(U128_eq(a, b))
		return 0;

	//The I32x4 data contains 0x00... -> 0x7F... as >0
	//And then 0x80... -> 0xFF... as <0.
	//So compare won't work correctly since it contains unsigned data.
	//To hack around this, we have to mask out everything except sign bits.
	//Then we can use the sign bits to generate a final verdict

	I32x4 aSign = I32x4_lt(a, I32x4_zero());
	I32x4 bSign = I32x4_lt(b, I32x4_zero());
	I32x4 signEq = I32x4_eq(aSign, bSign);
	I32x4 signGt = I32x4_gt(aSign, bSign);

	I32x4 mask = I32x4_xxxx4(I32_MAX);
	a = I32x4_and(a, mask);
	b = I32x4_and(b, mask);

	I32x4 signSameAndGt = I32x4_and(signEq, I32x4_gt(a, b));
	I32x4 result = I32x4_or(signGt, signSameAndGt);

	for(U8 i = 0; i < 4; ++i)
		if(I32x4_get(result, i))
			return 1;

	return -1;
}

Bool U128_eq(U128 a, U128 b) { return I32x4_eq4(a, b); }
Bool U128_neq(U128 a, U128 b) { return !U128_eq(a, b); }
Bool U128_lt(U128 a, U128 b) { return U128_cmp(a, b) < 0; }
Bool U128_leq(U128 a, U128 b) { return U128_cmp(a, b) <= 0; }
Bool U128_gt(U128 a, U128 b) { return U128_cmp(a, b) > 0; }
Bool U128_geq(U128 a, U128 b) { return U128_cmp(a, b) >= 0; }

//U128 U128_div(U128 a, U128 b);
//U128 U128_mod(U128 a, U128 b);
U128 U128_mul(U128 a, U128 b);
U128 U128_add(U128 a, U128 b);
U128 U128_sub(U128 a, U128 b);

//U128 U128_negate(U128 a) { return U128_sub(U128_zero(), a); }
//U128 U128_complement(U128 a) { return U128_sub(U128_one(), a); }

U128 U128_min(U128 a, U128 b);
U128 U128_max(U128 a, U128 b);
U128 U128_clamp(U128 a, U128 mi, U128 ma);
