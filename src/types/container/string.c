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
#include "types/math/math.h"
#include "types/base/c8.h"
#include "types/base/constants.h"

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

Bool CharString_isValidFileName(const CharString str) {

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

	return !!strl;
}

Bool CharString_clear(CharString *str) {

	if(!str || CharString_isRef(*str) || !str->capacityAndRefInfo)
		return false;

	if(str->lenAndNullTerminated >> 63)					//If null terminated, we want to keep it null terminated
		((C8*)str->ptr)[0] = '\0';

	str->lenAndNullTerminated &= ~(((U64)1 << 63) - 1);		//Clear size
	return true;
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

	Buffer_memcpy(b, CharString_bufferConst(str));
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

	Buffer_memcpy(b, CharString_bufferConst(*str));

	b.ptrNonConst[length] = '\0';
	str->lenAndNullTerminated |= (U64)1 << 63;

	if (str->capacityAndRefInfo)
		alloc.free(alloc.ptr, Buffer_createManagedPtr(str->ptrNonConst, str->capacityAndRefInfo));

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

CharString CharString_newLine() { return CharString_createRefCStrConst("\n"); }

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

U64 CharString_hash(const CharString s) {
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

//Simple file utils

Bool CharString_formatPath(CharString *str) {
	return CharString_replaceAllSensitive(str, '\\', '/', 0, 0);
}

ECompareResult CharString_compare(const CharString *a, const CharString *b, EStringCase caseSensitive) {

	const U64 al = !a ? 0 : CharString_length(*a);
	const U64 bl = !b ? 0 : CharString_length(*b);

	//We want to sort on contents
	//Provided it's the same level of parenting.
	//This ensures things with the same parent also stay at the same location

	for (U64 i = 0; i < al && i < bl; ++i) {

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

Bool CharString_createFromETypeId(ETypeId type, Allocator alloc, CharString *result, Error *e_rr) {

	EDataType dataType = ETypeId_getDataType(type);
	EDataTypeStride dataTypeStride = ETypeId_getDataTypeStride(type);
	U8 w = ETypeId_getWidth(type);
	U8 h = ETypeId_getHeight(type);

	Bool s_uccess = true;
	const C8 *ptr = NULL;
	Bool createString = false;

	switch (dataType) {

		default:
			gotoIfError2(clean, CharString_createCopy(CharString_createRefCStrConst("C8"), alloc, result));
			createString = true;
			goto clean;

		case EDataType_Bool:			ptr = "B1";		break;
		case EDataType_UInt:			ptr = "U";		break;
		case EDataType_Int:				ptr = "I";		break;
		case EDataType_Float:			ptr = "F";		break;
	}

	gotoIfError2(clean, CharString_createCopy(CharString_createRefCStrConst(ptr), alloc, result));
	createString = true;

	if(dataType != EDataType_Bool) {

		switch(dataTypeStride) {
			default:					ptr = "8";		break;
			case EDataTypeStride_16:	ptr = "16";		break;
			case EDataTypeStride_32:	ptr = "32";		break;
			case EDataTypeStride_64:	ptr = "64";		break;
		}

		gotoIfError2(clean, CharString_appendString(result, CharString_createRefCStrConst(ptr), alloc));
	}

	if(w == 1 && h == 1)
		goto clean;

	gotoIfError2(clean, CharString_append(result, 'x', alloc));
	gotoIfError2(clean, CharString_append(result, C8_createDec(w), alloc));

	if(h == 1)
		goto clean;

	gotoIfError2(clean, CharString_append(result, 'x', alloc));
	gotoIfError2(clean, CharString_append(result, C8_createDec(h), alloc));

clean:

	if (createString && !s_uccess)
		CharString_free(result, alloc);

	return s_uccess;
}

Bool CharString_createFromETypeIdHLSL(
	ETypeId type,
	EHLSLStringifyFlags flags,
	Allocator alloc,
	CharString *result,
	Error *e_rr
) {

	EDataType dataType = ETypeId_getDataType(type);
	EDataTypeStride dataTypeStride = ETypeId_getDataTypeStride(type);
	U8 w = ETypeId_getWidth(type);
	U8 h = ETypeId_getHeight(type);

	Bool has16Bit	= flags & EHLSLStringifyFlags_Has16Bit;
	Bool hasF64		= flags & EHLSLStringifyFlags_HasF64;
	Bool hasInt64	= flags & EHLSLStringifyFlags_HasI64;
	Bool isStrict	= flags & EHLSLStringifyFlags_IsStrict;

	Bool s_uccess = true;
	const C8 *ptr = NULL;

	switch (dataType) {

		default:
			retError(clean, Error_invalidState(0, "CharString_createFromETypeIdHLSL() HLSL doesn't have a type for a char"))

		case EDataType_Bool:			ptr = "bool";	break;

		case EDataType_UInt:
		case EDataType_Int:
			
			switch(dataTypeStride) {

				default:

					if(isStrict)
						retError(clean, Error_invalidState(0, "CharString_createFromETypeIdHLSL() HLSL doesn't have xint8_t"))

					ptr = dataType == EDataType_UInt ? (has16Bit ? "uint16_t" : "uint") : (has16Bit ? "int16_t" : "int");
					break;

				case EDataTypeStride_16:

					if (isStrict && !has16Bit)
						retError(clean, Error_invalidState(
							0, "CharString_createFromETypeIdHLSL() HLSL doesn't have xint16_t enabled"
						))

					ptr = dataType == EDataType_UInt ? (has16Bit ? "uint16_t" : "uint") : (has16Bit ? "int16_t" : "int");
					break;

				case EDataTypeStride_32:
					ptr = dataType == EDataType_UInt ? "uint" : "int";
					break;

				case EDataTypeStride_64:

					if (!hasInt64)
						retError(clean, Error_invalidState(
							0, "CharString_createFromETypeIdHLSL() HLSL doesn't have xint64_t enabled"
						))

					ptr = dataType == EDataType_UInt ? "uint64_t" : "int64_t";
					break;
			}

			break;

		case EDataType_Float:
			
			switch(dataTypeStride) {

				default:
					ptr = "float";
					break;

				case EDataTypeStride_16:

					if (isStrict && !has16Bit)
						retError(clean, Error_invalidState(
							0, "CharString_createFromETypeIdHLSL() HLSL doesn't have float16_t enabled"
						))

					ptr = has16Bit ? "float16_t" : "float";
					break;

				case EDataTypeStride_64:

					if (!hasF64)
						retError(clean, Error_invalidState(
							0, "CharString_createFromETypeIdHLSL() HLSL doesn't have double enabled"
						))

					ptr = "double";
					break;
			}
			
			break;
	}

	gotoIfError2(clean, CharString_createCopy(CharString_createRefCStrConst(ptr), alloc, result))

	if(w == 1 && h == 1)
		goto clean;

	gotoIfError2(clean, CharString_append(result, C8_createDec(w), alloc))

	if(h == 1)
		goto clean;

	gotoIfError2(clean, CharString_append(result, 'x', alloc))
	gotoIfError2(clean, CharString_append(result, C8_createDec(h), alloc))

clean:
	return s_uccess;
}

ETypeId ETypeId_parseVecOrMat(CharString str, U8 off, EDataType type, EDataTypeStride stride) {

	U64 strl = CharString_length(str);

	if(str.ptr[off] != 'x' || (strl != ((U64)off + 2) && strl != ((U64)off + 4)))
		return ETypeId_Undefined;

	U8 w = C8_dec(str.ptr[off + 1]);

	if(w == U8_MAX || !w || w > 4)
		return ETypeId_Undefined;

	Bool needsMatrix = w == 1;

	Bool isMatrix = strl == ((U64)off + 4);
	U8 h = 1;

	if (isMatrix) {

		if(str.ptr[off + 2] != 'x')
			return ETypeId_Undefined;

		h = C8_dec(str.ptr[off + 3]);

		if(h == U8_MAX || h <= 1 || h > 4)
			return ETypeId_Undefined;
	}

	if(needsMatrix && !isMatrix)
		return ETypeId_Undefined;

	return makeTypeId(LIBRARYID_DEFAULT, 0, w, h, stride, type);
}

ETypeId ETypeId_parse(CharString str) {

	U64 strl = CharString_length(str);

	if(!strl)
		return ETypeId_Undefined;

	switch (str.ptr[0]) {

		case 'C':
			return strl != 2 || str.ptr[1] != '8' ? ETypeId_Undefined : ETypeId_C8;

		case 'F':
		case 'U':
		case 'I': {

			if (strl < 2)
				return ETypeId_Undefined;

			U8 v = C8_dec(str.ptr[1]);

			switch (v) {
				case 8: case 1: case 3: case 6:		break;	//8, 16, 32, 64
				default:							return ETypeId_Undefined;
			}

			if (v != 8 && strl == 2)
				return ETypeId_Undefined;

			U8 start = 2;

			if (v != 8) {

				U8 v2 = C8_dec(str.ptr[2]);

				switch (v2) {
					case 6: case 2: case 4:			break;
					default:						return ETypeId_Undefined;
				}

				v = v * 10 + v2;

				switch (v) {
					case 16: case 32: case 64:		break;
					default:						return ETypeId_Undefined;
				}

				++start;
			}

			else if (str.ptr[0] == 'F')
				return ETypeId_Undefined;

			EDataType dataType;

			switch (str.ptr[0]) {
				default:	dataType = EDataType_Float;		break;
				case 'U':	dataType = EDataType_UInt;		break;
				case 'I':	dataType = EDataType_Int;		break;
			}

			EDataTypeStride stride;

			switch (v) {
				default:	stride = EDataTypeStride_8;		break;
				case 16:	stride = EDataTypeStride_16;	break;
				case 32:	stride = EDataTypeStride_32;	break;
				case 64:	stride = EDataTypeStride_64;	break;
			}

			if(strl == start)
				return makeTypeId(LIBRARYID_DEFAULT, 0, 1, 1, stride, dataType);

			return ETypeId_parseVecOrMat(str, start, dataType, stride);
		}

		//B1, B1xN, B1xWxH
		case 'B':

			if(strl == 1 || str.ptr[1] != '1')
				return ETypeId_Undefined;

			return strl == 2 ? ETypeId_B1 : ETypeId_parseVecOrMat(str, 2, EDataType_Bool, EDataTypeStride_8);

		default:
			return ETypeId_Undefined;
	}
}
