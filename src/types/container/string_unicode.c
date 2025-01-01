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

#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

Error CharString_createFromUTF16(const U16 *ptr, U64 limit, Allocator allocator, CharString *result) {

	Error err = CharString_reserve(result, limit == U64_MAX ? 16 : (limit * 4 + 1), allocator);

	if (err.genericError)
		return err;

	UnicodeCodePointInfo codepoint = (UnicodeCodePointInfo) { 0 };
	U64 j = 0;

	const Buffer buf = Buffer_createRefConst(ptr, limit == U64_MAX ? ((U64)1 << 48) - 1 : limit * sizeof(U16));
	Buffer buf0 = CharString_allocatedBuffer(*result);

	for(U64 i = 0; i < limit * 2; ) {

		U16 c = ptr[i >> 1];

		if (!c)
			break;

		if(limit == U64_MAX) {
			gotoIfError(clean, CharString_reserve(result, j + 5, allocator))
			buf0 = CharString_allocatedBuffer(*result);
		}

		//Read as UTF16 encoding

		gotoIfError(clean, Buffer_readAsUTF16(buf, i, &codepoint))
		i += codepoint.bytes;

		//Write as UTF8 encoding

		gotoIfError(clean, Buffer_writeAsUTF8(buf0, j, codepoint.index, &codepoint.bytes))
		j += codepoint.bytes;
		result->lenAndNullTerminated = j | ((U64)1 << 63);
	}

	result->ptrNonConst[j] = '\0';

clean:

	if(err.genericError)
		CharString_free(result, allocator);

	return err;
}

Error CharString_createFromUTF32(const U32 *ptr, U64 limit, Allocator allocator, CharString *result) {

	Error err = CharString_reserve(result, limit == U64_MAX ? 16 : (limit * 4 + 1), allocator);

	if (err.genericError)
		return err;

	UnicodeCodePointInfo codepoint = (UnicodeCodePointInfo) { 0 };
	U64 j = 0;

	Buffer buf0 = CharString_allocatedBuffer(*result);

	for(U64 i = 0; i < limit; ++i) {

		U32 c = ptr[i];

		if (!c)
			break;

		if(limit == U64_MAX) {
			gotoIfError(clean, CharString_reserve(result, j + 5, allocator))
			buf0 = CharString_allocatedBuffer(*result);
		}

		//Write as UTF8 encoding

		gotoIfError(clean, Buffer_writeAsUTF8(buf0, j, c, &codepoint.bytes))
		j += codepoint.bytes;
		result->lenAndNullTerminated = j | ((U64)1 << 63);
	}

	result->ptrNonConst[j] = '\0';

clean:

	if(err.genericError)
		CharString_free(result, allocator);

	return err;
}

Error CharString_toUTF16(CharString s, Allocator allocator, ListU16 *arr) {

	const U64 len = CharString_length(s);
	Error err = ListU16_reserve(arr, len + 1, allocator);

	if (err.genericError)
		return err;

	const Buffer buf0 = ListU16_allocatedBuffer(*arr);
	const Buffer buf = CharString_bufferConst(s);
	UnicodeCodePointInfo codepoint = (UnicodeCodePointInfo) { 0 };

	U64 j = 0;

	for (U64 i = 0; i < len; ) {

		//Read as UTF8 encoding

		gotoIfError(clean, Buffer_readAsUTF8(buf, i, &codepoint))
		i += codepoint.bytes;

		//Write as UTF16 encoding

		gotoIfError(clean, Buffer_writeAsUTF16(buf0, j, codepoint.index, &codepoint.bytes))
		j += codepoint.bytes;
	}

	arr->ptrNonConst[j / 2] = 0;
	arr->length = j / 2 + 1;

clean:

	if (err.genericError)
		ListU16_free(arr, allocator);

	return err;
}

Error CharString_toUTF32(CharString s, Allocator allocator, ListU32 *arr) {

	const U64 len = CharString_length(s);
	Error err = ListU32_reserve(arr, len + 1, allocator);

	if (err.genericError)
		return err;

	const Buffer buf = CharString_bufferConst(s);
	UnicodeCodePointInfo codepoint = (UnicodeCodePointInfo) { 0 };

	U32 *buf0 = arr->ptrNonConst;

	U64 j = 0;

	for (U64 i = 0; i < len; ) {

		gotoIfError(clean, Buffer_readAsUTF8(buf, i, &codepoint))	//Read as UTF8 encoding
		i += codepoint.bytes;

		buf0[j++] = codepoint.index;								//Write as UTF32 encoding
	}

	arr->ptrNonConst[j] = 0;
	arr->length = j;

clean:

	if (err.genericError)
		ListU32_free(arr, allocator);

	return err;
}

U64 CharString_unicodeCodepoints(CharString str) {

	U64 i = 0, j = 0;
	Buffer buf = CharString_bufferConst(str);

	while(i < Buffer_length(buf)) {

		UnicodeCodePointInfo codePoint = (UnicodeCodePointInfo) { 0 };

		if(Buffer_readAsUTF8(buf, i, &codePoint).genericError)
			return U64_MAX;

		i += codePoint.bytes;
		++j;
	}

	return j;
}

Bool CharString_isValidUTF8(CharString str) {
	return Buffer_isUTF8(CharString_bufferConst(str), 1);
}
