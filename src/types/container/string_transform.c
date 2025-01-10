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

#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/base/c8.h"

Bool CharString_setAt(CharString str, U64 i, C8 c) {

	if(i >= CharString_length(str) || CharString_isConstRef(str) || !c)
		return false;

	str.ptrNonConst[i] = c;
	return true;
}

Error CharString_append(CharString *s, C8 c, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, "CharString_append()::s is required");

	if(!c && CharString_isNullTerminated(*s))
		return Error_none();

	return CharString_resize(s, CharString_length(*s) + !!c, c, allocator);
}

Error CharString_appendString(CharString *s, CharString other, Allocator allocator) {

	const U64 otherl = CharString_length(other);

	if (!otherl)
		return Error_none();

	other = CharString_createRefStrConst(other);

	if (!s)
		return Error_nullPointer(0, "CharString_appendString()::s is required");

	const U64 oldLen = CharString_length(*s);

	if (CharString_isRef(*s) && oldLen)
		return Error_invalidParameter(0, 0, "CharString_appendString()::s has to be managed memory");

	const Error err = CharString_resize(s, oldLen + otherl, other.ptr[0], allocator);

	if (err.genericError)
		return err;

	Buffer_memcpy(Buffer_createRef((U8*)s->ptrNonConst + oldLen, otherl), CharString_bufferConst(other));
	return Error_none();
}

Error CharString_prepend(CharString *s, C8 c, Allocator allocator) {
	return CharString_insert(s, c, 0, allocator);
}

Error CharString_prependString(CharString *s, CharString other, Allocator allocator) {
	return CharString_insertString(s, other, 0, allocator);
}

Error CharString_insert(CharString *s, C8 c, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, "CharString_insert()::s is required");

	const U64 strl = CharString_length(*s);

	if (CharString_isRef(*s) && strl)
		return Error_invalidParameter(0, 0, "CharString_insert()::s should be managed memory");

	if(i > strl)
		return Error_outOfBounds(2, i, strl, "CharString_insert()::i is out of bounds");

	if(i == strl && !c && CharString_isNullTerminated(*s))
		return Error_none();

	if(!c && i != strl)
		return Error_invalidOperation(0, "CharString_insert()::c is 0, which isn't allowed if i != strl");

	const Error err = CharString_resize(s, strl + 1, c, allocator);

	if (err.genericError)
		return err;

	//If it's not append (otherwise it's already handled)

	if (i != strl - 1) {

		//Move one to the right

		Buffer_memmove(
			Buffer_createRef(s->ptrNonConst + i + 1,  strl),
			Buffer_createRef(s->ptrNonConst + i, strl)
		);

		s->ptrNonConst[i] = c;
	}

	return Error_none();
}

Error CharString_insertString(CharString *s, CharString other, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, "CharString_insertString()::s is required");

	const U64 oldLen = CharString_length(*s);

	if (CharString_isRef(*s) && oldLen)
		return Error_invalidParameter(0, 0, "CharString_insertString()::s should be managed memory");

	const U64 otherl = CharString_length(other);

	if (!otherl)
		return Error_none();

	const Error err = CharString_resize(s, oldLen + otherl, ' ', allocator);

	if (err.genericError)
		return err;

	//Move one to the right

	Buffer_memmove(
		Buffer_createRef(s->ptrNonConst + i + otherl, oldLen - i),
		Buffer_createRef(s->ptrNonConst + i, oldLen - i)
	);

	Buffer_memcpy(
		Buffer_createRef(s->ptrNonConst + i, otherl),
		CharString_bufferConst(other)
	);

	return Error_none();
}

Error CharString_replaceAllString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	U64 off,
	U64 len
) {

	if (!s)
		return Error_nullPointer(0, "CharString_replaceAllString()::s is required");

	if(CharString_isRef(*s))
		return Error_constData(0, 0, "CharString_replaceAllString()::s must be managed memory");

	ListU64 finds = { 0 };
	Error err = CharString_findAllString(*s, search, allocator, caseSensitive, off, len, &finds);

	if(err.genericError)
		return err;

	if (!finds.length)
		return Error_none();

	//Easy replace

	const U64 *ptr = finds.ptr;

	const U64 searchl = CharString_length(search);
	const U64 replacel = CharString_length(replace);
	const U64 strl = CharString_length(*s);

	if (searchl == replacel) {

		for (U64 i = 0; i < finds.length; ++i)
			for (U64 j = ptr[i], k = j + replacel, l = 0; j < k; ++j, ++l)
				s->ptrNonConst[j] = replace.ptr[l];

		goto clean;
	}

	//Shrink replaces

	if (searchl > replacel) {

		const U64 diff = searchl - replacel;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			//We move our replacement string to iCurrent

			Buffer_memcpy(
				Buffer_createRef(s->ptrNonConst + iCurrent, replacel),
				CharString_bufferConst(replace)
			);

			iCurrent += replacel;

			//We then move the tail of the string

			const U64 iStart = ptr[i] + searchl;
			const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

			Buffer_memmove(
				Buffer_createRef(s->ptrNonConst + iCurrent, iEnd - iStart),
				Buffer_createRef(s->ptrNonConst + iStart, iEnd - iStart)
			);

			iCurrent += iEnd - iStart;
		}

		//Ensure the string is now the right size

		err = CharString_resize(s, strl - diff * finds.length, ' ', allocator);
		goto clean;
	}

	//Grow replaces

	//Ensure the string is now the right size

	const U64 diff = replacel - searchl;

	gotoIfError(clean, CharString_resize(s, strl + diff * finds.length, ' ', allocator))

	//Move from right to left

	U64 newLoc = strl - 1;

	for (U64 i = finds.length - 1; i != U64_MAX; ++i) {

		//Find tail

		const U64 iStart = ptr[i] + searchl;
		const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

		for (U64 j = iEnd - 1; j != U64_MAX && j >= iStart; --j)
			((C8*)s->ptr)[newLoc--] = s->ptr[j];

		Buffer_memmove(
			Buffer_createRef(s->ptrNonConst + newLoc - (iEnd - iStart), iEnd - iStart),
			Buffer_createRef(s->ptrNonConst + iStart, iEnd - iStart)
		);

		newLoc -= iEnd - iStart;

		//Apply replacement before tail

		Buffer_memcpy(
			Buffer_createRef(s->ptrNonConst + newLoc - replacel, replacel),
			CharString_bufferConst(replace)
		);

		newLoc -= replacel;
	}

clean:
	ListU64_free(&finds, allocator);
	return err;
}

Error CharString_replaceString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	Bool isFirst,
	U64 off,
	U64 len
) {

	if (!s)
		return Error_nullPointer(0, "CharString_replaceString()::s is required");

	if(CharString_isRef(*s))
		return Error_constData(0, 0, "CharString_replaceString()::s must use managed memory");

	const U64 res = isFirst ? CharString_findFirstString(*s, search, caseSensitive, off, len) :
		CharString_findLastString(*s, search, caseSensitive, off, len);

	if (res == U64_MAX)
		return Error_none();

	const U64 searchl = CharString_length(search);
	const U64 replacel = CharString_length(replace);
	const U64 strl = CharString_length(*s);

	//Easy, early exit. Strings are same size.

	if (searchl == replacel) {
		Buffer_memcpy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));
		return Error_none();
	}

	//Replacement is smaller than our search
	//So we can just move from left to right

	if (replacel < searchl) {

		const U64 diff = searchl - replacel;	//How much we have to shrink

		//Copy our data over first

		Buffer_memmove(
			Buffer_createRef(s->ptrNonConst + res + replacel, strl - (res + searchl)),
			Buffer_createRef(s->ptrNonConst + res + searchl, strl - (res + searchl))
		);

		//Throw our replacement in there

		Buffer_memcpy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));

		//Shrink the string; this is done after because we need to read the right of the string first

		const Error err = CharString_resize(s, strl - diff, ' ', allocator);

		if (err.genericError)
			return err;

		return Error_none();
	}

	//Replacement is bigger than our search;
	//We need to grow first and move from right to left

	const U64 diff = replacel - searchl;

	const Error err = CharString_resize(s, strl + diff, ' ', allocator);

	if (err.genericError)
		return err;

	//Copy our data over first

	Buffer_memmove(
		Buffer_createRef(s->ptrNonConst + res + replacel, strl - (res + searchl)),
		Buffer_createRef(s->ptrNonConst + res + searchl, strl - (res + searchl))
	);

	//Throw our replacement in there

	Buffer_memcpy(Buffer_createRef(s->ptrNonConst + res, replacel), CharString_bufferConst(replace));
	return Error_none();
}


Bool CharString_cut(CharString s, U64 offset, U64 length, CharString *result) {

	if(!result || result->ptr)
		return false;

	const U64 strl = CharString_length(s);

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

	const Bool isNullTerm = CharString_isNullTerminated(s) && offset + length == strl;

	*result = CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr + offset, length, isNullTerm) :
		CharString_createRefSized(s.ptrNonConst + offset, length, isNullTerm);

	return true;
}

Bool CharString_cutAfter(
	CharString s,
	C8 c,
	EStringCase caseSensitive,
	Bool isFirst,
	CharString *result
) {

	const U64 found =
		isFirst ? CharString_findFirst(s, c, caseSensitive, 0, 0) :
		CharString_findLast(s, c, caseSensitive, 0, 0);

	if (found == U64_MAX)
		return false;

	return CharString_cut(s, 0, found, result);
}

Bool CharString_cutAfterString(
	CharString s,
	CharString other,
	EStringCase caseSensitive,
	Bool isFirst,
	CharString *result
) {

	const U64 found = isFirst ? CharString_findFirstString(s, other, caseSensitive, 0, 0) :
		CharString_findLastString(s, other, caseSensitive, 0, 0);

	if (found == U64_MAX)
		return false;

	return CharString_cut(s, 0, found, result);
}

Bool CharString_cutBefore(CharString s, C8 c, EStringCase caseSensitive, Bool isFirst, CharString *result) {

	if (!result)
		return false;

	U64 found =
		isFirst ? CharString_findFirst(s, c, caseSensitive, 0, 0) :
		CharString_findLast(s, c, caseSensitive, 0, 0);

	if (found == U64_MAX)
		return false;

	++found;	//The end of the occurence is the begin of the next string
	return CharString_cut(s, found, 0, result);
}

Bool CharString_cutBeforeString(CharString s, CharString other, EStringCase caseSensitive, Bool isFirst, CharString *result) {

	U64 found =
		isFirst ? CharString_findFirstString(s, other, caseSensitive, 0, 0) :
		CharString_findLastString(s, other, caseSensitive, 0, 0);

	if (found == U64_MAX)
		return false;

	found += CharString_length(other);	//The end of the occurence is the begin of the next string
	return CharString_cut(s, found, 0, result);
}

Bool CharString_erase(CharString *s, C8 c, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len) {

	if(!s || CharString_isRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	//Skipping first match

	const U64 find = CharString_find(*s, c, caseSensitive, isFirst, off, len);

	if(find == U64_MAX)
		return false;

	const U64 strl = CharString_length(*s);

	Buffer_memcpy(
		Buffer_createRef((U8*)s->ptr + find, strl - find - 1),
		Buffer_createRefConst(s->ptr + find + 1, strl - find - 1)
	);

	s->ptrNonConst[strl - 1] = '\0';
	s->lenAndNullTerminated = (strl - 1) | ((U64)1 << 63);

	return true;
}

Error CharString_eraseAtCount(CharString *s, U64 i, U64 count) {

	if(!s)
		return Error_nullPointer(0, "CharString_eraseAtCount()::s is required");

	if(!count)
		return Error_none();

	if(CharString_isRef(*s))
		return Error_constData(0, 0, "CharString_eraseAtCount()::s should be managed memory");

	const U64 strl = CharString_length(*s);

	if(i + count > strl)
		return Error_outOfBounds(0, i + count, strl, "CharString_eraseAtCount()::i + count is out of bounds");

	Buffer_memmove(
		Buffer_createRef(s->ptrNonConst + i, strl - i - count),
		Buffer_createRef(s->ptrNonConst + i + count, strl - i - count)
	);

	const U64 end = strl - count;
	s->ptrNonConst[end] = '\0';
	s->lenAndNullTerminated = end | ((U64)1 << 63);
	return Error_none();
}

Bool CharString_eraseString(CharString *s, CharString other, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len) {

	const U64 otherl = CharString_length(other);

	if(!s || !otherl || CharString_isRef(*s))
		return false;

	const U64 strl = CharString_length(*s);

	//Skipping first match

	const U64 find = CharString_findString(*s, other, caseSensitive, isFirst, off, len);

	if(find == U64_MAX)
		return false;

	Buffer_memmove(
		Buffer_createRef(s->ptrNonConst + find, strl - find - otherl),
		Buffer_createRef(s->ptrNonConst + find + otherl, strl - find - otherl)
	);

	const U64 end = strl - otherl;
	s->ptrNonConst[end] = '\0';
	s->lenAndNullTerminated = end | ((U64)1 << 63);
	return true;
}

//CharString's inline changes (no copy)

Bool CharString_cutEnd(CharString s, U64 length, CharString *result) {
	return CharString_cut(s, 0, length, result);
}

Bool CharString_cutBegin(CharString s, U64 off, CharString *result) {

	if (off > CharString_length(s))
		return false;

	return CharString_cut(s, off, 0, result);
}

Bool CharString_eraseAll(CharString *s, C8 c, EStringCase caseSensitive, U64 off, U64 len) {

	U64 strl = s ? CharString_length(*s) : 0;

	if(!s || CharString_isRef(*s) || off >= strl || off + len > strl)
		return false;

	if(len)
		strl = off + len;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	U64 out = off;

	for (U64 i = off; i < strl; ++i) {

		if(C8_transform(s->ptr[i], (EStringTransform) caseSensitive) == c)
			continue;

		s->ptrNonConst[out++] = s->ptr[i];
	}

	if(out == strl)
		return true;

	s->ptrNonConst[out] = '\0';
	s->lenAndNullTerminated = out | ((U64)1 << 63);
	return true;
}

Bool CharString_eraseAllString(CharString *s, CharString other, EStringCase caseSensitive, U64 off, U64 len) {

	U64 strl = s ? CharString_length(*s) : 0;
	const U64 otherl = CharString_length(other);

	if(!s || !otherl || CharString_isRef(*s) || off >= strl || off + len > strl)
		return false;

	if(len)
		strl = off + len;

	U64 out = off;

	for (U64 i = off; i < strl; ++i) {

		Bool match = i - strl >= otherl;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
			if (
				C8_transform(s->ptr[j], (EStringTransform) caseSensitive) !=
				C8_transform(other.ptr[k], (EStringTransform) caseSensitive)
			) {
				match = false;
				break;
			}

		if (match) {
			i += otherl - 1;	//-1 because we increment by 1 every time
			continue;
		}

		s->ptrNonConst[out++] = s->ptr[i];
	}

	if(out == strl)
		return true;

	s->ptrNonConst[out] = '\0';
	s->lenAndNullTerminated = out | ((U64)1 << 63);
	return true;
}

Bool CharString_replaceAll(CharString *s, C8 c, C8 v, EStringCase caseSensitive, U64 off, U64 len) {

	U64 strl = s ? CharString_length(*s) : 0;

	if (!s || CharString_isConstRef(*s) || off >= strl || off + len > strl)
		return false;

	if(!len)
		len = strl - off;

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for(U64 i = off; i < off + len; ++i)
		if(C8_transform(s->ptr[i], (EStringTransform)caseSensitive) == c)
			s->ptrNonConst[i] = v;

	return true;
}

Bool CharString_replace(CharString *s, C8 c, C8 v, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len) {

	if (!s || CharString_isConstRef(*s))
		return false;

	const U64 i =
		isFirst ? CharString_findFirst(*s, c, caseSensitive, off, len) :
		CharString_findLast(*s, c, caseSensitive, off, len);

	if(i != U64_MAX)
		s->ptrNonConst[i] = v;

	return true;
}

Bool CharString_transform(CharString str, EStringTransform stringTransform) {

	if(CharString_isConstRef(str))
		return false;

	for(U64 i = 0; i < CharString_length(str); ++i)
		str.ptrNonConst[i] = C8_transform(str.ptr[i], stringTransform);

	return true;
}

CharString CharString_trim(CharString s) {

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

	++last;		//We wanna include the last character that's non whitespace too

	const Bool isNullTerm = CharString_isNullTerminated(s) && last == strl;

	return CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr + first, last - first, isNullTerm) :
		CharString_createRefSized(s.ptrNonConst + first, last - first, isNullTerm);
}

Error CharString_popEndCount(CharString *s, U64 count) {
	return CharString_eraseAtCount(s, s ? CharString_length(*s) - count : 0, count);
}

Error CharString_popFrontCount(CharString *s, U64 count) { return CharString_eraseAtCount(s, 0, count); }

Error CharString_eraseAt(CharString *s, U64 i) { return CharString_eraseAtCount(s, i, 1); }
Error CharString_popFront(CharString *s) { return CharString_eraseAt(s, 0); }
Error CharString_popEnd(CharString *s) { return CharString_eraseAt(s, s ? CharString_length(*s) - 1 : 0); }
