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

#pragma once
#include "types/container/list.h"
#include "types/container/buffer.h"
#include "types/base/type_id.h"
#include "types/base/string.h"

#include <stdarg.h>

#ifdef __cplusplus
	extern "C" {
#endif

TList(CharString);
TListNamed(const C8*, ListConstC8);

static inline Bool ListCharString_sort(ListCharString list, EStringCase stringCase) {
	return GenericList_sortString(ListCharString_toList(list), stringCase);
}

static inline Bool ListCharString_sortSensitive(ListCharString list) {
	return GenericList_sortStringSensitive(ListCharString_toList(list));
}

static inline Bool ListCharString_sortInsensitive(ListCharString list) {
	return GenericList_sortStringInsensitive(ListCharString_toList(list));
}

//Strings that HAVE to be freed (anything that uses an allocator needs freeing)
//These reside on the heap/free space (or wherever allocator allocates them)
//Which means that they aren't const nor refs

void CharString_free(CharString *str, const Allocator *alloc);

Bool CharString_create(C8 c, U64 size, const Allocator *alloc, CharString *result, Error *e_rr);
Bool CharString_createCopy(const CharString str, const Allocator *alloc, CharString *result, Error *e_rr);

typedef struct CharStringCreateNumber {
	U64 v;
	U8 leadingZeros;
	const Allocator *allocator;
	CharString *result;
} CharStringCreateNumber;

Bool CharString_createNyto(const CharStringCreateNumber *number, Error *e_rr);
Bool CharString_createHex(const CharStringCreateNumber *number, Error *e_rr);
Bool CharString_createDec(const CharStringCreateNumber *number, Error *e_rr);
Bool CharString_createOct(const CharStringCreateNumber *number, Error *e_rr);
Bool CharString_createBin(const CharStringCreateNumber *number, Error *e_rr);

typedef struct CharStringSplit {
	const CharString *s;
	const Allocator *allocator;
	ListCharString *result;
} CharStringSplit;

Bool CharString_split(const CharStringSplit *split, C8 c, EStringCase casing, Error *e_rr);
Bool CharString_splitString(const CharStringSplit *split, const CharString *other, EStringCase casing, Error *e_rr);

Bool CharString_splitLine(const CharString s, const Allocator *alloc, ListCharString *result, Error *e_rr);

//This will operate on this string, so it will need a heap allocated string

Bool CharString_resize(CharString *str, U64 length, C8 defaultChar, const Allocator *alloc, Error *e_rr);
Bool CharString_reserve(CharString *str, U64 length, const Allocator *alloc, Error *e_rr);

Bool CharString_append(CharString *s, C8 c, const Allocator *allocator, Error *e_rr);
Bool CharString_appendString(CharString *s, const CharString *other, const Allocator *allocator, Error *e_rr);

Bool CharString_insert(CharString *s, C8 c, U64 i, const Allocator *allocator, Error *e_rr);
Bool CharString_insertString(CharString *s, const CharString *other, U64 i, const Allocator *allocator, Error *e_rr);

static inline Bool CharString_prepend(CharString *s, C8 c, const Allocator *allocator, Error *e_rr) {
	return CharString_insert(s, c, 0, allocator, e_rr);
}

static inline Bool CharString_prependString(CharString *s, const CharString *other, const Allocator *allocator, Error *e_rr) {
	return CharString_insertString(s, other, 0, allocator, e_rr);
}

typedef struct CharStringReplace2 {
	CharString *s;
	const CharString *search;
	const CharString *replace;
	const Allocator *allocator;
	U64 off;
	U64 len;
} CharStringReplace2;

Bool CharString_replaceAllString(const CharStringReplace2 *replace, EStringCase caseSensitive, Error *e_rr);
Bool CharString_replaceString(const CharStringReplace2 *replace, Bool isFirst, EStringCase caseSensitive, Error *e_rr);

typedef struct CharStringFind {
	const CharString *s;
	const Allocator *alloc;
	U64 off;
	U64 len;
	ListU64 *result;
} CharStringFind;

Bool CharString_findAll(const CharStringFind *find, C8 c, EStringCase caseSensitive, Error *e_rr);
Bool CharString_findAllString(const CharStringFind *find, const CharString *other, EStringCase caseSensitive, Error *e_rr);

U64 CharString_unicodeCodepoints(const CharString str);		//Returns U64_MAX if invalid codepoints were detected
U64 CharString_hash(const CharString s);					//Hash of content, for maps only (not for cryptography purposes)

CharString CharString_getFilePath(CharString *str);			//Formats on string first to ensure it's proper
CharString CharString_getBasePath(CharString *str);			//Formats on string first to ensure it's proper

//TODO: Regex

//List of CharString with additional functionality to allow something like std::vector<std::string>
//This handles copies, but should only be used when managed strings are used.
//For a list of ref strings, the normal functionality should be used and makes everything a lot easier.

void ListCharString_freeUnderlying(ListCharString *arr, const Allocator *alloc);
Bool ListCharString_createCopyUnderlying(
	const ListCharString *toCopy, const Allocator *alloc, ListCharString *arr, Error *e_rr
);

//Move data for strings + array wherever possible.
//Otherwise, allocate.
Bool ListCharString_move(ListCharString *src, const Allocator *alloc, ListCharString *dst, Error *e_rr);

//Combining all strings into one

typedef struct ListCharStringConcat {
	const ListCharString *arr;
	const Allocator *alloc;
	CharString *result;
} ListCharStringConcat;

Bool ListCharString_combine(const ListCharStringConcat *concat, Error *e_rr);
Bool ListCharString_concat(const ListCharStringConcat *concat, C8 between, Error *e_rr);
Bool ListCharString_concatString(const ListCharStringConcat *concat, const CharString *between, Error *e_rr);

//Formatting

Bool CharString_formatVariadic(const Allocator *alloc, CharString *result, Error *e_rr, const C8 *format, va_list args);
Bool CharString_format(const Allocator *alloc, CharString *result, Error *e_rr, const C8 *format, ...);

//Parsing and stringifying ETypeId (as long as it's a basic OxC3 base type)

Bool CharString_createFromETypeId(ETypeId type, const Allocator *alloc, CharString *result, Error *e_rr);

typedef enum EHLSLStringifyFlags {
	EHLSLStringifyFlags_None			= 0,
	EHLSLStringifyFlags_Has16Bit		= 1 << 0,
	EHLSLStringifyFlags_HasF64			= 1 << 1,
	EHLSLStringifyFlags_HasI64			= 1 << 2,
	EHLSLStringifyFlags_IsStrict		= 1 << 3		//Deny 'soft' promotion (e.g. 16-bit becomes 32-bit) if not present
} EHLSLStringifyFlags;

Bool CharString_createFromETypeIdHLSL(
	ETypeId type,
	EHLSLStringifyFlags stringifyFlags,
	const Allocator *alloc,
	CharString *result,
	Error *e_rr
);

ETypeId ETypeId_parse(const CharString str);

#ifdef __cplusplus
	}
#endif
