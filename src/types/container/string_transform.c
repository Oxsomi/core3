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

//types/container/string_transform.c

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

	//If it's not append (resize already wrote c at the end in that case).
	//Compared against strl, not strl - 1: strl is where an append lands now that the string grew by one.
	//Inserting at strl - 1 is a genuine insert (just before the last character) and used to be skipped.

	if (i != strl) {

		//Move the tail [i, strl) one to the right. The count is strl - i, not strl; the buffer only holds
		//strl + 1 characters, so shifting strl of them from i read and wrote past the end for any i > 1.
		//CharString_insertString below has always done it this way.

		Buffer_memmove(
			Buffer_createRef(s->ptrNonConst + i + 1, strl - i),
			Buffer_createRef(s->ptrNonConst + i, strl - i)
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
	ListU64 finds = { 0 };

	if (!replace || !replace->s || !replace->search || !replace->replace)
		retError(clean, Error_nullPointer(0, "CharString_replaceAllString()::replace is required"));

	alloc = replace->allocator;

	if(CharString_isRef(*replace->s))
		retError(clean, Error_constData(0, 0, "CharString_replaceAllString()::replace->s must be managed memory"));

	const CharStringFind find = { replace->s, replace->allocator, replace->off, replace->len, &finds };
	gotoIfError3(clean, CharString_findAllString(&find, replace->search, caseSensitive, e_rr));

	if (!finds.length)
		goto clean;

	const U64 *ptr = finds.ptr;
	const U64 searchl = CharString_length(*replace->search);
	const U64 replacel = CharString_length(*replace->replace);
	const U64 strl = CharString_length(*replace->s);
	const Buffer replaceBuf = CharString_bufferConst(*replace->replace);

	//Same size; overwrite in place, no resize and no moving

	if (searchl == replacel) {

		C8 *sPtr = replace->s->ptrNonConst;

		for (U64 i = 0; i < finds.length; ++i)
			Buffer_memcpy(Buffer_createRef(sPtr + ptr[i], replacel), replaceBuf);

		goto clean;
	}

	//Shrinking; walk left to right, since every byte moves towards the front

	if (searchl > replacel) {

		C8 *sPtr = replace->s->ptrNonConst;
		U64 write = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			Buffer_memcpy(Buffer_createRef(sPtr + write, replacel), replaceBuf);
			write += replacel;

			//The run between this match and the next one (or the end of the string) follows it

			const U64 tailStart = ptr[i] + searchl;
			const U64 tailEnd = i + 1 == finds.length ? strl : ptr[i + 1];

			Buffer_memmove(
				Buffer_createRef(sPtr + write, tailEnd - tailStart),
				Buffer_createRef(sPtr + tailStart, tailEnd - tailStart)
			);

			write += tailEnd - tailStart;
		}

		gotoIfError3(clean, CharString_resize(replace->s, write, ' ', alloc, e_rr));
		goto clean;
	}

	//Growing; resize first, then walk right to left so nothing is overwritten before it's read.
	//The pointer has to be re-read afterwards, since the resize may have reallocated.

	const U64 newLen = strl + (replacel - searchl) * finds.length;
	gotoIfError3(clean, CharString_resize(replace->s, newLen, ' ', alloc, e_rr));

	{
		C8 *sPtr = replace->s->ptrNonConst;
		U64 readEnd = strl;            //end of the not yet moved region, in the old layout
		U64 write = newLen;            //end of the already written region, in the new layout

		for (U64 i = finds.length - 1; i != U64_MAX; --i) {

			const U64 tailStart = ptr[i] + searchl;
			const U64 tailLen = readEnd - tailStart;

			write -= tailLen;

			Buffer_memmove(
				Buffer_createRef(sPtr + write, tailLen),
				Buffer_createRef(sPtr + tailStart, tailLen)
			);

			write -= replacel;
			Buffer_memcpy(Buffer_createRef(sPtr + write, replacel), replaceBuf);

			readEnd = ptr[i];
		}
	}

clean:

	if(alloc)
		ListU64_free(&finds, alloc);

	return s_uccess;
}

Bool CharString_replaceString(const CharStringReplace2 *replace, Bool isFirst, EStringCase caseSensitive, Error *e_rr) {

	Bool s_uccess = true;

	if (!replace || !replace->s || !replace->search || !replace->replace)
		retError(clean, Error_nullPointer(0, "CharString_replaceString()::replace and replace->s are required"));

	if(CharString_isRef(*replace->s))
		retError(clean, Error_constData(0, 0, "CharString_replaceString()::s must use managed memory"));

	const CharStringSensOffLen find = { replace->s, caseSensitive, replace->off, replace->len };

	const U64 res = isFirst ? CharString_findFirstString(&find, replace->search) :
		CharString_findLastString(&find, replace->search);

	if (res == U64_MAX)
		goto clean;

	const U64 searchl = CharString_length(*replace->search);
	const U64 replacel = CharString_length(*replace->replace);
	const U64 strl = CharString_length(*replace->s);
	const Buffer replaceBuf = CharString_bufferConst(*replace->replace);

	//The run after the match, which has to end up directly behind the replacement

	const U64 tailStart = res + searchl;
	const U64 tailLen = strl - tailStart;

	//Same size; overwrite in place

	if (searchl == replacel) {
		Buffer_memcpy(Buffer_createRef(replace->s->ptrNonConst + res, replacel), replaceBuf);
		goto clean;
	}

	//Shrinking; move the tail left, write the replacement, then give the slack back

	if (replacel < searchl) {

		C8 *sPtr = replace->s->ptrNonConst;

		Buffer_memmove(
			Buffer_createRef(sPtr + res + replacel, tailLen),
			Buffer_createRef(sPtr + tailStart, tailLen)
		);

		Buffer_memcpy(Buffer_createRef(sPtr + res, replacel), replaceBuf);

		gotoIfError3(clean, CharString_resize(
			replace->s, strl - (searchl - replacel), ' ', replace->allocator, e_rr
		));

		goto clean;
	}

	//Growing; resize first, then re-read the pointer since it may have moved, and shift the tail right

	gotoIfError3(clean, CharString_resize(
		replace->s, strl + (replacel - searchl), ' ', replace->allocator, e_rr
	));

	{
		C8 *sPtr = replace->s->ptrNonConst;

		Buffer_memmove(
			Buffer_createRef(sPtr + res + replacel, tailLen),
			Buffer_createRef(sPtr + tailStart, tailLen)
		);

		Buffer_memcpy(Buffer_createRef(sPtr + res, replacel), replaceBuf);
	}

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
