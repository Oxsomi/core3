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
#include <stdint.h>
#include <stdbool.h>

//Null is apparently non standard

#ifndef NULL
	#define NULL (void*)0
#endif

//Platform and arch stuff

#define SIMD_NONE 0
#define SIMD_SSE 1
#define SIMD_NEON 2

#define ARCH_NONE 0
#define ARCH_X64 1
#define ARCH_ARM 2

//A hint to show that something is implementation dependent
//For ease of implementing a new implementation
//These should never be directly called by anyone else than the main library the impl is for.

#define impl

//A hint to show that the end user should define these

#define user_impl

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

#if !_RELAX_FLOAT
	#if FLT_EVAL_METHOD != 0
		#error Flt eval method should be 0 to indicate consistent behavior
	#endif
#endif

typedef U64 Ns;		/// Time since Unix epoch in Ns
typedef I64 DNs;	/// Delta Ns

typedef char C8;

typedef bool Bool;

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
extern const U32 U32_MIN;
extern const U64 U64_MIN;

extern const I8  I8_MIN;
extern const I16 I16_MIN;
extern const I32 I32_MIN;
extern const I64 I64_MIN;

extern const U8  U8_MAX;
extern const C8  C8_MAX;
extern const U16 U16_MAX;
extern const U32 U32_MAX;
extern const U64 U64_MAX;

extern const I8  I8_MAX;
extern const I16 I16_MAX;
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
typedef Bool (*EqualsFunction)(const void *aPtr, const void *bPtr);		//Passing NULL generally indicates raw buffer compare
typedef U64 (*HashFunction)(const void *aPtr);							//Passing NULL generally indicates raw buffer hash

//Buffer (more functions in types/buffer.h)

typedef struct Buffer {
	const U8 *ptr;
	U64 lengthAndRefBits;		//refBits: [ b31 isRef, b30 isConst ]. Length should be max 48 bits
} Buffer;

U64 Buffer_length(Buffer buf);

Bool Buffer_isRef(Buffer buf);
Bool Buffer_isConstRef(Buffer buf);

Buffer Buffer_createManagedPtr(void *ptr, U64 length);

//Use this instead of simply copying the Buffer to a new location
//A copy like this is only fine if the other doesn't get freed.
//In all other cases, createRefFromBuffer should be called on the one that shouldn't be freeing.
//If it needs to be refcounted, RefPtr should be used.
//
Buffer Buffer_createRefFromBuffer(Buffer buf, Bool isConst);

//Char

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
