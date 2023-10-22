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

#include <stdio.h>

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

Bool BigInt_free(BigInt *a, Allocator allocator) {

	if(!a)
		return false;

	if(!a->isRef && a->length) {
		Buffer buf = Buffer_createManagedPtr((U8*)a->data, a->length * sizeof(U64));
		Buffer_free(&buf, allocator);
	}

	*a = (BigInt) { 0 };
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

Bool BigInt_add(BigInt *a, BigInt b) {

	if(!a || a->isConst)
		return false;

	Bool overflow = false;
	U64 len = U64_min(a->length, b.length);

	for(U64 i = 0; i < len; ++i) {

		U64 prev = a->data[i];
		U64 add = prev + b.data[i];
		Bool nextOverflow = add < prev;

		if(overflow) {
			prev = add;
			++add; 
			nextOverflow |= add < prev;
		}

		((U64*)a->data)[i] = add;
		overflow = nextOverflow;
	}

	U64 next = b.length;

	while(overflow && next < a->length) {
		U64 prev = a->data[next];
		overflow = (++((U64*)a->data)[next]) < prev;
	}

	return true;
}

Bool BigInt_sub(BigInt *a, BigInt b) {

	if(!a || a->isConst)
		return false;

	Bool underflow = false;
	U64 len = U64_min(a->length, b.length);

	for(U64 i = 0; i < len; ++i) {

		U64 prev = a->data[i];
		U64 sub = prev - b.data[i];
		Bool nextUnderflow = sub > prev;

		if(underflow) {
			prev = sub;
			--sub; 
			nextUnderflow |= sub > prev;
		}

		((U64*)a->data)[i] = sub;
		underflow = nextUnderflow;
	}

	U64 next = b.length;

	while(underflow && next < a->length) {
		U64 prev = a->data[next];
		underflow = (--((U64*)a->data)[next]) > prev;
	}

	return true;
}

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

Bool BigInt_lsh(BigInt *a, U16 bits) {

	if(!a || a->isConst)
		return false;

	if(!bits)
		return true;

	if(bits >= BigInt_bitCount(*a))
		return BigInt_and(a, BigInt_createNull());

	for(U64 i = a->length - 1; i != U64_MAX; --i) {
		
		U64 right = i < (bits >> 6) ? 0 : a->data[i - (bits >> 6)];
		U64 left = i <= (bits >> 6) ? 0 : a->data[i - (bits >> 6) - 1];

		U64 shift = bits & 63;

		right <<= shift;
		left >>= 64 - shift;

		((U64*)a->data)[i] = left | right;
	}

	return true;
}

Bool BigInt_rsh(BigInt *a, U16 bits) {

	if(!a || a->isConst)
		return false;

	if(!bits)
		return true;

	if(bits >= BigInt_bitCount(*a))
		return BigInt_and(a, BigInt_createNull());

	for(U64 i = 0; i < a->length; ++i) {

		U64 left = i + (bits >> 6) > a->length - 1 ? 0 : a->data[i + (bits >> 6)];
		U64 right = i + (bits >> 6) >= a->length - 1 ? 0 : a->data[i + (bits >> 6) + 1];

		U64 shift = bits & 63;

		right <<= 64 - shift;
		left >>= shift;

		((U64*)a->data)[i] = left | right;
	}

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

BigInt *BigInt_min(BigInt *a, BigInt *b) { return !a ? b : (!b ? a : (BigInt_leq(*a, *b) ? a : b)); }
BigInt *BigInt_max(BigInt *a, BigInt *b) { return !a ? b : (!b ? a : (BigInt_geq(*a, *b) ? a : b)); }
BigInt *BigInt_clamp(BigInt *a, BigInt *mi, BigInt *ma) { return BigInt_max(BigInt_min(a, ma), mi); }

Error BigInt_resize(BigInt *a, U8 newSize, Allocator alloc) {

	if(!a)
		return Error_nullPointer(0);

	if(a->isRef)
		return Error_invalidOperation(0);

	if(a->length == newSize)
		return Error_none();

	if(!newSize) {
		BigInt_free(a, alloc);
		return Error_none();
	}

	BigInt temp = (BigInt) { 0 };
	Error err = BigInt_create((U16)newSize << 6, alloc, &temp);

	if(err.genericError)
		return err;

	if(!BigInt_set(&temp, *a, false, (Allocator) { 0 }))
		return Error_invalidState(0);

	BigInt_free(a, alloc);
	*a = temp;
	return err;
}

Bool BigInt_set(BigInt *a, BigInt b, Bool allowResize, Allocator alloc) {

	if(!a || a->isConst)
		return false;

	if (allowResize && a->length != b.length) {
	
		Error err = BigInt_resize(a, b.length, alloc);

		if(err.genericError)
			return false;
	}

	for(U64 i = 0; i < a->length && i < b.length; ++i)
		((U64*)a->data)[i] = b.data[i];

	for(U64 i = b.length; i < a->length; ++i)
		((U64*)a->data)[i] = 0;

	return true;
}

Error BigInt_copy(BigInt *a, Allocator alloc, BigInt *b) {

	if(!a || !b)
		return Error_nullPointer(!a ? 0 : 2);

	if(b->data)
		return Error_invalidParameter(2, 0);

	Error err = BigInt_create((U16)a->length << 6, alloc, b);

	if(err.genericError)
		return err;

	if(!BigInt_set(a, *b, false, (Allocator) { 0 }))
		return Error_invalidState(0);

	return err;
}

Bool BigInt_trunc(BigInt *big, Allocator allocator) {

	if(!big)
		return false;

	U8 i = big->length - 1;

	for(; i != U8_MAX; --i)
		if(big->data[i])
			break;

	return !BigInt_resize(big, i + 1, allocator).genericError;
}

Bool BigInt_mod(BigInt *a, BigInt b);
Bool BigInt_div(BigInt *a, BigInt b);

Buffer BigInt_bufferConst(BigInt b) { 
	return b.isConst ? Buffer_createNull() : Buffer_createRef((U64*)b.data, BigInt_byteCount(b));
}

Buffer BigInt_buffer(BigInt b) { return Buffer_createConstRef(b.data, BigInt_byteCount(b)); }

U16 BigInt_byteCount(BigInt b) { return (U16)(b.length * sizeof(U64)); }
U16 BigInt_bitCount(BigInt b) { return BigInt_byteCount(b) * 8; }

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

U128 U128_mul64(U64 au, U64 bu) {

	#if _PLATFORM_TYPE == EPlatform_Linux
		return (__uint128) au * (__uint128) bu;
	#else

		U32 D[4];

		//Multiply two U64 as U32 each
		//aXbx: 0 1
		//aXby: 1 2
		//overflow 2: D3

		const I32x4 auau = I32x4_xxyy(I32x4_createFromU64x2(au, 0));
		const I32x4 bubu = I32x4_xxyy(I32x4_createFromU64x2(bu, 0));

		const I32x4 aXbx = I32x4_mulU32x2AsU64x2(auau, I32x4_xxxx(bubu));
		const I32x4 aXby = I32x4_mulU32x2AsU64x2(auau, I32x4_zzzz(bubu));

		//D0 is stored in lowest 32 bytes of first U64

		D[0] = (U32)I32x4_x(aXbx);

		//Obtain hi and lo parts of multiplications

		const I32x4 low = I32x4_createFromU64x2(U32_MAX, U32_MAX);
		const I32x4 aXbxLo = I32x4_and(aXbx, low);
		const I32x4 aXbxHi = I32x4_and(I32x4_rshByte(aXbx, 4), low);

		const I32x4 aXbyLo = I32x4_and(aXby, low);
		const I32x4 aXbyHi = I32x4_and(I32x4_rshByte(aXby, 4), low);

		//Calculate D1 by adding the overflow to the two other numbers

		const I32x4 D1_2 = I32x4_addI64x2(aXbyLo, aXbxHi);					//Add overflows of D0 and D1 to D1 and D2
		const I32x4 D1_2Temp = I32x4_addI64x2(D1_2, I32x4_zwzw(aXbxLo));	//Add last D1 to combine final value

		D[1] = (U32) I32x4_x(D1_2Temp);

		//Calculate D2 by adding the overflow to aXby.y 

		I32x4 D2_3 = I32x4_and(I32x4_rshByte(D1_2Temp, 4), low);
		D2_3 = I32x4_addI64x2(D2_3, I32x4_xyxy(aXbyHi));
		D2_3 = I32x4_addI64x2(D2_3, I32x4_zwzw(D1_2));

		D[2] = (U32) I32x4_x(D2_3);

		//Calculate D3 by adding the overflow to aXby.y hi

		D[3] = I32x4_y(D2_3) + I32x4_z(aXbyHi);

		//Grab resulting digits

		return I32x4_create4(D[0], D[1], D[2], D[3]);

	#endif
}

#if _PLATFORM_TYPE == EPlatform_Linux

	U128 U128_zero() { return (__uint128) 0; }
	U128 U128_one() { return (__uint128) 1; }

	U128 U128_xor(U128 a, U128 b) { return a ^ b; }
	U128 U128_or(U128 a, U128 b) { return a | b; }
	U128 U128_and(U128 a, U128 b) { return a & b; }
	U128 U128_not(U128 a) {  return ~a; }
	U128 U128_add64(U64 a, U64 b) { return (__uint128)a + b; }
	Bool U128_eq(U128 a, U128 b) { return a == b; }
	U128 U128_add(U128 a, U128 b) { return a + b; }
	U128 U128_sub(U128 a, U128 b) {  return a - b; }

	U128 U128_lsh(U128 a, U8 x) { return a << x; }
	U128 U128_rsh(U128 a, U8 x) { return a >> x; }

#else

	U128 U128_zero() { return I32x4_zero(); }
	U128 U128_one() { return I32x4_create4(1, 0, 0, 0); }

	U128 U128_xor(U128 a, U128 b) { return I32x4_xor(a, b); }
	U128 U128_or(U128 a, U128 b) { return I32x4_or(a, b); }
	U128 U128_and(U128 a, U128 b) { return I32x4_and(a, b); }
	U128 U128_not(U128 a) {  return I32x4_not(a); }
	Bool U128_eq(U128 a, U128 b) { return I32x4_eq4(a, b); }

	U128 U128_add64(U64 a, U64 b) {
		U64 c = a + b;
		return I32x4_create4((U32)c, (U32)(c >> 32), c < a, 0);
	}

	U128 U128_add(U128 av, U128 bv) {

		const U64 *a = (const U64*)&av;
		const U64 *b = (const U64*)&bv;

		U64 lo = a[0] + b[0];
		U64 hi = lo < a[0];
		hi += a[1] + b[1];

		return U128_createU64x2(lo, hi);
	}

	U128 U128_sub(U128 a, U128 b) { 
		return U128_add(a, U128_add(U128_not(b), U128_one()));
	}

	U128 U128_lsh(U128 a, U8 x) { return I32x4_lsh128(a, x); }
	U128 U128_rsh(U128 a, U8 x) { return I32x4_rsh128(a, x); }

#endif

I8 U128_cmp(U128 a, U128 b) {

	//Early exit

	if(U128_eq(a, b))
		return 0;

	#if _PLATFORM_TYPE == EPlatform_Linux
		return a < b ? -1 : 1;
	#else

		const U64 *a64 = (const U64*)&a;
		const U64 *b64 = (const U64*)&b;

		if(a64[1] > b64[1] || (a64[1] == b64[1] && a64[0] > b64[0]))
			return 1;

		return -1;

	#endif
}

Bool U128_neq(U128 a, U128 b) { return !U128_eq(a, b); }
Bool U128_lt(U128 a, U128 b) { return U128_cmp(a, b) < 0; }
Bool U128_leq(U128 a, U128 b) { return U128_cmp(a, b) <= 0; }
Bool U128_gt(U128 a, U128 b) { return U128_cmp(a, b) > 0; }
Bool U128_geq(U128 a, U128 b) { return U128_cmp(a, b) >= 0; }

U128 U128_div(U128 a, U128 b);
U128 U128_mod(U128 a, U128 b);
U128 U128_mul(U128 a, U128 b);

U128 U128_min(U128 a, U128 b) { return U128_leq(a, b) ? a : b; }
U128 U128_max(U128 a, U128 b) { return U128_geq(a, b) ? a : b; }
U128 U128_clamp(U128 a, U128 mi, U128 ma) { return U128_max(U128_min(a, ma), mi); }
