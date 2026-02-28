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

#include "types/base/string_read_helper.h"
#include "types/base/algorithm.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"

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

		if(
			CharString_startsWithStringInsensitive(&str, &CON, 0) ||
			CharString_startsWithStringInsensitive(&str, &AUX, 0) ||
			CharString_startsWithStringInsensitive(&str, &NUL, 0) ||
			CharString_startsWithStringInsensitive(&str, &PRN, 0)
		)
			illegalStart = 3;

		else if (strl >= 4) {

			if((CharString_startsWithStringInsensitive(&str, &COM, 0) || CharString_startsWithStringInsensitive(&str, &LPT, 0))
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

	//If string is not empty then it's a valid string

	return strl;
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

	//myTest/ <-- or myTest\ to myTest

	str = CharString_createRefStrConst(str);

	if(CharString_getAt(str, CharString_length(str) - 1) == '/' || CharString_getAt(str, CharString_length(str) - 1) == '\\')
		str.lenAndNullTerminated = CharString_length(str) - 1;

	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)
	//We also obviously don't support 0:\ and such or A:/ on unix

	Bool hasPrefix = false;

	#ifdef _WIN32

		if(
			CharString_length(str) >= 3 &&
			str.ptr[1] == ':' && ((str.ptr[2] != '/' && str.ptr[2] != '\\') || !C8_isAlpha(str.ptr[0]))
		)
			return false;

		//Absolute

		if(CharString_length(str) >= 2 && str.ptr[1] == ':') {
			str.ptr += 3;
			str.lenAndNullTerminated -= 3;
			hasPrefix = true;
		}

	#else

		if(CharString_length(str) >= 2 && str.ptr[1] == ':')
			return false;

	#endif

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

ECompareResult CharString_compare(const CharString *a, const CharString *b, EStringCase caseSensitive) {

	const U64 al = !a ? 0 : CharString_length(*a);
	const U64 bl = !b ? 0 : CharString_length(*b);

	//We want to sort on contents
	//Provided it's the same level of parenting.
	//This ensures things with the same parent also stay at the same location

	for (U64 i = 0; i < al && i < bl; ++i) {

		//TODO: sensitivity for unicode?

		const C8 ai = C8_transform(a->ptr[i], (EStringTransform) caseSensitive);
		const C8 bi = C8_transform(b->ptr[i], (EStringTransform) caseSensitive);

		if (ai < bi)
			return ECompareResult_Lt;

		if (ai > bi)
			return ECompareResult_Gt;
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
	const EStringTransform transform = (EStringTransform) strSensOff->caseSensitive;

	const U64 otherl = CharString_length(*other);
	U64 strl = CharString_length(*str);

	if(off > strl)
		return false;

	strl -= off;

	if(!otherl)
		return true;

	if (otherl > strl)
		return false;

	for (U64 i = off; i < off + otherl; ++i)
		if (C8_transform(str->ptr[i], transform) != C8_transform(other->ptr[i - off], transform))
			return false;

	return true;
}

Bool CharString_endsWithString(const CharStringSensOff *strSensOff, const CharString *other) {

	if (!other || !strSensOff || !strSensOff->str)
		return false;

	const CharString *str = strSensOff->str;
	const U64 off = strSensOff->off;
	const EStringTransform transform = (EStringTransform)strSensOff->caseSensitive;

	const U64 otherl = CharString_length(*other);
	U64 strl = CharString_length(*str);

	if(off > strl)
		return false;

	strl -= off;

	if(!otherl)
		return true;

	if (otherl > strl)
		return false;

	for (U64 i = strl - otherl; i < strl; ++i)
		if (C8_transform(str->ptr[i], transform) != C8_transform(other->ptr[i - (strl - otherl)], transform))
			return false;

	return true;
}

U64 CharString_countAll(const CharStringSensOff *strSensOff, C8 c) {

	if (!strSensOff || !strSensOff->str)
		return 0;

	const C8 *sPtr = strSensOff->str->ptr;
	const U64 slen = CharString_length(*strSensOff->str);
	const EStringTransform transform = (EStringTransform)strSensOff->caseSensitive;

	c = C8_transform(c, transform);

	U64 count = 0;

	for (U64 i = strSensOff->off; i < slen; ++i)
		if (C8_transform(sPtr[i], transform) == c)
			++count;

	return count;
}

U64 CharString_countAllString(const CharStringSensOff *strSensOff, const CharString *other) {

	if (!strSensOff || !strSensOff->str || !other)
		return 0;

	const U64 off = strSensOff->off;
	const EStringTransform transform = (EStringTransform)strSensOff->caseSensitive;

	const C8 *sPtr = strSensOff->str->ptr;
	const U64 strl = CharString_length(*strSensOff->str);

	const U64 otherl = CharString_length(*other);

	if(!otherl || strl < otherl)
		return 0;

	U64 j = 0;

	for (U64 i = off; i < strl - otherl + 1; ++i) {

		Bool match = true;

		for (U64 l = i, k = 0; l < strl && k < otherl; ++l, ++k)
			if (C8_transform(sPtr[l], transform) != C8_transform(other->ptr[k], transform)) {
				match = false;
				break;
			}

		if (match) {
			i += otherl - 1;
			++j;
		}
	}

	return j;
}

U64 CharString_findFirst(const CharStringSensOffLen *strSensOffLen, C8 c) {

	if (!strSensOffLen || !strSensOffLen->str)
		return U64_MAX;

	const U64 off = strSensOffLen->off;
	U64 len = strSensOffLen->len;
	const EStringTransform transform = (EStringTransform) strSensOffLen->caseSensitive;

	const C8 *sPtr = strSensOffLen->str->ptr;
	const U64 strl = CharString_length(*strSensOffLen->str);

	c = C8_transform(c, transform);

	if(off >= strl || off + len > strl)
		return U64_MAX;

	if(!len)
		len = strl - off;

	for (U64 i = off; i < off + len; ++i)
		if (C8_transform(sPtr[i], transform) == c)
			return i;

	return U64_MAX;
}

U64 CharString_findLast(const CharStringSensOffLen *strSensOffLen, C8 c) {
	
	if (!strSensOffLen || !strSensOffLen->str)
		return U64_MAX;

	const U64 off = strSensOffLen->off;
	U64 len = strSensOffLen->len;
	const EStringTransform transform = (EStringTransform) strSensOffLen->caseSensitive;

	const C8 *sPtr = strSensOffLen->str->ptr;
	const U64 strl = CharString_length(*strSensOffLen->str);

	c = C8_transform(c, transform);

	if(off >= strl || off + len > strl)
		return U64_MAX;

	if(!len)
		len = strl - off;

	for (U64 i = (off + len) - 1; i != U64_MAX && i >= off; --i)
		if (C8_transform(sPtr[i], transform) == c)
			return i;

	return U64_MAX;
}

U64 CharString_findFirstString(const CharStringSensOffLen *strSensOffLen, const CharString *other) {

	if (!strSensOffLen || !strSensOffLen->str || !other)
		return U64_MAX;

	const U64 off = strSensOffLen->off;
	const U64 len = strSensOffLen->len;
	const EStringTransform transform = (EStringTransform) strSensOffLen->caseSensitive;

	const C8 *sPtr = strSensOffLen->str->ptr;

	const U64 otherl = CharString_length(*other);
	U64 strl = CharString_length(*strSensOffLen->str);

	if(!otherl || strl < otherl)
		return U64_MAX;

	if(off >= strl || off + len > strl)
		return U64_MAX;

	if(len)
		strl = off + len;

	U64 i = off;

	for (; i < strl; ++i) {

		Bool match = true;
		U64 k = 0;

		for (U64 j = i; j < strl && k < otherl; ++j, ++k)
			if (C8_transform(sPtr[j], transform) != C8_transform(other->ptr[k], transform)) {
				match = false;
				break;
			}

		if (match && k == otherl)
			break;
	}

	return i >= strl ? U64_MAX : i;
}

U64 CharString_findLastString(const CharStringSensOffLen *strSensOffLen, const CharString *other) {

	if (!strSensOffLen || !strSensOffLen->str || !other)
		return U64_MAX;
	
	const U64 off = strSensOffLen->off;
	const U64 len = strSensOffLen->len;
	const EStringTransform transform = (EStringTransform) strSensOffLen->caseSensitive;

	const C8 *sPtr = strSensOffLen->str->ptr;

	const U64 otherl = CharString_length(*other);
	U64 strl = CharString_length(*strSensOffLen->str);

	if(!otherl || strl < otherl)
		return U64_MAX;

	if(off >= strl || off + len > strl)
		return U64_MAX;

	if(len)
		strl = off + len;

	U64 i = strl - 1;

	for (; i != U64_MAX && i >= off; --i) {

		Bool match = true;

		for (U64 j = i, k = otherl - 1; j != U64_MAX && k != U64_MAX; --j, --k)
			if (C8_transform(sPtr[j], transform) != C8_transform(other->ptr[k], transform)) {
				match = false;
				break;
			}

		if (match) {
			i -= otherl - 1;
			break;
		}
	}

	return i;
}

Bool CharString_equalsString(const CharString *s, const CharString *other, EStringCase caseSensitive) {

	if (!s || !other)
		return false;

	const U64 strl = CharString_length(*s);
	const U64 otherl = CharString_length(*other);

	if (strl != otherl)
		return false;

	if (caseSensitive == EStringCase_Sensitive)
		return Buffer_eq(CharString_bufferConst(*s), CharString_bufferConst(*other));

	for (U64 i = 0; i < strl; ++i)
		if (C8_toLower(s->ptr[i]) != C8_toLower(other->ptr[i]))
			return false;

	return true;
}

Bool CharString_equalsCString(const CharString *s, const C8 *literal, EStringCase caseSensitive) {

	CharString lit = CharString_createNull();

	if(literal)
		lit = CharString_createRefCStrConst(literal);

	return CharString_equalsString(s, &lit, caseSensitive);
}

Bool CharString_equals(const CharString s, C8 c, EStringCase caseSensitive) {
	return CharString_length(s) == 1 && s.ptr &&
		C8_transform(s.ptr[0], (EStringTransform) caseSensitive) ==
		C8_transform(c, (EStringTransform) caseSensitive);
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
		.lenAndNullTerminated = (strl - off) | ((U64)CharString_isNullTerminated(s) << 63),
		.capacityAndRefInfo = CharString_isConstRef(s) ? U64_MAX : 0
	};

clean:
	return s_uccess;
}

Bool CharString_parseNyto(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0n");

	const U64 prepend = CharString_startsWithStringInsensitive(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || strl > 11)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 1; i != U64_MAX; --i, j <<= 6) {

		const U8 v = C8_nyto(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64)1 << (10 * 6)) && v >= (1 << 4))		//We have 4 bits left
			return false;

		*result |= j * v;
	}

	return true;
}

Bool CharString_parseHex(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0x");
	const U64 prepend = CharString_startsWithStringInsensitive(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || strl > 16)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 0; i != U64_MAX; --i, j += 4) {

		const U8 v = C8_hex(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		*result |= (U64)v << j;
	}

	return true;
}

Bool CharString_parseOct(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0o");
	const U64 prepend = CharString_startsWithStringInsensitive(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	if (!result || !s.ptr || CharString_length(s) > 22)
		return false;

	*result = 0;

	for (U64 i = CharString_length(s) - 1, j = 0; i != U64_MAX; --i, j += 3) {

		U8 v = C8_oct(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64)1 << (21 * 3)) && v > 1)		//Out of value
			return false;

		*result |= (U64)v << j;
	}

	return true;
}

Bool CharString_parseBin(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0b");
	const U64 prepend = CharString_startsWithStringInsensitive(&s, &prefix, 0) ? CharString_length(prefix) : 0;
	
	if (!CharString_offsetAsRef(s, prepend, &s, NULL))
		return false;

	if (!result || !s.ptr || CharString_length(s) > 64)
		return false;

	*result = 0;

	for (U64 i = CharString_length(s) - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

		const U8 v = C8_bin(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(v)
			*result |= j;
	}

	return true;
}

Bool CharString_parseDec(const CharString s, U64 *result) {

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || strl > 20)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 1; i != U64_MAX; --i, j *= 10) {

		const U8 v = C8_dec(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == (U64) 1e19 && v > 1)		//Out of value
			return false;

		*result += j * v;
	}

	return true;
}

Bool CharString_parseDecSigned(CharString s, I64 *result) {

	const Bool neg = CharString_startsWithSensitive(s, '-', 0);

	if (!CharString_offsetAsRef(s, neg, &s, NULL))
		return false;

	const Bool b = CharString_parseDec(s, (U64*) result);

	if(!b)
		return false;

	if (!neg && *result == I64_MIN)			//Int min is the same as 63 bit max + 1
		return true;

	if (*(U64*)result >> 63)				//Guard against int overflow
		return false;

	if(neg)
		*result *= -1;

	return true;
}

//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

Bool CharString_parseDouble(CharString s, F64 *result) {

	//TODO: Replace with a standard function because they probably do something fancier.

	if (!result)
		return false;

	//Parse sign

	Bool sign = false;

	if (CharString_startsWithSensitive(s, '-', 0)) {

		sign = true;

		if (!CharString_offsetAsRef(s, 1, &s, NULL))
			return false;
	}

	else if (CharString_startsWithSensitive(s, '+', 0)) {
		
		if (!CharString_offsetAsRef(s, 1, &s, NULL))
			return false;
	}

	//Parse integer part

	F64 intPart = 0;
	Bool hasDigit = false;

	while(
		!CharString_startsWithSensitive(s, '.', 0) &&
		!CharString_startsWithInsensitive(s, 'e', 0)
	) {

		const U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX) {

			if((s.ptr[0] == 'f' || s.ptr[0] == 'F') && hasDigit && CharString_length(s) == 1) {
				*result = sign ? -intPart : intPart;
				return true;
			}

			return false;
		}

		hasDigit = true;
		intPart = intPart * 10 + v;

		if(!F64_isValid(intPart))
			return false;

		if (!CharString_offsetAsRef(s, 1, &s, NULL))
			s = CharString_createNull();

		if (!CharString_length(s)) {
			*result = sign ? -intPart : intPart;
			return true;
		}
	}

	//Parse fraction

	if (CharString_startsWithSensitive(s, '.', 0) && CharString_length(s) == 1) {
		*result = sign ? -intPart : intPart;
		return true;
	}

	if(CharString_startsWithSensitive(s, '.', 0))
		CharString_offsetAsRef(s, 1, &s, NULL);

	F64 fraction = 0, multiplier = 0.1;

	while(!CharString_startsWithInsensitive(s, 'e', 0)) {

		const U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX) {

			if((s.ptr[0] == 'f' || s.ptr[0] == 'F') && hasDigit && CharString_length(s) == 1)  {
				*result = sign ? -(intPart + fraction) : intPart + fraction;
				return F64_isValid(*result);
			}

			return false;
		}

		hasDigit = true;
		fraction += v * multiplier;

		if(!F64_isValid(fraction))
			return false;

		multiplier *= 0.1;

		if (!CharString_offsetAsRef(s, 1, &s, NULL))
			s = CharString_createNull();

		if (!CharString_length(s)) {
			*result = sign ? -(intPart + fraction) : intPart + fraction;
			return F64_isValid(*result);
		}
	}

	//Parse exponent sign

	Bool nextE = CharString_startsWithInsensitive(s, 'e', 0);

	if (!CharString_offsetAsRef(s, 1, &s, NULL)) {

		if (!nextE) {
			*result = sign ? -(intPart + fraction) : intPart + fraction;
			return F64_isValid(*result);
		}

		return false;
	}

	Bool esign = false;

	if (CharString_startsWithSensitive(s, '-', 0)) {

		esign = true;

		if (!CharString_offsetAsRef(s, 1, &s, NULL))
			return false;
	}

	else if (CharString_startsWithSensitive(s, '+', 0)) {

		if (!CharString_offsetAsRef(s, 1, &s, NULL))
			return false;
	}

	//Parse exponent (must be int)

	F64 exponent = 0;

	while(CharString_length(s)) {

		const U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX) {

			if(s.ptr[0] == 'f' || s.ptr[0] == 'F') {

				exponent = F64_exp10(esign ? -exponent : exponent);

				if (!F64_isValid(exponent))
					return false;

				const F64 res = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;

				if(!F64_isValid(res))
					return false;

				*result = res;
				return hasDigit && CharString_length(s) == 1;
			}

			return false;
		}

		exponent = exponent * 10 + v;

		if(!F64_isValid(fraction))
			return false;

		if (!CharString_offsetAsRef(s, 1, &s, NULL))
			break;
	}

	exponent = F64_exp10(esign ? -exponent : exponent);

	if (!F64_isValid(exponent))
		return false;

	const F64 res = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;

	if(!F64_isValid(res))
		return false;

	*result = res;
	return true;
}

Bool CharString_parseFloat(const CharString s, F32 *result) {

	F64 dub = 0;
	if(!CharString_parseDouble(s, &dub))
		return false;

	if(!F32_isValid((F32)dub))
		return false;

	*result = (F32)dub;
	return true;
}

Bool CharString_parseU64(const CharString s, U64 *result) {

	if (CharString_startsWithSensitive(s, '0', 0))
		switch (CharString_getAt(s, 1)) {

			//0 prefix is also octal.
			//For clarity, we even pass 08 and 09, so they can error.
			//Otherwise it's inconsistent behavior

			case '0':	case '1':	case '2':	case '3':	case '4':
			case '5':	case '6':	case '7':	case '8':	case '9':
			case 'O':	case 'o':
				return CharString_parseOct(s, result);

			case 'B':	case 'b':	return CharString_parseBin(s, result);
			case 'X':	case 'x':	return CharString_parseHex(s, result);
			case 'N':	case 'n':	return CharString_parseNyto(s, result);
		}

	return CharString_parseDec(s, result);
}

Bool CharString_isFloat(const CharString s) {

	U64 strl = CharString_length(s);

	if(!strl || !s.ptr)
		return false;

	//Validate sign

	U64 i = 0;

	if (s.ptr[0] == '-' || s.ptr[0] == '+') {

		if (++i >= strl)
			return false;
	}

	//Validate first int

	for(; i < strl; ++i)

		if (s.ptr[i] == '.' || s.ptr[i] == 'e' || s.ptr[i] == 'E')
			break;

		else if(!C8_isDec(s.ptr[i]))
			return false;

	if (i == strl)				//It's just an int
		return true;

	//Validate fraction

	if (s.ptr[i] == '.') {

		if (++i == strl)		//It's just an int
			return true;

		//Check for int until e/E

		for(; i < strl; ++i)

			if (s.ptr[i] == 'e' || s.ptr[i] == 'E')
				break;

			else if(!C8_isDec(s.ptr[i]))
				return false;
	}

	if (i == strl)				//It's just [-+]?[0-9]*[.]?[0-9]*
		return true;

	//Validate exponent

	if (s.ptr[i] == 'E' || s.ptr[i] == 'e') {

		if (++i == strl)
			return false;

		//e-NNN, e+NNN

		if (s.ptr[i] == '-' || s.ptr[i] == '+') {

			if (++i == strl)
				return false;
		}

		for(; i < strl; ++i)
			if(!C8_isDec(s.ptr[i]))
				return false;
	}

	//It's just [-+]?[0-9]*[.]?[0-9]*[fF]
	if (i == strl || (i + 1 == strl && (s.ptr[i] == 'f' || s.ptr[i] == 'F')))
		return true;

	return false;
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

	if (offset + length > strl)
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

	if (!cut || !cut->s)
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	const U64 found = isFirst ? CharString_findFirst(&strSensOffLen, c) : CharString_findLast(&strSensOffLen, c);

	if (found == U64_MAX)
		return false;

	return CharString_cut(cut->s, 0, found, cut->result);
}

Bool CharString_cutAfterString(const CharStringCut *cut, const CharString *other, Bool isFirst) {

	if (!cut || !cut->s)
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	const U64 found =
		isFirst ? CharString_findFirstString(&strSensOffLen, other) : CharString_findLastString(&strSensOffLen, other);

	if (found == U64_MAX)
		return false;

	return CharString_cut(cut->s, 0, found, cut->result);
}

Bool CharString_cutBefore(const CharStringCut *cut, C8 c, Bool isFirst) {

	if (!cut || !cut->s)
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	U64 found = isFirst ? CharString_findFirst(&strSensOffLen, c) : CharString_findLast(&strSensOffLen, c);

	if (found == U64_MAX)
		return false;

	++found;	//The end of the occurence is the begin of the next string
	return CharString_cut(cut->s, found, 0, cut->result);
}

Bool CharString_cutBeforeString(const CharStringCut *cut, const CharString *other, Bool isFirst) {

	if (!cut || !other)
		return false;

	const CharStringSensOffLen strSensOffLen = { cut->s, cut->caseSensitive, 0, 0 };
	U64 found = isFirst ? CharString_findFirstString(&strSensOffLen, other) : CharString_findLastString(&strSensOffLen, other);

	if (found == U64_MAX)
		return false;

	found += CharString_length(*other);	//The end of the occurence is the begin of the next string
	return CharString_cut(cut->s, found, 0, cut->result);
}
