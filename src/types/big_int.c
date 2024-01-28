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

#include "types/big_int.h"
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
		return Error_outOfBounds(0, bitCount, (U64)U8_MAX << 6, "BigInt_create()::bitCount out of bounds");

	Buffer buffer = Buffer_createNull();
	Error err = Buffer_createEmptyBytes(u64s * sizeof(U64), alloc, &buffer);

	if(err.genericError)
		return err;

	*big = (BigInt) { .data = (const U64*) buffer.ptr, .length = (U8) u64s };
	return err;
}

Error BigInt_createRef(U64 *ptr, U64 ptrCount, BigInt *big) {

	if(!big)
		return Error_nullPointer(2, "BigInt_createRef()::big is required");

	if(big->data)
		return Error_invalidParameter(2, 0, "BigInt_createRef()::big->data is required");

	if(ptrCount >> 8)
		return Error_outOfBounds(1, ptrCount, U8_MAX, "BigInt_createRef()::ptrCount is more than the BigInt limit (256 U64s)");

	*big = (BigInt) { .data = ptr, .isConst = false, .isRef = true, .length = (U8) ptrCount };
	return Error_none();
}

Error BigInt_createConstRef(const U64 *ptr, U64 ptrCount, BigInt *big) {

	if(!big)
		return Error_nullPointer(2, "BigInt_createConstRef()::big is required");

	if(big->data)
		return Error_invalidParameter(2, 0, "BigInt_createConstRef()::big->data is required");

	if(ptrCount >> 8)
		return Error_outOfBounds(1, 0, U8_MAX, "BigInt_createConstRef()::ptrCount is more than the BigInt limit (256 U64s)");

	*big = (BigInt) { .data = ptr, .isConst = true, .isRef = true, .length = (U8) ptrCount };
	return Error_none();
}

typedef enum EBase2Type {
	EBase2Type_Hex,
	EBase2Type_Bin,
	EBase2Type_Oct,
	EBase2Type_Nyto
} EBase2Type;

static const C8 *base2Types[] = { "0x", "0b", "0o", "0n" };
static const U8 base2Count[] = { 4, 1, 3, 6 };

Error BigInt_createFromBase2Type(CharString text, U16 bitCount, Allocator alloc, BigInt *big, EBase2Type type) {

	if(!big)
		return Error_nullPointer(3, "BigInt_createFromBase2Type()::big is required");

	Bool prefix = CharString_startsWithStringInsensitive(text, CharString_createRefCStrConst(base2Types[type]));
	U8 prefixChars = prefix * 2;
	U8 countPerChar = base2Count[type];

	U64 chars = CharString_length(text) - prefixChars;

	if(!chars)
		return Error_invalidParameter(0, 0, "BigInt_createFromBase2Type()::text starts with 0[xbon] but doesn't have content");

	if (bitCount == U16_MAX) {

		if(!big->data)
			return Error_nullPointer(3, "BigInt_createFromBase2Type()::big->data is required if bitCount is auto (U16_MAX)");

		bitCount = BigInt_bitCount(*big);
	}

	else if(!bitCount)
		bitCount = (U16) U64_min(chars * countPerChar, 0xFF * 64);

	else {

		if(big->data)
			return Error_invalidParameter(
				3, 0, "BigInt_createFromBase2Type()::big->data should be NULL if bitCount isn't auto (U16_MAX)"
			);

		if(bitCount > 0xFF * 64)
			return Error_invalidParameter(1, 0, "BigInt_createFromBase2Type()::bitCount is out of bounds (>16320)");
	}

	if(chars * countPerChar > ((bitCount + countPerChar - 1) / countPerChar * countPerChar))
		return Error_outOfBounds(
			0, chars * countPerChar, bitCount, "BigInt_createFromBase2Type()::text would overflow BigInt bitCount"
		);

	Bool allocated = false;
	Error err = Error_none();

	if (!big->data) {

		err = BigInt_create(bitCount, alloc, big);

		if(err.genericError)
			return err;

		allocated = true;
	}

	for (
		U64 i = CharString_length(text) - 1, j = 0, k = 0;
		i >= prefixChars && i != U64_MAX;
		--i, ++j
	) {
		U8 v = 0;

		switch (type) {
			case EBase2Type_Hex:	v = C8_hex(text.ptr[i]);	break;
			case EBase2Type_Bin:	v = C8_bin(text.ptr[i]);	break;
			case EBase2Type_Oct:	v = C8_oct(text.ptr[i]);	break;
			case EBase2Type_Nyto:	v = C8_nyto(text.ptr[i]);	break;
		}

		if(v == U8_MAX)
			_gotoIfError(clean, Error_invalidParameter(0, 1, "BigInt_createFromBase2Type()::text contains invalid char"));

		switch (type) {

			case EBase2Type_Hex:	((U8*)big->data)[j >> 1] |= v << (countPerChar * (j & 1));	break;
			case EBase2Type_Bin:	((U8*)big->data)[j >> 3] |= v << (countPerChar * (j & 7));	break;

			case EBase2Type_Nyto:
			case EBase2Type_Oct: {

				U64 lo = (U64)v << ((countPerChar * j) & 63);

				((U64*)big->data)[k] |= lo;

				if (((countPerChar * j) & ~63) != ((countPerChar * (j + 1)) & ~63)) {
					U64 hi = (U64)v >> (64 - (countPerChar * j) & 63);
					((U64*)big->data)[++k] |= hi;
				}

				break;
			}
		}
	}

	if(bitCount & 63) {		//Fix last U64 to handle out of bounds

		if(big->data[big->length - 1] >> (bitCount & 63))
			return Error_outOfBounds(0, bitCount, bitCount, "BigInt_createFromBase2Type()::text contains too much data");
	}

	goto success;

success:
	allocated = false;		//avoid allocation

clean:

	if(allocated)
		BigInt_free(big, alloc);

	return err;
}

Error BigInt_createFromHex(CharString text, U16 bitCount, Allocator alloc, BigInt *big) {
	return BigInt_createFromBase2Type(text, bitCount, alloc, big, EBase2Type_Hex);
}

Error BigInt_createFromOct(CharString text, U16 bitCount, Allocator alloc, BigInt *big) {
	return BigInt_createFromBase2Type(text, bitCount, alloc, big, EBase2Type_Oct);
}

Error BigInt_createFromBin(CharString text, U16 bitCount, Allocator alloc, BigInt *big) {
	return BigInt_createFromBase2Type(text, bitCount, alloc, big, EBase2Type_Bin);
}

Error BigInt_createFromNyto(CharString text, U16 bitCount, Allocator alloc, BigInt *big) {
	return BigInt_createFromBase2Type(text, bitCount, alloc, big, EBase2Type_Nyto);
}

#define _STRING_TO_U16(a, b) ((a) | ((U16)b << 8))

Error BigInt_createFromString(CharString text, U16 bitCount, Allocator alloc, BigInt *big) {

	if (CharString_length(text) > 2) {

		U16 start = *(const U16*) text.ptr;

		switch(start) {

			case _STRING_TO_U16('0', 'b'): case _STRING_TO_U16('0', 'B'):
				return BigInt_createFromBin(text, bitCount, alloc, big);

			case _STRING_TO_U16('0', 'x'): case _STRING_TO_U16('0', 'X'):
				return BigInt_createFromHex(text, bitCount, alloc, big);

			case _STRING_TO_U16('0', 'o'): case _STRING_TO_U16('0', 'O'):
				return BigInt_createFromOct(text, bitCount, alloc, big);

			case _STRING_TO_U16('0', 'n'): case _STRING_TO_U16('0', 'N'):
				return BigInt_createFromNyto(text, bitCount, alloc, big);
		}
	}

	return BigInt_createFromDec(text, bitCount, alloc, big);
}

Error BigInt_createFromDec(CharString text, U16 bitCount, Allocator alloc, BigInt *big) {

	if(!big)
		return Error_nullPointer(3, "BigInt_createFromDec()::big is required");

	if(!CharString_length(text))
		return Error_nullPointer(0, "BigInt_createFromDec()::text is required");

	if(CharString_length(text) > 4913)
		return Error_invalidParameter(0, 0, "BigInt_createFromDec()::text should be below 4913 characters (16320 bits)");

	U64 estBitCount = (U64) F64_ceil(F64_log2((F64)CharString_length(text)));

	if (bitCount == U16_MAX) {

		if(!big->data)
			return Error_nullPointer(3, "BigInt_createFromDec()::big->data is required when bitCount is auto (U16_MAX)");

		if(!BigInt_and(big, BigInt_createNull()))
			return Error_invalidState(0, "BigInt_createFromDec()::big clear failed");

		bitCount = BigInt_bitCount(*big);
	}

	else if(!bitCount)
		bitCount = (U16) U64_min(estBitCount, 0xFF * 64);

	else {

		if(big->data)
			return Error_invalidParameter(
				3, 0, "BigInt_createFromDec()::big->data should be NULL when bitCount isn't auto (U16_MAX)"
			);

		if(bitCount > 0xFF * 64)
			return Error_invalidParameter(1, 0, "BigInt_createFromBase2Type()::bitCount is out of bounds (>16320)");
	}

	if(estBitCount > 0xFF * 64 + 1)			//+1 to align to base10
		return Error_outOfMemory(0, "BigInt_createFromBase2Type() estBitCount is out of bounds (>16321)");

	Bool allocated = false;
	Error err = Error_none();
	BigInt temp = (BigInt) { 0 }, multiplier = (BigInt) { 0 };

	if (!big->data) {

		err = BigInt_create(bitCount, alloc, big);

		if(err.genericError)
			return err;

		allocated = true;
	}

	_gotoIfError(clean, BigInt_create(bitCount, alloc, &multiplier));
	_gotoIfError(clean, BigInt_create(bitCount, alloc, &temp));

	((U64*)multiplier.data)[0] = 1;		//Multiplier

	for (U64 i = CharString_length(text) - 1; i != U64_MAX; --i) {

		U8 v = C8_dec(text.ptr[i]);

		if(v == U8_MAX)
			_gotoIfError(clean, Error_invalidParameter(0, 1, "BigInt_createFromBase2Type()::text contains non alpha char"));

		((U8*)temp.data)[0] = v;

		if(!BigInt_mul(&temp, multiplier, alloc))
			_gotoIfError(clean, Error_invalidState(1, "BigInt_createFromBase2Type() mul failed"));

		if(!BigInt_add(big, temp))
			_gotoIfError(clean, Error_invalidState(2, "BigInt_createFromBase2Type() add failed"));

		//Multiply multiplier by 10

		if(!BigInt_and(&temp, BigInt_createNull()))
			_gotoIfError(clean, Error_invalidState(2, "BigInt_createFromBase2Type() clear failed"));

		((U8*)temp.data)[0] = 10;

		if(!BigInt_mul(&multiplier, temp, alloc))
			_gotoIfError(clean, Error_invalidState(3, "BigInt_createFromBase2Type() mul failed"));
	}

	if(bitCount & 63) {		//Fix last U64 to handle out of bounds

		if(big->data[big->length - 1] >> (bitCount & 63))
			return Error_outOfBounds(0, bitCount, bitCount, "BigInt_createFromBase2Type()::text contains too much data");
	}

	goto success;

success:
	allocated = false;		//avoid allocation

clean:

	BigInt_free(&multiplier, alloc);
	BigInt_free(&temp, alloc);

	if(allocated)
		BigInt_free(big, alloc);

	return err;
}

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

ECompareResult BigInt_cmp(BigInt a, BigInt b) {

	U64 biggestLen = U64_max(a.length, b.length);

	for (U64 i = biggestLen - 1; i != U64_MAX; --i) {

		U64 ai = i >= a.length ? 0 : a.data[i];
		U64 bi = i >= b.length ? 0 : b.data[i];

		if(ai > bi)
			return ECompareResult_Gt;

		else if(ai < bi)
			return ECompareResult_Lt;
	}

	return ECompareResult_Eq;
}

Bool BigInt_eq(BigInt a, BigInt b) { return BigInt_cmp(a, b) == ECompareResult_Eq; }
Bool BigInt_neq(BigInt a, BigInt b) { return BigInt_cmp(a, b) != ECompareResult_Eq; }
Bool BigInt_lt(BigInt a, BigInt b) { return BigInt_cmp(a, b) < ECompareResult_Eq; }
Bool BigInt_leq(BigInt a, BigInt b) { return BigInt_cmp(a, b) <= ECompareResult_Eq; }
Bool BigInt_gt(BigInt a, BigInt b) { return BigInt_cmp(a, b) > ECompareResult_Eq; }
Bool BigInt_geq(BigInt a, BigInt b) { return BigInt_cmp(a, b) >= ECompareResult_Eq; }

//TODO: div and mod

BigInt *BigInt_min(BigInt *a, BigInt *b) { return !a ? b : (!b ? a : (BigInt_leq(*a, *b) ? a : b)); }
BigInt *BigInt_max(BigInt *a, BigInt *b) { return !a ? b : (!b ? a : (BigInt_geq(*a, *b) ? a : b)); }
BigInt *BigInt_clamp(BigInt *a, BigInt *mi, BigInt *ma) { return BigInt_max(BigInt_min(a, ma), mi); }

Error BigInt_resize(BigInt *a, U8 newSize, Allocator alloc) {

	if(!a)
		return Error_nullPointer(0, "BigInt_resize()::a is required");

	if(a->isRef)
		return Error_invalidOperation(0, "BigInt_resize()::a is a ref, resize can't be called on that");

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
		return Error_invalidState(0, "BigInt_resize() set failed");

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

Error BigInt_createCopy(BigInt *a, Allocator alloc, BigInt *b) {

	if(!a || !b)
		return Error_nullPointer(!a ? 0 : 2, "BigInt_createCopy()::a or b is NULL");

	if(b->data)
		return Error_invalidParameter(2, 0, "BigInt_createCopy()::b->data should be NULL to avoid memory leaks");

	Error err = BigInt_create((U16)a->length << 6, alloc, b);

	if(err.genericError)
		return err;

	if(!BigInt_set(a, *b, false, (Allocator) { 0 }))
		return Error_invalidState(0, "BigInt_createCopy() set failed");

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

Buffer BigInt_buffer(BigInt b) { return Buffer_createRefConst(b.data, BigInt_byteCount(b)); }

U16 BigInt_byteCount(BigInt b) { return (U16)(b.length * sizeof(U64)); }
U16 BigInt_bitCount(BigInt b) { return BigInt_byteCount(b) * 8; }

U16 BigInt_bitScan(BigInt ai) {

	for (U64 i = ai.length - 1; i != U64_MAX; --i) {

		U64 a = ai.data[i];

		U64 offset = (Bool)((const U32*)&a)[1];
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

		if((a >> offset) & 1)
			return (U16)(i * 64 + offset);
	}

	return U16_MAX;
}

Error BigInt_isBase2(BigInt a, Allocator alloc, Bool *isBase2) {

	if(!isBase2)
		return Error_nullPointer(2, "BigInt_isBase2()->isBase2 is required");

	U16 v = BigInt_bitScan(a);

	if(v == U16_MAX) {
		*isBase2 = false;
		return Error_none();
	}

	BigInt temp = (BigInt) { 0 };
	Error err = BigInt_create(BigInt_bitCount(a), alloc, &temp);

	if(err.genericError)
		return err;

	*(U64*)temp.data = 1;

	if(!BigInt_lsh(&temp, v))
		_gotoIfError(clean, Error_invalidState(0, "BigInt_isBase2() lsh failed"));

	*isBase2 = BigInt_eq(temp, a);
clean:
	BigInt_free(&temp, alloc);
	return err;
}

Error BigInt_base2(BigInt b, Allocator alloc, CharString *result, EBase2Type type, Bool leadingZeros) {

	U8 countPerChar = base2Count[type];
	U64 len = U64_max(3, (((U64)b.length * 64 + countPerChar - 1) / countPerChar) + 2);
	Error err = CharString_resize(result, len, '0', alloc);

	if(err.genericError)
		return err;

	((C8*)result->ptr)[1] = base2Types[type][1];

	U64 firstLoc = len - 1;
	U8 mask = (1 << countPerChar) - 1;
	U64 i = len - 1;

	for (U64 j = 0, k = 0; j < len - 2 && k < b.length; ++j) {
	
		U64 v = b.data[k];
		v = ((U64)v >> ((countPerChar * j) & 63)) & mask;

		if (((countPerChar * j) & ~63) != ((countPerChar * (j + 1)) & ~63)) {

			U64 mask2 = ((U64)U64_MAX << (64 - (countPerChar * j) & 63)) & mask;

			if(k + 1 < b.length)
				v |= ((U64)((U64*)b.data)[k + 1] & mask2);

			++k;
		}

		if(!v)
			continue;

		switch(type) {
			case EBase2Type_Bin:	((C8*)result->ptr)[i - j] = C8_createBin((U8)v);	break;
			case EBase2Type_Oct:	((C8*)result->ptr)[i - j] = C8_createOct((U8)v);	break;
			case EBase2Type_Hex:	((C8*)result->ptr)[i - j] = C8_createHex((U8)v);	break;
			case EBase2Type_Nyto:	((C8*)result->ptr)[i - j] = C8_createNyto((U8)v);	break;
		}

		firstLoc = j + 1;
	}

	if(!leadingZeros)
		_gotoIfError(clean, CharString_eraseAtCount(result, 2, (len - 2) - firstLoc));

	goto success;

clean:
	CharString_free(result, alloc);
success:
	return err;
}

Error BigInt_hex(BigInt b, Allocator alloc, CharString *result, Bool leadingZeros) {
	return BigInt_base2(b, alloc, result, EBase2Type_Hex, leadingZeros);
}

Error BigInt_oct(BigInt b, Allocator alloc, CharString *result, Bool leadingZeros) {
	return BigInt_base2(b, alloc, result, EBase2Type_Oct, leadingZeros);
}

Error BigInt_bin(BigInt b, Allocator alloc, CharString *result, Bool leadingZeros) {
	return BigInt_base2(b, alloc, result, EBase2Type_Bin, leadingZeros);
}

Error BigInt_nyto(BigInt b, Allocator alloc, CharString *result, Bool leadingZeros) {
	return BigInt_base2(b, alloc, result, EBase2Type_Nyto, leadingZeros);
}

Error BigInt_dec(BigInt b, Allocator allocator, CharString *result, Bool leadingZeros);

Error BigInt_toString(
	BigInt b,
	Allocator allocator,
	CharString *result,
	EIntegerEncoding encoding,
	Bool leadingZeros
) {
	switch (encoding) {
		case EIntegerEncoding_Bin:		return BigInt_bin(b, allocator, result, leadingZeros);
		case EIntegerEncoding_Oct:		return BigInt_oct(b, allocator, result, leadingZeros);
		case EIntegerEncoding_Hex:		return BigInt_hex(b, allocator, result, leadingZeros);
		case EIntegerEncoding_Nyto:		return BigInt_nyto(b, allocator, result, leadingZeros);
		default:						return Error_invalidParameter(3, 0, "BigInt_toString()::encoding is invalid");
	}
}

//U128

U128 U128_create(const U8 data[16]) {
	return I32x4_createFromU64x2(((const U64*)data)[0], ((const U64*)data)[1]);
}

U128 U128_createU64x2(U64 a, U64 b) {
	return I32x4_createFromU64x2(a, b);
}

U128 U128_mul64(U64 au, U64 bu) {

	#if _PLATFORM_TYPE == PLATFORM_LINUX
		return (__uint128) au * (__uint128) bu;

	#elif _PLATFORM_TYPE == PLATFORM_WINDOWS

		U64 hiProd = 0;
		U64 loProd = _umul128(bu, au, &hiProd);
		return U128_createU64x2(loProd, hiProd);

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

#if _PLATFORM_TYPE == PLATFORM_LINUX

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

U8 U128_bitScan(U128 a) {

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		unsigned long index = 0;
		Bool hasFirstBit = _BitScanReverse64(&index, ((const U64*)&a)[1]);

		if (hasFirstBit)
			return (U8)index + 64;

		Bool hasLastBit = _BitScanReverse64(&index, ((const U64*)&a)[0]);
		return hasLastBit ? (U8) index : U8_MAX;

	//TODO: #elif _SIMD == SIMD_SSE: _lzcnt_u64

	#else

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

	#endif
}

Bool U128_isBase2(U128 a) {

	U8 v = U128_bitScan(a);

	if(v == U8_MAX)
		return false;

	return U128_eq(U128_lsh(U128_one(), v), a);
}

U128 U128_createFromBase2(CharString text, Error *failed, EBase2Type type) {
	
	U128 result = U128_zero();
	BigInt asBigInt = (BigInt) { 0 };
	Error err;
	
	_gotoIfError(clean, BigInt_createRef((U64*) &result, 2, &asBigInt));
	_gotoIfError(clean, BigInt_createFromBase2Type(text, U16_MAX, (Allocator) { 0 }, &asBigInt, type));

clean:
	
	if(failed && err.genericError)
		*failed = err;

	return result;
}

U128 U128_createFromHex(CharString text, Error *failed) { return U128_createFromBase2(text, failed, EBase2Type_Hex); }
U128 U128_createFromOct(CharString text, Error *failed) { return U128_createFromBase2(text, failed, EBase2Type_Oct); }
U128 U128_createFromBin(CharString text, Error *failed) { return U128_createFromBase2(text, failed, EBase2Type_Bin); }
U128 U128_createFromNyto(CharString text, Error *failed) { return U128_createFromBase2(text, failed, EBase2Type_Nyto); }

U128 U128_createFromDec(CharString text, Error *failed, Allocator alloc) {

	U128 result = U128_zero();
	BigInt asBigInt = (BigInt) { 0 };
	Error err;

	//TODO: Optimize with U128_mul when available!

	_gotoIfError(clean, BigInt_createRef((U64*) &result, 2, &asBigInt));
	_gotoIfError(clean, BigInt_createFromDec(text, U16_MAX, alloc, &asBigInt));

clean:

	if(failed && err.genericError)
		*failed = err;

	return result;
}

//U128 has special implementation for dec that's faster because mul is overloaded which is faster than BigInt

U128 U128_createFromString(CharString text, Error *failed, Allocator alloc) {

	U128 v = U128_zero();

	if (CharString_length(text) > 2) {

		U16 start = *(const U16*) text.ptr;

		switch(start) {

			case _STRING_TO_U16('0', 'b'): case _STRING_TO_U16('0', 'B'):
			case _STRING_TO_U16('0', 'x'): case _STRING_TO_U16('0', 'X'):
			case _STRING_TO_U16('0', 'o'): case _STRING_TO_U16('0', 'O'):
			case _STRING_TO_U16('0', 'n'): case _STRING_TO_U16('0', 'N'): {

				BigInt bigInt = (BigInt) { 0 };
				Error err = BigInt_createRef((U64*)&v, 2, &bigInt);

				if(err.genericError) {

					if(failed)
						*failed = err;

					return v;
				}

				err = BigInt_createFromString(text, U16_MAX, (Allocator) { 0 }, &bigInt);

				if(failed)
					*failed = err;
				
				return v;
			}
		}
	}

	return U128_createFromDec(text, failed, alloc);
}

ECompareResult U128_cmp(U128 a, U128 b) {

	//Early exit

	if(U128_eq(a, b))
		return ECompareResult_Eq;

	#if _PLATFORM_TYPE == PLATFORM_LINUX
		return a < b ? -1 : 1;
	#else

		const U64 *a64 = (const U64*)&a;
		const U64 *b64 = (const U64*)&b;

		if(a64[1] > b64[1] || (a64[1] == b64[1] && a64[0] > b64[0]))
			return ECompareResult_Gt;

		return ECompareResult_Lt;

	#endif
}

Bool U128_neq(U128 a, U128 b) { return U128_eq(a, b) != ECompareResult_Eq; }
Bool U128_lt(U128 a, U128 b) { return U128_cmp(a, b) < ECompareResult_Eq; }
Bool U128_leq(U128 a, U128 b) { return U128_cmp(a, b) <= ECompareResult_Eq; }
Bool U128_gt(U128 a, U128 b) { return U128_cmp(a, b) > ECompareResult_Eq; }
Bool U128_geq(U128 a, U128 b) { return U128_cmp(a, b) >= ECompareResult_Eq; }

U128 U128_div(U128 a, U128 b);			//if(isBase2) rsh(a, firstBitId)
U128 U128_mod(U128 a, U128 b);
U128 U128_mul(U128 a, U128 b);

U128 U128_min(U128 a, U128 b) { return U128_leq(a, b) ? a : b; }
U128 U128_max(U128 a, U128 b) { return U128_geq(a, b) ? a : b; }
U128 U128_clamp(U128 a, U128 mi, U128 ma) { return U128_max(U128_min(a, ma), mi); }
