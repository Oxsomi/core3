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
#include "types/container/string.h"

#ifdef __cplusplus
	extern "C" {
#endif
		
static inline Bool CharString_replaceFirstString(const CharStringReplace2 *replace, EStringCase caseSensitive, Error *e_rr) {
	return CharString_replaceString(replace, true, caseSensitive, e_rr);
}

static inline Bool CharString_replaceLastString(const CharStringReplace2 *replace, EStringCase caseSensitive, Error *e_rr) {
	return CharString_replaceString(replace, false, caseSensitive, e_rr);
}

static inline Bool CharString_splitSensitive(const CharStringSplit *split, C8 c, Error *e_rr) {
	return CharString_split(split, c, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_splitInsensitive(const CharStringSplit *split, C8 c, Error *e_rr) {
	return CharString_split(split, c, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_splitStringSensitive(const CharStringSplit *split, const CharString *other, Error *e_rr) {
	return CharString_splitString(split, other, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_splitStringInsensitive(const CharStringSplit *split, const CharString *other, Error *e_rr) {
	return CharString_splitString(split, other, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_replaceAllStringSensitive(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceAllString(replace, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceAllStringInsensitive(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceAllString(replace, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_replaceStringSensitive(const CharStringReplace2 *replace, Bool isFirst, Error *e_rr) {
	return CharString_replaceString(replace, isFirst, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceStringInsensitive(const CharStringReplace2 *replace, Bool isFirst, Error *e_rr) {
	return CharString_replaceString(replace, isFirst, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_replaceLastStringSensitive(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceString(replace, false, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceLastStringInsensitive(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceString(replace, false, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_replaceFirstStringSensitive(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceString(replace, true, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceFirstStringInsensitive(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceString(replace, true, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_findAllSensitive(const CharStringFind *find, C8 c, Error *e_rr) {
	return CharString_findAll(find, c, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_findAllInsensitive(const CharStringFind *find, C8 c, Error *e_rr) {
	return CharString_findAll(find, c, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_findAllStringSensitive(const CharStringFind *find, const CharString *other, Error *e_rr) {
	return CharString_findAllString(find, other, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_findAllStringInsensitive(const CharStringFind *find, const CharString *other, Error *e_rr) {
	return CharString_findAllString(find, other, EStringCase_Insensitive, e_rr);
}

#ifdef __cplusplus
	}
#endif
