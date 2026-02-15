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

#pragma once
#include "platforms/platform.h"
#include "types/container/string_unicode.h"
#include "types/container/string_helper.h"

#ifdef __cplusplus
	extern "C" {
#endif

static inline Bool CharString_createx(C8 c, U64 size, CharString *result, Error *e_rr) {
	return CharString_create(c, size, Platform_instance->alloc, result, e_rr);
}

static inline Bool CharString_createCopyx(const CharString str, CharString *result, Error *e_rr) {
	return CharString_createCopy(str, Platform_instance->alloc, result, e_rr);
}

static inline Bool CharString_createNytox(U64 v, U8 leadingZeros, CharString *result, Error *e_rr) {
	const CharStringCreateNumber number = {
		.v = v,
		.leadingZeros = leadingZeros,
		.allocator = Platform_instance->alloc,
		.result = result
	};
	return CharString_createNyto(&number, e_rr);
}

static inline Bool CharString_createHexx(U64 v, U8 leadingZeros, CharString *result, Error *e_rr) {
	const CharStringCreateNumber number = {
		.v = v,
		.leadingZeros = leadingZeros,
		.allocator = Platform_instance->alloc,
		.result = result
	};
	return CharString_createHex(&number, e_rr);
}

static inline Bool CharString_createDecx(U64 v, U8 leadingZeros, CharString *result, Error *e_rr) {
	const CharStringCreateNumber number = {
		.v = v,
		.leadingZeros = leadingZeros,
		.allocator = Platform_instance->alloc,
		.result = result
	};
	return CharString_createDec(&number, e_rr);
}

static inline Bool CharString_createOctx(U64 v, U8 leadingZeros, CharString *result, Error *e_rr) {
	const CharStringCreateNumber number = {
		.v = v,
		.leadingZeros = leadingZeros,
		.allocator = Platform_instance->alloc,
		.result = result
	};
	return CharString_createOct(&number, e_rr);
}

static inline Bool CharString_createBinx(U64 v, U8 leadingZeros, CharString *result, Error *e_rr) {
	const CharStringCreateNumber number = {
		.v = v,
		.leadingZeros = leadingZeros,
		.allocator = Platform_instance->alloc,
		.result = result
	};
	return CharString_createBin(&number, e_rr);
}

static inline Bool CharString_createFromUTF16x(const U16 *ptr, U64 max, CharString *result, Error *e_rr) {
	return CharString_createFromUTF16(ptr, max, Platform_instance->alloc, result, e_rr);
}

static inline Bool CharString_createFromUTF32x(const U32 *ptr, U64 max, CharString *result, Error *e_rr) {
	return CharString_createFromUTF32(ptr, max, Platform_instance->alloc, result, e_rr);
}

static inline Bool CharString_toUTF16x(const CharString s, ListU16 *arr, Error *e_rr) {
	return CharString_toUTF16(s, Platform_instance->alloc, arr, e_rr);
}

static inline Bool CharString_toUTF32x(const CharString s, ListU32 *arr, Error *e_rr) {
	return CharString_toUTF32(s, Platform_instance->alloc, arr, e_rr);
}

static inline Bool CharString_splitx(const CharString *s, C8 c,  EStringCase casing, ListCharString *result, Error *e_rr) {
	const CharStringSplit split = { s, Platform_instance->alloc, result };
	return CharString_split(&split, c, casing, e_rr);
}

static inline Bool CharString_splitStringx(
	const CharString *s,
	const CharString *other,
	EStringCase casing,
	ListCharString *result,
	Error *e_rr
) {
	const CharStringSplit split = { s, Platform_instance->alloc, result };
	return CharString_splitString(&split, other, casing, e_rr);
}

static inline Bool CharString_splitSensitivex(const CharString *s, C8 c, ListCharString *result, Error *e_rr) {
	const CharStringSplit split = { s, Platform_instance->alloc, result };
	return CharString_splitSensitive(&split, c, e_rr);
}

static inline Bool CharString_splitInsensitivex(const CharString *s, C8 c, ListCharString *result, Error *e_rr) {
	const CharStringSplit split = { s, Platform_instance->alloc, result };
	return CharString_splitInsensitive(&split, c, e_rr);
}

static inline Bool CharString_splitStringSensitivex(
	const CharString *s,
	const CharString *other,
	ListCharString *result,
	Error *e_rr
) {
	const CharStringSplit split = { s, Platform_instance->alloc, result };
	return CharString_splitStringSensitive(&split, other, e_rr);
}

static inline Bool CharString_splitStringInsensitivex(
	const CharString *s,
	const CharString *other,
	ListCharString *result,
	Error *e_rr
) {
	const CharStringSplit split = { s, Platform_instance->alloc, result };
	return CharString_splitStringInsensitive(&split, other, e_rr);
}

static inline Bool CharString_splitLinex(const CharString s, ListCharString *result, Error *e_rr) {
	return CharString_splitLine(s, Platform_instance->alloc, result, e_rr);
}

static inline Bool CharString_resizex(CharString *str, U64 length, C8 defaultChar, Error *e_rr) {
	return CharString_resize(str, length, defaultChar, Platform_instance->alloc, e_rr);
}

static inline Bool CharString_reservex(CharString *str, U64 length, Error *e_rr) {
	return CharString_reserve(str, length, Platform_instance->alloc, e_rr);
}

static inline Bool CharString_appendx(CharString *s, C8 c, Error *e_rr) {
	return CharString_append(s, c, Platform_instance->alloc, e_rr);
}

static inline Bool CharString_appendStringx(CharString *s, const CharString *other, Error *e_rr) {
	return CharString_appendString(s, other, Platform_instance->alloc, e_rr);
}

static inline Bool CharString_prependx(CharString *s, C8 c, Error *e_rr) {
	return CharString_prepend(s, c, Platform_instance->alloc, e_rr);
}

static inline Bool CharString_prependStringx(CharString *s, const CharString *other, Error *e_rr) {
	return CharString_prependString(s, other, Platform_instance->alloc, e_rr);
}

static inline Bool CharString_insertx(CharString *s, C8 c, U64 i, Error *e_rr) {
	return CharString_insert(s, c, i, Platform_instance->alloc, e_rr);
}

static inline Bool CharString_insertStringx(CharString *s, const CharString *other, U64 i, Error *e_rr) {
	return CharString_insertString(s, other, i, Platform_instance->alloc, e_rr);
}

typedef struct CharStringReplace3 {
	CharString *s;
	const CharString *search;
	const CharString *replace;
	U64 off;
	U64 len;
} CharStringReplace3;

static inline Bool CharString_replaceAllStringx(const CharStringReplace3 *replace, EStringCase caseSensitive, Error *e_rr) {

	Bool s_uccess = true;

	if (!replace)
		retError(clean, Error_nullPointer(0, "CharString_replaceAllStringx()::replace is required"));

	const CharStringReplace2 replace2 = {
		replace->s,
		replace->search,
		replace->replace,
		Platform_instance->alloc,
		replace->off,
		replace->len
	};

	gotoIfError3(clean, CharString_replaceAllString(&replace2, caseSensitive, e_rr));
clean:
	return s_uccess;
}

static inline Bool CharString_replaceStringx(
	const CharStringReplace3 *replace,
	Bool isFirst,
	EStringCase caseSensitive,
	Error *e_rr
) {
	Bool s_uccess = true;

	if (!replace)
		retError(clean, Error_nullPointer(0, "CharString_replaceStringx()::replace is required"));

	const CharStringReplace2 replace2 = {
		replace->s,
		replace->search,
		replace->replace,
		Platform_instance->alloc,
		replace->off,
		replace->len
	};

	gotoIfError3(clean, CharString_replaceString(&replace2, isFirst, caseSensitive, e_rr));

clean:
	return s_uccess;
}

static inline Bool CharString_replaceFirstStringx(const CharStringReplace3 *replace, EStringCase caseSensitive, Error *e_rr) {
	return CharString_replaceStringx(replace, true, caseSensitive, e_rr);
}

static inline Bool CharString_replaceLastStringx(const CharStringReplace3 *replace, EStringCase caseSensitive, Error *e_rr) {
	return CharString_replaceStringx(replace, false, caseSensitive, e_rr);
}

static inline Bool CharString_replaceAllStringSensitivex(const CharStringReplace3 *replace, Error *e_rr) {
	return CharString_replaceAllStringx(replace, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceAllStringInsensitivex(const CharStringReplace3 *replace, Error *e_rr) {
	return CharString_replaceAllStringx(replace, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_replaceStringSensitivex(const CharStringReplace3 *replace, Bool isFirst, Error *e_rr) {
	return CharString_replaceStringx(replace, isFirst, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceStringInsensitivex(const CharStringReplace3 *replace, Bool isFirst, Error *e_rr) {
	return CharString_replaceStringx(replace, isFirst, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_replaceFirstStringSensitivex(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceFirstString(replace, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceFirstStringInsensitivex(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceFirstString(replace, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_replaceLastStringSensitivex(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceLastString(replace, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_replaceLastStringInsensitivex(const CharStringReplace2 *replace, Error *e_rr) {
	return CharString_replaceLastString(replace, EStringCase_Insensitive, e_rr);
}

typedef struct CharStringFind1 {
	const CharString *s;
	U64 off;
	U64 len;
	ListU64 *result;
} CharStringFind1;

static inline Bool CharString_findAllx(const CharStringFind1 *find1, C8 c, EStringCase caseSensitive, Error *e_rr) {

	Bool s_uccess = true;

	if (!find1)
		retError(clean, Error_nullPointer(0, "CharString_findAllx()::find1 is required"));

	const CharStringFind find = {
		find1->s,
		Platform_instance->alloc,
		find1->off,
		find1->len,
		find1->result
	};

	gotoIfError3(clean, CharString_findAll(&find, c, caseSensitive, e_rr));

clean:
	return s_uccess;
}

static inline Bool CharString_findAllSensitivex(const CharStringFind1 *find1, C8 c, Error *e_rr) {
	return CharString_findAllx(find1, c, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_findAllInsensitivex(const CharStringFind1 *find1, C8 c, Error *e_rr) {
	return CharString_findAllx(find1, c, EStringCase_Insensitive, e_rr);
}

static inline Bool CharString_findAllStringx(
	const CharStringFind1 *find1,
	const CharString *other,
	EStringCase caseSensitive,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!find1)
		retError(clean, Error_nullPointer(0, "CharString_findAllStringx()::find1 is required"));
	
	const CharStringFind find = {
		find1->s,
		Platform_instance->alloc,
		find1->off,
		find1->len,
		find1->result
	};

	gotoIfError3(clean, CharString_findAllString(&find, other, caseSensitive, e_rr));

clean:
	return s_uccess;
}

static inline Bool CharString_findAllStringSensitivex(const CharStringFind1 *find1, const CharString *other, Error *e_rr) {
	return CharString_findAllStringx(find1, other, EStringCase_Sensitive, e_rr);
}

static inline Bool CharString_findAllStringInsensitivex(const CharStringFind1 *find1, const CharString *other, Error *e_rr) {
	return CharString_findAllStringx(find1, other, EStringCase_Insensitive, e_rr);
}

static inline Bool ListCharString_createCopyUnderlyingx(const ListCharString *toCopy, ListCharString *arr, Error *e_rr) {
	return ListCharString_createCopyUnderlying(toCopy, Platform_instance->alloc, arr, e_rr);
}

static inline Bool ListCharString_movex(ListCharString *src, ListCharString *dst, Error *e_rr) {
	return ListCharString_move(src, Platform_instance->alloc, dst, e_rr);
}

static inline Bool ListCharString_combinex(const ListCharString *arr, CharString *result, Error *e_rr) {
	const ListCharStringConcat concat = { arr, Platform_instance->alloc, result };
	return ListCharString_combine(&concat, e_rr);
}

static inline Bool ListCharString_concatx(const ListCharString *arr, C8 between, CharString *result, Error *e_rr) {
	const ListCharStringConcat concat = { arr, Platform_instance->alloc, result };
	return ListCharString_concat(&concat, between, e_rr);
}

static inline Bool ListCharString_concatStringx(
	const ListCharString *arr,
	const CharString *between,
	CharString *result,
	Error *e_rr
) {
	const ListCharStringConcat concat = { arr, Platform_instance->alloc, result };
	return ListCharString_concatString(&concat, between, e_rr);
}

static inline Bool CharString_formatVariadicx(CharString *result, Error *e_rr, const C8 *format, va_list args) {
	return CharString_formatVariadic(Platform_instance->alloc, result, e_rr, format, args);
}

static inline Bool CharString_formatx(CharString *result, Error *e_rr, const C8 *format, ...) {

	Bool s_uccess = true;

	if(!result || !format)
		retError(clean, Error_nullPointer(!result ? 1 : 2, "CharString_formatx()::result and format are required"));

	va_list arg1;
	va_start(arg1, format);
	s_uccess = CharString_formatVariadic(Platform_instance->alloc, result, e_rr, format, arg1);
	va_end(arg1);

clean:
	return s_uccess;
}

static inline void CharString_freex(CharString *str) { CharString_free(str, Platform_instance->alloc); }

static inline void ListCharString_freeUnderlyingx(ListCharString *arr) {
	ListCharString_freeUnderlying(arr, Platform_instance->alloc);
}

#ifdef __cplusplus
	}
#endif
