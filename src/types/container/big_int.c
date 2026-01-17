/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/container/big_int.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/base/string_read_helper.h"
#include "types/container/string.h"
#include "types/base/string_mut.h"
#include "types/base/allocator.h"
#include "types/base/time.h"
#include "types/base/c8.h"
#include "types/base/constants.h"
#include "types/container/log.h"
#include "types/base/mathf.h"

Bool BigInt_create(U16 bitCount, const Allocator *alloc, BigInt *big, Error *e_rr) {

	Bool s_uccess = true;
	const U64 u64s = (bitCount + 63) >> 6;

	if(u64s >> 8)
		retError(clean, Error_outOfBounds(0, bitCount, (U64)U8_MAX << 6, "BigInt_create()::bitCount out of bounds"));

	Buffer buffer = Buffer_createNull();
	gotoIfError3(clean, Buffer_createEmptyBytes(u64s * sizeof(U64), alloc, &buffer, e_rr));

	*big = (BigInt) { .data = (const U64*) buffer.ptr, .length = (U8) u64s };

clean:
	return s_uccess;
}

Bool BigInt_createRef(U64 *ptr, U64 ptrCount, BigInt *big, Error *e_rr) {

	Bool s_uccess = true;

	if(!big)
		retError(clean, Error_nullPointer(2, "BigInt_createRef()::big is required"));

	if(((U64)(void*)ptr) & 7)
		retError(clean, Error_invalidParameter(2, 0, "BigInt_createRef()::ptr is misaligned, requiring proper alignment!"));

	if(big->data)
		retError(clean, Error_invalidParameter(2, 0, "BigInt_createRef()::big->data should be empty"));

	if(ptrCount >> 8)
		retError(clean, Error_outOfBounds(
			1, ptrCount, U8_MAX, "BigInt_createRef()::ptrCount is more than the BigInt limit (256 U64s)"
		));

	*big = (BigInt) { .data = ptr, .isConst = false, .isRef = true, .length = (U8) ptrCount };

clean:
	return s_uccess;
}

Bool BigInt_createRefConst(const U64 *ptr, U64 ptrCount, BigInt *big, Error *e_rr) {

	Bool s_uccess = true;

	if(!big)
		retError(clean, Error_nullPointer(2, "BigInt_createRefConst()::big is required"));

	if(((U64)(void*)ptr) & 7)
		retError(clean, Error_invalidParameter(
			2, 0, "BigInt_createRefConst()::ptr is misaligned, requiring proper alignment!"
		));

	if(big->data)
		retError(clean, Error_invalidParameter(2, 0, "BigInt_createRefConst()::big->data should be empty"));

	if(ptrCount >> 8)
		retError(clean, Error_outOfBounds(
			1, 0, U8_MAX, "BigInt_createRefConst()::ptrCount is more than the BigInt limit (255 U64s)"
		));

	*big = (BigInt) { .data = ptr, .isConst = true, .isRef = true, .length = (U8) ptrCount };

clean:
	return s_uccess;
}

Bool BigInt_createCopy(BigInt *a, const Allocator *alloc, BigInt *b, Error *e_rr) {

	Bool s_uccess = true;

	if (!a || !b)
		retError(clean, Error_nullPointer(!a ? 0 : 2, "BigInt_createCopy()::a or b is NULL"));

	if (b->data)
		retError(clean, Error_invalidParameter(2, 0, "BigInt_createCopy()::b->data should be NULL to avoid memory leaks"));

	gotoIfError3(clean, BigInt_create((U16)a->length << 6, alloc, b, e_rr));

	if (!BigInt_set(a, *b, false, NULL, e_rr))
		retError(clean, Error_invalidState(0, "BigInt_createCopy() set failed"));

clean:
	return s_uccess;
}

void BigInt_free(BigInt *a, const Allocator *allocator) {

	if (!a)
		return;

	if (!a->isRef && a->length) {
		Buffer buf = Buffer_createManagedPtr(a->dataNonConst, a->length * sizeof(U64));
		Buffer_free(&buf, allocator);
	}

	*a = (BigInt) { 0 };
}

static const C8 *base2Types[] = { "0x", "0b", "0o", "0n" };
static const U8 base2Count[] = { 4, 1, 3, 6 };

Bool BigInt_createFromBase2Type(const BigIntCreate *bigIntCreate, EIntEncoding type, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	BigInt *big = NULL;

	if(!bigIntCreate || !bigIntCreate->text || !bigIntCreate->big)
		retError(clean, Error_nullPointer(0, "BigInt_createFromBase2Type()::bigIntCreate, ->text and ->big are required"));

	if((U64)type >= EIntEncoding_Count)
		retError(clean, Error_invalidEnum(3, (U64)type, EIntEncoding_Count, "BigInt_createFromBase2Type()::type is invalid"));

	const CharString prefix = CharString_createRefCStrConst(base2Types[type]);
	const U8 prefixChars = CharString_startsWithStringInsensitive(bigIntCreate->text, &prefix, 0) ? 2 : 0;
	const U8 countPerChar = base2Count[type];

	const U64 textl = CharString_length(*bigIntCreate->text);
	const U64 chars = textl - prefixChars;
	const C8 *const textPtr = bigIntCreate->text->ptr;

	big = bigIntCreate->big;
	U16 bitCount = bigIntCreate->bitCount;

	if(!chars)
		retError(clean, Error_invalidParameter(
			0, 0, "BigInt_createFromBase2Type()::text starts with 0[xbon] but doesn't have content"
		));

	if (bitCount == U16_MAX) {

		if(!big->data)
			retError(clean, Error_nullPointer(
				3, "BigInt_createFromBase2Type()::big->data is required if bitCount is auto (U16_MAX)"
			));

		bitCount = BigInt_bitCount(*big);
	}

	else if(!bitCount)
		bitCount = (U16) U64_min(chars * countPerChar, 0xFF * 64);

	else {

		if(big->data)
			retError(clean, Error_invalidParameter(
				3, 0, "BigInt_createFromBase2Type()::big->data should be NULL if bitCount isn't auto (U16_MAX)"
			));

		if(bitCount > 0xFF * 64)
			retError(clean, Error_invalidParameter(1, 0, "BigInt_createFromBase2Type()::bitCount is out of bounds (>16320)"));
	}

	if(chars * countPerChar > (((U32)bitCount + countPerChar - 1) / countPerChar * countPerChar))
		retError(clean, Error_outOfBounds(
			0, chars * countPerChar, bitCount, "BigInt_createFromBase2Type()::text would overflow BigInt bitCount"
		));

	if (!big->data) {
		gotoIfError3(clean, BigInt_create(bitCount, bigIntCreate->alloc, big, e_rr));
		allocated = true;
	}

	for (
		U64 i = textl - 1, j = 0, k = 0;
		i >= prefixChars && i != U64_MAX;
		--i, ++j
	) {
		U8 v = 0;

		switch (type) {
			case EIntEncoding_Hex:	v = C8_hex(textPtr[i]);		break;
			case EIntEncoding_Bin:	v = C8_bin(textPtr[i]);		break;
			case EIntEncoding_Oct:	v = C8_oct(textPtr[i]);		break;
			case EIntEncoding_Nyto:	v = C8_nyto(textPtr[i]);	break;
		}

		if(v == U8_MAX)
			retError(clean, Error_invalidParameter(0, 1, "BigInt_createFromBase2Type()::text contains invalid char"));

		switch (type) {

			case EIntEncoding_Hex:	((U8*)big->data)[j >> 1] |= v << (countPerChar * (j & 1));	break;
			case EIntEncoding_Bin:	((U8*)big->data)[j >> 3] |= v << (countPerChar * (j & 7));	break;

			case EIntEncoding_Nyto:
			case EIntEncoding_Oct: {

				const U64 lo = (U64)v << ((countPerChar * j) & 63);

				big->dataNonConst[k] |= lo;

				if (((countPerChar * j) & ~63) != ((countPerChar * (j + 1)) & ~63)) {

					if(k + 1 >= big->length) {

						if (i != prefixChars) {
							retError(clean, Error_invalidParameter(
								0, 1, "BigInt_createFromBase2Type()::text contains more data than BigInt can hold"
							));
						}

						else if((U64)v >> (64 - ((countPerChar * j) & 63)))
							retError(clean, Error_invalidParameter(
								0, 1, "BigInt_createFromBase2Type()::text contains more data than BigInt can hold (overflow)"
							));

						goto clean;
					}

					const U64 hi = (U64)v >> (64 - ((countPerChar * j) & 63));
					big->dataNonConst[++k] |= hi;
				}

				break;
			}
		}
	}

	if(bitCount & 63) {		//Fix last U64 to handle out of bounds

		if (big->data[big->length - 1] >> (bitCount & 63))
			retError(clean, Error_outOfBounds(
				0, bitCount, bitCount, "BigInt_createFromBase2Type()::text contains too much data"
			));
	}

clean:

	if(!s_uccess && allocated)
		BigInt_free(big, bigIntCreate->alloc);

	return s_uccess;
}

Bool BigInt_createFromDec(const BigIntCreate *bigIntCreate, Error *e_rr) {

	Bool s_uccess = true;
	BigInt *big = NULL;
	const Allocator *alloc = NULL;
	Bool allocated = false;

	BigInt temp = { 0 }, multiplier = { 0 };

	if (!bigIntCreate || !bigIntCreate->text || !bigIntCreate->big)
		retError(clean, Error_nullPointer(0, "BigInt_createFromBase2Type()::bigIntCreate, ->text and ->big are required"));

	const U64 textl = CharString_length(*bigIntCreate->text);

	if(!textl)
		retError(clean, Error_nullPointer(0, "BigInt_createFromDec()::text is required"));

	if(textl > 4913)
		retError(clean, Error_invalidParameter(
			0, 0, "BigInt_createFromDec()::text should be below 4913 characters (16320 bits)"
		));

	big = bigIntCreate->big;
	U16 bitCount = bigIntCreate->bitCount;
	const C8 *const textPtr = bigIntCreate->text->ptr;

	U64 estBitCount = (U64) F64_ceil(F64_log2((F64) textl));

	if (bitCount == U16_MAX) {

		if(!big->data)
			retError(clean, Error_nullPointer(
				3, "BigInt_createFromDec()::big->data is required when bitCount is auto (U16_MAX)"
			));

		if(!BigInt_and(big, BigInt_createNull()))
			retError(clean, Error_invalidState(0, "BigInt_createFromDec()::big clear failed"));

		bitCount = BigInt_bitCount(*big);
	}

	else if(!bitCount)
		bitCount = (U16) U64_min(estBitCount, 0xFF * 64);

	else {

		if(big->data)
			retError(clean, Error_invalidParameter(
				3, 0, "BigInt_createFromDec()::big->data should be NULL when bitCount isn't auto (U16_MAX)"
			));

		if(bitCount > 0xFF * 64)
			retError(clean, Error_invalidParameter(1, 0, "BigInt_createFromBase2Type()::bitCount is out of bounds (>16320)"));
	}

	if(estBitCount > 0xFF * 64 + 1)			//+1 to align to base10
		retError(clean, Error_outOfMemory(0, "BigInt_createFromBase2Type() estBitCount is out of bounds (>16321)"));

	alloc = bigIntCreate->alloc;

	if (!big->data) {
		gotoIfError3(clean, BigInt_create(bitCount, alloc, big, e_rr));
		allocated = true;
	}

	gotoIfError3(clean, BigInt_create(bitCount, alloc, &multiplier, e_rr));
	gotoIfError3(clean, BigInt_create(bitCount, alloc, &temp, e_rr));

	((U64*)multiplier.data)[0] = 1;		//Multiplier

	for (U64 i = textl - 1; i != U64_MAX; --i) {

		U8 v = C8_dec(textPtr[i]);

		if (v == U8_MAX)
			retError(clean, Error_invalidParameter(0, 1, "BigInt_createFromBase2Type()::text contains non alpha char"));

		((U8*)temp.data)[0] = v;

		gotoIfError3(clean, BigInt_mul(&temp, multiplier, alloc, e_rr));

		if (!BigInt_add(big, temp))
			retError(clean, Error_invalidState(2, "BigInt_createFromBase2Type() add failed"));

		//Multiply multiplier by 10

		if (!BigInt_and(&temp, BigInt_createNull()))
			retError(clean, Error_invalidState(2, "BigInt_createFromBase2Type() clear failed"));

		((U8*)temp.data)[0] = 10;

		gotoIfError3(clean, BigInt_mul(&multiplier, temp, alloc, e_rr));
	}

	if(bitCount & 63) {		//Fix last U64 to handle out of bounds

		if (big->data[big->length - 1] >> (bitCount & 63))
			retError(clean, Error_outOfBounds(
				0, bitCount, bitCount, "BigInt_createFromBase2Type()::text contains too much data"
			));
	}

clean:

	if (alloc) {

		BigInt_free(&multiplier, alloc);
		BigInt_free(&temp, alloc);

		if (!s_uccess && allocated)
			BigInt_free(big, alloc);
	}

	return s_uccess;
}

Bool BigInt_createFromString(const BigIntCreate *bigIntCreate, Error *e_rr) {

	if (bigIntCreate && bigIntCreate->text && CharString_length(*bigIntCreate->text) > 2) {

		U16 start = Buffer_readU16(CharString_bufferConst(*bigIntCreate->text), 0, NULL, NULL);

		switch (start) {

			case C8x2('0', 'b'): case C8x2('0', 'B'):
				return BigInt_createFromBin(bigIntCreate, e_rr);

			case C8x2('0', 'x'): case C8x2('0', 'X'):
				return BigInt_createFromHex(bigIntCreate, e_rr);

			case C8x2('0', 'o'): case C8x2('0', 'O'):
				return BigInt_createFromOct(bigIntCreate, e_rr);

			case C8x2('0', 'n'): case C8x2('0', 'N'):
				return BigInt_createFromNyto(bigIntCreate, e_rr);
		}
	}

	return BigInt_createFromDec(bigIntCreate, e_rr);
}

Bool BigInt_mul(BigInt *a, BigInt b, const Allocator *allocator, Error *e_rr) {

	if(!a || a->isConst || !a->length)
		return false;

	if(!b.length)
		return BigInt_and(a, b);

	Bool s_uccess = true;
	BigInt temp = { 0 };
	gotoIfError3(clean, BigInt_create(BigInt_bitCount(*a), allocator, &temp, e_rr));

	const U32 digitsA = (U32)a->length * 2;
	const U32 digitsB = (U32)b.length * 2;

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
		//When 128-bit numbers are available (hardware accelerated) this could be extended the same way to speed it up.

		const U32 startX = (U32) U64_min(i, digitsA - 1);
		const U32 startRayT = i - startX;
		const U32 endRayT = (U32) U64_min(i, digitsB - 1) - startRayT;

		for (U32 t = startRayT; t <= endRayT; ++t) {

			const U64 x = i - t;
			const U64 y = t;

			const U64 prevMul = mul;
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

			const U64 prev = dst[i + 1];
			dst[i + 1] += (U32) (mul >> 32);

			if (dst[i + 1] < prev && i + 2 < digitsA) {		//Overflow in our overflow.

				U64 j = i + 2, v = 0;

				do {										//Keep on adding overflow
					v = ++dst[j++];
				} while(!v && j < digitsA);
			}
		}
	}

	gotoIfError3(clean, BigInt_set(a, temp, false, NULL, e_rr));

clean:
	BigInt_free(&temp, allocator);
	return s_uccess;
}

Bool BigInt_add(BigInt *a, BigInt b) {

	if(!a || a->isConst)
		return false;

	Bool overflow = false;
	const U64 len = U64_min(a->length, b.length);

	for(U64 i = 0; i < len; ++i) {

		U64 prev = a->data[i];
		U64 add = prev + b.data[i];
		Bool nextOverflow = add < prev;

		if(overflow) {
			prev = add;
			++add;
			nextOverflow |= add < prev;
		}

		a->dataNonConst[i] = add;
		overflow = nextOverflow;
	}

	const U64 next = b.length;

	while(overflow && next < a->length) {
		const U64 prev = a->data[next];
		overflow = (++a->dataNonConst[next]) < prev;
	}

	return true;
}

Bool BigInt_sub(BigInt *a, BigInt b) {

	if(!a || a->isConst)
		return false;

	Bool underflow = false;
	const U64 len = U64_min(a->length, b.length);

	for(U64 i = 0; i < len; ++i) {

		U64 prev = a->data[i];
		U64 sub = prev - b.data[i];
		Bool nextUnderflow = sub > prev;

		if(underflow) {
			prev = sub;
			--sub;
			nextUnderflow |= sub > prev;
		}

		a->dataNonConst[i] = sub;
		underflow = nextUnderflow;
	}

	const U64 next = b.length;

	while(underflow && next < a->length) {
		const U64 prev = a->data[next];
		underflow = (--a->dataNonConst[next]) > prev;
	}

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
		const U64 shift = bits & 63;

		if(!shift) {
			a->dataNonConst[i] = right;
			continue;
		}

		U64 left = i <= (bits >> 6) ? 0 : a->data[i - (bits >> 6) - 1];

		right <<= shift;
		left >>= 64 - shift;

		a->dataNonConst[i] = left | right;
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

		U64 left = i + (bits >> 6) > (U64)(a->length - 1) ? 0 : a->data[i + (bits >> 6)];
		const U64 shift = bits & 63;

		if(!shift) {
			a->dataNonConst[i] = left;
			continue;
		}

		U64 right = i + (bits >> 6) >= (U64)(a->length - 1) ? 0 : a->data[i + (bits >> 6) + 1];

		right <<= 64 - shift;
		left >>= shift;

		a->dataNonConst[i] = left | right;
	}

	return true;
}

Bool BigInt_resize(BigInt *a, U8 newSize, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!a)
		retError(clean, Error_nullPointer(0, "BigInt_resize()::a is required"));

	if(a->isRef)
		retError(clean, Error_invalidOperation(0, "BigInt_resize()::a is a ref, resize can't be called on that"));

	if (a->length == newSize)
		goto clean;

	if(!newSize) {
		BigInt_free(a, alloc);
		goto clean;
	}

	BigInt temp = { 0 };
	gotoIfError3(clean, BigInt_create((U16)newSize << 6, alloc, &temp, e_rr));

	if(!BigInt_set(&temp, *a, false, NULL, e_rr))
		retError(clean, Error_invalidState(0, "BigInt_resize() set failed"));

	BigInt_free(a, alloc);
	*a = temp;

clean:
	return s_uccess;
}

Bool BigInt_set(BigInt *a, BigInt b, Bool allowResize, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!a || a->isConst)
		retError(clean, Error_nullPointer(0, "BigInt_set()::a is required"));

	if (allowResize && a->length != b.length)
		gotoIfError3(clean, BigInt_resize(a, b.length, alloc, e_rr));

	for(U64 i = 0; i < a->length && i < b.length; ++i)
		a->dataNonConst[i] = b.data[i];

	for(U64 i = b.length; i < a->length; ++i)
		a->dataNonConst[i] = 0;

clean:
	return s_uccess;
}

//TODO: div and mod
//Bool BigInt_mod(BigInt *a, BigInt b);
//Bool BigInt_div(BigInt *a, BigInt b);

Bool BigInt_base2(const BigIntStringify *stringify, EIntEncoding type, BigInt b, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;
	const U8 countPerChar = base2Count[type];
	const U64 len = U64_max(3, (((U64)b.length * 64 + countPerChar - 1) / countPerChar) + 2);

	if (!stringify)
		retError(clean, Error_nullPointer(0, "BigInt_base2()::stringify is required"));

	if (type >= EIntEncoding_Base2End)
		retError(clean, Error_outOfBounds(0, type, EIntEncoding_Base2End, "BigInt_base2()::type is out of bounds"));

	gotoIfError3(clean, CharString_resize(stringify->result, len, '0', stringify->alloc, e_rr));
	allocated = true;

	C8 *resultPtr = stringify->result->ptrNonConst;

	resultPtr[1] = base2Types[type][1];

	U64 firstLoc = len - 1;
	const U8 mask = (1 << countPerChar) - 1;
	const U64 i = len - 1;

	for (U64 j = 0, k = 0; j < len - 2 && k < b.length; ++j) {

		U64 v = b.data[k];
		v = ((U64)v >> ((countPerChar * j) & 63)) & mask;

		if (((countPerChar * j) & ~63) != ((countPerChar * (j + 1)) & ~63)) {

			const U64 mask2 = ((U64)U64_MAX << (64 - ((countPerChar * j) & 63))) & mask;

			if(k + 1 < b.length)
				v |= b.data[k + 1] & mask2;

			++k;
		}

		if(!v)
			continue;

		switch(type) {
			case EIntEncoding_Bin:	resultPtr[i - j] = C8_createBin((U8)v);	break;
			case EIntEncoding_Oct:	resultPtr[i - j] = C8_createOct((U8)v);	break;
			case EIntEncoding_Hex:	resultPtr[i - j] = C8_createHex((U8)v);	break;
			case EIntEncoding_Nyto:	resultPtr[i - j] = C8_createNyto((U8)v);	break;
		}

		firstLoc = j + 1;
	}

	if (!stringify->leadingZeros)
		gotoIfError3(clean, CharString_eraseAtCount(stringify->result, 2, (len - 2) - firstLoc, e_rr));

clean:
	if(!s_uccess && allocated)
		CharString_free(stringify->result, stringify->alloc);

	return s_uccess;
}

Bool BigInt_toString(const BigIntStringify *stringify, EIntEncoding encoding, BigInt b, Error *e_rr) {

	Bool s_uccess = true;

	switch (encoding) {

		case EIntEncoding_Bin: case EIntEncoding_Oct: case EIntEncoding_Hex: case EIntEncoding_Nyto:
			gotoIfError3(clean, BigInt_base2(stringify, encoding, b, e_rr));
			break;

		default:
			retError(clean, Error_invalidParameter(3, 0, "BigInt_toString()::encoding is invalid"));
	}

clean:
	return s_uccess;
}

//Bool BigInt_dec(const BigIntStringify *stringify, BigInt b, Error *e_rr);
