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

//types/base/unicode.c

#include "types/base/unicode.h"

typedef struct UnicodeCaseRange {
	U32 first;
	I32 delta;
	U16 count, stride;
} UnicodeCaseRange;

#include "unicode_case.inc.h"

U8 UnicodeCodePoint_fromUTF8(const C8 *ptr, U64 len, UnicodeCodePoint *c) {

	if (!ptr || !len || !c)
		return 0;

	const U8 v0 = (U8) ptr[0];

	if (v0 < 0x80) {
		*c = v0;
		return 1;
	}

	const U8 bytes = UnicodeCodePoint_leadLengthUTF8((C8) v0);

	if (!bytes || len < bytes)
		return 0;

	//The second byte's range also rules out overlong three and four byte forms, surrogates and past U+10FFFF

	const U8 v1 = (U8) ptr[1];
	const U8 min1 = v0 == 0xE0 ? 0xA0 : (v0 == 0xF0 ? 0x90 : 0x80);
	const U8 max1 = v0 == 0xED ? 0x9F : (v0 == 0xF4 ? 0x8F : 0xBF);

	if (v1 < min1 || v1 > max1)
		return 0;

	U32 v = ((U32) v0 & (0x7F >> bytes)) << 6 | (v1 & 0x3F);

	for (U8 i = 2; i < bytes; ++i) {

		const U8 vi = (U8) ptr[i];

		if ((vi & 0xC0) != 0x80)
			return 0;

		v = v << 6 | (vi & 0x3F);
	}

	*c = v;
	return bytes;
}

U8 UnicodeCodePoint_toUTF8(UnicodeCodePoint c, C8 *ptr, U64 len) {

	if (!ptr || (c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)
		return 0;

	const U8 bytes = UnicodeCodePoint_lengthUTF8(c);

	if (len < bytes)
		return 0;

	if (bytes == 1) {
		ptr[0] = (C8) c;
		return 1;
	}

	static const U8 lead[5] = { 0, 0, 0xC0, 0xE0, 0xF0 };

	for (U8 i = bytes - 1; i; --i, c >>= 6)
		ptr[i] = (C8) (0x80 | (c & 0x3F));

	ptr[0] = (C8) (lead[bytes] | c);
	return bytes;
}

static UnicodeCodePoint UnicodeCodePoint_map(const UnicodeCaseRange *ranges, U64 count, UnicodeCodePoint c) {

	//The last run that starts at or before c: every step halves what's left, keeping the upper half when its first run
	// still starts at or before c. The loop runs log2(count) times whatever c is, and the pick is a select a compiler
	// can turn into a conditional move, so the search needn't branch on c.

	const UnicodeCaseRange *run = ranges;

	for (U64 left = count; left > 1; ) {
		const U64 half = left >> 1;
		run = run[half].first <= c ? run + half : run;
		left -= half;
	}

	if (run->first > c)
		return c;

	//The run maps every stride-th code point from first; the ones a stride of 2 skips past don't map

	const U32 step = c - run->first;
	const Bool mapped = step < (U32) run->count * run->stride && !(step & (run->stride - 1));

	return mapped ? (UnicodeCodePoint) ((I32) c + run->delta) : c;
}

#define UnicodeCodePoint_mapTable(table, c) UnicodeCodePoint_map(table, sizeof(table) / sizeof(table[0]), c)

UnicodeCodePoint UnicodeCodePoint_caseFold(UnicodeCodePoint c) {

	if (c < 0x80)
		return c - 'A' < 26 ? c + ('a' - 'A') : c;

	return UnicodeCodePoint_mapTable(UnicodeCase_fold, c);
}

UnicodeCodePoint UnicodeCodePoint_toLower(UnicodeCodePoint c) {

	if (c < 0x80)
		return c - 'A' < 26 ? c + ('a' - 'A') : c;

	return UnicodeCodePoint_mapTable(UnicodeCase_lower, c);
}

UnicodeCodePoint UnicodeCodePoint_toUpper(UnicodeCodePoint c) {

	if (c < 0x80)
		return c - 'a' < 26 ? c - ('a' - 'A') : c;

	return UnicodeCodePoint_mapTable(UnicodeCase_upper, c);
}
