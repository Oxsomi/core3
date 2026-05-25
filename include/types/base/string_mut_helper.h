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

//types/base/string_mut_helper.h

#pragma once
#include "types/base/string_mut.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Erase

static inline Bool CharString_eraseFirst(const CharStringReplaceErase *erase, C8 c) {
	return CharString_erase(erase, c, true);
}

static inline Bool CharString_eraseLast(const CharStringReplaceErase *erase, C8 c) {
	return CharString_erase(erase, c, false);
}

static inline Bool CharString_eraseFirstString(const CharStringReplaceErase *erase, const CharString *other) {
	return CharString_eraseString(erase, other, true);
}

static inline Bool CharString_eraseLastString(const CharStringReplaceErase *erase, const CharString *other) {
	return CharString_eraseString(erase, other, false);
}

static inline Bool CharString_eraseAllSensitive(CharString *s, C8 c, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Sensitive, off, len };
	return CharString_eraseAll(&erase, c);
}

static inline Bool CharString_eraseAllInsensitive(CharString *s, C8 c, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Insensitive, off, len };
	return CharString_eraseAll(&erase, c);
}

static inline Bool CharString_eraseSensitive(CharString *s, C8 c, Bool isFirst, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Sensitive, off, len };
	return CharString_erase(&erase, c, isFirst);
}

static inline Bool CharString_eraseInsensitive(CharString *s, C8 c, Bool isFirst, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Insensitive, off, len };
	return CharString_erase(&erase, c, isFirst);
}

static inline Bool CharString_eraseLastSensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseSensitive(s, c, false, off, len);
}

static inline Bool CharString_eraseLastInsensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseInsensitive(s, c, false, off, len);
}

static inline Bool CharString_eraseFirstSensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseSensitive(s, c, true, off, len);
}

static inline Bool CharString_eraseFirstInsensitive(CharString *s, C8 c, U64 off, U64 len) {
	return CharString_eraseInsensitive(s, c, true, off, len);
}

static inline Bool CharString_eraseAllStringSensitive(CharString *s, const CharString *other, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Sensitive, off, len };
	return CharString_eraseAllString(&erase, other);
}

static inline Bool CharString_eraseAllStringInsensitive(CharString *s, const CharString *other, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Insensitive, off, len };
	return CharString_eraseAllString(&erase, other);
}

static inline Bool CharString_eraseStringSensitive(CharString *s, const CharString *other, Bool isFirst, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Sensitive, off, len };
	return CharString_eraseString(&erase, other, isFirst);
}

static inline Bool CharString_eraseStringInsensitive(CharString *s, const CharString *other, Bool isFirst, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Insensitive, off, len };
	return CharString_eraseString(&erase, other, isFirst);
}

static inline Bool CharString_eraseLastStringSensitive(CharString *s, const CharString *other, U64 off, U64 len) {
	return CharString_eraseStringSensitive(s, other, false, off, len);
}

static inline Bool CharString_eraseLastStringInsensitive(CharString *s, const CharString *other, U64 off, U64 len) {
	return CharString_eraseStringInsensitive(s, other, false, off, len);
}

static inline Bool CharString_eraseFirstStringSensitive(CharString *s, const CharString *other, U64 off, U64 len) {
	return CharString_eraseStringSensitive(s, other, true, off, len);
}

static inline Bool CharString_eraseFirstStringInsensitive(CharString *s, const CharString *other, U64 off, U64 len) {
	return CharString_eraseStringInsensitive(s, other, true, off, len);
}

//Replace

static inline Bool CharString_replaceLast(const CharStringReplaceErase *replace, C8 c, C8 v) {
	return CharString_replace(replace, c, v, false);
}

static inline Bool CharString_replaceFirst(const CharStringReplaceErase *replace, C8 c, C8 v) {
	return CharString_replace(replace, c, v, true);
}

static inline Bool CharString_replaceAllSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Sensitive, off, len };
	return CharString_replaceAll(&erase, c, v);
}

static inline Bool CharString_replaceAllInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Insensitive, off, len };
	return CharString_replaceAll(&erase, c, v);
}

static inline Bool CharString_replaceSensitive(CharString *s, C8 c, C8 v, Bool isFirst, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Sensitive, off, len };
	return CharString_replace(&erase, c, v, isFirst);
}

static inline Bool CharString_replaceInsensitive(CharString *s, C8 c, C8 v, Bool isFirst, U64 off, U64 len) {
	const CharStringReplaceErase erase = { s, EStringCase_Insensitive, off, len };
	return CharString_replace(&erase, c, v, isFirst);
}

static inline Bool CharString_replaceLastSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceSensitive(s, c, v, false, off, len);
}

static inline Bool CharString_replaceLastInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceInsensitive(s, c, v, false, off, len);
}

static inline Bool CharString_replaceFirstSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceSensitive(s, c, v, true, off, len);
}

static inline Bool CharString_replaceFirstInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len) {
	return CharString_replaceInsensitive(s, c, v, true, off, len);
}

#ifdef __cplusplus
	}
#endif
