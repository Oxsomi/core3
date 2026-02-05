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
#include "types/base/string.h"
#include "types/base/algorithm.h"

#ifdef __cplusplus
	extern "C" {
#endif

Bool CharString_isValidFileName(const CharString str);
Bool CharString_isValidFilePath(CharString str);

ECompareResult CharString_compare(const CharString *a, const CharString *b, EStringCase caseSensitive);

static inline ECompareResult CharString_compareSensitive(const CharString *a, const CharString *b) {
	return CharString_compare(a, b, EStringCase_Sensitive);
}

static inline ECompareResult CharString_compareInsensitive(const CharString *a, const CharString *b) {
	return CharString_compare(a, b, EStringCase_Insensitive);
}

static inline Bool CharString_startsWith(const CharString str, C8 c, EStringCase caseSensitive, U64 off) {
	return
		CharString_length(str) > off && str.ptr &&
		C8_transform(str.ptr[off], (EStringTransform)caseSensitive) ==
		C8_transform(c, (EStringTransform) caseSensitive);
}

static inline Bool CharString_endsWith(const CharString str, C8 c, EStringCase caseSensitive, U64 off) {
	return
		CharString_length(str) > off && str.ptr &&
		C8_transform(str.ptr[CharString_length(str) - 1 - off], (EStringTransform) caseSensitive) ==
		C8_transform(c, (EStringTransform) caseSensitive);
}

typedef struct CharStringSensOff {
	const CharString *str;
	EStringCase caseSensitive;
	U64 off;
} CharStringSensOff;

Bool CharString_startsWithString(const CharStringSensOff *strSensOff, const CharString *other);
Bool CharString_endsWithString(const CharStringSensOff *strSensOff, const CharString *other);

U64 CharString_countAll(const CharStringSensOff *strSensOff, C8 c);
U64 CharString_countAllString(const CharStringSensOff *strSensOff, const CharString *other);

typedef struct CharStringSensOffLen {
	const CharString *str;
	EStringCase caseSensitive;
	U64 off;
	U64 len;
} CharStringSensOffLen;

U64 CharString_findFirst(const CharStringSensOffLen *strSensOffLen, C8 c);
U64 CharString_findLast(const CharStringSensOffLen *strSensOffLen, C8 c);

U64 CharString_findFirstString(const CharStringSensOffLen *strSensOffLen, const CharString *other);
U64 CharString_findLastString(const CharStringSensOffLen *strSensOffLen, const CharString *other);

static inline Bool CharString_contains(const CharStringSensOffLen *strSensOffLen, C8 c) {
	return CharString_findFirst(strSensOffLen, c) != U64_MAX;
}

static inline Bool CharString_containsString(const CharStringSensOffLen *strSensOffLen, const CharString *other) {
	return CharString_findFirstString(strSensOffLen, other) != U64_MAX;
}

Bool CharString_equalsString(const CharString *s, const CharString *other, EStringCase caseSensitive);
Bool CharString_equals(const CharString s, C8 c, EStringCase caseSensitive);

Bool CharString_parseNyto(CharString s, U64 *result);
Bool CharString_parseHex(CharString s, U64 *result);
Bool CharString_parseDec(const CharString s, U64 *result);
Bool CharString_parseDecSigned(CharString s, I64 *result);
Bool CharString_parseDouble(CharString s, F64 *result);
Bool CharString_parseFloat(const CharString s, F32 *result);
Bool CharString_parseOct(CharString s, U64 *result);
Bool CharString_parseBin(CharString s, U64 *result);
Bool CharString_parseU64(const CharString s, U64 *result);

#define CharString_matchesPattern(testFunc, ...)								\
																				\
	U64 strl = CharString_length(s);											\
	__VA_ARGS__;																\
																				\
	for(U64 i = start; i < strl; ++i)											\
		if(!testFunc(s.ptr[i]))													\
			return false;														\
																				\
	return strl > start;

#define CharString_matchesPatternNum(testFunc, num)								\
																				\
	CharString other = CharString_createRefCStrConst(num);						\
	CharString_matchesPattern(													\
		testFunc,																\
		CharStringSensOff strSensOff = { &s, EStringCase_Insensitive, 0 };		\
		U64 start = CharString_startsWithString(								\
			&strSensOff, &other													\
		) ? CharString_length(other) : 0										\
	)

//[0-9A-Za-z_$]+
static inline Bool CharString_isNyto(const CharString s) {
	CharString_matchesPatternNum(C8_isNyto, "0n");
}

//[0-9A-Za-z]+
static inline Bool CharString_isAlphaNumeric(const CharString s) {
	CharString_matchesPattern(C8_isAlphaNumeric, U64 start = 0);
}

//[0-9A-Fa-f]+
static inline Bool CharString_isHex(const CharString s) {
	CharString_matchesPatternNum(C8_isHex, "0x");
}

//[0-7]+
static inline Bool CharString_isOct(const CharString s) {
	CharString_matchesPatternNum(C8_isOct, "0o");
}

//[0-1]+
static inline Bool CharString_isBin(const CharString s) {
	CharString_matchesPatternNum(C8_isBin, "0b");
}

//[0-9]+
static inline Bool CharString_isDec(const CharString s) {
	CharString_matchesPattern(C8_isDec, U64 start = 0);
}

//[-+]?[0-9]+
static inline Bool CharString_isSignedNumber(const CharString s) {
	CharString_matchesPattern(
		C8_isDec,
		U64 start = CharString_startsWith(s, '-', EStringCase_Sensitive, 0) ||
		CharString_startsWith(s, '+', EStringCase_Sensitive, 0)
	);
}

//[+]?[0-9]+
static inline Bool CharString_isUnsignedNumber(const CharString s) {
	CharString_matchesPattern(
		C8_isDec,
		U64 start = CharString_startsWith(s, '+', EStringCase_Sensitive, 0)
	);
}

#undef CharString_matchesPattern

Bool CharString_isFloat(const CharString s);			//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

//Cut will return a reference into the current string to avoid copies

Bool CharString_cut(const CharString *s, U64 off, U64 length, CharString *result);

typedef struct CharStringCut {
	const CharString *s;
	EStringCase caseSensitive;
	CharString *result;
} CharStringCut;

Bool CharString_cutAfter(const CharStringCut *cut, C8 c, Bool isFirst);
Bool CharString_cutAfterString(const CharStringCut *cut, const CharString *other, Bool isFirst);
Bool CharString_cutBefore(const CharStringCut *cut, C8 c, Bool isFirst);
Bool CharString_cutBeforeString(const CharStringCut *s, const CharString *other, Bool isFirst);

static inline Bool CharString_cutEnd(const CharString *s, U64 length, CharString *result) {
	return CharString_cut(s, 0, length, result);
}

static inline Bool CharString_cutBegin(const CharString *s, U64 off, CharString *result) {

	if (!s || off > CharString_length(*s))
		return false;

	return CharString_cut(s, off, 0, result);
}

#ifdef __cplusplus
	}
#endif
