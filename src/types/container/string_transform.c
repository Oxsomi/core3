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
#include "types/base/string_read.h"
#include "types/base/string_mut.h"
#include "types/base/c8.h"
#include "types/base/constants.h"

Bool CharString_append(CharString *s, C8 c, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if (!s)
		retError(clean, Error_nullPointer(0, "CharString_append()::s is required"));

	if (!c && CharString_isNullTerminated(*s))
		goto clean;

	gotoIfError3(clean, CharString_resize(s, CharString_length(*s) + !!c, c, allocator, e_rr));

clean:
	return s_uccess;
}

Bool CharString_appendString(CharString *s, const CharString *other, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	const U64 otherl = !other ? 0 : CharString_length(*other);

	if (!otherl)
		goto clean;

	if (!s)
		retError(clean, Error_nullPointer(0, "CharString_appendString()::s is required"));

	const U64 oldLen = CharString_length(*s);

	if (CharString_isRef(*s) && oldLen)
		retError(clean, Error_invalidParameter(0, 0, "CharString_appendString()::s has to be managed memory"));

	gotoIfError3(clean, CharString_resize(s, oldLen + otherl, other->ptr[0], allocator, e_rr));

	Buffer_memcpy(Buffer_createRef((U8*)s->ptrNonConst + oldLen, otherl), CharString_bufferConst(*other));

clean:
	return s_uccess;
}

Bool CharString_insert(CharString *s, C8 c, U64 i, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if (!s)
		retError(clean, Error_nullPointer(0, "CharString_insert()::s is required"));

	const U64 strl = CharString_length(*s);

	if (CharString_isRef(*s) && strl)
		retError(clean, Error_invalidParameter(0, 0, "CharString_insert()::s should be managed memory"));

	if(i > strl)
		retError(clean, Error_outOfBounds(2, i, strl, "CharString_insert()::i is out of bounds"));

	if (i == strl && !c && CharString_isNullTerminated(*s))
		goto clean;

	if(!c && i != strl)
		retError(clean, Error_invalidOperation(0, "CharString_insert()::c is 0, which isn't allowed if i != strl"));

	gotoIfError3(clean, CharString_resize(s, strl + 1, c, allocator, e_rr));

	//If it's not append (otherwise it's already handled)

	if (i != strl - 1) {

		//Move one to the right

		Buffer_memmove(
			Buffer_createRef(s->ptrNonConst + i + 1,  strl),
			Buffer_createRef(s->ptrNonConst + i, strl)
		);

		s->ptrNonConst[i] = c;
	}

clean:
	return s_uccess;
}

Bool CharString_insertString(CharString *s, const CharString *other, U64 i, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if (!s)
		retError(clean, Error_nullPointer(0, "CharString_insertString()::s is required"));

	const U64 oldLen = CharString_length(*s);

	if (CharString_isRef(*s) && oldLen)
		retError(clean, Error_invalidParameter(0, 0, "CharString_insertString()::s should be managed memory"));

	const U64 otherl = !other ? 0 : CharString_length(*other);

	if (!otherl)
		goto clean;

	gotoIfError3(clean, CharString_resize(s, oldLen + otherl, ' ', allocator, e_rr));

	//Move one to the right

	Buffer_memmove(
		Buffer_createRef(s->ptrNonConst + i + otherl, oldLen - i),
		Buffer_createRef(s->ptrNonConst + i, oldLen - i)
	);

	Buffer_memcpy(
		Buffer_createRef(s->ptrNonConst + i, otherl),
		CharString_bufferConst(*other)
	);

clean:
	return s_uccess;
}

Bool CharString_replaceAllString(const CharStringReplace2 *replace, EStringCase caseSensitive, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = NULL;

	if (!replace || !replace->s)
		retError(clean, Error_nullPointer(0, "CharString_replaceAllString()::replace is required"));

	alloc = replace->allocator;

	if(CharString_isRef(*replace->s))
		retError(clean, Error_constData(0, 0, "CharString_replaceAllString()::replace->s must be managed memory"));

	ListU64 finds = { 0 };

	const CharStringFind find = { replace->s, replace->allocator, caseSensitive, replace->off, replace->len, &finds };
	gotoIfError3(clean, CharString_findAllString(&find, replace->search, e_rr));

	if (!finds.length)
		goto clean;

	//Easy replace

	const U64 *ptr = finds.ptr;

	const U64 searchl = CharString_length(*replace->search);
	C8 *sPtrNonConst = replace->search->ptr;

	const U64 strl = CharString_length(*replace->replace);
	const Buffer replaceBuf = CharString_bufferConst(*replace->replace);
	const U64 replacel = CharString_length(*replace->replace);

	if (searchl == replacel) {

		for (U64 i = 0; i < finds.length; ++i)
			for (U64 j = ptr[i], k = j + replacel, l = 0; j < k; ++j, ++l)
				sPtrNonConst[j] = (C8) replaceBuf.ptr[l];

		goto clean;
	}

	//Shrink replaces

	if (searchl > replacel) {

		const U64 diff = searchl - replacel;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			//We move our replacement string to iCurrent

			Buffer_memcpy(Buffer_createRef(sPtrNonConst + iCurrent, replacel), replaceBuf);

			iCurrent += replacel;

			//We then move the tail of the string

			const U64 iStart = ptr[i] + searchl;
			const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

			Buffer_memmove(
				Buffer_createRef(sPtrNonConst + iCurrent, iEnd - iStart),
				Buffer_createRef(sPtrNonConst + iStart, iEnd - iStart)
			);

			iCurrent += iEnd - iStart;
		}

		//Ensure the string is now the right size

		gotoIfError3(clean, CharString_resize(s, strl - diff * finds.length, ' ', alloc, e_rr));
		goto clean;
	}

	//Grow replaces

	//Ensure the string is now the right size

	const U64 diff = replacel - searchl;

	gotoIfError3(clean, CharString_resize(s, strl + diff * finds.length, ' ', alloc, e_rr));

	//Move from right to left

	U64 newLoc = strl - 1;

	for (U64 i = finds.length - 1; i != U64_MAX; ++i) {

		//Find tail

		const U64 iStart = ptr[i] + searchl;
		const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

		for (U64 j = iEnd - 1; j != U64_MAX && j >= iStart; --j)
			sPtrNonConst[newLoc--] = sPtrNonConst[j];

		Buffer_memmove(
			Buffer_createRef(sPtrNonConst + newLoc - (iEnd - iStart), iEnd - iStart),
			Buffer_createRef(sPtrNonConst + iStart, iEnd - iStart)
		);

		newLoc -= iEnd - iStart;

		//Apply replacement before tail

		Buffer_memcpy(Buffer_createRef(sPtrNonConst + newLoc - replacel, replacel), replaceBuf);
		newLoc -= replacel;
	}

clean:

	if(alloc)
		ListU64_free(&finds, alloc);

	return s_uccess;
}

Bool CharString_replaceString(const CharStringReplace2 *replace, Bool isFirst, EStringCase caseSensitive, Error *e_rr) {

	Bool s_uccess = true;

	if (!replace || replace->s)
		retError(clean, Error_nullPointer(0, "CharString_replaceString()::replace or replace->s are required"));

	if(CharString_isRef(*replace->s))
		retError(clean, Error_constData(0, 0, "CharString_replaceString()::s must use managed memory"));
	
	const CharStringSensOffLen find = { replace->s, caseSensitive, replace->off, replace->len };

	const U64 res = isFirst ? CharString_findFirstString(&find, replace->search) :
		CharString_findLastString(&find, replace->search);

	if (res == U64_MAX)
		goto clean;

	const U64 searchl = CharString_length(*replace->search);
	C8 *sPtrNonConst = replace->search->ptr;

	const U64 strl = CharString_length(*replace->replace);
	const Buffer replaceBuf = CharString_bufferConst(*replace->replace);
	const U64 replacel = CharString_length(*replace->replace);

	//Easy, early exit. Strings are same size.

	if (searchl == replacel) {
		Buffer_memcpy(Buffer_createRef((U8*)sPtrNonConst + res, replacel), replaceBuf);
		goto clean;
	}

	//Replacement is smaller than our search
	//So we can just move from left to right

	if (replacel < searchl) {

		const U64 diff = searchl - replacel;	//How much we have to shrink

		//Copy our data over first

		Buffer_memmove(
			Buffer_createRef(sPtrNonConst + res + replacel, strl - (res + searchl)),
			Buffer_createRef(sPtrNonConst + res + searchl, strl - (res + searchl))
		);

		//Throw our replacement in there

		Buffer_memcpy(Buffer_createRef(sPtrNonConst + res, replacel), replaceBuf);

		//Shrink the string; this is done after because we need to read the right of the string first

		gotoIfError3(clean, CharString_resize(s, strl - diff, ' ', alloc, e_rr));
		goto clean;
	}

	//Replacement is bigger than our search;
	//We need to grow first and move from right to left

	const U64 diff = replacel - searchl;

	gotoIfError3(clean, CharString_resize(s, strl + diff, ' ', alloc, e_rr));

	//Copy our data over first

	Buffer_memmove(
		Buffer_createRef(sPtrNonConst + res + replacel, strl - (res + searchl)),
		Buffer_createRef(sPtrNonConst + res + searchl, strl - (res + searchl))
	);

	//Throw our replacement in there

	Buffer_memcpy(Buffer_createRef(sPtrNonConst + res, replacel), replaceBuf);

clean:
	return s_uccess;
}

CharString CharString_getFilePath(CharString *str) {

	if(!CharString_formatPath(str))
		return CharString_createNull();

	CharString res = CharString_createNull();
	const CharStringCut cut = { str, EStringCase_Sensitive, &res };
	CharString_cutBefore(&cut, '/', false);
	return res;
}

CharString CharString_getBasePath(CharString *str) {

	if(!CharString_formatPath(str))
		return CharString_createNull();

	CharString res = CharString_createNull();
	const CharStringCut cut = { str, EStringCase_Sensitive, &res };
	CharString_cutAfter(&cut, '/', false);
	return res;
}
