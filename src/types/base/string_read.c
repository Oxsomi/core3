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

//types/base/string_read.c

#include "types/base/string_read_helper.h"
#include "types/base/algorithm.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/base/unicode.h"
#include <stdlib.h>

Bool CharString_isValidFileName(const CharString str) {

	const CharString CON = CharString_createRefCStrConst("CON");
	const CharString AUX = CharString_createRefCStrConst("AUX");
	const CharString NUL = CharString_createRefCStrConst("NUL");
	const CharString PRN = CharString_createRefCStrConst("PRN");
	const CharString COM = CharString_createRefCStrConst("COM");
	const CharString LPT = CharString_createRefCStrConst("LPT");

	for(U64 i = 0; i < CharString_length(str); ++i)
		if(!C8_isValidFileName(str.ptr[i]))
			return false;

	//Trailing or leading space is illegal

	if(CharString_endsWithSensitive(str, ' ', 0))
		return false;

	if(CharString_startsWithSensitive(str, ' ', 0))
		return false;

	//Validation to make sure we're not using weird legacy MS DOS keywords
	//Because these will not be writable correctly!

	U64 illegalStart = 0;
	const U64 strl = CharString_length(str);

	if (strl >= 3) {

		if (
			CharString_startsWithStringInsensitive(&str, &CON, 0) ||
			CharString_startsWithStringInsensitive(&str, &AUX, 0) ||
			CharString_startsWithStringInsensitive(&str, &NUL, 0) ||
			CharString_startsWithStringInsensitive(&str, &PRN, 0)
		)
			illegalStart = 3;

		else if (strl >= 4) {

			if (
				(CharString_startsWithStringInsensitive(&str, &COM, 0) || CharString_startsWithStringInsensitive(&str, &LPT, 0))
				&& C8_isDec(str.ptr[3])
			)
				illegalStart = 4;
		}
	}

	//PRNtscreen.pdf is valid, but PRN.pdf isn't.
	///NULlpointer.txt is valid, NUL.txt isn't.

	if(illegalStart && (strl == illegalStart || CharString_getAt(str, illegalStart) == '.'))
		return false;

	//Can't end with trailing . (so . and .. are illegal)

	if (strl && str.ptr[strl - 1] == '.')
		return false;

	//If string is not empty (and less than 256 chars) then it's a valid file name

	return strl && strl < 256;
}

//We support valid file names or ., .. in file path parts.

static inline Bool CharString_isSupportedInFilePath(CharString str) {
	return CharString_isValidFileName(str) || (
		CharString_getAt(str, 0) == '.' && (
			CharString_length(str) == 1 || (CharString_getAt(str, 1) == '.' && CharString_length(str) == 2)
		)
	);
}

//File_resolve but without validating if it'd be a valid (permitted) path on disk.

Bool CharString_isValidFilePath(CharString str) {

	str = CharString_createRefStrConst(str);

	//A bare virtual root ("//", "//.", "//./") is a valid path referring to the virtual root.
	//It has no name component, so the general validation below (which requires a non-empty remainder) would reject it.
	//Detect it up-front, before the trailing-slash trim collapses "//" into "/".

	if(CharString_getAt(str, 0) == '/' && CharString_getAt(str, 1) == '/') {

		U64 afterLen = CharString_length(str) - 2;                          //Part after the leading "//"

		if(afterLen && str.ptr[CharString_length(str) - 1] == '/')          //Ignore one trailing slash
			--afterLen;

		if(!afterLen || (afterLen == 1 && str.ptr[2] == '.'))              //Empty or a single "."
			return true;
	}

	//Drive relative paths are unsupported; a drive prefix must include its root separator.
	Bool hasPrefix = false;

	#ifdef _WIN32

		if(CharString_length(str) >= 2 && str.ptr[1] == ':') {
			if (
				CharString_length(str) < 3 || !C8_isAlpha(str.ptr[0]) ||
				(str.ptr[2] != '/' && str.ptr[2] != '\\')
			)
				return false;

			if(CharString_length(str) == 3)
				return true;

			str.ptr += 3;
			str.lenAndNullTerminated -= 3;
			hasPrefix = true;
		}

	#else

		if(CharString_length(str) >= 2 && str.ptr[1] == ':')
			return false;

	#endif

	if (
		CharString_getAt(str, CharString_length(str) - 1) == '/' ||
		CharString_getAt(str, CharString_length(str) - 1) == '\\'
	)
		str.lenAndNullTerminated = CharString_length(str) - 1;

	//Virtual files

	if(CharString_getAt(str, 0) == '/' && CharString_getAt(str, 1) == '/') {

		if(hasPrefix)
			return false;

		str.ptr += 2;
		str.lenAndNullTerminated -= 2;
	}

	//Absolute path

	if(CharString_getAt(str, 0) == '/' || CharString_getAt(str, 0) == '\\') {

		if(hasPrefix)
			return false;

		++str.ptr;
		--str.lenAndNullTerminated;
		hasPrefix = true;
	}

	//Windows network paths, this is unsupported currently

	if(CharString_getAt(str, 0) == '\\' && CharString_getAt(str, 1) == '\\')
		return false;

	//Split by / or \.

	U64 prev = 0;
	U64 strl = CharString_length(str);

	//An empty string has a null ptr, and the trailing segment below offsets it, which is undefined even by the
	// zero prev would be here.
	//The final `return !!strl` already rejects the empty string, so leaving early is the same answer.

	if(!strl)
		return false;

	for (U64 i = 0; i < strl; ++i) {

		const C8 c = str.ptr[i];

		//Push previous

		if (c == '/' || c == '\\') {

			if(!(i - prev))
				return false;

			const CharString part = CharString_createRefSizedConst(str.ptr + prev, i - prev, false);

			if(!CharString_isSupportedInFilePath(part))
				return false;

			prev = i + 1;
		}
	}

	//Validate ending

	const CharString part = CharString_createRefSizedConst(str.ptr + prev, strl - prev, CharString_isNullTerminated(str));

	if(!CharString_isSupportedInFilePath(part))
		return false;

	return !!strl;
}

//The folded code point that ends at ptr[*end - 1], moving *end to its start.
//It's decoded from the nearest byte before it that isn't a continuation, at most three back, and only taken if it
// ends exactly there; anything else is one invalid byte. That splits a string the same way UnicodeCodePoint_readFolded
// does.

static inline U32 CharString_prevFolded(const C8 *ptr, U64 *end) {

	const U64 e = *end;
	const U8 last = (U8) ptr[e - 1];

	if (last < 0x80) {
		--*end;
		return (U8) C8_toLower((C8) last);
	}

	U64 start = e - 1;

	while (start && e - start < 4 && ((U8) ptr[start] & 0xC0) == 0x80)
		--start;

	UnicodeCodePoint c = 0;

	if (UnicodeCodePoint_fromUTF8(ptr + start, e - start, &c) == e - start) {
		*end = start;
		return UnicodeCodePoint_caseFold(c);
	}

	--*end;
	return 0x110000 + last;
}

//The insensitive walks only fold where two strings differ: bytes that are the same in both are passed as they are.
//How many bytes of a and b that covers from the start: their common bytes, stopping short of a code point that the
// first difference (or either end) cuts through, since that one has to be folded whole.

__forceinline__ static U64 CharString_sameFolded(const C8 *a, U64 al, const C8 *b, U64 bl) {

	const U64 limit = al < bl ? al : bl;
	U64 same = 0;

	while (same < limit && a[same] == b[same])
		++same;

	if (!same)
		return 0;

	//If the cut is inside a multibyte code point, back up to its lead byte so it gets folded whole: skip continuation
	// bytes (at most three back), and the first other byte is that lead if its length reaches past the cut.

	for (U64 back = 1; back <= 3 && back <= same; ++back) {

		const C8 c = a[same - back];

		if (((U8) c & 0xC0) == 0x80)
			continue;

		if (UnicodeCodePoint_leadLengthUTF8(c) > back)
			same -= back;

		break;
	}

	return same;
}

//Folds the code points at a[*i] and b[*j], moves both past them and orders them (<0, 0 or >0)

static I32 CharString_stepFolded(const C8 *a, U64 *i, U64 al, const C8 *b, U64 *j, U64 bl) {
	const U32 fa = UnicodeCodePoint_readFolded(a, i, al), fb = UnicodeCodePoint_readFolded(b, j, bl);
	return fa < fb ? -1 : (fa > fb ? 1 : 0);
}

//How many bytes of s from off match o (ending at or before end), or U64_MAX if o doesn't match there.

__forceinline__ static U64 CharString_matchAt(const C8 *s, U64 off, U64 end, const C8 *o, U64 ol, EStringCase stringCase) {

	if (stringCase != EStringCase_Insensitive) {

		if (end - off < ol)
			return U64_MAX;

		for (U64 k = 0; k < ol; ++k)
			if (!C8_matches(s[off + k], o[k], stringCase))
				return U64_MAX;

		return ol;
	}

	U64 i = off, k = 0;

	while (k < ol) {

		if (i == end)
			return U64_MAX;

		const C8 x = s[i], y = o[k];

		if (x == y) {

			const U64 same = CharString_sameFolded(s + i, end - i, o + k, ol - k);

			if (same) {
				i += same;
				k += same;
				continue;
			}
		}

		if (((U8) x | (U8) y) < 0x80) {        //Two ASCII bytes that differ can only differ in case

			if (C8_toLower(x) != C8_toLower(y))
				return U64_MAX;

			++i;
			++k;
			continue;
		}

		if (CharString_stepFolded(s, &i, end, o, &k, ol))
			return U64_MAX;
	}

	return i - off;
}

//The bytes a match of o can start at, so a search passes every other position with a compare or two: o's first byte
// in either case, and under Insensitive the first folded code point's, plus anything past ASCII, which can fold onto
// it (U+212A Kelvin sign onto k).

typedef struct CharStringMatchStart {
	U32 folded;                //Under Insensitive: o's first folded code point, what a code point past ASCII has to fold to
	C8 lower, upper;
	Bool pastAscii;
	U8 padding;
} CharStringMatchStart;

static inline CharStringMatchStart CharString_matchStart(const CharString *o, EStringCase stringCase) {

	CharStringMatchStart start = (CharStringMatchStart) { 0 };

	if (stringCase != EStringCase_Insensitive) {
		C8_caseVariants(o->ptr[0], stringCase, &start.lower, &start.upper);
		return start;
	}

	U64 k = 0;
	start.folded = UnicodeCodePoint_readFolded(o->ptr, &k, CharString_length(*o));

	//An invalid first byte only matches itself; a valid code point is reached from its ASCII cases if it has them,
	// and from any code point past ASCII that folds the same

	if (start.folded >= 0x110000) {
		start.lower = start.upper = o->ptr[0];
		return start;
	}

	start.pastAscii = true;

	if (start.folded < 0x80)
		C8_caseVariants((C8) start.folded, EStringCase_InsensitiveAscii, &start.lower, &start.upper);

	else start.lower = start.upper = o->ptr[0];

	return start;
}

//A byte compare or two settles almost every position; only a lead byte past ASCII is folded and compared, before a
// whole match is tried

__forceinline__ static Bool CharString_canStartMatch(const CharStringMatchStart *start, const C8 *s, U64 i, U64 end) {

	const C8 c = s[i];

	if (c == start->lower || c == start->upper)
		return true;

	if (!start->pastAscii || (U8) c < 0xC2)
		return false;

	return UnicodeCodePoint_readFolded(s, &i, end) == start->folded;
}

//Kept out of CharString_compare so the byte loop there stays tight

static ECompareResult CharString_compareFolded(const C8 *a, U64 al, const C8 *b, U64 bl) {

	U64 i = 0, j = 0;

	while (i < al && j < bl) {

		const U8 x = (U8) a[i], y = (U8) b[j];

		if (x == y) {

			const U64 same = CharString_sameFolded(a + i, al - i, b + j, bl - j);

			if (same) {
				i += same;
				j += same;
				continue;
			}
		}

		if ((x | y) < 0x80) {        //Two ASCII bytes that differ, maybe only in case

			const U8 lx = (U8) C8_toLower((C8) x), ly = (U8) C8_toLower((C8) y);

			if (lx != ly)
				return lx < ly ? ECompareResult_Lt : ECompareResult_Gt;

			++i;
			++j;
			continue;
		}

		const I32 order = CharString_stepFolded(a, &i, al, b, &j, bl);

		if (order)
			return order < 0 ? ECompareResult_Lt : ECompareResult_Gt;
	}

	return i < al ? ECompareResult_Gt : (j < bl ? ECompareResult_Lt : ECompareResult_Eq);
}

ECompareResult CharString_compare(const CharString *a, const CharString *b, EStringCase caseSensitive) {

	const U64 al = !a ? 0 : CharString_length(*a);
	const U64 bl = !b ? 0 : CharString_length(*b);

	//We want to sort on contents
	//Provided it's the same level of parenting.
	//This ensures things with the same parent also stay at the same location

	if (caseSensitive == EStringCase_Insensitive)
		return CharString_compareFolded(a ? a->ptr : NULL, al, b ? b->ptr : NULL, bl);

	//Only a mismatch asks whether A-Z fold

	for (U64 i = 0; i < al && i < bl; ++i) {

		U8 ai = (U8) a->ptr[i], bi = (U8) b->ptr[i];

		if (ai == bi)
			continue;

		if (caseSensitive == EStringCase_InsensitiveAscii) {

			ai = (U8) C8_toLower((C8) ai);
			bi = (U8) C8_toLower((C8) bi);

			if (ai == bi)
				continue;
		}

		return ai < bi ? ECompareResult_Lt : ECompareResult_Gt;
	}

	//If they start with the same thing, we want to sort on length

	if (al < bl)
		return ECompareResult_Lt;

	if (al > bl)
		return ECompareResult_Gt;

	return ECompareResult_Eq;
}

Bool CharString_startsWithString(const CharStringSensOff *strSensOff, const CharString *other) {

	if (!other || !strSensOff || !strSensOff->str)
		return false;

	const CharString *str = strSensOff->str;
	const U64 off = strSensOff->off;
	const U64 strl = CharString_length(*str);

	if(off > strl)
		return false;

	return CharString_matchAt(
		str->ptr, off, strl, other->ptr, CharString_length(*other), strSensOff->caseSensitive
	) != U64_MAX;
}

Bool CharString_endsWithString(const CharStringSensOff *strSensOff, const CharString *other) {

	if (!other || !strSensOff || !strSensOff->str)
		return false;

	const CharString *str = strSensOff->str;
	const U64 off = strSensOff->off;
	const EStringCase caseSensitive = strSensOff->caseSensitive;

	const U64 otherl = CharString_length(*other);
	const U64 strl = CharString_length(*str);

	if(off > strl)
		return false;

	const U64 end = strl - off;

	if (caseSensitive != EStringCase_Insensitive)
		return otherl <= end &&
			CharString_matchAt(str->ptr, end - otherl, end, other->ptr, otherl, caseSensitive) != U64_MAX;

	//Folding can change how many bytes match, so walk back from both ends

	U64 i = end, k = otherl;

	while (k) {

		if (!i)
			return false;

		if (CharString_prevFolded(str->ptr, &i) != CharString_prevFolded(other->ptr, &k))
			return false;
	}

	return true;
}

U64 CharString_countAll(const CharStringSensOff *strSensOff, C8 c) {

	if (!strSensOff || !strSensOff->str)
		return 0;

	const C8 *sPtr = strSensOff->str->ptr;
	const U64 slen = CharString_length(*strSensOff->str);

	C8 lower = 0, upper = 0;
	C8_caseVariants(c, strSensOff->caseSensitive, &lower, &upper);

	U64 count = 0;

	for (U64 i = strSensOff->off; i < slen; ++i)
		if (sPtr[i] == lower || sPtr[i] == upper)
			++count;

	return count;
}

U64 CharString_countAllString(const CharStringSensOff *strSensOff, const CharString *other) {

	if (!strSensOff || !strSensOff->str || !other)
		return 0;

	const C8 *sPtr = strSensOff->str->ptr;
	const U64 strl = CharString_length(*strSensOff->str);
	const U64 otherl = CharString_length(*other);

	if(!otherl)
		return 0;

	const CharStringMatchStart start = CharString_matchStart(other, strSensOff->caseSensitive);
	U64 count = 0;

	for (U64 i = strSensOff->off; i < strl; ) {

		if (!CharString_canStartMatch(&start, sPtr, i, strl)) {
			++i;
			continue;
		}

		const U64 matched = CharString_matchAt(sPtr, i, strl, other->ptr, otherl, strSensOff->caseSensitive);

		if (matched == U64_MAX) {
			++i;
			continue;
		}

		i += matched;
		++count;
	}

	return count;
}

U64 CharString_findFirst(const CharStringSensOffLen *strSensOffLen, C8 c) {

	if (!strSensOffLen || !strSensOffLen->str)
		return U64_MAX;

	const U64 off = strSensOffLen->off;
	U64 len = strSensOffLen->len;

	const C8 *sPtr = strSensOffLen->str->ptr;
	const U64 strl = CharString_length(*strSensOffLen->str);

	C8 lower = 0, upper = 0;
	C8_caseVariants(c, strSensOffLen->caseSensitive, &lower, &upper);

	if(off >= strl || len > strl - off)
		return U64_MAX;

	if(!len)
		len = strl - off;

	for (U64 i = off; i < off + len; ++i)
		if (sPtr[i] == lower || sPtr[i] == upper)
			return i;

	return U64_MAX;
}

U64 CharString_findLast(const CharStringSensOffLen *strSensOffLen, C8 c) {

	if (!strSensOffLen || !strSensOffLen->str)
		return U64_MAX;

	const U64 off = strSensOffLen->off;
	U64 len = strSensOffLen->len;

	const C8 *sPtr = strSensOffLen->str->ptr;
	const U64 strl = CharString_length(*strSensOffLen->str);

	C8 lower = 0, upper = 0;
	C8_caseVariants(c, strSensOffLen->caseSensitive, &lower, &upper);

	if(off >= strl || len > strl - off)
		return U64_MAX;

	if(!len)
		len = strl - off;

	for (U64 i = (off + len) - 1; i != U64_MAX && i >= off; --i)
		if (sPtr[i] == lower || sPtr[i] == upper)
			return i;

	return U64_MAX;
}

U64 CharString_findFirstStringMatch(const CharStringSensOffLen *strSensOffLen, const CharString *other, U64 *matchLen) {

	if (!strSensOffLen || !strSensOffLen->str || !other)
		return U64_MAX;

	const U64 off = strSensOffLen->off;
	const U64 len = strSensOffLen->len;

	const C8 *sPtr = strSensOffLen->str->ptr;

	const U64 otherl = CharString_length(*other);
	const U64 strl = CharString_length(*strSensOffLen->str);

	if(!otherl || off >= strl || len > strl - off)
		return U64_MAX;

	const U64 end = len ? off + len : strl;
	const CharStringMatchStart start = CharString_matchStart(other, strSensOffLen->caseSensitive);

	for (U64 i = off; i < end; ++i) {

		if (!CharString_canStartMatch(&start, sPtr, i, end))
			continue;

		const U64 matched = CharString_matchAt(sPtr, i, end, other->ptr, otherl, strSensOffLen->caseSensitive);

		if (matched == U64_MAX)
			continue;

		if (matchLen)
			*matchLen = matched;

		return i;
	}

	return U64_MAX;
}

U64 CharString_findLastStringMatch(const CharStringSensOffLen *strSensOffLen, const CharString *other, U64 *matchLen) {

	if (!strSensOffLen || !strSensOffLen->str || !other)
		return U64_MAX;

	const U64 off = strSensOffLen->off;
	const U64 len = strSensOffLen->len;

	const C8 *sPtr = strSensOffLen->str->ptr;

	const U64 otherl = CharString_length(*other);
	const U64 strl = CharString_length(*strSensOffLen->str);

	if(!otherl || off >= strl || len > strl - off)
		return U64_MAX;

	const U64 end = len ? off + len : strl;
	const CharStringMatchStart start = CharString_matchStart(other, strSensOffLen->caseSensitive);

	for (U64 i = end - 1; i != U64_MAX && i >= off; --i) {

		if (!CharString_canStartMatch(&start, sPtr, i, end))
			continue;

		const U64 matched = CharString_matchAt(sPtr, i, end, other->ptr, otherl, strSensOffLen->caseSensitive);

		if (matched == U64_MAX)
			continue;

		if (matchLen)
			*matchLen = matched;

		return i;
	}

	return U64_MAX;
}

U64 CharString_findFirstString(const CharStringSensOffLen *strSensOffLen, const CharString *other) {
	return CharString_findFirstStringMatch(strSensOffLen, other, NULL);
}

U64 CharString_findLastString(const CharStringSensOffLen *strSensOffLen, const CharString *other) {
	return CharString_findLastStringMatch(strSensOffLen, other, NULL);
}

Bool CharString_equalsString(const CharString *s, const CharString *other, EStringCase caseSensitive) {

	if (!s || !other)
		return false;

	const U64 strl = CharString_length(*s);
	const U64 otherl = CharString_length(*other);

	//Folding can change a code point's byte length (U+212A Kelvin sign is 3 bytes, k is 1), so strings of different
	// lengths can still be equal. Only ASCII strings of one length skip the folded compare.

	if (caseSensitive == EStringCase_Insensitive) {

		if (strl == otherl) {

			U64 i = 0;

			for (; i < strl && ((U8) s->ptr[i] | (U8) other->ptr[i]) < 0x80; ++i)
				if (!C8_matches(s->ptr[i], other->ptr[i], EStringCase_InsensitiveAscii))
					return false;

			if (i == strl)
				return true;
		}

		return CharString_compare(s, other, caseSensitive) == ECompareResult_Eq;
	}

	if (strl != otherl)
		return false;

	if (caseSensitive == EStringCase_Sensitive)
		return Buffer_eq(CharString_bufferConst(*s), CharString_bufferConst(*other));

	for (U64 i = 0; i < strl; ++i)
		if (!C8_matches(s->ptr[i], other->ptr[i], caseSensitive))
			return false;

	return true;
}

Bool CharString_equalsStringIgnoreEol(const CharString *s, const CharString *other, EStringCase caseSensitive) {

	if (!s || !other)
		return false;

	const U64 na = CharString_length(*s), nb = CharString_length(*other);
	U64 i = 0, j = 0;

	while (i < na && j < nb) {

		if(s->ptr[i] == '\r' && i + 1 < na && s->ptr[i + 1] == '\n')
			++i;

		if(other->ptr[j] == '\r' && j + 1 < nb && other->ptr[j + 1] == '\n')
			++j;

		if (caseSensitive == EStringCase_Insensitive) {

			if (UnicodeCodePoint_readFolded(s->ptr, &i, na) != UnicodeCodePoint_readFolded(other->ptr, &j, nb))
				return false;

			continue;
		}

		if(!C8_matches(s->ptr[i], other->ptr[j], caseSensitive))
			return false;

		++i;
		++j;
	}

	return i == na && j == nb;
}

Bool CharString_equalsCString(const CharString *s, const C8 *literal, EStringCase caseSensitive) {

	CharString lit = CharString_createNull();

	if(literal)
		lit = CharString_createRefCStrConst(literal);

	return CharString_equalsString(s, &lit, caseSensitive);
}

Bool CharString_equals(const CharString s, C8 c, EStringCase caseSensitive) {
	return CharString_length(s) == 1 && s.ptr && C8_matches(s.ptr[0], c, caseSensitive);
}

static inline Bool CharString_offsetAsRef(CharString s, U64 off, CharString *result, Error *e_rr) {

	Bool s_uccess = true;

	if (!result)
		retError(clean, Error_nullPointer(2, "CharString_offsetAsRef()::result is required"));

	if (CharString_isEmpty(s)) {
		*result = CharString_createNull();
		goto clean;
	}

	const U64 strl = CharString_length(s);

	if(off >= strl)
		retError(clean, Error_outOfBounds(1, off, strl, "CharString_offsetAsRef()::off is out of bounds"));

	*result = (CharString) {
		.ptr = s.ptr + off,
		.lenAndNullTerminated = (strl - off) | ((U64) CharString_isNullTerminated(s) << 63),
		.capacityAndRefInfo = CharString_isConstRef(s) ? U64_MAX : 0
	};

clean:
	return s_uccess;
}

Bool CharString_parseNyto(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0n");

	const U64 prepend = CharString_startsWithStringInsensitiveAscii(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || strl > 11)
		return false;

	U64 value = 0;

	for (U64 i = strl - 1, j = 1; i != U64_MAX; --i, j <<= 6) {

		const U8 v = C8_nyto(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64) 1 << (10 * 6)) && v >= (1 << 4))        //We have 4 bits left
			return false;

		value |= j * v;
	}

	*result = value;
	return true;
}

Bool CharString_parseHex(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0x");
	const U64 prepend = CharString_startsWithStringInsensitiveAscii(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || strl > 16)
		return false;

	U64 value = 0;

	for (U64 i = strl - 1, j = 0; i != U64_MAX; --i, j += 4) {

		const U8 v = C8_hex(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		value |= (U64) v << j;
	}

	*result = value;
	return true;
}

Bool CharString_parseOct(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0o");
	const U64 prepend = CharString_startsWithStringInsensitiveAscii(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	if (!result || !s.ptr || CharString_length(s) > 22)
		return false;

	U64 value = 0;

	for (U64 i = CharString_length(s) - 1, j = 0; i != U64_MAX; --i, j += 3) {

		U8 v = C8_oct(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == 63 && v > 1)
			return false;

		value |= (U64) v << j;
	}

	*result = value;
	return true;
}

Bool CharString_parseBin(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0b");
	const U64 prepend = CharString_startsWithStringInsensitiveAscii(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	if (!result || !s.ptr || CharString_length(s) > 64)
		return false;

	U64 value = 0;

	for (U64 i = CharString_length(s) - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

		const U8 v = C8_bin(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(v)
			value |= j;
	}

	*result = value;
	return true;
}

Bool CharString_parseDec(const CharString s, U64 *result) {

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || !strl || strl > 20)
		return false;

	U64 value = 0;

	for (U64 i = 0; i < strl; ++i) {

		const U8 v = C8_dec(s.ptr[i]);

		if (v == U8_MAX || value > (U64_MAX - v) / 10)
			return false;

		value = value * 10 + v;
	}

	*result = value;
	return true;
}

Bool CharString_parseDecSigned(CharString s, I64 *result) {

	if(!result)
		return false;

	const Bool neg = CharString_startsWithSensitive(s, '-', 0);

	if (!CharString_offsetAsRef(s, neg || CharString_startsWithSensitive(s, '+', 0), &s, NULL))
		return false;

	U64 value = 0;

	if(!CharString_parseDec(s, &value))
		return false;

	if(value > (U64) I64_MAX + neg)
		return false;

	*result = neg ? (value == (U64) I64_MAX + 1 ? I64_MIN : -(I64) value) : (I64) value;
	return true;
}

//Normalize decimal syntax to a signed integer significand and exponent, without a locale decimal separator.
//A binary64 rounding boundary is k * 2^q with k < 2^54 and q >= -1075.
//Its exact decimal has at most 768 significant digits, and its last nonzero digit is 5 unless it is an integer.
//At 768 digits every boundary ends in 0 or 5, since integer boundaries have at most 309 digits.
//A discarded nonzero tail can change rounding only when the retained prefix equals a boundary.
//Increasing a final 0 or 5 by one selects the same side without crossing another boundary.
//This keeps the conversion within runtimes that retain at most 768 decimal digits.

#define _CHARSTRING_DECIMAL_DIGITS 768

//Allow two signs, the exponent marker, five exponent digits and the null terminator.

#define _CHARSTRING_DECIMAL_TEXT_SIZE (_CHARSTRING_DECIMAL_DIGITS + 9)

static Bool CharString_decimalText(CharString s, C8 *text) {

	const U64 length = CharString_length(s);

	if(!s.ptr || !length)
		return false;

	U64 i = 0;
	const Bool negative = s.ptr[0] == '-';

	if(negative || s.ptr[0] == '+')
		++i;

	U64 significant = 0, fractional = 0;
	U16 stored = 0;
	Bool hasDigit = false, fraction = false, sticky = false;

	for(; i < length; ++i) {

		const C8 c = s.ptr[i];

		if(c == '.' && !fraction) {
			fraction = true;
			continue;
		}

		if(!C8_isDec(c))
			break;

		hasDigit = true;
		fractional += fraction;

		if(c != '0' || significant) {

			++significant;

			if(stored < _CHARSTRING_DECIMAL_DIGITS) {

				if(text)
					text[negative + stored] = c;

				++stored;
			}

			else sticky |= c != '0';
		}
	}

	if(!hasDigit)
		return false;

	U64 exponent = 0;
	Bool exponentNegative = false;

	if(i < length && (s.ptr[i] == 'e' || s.ptr[i] == 'E')) {

		++i;

		if(i < length && (s.ptr[i] == '-' || s.ptr[i] == '+'))
			exponentNegative = s.ptr[i++] == '-';

		const U64 begin = i;

		for(; i < length && C8_isDec(s.ptr[i]); ++i) {
			const U8 digit = C8_dec(s.ptr[i]);

			exponent = exponent > (U64_MAX - digit) / 10 ? U64_MAX : exponent * 10 + digit;
		}

		if(i == begin)
			return false;
	}

	if(i < length && (s.ptr[i] == 'f' || s.ptr[i] == 'F'))
		++i;

	if(i != length)
		return false;

	if(!text)
		return true;

	if(negative)
		text[0] = '-';

	if(!significant) {

		text[negative] = '0';
		text[negative + 1] = 0;
		return true;
	}

	if(sticky && (text[negative + stored - 1] == '0' || text[negative + stored - 1] == '5'))
		++text[negative + stored - 1];

	const I64 shift = (I64) (significant - stored) - (I64) fractional;
	const Bool shiftNegative = shift < 0;
	const U64 magnitude = (U64) (shiftNegative ? -shift : shift);

	if(shiftNegative == exponentNegative)
		exponent = magnitude > 10000 || exponent > 10000 ? 10000 : magnitude + exponent;

	else if(magnitude > exponent) {
		exponent = magnitude - exponent;
		exponentNegative = shiftNegative;
	}

	else exponent -= magnitude;

	//Exponents beyond this range cannot represent a nonzero finite binary64 with this bounded significand.

	if(exponent > 10000)
		exponent = 10000;

	U16 at = negative + stored;

	text[at++] = 'e';

	if(exponentNegative)
		text[at++] = '-';

	C8 digits[5];
	U8 count = 0;

	do {
		digits[count++] = (C8) ('0' + exponent % 10);
		exponent /= 10;
	} while(exponent);

	while(count)
		text[at++] = digits[--count];

	text[at] = 0;
	return true;
}

Bool CharString_parseDouble(CharString s, F64 *result) {

	C8 text[_CHARSTRING_DECIMAL_TEXT_SIZE];

	if(!result || !CharString_decimalText(s, text))
		return false;

	const F64 value = strtod(text, NULL);

	if(!F64_isValid(value))
		return false;

	*result = value;
	return true;
}

Bool CharString_parseFloat(const CharString s, F32 *result) {

	C8 text[_CHARSTRING_DECIMAL_TEXT_SIZE];

	if(!result || !CharString_decimalText(s, text))
		return false;

	const F32 value = strtof(text, NULL);

	if(!F32_isValid(value))
		return false;

	*result = value;
	return true;
}

Bool CharString_parseU64(const CharString s, U64 *result) {

	if (CharString_startsWithSensitive(s, '0', 0))
		switch (CharString_getAt(s, 1)) {

			//0 prefix is also octal.
			//For clarity, we even pass 08 and 09, so they can error.
			//Otherwise it's inconsistent behavior

			case '0':    case '1':    case '2':    case '3':    case '4':
			case '5':    case '6':    case '7':    case '8':    case '9':
			case 'O':    case 'o':
				return CharString_parseOct(s, result);

			case 'B':    case 'b':    return CharString_parseBin(s, result);
			case 'X':    case 'x':    return CharString_parseHex(s, result);
			case 'N':    case 'n':    return CharString_parseNyto(s, result);
		}

	return CharString_parseDec(s, result);
}

Bool CharString_isFloat(const CharString s) {
	return CharString_decimalText(s, NULL);
}

Bool CharString_cut(const CharString *s, U64 offset, U64 length, CharString *result) {

	if(!s || !result || (result->ptr && !CharString_isRef(*result)))
		return false;

	const U64 strl = CharString_length(*s);

	if (!strl && !offset && !length) {
		*result = CharString_createNull();
		return true;
	}

	if(offset >= strl)
		return false;

	if(!length)
		length = strl - offset;

	if (length > strl - offset)
		return false;

	if (offset == strl) {
		*result = CharString_createNull();
		return false;
	}

	const Bool isNullTerm = CharString_isNullTerminated(*s) && offset + length == strl;

	*result = CharString_isConstRef(*s) ? CharString_createRefSizedConst(s->ptr + offset, length, isNullTerm) :
		CharString_createRefSized(s->ptrNonConst + offset, length, isNullTerm);

	return true;
}

Bool CharString_cutAfter(const CharStringCut *cut, C8 c, Bool isFirst) {

	if (!cut || !cut->s || !cut->result || (cut->result->ptr && !CharString_isRef(*cut->result)))
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	const U64 found = isFirst ? CharString_findFirst(&strSensOffLen, c) : CharString_findLast(&strSensOffLen, c);

	if (found == U64_MAX)
		return false;

	if(!found) {
		*cut->result = CharString_createNull();
		return true;
	}

	return CharString_cut(cut->s, 0, found, cut->result);
}

Bool CharString_cutAfterString(const CharStringCut *cut, const CharString *other, Bool isFirst) {

	if (!cut || !cut->s || !cut->result || (cut->result->ptr && !CharString_isRef(*cut->result)))
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	const U64 found =
		isFirst ? CharString_findFirstString(&strSensOffLen, other) : CharString_findLastString(&strSensOffLen, other);

	if (found == U64_MAX)
		return false;

	if(!found) {
		*cut->result = CharString_createNull();
		return true;
	}

	return CharString_cut(cut->s, 0, found, cut->result);
}

Bool CharString_cutBefore(const CharStringCut *cut, C8 c, Bool isFirst) {

	if (!cut || !cut->s)
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	U64 found = isFirst ? CharString_findFirst(&strSensOffLen, c) : CharString_findLast(&strSensOffLen, c);

	if (found == U64_MAX)
		return false;

	++found;    //The end of the occurence is the begin of the next string
	return CharString_cut(cut->s, found, 0, cut->result);
}

Bool CharString_cutBeforeString(const CharStringCut *cut, const CharString *other, Bool isFirst) {

	if (!cut || !other)
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	U64 matchLen = 0;

	U64 found = isFirst ? CharString_findFirstStringMatch(&strSensOffLen, other, &matchLen) :
		CharString_findLastStringMatch(&strSensOffLen, other, &matchLen);

	if (found == U64_MAX)
		return false;

	found += matchLen;    //The end of the occurence is the begin of the next string
	return CharString_cut(cut->s, found, 0, cut->result);
}
