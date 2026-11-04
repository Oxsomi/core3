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

//types/base/string_mut.c

#include "types/base/string_read.h"
#include "types/base/string_mut.h"

Bool CharString_erase(const CharStringReplaceErase *erase, C8 c, Bool isFirst) {

	if (!erase)
		return false;

	CharString *s = erase->s;

	if (!s || CharString_isRef(*s))
		return false;

	//Skipping first match

	CharStringSensOffLen strSensOffLen = {
		s,
		erase->caseSensitive,
		erase->off,
		erase->len
	};

	const U64 find = isFirst ? CharString_findFirst(&strSensOffLen, c) : CharString_findLast(&strSensOffLen, c);

	if (find == U64_MAX)
		return false;

	const U64 strl = CharString_length(*s);

	//Closing the gap shifts the tail down by one, so source and destination are the same allocation one byte
	// apart and overlap everywhere but the last byte.
	//memcpy is undefined on overlapping ranges, so this has to be the move.

	Buffer_memmove(
		Buffer_createRef((U8 *) s->ptr + find, strl - find - 1),
		Buffer_createRefConst(s->ptr + find + 1, strl - find - 1)
	);

	s->ptrNonConst[strl - 1] = '\0';
	s->lenAndNullTerminated = (strl - 1) | ((U64) 1 << 63);
	return true;
}

Bool CharString_eraseAtCount(CharString *s, U64 i, U64 count, Error *e_rr) {

	Bool s_uccess = true;

	if (!s)
		retError(clean, Error_nullPointer(0, "CharString_eraseAtCount()::s is required"));

	if (!count)
		goto clean;

	if (CharString_isRef(*s))
		retError(clean, Error_constData(0, 0, "CharString_eraseAtCount()::s should be managed memory"));

	const U64 strl = CharString_length(*s);

	if (i > strl || count > strl - i)
		retError(clean, Error_outOfBounds(0, i, strl, "CharString_eraseAtCount()::i or count is out of bounds"));

	Buffer_memmove(
		Buffer_createRef(s->ptrNonConst + i, strl - i - count),
		Buffer_createRef(s->ptrNonConst + i + count, strl - i - count)
	);

	const U64 end = strl - count;
	s->ptrNonConst[end] = '\0';
	s->lenAndNullTerminated = end | ((U64) 1 << 63);

clean:
	return s_uccess;
}

Bool CharString_eraseString(const CharStringReplaceErase *erase, const CharString *other, Bool isFirst) {

	if (!erase)
		return false;

	CharString *s = erase->s;

	const U64 otherl = !other ? 0 : CharString_length(*other);

	if (!s || !otherl || CharString_isRef(*s))
		return false;

	const U64 strl = CharString_length(*s);

	//Skipping first match

	CharStringSensOffLen strSensOffLen = {
		s,
		erase->caseSensitive,
		erase->off,
		erase->len
	};

	U64 matchLen = 0;

	const U64 find = isFirst ? CharString_findFirstStringMatch(&strSensOffLen, other, &matchLen) :
		CharString_findLastStringMatch(&strSensOffLen, other, &matchLen);

	if (find == U64_MAX)
		return false;

	Buffer_memmove(
		Buffer_createRef(s->ptrNonConst + find, strl - find - matchLen),
		Buffer_createRef(s->ptrNonConst + find + matchLen, strl - find - matchLen)
	);

	const U64 end = strl - matchLen;
	s->ptrNonConst[end] = '\0';
	s->lenAndNullTerminated = end | ((U64) 1 << 63);
	return true;
}

Bool CharString_eraseAll(const CharStringReplaceErase *erase, C8 c) {

	if (!erase)
		return false;

	CharString *s = erase->s;
	U64 strl = s ? CharString_length(*s) : 0;
	U64 realStrl = strl;
	U64 off = erase->off;
	U64 len = erase->len;

	if (!s || CharString_isRef(*s) || off >= strl || len > strl - off)
		return false;

	if (len)
		strl = off + len;

	C8 lower = 0, upper = 0;
	C8_caseVariants(c, erase->caseSensitive, &lower, &upper);

	U64 out = off;

	for (U64 i = off; i < strl; ++i) {

		if (s->ptr[i] == lower || s->ptr[i] == upper)
			continue;

		s->ptrNonConst[out++] = s->ptr[i];
	}

	if (out == strl)
		return true;

	for(U64 i = strl; i < realStrl; ++i)
		s->ptrNonConst[out++] = s->ptr[i];

	s->ptrNonConst[out] = '\0';
	s->lenAndNullTerminated = out | ((U64) 1 << 63);
	return true;
}

Bool CharString_eraseAllString(const CharStringReplaceErase *erase, const CharString *other) {
	
	if (!erase)
		return false;

	CharString *s = erase->s;
	U64 strl = s ? CharString_length(*s) : 0;
	const U64 realStrl = strl;
	U64 off = erase->off;
	U64 len = erase->len;

	const U64 otherl = !other ? 0 : CharString_length(*other);

	if (!s || !otherl || CharString_isRef(*s) || off >= strl || len > strl - off)
		return false;

	if (len)
		strl = off + len;

	U64 out = off;

	//Searching ahead only reads bytes from i on, and everything kept so far was written before i

	for (U64 i = off; i < strl; ) {

		const CharStringSensOffLen at = { s, erase->caseSensitive, i, strl - i };
		U64 matchLen = 0;

		const U64 find = CharString_findFirstStringMatch(&at, other, &matchLen);
		const U64 keepEnd = find == U64_MAX ? strl : find;

		Buffer_memmove(
			Buffer_createRef(s->ptrNonConst + out, keepEnd - i),
			Buffer_createRef(s->ptrNonConst + i, keepEnd - i)
		);

		out += keepEnd - i;

		if (find == U64_MAX)
			break;

		i = find + matchLen;
	}

	if (out == strl)
		return true;

	for(U64 i = strl; i < realStrl; ++i)
		s->ptrNonConst[out++] = s->ptr[i];

	s->ptrNonConst[out] = '\0';
	s->lenAndNullTerminated = out | ((U64) 1 << 63);
	return true;
}

Bool CharString_replaceAll(const CharStringReplaceErase *replace, C8 c, C8 v) {

	if (!replace)
		return false;

	CharString *s = replace->s;
	U64 strl = s ? CharString_length(*s) : 0;
	U64 off = replace->off;
	U64 len = replace->len;

	if (!s || CharString_isConstRef(*s) || off >= strl || len > strl - off)
		return false;

	if (!len)
		len = strl - off;

	C8 lower = 0, upper = 0;
	C8_caseVariants(c, replace->caseSensitive, &lower, &upper);

	for (U64 i = off; i < off + len; ++i)
		if (s->ptr[i] == lower || s->ptr[i] == upper)
			s->ptrNonConst[i] = v;

	return true;
}

Bool CharString_replace(const CharStringReplaceErase *replace, C8 c, C8 v, Bool isFirst) {

	if (!replace)
		return false;

	CharString *s = replace->s;

	if (!s || CharString_isConstRef(*s))
		return false;

	CharStringSensOffLen strSensOffLen = {
		s,
		replace->caseSensitive,
		replace->off,
		replace->len
	};

	const U64 i = isFirst ? CharString_findFirst(&strSensOffLen, c) : CharString_findLast(&strSensOffLen, c);

	if (i != U64_MAX)
		s->ptrNonConst[i] = v;

	return true;
}

Bool CharString_transformAscii(CharString *str, EStringTransform stringTransform) {

	if (!str || CharString_isConstRef(*str))
		return false;

	for (U64 i = 0; i < CharString_length(*str); ++i)
		str->ptrNonConst[i] = C8_transform(str->ptr[i], stringTransform);

	return true;
}

CharString CharString_trim(const CharString s) {

	const U64 strl = CharString_length(s);
	U64 first = strl;

	for (U64 i = 0; i < strl; ++i)
		if (!C8_isWhitespace(s.ptr[i])) {
			first = i;
			break;
		}

	if (first == strl)
		return CharString_createNull();

	U64 last = strl;

	for (U64 i = strl - 1; i != U64_MAX; --i)
		if (!C8_isWhitespace(s.ptr[i])) {
			last = i;
			break;
		}

	++last;        //We wanna include the last character that's non whitespace too

	const Bool isNullTerm = CharString_isNullTerminated(s) && last == strl;

	return CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr + first, last - first, isNullTerm) :
		CharString_createRefSized(s.ptrNonConst + first, last - first, isNullTerm);
}
