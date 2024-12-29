/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "types/base/platform_types.h"

//Null is apparently non-standard

#ifndef NULL
	#define NULL ((void*)0)
#endif

//A hint to show that something is implementation dependent
//For ease of implementing a new implementation
//These should never be directly called by anyone else than the main library the impl is for.

#define impl

#ifdef __cplusplus
	extern "C" {
#endif

//Types

typedef int8_t  I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef float F32;
typedef double F64;

typedef U64 Ns;		//Time since Unix epoch in Ns
typedef I64 DNs;	//Delta Ns

typedef char C8;

typedef bool Bool;

//Stack strings that are faster and easier to allocate

#define SHORTSTRING_LEN 32
#define LONGSTRING_LEN 64

typedef C8 ShortString[SHORTSTRING_LEN];
typedef C8 LongString[LONGSTRING_LEN];

//Constants

extern const U64 KIBI;
extern const U64 MIBI;
extern const U64 GIBI;
extern const U64 TIBI;
extern const U64 PEBI;

extern const U64 KILO;
extern const U64 MEGA;
extern const U64 GIGA;
extern const U64 TERA;
extern const U64 PETA;

extern const Ns MU;
extern const Ns MS;
extern const Ns SECOND;
extern const Ns MIN;
extern const Ns HOUR;
extern const Ns DAY;
extern const Ns WEEK;

extern const U8 U8_MIN;
extern const C8 C8_MIN;
extern const U16 U16_MIN;
extern const U32 U24_MIN;
extern const U32 U32_MIN;
extern const U64 U64_MIN;

extern const I8  I8_MIN;
extern const I16 I16_MIN;
extern const U32 I24_MIN;
extern const I32 I32_MIN;
extern const I64 I64_MIN;

extern const U8  U8_MAX;
extern const C8  C8_MAX;
extern const U16 U16_MAX;
extern const U32 U24_MAX;
extern const U32 U32_MAX;
extern const U64 U64_MAX;

extern const I8  I8_MAX;
extern const I16 I16_MAX;
extern const U32 I24_MAX;
extern const I32 I32_MAX;
extern const I64 I64_MAX;

extern const F32 F32_MIN;
extern const F32 F32_MAX;

extern const F64 F64_MIN;
extern const F64 F64_MAX;

typedef enum ECompareResult {
	ECompareResult_Lt,
	ECompareResult_Eq,
	ECompareResult_Gt
} ECompareResult;

typedef ECompareResult (*CompareFunction)(const void *aPtr, const void *bPtr);
typedef Bool (*EqualsFunction)(const void *aPtr, const void *bPtr);		//Passing NULL as func indicates raw buffer compare
typedef U64 (*HashFunction)(const void *aPtr);							//Passing NULL as func indicates raw buffer hash

typedef struct Error Error;

//Fixed point

#define FixedPoint(frac, integ)																\
																							\
typedef U64 FP##integ##f##frac;																\
																							\
FP##integ##f##frac FP##integ##f##frac##_add(FP##integ##f##frac a, FP##integ##f##frac b);	\
FP##integ##f##frac FP##integ##f##frac##_sub(FP##integ##f##frac a, FP##integ##f##frac b);	\
																							\
FP##integ##f##frac FP##integ##f##frac##_fromDouble(F64 v);									\
F64 FP##integ##f##frac##_toDouble(FP##integ##f##frac value);

//Fixed point 42 (FP37f4): 4 fract, 37 integer, 1 sign.
//+-1.4M km precision 1/16th cm
//3x F42 < 128 bit (2 bit remainder)
//Can pack 3 in uint4.

FixedPoint(4, 37)

//Fixed point for bigger scale, 53 (FP46f6): 6 fract, 46 integer, 1 sign.
//~700M km (about 4.5au) with precision 1/64th cm.
//Can pack 3 in uint4 + uint.

FixedPoint(6, 46)

//Buffer (more functions in types/buffer.h)

typedef struct Buffer {

	union {
		const U8 *ptr;
		U8 *ptrNonConst;		//Requires !Buffer_isConstRef(buf)
	};
	
	U64 lengthAndRefBits;		//refBits: [ b31 isRef, b30 isConst ]. Length should be max 48 bits

} Buffer;

U64 Buffer_length(Buffer buf);

Bool Buffer_isRef(Buffer buf);
Bool Buffer_isConstRef(Buffer buf);

Buffer Buffer_createManagedPtr(void *ptr, U64 length);

//These should never be Buffer_free-d because Buffer doesn't know if it's allocated

Buffer Buffer_createNull();

Buffer Buffer_createRef(void *v, U64 length);
Buffer Buffer_createRefConst(const void *v, U64 length);

//Bit manipulation

Bool Buffer_copy(Buffer dst, Buffer src);
Bool Buffer_revCopy(Buffer dst, Buffer src);		//Copies bytes from range backwards; useful if ranges overlap

Error Buffer_setAllBits(Buffer dst);
Error Buffer_unsetAllBits(Buffer dst);

Error Buffer_setAllBitsTo(Buffer buf, Bool isOn);

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output);

Error Buffer_setBit(Buffer buf, U64 offset);
Error Buffer_resetBit(Buffer buf, U64 offset);

Error Buffer_setBitTo(Buffer buf, U64 offset, Bool value);

Error Buffer_bitwiseOr(Buffer dst, Buffer src);
Error Buffer_bitwiseAnd(Buffer dst, Buffer src);
Error Buffer_bitwiseXor(Buffer dst, Buffer src);
Error Buffer_bitwiseNot(Buffer dst);

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits);
Error Buffer_unsetBitRange(Buffer dst, U64 dstOff, U64 bits);

//Comparison

Bool Buffer_eq(Buffer buf0, Buffer buf1);			//Also compares size
Bool Buffer_neq(Buffer buf0, Buffer buf1);			//Also compares size

#define BUFFER_OP(T)									\
Error Buffer_append##T(Buffer *buf, T v);				\
Error Buffer_consume##T(Buffer *buf, T *v);				\
T Buffer_read##T(Buffer buf, U64 off,  Bool *success);	\
Bool Buffer_write##T(Buffer buf, U64 off, T v)

#define BUFFER_OP_IMPL(T)																	\
Error Buffer_append##T(Buffer *buf, T v)	{ return Buffer_append(buf, &v, sizeof(v)); }	\
Error Buffer_consume##T(Buffer *buf, T *v)	{ return Buffer_consume(buf, v, sizeof(*v)); }	\
																							\
T Buffer_read##T(Buffer buf, U64 off, Bool *success) {										\
																							\
	T v = (T) { 0 };																		\
	Error err = Buffer_offset(&buf, off);													\
																							\
	if(err.genericError) {																	\
		if(success) *success = false;														\
		return v;																			\
	}																						\
																							\
	Bool ok = !Buffer_consume##T(&buf, &v).genericError;									\
	if(success) *success = ok;																\
	return v;																				\
}																							\
																							\
Bool Buffer_write##T(Buffer buf, U64 off, T v) {											\
	Error err = Buffer_offset(&buf, off);													\
	if(err.genericError) return false;														\
	return !Buffer_append##T(&buf, v).genericError;											\
}

BUFFER_OP(U64);
BUFFER_OP(U32);
BUFFER_OP(U16);
BUFFER_OP(U8);

BUFFER_OP(I64);
BUFFER_OP(I32);
BUFFER_OP(I16);
BUFFER_OP(I8);

BUFFER_OP(F64);
BUFFER_OP(F32);

Error Buffer_offset(Buffer *buf, U64 length);

Error Buffer_append(Buffer *buf, const void *v, U64 length);
Error Buffer_appendBuffer(Buffer *buf, Buffer append);

Error Buffer_consume(Buffer *buf, void *v, U64 length);

//Use this instead of simply copying the Buffer to a new location
//A copy like this is only fine if the other doesn't get freed.
//In all other cases, createRefFromBuffer should be called on the one that shouldn't be freeing.
//If it needs to be refcounted, RefPtr should be used.
Buffer Buffer_createRefFromBuffer(Buffer buf, Bool isConst);

//Char
//All char helper functions assume ASCII, otherwise use string functions

C8 C8_toLower(C8 c);
C8 C8_toUpper(C8 c);

typedef enum EStringCase {
	EStringCase_Sensitive,			//Prefer when possible; avoids transforming the character
	EStringCase_Insensitive
} EStringCase;

typedef enum EStringTransform {
	EStringTransform_None,
	EStringTransform_Lower,
	EStringTransform_Upper
} EStringTransform;

C8 C8_transform(C8 c, EStringTransform transform);

Bool C8_isBin(C8 c);
Bool C8_isOct(C8 c);
Bool C8_isDec(C8 c);

Bool C8_isUpperCase(C8 c);
Bool C8_isLowerCase(C8 c);
Bool C8_isUpperCaseHex(C8 c);
Bool C8_isLowerCaseHex(C8 c);
Bool C8_isWhitespace(C8 c);
Bool C8_isNewLine(C8 c);

Bool C8_isHex(C8 c);
Bool C8_isNyto(C8 c);
Bool C8_isAlphaNumeric(C8 c);
Bool C8_isAlpha(C8 c);

Bool C8_isSymbol(C8 c);			//~"#%&'()*+,-./$:;<=>?@[\]^`_{|}~
Bool C8_isLexerSymbol(C8 c);	//isSymbol but excluding $_

Bool C8_isValidAscii(C8 c);
Bool C8_isValidFileName(C8 c);

U8 C8_bin(C8 c);
U8 C8_oct(C8 c);
U8 C8_dec(C8 c);

U8 C8_hex(C8 c);
U8 C8_nyto(C8 c);

C8 C8_createBin(U8 v);
C8 C8_createOct(U8 v);
C8 C8_createDec(U8 v);
C8 C8_createHex(U8 v);

//Nytodecimal: 0-9A-Za-z_$
C8 C8_createNyto(U8 v);

//Transforming a string to a U16, U32 or U64

#define C8x2(x, y) ((((U16)y) << 8) | x)
#define C8x4(x, y, z, w) ((((U32)C8x2(z, w)) << 16) | C8x2(x, y))
#define C8x8(x, y, z, w, a, b, c, d) ((((U64)C8x4(a, b, c, d)) << 32) | C8x4(x, y, z, w))

//Default stacktrace

#define STACKTRACE_SIZE 32
typedef void *StackTrace[STACKTRACE_SIZE];

typedef struct CharString CharString;

//Version

#define OXC3_MAKE_VERSION(major, minor, patch) (((major) << 22) | ((minor) << 12) | (patch))

#define OXC3_GET_MAJOR(v) (((U32)(v)) >> 22)
#define OXC3_GET_MINOR(v) (((U32)(v)) << 10 >> 22)
#define OXC3_GET_PATCH(v) (((U32)(v)) << 20 >> 20)

#define OXC3_MAJOR 0
#define OXC3_MINOR 2
#define OXC3_PATCH 83
#define OXC3_VERSION OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)

#ifdef __cplusplus
	}
#endif
