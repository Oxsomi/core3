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

#pragma once
#include "types/base/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

Bool CharString_eraseAtCount(CharString *s, U64 i, U64 count, Error *e_rr);

//Used for both CharString_erase and CharString_replace
typedef struct CharStringReplaceErase {
	CharString *s;
	EStringCase caseSensitive;
	U64 off;
	U64 len;
} CharStringReplaceErase;

Bool CharString_eraseAll(const CharStringReplaceErase *erase, C8 c);
Bool CharString_eraseAllString(const CharStringReplaceErase *erase, const CharString *other);
Bool CharString_erase(const CharStringReplaceErase *erase, C8 c, Bool isFirst);
Bool CharString_eraseString(const CharStringReplaceErase *erase, const CharString *other, Bool isFirst);

Bool CharString_replaceAll(const CharStringReplaceErase *replace, C8 c, C8 v);
Bool CharString_replace(const CharStringReplaceErase *replace, C8 c, C8 v, Bool isFirst);

CharString CharString_trim(const CharString s);		//Returns a substring ref in a string

Bool CharString_transform(CharString *s, EStringTransform stringTransform);

static inline Bool CharString_popEndCount(CharString *s, U64 count, Error *e_rr) {
	return CharString_eraseAtCount(s, s ? CharString_length(*s) - count : 0, count, e_rr);
}

static inline Bool CharString_popFrontCount(CharString *s, U64 count, Error *e_rr) {
	return CharString_eraseAtCount(s, 0, count, e_rr);
}

static inline Bool CharString_eraseAt(CharString *s, U64 i, Error *e_rr) { return CharString_eraseAtCount(s, i, 1, e_rr); }
static inline Bool CharString_popFront(CharString *s, Error *e_rr) { return CharString_eraseAt(s, 0, e_rr); }
static inline Bool CharString_popEnd(CharString *s, Error *e_rr) {
	return CharString_eraseAt(s, s ? CharString_length(*s) - 1 : 0, e_rr);
}

static inline Bool CharString_toLower(CharString *str) { return CharString_transform(str, EStringTransform_Lower); }
static inline Bool CharString_toUpper(CharString *str) { return CharString_transform(str, EStringTransform_Upper); }

static inline Bool CharString_formatPath(CharString *str) {
	CharStringReplaceErase replace = { str, EStringCase_Sensitive, 0, 0 };
	return CharString_replaceAll(&replace, '\\', '/');
}

#ifdef __cplusplus
	}
#endif
