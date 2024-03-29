/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "list.h"

#include <stdarg.h>

//For simplicity;
//A string is ALWAYS ASCII (7-bit) and no null terminator.
//The null terminator is ommitted for speed and to allow references into an existing string.
//The null terminator is only useful if it's created unsafely (which is only recommended for hardcoded strings).
//
//There are four types of strings:
// 
//Stack strings (or heap if you manually allocate it there):
//	ShortString; 31 chars max
//	LongString; 63 char max
//
//CharString; A string that goes on the heap (or wherever the allocator tells it to go)
//CharString(Ref); A reference to an already allocated string (CharString with capacity 0)
//CharString(Ref) const; A const reference that should not allow operations on it (CharString with capacity -1)

//Stack strings that are faster and easier to allocate

#define _SHORTSTRING_LEN 32
#define _LONGSTRING_LEN 64

typedef C8 ShortString[_SHORTSTRING_LEN];
typedef C8 LongString[_LONGSTRING_LEN];

typedef struct List List;
typedef struct Buffer Buffer;
typedef struct Allocator Allocator;
typedef struct Error Error;

//Heap string

typedef struct CharString {
	U64 lenAndNullTerminated;		//First bit contains if it's null terminated or not. Length excludes null terminator.
	U64 capacityAndRefInfo;			//capacityAndRefInfo = 0: ref, capacityAndRefInfo = -1: const ref
	const C8 *ptr;					//This is non const if not a const ref, but for safety this is const (cast away if not).
} CharString;

typedef struct CharStringList {
	U64 length;
	CharString *ptr;
} CharStringList;

//Simple helper functions (inlines)

Bool CharString_isConstRef(CharString str);
Bool CharString_isRef(CharString str);
Bool CharString_isEmpty(CharString str);
Bool CharString_isNullTerminated(CharString str);
U64  CharString_bytes(CharString str);
U64  CharString_length(CharString str);

Buffer CharString_buffer(CharString str);
Buffer CharString_bufferConst(CharString str);

//Iteration

C8 *CharString_begin(CharString str);
C8 *CharString_end(CharString str);

C8 *CharString_charAt(CharString str, U64 off);

const C8 *CharString_beginConst(CharString str);
const C8 *CharString_endConst(CharString str);

const C8 *CharString_charAtConst(CharString str, U64 off);
Bool CharString_isValidAscii(CharString str);
Bool CharString_isValidFileName(CharString str);

ECompareResult CharString_compare(CharString a, CharString b, EStringCase caseSensitive);

//Only checks characters. Please use resolvePath to actually validate if it's safely accessible.

Bool CharString_isValidFilePath(CharString str);
Bool CharString_clear(CharString *str);

U64 CharString_calcStrLen(const C8 *ptr, U64 maxSize);
//U64 CharString_hash(CharString s);								TODO:

C8 CharString_getAt(CharString str, U64 i);
Bool CharString_setAt(CharString str, U64 i, C8 c);

//Freeing refs won't do anything, but is still recommended for consistency. 
//Const ref disallow modifying functions to be used.

CharString CharString_createNull();

CharString CharString_createConstRefAuto(const C8 *ptr, U64 maxSize);		//Auto detect end (up to maxSize chars)

CharString CharString_createConstRefCStr(const C8 *ptr);					//Only use this if string is created safely (null terminator)
CharString CharString_createRefAuto(C8 *ptr, U64 maxSize);					//Auto detect end (up to maxSize chars)

//hasNullAfterSize is true if the size given excludes the null terminator (e.g. ptr[size] == '\0').
//In this case ptr[size] has to be the null terminator.
//If this is false, it will automatically check if ptr contains a null terminator

CharString CharString_createConstRefSized(const C8 *ptr, U64 size, Bool hasNullAfterSize);
CharString CharString_createRefSized(C8 *ptr, U64 size, Bool hasNullAfterSize);

//

CharString CharString_createConstRefShortString(const ShortString str);
CharString CharString_createConstRefLongString(const LongString str);

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

Error CharString_split(
	CharString s,
	C8 c, 
	EStringCase casing, 
	Allocator allocator,
	CharStringList *result
);

Error CharString_splitString(
	CharString s,
	CharString other,
	EStringCase casing,
	Allocator allocator,
	CharStringList *result
);

Error CharString_splitLine(CharString s, Allocator alloc, CharStringList *result);

//This will operate on this string, so it will need a heap allocated string

Error CharString_resize(CharString *str, U64 length, C8 defaultChar, Allocator alloc);
Error CharString_reserve(CharString *str, U64 length, Allocator alloc);

Error CharString_append(CharString *s, C8 c, Allocator allocator);
Error CharString_appendString(CharString *s, CharString other, Allocator allocator);

CharString CharString_newLine();			//Should only be used when writing a file for the current OS. Not when parsing files.

Error CharString_insert(CharString *s, C8 c, U64 i, Allocator allocator);
Error CharString_insertString(CharString *s, CharString other, U64 i, Allocator allocator);

Error CharString_replaceAllString(
	CharString *s, 
	CharString search, 
	CharString replace, 
	EStringCase caseSensitive, 
	Allocator allocator
);

Error CharString_replaceString(
	CharString *s, 
	CharString search, 
	CharString replace, 
	EStringCase caseSensitive,
	Allocator allocator,
	Bool isFirst
);

Error CharString_replaceFirstString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator
);

Error CharString_replaceLastString(
	CharString *s, 
	CharString search, 
	CharString replace, 
	EStringCase caseSensitive,
	Allocator allocator
);

//Simple checks (consts)

Bool CharString_startsWith(CharString str, C8 c, EStringCase caseSensitive);
Bool CharString_startsWithString(CharString str, CharString other, EStringCase caseSensitive);

Bool CharString_endsWith(CharString str, C8 c, EStringCase caseSensitive);
Bool CharString_endsWithString(CharString str, CharString other, EStringCase caseSensitive);

U64 CharString_countAll(CharString s, C8 c, EStringCase caseSensitive);
U64 CharString_countAllString(CharString s, CharString other, EStringCase caseSensitive);

//Returns the locations (U64[])

Error CharString_findAll(
	CharString s, 
	C8 c, 
	Allocator alloc,
	EStringCase caseSensitive,
	List *result
);

Error CharString_findAllString(
	CharString s, 
	CharString other,
	Allocator alloc,
	EStringCase caseSensitive,
	List *result
);

//

U64 CharString_findFirst(CharString s, C8 c, EStringCase caseSensitive);
U64 CharString_findLast(CharString s, C8 c, EStringCase caseSensitive);

U64 CharString_find(CharString s, C8 c, EStringCase caseSensitive, Bool isFirst);

U64 CharString_findFirstString(CharString s, CharString other, EStringCase caseSensitive);
U64 CharString_findLastString(CharString s, CharString other, EStringCase caseSensitive);
U64 CharString_findString(CharString s, CharString other, EStringCase caseSensitive, Bool isFirst);

Bool CharString_contains(CharString str, C8 c, EStringCase caseSensitive);
Bool CharString_containsString(CharString str, CharString other, EStringCase caseSensitive);

Bool CharString_equalsString(CharString s, CharString other, EStringCase caseSensitive);
Bool CharString_equals(CharString s, C8 c, EStringCase caseSensitive);

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

Error CharString_eraseAtCount(CharString *s, U64 i, U64 count);
Error CharString_popFrontCount(CharString *s, U64 count);
Error CharString_popEndCount(CharString *s, U64 count);

Error CharString_eraseAt(CharString *s, U64 i);
Error CharString_popFront(CharString *s);
Error CharString_popEnd(CharString *s);

Bool CharString_eraseAll(CharString *s, C8 c, EStringCase caseSensitive);
Bool CharString_eraseAllString(CharString *s, CharString other, EStringCase caseSensitive);

Bool CharString_erase(CharString *s, C8 c, EStringCase caseSensitive, Bool isFirst);

Bool CharString_eraseFirst(CharString *s, C8 c, EStringCase caseSensitive);
Bool CharString_eraseLast(CharString *s, C8 c, EStringCase caseSensitive);

Bool CharString_eraseString(CharString *s, CharString other, EStringCase caseSensitive, Bool isFirst);

Bool CharString_eraseFirstString(CharString *s, CharString other, EStringCase caseSensitive);
Bool CharString_eraseLastString(CharString *s, CharString other, EStringCase caseSensitive);

Bool CharString_replaceAll(CharString *s, C8 c, C8 v, EStringCase caseSensitive);

Bool CharString_replace(
	CharString *s, 
	C8 c, 
	C8 v, 
	EStringCase caseSensitive, 
	Bool isFirst
);

Bool CharString_replaceFirst(CharString *s, C8 c, C8 v, EStringCase caseSensitive);
Bool CharString_replaceLast(CharString *s, C8 c, C8 v, EStringCase caseSensitive);

CharString CharString_trim(CharString s);		//Returns a substring ref in a string

Bool CharString_transform(CharString s, EStringTransform stringTransform);

Bool CharString_toLower(CharString str);
Bool CharString_toUpper(CharString str);

//Simple file utils

Bool CharString_formatPath(CharString *str);

CharString CharString_getFilePath(CharString *str);	//Formats on string first to ensure it's proper
CharString CharString_getBasePath(CharString *str);	//Formats on string first to ensure it's proper

//TODO: Regex

//CharString list
//To return from string operations
//These strings will be a ref into existing string to prevent copies, use CharStringList_createCopy to move them to the heap.
//Because they contain mixed strings, the functions still need allocators to free heap strings.
//This is different than List<CharString> because that one won't enforce string allocation properly.

Error CharStringList_create(U64 length, Allocator alloc, CharStringList *result);
Bool CharStringList_free(CharStringList *arr, Allocator alloc);

Error CharStringList_createCopy(CharStringList toCopy, Allocator alloc, CharStringList *arr);

//Store the string directly into CharStringList (no copy)
//The allocator is used to free strings if they are heap allocated

Error CharStringList_set(CharStringList arr, U64 i, CharString str, Allocator alloc);
Error CharStringList_unset(CharStringList arr, U64 i, Allocator alloc);

//Combining all strings into one

Error CharStringList_combine(CharStringList arr, Allocator alloc, CharString *result);

Error CharStringList_concat(CharStringList arr, C8 between, Allocator alloc, CharString *result);
Error CharStringList_concatString(CharStringList arr, CharString between, Allocator alloc, CharString *result);

//Formatting

Error CharString_formatVariadic(Allocator alloc, CharString *result, const C8 *format, va_list args);
Error CharString_format(Allocator alloc, CharString *result, const C8 *format, ...);
