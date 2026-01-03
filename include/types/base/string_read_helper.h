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
#include "types/base/string_read.h"

#ifdef __cplusplus
	extern "C" {
#endif
		
//These cut functions will produce a reference into the first string to avoid copies.
		
static inline Bool CharString_startsWithSensitive(const CharString str, C8 c, U64 off) {
	return CharString_startsWith(str, c, EStringCase_Sensitive, off);
}

static inline Bool CharString_startsWithStringSensitive(const CharString *str, const CharString *other, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Sensitive, off };
	return CharString_startsWithString(&strSensOff, other);
}

static inline Bool CharString_endsWithSensitive(const CharString str, C8 c, U64 off) {
	return CharString_endsWith(str, c, EStringCase_Sensitive, off);
}

static inline Bool CharString_endsWithStringSensitive(const CharString *str, const CharString *other, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Sensitive, off };
	return CharString_endsWithString(&strSensOff, other);
}

static inline Bool CharString_countAllSensitive(const CharString *str, C8 c, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Sensitive, off };
	return CharString_countAll(&strSensOff, c);
}

static inline Bool CharString_countAllStringSensitive(const CharString *str, const CharString *other, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Sensitive, off };
	return CharString_countAllString(&strSensOff, other);
}

static inline Bool CharString_startsWithInsensitive(const CharString str, C8 c, U64 off) {
	return CharString_startsWith(str, c, EStringCase_Insensitive, off);
}

static inline Bool CharString_startsWithStringInsensitive(const CharString *str, const CharString *other, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Insensitive, off };
	return CharString_startsWithString(&strSensOff, other);
}

static inline Bool CharString_endsWithInsensitive(const CharString str, C8 c, U64 off) {
	return CharString_endsWith(str, c, EStringCase_Insensitive, off);
}

static inline Bool CharString_endsWithStringInsensitive(const CharString *str, const CharString *other, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Insensitive, off };
	return CharString_endsWithString(&strSensOff, other);
}

static inline Bool CharString_countAllInsensitive(const CharString *str, C8 c, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Insensitive, off };
	return CharString_countAll(&strSensOff, c);
}

static inline Bool CharString_countAllStringInsensitive(const CharString *str, const CharString *other, U64 off) {
	CharStringSensOff strSensOff = { str, EStringCase_Insensitive, off };
	return CharString_countAllString(&strSensOff, other);
}

static inline U64 CharString_findFirstSensitive(const CharString *s, C8 c, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Sensitive, off, len };
	return CharString_findFirst(&strSensOffLen, c);
}

static inline U64 CharString_findFirstInsensitive(const CharString *s, C8 c, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Insensitive, off, len };
	return CharString_findFirst(&strSensOffLen, c);
}

static inline U64 CharString_findLastSensitive(const CharString *s, C8 c, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Sensitive, off, len };
	return CharString_findLast(&strSensOffLen, c);
}

static inline U64 CharString_findLastInsensitive(const CharString *s, C8 c, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Insensitive, off, len };
	return CharString_findLast(&strSensOffLen, c);
}

static inline U64 CharString_findFirstStringSensitive(const CharString *s, const CharString *other, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Sensitive, off, len };
	return CharString_findFirstString(&strSensOffLen, other);
}

static inline U64 CharString_findFirstStringInsensitive(const CharString *s, const CharString *other, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Insensitive, off, len };
	return CharString_findFirstString(&strSensOffLen, other);
}

static inline U64 CharString_findLastStringSensitive(const CharString *s, const CharString *other, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Sensitive, off, len };
	return CharString_findLastString(&strSensOffLen, other);
}

static inline U64 CharString_findLastStringInsensitive(const CharString *s, const CharString *other, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Insensitive, off, len };
	return CharString_findLastString(&strSensOffLen, other);
}

static inline Bool CharString_containsSensitive(const CharString *s, C8 c, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Sensitive, off, len };
	return CharString_contains(&strSensOffLen, c);
}

static inline Bool CharString_containsInsensitive(const CharString *s, C8 c, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Insensitive, off, len };
	return CharString_contains(&strSensOffLen, c);
}

static inline Bool CharString_containsStringSensitive(const CharString *s, const CharString *other, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Sensitive, off, len };
	return CharString_containsString(&strSensOffLen, other);
}

static inline Bool CharString_containsStringInsensitive(const CharString *s, const CharString *other, U64 off, U64 len) {
	CharStringSensOffLen strSensOffLen = { s, EStringCase_Insensitive, off, len };
	return CharString_containsString(&strSensOffLen, other);
}

static inline Bool CharString_equalsStringSensitive(const CharString *s, const CharString *other) {
	return CharString_equalsString(s, other, EStringCase_Sensitive);
}

static inline Bool CharString_equalsSensitive(const CharString s, C8 c) {
	return CharString_equals(s, c, EStringCase_Sensitive);
}

static inline Bool CharString_equalsStringInsensitive(const CharString *s, const CharString *other) {
	return CharString_equalsString(s, other, EStringCase_Insensitive);
}

static inline Bool CharString_equalsInsensitive(const CharString s, C8 c) {
	return CharString_equals(s, c, EStringCase_Insensitive);
}

static inline Bool CharString_cutAfterLast(const CharStringCut *cut, C8 c) {
	return CharString_cutAfter(cut, c, false);
}

static inline Bool CharString_cutAfterFirst(const CharStringCut *cut, C8 c) {
	return CharString_cutAfter(cut, c, true);
}

static inline Bool CharString_cutAfterLastString(const CharStringCut *cut, const CharString *other) {
	return CharString_cutAfterString(cut, other, false);
}

static inline Bool CharString_cutAfterFirstString(const CharStringCut *cut, const CharString *other) {
	return CharString_cutAfterString(cut, other, true);
}

static inline Bool CharString_cutAfterSensitive(const CharString *s, C8 c, Bool isFirst, CharString *result) {
	const CharStringCut cut = { s, EStringCase_Sensitive, result };
	return CharString_cutAfter(&cut, c, isFirst);
}

static inline Bool CharString_cutAfterInsensitive(const CharString *s, C8 c, Bool isFirst, CharString *result) {
	const CharStringCut cut = { s, EStringCase_Insensitive, result };
	return CharString_cutAfter(&cut, c, isFirst);
}

static inline Bool CharString_cutAfterLastSensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutAfterSensitive(s, c, false, result);
}

static inline Bool CharString_cutAfterLastInsensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutAfterInsensitive(s, c, false, result);
}

static inline Bool CharString_cutAfterFirstSensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutAfterSensitive(s, c, true, result);
}

static inline Bool CharString_cutAfterFirstInsensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutAfterInsensitive(s, c, true, result);
}

static inline Bool CharString_cutAfterStringSensitive(
	const CharString *s, const CharString *other, Bool isFirst, CharString *result
) {
	const CharStringCut cut = { s, EStringCase_Sensitive, result };
	return CharString_cutAfterString(&cut, other, isFirst);
}

static inline Bool CharString_cutAfterStringInsensitive(
	const CharString *s, const CharString *other, Bool isFirst, CharString *result
) {
	const CharStringCut cut = { s, EStringCase_Insensitive, result };
	return CharString_cutAfterString(&cut, other, isFirst);
}

static inline Bool CharString_cutAfterLastStringSensitive(const CharString *s, const CharString *other, CharString *result) {
	return CharString_cutAfterStringSensitive(s, other, false, result);
}

static inline Bool CharString_cutAfterLastStringInsensitive(const CharString *s, const CharString *other, CharString *result) {
	return CharString_cutAfterStringInsensitive(s, other, false, result);
}

static inline Bool CharString_cutAfterFirstStringSensitive(const CharString *s, const CharString *other, CharString *result) {
	return CharString_cutAfterStringSensitive(s, other, true, result);
}

static inline Bool CharString_cutAfterFirstStringInsensitive(
	const CharString *s, const CharString *other, CharString *result
) {
	return CharString_cutAfterStringInsensitive(s, other, true, result);
}

static inline Bool CharString_cutBeforeLast(const CharStringCut *cut, C8 c) {
	return CharString_cutBefore(cut, c, false);
}

static inline Bool CharString_cutBeforeFirst(const CharStringCut *cut, C8 c) {
	return CharString_cutBefore(cut, c, true);
}

static inline Bool CharString_cutBeforeLastString(const CharStringCut *cut, const CharString *other) {
	return CharString_cutBeforeString(cut, other, false);
}

static inline Bool CharString_cutBeforeFirstString(const CharStringCut *cut, const CharString *other) {
	return CharString_cutBeforeString(cut, other, true);
}

static inline Bool CharString_cutBeforeSensitive(const CharString *s, C8 c, Bool isFirst, CharString *result) {
	const CharStringCut cut = { s, EStringCase_Sensitive, result };
	return CharString_cutBefore(&cut, c, isFirst);
}

static inline Bool CharString_cutBeforeInsensitive(const CharString *s, C8 c, Bool isFirst, CharString *result) {
	const CharStringCut cut = { s, EStringCase_Insensitive, result };
	return CharString_cutBefore(&cut, c, isFirst);
}

static inline Bool CharString_cutBeforeLastSensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutBeforeSensitive(s, c, false, result);
}

static inline Bool CharString_cutBeforeLastInsensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutBeforeInsensitive(s, c, false, result);
}

static inline Bool CharString_cutBeforeFirstSensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutBeforeSensitive(s, c, true, result);
}

static inline Bool CharString_cutBeforeFirstInsensitive(const CharString *s, C8 c, CharString *result) {
	return CharString_cutBeforeInsensitive(s, c, true, result);
}

static inline Bool CharString_cutBeforeStringSensitive(
	const CharString *s, const CharString *other, Bool isFirst, CharString *result
) {
	const CharStringCut cut = { s, EStringCase_Sensitive, result };
	return CharString_cutBeforeString(&cut, other, isFirst);
}

static inline Bool CharString_cutBeforeStringInsensitive(
	const CharString *s, const CharString *other, Bool isFirst, CharString *result
) {
	const CharStringCut cut = { s, EStringCase_Insensitive, result };
	return CharString_cutBeforeString(&cut, other, isFirst);
}

static inline Bool CharString_cutBeforeLastStringSensitive(const CharString *s, const CharString *other, CharString *result) {
	return CharString_cutBeforeStringSensitive(s, other, false, result);
}

static inline Bool CharString_cutBeforeLastStringInsensitive(
	const CharString *s, const CharString *other, CharString *result
) {
	return CharString_cutBeforeStringInsensitive(s, other, false, result);
}

static inline Bool CharString_cutBeforeFirstStringSensitive(const CharString *s, const CharString *other, CharString *result) {
	return CharString_cutBeforeStringSensitive(s, other, true, result);
}

static inline Bool CharString_cutBeforeFirstStringInsensitive(
	const CharString *s, const CharString *other, CharString *result
) {
	return CharString_cutBeforeStringInsensitive(s, other, true, result);
}

#ifdef __cplusplus
	}
#endif
