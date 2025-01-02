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

//Default stacktrace

#define STACKTRACE_SIZE 32
typedef void *StackTrace[STACKTRACE_SIZE];

typedef struct CharString CharString;
typedef struct Error Error;

typedef enum EStringCase {
	EStringCase_Sensitive,			//Prefer when possible; avoids transforming the character
	EStringCase_Insensitive
} EStringCase;

typedef enum EStringTransform {
	EStringTransform_None,
	EStringTransform_Lower,
	EStringTransform_Upper
} EStringTransform;

typedef struct Buffer {

	union {
		const U8 *ptr;
		U8 *ptrNonConst;		//Requires !Buffer_isConstRef(buf)
	};

	U64 lengthAndRefBits;		//refBits: [ b31 isRef, b30 isConst ]. Length should be max 48 bits

} Buffer;

//Version

#define OXC3_MAKE_VERSION(major, minor, patch) (((major) << 22) | ((minor) << 12) | (patch))

#define OXC3_GET_MAJOR(v) (((U32)(v)) >> 22)
#define OXC3_GET_MINOR(v) (((U32)(v)) << 10 >> 22)
#define OXC3_GET_PATCH(v) (((U32)(v)) << 20 >> 20)

#define OXC3_MAJOR 0
#define OXC3_MINOR 2
#define OXC3_PATCH 85
#define OXC3_VERSION OXC3_MAKE_VERSION(OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH)

#ifdef __cplusplus
	}
#endif
