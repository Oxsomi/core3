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

U128 U128_mul64(U64 au, U64 bu, Allocator alloc) {

	//We interpret au and bu as a U32[2] interleaved with an empty U32 each.
	// 
	//a0 a1 a2 a3 00 00 00 00 a4 a5 a6 a7 00 00 00 00
	//b0 b1 b2 b3 00 00 00 00 b4 b5 b6 b7 00 00 00 00
	//=
	//xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
	// 
	//This basically means we can use it as a multiply two U32s and not overflow.
	//However, this is not all that a multiplication has to do.
	//It has to do this operation for every single element and merge them together,
	//Respecting baseN offset.
	// 
	//For example:
	//1 2
	//3 4 x
	//=
	//--->1
	//0 2 
	//3 4 x
	//=
	//6 8
	//--->2
	//0 1
	//3 4 x
	//=
	//3 4 (shift e+1)
	//= 3 4 0
	//---> 4
	//340 + 68 = 408.
	//
	//a0 = 12 % 10 = 2
	//a1 = 12 / 10 = 1
	//
	//b0 = 34 % 10 = 4
	//b1 = 34 / 10 = 3
	//
	//From this we can understand that:
	//(a0 * b0) % 10 = (2 * 4) % 10 = 8 % 10 = 8 = digit0
	// c = ((a0 * b0) / 10 + (a0 * b1) + (a1 * b0))
	//   = (8 / 10 (int) + 6 + 4) % 10 = (0 + 6 + 4) = 10
	//c % 10 = 10 % 10 = 0 = digit1
	//((a1 * b1) + c / 10) % 10 = (3 + 10 / 10) = 4 = digit2
	//((a1 * b1) + c / 10) / 10 = 3 / 10 = 0 = digit3
	//0 4 0 8 = 1 2 * 3 4
	//
	// % 10 in baseU32 context is just a simple trunc from U64 to U32
	// / 10 in baseU32 context is just a simple shift by 32 from the U64

	#if _PLATFORM_TYPE == EPlatform_Linux

		return (__int128) au * (__int128) bu;

	#elif _SIMD == SIMD_NONE
		
		U64 a[2] = { au, 0 };
		U64 b[2] = { bu, 0 };

		BigInt aBig = (BigInt) { 0 }, bBig = (BigInt) { 0 };

		if(
			(BigInt_createRef(a, 2, &aBig)).genericError || 
			(BigInt_createRef(b, 2, &bBig)).genericError || 
			!BigInt_mul(&aBig, bBig, alloc)
		)
			return U128_zero();
			
		return U128_createU64x2(a[0], a[1]);

	#else

		I32x4 mask4 = I32x4_xxxx4(U16_MAX);

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
		I32x4 b = I32x4_zxwy(I32x4_blend(low, hiSwiz, 0b0011));

		//Then multiply the U32x4 with the other U32x4.

		I32x4 mul = I32x4_mul(a, b);

		//Split up the result to get the overflow and real parts

		I32x4 lows = I32x4_and(mul, mask4);
		I32x4 highs = I32x4_and(I32x4_rshByte(mul, 2), mask4);

		//TODO: Merge overflow and other bits

		lows;
		highs;

		return mul;

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

//U128 U128_not(U128 a) { return I32x4_not(a); }

Bool U128_eq(U128 a, U128 b) { return I32x4_eq4(a, b); }
Bool U128_neq(U128 a, U128 b) { return !U128_eq(a, b); }
Bool U128_lt(U128 a, U128 b);
Bool U128_leq(U128 a, U128 b);
Bool U128_gt(U128 a, U128 b);
Bool U128_geq(U128 a, U128 b);

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
