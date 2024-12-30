/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/allocator.h"
#include "types/math/math.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/c8.h"

#include <ctype.h>
#include <stdio.h>

TListImpl(CharString);
TListNamedImpl(ListConstC8);

Bool ListCharString_sort(ListCharString list, EStringCase stringCase) {
	return GenericList_sortString(ListCharString_toList(list), stringCase);
}

Bool ListCharString_sortSensitive(ListCharString list) {
	return GenericList_sortStringSensitive(ListCharString_toList(list));
}

Bool ListCharString_sortInsensitive(ListCharString list) {
	return GenericList_sortStringInsensitive(ListCharString_toList(list));
}

Bool CharString_isConstRef(CharString str) { return str.capacityAndRefInfo == U64_MAX; }
Bool CharString_isRef(CharString str) { return !str.capacityAndRefInfo || CharString_isConstRef(str); }
U64  CharString_bytes(CharString str) { return str.lenAndNullTerminated << 1 >> 1; }
U64  CharString_length(CharString str) { return CharString_bytes(str); }
U64  CharString_capacity(CharString str) { return CharString_isRef(str) ? 0 : str.capacityAndRefInfo; }
Bool CharString_isEmpty(CharString str) { return !CharString_bytes(str); }
Bool CharString_isNullTerminated(CharString str) { return str.lenAndNullTerminated >> 63; }

Buffer CharString_buffer(CharString str) {
	return CharString_isConstRef(str) ? Buffer_createNull() : Buffer_createRef(str.ptrNonConst, CharString_bytes(str));
}

Buffer CharString_bufferConst(CharString str) {
	return Buffer_createRefConst(str.ptr, CharString_bytes(str));
}

Buffer CharString_allocatedBuffer(CharString str) {
	return CharString_isRef(str) ? Buffer_createNull() : Buffer_createRef(str.ptrNonConst, CharString_capacity(str));
}

Buffer CharString_allocatedBufferConst(CharString str) {
	return CharString_isRef(str) ? Buffer_createNull() : Buffer_createRefConst(str.ptr, CharString_capacity(str));
}

//Iteration

C8 *CharString_begin(CharString str) { return CharString_isConstRef(str) ? NULL : str.ptrNonConst; }
const C8 *CharString_beginConst(CharString str) { return str.ptr; }

C8 *CharString_end(CharString str) { return CharString_isConstRef(str) ? NULL : str.ptrNonConst + CharString_length(str); }
const C8 *CharString_endConst(CharString str) { return str.ptr + CharString_length(str); }

C8 *CharString_charAt(CharString str, U64 off) {
	return CharString_isConstRef(str) || off >= CharString_length(str) ? NULL : str.ptrNonConst + off;
}

const C8 *CharString_charAtConst(CharString str, U64 off) {
	return off >= CharString_length(str) ? NULL : str.ptr + off;
}

Bool CharString_isValidAscii(CharString str) {

	for(U64 i = 0; i < CharString_length(str); ++i)
		if(!C8_isValidAscii(str.ptr[i]))
			return false;

	return true;
}

Bool CharString_isValidFileName(CharString str) {

	//TODO: Understand UTF8

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
			CharString_startsWithStringInsensitive(str, CharString_createRefCStrConst("CON"), 0) ||
			CharString_startsWithStringInsensitive(str, CharString_createRefCStrConst("AUX"), 0) ||
			CharString_startsWithStringInsensitive(str, CharString_createRefCStrConst("NUL"), 0) ||
			CharString_startsWithStringInsensitive(str, CharString_createRefCStrConst("PRN"), 0)
		)
			illegalStart = 3;

		else if (strl >= 4) {

			if(
				(
					CharString_startsWithStringInsensitive(str, CharString_createRefCStrConst("COM"), 0) ||
					CharString_startsWithStringInsensitive(str, CharString_createRefCStrConst("LPT"), 0)
				) &&
				C8_isDec(str.ptr[3])
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

Bool CharString_isSupportedInFilePath(CharString str) {
	return
		CharString_isValidFileName(str) ||
		(CharString_getAt(str, 0) == '.' && (
			CharString_length(str) == 1 || (CharString_getAt(str, 1) == '.' && CharString_length(str) == 2)
		));
}

//File_resolve but without validating if it'd be a valid (permitted) path on disk.

Bool CharString_isValidFilePath(CharString str) {

	//TODO: Understand UTF8

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

	return strl;
}

Bool CharString_clear(CharString *str) {

	if(!str || CharString_isRef(*str) || !str->capacityAndRefInfo)
		return false;

	if(str->lenAndNullTerminated >> 63)					//If null terminated, we want to keep it null terminated
		((C8*)str->ptr)[0] = '\0';

	str->lenAndNullTerminated &= ~(((U64)1 << 63) - 1);		//Clear size
	return true;
}

C8 CharString_getAt(CharString str, U64 i) {
	return i < CharString_length(str) ? str.ptr[i] : C8_MAX;
}

Bool CharString_setAt(CharString str, U64 i, C8 c) {

	if(i >= CharString_length(str) || CharString_isConstRef(str) || !c)
		return false;

	str.ptrNonConst[i] = c;
	return true;
}

CharString CharString_createNull() { return (CharString) { 0 }; }

CharString CharString_createRefAutoConst(const C8 *ptr, U64 maxSize) {

	if(!ptr)
		return CharString_createNull();

	const U64 strl = CharString_calcStrLen(ptr, maxSize);

	return (CharString) {
		.lenAndNullTerminated = strl | ((U64)(strl != maxSize) << 63),
		.ptr = ptr,
		.capacityAndRefInfo = U64_MAX		//Flag as const
	};
}

CharString CharString_createRefCStrConst(const C8 *ptr) {
	return CharString_createRefAutoConst(ptr, U64_MAX);
}

CharString CharString_createRefAuto(C8 *ptr, U64 maxSize) {
	CharString str = CharString_createRefAutoConst(ptr, maxSize);
	str.capacityAndRefInfo = 0;		//Flag as mutable
	return str;
}

CharString CharString_createRefSizedConst(const C8 *ptr, U64 size, Bool isNullTerminated) {

	if(!ptr || (size >> 48))
		return CharString_createNull();

	if(isNullTerminated && ptr[size])	//Invalid!
		return CharString_createNull();

	if(!isNullTerminated && size) {

		isNullTerminated = !ptr[size - 1];

		if(isNullTerminated)
			--size;
	}

	return (CharString) {
		.lenAndNullTerminated = size | ((U64)isNullTerminated << 63),
		.ptr = ptr,
		.capacityAndRefInfo = U64_MAX		//Flag as const
	};
}

CharString CharString_createRefSized(C8 *ptr, U64 size, Bool isNullTerminated) {
	CharString str = CharString_createRefSizedConst(ptr, size, isNullTerminated);
	str.capacityAndRefInfo = 0;		//Flag as mutable
	return str;
}

CharString CharString_createRefStrConst(CharString str) {
	return CharString_createRefSizedConst(str.ptr, CharString_length(str), CharString_isNullTerminated(str));
}

CharString CharString_createRefShortStringConst(const ShortString str) {
	return CharString_createRefAutoConst(str, SHORTSTRING_LEN);
}

CharString CharString_createRefLongStringConst(const LongString str) {
	return CharString_createRefAutoConst(str, LONGSTRING_LEN);
}

CharString CharString_createRefShortString(ShortString str) {
	return CharString_createRefAuto(str, SHORTSTRING_LEN);
}

CharString CharString_createRefLongString(LongString str) {
	return CharString_createRefAuto(str, LONGSTRING_LEN);
}

//Simple checks (consts)

Error CharString_create(C8 c, U64 size, Allocator alloc, CharString *result) {

	if (!alloc.alloc)
		return Error_nullPointer(2, "CharString_create()::alloc is required");

	if (!result)
		return Error_nullPointer(3, "CharString_create()::result is required");

	if (result->ptr)
		return Error_invalidOperation(0, "CharString_create()::result isn't empty, might indicate memleak");

	if (size >> 48)
		return Error_invalidOperation(1, "CharString_create()::size must be 48-bit");

	if (!size) {
		*result = CharString_createNull();
		return Error_none();
	}

	Buffer b = Buffer_createNull();
	const Error err = alloc.alloc(alloc.ptr, size + 1, &b);

	if (err.genericError)
		return err;

	const U64 realSize = size;

	b.ptrNonConst[size] = '\0';

	//Quick fill (is okay, allocator is guaranteed to align 8-byte)

	const U16 cc2 = ((U16)c << 8) | ((U16)c << 0);
	const U32 cc4 = ((U32)cc2 << 16) | cc2;
	const U64 cc8 = ((U64)cc4 << 32) | cc4;

	for (U64 i = 0; i < size >> 3; ++i)
		*((U64*)b.ptrNonConst + i) = cc8;

	size &= 7;

	if (size & 4) {
		*(U32*)(b.ptrNonConst + (realSize >> 3 << 3)) = cc4;
		size &= 3;
	}

	if (size & 2) {
		*(U16*)(b.ptrNonConst + (realSize >> 2 << 2)) = cc2;
		size &= 1;
	}

	if(size & 1)
		b.ptrNonConst[realSize - 1] = c;

	*result = (CharString) {
		.lenAndNullTerminated = (realSize | ((U64)1 << 63)),
		.capacityAndRefInfo = realSize + 1,
		.ptrNonConst = (C8*)b.ptr
	};

	return Error_none();
}

Error CharString_createCopy(CharString str, Allocator alloc, CharString *result) {

	if (!alloc.alloc || !result)
		return Error_nullPointer(!result ? 2 : 1, "CharString_createCopy()::alloc and result are required");

	if (result->ptr)
		return Error_invalidOperation(0, "CharString_createCopy()::result wasn't empty, might indicate a memleak");

	const U64 strl = CharString_length(str);

	if (!strl) {
		*result = CharString_createNull();
		return Error_none();
	}

	Buffer b = Buffer_createNull();
	const Error err = alloc.alloc(alloc.ptr, strl + 1, &b);

	if (err.genericError)
		return err;

	Buffer_copy(b, CharString_bufferConst(str));
	b.ptrNonConst[strl] = '\0';

	*result = (CharString) {
		.ptrNonConst = (C8*) b.ptrNonConst,
		.lenAndNullTerminated = (strl | ((U64)1 << 63)),
		.capacityAndRefInfo = strl + 1
	};

	return Error_none();
}

Bool CharString_free(CharString *str, Allocator alloc) {

	if (!str)
		return true;

	if(!alloc.free)
		return false;

	Bool freed = true;

	if(!CharString_isRef(*str))
		freed = alloc.free(alloc.ptr, Buffer_createManagedPtr((U8*)str->ptr, str->capacityAndRefInfo));

	*str = CharString_createNull();
	return freed;
}

Error CharString_split(
	CharString s,
	C8 c,
	EStringCase casing,
	Allocator allocator,
	ListCharString *result
) {

	const U64 length = CharString_countAll(s, c, casing, 0);

	const Error err = ListCharString_create(length + 1, allocator, result);

	if (err.genericError)
		return err;

	const U64 strl = CharString_length(s);

	if (!length) {

		const Bool b = CharString_isNullTerminated(s);

		result->ptrNonConst[0] = CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr, strl, b) :
			CharString_createRefSized(s.ptrNonConst, strl, b);

		return Error_none();
	}

	const ListCharString str = *result;

	c = C8_transform(c, (EStringTransform) casing);

	U64 count = 0, last = 0;

	for (U64 i = 0; i < strl; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform) casing) == c) {

			str.ptrNonConst[count++] =
				CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr + last, i - last, false) :
				CharString_createRefSized(s.ptrNonConst + last, i - last, false);

			last = i + 1;
		}

	const Bool b = CharString_isNullTerminated(s);

	str.ptrNonConst[count++] =
		CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr + last, strl - last, b) :
		CharString_createRefSized(s.ptrNonConst + last, strl - last, b);

	return Error_none();
}

Error CharString_splitString(
	CharString s,
	CharString other,
	EStringCase casing,
	Allocator allocator,
	ListCharString *result
) {

	const U64 length = CharString_countAllString(s, other, casing, 0);

	const Error err = ListCharString_create(length + 1, allocator, result);

	if (err.genericError)
		return err;

	const Bool b = CharString_isNullTerminated(s);
	const U64 strl = CharString_length(s);
	const U64 otherl = CharString_length(other);

	if (!length) {

		*result->ptrNonConst = CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr, strl, b) :
			CharString_createRefSized(s.ptrNonConst, strl, b);

		return Error_none();
	}

	const ListCharString str = *result;

	U64 count = 0, last = 0;

	for (U64 i = 0; i < strl - otherl + 1; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {

			str.ptrNonConst[count++] =
				CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr + last, i - last, false) :
				CharString_createRefSized(s.ptrNonConst + last, i - last, false);

			last = i + otherl;
			i += otherl - 1;
		}
	}

	str.ptrNonConst[count++] =
		CharString_isConstRef(s) ? CharString_createRefSizedConst(s.ptr + last, strl - last, b) :
		CharString_createRefSized(s.ptrNonConst + last, strl - last, b);

	return Error_none();
}

Error CharString_splitLine(CharString s, Allocator alloc, ListCharString *result) {

	if(!result)
		return Error_nullPointer(2, "CharString_splitLine()::result is invalid");

	if(result->ptr)
		return Error_invalidParameter(2, 1, "CharString_splitLine()::result wasn't empty, might indicate memleak");

	U64 v = 0, lastLineEnd = U64_MAX;
	const U64 strl = CharString_length(s);

	for(U64 i = 0; i < strl; ++i)

		if (s.ptr[i] == '\n') {			//Unix line endings
			++v;
			lastLineEnd = i;
			continue;
		}

		else if (s.ptr[i] == '\r') {	//Windows/Legacy Mac line endings

			if(i + 1 < strl && s.ptr[i + 1] == '\n')		//Windows
				++i;

			++v;
			lastLineEnd = i;
			continue;
		}

	if(lastLineEnd != strl)
		++v;

	const Error err = ListCharString_create(v, alloc, result);

	if (err.genericError)
		return err;

	v = 0;
	lastLineEnd = 0;

	for(U64 i = 0; i < strl; ++i) {

		Bool isLineEnd = false;

		const U64 iOld = i;

		if (s.ptr[i] == '\n')			//Unix line endings
			isLineEnd = true;

		else if (s.ptr[i] == '\r') {	//Windows/Legacy Mac line endings

			if(i + 1 < strl && s.ptr[i + 1] == '\n')		//Windows
				++i;

			isLineEnd = true;
		}

		if(!isLineEnd)
			continue;

		result->ptrNonConst[v++] = CharString_isConstRef(s) ?
			CharString_createRefSizedConst(s.ptr + lastLineEnd, iOld - lastLineEnd, false) :
			CharString_createRefSized(s.ptrNonConst + lastLineEnd, iOld - lastLineEnd, false);

		lastLineEnd = i + 1;
	}

	if(lastLineEnd != strl)
		result->ptrNonConst[v++] = CharString_isConstRef(s) ?
			CharString_createRefSizedConst(s.ptr + lastLineEnd, strl - lastLineEnd, CharString_isNullTerminated(s)) :
			CharString_createRefSized(s.ptrNonConst + lastLineEnd, strl - lastLineEnd, CharString_isNullTerminated(s));

	return Error_none();
}

Error CharString_reserve(CharString *str, U64 length, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0, "CharString_reserve()::str is required");

	if (CharString_isRef(*str) && CharString_length(*str))
		return Error_invalidOperation(0, "CharString_reserve()::str has to be managed memory");

	if (length >> 48)
		return Error_invalidOperation(1, "CharString_reserve()::length should be 48-bit");

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(2, "CharString_reserve()::alloc is required");

	if (length + 1 <= str->capacityAndRefInfo)
		return Error_none();

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, length + 1, &b);

	if (err.genericError)
		return err;

	Buffer_copy(b, CharString_bufferConst(*str));

	b.ptrNonConst[length] = '\0';
	str->lenAndNullTerminated |= (U64)1 << 63;

	if(str->capacityAndRefInfo)
		err =
			alloc.free(alloc.ptr, Buffer_createManagedPtr(str->ptrNonConst, str->capacityAndRefInfo)) ?
			Error_none() : Error_invalidOperation(0, "CharString_reserve() free failed");

	str->capacityAndRefInfo = Buffer_length(b);
	str->ptr = (const C8*) b.ptr;
	return err;
}

Error CharString_resize(CharString *str, U64 length, C8 defaultChar, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0, "CharString_resize()::str is required");

	const U64 strl = CharString_length(*str);

	if (CharString_isRef(*str) && strl)
		return Error_invalidOperation(0, "CharString_resize()::str needs to be managed memory");

	if (length >> 48)
		return Error_invalidOperation(1, "CharString_resize()::length should be 48-bit");

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(3, "CharString_resize()::alloc is required");

	if (length == strl && CharString_isNullTerminated(*str))
		return Error_none();

	//Simple resize; we cut off the tail

	if (length < strl) {
		str->lenAndNullTerminated = ((U64)1 << 63) | length;
		str->ptrNonConst[length] = '\0';
		return Error_none();
	}

	//Resize that triggers buffer resize

	if (length + 1 > str->capacityAndRefInfo) {

		//Reserve 50% more to ensure we don't resize too many times

		const Error err = CharString_reserve(str, U64_max(64, length * 3 / 2) + 1, alloc);

		if (err.genericError)
			return err;
	}

	//Our capacity is big enough to handle it:

	for (U64 i = strl; i < length; ++i)
		str->ptrNonConst[i] = defaultChar;

	str->ptrNonConst[length] = '\0';
	str->lenAndNullTerminated = length | ((U64)1 << 63);
	return Error_none();
}

Error CharString_append(CharString *s, C8 c, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, "CharString_append()::s is required");

	if(!c && CharString_isNullTerminated(*s))
		return Error_none();

	return CharString_resize(s, CharString_length(*s) + (Bool)c, c, allocator);
}

CharString CharString_newLine() { return CharString_createRefCStrConst("\n"); }

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

	Buffer_copy(Buffer_createRef((U8*)s->ptrNonConst + oldLen, otherl), CharString_bufferConst(other));
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

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + i + 1,  strl),
			Buffer_createRefConst(s->ptr + i, strl)
		);

		((C8*)s->ptr)[i] = c;
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

	Buffer_revCopy(
		Buffer_createRef((U8*)s->ptr + i + otherl, oldLen - i),
		Buffer_createRefConst(s->ptr + i, oldLen - i)
	);

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + i, otherl),
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
				((C8*)s->ptr)[j] = replace.ptr[l];

		goto clean;
	}

	//Shrink replaces

	if (searchl > replacel) {

		const U64 diff = searchl - replacel;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			//We move our replacement string to iCurrent

			Buffer_copy(
				Buffer_createRef((U8*)s->ptr + iCurrent, replacel),
				CharString_bufferConst(replace)
			);

			iCurrent += replacel;

			//We then move the tail of the string

			const U64 iStart = ptr[i] + searchl;
			const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

			Buffer_copy(
				Buffer_createRef((U8*)s->ptr + iCurrent, iEnd - iStart),
				Buffer_createRefConst(s->ptr + iStart, iEnd - iStart)
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

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + newLoc - (iEnd - iStart), iEnd - iStart),
			Buffer_createRefConst(s->ptr + iStart, iEnd - iStart)
		);

		newLoc -= iEnd - iStart;

		//Apply replacement before tail

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + newLoc - replacel, replacel),
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
		Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));
		return Error_none();
	}

	//Replacement is smaller than our search
	//So we can just move from left to right

	if (replacel < searchl) {

		const U64 diff = searchl - replacel;	//How much we have to shrink

		//Copy our data over first

		Buffer_copy(
			Buffer_createRef((U8*)s->ptr + res + replacel, strl - (res + searchl)),
			Buffer_createRefConst(s->ptr + res + searchl, strl - (res + searchl))
		);

		//Throw our replacement in there

		Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));

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

	Buffer_revCopy(
		Buffer_createRef((U8*)s->ptr + res + replacel, strl - (res + searchl)),
		Buffer_createRefConst(s->ptr + res + searchl, strl - (res + searchl))
	);

	//Throw our replacement in there

	Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));
	return Error_none();
}

Bool CharString_startsWith(CharString str, C8 c, EStringCase caseSensitive, U64 off) {
	return
		CharString_length(str) > off && str.ptr &&
		C8_transform(str.ptr[off], (EStringTransform)caseSensitive) ==
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool CharString_endsWith(CharString str, C8 c, EStringCase caseSensitive, U64 off) {
	return
		CharString_length(str) > off && str.ptr &&
		C8_transform(str.ptr[CharString_length(str) - 1 - off], (EStringTransform) caseSensitive) ==
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool CharString_startsWithString(CharString str, CharString other, EStringCase caseSensitive, U64 off) {

	const U64 otherl = CharString_length(other);
	U64 strl = CharString_length(str);

	if(off > strl)
		return false;

	strl -= off;

	if(!otherl)
		return true;

	if (otherl > strl)
		return false;

	for (U64 i = off; i < off + otherl; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) !=
			C8_transform(other.ptr[i - off], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool CharString_endsWithString(CharString str, CharString other, EStringCase caseSensitive, U64 off) {

	const U64 otherl = CharString_length(other);
	U64 strl = CharString_length(str);

	if(off > strl)
		return false;

	strl -= off;

	if(!otherl)
		return true;

	if (otherl > strl)
		return false;

	for (U64 i = strl - otherl; i < strl; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) !=
			C8_transform(other.ptr[i - (strl - otherl)], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

U64 CharString_countAll(CharString s, C8 c, EStringCase caseSensitive, U64 off) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	U64 count = 0;

	for (U64 i = off; i < CharString_length(s); ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			++count;

	return count;
}

U64 CharString_countAllString(CharString s, CharString other, EStringCase caseSensitive, U64 off) {

	const U64 otherl = CharString_length(other);
	const U64 strl = CharString_length(s);

	if(!otherl || strl < otherl)
		return 0;

	U64 j = 0;

	for (U64 i = off; i < strl - otherl + 1; ++i) {

		Bool match = true;

		for (U64 l = i, k = 0; l < strl && k < otherl; ++l, ++k)
			if (
				C8_transform(s.ptr[l], (EStringTransform)caseSensitive) !=
				C8_transform(other.ptr[k], (EStringTransform)caseSensitive)
			) {
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

Error CharString_findAll(
	CharString s,
	C8 c,
	Allocator alloc,
	EStringCase caseSensitive,
	U64 off,
	U64 len,
	ListU64 *result
) {

	if(!result)
		return Error_nullPointer(6, "CharString_findAll()::result is required");

	if(result->ptr)
		return Error_invalidParameter(6, 0, "CharString_findAll()::result wasn't empty, might indicate memleak");

	U64 strl = CharString_length(s);

	if(off >= strl || (off + len) > strl)
		return Error_invalidParameter(4, 0, "CharString_findAll()::off or len out of bounds");

	if(len)
		strl = off + len;

	ListU64 l = (ListU64) { 0 };
	Error err = ListU64_reserve(&l, (strl - off) / 25 + 16, alloc);

	if (err.genericError)
		return err;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	for (U64 i = off; i < strl; ++i)
		if (c == C8_transform(s.ptr[i], (EStringTransform)caseSensitive))
			if ((err = ListU64_pushBack(&l, i, alloc)).genericError) {
				ListU64_free(&l, alloc);
				return err;
			}

	*result = l;
	return Error_none();
}

Error CharString_findAllString(
	CharString s,
	CharString other,
	Allocator alloc,
	EStringCase caseSensitive,
	U64 off,
	U64 len,
	ListU64 *result
) {

	if(!result)
		return Error_nullPointer(6, "CharString_findAllString()::result is required");

	if(result->ptr)
		return Error_invalidParameter(6, 0, "CharString_findAllString()::result wasn't empty, might indicate memleak");

	const U64 otherl = CharString_length(other);
	U64 strl = CharString_length(s);

	if(!otherl)
		return Error_invalidParameter(1, 0, "CharString_findAllString()::other is empty");

	if(off >= strl || off + len > strl)
		return Error_invalidParameter(4, 0, "CharString_findAllString()::off or len is out of bounds");

	if(len)
		strl = off + len;

	ListU64 l = (ListU64) { 0 };

	if((strl - off) < otherl) {
		*result = l;
		return Error_none();
	}

	Error err = ListU64_reserve(&l, (strl - off) / otherl / 25 + 16, alloc);

	if (err.genericError)
		return err;

	for (U64 i = off; i < strl; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)caseSensitive) !=
				C8_transform(other.ptr[k], (EStringTransform)caseSensitive)
			) {
				match = false;
				break;
			}

		if (match) {

			if ((err = ListU64_pushBack(&l, i, alloc)).genericError) {
				ListU64_free(&l, alloc);
				return err;
			}

			i += otherl - 1;
		}
	}

	*result = l;
	return Error_none();
}

U64 CharString_findFirst(CharString s, C8 c, EStringCase caseSensitive, U64 off, U64 len) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	if(off >= CharString_length(s) || off + len > CharString_length(s))
		return U64_MAX;

	if(!len)
		len = CharString_length(s) - off;

	for (U64 i = off; i < off + len; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return U64_MAX;
}

U64 CharString_findLast(CharString s, C8 c, EStringCase caseSensitive, U64 off, U64 len) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	if(off >= CharString_length(s) || off + len > CharString_length(s))
		return U64_MAX;

	if(!len)
		len = CharString_length(s) - off;

	for (U64 i = (off + len) - 1; i != U64_MAX && i >= off; --i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return U64_MAX;
}

U64 CharString_findFirstString(CharString s, CharString other, EStringCase caseSensitive, U64 off, U64 len) {

	const U64 otherl = CharString_length(other);
	U64 strl = CharString_length(s);

	if(!otherl || strl < otherl)
		return U64_MAX;

	if(off >= CharString_length(s) || off + len > CharString_length(s))
		return U64_MAX;

	if(len)
		strl = off + len;

	U64 i = off;

	for (; i < strl; ++i) {

		Bool match = true;
		U64 k = 0;

		for (U64 j = i; j < strl && k < otherl; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)caseSensitive) !=
				C8_transform(other.ptr[k], (EStringTransform)caseSensitive)
			) {
				match = false;
				break;
			}

		if (match && k == otherl)
			break;
	}

	return i >= strl ? U64_MAX : i;
}

U64 CharString_findLastString(CharString s, CharString other, EStringCase caseSensitive, U64 off, U64 len) {

	U64 strl = CharString_length(s);
	const U64 otherl = CharString_length(other);

	if(!otherl || strl < otherl)
		return U64_MAX;

	if(off >= CharString_length(s) || off + len > CharString_length(s))
		return U64_MAX;

	if(len)
		strl = off + len;

	U64 i = strl - 1;

	for (; i != U64_MAX && i >= off; --i) {

		Bool match = true;

		for (U64 j = i, k = otherl - 1; j != U64_MAX && k != U64_MAX; --j, --k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)caseSensitive) !=
				C8_transform(other.ptr[k], (EStringTransform)caseSensitive)
			) {
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

Bool CharString_equalsString(CharString s, CharString other, EStringCase caseSensitive) {

	const U64 strl = CharString_length(s);
	const U64 otherl = CharString_length(other);

	if (strl != otherl)
		return false;

	if (caseSensitive == EStringCase_Sensitive)
		return Buffer_eq(CharString_bufferConst(s), CharString_bufferConst(other));

	for (U64 i = 0; i < strl; ++i)
		if (C8_toLower(s.ptr[i]) != C8_toLower(other.ptr[i]))
			return false;

	return true;
}

Bool CharString_equals(CharString s, C8 c, EStringCase caseSensitive) {
	return CharString_length(s) == 1 && s.ptr &&
		C8_transform(s.ptr[0], (EStringTransform) caseSensitive) ==
		C8_transform(c, (EStringTransform) caseSensitive);
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

	Buffer_copy(
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

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + i, strl - i - count),
		Buffer_createRefConst(s->ptr + i + count, strl - i - count)
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

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + find, strl - find - otherl),
		Buffer_createRefConst(s->ptr + find + otherl, strl - find - otherl)
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

Bool ListCharString_freeUnderlying(ListCharString *arr, Allocator alloc) {

	if(!arr || !ListCharString_allocatedBytes(*arr))
		return true;

	Bool freed = true;

	for(U64 i = 0; i < arr->length; ++i) {
		CharString *str = arr->ptrNonConst + i;
		freed &= CharString_free(str, alloc);
	}

	freed &= ListCharString_free(arr, alloc);
	return freed;
}

Error ListCharString_createCopyUnderlying(ListCharString toCopy, Allocator alloc, ListCharString *arr) {

	if(!toCopy.length) {

		if(!arr)
			return Error_nullPointer(3, "ListCharString_createCopyUnderlying()::arr is required");

		if (arr->ptr)
			return Error_invalidOperation(
				0, "ListCharString_createCopyUnderlying()::arr wasn't empty, which might indicate memleak"
			);

		*arr = (ListCharString) { 0 };
		return Error_none();
	}

	Error err = ListCharString_create(toCopy.length, alloc, arr);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < toCopy.length; ++i) {

		err = CharString_createCopy(toCopy.ptr[i], alloc, arr->ptrNonConst + i);

		if (err.genericError) {
			ListCharString_freeUnderlying(arr, alloc);
			return err;
		}
	}

	return Error_none();
}

Bool ListCharString_move(ListCharString *src, Allocator alloc, ListCharString *dst, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!src || !dst)
		retError(clean, Error_nullPointer(!src ? 0 : 2, "ListCharString_move()::src and dst are required"))

	if(dst->ptr)
		retError(clean, Error_invalidParameter(2, 0, "ListCharString_move()::dst contained data, might indicate memleak"))

	allocated = true;
	Bool isListRef = ListCharString_isRef(*src);
	Bool anyRef = isListRef;

	if(!anyRef)
		for(U64 i = 0; i < src->length; ++i)
			if (CharString_isRef(src->ptr[i])) {
				anyRef = true;
				break;
			}

	if(!anyRef)
		*dst = *src;

	else {

		gotoIfError2(clean, ListCharString_resize(dst, src->length, alloc))

		for(U64 i = 0; i < src->length; ++i) {

			if(isListRef || CharString_isRef(src->ptr[i]))
				gotoIfError2(clean, CharString_createCopy(
					src->ptr[i], alloc, &dst->ptrNonConst[i]
				))

			else {
				dst->ptrNonConst[i] = src->ptr[i];
				src->ptrNonConst[i] = CharString_createNull();
			}
		}

		if(!ListCharString_isRef(*src))
			ListCharString_freeUnderlying(src, alloc);
	}

	*src = (ListCharString){ 0 };

clean:

	if(allocated && !s_uccess)
		ListCharString_freeUnderlying(dst, alloc);

	return s_uccess;
}

U64 CharString_calcStrLen(const C8 *ptr, U64 maxSize) {

	U64 i = 0;

	if(ptr)
		for(; i < maxSize && ptr[i]; ++i)
			;

	return i;
}

U64 CharString_hash(CharString s) {
	U64 hash = Buffer_fnv1a64Single(CharString_length(s), Buffer_fnv1a64Offset);
	return Buffer_fnv1a64(CharString_bufferConst(s), hash);
}

CharString CharString_getFilePath(CharString *str) {

	if(!CharString_formatPath(str))
		return CharString_createNull();

	CharString res = CharString_createNull();
	CharString_cutBefore(*str, '/', EStringCase_Sensitive, false, &res);
	return res;
}

CharString CharString_getBasePath(CharString *str) {

	if(!CharString_formatPath(str))
		return CharString_createNull();

	CharString res = CharString_createNull();
	CharString_cutAfter(*str, '/', EStringCase_Sensitive, false, &res);
	return res;
}

Error ListCharString_concat(ListCharString arr, C8 between, Allocator alloc, CharString *result) {

	U64 length = 0;

	for(U64 i = 0; i < arr.length; ++i)
		length += CharString_length(arr.ptr[i]);

	if(arr.length > 1)
		length += arr.length - 1;

	const Error err = CharString_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0, l = CharString_length(arr.ptr[i]); k < l; ++k)
			result->ptrNonConst[j++] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			result->ptrNonConst[j++] = between;
	}

	return Error_none();
}

Error ListCharString_concatString(ListCharString arr, CharString between, Allocator alloc, CharString *result) {

	U64 length = 0;

	for(U64 i = 0; i < arr.length; ++i)
		length += CharString_length(arr.ptr[i]);

	if(arr.length > 1)
		length += (arr.length - 1) * CharString_length(between);

	const Error err = CharString_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0, l = CharString_length(arr.ptr[i]); k < l; ++k)
			result->ptrNonConst[j++] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			for(U64 k = 0; k < CharString_length(between); ++k)
				result->ptrNonConst[j++] = between.ptr[k];
	}

	return Error_none();
}

Error CharString_popEndCount(CharString *s, U64 count) {
	return CharString_eraseAtCount(s, s ? CharString_length(*s) - count : 0, count);
}

Error CharString_popFrontCount(CharString *s, U64 count) { return CharString_eraseAtCount(s, 0, count); }

Error CharString_eraseAt(CharString *s, U64 i) { return CharString_eraseAtCount(s, i, 1); }
Error CharString_popFront(CharString *s) { return CharString_eraseAt(s, 0); }
Error CharString_popEnd(CharString *s) { return CharString_eraseAt(s, s ? CharString_length(*s) - 1 : 0); }

//Simple file utils

Bool CharString_formatPath(CharString *str) {
	return CharString_replaceAllSensitive(str, '\\', '/', 0, 0);
}

ECompareResult CharString_compare(CharString a, CharString b, EStringCase caseSensitive) {

	const U64 al = CharString_length(a);
	const U64 bl = CharString_length(b);

	//We want to sort on contents
	//Provided it's the same level of parenting.
	//This ensures things with the same parent also stay at the same location

	for (U64 i = 0; i < al && i < bl; ++i) {

		const C8 ai = C8_transform(a.ptr[i], (EStringTransform) caseSensitive);
		const C8 bi = C8_transform(b.ptr[i], (EStringTransform) caseSensitive);

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

//Format
//https://stackoverflow.com/questions/56331128/call-to-snprintf-and-vsnprintf-inside-a-variadic-function

#ifdef _WIN32
	#define calcFormatLen _vscprintf
#else
	int calcFormatLen(const char * format, va_list pargs) {
		int retval;
		va_list argcopy;
		va_copy(argcopy, pargs);
		retval = vsnprintf(NULL, 0, format, argcopy);
		va_end(argcopy);
		return retval;
	}
#endif

Error CharString_formatVariadic(Allocator alloc, CharString *result, const C8 *format, va_list args) {

	if(!result || !format)
		return Error_nullPointer(!result ? 1 : 2, "CharString_formatVariadic()::result and format are required");

	va_list arg2;
	va_copy(arg2, args);

	const int len = calcFormatLen(format, arg2);

	if(len < 0)
		return Error_stderr(0, "CharString_formatVariadic() len can't be <0");

	if (result->ptr)
		return Error_invalidOperation(0, "CharString_formatVariadic()::result isn't empty, could indicate memleak");

	if (len == 0) {
		*result = CharString_createNull();
		return Error_none();
	}

	const Error err = CharString_create('\0', (U64) len, alloc, result);

	if(err.genericError)
		return err;

	if(vsnprintf((C8*)result->ptr, len + 1, format, args) < 0) {
		CharString_free(result, alloc);
		return Error_stderr(0, "CharString_formatVariadic() vsnprintf failed");
	}

	return Error_none();
}

Error CharString_format(Allocator alloc, CharString *result, const C8 *format, ...) {

	if(!result || !format)
		return Error_nullPointer(!result ? 1 : 2, "CharString_format()::result and format are required");

	va_list arg1;
	va_start(arg1, format);
	const Error err = CharString_formatVariadic(alloc, result, format, arg1);
	va_end(arg1);

	return err;
}
