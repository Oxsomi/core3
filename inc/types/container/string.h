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

#pragma once
#include "types/container/list.h"

#include <stdarg.h>

#ifdef __cplusplus
	extern "C" {
#endif

//Since a CharString can be a ref to existing memory, it doesn't necessarily have a null terminator.
//The null terminator is omitted for speed and to allow references into an existing string or data.
//The null terminator is automatically added on copy.
//
//There are four/five types of strings:
//
//Stack strings (or heap if you manually allocate it there):
//	ShortString; 31 chars max (includes null terminator)
//	LongString; 63 char max (includes null terminator)
//
//CharString; A string that goes on the heap (or wherever the allocator tells it to go)
//CharString(Ref); A reference to an already allocated string (CharString with capacity 0)
//CharString(Ref) const; A const reference that should not allow operations on it (CharString with capacity -1)

//Dynamic string

typedef struct CharString {
	union {
		const C8 *ptr;				//This is non const if not a const ref, but for safety this is const (cast away if not).
		C8 *ptrNonConst;			//Only use if !isConstRef
	};
	U64 lenAndNullTerminated;		//First bit contains if it's null terminated or not. Length excludes null terminator.
	U64 capacityAndRefInfo;			//capacityAndRefInfo = 0: ref, capacityAndRefInfo = -1: const ref
} CharString;

TList(CharString);
TListNamed(const C8*, ListConstC8);

Bool ListCharString_sort(ListCharString list, EStringCase stringCase);
Bool ListCharString_sortSensitive(ListCharString list);
Bool ListCharString_sortInsensitive(ListCharString list);

//Simple helper functions

Bool CharString_isConstRef(CharString str);
Bool CharString_isRef(CharString str);
Bool CharString_isEmpty(CharString str);
Bool CharString_isNullTerminated(CharString str);
U64  CharString_bytes(CharString str);
U64  CharString_length(CharString str);
U64  CharString_capacity(CharString str);				//Returns 0 if ref

Buffer CharString_buffer(CharString str);
Buffer CharString_bufferConst(CharString str);
Buffer CharString_allocatedBuffer(CharString str);		//Returns null buffer if ref
Buffer CharString_allocatedBufferConst(CharString str);

//Iteration

C8 *CharString_begin(CharString str);
const C8 *CharString_beginConst(CharString str);

C8 *CharString_end(CharString str);
const C8 *CharString_endConst(CharString str);

C8 *CharString_charAt(CharString str, U64 off);
const C8 *CharString_charAtConst(CharString str, U64 off);

//Returns U32_MAX if it wasn't a valid UTF8 codepoint
//TODO: U32 CharString_codepointAtByteConst(CharString str, U64 startByteOffset, U8 *length);
//TODO: U32 CharString_codepointAtConst(CharString str, U64 offset, U8 *length);

Bool CharString_isValidAscii(CharString str);
Bool CharString_isValidUTF8(CharString str);
Bool CharString_isValidFileName(CharString str);

ECompareResult CharString_compare(CharString a, CharString b, EStringCase caseSensitive);
ECompareResult CharString_compareSensitive(CharString a, CharString b);						//TODO: sensitivity for unicode?
ECompareResult CharString_compareInsensitive(CharString a, CharString b);

//Only checks characters. Please use resolvePath to actually validate if it's safely accessible.

Bool CharString_isValidFilePath(CharString str);
Bool CharString_clear(CharString *str);

U64 CharString_calcStrLen(const C8 *ptr, U64 maxSize);
U64 CharString_unicodeCodepoints(CharString str);				//Returns U64_MAX if invalid codepoints were detected
U64 CharString_hash(CharString s);								//Hash of content, for maps only (not for cryptography purposes)

C8 CharString_getAt(CharString str, U64 i);
Bool CharString_setAt(CharString str, U64 i, C8 c);

//Freeing refs won't do anything, but is still recommended for consistency.
//Const ref disallow modifying functions to be used.

CharString CharString_createNull();

CharString CharString_createRefAutoConst(const C8 *ptr, U64 maxSize);		//Auto detect end (up to maxSize chars)

CharString CharString_createRefCStrConst(const C8 *ptr);					//Only use this if string is created safely (\0)
CharString CharString_createRefAuto(C8 *ptr, U64 maxSize);					//Auto detect end (up to maxSize chars)

//isNullTerminated is true if the size given excludes the null terminator (e.g. ptr[size] == '\0').
//In this case ptr[size] has to be the null terminator.
//If this is false, it will automatically check if ptr contains a null terminator

CharString CharString_createRefSizedConst(const C8 *ptr, U64 size, Bool isNullTerminated);
CharString CharString_createRefSized(C8 *ptr, U64 size, Bool isNullTerminated);

CharString CharString_createRefStrConst(CharString str);

CharString CharString_createRefShortStringConst(const ShortString str);
CharString CharString_createRefLongStringConst(const LongString str);

CharString CharString_createRefShortString(ShortString str);
CharString CharString_createRefLongString(LongString str);

//Strings that HAVE to be freed (anything that uses an allocator needs freeing)
//These reside on the heap/free space (or wherever allocator allocates them)
//Which means that they aren't const nor refs

Bool CharString_free(CharString *str, Allocator alloc);

Error CharString_create(C8 c, U64 size, Allocator alloc, CharString *result);
Error CharString_createCopy(CharString str, Allocator alloc, CharString *result);

Error CharString_createNyto(U64 v, U8 leadingZeros, Allocator allocator, CharString *result);
Error CharString_createHex(U64 v, U8 leadingZeros, Allocator allocator, CharString *result);
Error CharString_createDec(U64 v, U8 leadingZeros, Allocator allocator, CharString *result);
Error CharString_createOct(U64 v, U8 leadingZeros, Allocator allocator, CharString *result);
Error CharString_createBin(U64 v, U8 leadingZeros, Allocator allocator, CharString *result);

//Interop between wchar_t. Converts UTF16 or UTF32 to UTF8

Error CharString_createFromUTF16(const U16 *ptr, U64 limit, Allocator allocator, CharString *result);
Error CharString_createFromUTF32(const U32 *ptr, U64 limit, Allocator allocator, CharString *result);

//TODO: Error CharString_setCodepointAt(CharString str, U64 index, U32 codepoint);

Error CharString_split(
	CharString s,
	C8 c,
	EStringCase casing,
	Allocator allocator,
	ListCharString *result
);

Error CharString_splitString(
	CharString s,
	CharString other,
	EStringCase casing,
	Allocator allocator,
	ListCharString *result
);

Error CharString_splitSensitive(CharString s, C8 c, Allocator allocator, ListCharString *result);
Error CharString_splitInsensitive(CharString s, C8 c, Allocator allocator, ListCharString *result);
Error CharString_splitStringSensitive(CharString s, CharString other, Allocator allocator, ListCharString *result);
Error CharString_splitStringInsensitive(CharString s, CharString other, Allocator allocator, ListCharString *result);

//TODO: CharString_splitCodepoint

Error CharString_splitLine(CharString s, Allocator alloc, ListCharString *result);

//This will operate on this string, so it will need a heap allocated string

Error CharString_resize(CharString *str, U64 length, C8 defaultChar, Allocator alloc);
Error CharString_reserve(CharString *str, U64 length, Allocator alloc);

//TODO: CharString_resizeCodepoint?

Error CharString_append(CharString *s, C8 c, Allocator allocator);
Error CharString_appendString(CharString *s, CharString other, Allocator allocator);

Error CharString_prepend(CharString *s, C8 c, Allocator allocator);
Error CharString_prependString(CharString *s, CharString other, Allocator allocator);

//TODO: CharString_appendCodepoint/prependCodepoint

CharString CharString_newLine();			//Always \n since all OSes can handle that nowadays

Error CharString_insert(CharString *s, C8 c, U64 i, Allocator allocator);
Error CharString_insertString(CharString *s, CharString other, U64 i, Allocator allocator);

//TODO: CharString_insertCodepoint

Error CharString_replaceAllString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	Bool isFirst,
	U64 off,
	U64 len
);

Error CharString_replaceFirstString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceLastString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceAllStringSensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceStringSensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	Bool isFirst,
	U64 off,
	U64 len
);

Error CharString_replaceFirstStringSensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceLastStringSensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceAllStringInsensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceStringInsensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	Bool isFirst,
	U64 off,
	U64 len
);

Error CharString_replaceFirstStringInsensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	U64 off,
	U64 len
);

Error CharString_replaceLastStringInsensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	U64 off,
	U64 len
);

//TODO: CharString_replaceCodepoint

//Interop between wide strings. Converts UTF8 to UTF16 or UTF32

Error CharString_toUTF16(CharString s, Allocator allocator, ListU16 *arr);
Error CharString_toUTF32(CharString s, Allocator allocator, ListU32 *arr);

//Simple checks (consts)

Bool CharString_startsWith(CharString str, C8 c, EStringCase caseSensitive, U64 off);
Bool CharString_startsWithString(CharString str, CharString other, EStringCase caseSensitive, U64 off);

Bool CharString_endsWith(CharString str, C8 c, EStringCase caseSensitive, U64 off);
Bool CharString_endsWithString(CharString str, CharString other, EStringCase caseSensitive, U64 off);

U64 CharString_countAll(CharString s, C8 c, EStringCase caseSensitive, U64 off);
U64 CharString_countAllString(CharString s, CharString other, EStringCase caseSensitive, U64 off);

Bool CharString_startsWithSensitive(CharString str, C8 c, U64 off);
Bool CharString_startsWithStringSensitive(CharString str, CharString other, U64 off);

Bool CharString_endsWithSensitive(CharString str, C8 c, U64 off);
Bool CharString_endsWithStringSensitive(CharString str, CharString other, U64 off);

U64 CharString_countAllSensitive(CharString s, C8 c, U64 off);
U64 CharString_countAllStringSensitive(CharString s, CharString other, U64 off);

Bool CharString_startsWithInsensitive(CharString str, C8 c, U64 off);
Bool CharString_startsWithStringInsensitive(CharString str, CharString other, U64 off);

Bool CharString_endsWithInsensitive(CharString str, C8 c, U64 off);
Bool CharString_endsWithStringInsensitive(CharString str, CharString other, U64 off);

U64 CharString_countAllInsensitive(CharString s, C8 c, U64 off);
U64 CharString_countAllStringInsensitive(CharString s, CharString other, U64 off);

//TODO: CharString_countCodepoint/endsWithCodepoint/startsWithCodepoint

Error CharString_findAll(CharString s, C8 c, Allocator alloc, EStringCase caseSensitive, U64 off, U64 len, ListU64 *result);
Error CharString_findAllString(
	CharString s,
	CharString other,
	Allocator alloc,
	EStringCase caseSensitive,
	U64 off,
	U64 len,
	ListU64 *result
);

U64 CharString_findFirst(CharString s, C8 c, EStringCase caseSensitive, U64 off, U64 len);
U64 CharString_findLast(CharString s, C8 c, EStringCase caseSensitive, U64 off, U64 len);

U64 CharString_find(CharString s, C8 c, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len);

U64 CharString_findFirstString(CharString s, CharString other, EStringCase caseSensitive, U64 off, U64 len);
U64 CharString_findLastString(CharString s, CharString other, EStringCase caseSensitive, U64 off, U64 len);
U64 CharString_findString(CharString s, CharString other, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len);

Error CharString_findAllSensitive(CharString s, C8 c, U64 off, U64 len, Allocator alloc, ListU64 *result);
Error CharString_findAllInsensitive(CharString s, C8 c, U64 off, U64 len, Allocator alloc, ListU64 *result);

Error CharString_findAllStringSensitive(CharString s, CharString other, U64 off, U64 len, Allocator alloc, ListU64 *result);
Error CharString_findAllStringInsensitive(CharString s, CharString other, U64 off, U64 len, Allocator alloc, ListU64 *result);

U64 CharString_findFirstSensitive(CharString s, C8 c, U64 off, U64 len);
U64 CharString_findFirstInsensitive(CharString s, C8 c, U64 off, U64 len);

U64 CharString_findLastSensitive(CharString s, C8 c, U64 off, U64 len);
U64 CharString_findLastInsensitive(CharString s, C8 c, U64 off, U64 len);

U64 CharString_findSensitive(CharString s, C8 c, Bool isFirst, U64 off, U64 len);
U64 CharString_findInsensitive(CharString s, C8 c, Bool isFirst, U64 off, U64 len);

U64 CharString_findFirstStringSensitive(CharString s, CharString other, U64 off, U64 len);
U64 CharString_findFirstStringInsensitive(CharString s, CharString other, U64 off, U64 len);

U64 CharString_findLastStringSensitive(CharString s, CharString other, U64 off, U64 len);
U64 CharString_findLastStringInsensitive(CharString s, CharString other, U64 off, U64 len);

U64 CharString_findStringSensitive(CharString s, CharString other, Bool isFirst, U64 off, U64 len);
U64 CharString_findStringInsensitive(CharString s, CharString other, Bool isFirst, U64 off, U64 len);

//TODO: CharString_findCodepoint

Bool CharString_contains(CharString str, C8 c, EStringCase caseSensitive, U64 off, U64 len);
Bool CharString_containsString(CharString str, CharString other, EStringCase caseSensitive, U64 off, U64 len);

Bool CharString_containsSensitive(CharString str, C8 c, U64 off, U64 len);
Bool CharString_containsInsensitive(CharString str, C8 c, U64 off, U64 len);

Bool CharString_containsStringSensitive(CharString str, CharString other, U64 off, U64 len);
Bool CharString_containsStringInsensitive(CharString str, CharString other, U64 off, U64 len);

//TODO: CharString_containsCodepoint

Bool CharString_equalsString(CharString s, CharString other, EStringCase caseSensitive);
Bool CharString_equals(CharString s, C8 c, EStringCase caseSensitive);

Bool CharString_equalsStringSensitive(CharString s, CharString other);
Bool CharString_equalsSensitive(CharString s, C8 c);

Bool CharString_equalsStringInsensitive(CharString s, CharString other);
Bool CharString_equalsInsensitive(CharString s, C8 c);

//TODO: CharString_equalsCodepoint

Bool CharString_parseNyto(CharString s, U64 *result);
Bool CharString_parseHex(CharString s, U64 *result);
Bool CharString_parseDec(CharString s, U64 *result);
Bool CharString_parseDecSigned(CharString s, I64 *result);
Bool CharString_parseDouble(CharString s, F64 *result);
Bool CharString_parseFloat(CharString s, F32 *result);
Bool CharString_parseOct(CharString s, U64 *result);
Bool CharString_parseBin(CharString s, U64 *result);
Bool CharString_parseU64(CharString s, U64 *result);

Bool CharString_isNyto(CharString s);				//[0-9A-Za-z_$]+
Bool CharString_isAlphaNumeric(CharString s);		//[0-9A-Za-z]+
Bool CharString_isHex(CharString s);				//[0-9A-Fa-f]+
Bool CharString_isDec(CharString s);				//[0-9]+
Bool CharString_isOct(CharString s);				//[0-7]+
Bool CharString_isBin(CharString s);				//[0-1]+

Bool CharString_isUnsignedNumber(CharString s);		//[+]?[0-9]+
Bool CharString_isSignedNumber(CharString s);		//[-+]?[0-9]+
Bool CharString_isFloat(CharString s);				//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

//Things that perform on this string to reduce overhead

Bool CharString_cutBegin(CharString s, U64 off, CharString *result);
Bool CharString_cutEnd(CharString s, U64 length, CharString *result);
Bool CharString_cut(CharString s, U64 off, U64 length, CharString *result);

Bool CharString_cutAfter(
	CharString s,
	C8 c,
	EStringCase caseSensitive,
	Bool isFirst,
	CharString *result
);

Bool CharString_cutAfterLast(CharString s, C8 c, EStringCase caseSensitive, CharString *result);
Bool CharString_cutAfterFirst(CharString s, C8 c, EStringCase caseSensitive, CharString *result);

Bool CharString_cutAfterString(
	CharString s,
	CharString other,
	EStringCase caseSensitive,
	Bool isFirst,
	CharString *result
);

Bool CharString_cutAfterLastString(CharString s, CharString other, EStringCase caseSensitive, CharString *result);
Bool CharString_cutAfterFirstString(CharString s, CharString other, EStringCase caseSensitive, CharString *result);

Bool CharString_cutBefore(
	CharString s,
	C8 c,
	EStringCase caseSensitive,
	Bool isFirst,
	CharString *result
);

Bool CharString_cutBeforeLast(CharString s, C8 c, EStringCase caseSensitive, CharString *result);
Bool CharString_cutBeforeFirst(CharString s, C8 c, EStringCase caseSensitive, CharString *result);

Bool CharString_cutBeforeString(
	CharString s,
	CharString other,
	EStringCase caseSensitive,
	Bool isFirst,
	CharString *result
);

Bool CharString_cutBeforeLastString(CharString s, CharString other, EStringCase caseSensitive, CharString *result);
Bool CharString_cutBeforeFirstString(CharString s, CharString other, EStringCase caseSensitive, CharString *result);

Bool CharString_cutAfterSensitive(CharString s, C8 c, Bool isFirst, CharString *result);
Bool CharString_cutAfterInsensitive(CharString s, C8 c, Bool isFirst, CharString *result);

Bool CharString_cutAfterLastSensitive(CharString s, C8 c, CharString *result);
Bool CharString_cutAfterLastInsensitive(CharString s, C8 c, CharString *result);

Bool CharString_cutAfterFirstSensitive(CharString s, C8 c, CharString *result);
Bool CharString_cutAfterFirstInsensitive(CharString s, C8 c, CharString *result);

Bool CharString_cutAfterStringSensitive(CharString s, CharString other, Bool isFirst, CharString *result);
Bool CharString_cutAfterStringInsensitive(CharString s, CharString other, Bool isFirst, CharString *result);

Bool CharString_cutAfterFirstStringSensitive(CharString s, CharString other, CharString *result);
Bool CharString_cutAfterFirstStringInsensitive(CharString s, CharString other, CharString *result);

Bool CharString_cutAfterLastStringSensitive(CharString s, CharString other, CharString *result);
Bool CharString_cutAfterLastStringInsensitive(CharString s, CharString other, CharString *result);

Bool CharString_cutBeforeSensitive(CharString s, C8 c, Bool isFirst, CharString *result);
Bool CharString_cutBeforeInsensitive(CharString s, C8 c, Bool isFirst, CharString *result);

Bool CharString_cutBeforeLastSensitive(CharString s, C8 c, CharString *result);
Bool CharString_cutBeforeLastInsensitive(CharString s, C8 c, CharString *result);

Bool CharString_cutBeforeFirstSensitive(CharString s, C8 c, CharString *result);
Bool CharString_cutBeforeFirstInsensitive(CharString s, C8 c, CharString *result);

Bool CharString_cutBeforeStringSensitive(CharString s, CharString other, Bool isFirst, CharString *result);
Bool CharString_cutBeforeStringInsensitive(CharString s, CharString other, Bool isFirst, CharString *result);

Bool CharString_cutBeforeFirstStringSensitive(CharString s, CharString other, CharString *result);
Bool CharString_cutBeforeFirstStringInsensitive(CharString s, CharString other, CharString *result);

Bool CharString_cutBeforeLastStringSensitive(CharString s, CharString other, CharString *result);
Bool CharString_cutBeforeLastStringInsensitive(CharString s, CharString other, CharString *result);

//TODO: CharString_cut(Before/After)Codepoint

Error CharString_eraseAtCount(CharString *s, U64 i, U64 count);
Error CharString_popFrontCount(CharString *s, U64 count);
Error CharString_popEndCount(CharString *s, U64 count);

Error CharString_eraseAt(CharString *s, U64 i);
Error CharString_popFront(CharString *s);
Error CharString_popEnd(CharString *s);

Bool CharString_eraseAll(CharString *s, C8 c, EStringCase caseSensitive, U64 off, U64 len);
Bool CharString_eraseAllString(CharString *s, CharString other, EStringCase caseSensitive, U64 off, U64 len);

Bool CharString_erase(CharString *s, C8 c, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len);

Bool CharString_eraseFirst(CharString *s, C8 c, EStringCase caseSensitive, U64 off, U64 len);
Bool CharString_eraseLast(CharString *s, C8 c, EStringCase caseSensitive, U64 off, U64 len);

Bool CharString_eraseString(CharString *s, CharString other, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len);

Bool CharString_eraseFirstString(CharString *s, CharString other, EStringCase caseSensitive, U64 off, U64 len);
Bool CharString_eraseLastString(CharString *s, CharString other, EStringCase caseSensitive, U64 off, U64 len);

//Duplicates of erase to simplify casing

Bool CharString_eraseAllSensitive(CharString *s, C8 c, U64 off, U64 len);
Bool CharString_eraseAllStringSensitive(CharString *s, CharString other, U64 off, U64 len);

Bool CharString_eraseSensitive(CharString *s, C8 c, Bool isFirst, U64 off, U64 len);

Bool CharString_eraseFirstSensitive(CharString *s, C8 c, U64 off, U64 len);
Bool CharString_eraseLastSensitive(CharString *s, C8 c, U64 off, U64 len);

Bool CharString_eraseStringSensitive(CharString *s, CharString other, Bool isFirst, U64 off, U64 len);

Bool CharString_eraseFirstStringSensitive(CharString *s, CharString other, U64 off, U64 len);
Bool CharString_eraseLastStringSensitive(CharString *s, CharString other, U64 off, U64 len);

Bool CharString_eraseAllInsensitive(CharString *s, C8 c, U64 off, U64 len);
Bool CharString_eraseAllStringInsensitive(CharString *s, CharString other, U64 off, U64 len);

Bool CharString_eraseInsensitive(CharString *s, C8 c, Bool isFirst, U64 off, U64 len);

Bool CharString_eraseFirstInsensitive(CharString *s, C8 c, U64 off, U64 len);
Bool CharString_eraseLastInsensitive(CharString *s, C8 c, U64 off, U64 len);

Bool CharString_eraseStringInsensitive(CharString *s, CharString other, Bool isFirst, U64 off, U64 len);

Bool CharString_eraseFirstStringInsensitive(CharString *s, CharString other, U64 off, U64 len);
Bool CharString_eraseLastStringInsensitive(CharString *s, CharString other, U64 off, U64 len);

//TODO: CharString_eraseCodepoint

//Replace

Bool CharString_replaceAll(CharString *s, C8 c, C8 v, EStringCase caseSensitive, U64 off, U64 len);
Bool CharString_replace(CharString *s, C8 c, C8 v, EStringCase caseSensitive, Bool isFirst, U64 off, U64 len);
Bool CharString_replaceFirst(CharString *s, C8 c, C8 v, EStringCase caseSensitive, U64 off, U64 len);
Bool CharString_replaceLast(CharString *s, C8 c, C8 v, EStringCase caseSensitive, U64 off, U64 len);

Bool CharString_replaceAllSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len);
Bool CharString_replaceSensitive(CharString *s, C8 c, C8 v, Bool isFirst, U64 off, U64 len);
Bool CharString_replaceFirstSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len);
Bool CharString_replaceLastSensitive(CharString *s, C8 c, C8 v, U64 off, U64 len);

Bool CharString_replaceAllInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len);
Bool CharString_replaceInsensitive(CharString *s, C8 c, C8 v, Bool isFirst, U64 off, U64 len);
Bool CharString_replaceFirstInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len);
Bool CharString_replaceLastInsensitive(CharString *s, C8 c, C8 v, U64 off, U64 len);

//TODO: CharString_replaceCodepoint

CharString CharString_trim(CharString s);		//Returns a substring ref in a string

Bool CharString_transform(CharString s, EStringTransform stringTransform);

Bool CharString_toLower(CharString str);		//TODO: Understand UTF8
Bool CharString_toUpper(CharString str);		//TODO: Understand UTF8

//Simple file utils

Bool CharString_formatPath(CharString *str);

CharString CharString_getFilePath(CharString *str);		//Formats on string first to ensure it's proper
CharString CharString_getBasePath(CharString *str);		//Formats on string first to ensure it's proper

//TODO: Regex

//List of CharString with additional functionality to allow something like std::vector<std::string>
//This handles copies, but should only be used when managed strings are used.
//For a list of ref strings, the normal functionality should be used and makes everything a lot easier.

Bool ListCharString_freeUnderlying(ListCharString *arr, Allocator alloc);

Error ListCharString_createCopyUnderlying(ListCharString toCopy, Allocator alloc, ListCharString *arr);

//Move data for strings + array wherever possible.
//Otherwise, allocate.
Bool ListCharString_move(ListCharString *src, Allocator alloc, ListCharString *dst, Error *e_rr);

//Combining all strings into one

Error ListCharString_combine(ListCharString arr, Allocator alloc, CharString *result);

Error ListCharString_concat(ListCharString arr, C8 between, Allocator alloc, CharString *result);
Error ListCharString_concatString(ListCharString arr, CharString between, Allocator alloc, CharString *result);

//Formatting

Error CharString_formatVariadic(Allocator alloc, CharString *result, const C8 *format, va_list args);
Error CharString_format(Allocator alloc, CharString *result, const C8 *format, ...);

#ifdef __cplusplus
	}
#endif
