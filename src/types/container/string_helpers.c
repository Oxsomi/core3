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

#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

Error CharString_splitSensitive(CharString s, C8 c, Allocator allocator, ListCharString *result) {
	return CharString_split(s, c, EStringCase_Sensitive, allocator, result);
}

Error CharString_splitInsensitive(CharString s, C8 c, Allocator allocator, ListCharString *result) {
	return CharString_split(s, c, EStringCase_Insensitive, allocator, result);
}

Error CharString_splitStringSensitive(CharString s, CharString other, Allocator allocator, ListCharString *result) {
	return CharString_splitString(s, other, EStringCase_Sensitive, allocator, result);
}

Error CharString_splitStringInsensitive(CharString s, CharString other, Allocator allocator, ListCharString *result) {
	return CharString_splitString(s, other, EStringCase_Insensitive, allocator, result);
}

Error CharString_replaceFirstString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	U64 off,
	U64 len
) {
	return CharString_replaceString(s, search, replace, caseSensitive, allocator, true, off, len);
}

Error CharString_replaceLastString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	U64 off,
	U64 len
) {
	return CharString_replaceString(s, search, replace, caseSensitive, allocator, false, off, len);
}

Error CharString_replaceAllStringSensitive(
	CharString *s, CharString search, CharString replace, Allocator allocator, U64 off, U64 len
) {
	return CharString_replaceAllString(s, search, replace, EStringCase_Sensitive, allocator, off, len);
}

Error CharString_replaceAllStringInsensitive(
	CharString *s, CharString search, CharString replace, Allocator allocator, U64 off, U64 len
) {
	return CharString_replaceAllString(s, search, replace, EStringCase_Insensitive, allocator, off, len);
}

Error CharString_replaceStringSensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	Bool isFirst,
	U64 off,
	U64 len
) {
	return CharString_replaceString(s, search, replace, EStringCase_Sensitive, allocator, isFirst, off, len);
}

Error CharString_replaceStringInsensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	Bool isFirst,
	U64 off,
	U64 len
) {
	return CharString_replaceString(s, search, replace, EStringCase_Insensitive, allocator, isFirst, off, len);
}

Error CharString_replaceFirstStringSensitive(
	CharString *s, CharString search, CharString replace, Allocator allocator, U64 off, U64 len
) {
	return CharString_replaceFirstString(s, search, replace, EStringCase_Sensitive, allocator, off, len);
}

Error CharString_replaceFirstStringInsensitive(
	CharString *s, CharString search, CharString replace, Allocator allocator, U64 off, U64 len
) {
	return CharString_replaceFirstString(s, search, replace, EStringCase_Insensitive, allocator, off, len);
}

Error CharString_replaceLastStringSensitive(
	CharString *s, CharString search, CharString replace, Allocator allocator, U64 off, U64 len
) {
	return CharString_replaceLastString(s, search, replace, EStringCase_Sensitive, allocator, off, len);
}

Error CharString_replaceLastStringInsensitive(
	CharString *s, CharString search, CharString replace, Allocator allocator, U64 off, U64 len
) {
	return CharString_replaceLastString(s, search, replace, EStringCase_Insensitive, allocator, off, len);
}

Bool CharString_startsWithSensitive(CharString str, C8 c, U64 off) {
	return CharString_startsWith(str, c, EStringCase_Sensitive, off);
}

Bool CharString_startsWithInsensitive(CharString str, C8 c, U64 off) {
	return CharString_startsWith(str, c, EStringCase_Insensitive, off);
}

Bool CharString_startsWithStringSensitive(CharString str, CharString other, U64 off) {
	return CharString_startsWithString(str, other, EStringCase_Sensitive, off);
}

Bool CharString_startsWithStringInsensitive(CharString str, CharString other, U64 off) {
	return CharString_startsWithString(str, other, EStringCase_Insensitive, off);
}

Bool CharString_endsWithSensitive(CharString str, C8 c, U64 off) {
	return CharString_endsWith(str, c, EStringCase_Sensitive, off);
}

Bool CharString_endsWithInsensitive(CharString str, C8 c, U64 off) {
	return CharString_endsWith(str, c, EStringCase_Insensitive, off);
}

Bool CharString_endsWithStringSensitive(CharString str, CharString other, U64 off) {
	return CharString_endsWithString(str, other, EStringCase_Sensitive, off);
}

Bool CharString_endsWithStringInsensitive(CharString str, CharString other, U64 off) {
	return CharString_endsWithString(str, other, EStringCase_Insensitive, off);
}

U64 CharString_countAllSensitive(CharString s, C8 c, U64 off) {
	return CharString_countAll(s, c, EStringCase_Sensitive, off);
}

U64 CharString_countAllInsensitive(CharString s, C8 c, U64 off) {
	return CharString_countAll(s, c, EStringCase_Insensitive, off);
}

U64 CharString_countAllStringSensitive(CharString str, CharString other, U64 off) {
	return CharString_countAllString(str, other, EStringCase_Sensitive, off);
}

U64 CharString_countAllStringInsensitive(CharString str, CharString other, U64 off) {
	return CharString_countAllString(str, other, EStringCase_Insensitive, off);
}

Bool CharString_equalsSensitive(CharString s, C8 c) { return CharString_equals(s, c, EStringCase_Sensitive); }
Bool CharString_equalsInsensitive(CharString s, C8 c) { return CharString_equals(s, c, EStringCase_Insensitive); }

Bool CharString_equalsStringSensitive(CharString s, CharString other) {
	return CharString_equalsString(s, other, EStringCase_Sensitive);
}

Bool CharString_equalsStringInsensitive(CharString s, CharString other) {
	return CharString_equalsString(s, other, EStringCase_Insensitive);
}

Error ListCharString_combine(ListCharString arr, Allocator alloc, CharString *result) {
	return ListCharString_concatString(arr, CharString_createNull(), alloc, result);
}

U64 CharString_find(CharString s, C8 c, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len) {
	return isFirst ? CharString_findFirst(s, c, caseSensitive, off, len) :
		CharString_findLast(s, c, caseSensitive, off, len);
}

U64 CharString_findString(CharString s, CharString other, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len) {
	return isFirst ? CharString_findFirstString(s, other, caseSensitive, off, len) :
		CharString_findLastString(s, other, caseSensitive, off, len);
}

Error CharString_findAllSensitive(CharString s, C8 c, U64 off, U64 len, Allocator alloc, ListU64 *result) {
	return CharString_findAll(s, c, alloc, EStringCase_Sensitive, off, len, result);
}

Error CharString_findAllInsensitive(CharString s, C8 c, U64 off, U64 len, Allocator alloc, ListU64 *result) {
	return CharString_findAll(s, c, alloc, EStringCase_Insensitive, off, len, result);
}

Error CharString_findAllStringSensitive(CharString s, CharString other, U64 off, U64 len, Allocator alloc, ListU64 *result) {
	return CharString_findAllString(s, other, alloc, EStringCase_Sensitive, off, len, result);
}

Error CharString_findAllStringInsensitive(CharString s, CharString other, U64 off, U64 len, Allocator alloc, ListU64 *result) {
	return CharString_findAllString(s, other, alloc, EStringCase_Insensitive, off, len, result);
}

U64 CharString_findFirstSensitive(CharString s, C8 c, U64 off, U64 len) {
	return CharString_findFirst(s, c, EStringCase_Sensitive, off, len);
}

U64 CharString_findFirstInsensitive(CharString s, C8 c, U64 off, U64 len) {
	return CharString_findFirst(s, c, EStringCase_Insensitive, off, len);
}

U64 CharString_findLastSensitive(CharString s, C8 c, U64 off, U64 len) {
	return CharString_findLast(s, c, EStringCase_Sensitive, off, len);
}

U64 CharString_findLastInsensitive(CharString s, C8 c, U64 off, U64 len) {
	return CharString_findLast(s, c, EStringCase_Insensitive, off, len);
}

U64 CharString_findSensitive(CharString s, C8 c, Bool isFirst, U64 off, U64 len) {
	return CharString_find(s, c, EStringCase_Sensitive, isFirst, off, len);
}

U64 CharString_findInsensitive(CharString s, C8 c, Bool isFirst, U64 off, U64 len) {
	return CharString_find(s, c, EStringCase_Insensitive, isFirst, off, len);
}

U64 CharString_findFirstStringSensitive(CharString s, CharString other, U64 off, U64 len) {
	return CharString_findFirstString(s, other, EStringCase_Sensitive, off, len);
}

U64 CharString_findFirstStringInsensitive(CharString s, CharString other, U64 off, U64 len) {
	return CharString_findFirstString(s, other, EStringCase_Insensitive, off, len);
}

U64 CharString_findLastStringSensitive(CharString s, CharString other, U64 off, U64 len) {
	return CharString_findLastString(s, other, EStringCase_Sensitive, off, len);
}

U64 CharString_findLastStringInsensitive(CharString s, CharString other, U64 off, U64 len) {
	return CharString_findLastString(s, other, EStringCase_Insensitive, off, len);
}

U64 CharString_findStringSensitive(CharString s, CharString other, Bool isFirst, U64 off, U64 len) {
	return CharString_findString(s, other, EStringCase_Sensitive, isFirst, off, len);
}

U64 CharString_findStringInsensitive(CharString s, CharString other, Bool isFirst, U64 off, U64 len) {
	return CharString_findString(s, other, EStringCase_Insensitive, isFirst, off, len);
}

Bool CharString_contains(CharString str, C8 c, EStringCase caseSensitive, U64 off, U64 len) {
	return CharString_findFirst(str, c, caseSensitive, off, len) != U64_MAX;
}

Bool CharString_containsString(CharString str, CharString other, EStringCase caseSensitive, U64 off, U64 len) {
	return CharString_findFirstString(str, other, caseSensitive, off, len) != U64_MAX;
}

Bool CharString_containsSensitive(CharString str, C8 c, U64 off, U64 len) {
	return CharString_contains(str, c, EStringCase_Sensitive, off, len);
}

Bool CharString_containsInsensitive(CharString str, C8 c, U64 off, U64 len) {
	return CharString_contains(str, c, EStringCase_Insensitive, off, len);
}

Bool CharString_containsStringSensitive(CharString str, CharString other, U64 off, U64 len) {
	return CharString_containsString(str, other, EStringCase_Sensitive, off, len);
}

Bool CharString_containsStringInsensitive(CharString str, CharString other, U64 off, U64 len) {
	return CharString_containsString(str, other, EStringCase_Insensitive, off, len);
}

Bool CharString_cutAfterLast(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfter(s, c, caseSensitive, false, result);
}

Bool CharString_cutAfterFirst(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfter(s, c, caseSensitive, true, result);
}

Bool CharString_cutAfterLastString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfterString(s, other, caseSensitive, false, result);
}

Bool CharString_cutAfterFirstString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfterString(s, other, caseSensitive, true, result);
}

Bool CharString_cutBeforeLast(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBefore(s, c, caseSensitive, false, result);
}

Bool CharString_cutBeforeFirst(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBefore(s, c, caseSensitive, true, result);
}

Bool CharString_cutBeforeLastString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBeforeString(s, other, caseSensitive, false, result);
}

Bool CharString_cutBeforeFirstString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBeforeString(s, other, caseSensitive, true, result);
}

Bool CharString_cutAfterSensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutAfter(s, c, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutAfterInsensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutAfter(s, c, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutAfterLastSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterLast(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterLastInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterLast(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutAfterFirstSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterFirst(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterFirstInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterFirst(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutAfterStringSensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutAfterString(s, other, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutAfterStringInsensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutAfterString(s, other, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutAfterFirstStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterFirstString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterFirstStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterFirstString(s, other, EStringCase_Insensitive, result);
}

Bool CharString_cutAfterLastStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterLastString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterLastStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterLastString(s, other, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeSensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutBefore(s, c, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutBeforeInsensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutBefore(s, c, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutBeforeLastSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeLast(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeLastInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeLast(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeFirstSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeFirst(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeFirstInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeFirst(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeStringSensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutBeforeString(s, other, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutBeforeStringInsensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutBeforeString(s, other, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutBeforeFirstStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeFirstString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeFirstStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeFirstString(s, other, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeLastStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeLastString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeLastStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeLastString(s, other, EStringCase_Insensitive, result);
}

Bool CharString_eraseFirst(CharString *s, C8 c, EStringCase caseSensitive, U64 off, U64 len) {
	return CharString_erase(s, c, caseSensitive, true, off, len);
}

Bool CharString_eraseLast(CharString *s, C8 c, EStringCase caseSensitive, U64 off, U64 len) {
	return CharString_erase(s, c, caseSensitive, false, off, len);
}

Bool CharString_eraseFirstString(CharString *s, CharString other, EStringCase caseSensitive, U64 off, U64 len){
	return CharString_eraseString(s, other, caseSensitive, true, off, len);
}

Bool CharString_eraseLastString(CharString *s, CharString other, EStringCase caseSensitive, U64 off, U64 len) {
	return CharString_eraseString(s, other, caseSensitive, false, off, len);
}

Bool CharString_eraseAllSensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseAll(s, c, EStringCase_Sensitive, off, len);
}

Bool CharString_eraseAllInsensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseAll(s, c, EStringCase_Insensitive, off, len);
}

Bool CharString_eraseSensitive(CharString *s, C8 c, Bool isFirst, U64 off, U64 len) {
	return CharString_erase(s, c, EStringCase_Sensitive, isFirst, off, len);
}

Bool CharString_eraseInsensitive(CharString *s, C8 c, Bool isFirst, U64 off, U64 len) {
	return CharString_erase(s, c, EStringCase_Insensitive, isFirst, off, len);
}

Bool CharString_eraseFirstSensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseFirst(s, c, EStringCase_Sensitive, off, len);
}

Bool CharString_eraseFirstInsensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseFirst(s, c, EStringCase_Insensitive, off, len);
}

Bool CharString_eraseLastSensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseLast(s, c, EStringCase_Sensitive, off, len);
}

Bool CharString_eraseLastInsensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseLast(s, c, EStringCase_Insensitive, off, len);
}

Bool CharString_eraseAllStringSensitive(CharString *s, CharString other, U64 off, U64 len) {
	return CharString_eraseAllString(s, other, EStringCase_Sensitive, off, len);
}

Bool CharString_eraseAllStringInsensitive(CharString *s, CharString other, U64 off, U64 len) {
	return CharString_eraseAllString(s, other, EStringCase_Insensitive, off, len);
}

Bool CharString_eraseStringSensitive(CharString *s, CharString other, Bool isFirst, U64 off, U64 len) {
	return CharString_eraseString(s, other, EStringCase_Sensitive, isFirst, off, len);
}

Bool CharString_eraseStringInsensitive(CharString *s, CharString other, Bool isFirst, U64 off, U64 len) {
	return CharString_eraseString(s, other, EStringCase_Insensitive, isFirst, off, len);
}

Bool CharString_eraseFirstStringSensitive(CharString *s, CharString other, U64 off, U64 len) {
	return CharString_eraseFirstString(s, other, EStringCase_Sensitive, off, len);
}

Bool CharString_eraseFirstStringInsensitive(CharString *s, CharString other, U64 off, U64 len) {
	return CharString_eraseFirstString(s, other, EStringCase_Insensitive, off, len);
}

Bool CharString_eraseLastStringSensitive(CharString *s, CharString other, U64 off, U64 len) {
	return CharString_eraseLastString(s, other, EStringCase_Sensitive, off, len);
}

Bool CharString_eraseLastStringInsensitive(CharString *s, CharString other, U64 off, U64 len) {
	return CharString_eraseLastString(s, other, EStringCase_Insensitive, off, len);
}

Bool CharString_replaceFirst(CharString *s, C8 c, C8 v, EStringCase caseSensitive, U64 off, U64 len) {
	return CharString_replace(s, c, v, caseSensitive, true, off, len);
}

Bool CharString_replaceLast(CharString *s, C8 c, C8 v, EStringCase caseSensitive, U64 off, U64 len) {
	return CharString_replace(s, c, v, caseSensitive, false, off, len);
}

Bool CharString_replaceAllSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceAll(s, c, v, EStringCase_Sensitive, off, len);
}

Bool CharString_replaceAllInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceAll(s, c, v, EStringCase_Insensitive, off, len);
}

Bool CharString_replaceSensitive(CharString *s, C8 c, C8 v, Bool isFirst, U64 off, U64 len) {
	return CharString_replace(s, c, v, EStringCase_Sensitive, isFirst, off, len);
}

Bool CharString_replaceInsensitive(CharString *s, C8 c, C8 v, Bool isFirst, U64 off, U64 len) {
	return CharString_replace(s, c, v, EStringCase_Insensitive, isFirst, off, len);
}

Bool CharString_replaceFirstSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceFirst(s, c, v, EStringCase_Sensitive, off, len);
}

Bool CharString_replaceFirstInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceFirst(s, c, v, EStringCase_Insensitive, off, len);
}

Bool CharString_replaceLastSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceLast(s, c, v, EStringCase_Sensitive, off, len);
}

Bool CharString_replaceLastInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceLast(s, c, v, EStringCase_Insensitive, off, len);
}

Bool CharString_toLower(CharString str) { return CharString_transform(str, EStringTransform_Lower); }
Bool CharString_toUpper(CharString str) { return CharString_transform(str, EStringTransform_Upper); }

ECompareResult CharString_compareSensitive(CharString a, CharString b) {
	return CharString_compare(a, b, EStringCase_Sensitive);
}

ECompareResult CharString_compareInsensitive(CharString a, CharString b) {
	return CharString_compare(a, b, EStringCase_Insensitive);
}
