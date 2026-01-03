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
#include "types/base/math.h"
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

//Simple checks (consts)

Bool CharString_create(C8 c, U64 size, const Allocator *alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;

	if (!alloc || !alloc->alloc)
		retError(clean, Error_nullPointer(2, "CharString_create()::alloc is required"));

	if (!result)
		retError(clean, Error_nullPointer(3, "CharString_create()::result is required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(0, "CharString_create()::result isn't empty, might indicate memleak"));

	if (size >> 48)
		retError(clean, Error_invalidOperation(1, "CharString_create()::size must be 48-bit"));

	if (!size) {
		*result = CharString_createNull();
		goto clean;
	}

	Buffer b = Buffer_createNull();
	gotoIfError3(clean, alloc->alloc(alloc->ptr, size + 1, &b, e_rr));

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

clean:
	return s_uccess;
}

Bool CharString_createCopy(const CharString str, const Allocator *alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;

	if (!alloc || !alloc->alloc || !result)
		retError(clean, Error_nullPointer(!result ? 2 : 1, "CharString_createCopy()::alloc and result are required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(0, "CharString_createCopy()::result wasn't empty, might indicate a memleak"));

	const U64 strl = CharString_length(str);

	if (!strl) {
		*result = CharString_createNull();
		goto clean;
	}

	Buffer b = Buffer_createNull();
	gotoIfError3(clean, alloc->alloc(alloc->ptr, strl + 1, &b, e_rr));

	Buffer_memcpy(b, CharString_bufferConst(str));
	b.ptrNonConst[strl] = '\0';

	*result = (CharString) {
		.ptrNonConst = (C8*) b.ptrNonConst,
		.lenAndNullTerminated = (strl | ((U64)1 << 63)),
		.capacityAndRefInfo = strl + 1
	};

clean:
	return s_uccess;
}

void CharString_free(CharString *str, const Allocator *alloc) {

	if (!str || !alloc || !alloc->free)
		return;

	if(!CharString_isRef(*str))
		alloc->free(alloc->ptr, Buffer_createManagedPtr((U8*)str->ptr, str->capacityAndRefInfo));

	*str = CharString_createNull();
}

Bool CharString_split(const CharStringSplit *split, C8 c, EStringCase casing, Error *e_rr) {

	Bool s_uccess = true;

	if (!split || !split->s)
		retError(clean, Error_nullPointer(!split ? 0 : 1, "CharString_split()::split and split->s are required"));

	const CharStringSensOff countAll = { split->s, casing, 0 };

	const CharString *s = split->s;
	const U64 length = CharString_countAll(&countAll, c);

	gotoIfError3(clean, ListCharString_create(length + 1, split->allocator, split->result, e_rr));

	const U64 strl = CharString_length(*s);
	const Bool b = CharString_isNullTerminated(*s);
	const Bool isConstRef = CharString_isConstRef(*s);
	const C8 *sPtr = s->ptr;
	C8 *sPtrNonConst = s->ptrNonConst;

	const ListCharString str = *split->result;
	EStringTransform transform = (EStringTransform) casing;

	if (!length) {

		str.ptrNonConst[0] = isConstRef ? CharString_createRefSizedConst(sPtr, strl, b) :
			CharString_createRefSized(sPtrNonConst, strl, b);

		goto clean;
	}

	c = C8_transform(c, transform);

	U64 count = 0, last = 0;

	for (U64 i = 0; i < strl; ++i)
		if (C8_transform(sPtr[i], transform) == c) {

			str.ptrNonConst[count++] =
				isConstRef ? CharString_createRefSizedConst(sPtr + last, i - last, false) :
				CharString_createRefSized(sPtrNonConst + last, i - last, false);

			last = i + 1;
		}

	str.ptrNonConst[count++] =
		isConstRef ? CharString_createRefSizedConst(sPtr + last, strl - last, b) :
		CharString_createRefSized(sPtrNonConst + last, strl - last, b);

clean:
	return s_uccess;
}

Bool CharString_splitString(const CharStringSplit *split, const CharString *other, EStringCase casing, Error *e_rr) {

	Bool s_uccess = true;

	if (!split || !split->s || !other)
		retError(clean, Error_nullPointer(!split || !split->s ? 0 : 1, "CharString_splitString()::s and other are required"));

	const CharStringSensOff countAll = { split->s, casing, 0 };

	const CharString *s = split->s;
	const U64 length = CharString_countAllString(&countAll, other);

	gotoIfError3(clean, ListCharString_create(length + 1, split->allocator, split->result, e_rr));

	const Bool b = CharString_isNullTerminated(*s);
	const U64 strl = CharString_length(*s);
	const U64 otherl = CharString_length(*other);
	const Bool isConstRef = CharString_isConstRef(*s);

	const C8 *otherPtr = other->ptr;
	const C8 *sPtr = s->ptr;
	C8 *sPtrNonConst = s->ptrNonConst;

	EStringTransform transform = (EStringTransform) casing;
	const ListCharString str = *split->result;

	if (!length) {

		str.ptrNonConst[0] = isConstRef ? CharString_createRefSizedConst(sPtr, strl, b) :
			CharString_createRefSized(sPtrNonConst, strl, b);

		goto clean;
	}

	U64 count = 0, last = 0;

	for (U64 i = 0; i < strl - otherl + 1; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
			if (C8_transform(sPtr[j], transform) != C8_transform(otherPtr[k], transform)) {
				match = false;
				break;
			}

		if (match) {

			str.ptrNonConst[count++] =
				isConstRef ? CharString_createRefSizedConst(sPtr + last, i - last, false) :
				CharString_createRefSized(sPtrNonConst + last, i - last, false);

			last = i + otherl;
			i += otherl - 1;
		}
	}

	str.ptrNonConst[count++] =
		isConstRef ? CharString_createRefSizedConst(sPtr + last, strl - last, b) :
		CharString_createRefSized(sPtrNonConst + last, strl - last, b);

clean:
	return s_uccess;
}

Bool CharString_splitLine(const CharString s, const Allocator *alloc, ListCharString *result, Error *e_rr) {

	Bool s_uccess = true;

	if (!result)
		retError(clean, Error_nullPointer(2, "CharString_splitLine()::result is invalid"));

	if (result->ptr)
		retError(clean, Error_invalidParameter(2, 1, "CharString_splitLine()::result wasn't empty, might indicate memleak"));

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

	gotoIfError3(clean, ListCharString_create(v, alloc, result, e_rr));

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

clean:
	return s_uccess;
}

Bool CharString_reserve(CharString *str, U64 length, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!str)
		retError(clean, Error_nullPointer(0, "CharString_reserve()::str is required"));

	if (CharString_isRef(*str) && CharString_length(*str))
		retError(clean, Error_invalidOperation(0, "CharString_reserve()::str has to be managed memory"));

	if (length >> 48)
		retError(clean, Error_invalidOperation(1, "CharString_reserve()::length should be 48-bit"));

	if (!alloc || !alloc->alloc || !alloc->free)
		retError(clean, Error_nullPointer(2, "CharString_reserve()::alloc is required"));

	if (length + 1 <= str->capacityAndRefInfo)
		goto clean;

	Buffer b = Buffer_createNull();
	gotoIfError3(clean, alloc->alloc(alloc->ptr, length + 1, &b));

	Buffer_memcpy(b, CharString_bufferConst(*str));

	b.ptrNonConst[length] = '\0';
	str->lenAndNullTerminated |= (U64)1 << 63;

	if (str->capacityAndRefInfo)
		alloc->free(alloc->ptr, Buffer_createManagedPtr(str->ptrNonConst, str->capacityAndRefInfo));

	str->capacityAndRefInfo = Buffer_length(b);
	str->ptr = (const C8*) b.ptr;

clean:
	return s_uccess;
}

Bool CharString_resize(CharString *str, U64 length, C8 defaultChar, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!str)
		retError(clean, Error_nullPointer(0, "CharString_resize()::str is required"));

	const U64 strl = CharString_length(*str);

	if (CharString_isRef(*str) && strl)
		retError(clean, Error_invalidOperation(0, "CharString_resize()::str needs to be managed memory"));

	if (length >> 48)
		retError(clean, Error_invalidOperation(1, "CharString_resize()::length should be 48-bit"));

	if (!alloc || !alloc->alloc)
		retError(clean, Error_nullPointer(3, "CharString_resize()::alloc is required"));

	if (length == strl && CharString_isNullTerminated(*str))
		goto clean;

	//Simple resize; we cut off the tail

	if (length < strl) {
		str->lenAndNullTerminated = ((U64)1 << 63) | length;
		str->ptrNonConst[length] = '\0';
		goto clean;
	}

	//Resize that triggers buffer resize
	//Reserve 50% more to ensure we don't resize too many times

	if (length + 1 > str->capacityAndRefInfo)
		gotoIfError3(clean, CharString_reserve(str, U64_max(64, length * 3 / 2) + 1, alloc, e_rr));

	//Our capacity is big enough to handle it:

	for (U64 i = strl; i < length; ++i)
		str->ptrNonConst[i] = defaultChar;

	str->ptrNonConst[length] = '\0';
	str->lenAndNullTerminated = length | ((U64)1 << 63);

clean:
	return s_uccess;
}

Bool CharString_findAll(const CharStringFind *find, C8 c, EStringCase caseSensitive, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = NULL;

	if (!find || !find->result)
		retError(clean, Error_nullPointer(0, "CharString_findAll()::find and find->result are required"));

	if(find->result->ptr)
		retError(clean, Error_invalidParameter(6, 0, "CharString_findAll()::result wasn't empty, might indicate memleak"));

	U64 strl = !find->s ? 0 : CharString_length(*find->s);
	const U64 off = find->off;
	const U64 len = find->len;

	if(off >= strl || off + len > strl)
		retError(clean, Error_invalidParameter(4, 0, "CharString_findAll()::off or len out of bounds"));

	const C8 *sPtr = find->s->ptr;

	if(len)
		strl = off + len;

	ListU64 l = (ListU64) { 0 };
	gotoIfError3(clean, ListU64_reserve(&l, (strl - off) / 25 + 16, find->alloc, e_rr));
	alloc = find->alloc;

	EStringTransform transform = (EStringTransform) caseSensitive;

	c = C8_transform(c, transform);

	for (U64 i = off; i < strl; ++i)
		if (c == C8_transform(sPtr[i], transform))
			gotoIfError3(clean, ListU64_pushBack(&l, i, alloc, e_rr));

	*find->result = l;

clean:

	if(!s_uccess && alloc)
		ListU64_free(&l, alloc);

	return s_uccess;
}

Bool CharString_findAllString(const CharStringFind *find, const CharString *other, EStringCase caseSensitive, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = NULL;

	if (!find || !find->result)
		retError(clean, Error_nullPointer(0, "CharString_findAllString()::find and find->result are required"));

	if(find->result->ptr)
		retError(clean, Error_invalidParameter(
			6, 0, "CharString_findAllString()::result wasn't empty, might indicate memleak"
		));

	const U64 otherl = !other ? 0 : CharString_length(*other);
	U64 strl = !find->s ? 0 : CharString_length(*find->s);
	const U64 off = find->off;
	const U64 len = find->len;

	if(!otherl)
		retError(clean, Error_invalidParameter(1, 0, "CharString_findAllString()::other is empty"));

	if(off >= strl || off + len > strl)
		retError(clean, Error_invalidParameter(4, 0, "CharString_findAllString()::off or len is out of bounds"));

	const C8 *sPtr = find->s->ptr;
	const C8 *otherPtr = other->ptr;

	if(len)
		strl = off + len;

	ListU64 l = (ListU64) { 0 };

	if((strl - off) < otherl) {
		*find->result = l;
		goto clean;
	}

	gotoIfError3(clean, ListU64_reserve(&l, (strl - off) / otherl / 25 + 16, find->alloc, e_rr));
	alloc = find->alloc;

	EStringTransform transform = (EStringTransform) caseSensitive;

	for (U64 i = off; i < strl; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
			if (C8_transform(sPtr[j], transform) != C8_transform(otherPtr[k], transform)) {
				match = false;
				break;
			}

		if (match) {
			gotoIfError3(clean, ListU64_pushBack(&l, i, alloc, e_rr));
			i += otherl - 1;
		}
	}

	*find->result = l;

clean:

	if (!s_uccess && alloc)
		ListU64_free(&l, alloc);

	return s_uccess;
}

void ListCharString_freeUnderlying(ListCharString *arr, const Allocator *alloc) {

	if(!arr || !ListCharString_allocatedBytes(*arr))
		return true;

	for(U64 i = 0; i < arr->length; ++i) {
		CharString *str = arr->ptrNonConst + i;
		CharString_free(str, alloc);
	}

	ListCharString_free(arr, alloc);
}

Bool ListCharString_createCopyUnderlying(
	const ListCharString *toCopy, const Allocator *alloc, ListCharString *arr, Error *e_rr
) {

	Bool s_uccess = true;
	Bool alloc = false;

	if (!toCopy)
		retError(clean, Error_nullPointer(0, "ListCharString_createCopyUnderlying()::toCopy is required"));

	U64 toCopyl = toCopy->length;

	if(!toCopyl) {

		if(!arr)
			retError(clean, Error_nullPointer(3, "ListCharString_createCopyUnderlying()::arr is required"));

		if (arr->ptr)
			retError(clean, Error_invalidOperation(
				0, "ListCharString_createCopyUnderlying()::arr wasn't empty, which might indicate memleak"
			));

		*arr = (ListCharString) { 0 };
		goto clean;
	}

	gotoIfError3(clean, ListCharString_create(toCopyl, alloc, arr, e_rr));
	alloc = true;

	for (U64 i = 0; i < toCopyl; ++i)
		gotoIfError3(clean, CharString_createCopy(toCopy->ptr[i], alloc, arr->ptrNonConst + i, e_rr));

clean:

	if(!s_uccess && alloc)
		ListCharString_freeUnderlying(arr, alloc);

	return s_uccess;
}

Bool ListCharString_move(ListCharString *src, const Allocator *alloc, ListCharString *dst, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if (!src || !dst)
		retError(clean, Error_nullPointer(!src ? 0 : 2, "ListCharString_move()::src and dst are required"));

	if(dst->ptr)
		retError(clean, Error_invalidParameter(2, 0, "ListCharString_move()::dst contained data, might indicate memleak"));

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

		gotoIfError3(clean, ListCharString_resize(dst, src->length, alloc, e_rr));

		for(U64 i = 0; i < src->length; ++i) {

			if (isListRef || CharString_isRef(src->ptr[i])) {
				gotoIfError3(clean, CharString_createCopy(
					src->ptr[i], alloc, &dst->ptrNonConst[i], e_rr
				));
			}

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

Bool ListCharString_concat(const ListCharStringConcat *conc, C8 between, Error *e_rr) {

	Bool s_uccess = true;

	if (!conc || !conc->arr)
		retError(clean, Error_nullPointer(0, "ListCharString_concat()::conc and conc->arr are required"));

	const U64 arrl = conc->arr->length;
	const CharString *arrPtr = conc->arr->ptr;

	CharString *result = conc->result;

	U64 length = 0;

	for(U64 i = 0; i < arrl; ++i)
		length += CharString_length(arrPtr[i]);

	if(arrl > 1)
		length += arrl - 1;

	gotoIfError3(clean, CharString_create(' ', length, conc->alloc, result, e_rr));

	for(U64 i = 0, j = 0; i < arrl; ++i) {

		for(U64 k = 0, l = CharString_length(arrPtr[i]); k < l; ++k)
			result->ptrNonConst[j++] = arrPtr[i].ptr[k];

		if (i != arrl - 1)
			result->ptrNonConst[j++] = between;
	}

clean:
	return s_uccess;
}

Bool ListCharString_concatString(const ListCharStringConcat *conc, const CharString *between, Error *e_rr) {

	Bool s_uccess = true;

	if (!conc || !conc->arr)
		retError(clean, Error_nullPointer(0, "ListCharString_concatString()::conc and conc->arr are required"));

	const U64 betweenl = !between ? 0 : CharString_length(*between);
	const U64 arrl = conc->arr->length;
	const CharString *arrPtr = conc->arr->ptr;
	const C8 *betweenPtr = !between ? NULL : between->ptr;
	U64 length = 0;

	CharString *result = conc->result;

	for(U64 i = 0; i < arrl; ++i)
		length += CharString_length(arrPtr[i]);

	if(arrl > 1)
		length += (arrl - 1) * betweenl;

	gotoIfError3(clean, CharString_create(' ', length, conc->alloc, result, e_rr));

	for(U64 i = 0, j = 0; i < arrl; ++i) {

		for(U64 k = 0, l = CharString_length(arrPtr[i]); k < l; ++k)
			result->ptrNonConst[j++] = arrPtr[i].ptr[k];

		if (i != arrl - 1)
			for(U64 k = 0; k < betweenl; ++k)
				result->ptrNonConst[j++] = betweenPtr[k];
	}

clean:
	return s_uccess;
}

//Format
//https://stackoverflow.com/questions/56331128/call-to-snprintf-and-vsnprintf-inside-a-variadic-function

#ifdef _WIN32
	#define calcFormatLen _vscprintf
#else
	int calcFormatLen(const char *format, va_list pargs) {
		int retval;
		va_list argcopy;
		va_copy(argcopy, pargs);
		retval = vsnprintf(NULL, 0, format, argcopy);
		va_end(argcopy);
		return retval;
	}
#endif

Bool CharString_formatVariadic(const Allocator *alloc, CharString *result, Error *e_rr, const C8 *format, va_list args) {

	Bool s_uccess = true;

	if(!result || !format)
		retError(clean, Error_nullPointer(!result ? 1 : 2, "CharString_formatVariadic()::result and format are required"));

	va_list arg2;
	va_copy(arg2, args);

	const int len = calcFormatLen(format, arg2);

	if(len < 0)
		retError(clean, Error_stderr(0, "CharString_formatVariadic() len can't be <0"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(0, "CharString_formatVariadic()::result isn't empty, could indicate memleak"));

	if (len == 0) {
		*result = CharString_createNull();
		goto clean;
	}

	gotoIfError3(clean, CharString_create('\0', (U64) len, alloc, result, e_rr));

	if(vsnprintf((C8*)result->ptr, len + 1, format, args) < 0) {
		CharString_free(result, alloc);
		retError(clean, Error_stderr(0, "CharString_formatVariadic() vsnprintf failed"));
	}

clean:
	return s_uccess;
}

Bool CharString_format(const Allocator *alloc, CharString *result, Error *e_rr, const C8 *format, ...) {

	Bool s_uccess = true;

	if(!result || !format)
		retError(clean, Error_nullPointer(!result ? 1 : 2, "CharString_format()::result and format are required"));

	va_list arg1;
	va_start(arg1, format);
	gotoIfError3(clean, CharString_formatVariadic(alloc, result, e_rr, format, arg1));
	va_end(arg1);

clean:
	return s_uccess;
}

Bool CharString_createFromETypeId(ETypeId type, const Allocator *alloc, CharString *result, Error *e_rr) {

	EDataType dataType = ETypeId_getDataType(type);
	EDataTypeStride dataTypeStride = ETypeId_getDataTypeStride(type);
	U8 w = ETypeId_getWidth(type);
	U8 h = ETypeId_getHeight(type);

	Bool s_uccess = true;
	const C8 *ptr = NULL;
	Bool createString = false;

	switch (dataType) {

		default:
			gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst("C8"), alloc, result, e_rr));
			createString = true;
			goto clean;

		case EDataType_Bool:			ptr = "B1";		break;
		case EDataType_UInt:			ptr = "U";		break;
		case EDataType_Int:				ptr = "I";		break;
		case EDataType_Float:			ptr = "F";		break;
	}

	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(ptr), alloc, result, e_rr));
	createString = true;

	if(dataType != EDataType_Bool) {

		switch(dataTypeStride) {
			default:					ptr = "8";		break;
			case EDataTypeStride_16:	ptr = "16";		break;
			case EDataTypeStride_32:	ptr = "32";		break;
			case EDataTypeStride_64:	ptr = "64";		break;
		}

		CharString other = CharString_createRefCStrConst(ptr);
		gotoIfError3(clean, CharString_appendString(result, &other, alloc, e_rr));
	}

	if(w == 1 && h == 1)
		goto clean;

	gotoIfError3(clean, CharString_append(result, 'x', alloc, e_rr));
	gotoIfError3(clean, CharString_append(result, C8_createDec(w), alloc, e_rr));

	if(h == 1)
		goto clean;

	gotoIfError3(clean, CharString_append(result, 'x', alloc, e_rr));
	gotoIfError3(clean, CharString_append(result, C8_createDec(h), alloc, e_rr));

clean:

	if (createString && !s_uccess)
		CharString_free(result, alloc);

	return s_uccess;
}

Bool CharString_createFromETypeIdHLSL(
	ETypeId type,
	EHLSLStringifyFlags flags,
	const Allocator *alloc,
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
			retError(clean, Error_invalidState(0, "CharString_createFromETypeIdHLSL() HLSL doesn't have a type for a char"));

		case EDataType_Bool:			ptr = "bool";	break;

		case EDataType_UInt:
		case EDataType_Int:
			
			switch(dataTypeStride) {

				default:

					if(isStrict)
						retError(clean, Error_invalidState(0, "CharString_createFromETypeIdHLSL() HLSL doesn't have xint8_t"));

					ptr = dataType == EDataType_UInt ? (has16Bit ? "uint16_t" : "uint") : (has16Bit ? "int16_t" : "int");
					break;

				case EDataTypeStride_16:

					if (isStrict && !has16Bit)
						retError(clean, Error_invalidState(
							0, "CharString_createFromETypeIdHLSL() HLSL doesn't have xint16_t enabled"
						));

					ptr = dataType == EDataType_UInt ? (has16Bit ? "uint16_t" : "uint") : (has16Bit ? "int16_t" : "int");
					break;

				case EDataTypeStride_32:
					ptr = dataType == EDataType_UInt ? "uint" : "int";
					break;

				case EDataTypeStride_64:

					if (!hasInt64)
						retError(clean, Error_invalidState(
							0, "CharString_createFromETypeIdHLSL() HLSL doesn't have xint64_t enabled"
						));

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
						));

					ptr = has16Bit ? "float16_t" : "float";
					break;

				case EDataTypeStride_64:

					if (!hasF64)
						retError(clean, Error_invalidState(
							0, "CharString_createFromETypeIdHLSL() HLSL doesn't have double enabled"
						));

					ptr = "double";
					break;
			}
			
			break;
	}

	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(ptr), alloc, result, e_rr));

	if(w == 1 && h == 1)
		goto clean;

	gotoIfError3(clean, CharString_append(result, C8_createDec(w), alloc, e_rr));

	if(h == 1)
		goto clean;

	gotoIfError3(clean, CharString_append(result, 'x', alloc, e_rr));
	gotoIfError3(clean, CharString_append(result, C8_createDec(h), alloc, e_rr));

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

ETypeId ETypeId_parse(const CharString str) {

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

Bool ListCharString_combine(const ListCharStringConcat *concat, Error *e_rr) {
	return ListCharString_concatString(concat, NULL, e_rr);
}

//TODO: Clean this somehow..

#define CharString_createNum(maxVal, func, prefixRaw, ...)																	\
																															\
	Bool s_uccess = true;																									\
	Allocator *allocator = NULL;																							\
																															\
	if (!number || !number->result)																							\
		retError(clean, Error_nullPointer(3, "CharString_createNum()::number and number->result are required"));			\
																															\
	CharString *result = number->result;																					\
	CharString prefix = CharString_createRefCStrConst(prefixRaw);															\
																															\
	if (result->ptr)																										\
		retError(clean, Error_invalidOperation(0, "CharString_createNum()::result wasn't empty, might indicate memleak"));	\
																															\
	gotoIfError3(clean, CharString_reserve(																					\
		result, maxVal + CharString_length(prefix) + 1, number->allocator, e_rr												\
	));																														\
																															\
	allocator = number->allocator;																							\
																															\
	gotoIfError3(clean, CharString_appendString(result, &prefix, allocator, e_rr));											\
																															\
	U64 v = number->v;																										\
	U8 leadingZeros = number->leadingZeros;																					\
																															\
	Bool foundFirstNonZero = false;																							\
																															\
	for (U64 i = maxVal - 1; i != U64_MAX; --i) {																			\
																															\
		C8 c = C8_create##func(__VA_ARGS__);																				\
																															\
		if (!foundFirstNonZero)																								\
			foundFirstNonZero = c != '0' || i < leadingZeros;																\
																															\
		if (foundFirstNonZero)																								\
			gotoIfError3(clean, CharString_append(result, c, allocator, e_rr));												\
	}																														\
																															\
	/* Ensure we don't return an empty string on 0 */																		\
																															\
	if (!v && !foundFirstNonZero)																							\
		gotoIfError3(clean, CharString_append(result, '0', allocator, e_rr));												\
																															\
	result->ptrNonConst[CharString_length(*result)] = '\0';																	\
clean:																														\
	if(allocator && !s_uccess) CharString_free(result, allocator);																\
	return s_uccess;

Bool CharString_createNyto(const CharStringCreateNumber *number, Error *e_rr) {
	CharString_createNum(11, Nyto, "0n", (v >> (6 * i)) & 63);
}

Bool CharString_createHex(const CharStringCreateNumber *number, Error *e_rr) {
	CharString_createNum(16, Hex, "0x", (v >> (4 * i)) & 15);
}

Bool CharString_createDec(const CharStringCreateNumber *number, Error *e_rr) {
	CharString_createNum(20, Dec, "", (v / U64_exp10(i)) % 10);
}

Bool CharString_createOct(const CharStringCreateNumber *number, Error *e_rr) {
	CharString_createNum(22, Oct, "0o", (v >> (3 * i)) & 7);
}

Bool CharString_createBin(const CharStringCreateNumber *number, Error *e_rr) {
	CharString_createNum(64, Bin, "0b", (v >> i) & 1);
}
