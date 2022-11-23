#pragma once
#include "error.h"
#include "allocator.h"
#include "buffer.h"

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
//String; A string that goes on the heap (or wherever the allocator tells it to go)
//String(Ref); A reference to an already allocated string (String with capacity 0)
//String(Ref) const; A const reference that should not allow operations on it (String with capacity -1)

//Stack strings that are faster and easier to allocate

#define ShortString_LEN 32
#define LongString_LEN 64

typedef C8 ShortString[ShortString_LEN];
typedef C8 LongString[LongString_LEN];

typedef struct List List;

//Heap string

typedef struct String {
	U64 length, capacity;		//capacity = 0: ref, capacity = -1: const ref
	C8 *ptr;
} String;

typedef struct StringList {
	U64 length;
	String *ptr;
} StringList;

//Simple helper functions (inlines)

inline Bool String_isConstRef(String str) { return str.capacity == U64_MAX; }
inline Bool String_isRef(String str) { return !str.capacity || String_isConstRef(str); }
inline Bool String_isEmpty(String str) { return !str.length || !str.ptr; }
inline U64  String_bytes(String str) { return str.length; }

inline Buffer String_buffer(String str) { return Buffer_createRef(str.ptr, str.length); }

//Iteration

inline C8 *String_begin(String str) { 
	return String_isConstRef(str) ? NULL : str.ptr; 
}

inline C8 *String_end(String str) {
	return String_isConstRef(str) ? NULL : str.ptr + str.length;
}

inline C8 *String_charAt(String str, U64 off) { 
	return String_isConstRef(str) || off >= str.length ? NULL : str.ptr + off; 
}

inline const C8 *String_beginConst(String str) { return str.ptr; }
inline const C8 *String_endConst(String str) { return str.ptr + str.length; }

inline const C8 *String_charAtConst(String str, U64 off) { 
	return off >= str.length ? NULL : str.ptr + off; 
}

inline const Bool String_isValidAscii(String str) {

	for(U64 i = 0; i < str.length; ++i)
		if(!C8_isValidAscii(str.ptr[i]))
			return false;

	return true;
}

inline const Bool String_isValidFileName(String str, Bool acceptTrailingNull) {

	for(U64 i = 0; i < str.length; ++i)
		if(!C8_isValidFileName(str.ptr[i]) && !(i == str.length - 1 && !str.ptr[i] && acceptTrailingNull))
			return false;

	return str.length;
}

//Only checks characters. Please use resolvePath to actually validate

inline const Bool String_isValidFilePath(String str) {

	//Don't allow ending with .

	if(str.length && (
		str.ptr[str.length - 1] == '.' || 
		(str.length >= 2 && str.ptr[str.length - 2] == '.'&& str.ptr[str.length - 1] == '\0')
	))
		return false;

	for(U64 i = 0; i < str.length; ++i)
		if(
			!C8_isValidFileName(str.ptr[i]) && str.ptr[i] != '/' && str.ptr[i] != '\\' && 
			!(i == str.length - 1 && !str.ptr[i]) &&						//Allow null terminator
			!(i == 1 && str.ptr[i] == ':')									//Allow drive names for Windows
		)
			return false;

	return str.length;
}

//

inline void String_clear(String *str) { 
	if(str) 
		str->length = 0; 
}

U64 String_calcStrLen(const C8 *ptr, U64 maxSize);
//U64 String_hash(String s);								TODO:

inline C8 String_getAt(String str, U64 i) { 
	return i < str.length && str.ptr ? str.ptr[i] : C8_MAX;
}

inline Bool String_setAt(String str, U64 i, C8 c) { 

	if(i >= str.length || String_isConstRef(str))
		return false;

	str.ptr[i] = c;
	return true;
}

//Freeing refs won't do anything, but is still recommended for consistency. 
//Const ref disallow modifying functions to be used.

inline String String_createNull() { return (String) { 0 }; }

inline String String_createConstRef(const C8 *ptr, U64 maxSize) { 
	return (String) { 
		.length = String_calcStrLen(ptr, maxSize),
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

//Only use this if string is created safely in code

inline String String_createConstRefUnsafe(const C8 *ptr) {
	return String_createConstRef(ptr, U64_MAX);
}

inline String String_createRef(C8 *ptr, U64 maxSize) { 
	return (String) { 
		.length = String_calcStrLen(ptr, maxSize),
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

inline String String_createConstRefSized(const C8 *ptr, U64 size) {
	return (String) { 
		.length = size,
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

inline String String_createRefSized(C8 *ptr, U64 size) {
	return (String) { 
		.length = size,
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

inline String String_createConstRefShortString(const ShortString str) {
	return (String) {
		.length = String_calcStrLen(str, ShortString_LEN - 1),
		.ptr = (C8*) str,
		.capacity = U64_MAX		//Flag as const
	};
}

inline String String_createConstRefLongString(const LongString str) {
	return (String) {
		.length = String_calcStrLen(str, LongString_LEN - 1),
		.ptr = (C8*) str,
		.capacity = U64_MAX		//Flag as const
	};
}

inline String String_createRefShortString(ShortString str) {
	return (String) {
		.length = String_calcStrLen(str, ShortString_LEN - 1),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

inline String String_createRefLongString(LongString str) {
	return (String) {
		.length = String_calcStrLen(str, LongString_LEN - 1),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

//Strings that HAVE to be freed (anything that uses an allocator needs freeing)
//These reside on the heap/free space (or wherever allocator allocates them)
//Which means that they aren't const nor refs

Error String_free(String *str, Allocator alloc);

Error String_create(C8 c, U64 size, Allocator alloc, String *result);
Error String_createCopy(String str, Allocator alloc, String *result);

Error String_createNyto(U64 v, Bool leadingZeros, Allocator allocator, String *result);
Error String_createHex(U64 v, Bool leadingZeros, Allocator allocator, String *result);
Error String_createDec(U64 v, Bool leadingZeros, Allocator allocator, String *result);
Error String_createOct(U64 v, Bool leadingZeros, Allocator allocator, String *result);
Error String_createBin(U64 v, Bool leadingZeros, Allocator allocator, String *result);

Error String_split(
	String s,
	C8 c, 
	EStringCase casing, 
	Allocator allocator,
	StringList *result
);

Error String_splitString(
	String s,
	String other,
	EStringCase casing,
	Allocator allocator,
	StringList *result
);

//This will operate on this string, so it will need a heap allocated string

Error String_resize(String *str, U64 length, C8 defaultChar, Allocator alloc);
Error String_reserve(String *str, U64 length, Allocator alloc);

Error String_append(String *s, C8 c, Allocator allocator);
Error String_appendString(String *s, String other, Allocator allocator);

Error String_insert(String *s, C8 c, U64 i, Allocator allocator);
Error String_insertString(String *s, String other, U64 i, Allocator allocator);

Error String_replaceAllString(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive, 
	Allocator allocator
);

Error String_replaceString(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Allocator allocator,
	Bool isFirst
);

inline Error String_replaceFirstString(
	String *s,
	String search,
	String replace,
	EStringCase caseSensitive,
	Allocator allocator
) {
	return String_replaceString(s, search, replace, caseSensitive, allocator, true);
}

inline Error String_replaceLastString(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Allocator allocator
) {
	return String_replaceString(s, search, replace, caseSensitive, allocator, false);
}

//Simple checks (consts)

inline Bool String_startsWith(String str, C8 c, EStringCase caseSensitive) { 
	return 
		str.length && str.ptr && 
		C8_transform(*str.ptr, (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_startsWithString(String str, String other, EStringCase caseSensitive);

inline Bool String_endsWith(String str, C8 c, EStringCase caseSensitive) {
	return 
		str.length && str.ptr && 
		C8_transform(str.ptr[str.length - 1], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_endsWithString(String str, String other, EStringCase caseSensitive);

U64 String_countAll(String s, C8 c, EStringCase caseSensitive);
U64 String_countAllString(String s, String other, EStringCase caseSensitive);

//Returns the locations (U64[])

List String_findAll(
	String s, 
	C8 c, 
	Allocator alloc,
	EStringCase caseSensitive
);

List String_findAllString(
	String s, 
	String other,
	Allocator alloc,
	EStringCase caseSensitive
);

//

U64 String_findFirst(String s, C8 c, EStringCase caseSensitive);
U64 String_findLast(String s, C8 c, EStringCase caseSensitive);

inline U64 String_find(String s, C8 c, EStringCase caseSensitive, Bool isFirst) {
	return isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);
}

U64 String_findFirstString(String s, String other, EStringCase caseSensitive);
U64 String_findLastString(String s, String other, EStringCase caseSensitive);

inline U64 String_findString(String s, String other, EStringCase caseSensitive, Bool isFirst) {
	return isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);
}

inline Bool String_contains(String str, C8 c, EStringCase caseSensitive) { 
	return String_findFirst(str, c, caseSensitive) != U64_MAX;
}

inline Bool String_containsString(String str, String other, EStringCase caseSensitive)  { 
	return String_findFirstString(str, other, caseSensitive) != U64_MAX; 
}

Bool String_equalsString(String s, String other, EStringCase caseSensitive);
Bool String_equals(String s, C8 c, EStringCase caseSensitive);

Bool String_parseNyto(String s, U64 *result);
Bool String_parseHex(String s, U64 *result);
Bool String_parseDec(String s, U64 *result);
Bool String_parseDecSigned(String s, I64 *result);
Bool String_parseFloat(String s, F32 *result);
Bool String_parseOct(String s, U64 *result);
Bool String_parseBin(String s, U64 *result);

Bool String_isNyto(String s);				//[0-9A-Za-z_$]+
Bool String_isAlphaNumeric(String s);		//[0-9A-Za-z_]+
Bool String_isHex(String s);				//[0-9A-Fa-f]+
Bool String_isDec(String s);				//[0-9]+
Bool String_isOct(String s);				//[0-7]+
Bool String_isBin(String s);				//[0-1]+

Bool String_isUnsignedNumber(String s);		//[+]?[0-9]+
Bool String_isSignedNumber(String s);		//[-+]?[0-9]+
Bool String_isFloat(String s);				//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

Error String_offsetAsRef(String s, U64 off, String *result);

//Things that perform on this string to reduce overhead

Bool String_cutBegin(String s, U64 off, String *result);
Bool String_cutEnd(String s, U64 length, String *result);
Bool String_cut(String s, U64 off, U64 length, String *result);

Bool String_cutAfter(String s, C8 c, EStringCase caseSensitive, Bool isFirst, String *result);

inline Bool String_cutAfterLast(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutAfter(s, c, caseSensitive, false, result);
}

inline Bool String_cutAfterFirst(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutAfter(s, c, caseSensitive, true, result);
}

Bool String_cutAfterString(
	String s, String other, EStringCase caseSensitive, Bool isFirst, String *result
);

inline Bool String_cutAfterLastString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutAfterString(s, other, caseSensitive, false, result);
}

inline Bool String_cutAfterFirstString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutAfterString(s, other, caseSensitive, true, result);
}

Bool String_cutBefore(String s, C8 c, EStringCase caseSensitive, Bool isFirst, String *result);

inline Bool String_cutBeforeLast(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutBefore(s, c, caseSensitive, false, result);
}

inline Bool String_cutBeforeFirst(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutBefore(s, c, caseSensitive, true, result);
}

Bool String_cutBeforeString(String s, String other, EStringCase caseSensitive, Bool isFirst, String *result);

inline Bool String_cutBeforeLastString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutBeforeString(s, other, caseSensitive, false, result);
}

inline Bool String_cutBeforeFirstString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutBeforeString(s, other, caseSensitive, true, result);
}

Error String_eraseAtCount(String *s, U64 i, U64 count);
inline Error String_popFrontCount(String *s, U64 count) { return String_eraseAtCount(s, 0, count); }
inline Error String_popEndCount(String *s, U64 count) { return String_eraseAtCount(s, s ? s->length : 0, count); }

inline Error String_eraseAt(String *s, U64 i) { return String_eraseAtCount(s, i, 1); }
inline Error String_popFront(String *s) { return String_eraseAt(s, 0); }
inline Error String_popEnd(String *s) { return String_eraseAt(s, s ? s->length : 0); }

Bool String_eraseAll(String *s, C8 c, EStringCase caseSensitive);
Bool String_eraseAllString(String *s, String other, EStringCase caseSensitive);

Bool String_erase(String *s, C8 c, EStringCase caseSensitive, Bool isFirst);

inline Bool String_eraseFirst(String *s, C8 c, EStringCase caseSensitive) {
	return String_erase(s, c, caseSensitive, true);
}

inline Bool String_eraseLast(String *s, C8 c, EStringCase caseSensitive) {
	return String_erase(s, c, caseSensitive, false);
}

Bool String_eraseString(String *s, String other, EStringCase caseSensitive, Bool isFirst);

inline Bool String_eraseFirstString(String *s, String other, EStringCase caseSensitive){
	return String_eraseString(s, other, caseSensitive, true);
}

inline Bool String_eraseLastString(String *s, String other, EStringCase caseSensitive) {
	return String_eraseString(s, other, caseSensitive, false);
}

Bool String_replaceAll(String *s, C8 c, C8 v, EStringCase caseSensitive);
Bool String_replace(String *s, C8 c, C8 v, EStringCase caseSensitive, Bool isFirst);

inline Bool String_replaceFirst(String *s, C8 c, C8 v, EStringCase caseSensitive) {
	return String_replace(s, c, v, caseSensitive, true);
}

inline Bool String_replaceLast(String *s, C8 c, C8 v, EStringCase caseSensitive) {
	return String_replace(s, c, v, caseSensitive, false);
}

String String_trim(String s);		//Returns a substring ref in a string

Bool String_transform(String s, EStringTransform stringTransform);

inline Bool String_toLower(String str) { return String_transform(str, EStringTransform_Lower); }
inline Bool String_toUpper(String str) { return String_transform(str, EStringTransform_Upper); }

//Simple file utils

inline Bool String_formatPath(String *str) { 
	return String_replaceAll(str, '\\', '/', EStringCase_Insensitive);
}

String String_getFilePath(String *str);	//Formats on string first to ensure it's proper
String String_getBasePath(String *str);	//Formats on string first to ensure it's proper

//TODO: Regex

//String list
//To return from string operations
//These strings will be a ref into existing string to prevent copies, use StringList_createCopy to move them to the heap.
//Because they contain mixed strings, the functions still need allocators to free heap strings.
//This is different than List<String> because that one won't enforce string allocation properly.

Error StringList_create(U64 length, Allocator alloc, StringList *result);
Error StringList_free(StringList *arr, Allocator alloc);

Error StringList_createCopy(const StringList *toCopy, Allocator alloc, StringList *arr);

//Store the string directly into StringList (no copy)
//The allocator is used to free strings if they are heap allocated

Error StringList_set(StringList arr, U64 i, String str, Allocator alloc);
Error StringList_unset(StringList arr, U64 i, Allocator alloc);

//Combining all strings into one

Error StringList_combine(StringList arr, Allocator alloc, String *result);

Error StringList_concat(StringList arr, C8 between, Allocator alloc, String *result);
Error StringList_concatString(StringList arr, String between, Allocator alloc, String *result);
