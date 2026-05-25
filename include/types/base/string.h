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

//types/base/string.h

#pragma once
#include "types/base/buffer.h"
#include "types/base/constants.h"
#include "types/base/c8.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ECompareResult ECompareResult;

//Since a CharString can be a ref to existing memory, it doesn't necessarily have a null terminator.
//The null terminator is omitted for speed and to allow references into an existing string or data.
//The null terminator is automatically added on copy.
//Be careful that when passing a string to an API that expects null termination that you copy it first.
//
//There are four/five types of strings:
//
//Stack strings (or heap if you manually allocate it there):
//	ShortString; 31 chars max (includes null terminator)
//	LongString; 63 char max (includes null terminator)
//
//CharString; A string that goes on the heap (or wherever the allocator tells it to go)
//CharString(Ref); A reference to an already allocated string (CharString with capacity 0)
//CharString(Ref) const; A const reference that should not allow operations on it (CharString with capacity -1)

//Dynamic string

typedef struct CharString {

	union {
		const C8 *ptr;				//This is non const if not a const ref, but for safety this is const (cast away if not).
		C8 *ptrNonConst;			//Only use if !isConstRef
	};

	U64 lenAndNullTerminated;		//First bit contains if it's null terminated or not. Length excludes null terminator.
	U64 capacityAndRefInfo;			//capacityAndRefInfo = 0: ref, capacityAndRefInfo = -1: const ref

} CharString;

//Stack strings that are faster and easier to allocate

#define SHORTSTRING_LEN 32
#define LONGSTRING_LEN 64

typedef C8 ShortString[SHORTSTRING_LEN];
typedef C8 LongString[LONGSTRING_LEN];

CharString CharString_createRefAutoConst(const C8 *ptr, U64 maxSize);		//Auto detect end (up to maxSize chars)
CharString CharString_createRefSizedConst(const C8 *ptr, U64 size, Bool isNullTerminated);

//Simple helper functions

static inline U64  CharString_bytes(const CharString str) { return str.lenAndNullTerminated << 1 >> 1; }
static inline Bool CharString_isConstRef(const CharString str) { return str.capacityAndRefInfo == U64_MAX; }
static inline Bool CharString_isRef(const CharString str) { return !str.capacityAndRefInfo || CharString_isConstRef(str); }
static inline Bool CharString_isEmpty(const CharString str) { return !CharString_bytes(str); }
static inline Bool CharString_isNullTerminated(const CharString str) { return str.lenAndNullTerminated >> 63; }
static inline U64  CharString_length(const CharString str) { return CharString_bytes(str); }

//Returns 0 if ref
static inline U64  CharString_capacity(const CharString str) { return CharString_isRef(str) ? 0 : str.capacityAndRefInfo; }

static inline Buffer CharString_buffer(const CharString str) {
	return CharString_isConstRef(str) ? Buffer_createNull() : Buffer_createRef(str.ptrNonConst, CharString_bytes(str));
}

static inline Buffer CharString_bufferConst(const CharString str) {
	return Buffer_createRefConst(str.ptr, CharString_bytes(str));
}

static inline Buffer CharString_allocatedBuffer(const CharString str) {
	return CharString_isRef(str) ? Buffer_createNull() : Buffer_createRef(str.ptrNonConst, CharString_capacity(str));
}

static inline Buffer CharString_allocatedBufferConst(const CharString str) {
	return CharString_isRef(str) ? Buffer_createNull() : Buffer_createRefConst(str.ptr, CharString_capacity(str));
}

//Iteration

static inline C8 *CharString_begin(const CharString str) { return CharString_isConstRef(str) ? NULL : str.ptrNonConst; }
static inline const C8 *CharString_beginConst(const CharString str) { return str.ptr; }

static inline C8 *CharString_end(const CharString str) {
	return CharString_isConstRef(str) ? NULL : str.ptrNonConst + CharString_length(str);
}

static inline const C8 *CharString_endConst(const CharString str) { return str.ptr + CharString_length(str); }

static inline C8 *CharString_charAt(const CharString str, U64 off) {
	return CharString_isConstRef(str) || off >= CharString_length(str) ? NULL : str.ptrNonConst + off;
}

static inline const C8 *CharString_charAtConst(const CharString str, U64 off) {
	return off >= CharString_length(str) ? NULL : str.ptr + off;
}

static inline Bool CharString_isValidAscii(const CharString str) {

	for (U64 i = 0; i < CharString_length(str); ++i)
		if (!C8_isValidAscii(str.ptr[i]))
			return false;

	return true;
}

//Only checks characters. Please use resolvePath to actually validate if it's safely accessible.

static inline U64 CharString_calcStrLen(const C8 *ptr, U64 maxSize) {
	if (!ptr) return 0;
	U64 i = 0;
	for (; i < maxSize && ptr[i]; ++i) (void) 0;
	return i;
}

static inline C8 CharString_getAt(const CharString str, U64 i) {
	return i < CharString_length(str) ? str.ptr[i] : C8_MAX;
}

static inline Bool CharString_setAt(const CharString str, U64 i, C8 c) {

	if (i >= CharString_length(str) || CharString_isConstRef(str) || !c)
		return false;

	str.ptrNonConst[i] = c;
	return true;
}

//Freeing refs won't do anything, but is still recommended for consistency.
//Const ref disallow modifying functions to be used.

static inline CharString CharString_createNull() {
	CharString str = { .ptr = NULL, .lenAndNullTerminated = 0, .capacityAndRefInfo = 0 };
	return str;
}

//Only use this if string is known to be safe (with null terminator '\0')
static inline CharString CharString_createRefCStrConst(const C8 *ptr) {
	return CharString_createRefAutoConst(ptr, U64_MAX);
}

//Auto detect end (up to maxSize chars)
static inline CharString CharString_createRefAuto(C8 *ptr, U64 maxSize) {
	CharString str = CharString_createRefAutoConst(ptr, maxSize);
	str.capacityAndRefInfo = 0;		//Flag as mutable
	return str;
}

//isNullTerminated is true if the size given excludes the null terminator (e.g. ptr[size] == '\0').
//In this case ptr[size] has to be the null terminator.
//If this is false, it will automatically check if ptr contains a null terminator

static inline CharString CharString_createRefSized(C8 *ptr, U64 size, Bool isNullTerminated) {
	CharString str = CharString_createRefSizedConst(ptr, size, isNullTerminated);
	str.capacityAndRefInfo = 0;		//Flag as mutable
	return str;
}

static inline CharString CharString_createRefStrConst(const CharString str) {
	return CharString_createRefSizedConst(str.ptr, CharString_length(str), CharString_isNullTerminated(str));
}

static inline CharString CharString_createRefShortStringConst(const ShortString str) {
	return CharString_createRefAutoConst(str, SHORTSTRING_LEN);
}

static inline CharString CharString_createRefLongStringConst(const LongString str) {
	return CharString_createRefAutoConst(str, LONGSTRING_LEN);
}

static inline CharString CharString_createRefShortString(ShortString str) {
	return CharString_createRefAuto(str, SHORTSTRING_LEN);
}

static inline CharString CharString_createRefLongString(LongString str) {
	return CharString_createRefAuto(str, LONGSTRING_LEN);
}

//Clear owned memory of CharString
static inline Bool CharString_clear(CharString *str) {

	if(!str || CharString_isRef(*str))
		return false;

	if(CharString_isNullTerminated(*str))					//If null terminated, we want to keep it null terminated
		str->ptrNonConst[0] = '\0';

	str->lenAndNullTerminated &= ~(((U64)1 << 63) - 1);		//Clear size
	return true;
}

static inline CharString CharString_newLine() {			//Always \n since all OSes can handle that nowadays
	return CharString_createRefCStrConst("\n");
}

#ifdef __cplusplus
	}
#endif
