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

//types/container/string_unicode.c

#include "types/container/string.h"
#include "types/container/list_basic_types.h"

Bool CharString_createFromUTF16(const U16 *ptr, U64 limit, const Allocator *allocator, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	Bool alloc = false;

	gotoIfError3(clean, CharString_reserve(result, limit == U64_MAX ? 16 : (limit * 4 + 1), allocator, e_rr));
	alloc = true;

	UnicodeCodePointInfo codepoint = { 0 };
	U64 j = 0;

	const Buffer buf = Buffer_createRefConst(ptr, limit == U64_MAX ? ((U64)1 << 48) - 1 : limit * sizeof(U16));
	Buffer buf0 = CharString_allocatedBuffer(*result);

	for(U64 i = 0; i < limit * 2; ) {

		U16 c = ptr[i >> 1];

		if (!c) {
			result->lenAndNullTerminated = j | ((U64)1 << 63);
			break;
		}

		if(limit == U64_MAX) {
			gotoIfError3(clean, CharString_reserve(result, j + 5, allocator, e_rr));
			buf0 = CharString_allocatedBuffer(*result);
		}

		//Read as UTF16 encoding

		gotoIfError3(clean, Buffer_readAsUTF16(buf, i, &codepoint, e_rr));
		i += codepoint.bytes;

		//Write as UTF8 encoding

		gotoIfError3(clean, Buffer_writeAsUTF8(buf0, j, codepoint.index, &codepoint.bytes, e_rr));
		j += codepoint.bytes;
		result->lenAndNullTerminated = j | ((U64)1 << 63);
	}

	result->ptrNonConst[j] = '\0';

clean:

	if(!s_uccess && alloc)
		CharString_free(result, allocator);

	return s_uccess;
}

Bool CharString_createFromUTF32(const U32 *ptr, U64 limit, const Allocator *allocator, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	Bool alloc = false;

	gotoIfError3(clean, CharString_reserve(result, limit == U64_MAX ? 16 : (limit * 4 + 1), allocator, e_rr));
	alloc = true;

	UnicodeCodePointInfo codepoint = { 0 };
	U64 j = 0;

	Buffer buf0 = CharString_allocatedBuffer(*result);

	for(U64 i = 0; i < limit; ++i) {

		U32 c = ptr[i];

		if (!c) {
			result->lenAndNullTerminated = j | ((U64)1 << 63);
			break;
		}

		if(limit == U64_MAX) {
			gotoIfError3(clean, CharString_reserve(result, j + 5, allocator, e_rr));
			buf0 = CharString_allocatedBuffer(*result);
		}

		//Write as UTF8 encoding

		gotoIfError3(clean, Buffer_writeAsUTF8(buf0, j, c, &codepoint.bytes, e_rr));
		j += codepoint.bytes;
		result->lenAndNullTerminated = j | ((U64)1 << 63);
	}

	result->ptrNonConst[j] = '\0';

clean:

	if (!s_uccess && alloc)
		CharString_free(result, allocator);

	return s_uccess;
}

Bool CharString_toUTF16(const CharString s, const Allocator *allocator, ListU16 *arr, Error *e_rr) {

	Bool s_uccess = true;
	Bool alloc = false;

	const U64 len = CharString_length(s);

	if(!arr)
		retError(clean, Error_nullPointer(0, "CharString_toUTF16()::arr is required"));

	Bool isRef = arr->length && ListU16_isRef(*arr);

	if(!isRef) {
		gotoIfError3(clean, ListU16_reserve(arr, len + 1, allocator, e_rr));
		alloc = true;
	}

	else if(ListU16_isConstRef(*arr))
		retError(clean, Error_constData(0, 0, "CharString_toUTF16()::arr should be writable"));

	const Buffer buf0 = isRef ? ListU16_buffer(*arr) : ListU16_allocatedBuffer(*arr);
	const Buffer buf = CharString_bufferConst(s);
	UnicodeCodePointInfo codepoint = { 0 };

	U64 j = 0;

	for (U64 i = 0; i < len; ) {

		//Read as UTF8 encoding

		gotoIfError3(clean, Buffer_readAsUTF8(buf, i, &codepoint, e_rr));
		i += codepoint.bytes;

		//Write as UTF16 encoding

		gotoIfError3(clean, Buffer_writeAsUTF16(buf0, j, codepoint.index, &codepoint.bytes, e_rr));
		j += codepoint.bytes;
	}

	if(j + 2 > Buffer_length(buf0))
		retError(clean, Error_outOfBounds(0, j + 1, Buffer_length(buf0), "CharString_toUTF16() no space for null terminator"));

	arr->ptrNonConst[j / 2] = 0;
	arr->length = j / 2 + 1;

clean:

	if (!s_uccess && alloc)
		ListU16_free(arr, allocator);

	return s_uccess;
}

Bool CharString_toUTF32(const CharString s, const Allocator *allocator, ListU32 *arr, Error *e_rr) {

	Bool s_uccess = true;
	Bool alloc = false;

	const U64 len = CharString_length(s);
	gotoIfError3(clean, ListU32_reserve(arr, len + 1, allocator, e_rr));
	alloc = true;

	const Buffer buf = CharString_bufferConst(s);
	UnicodeCodePointInfo codepoint = { 0 };

	U32 *buf0 = arr->ptrNonConst;

	U64 j = 0;

	for (U64 i = 0; i < len; ) {

		gotoIfError3(clean, Buffer_readAsUTF8(buf, i, &codepoint, e_rr));	//Read as UTF8 encoding
		i += codepoint.bytes;

		buf0[j++] = codepoint.index;										//Write as UTF32 encoding
	}

	arr->ptrNonConst[j] = 0;
	arr->length = j;

clean:

	if (!s_uccess && alloc)
		ListU32_free(arr, allocator);

	return s_uccess;
}

U64 CharString_unicodeCodepoints(const CharString str) {

	U64 i = 0, j = 0;
	Buffer buf = CharString_bufferConst(str);

	while(i < Buffer_length(buf)) {

		UnicodeCodePointInfo codePoint = (UnicodeCodePointInfo) { 0 };

		if(!Buffer_readAsUTF8(buf, i, &codePoint, NULL))
			return U64_MAX;

		i += codePoint.bytes;
		++j;
	}

	return j;
}
