#pragma once
#include "hash.h"
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

//Heap string

struct String {
	U64 len, capacity;		//capacity = 0: ref, capacity = -1: const ref
	C8 *ptr;
};

//Simple helper functions (inlines)

inline Bool String_isConstRef(struct String str) { return str.capacity == U64_MAX; }
inline Bool String_isRef(struct String str) { return !str.capacity || String_isConstRef(str); }
inline Bool String_isEmpty(struct String str) { return !str.len || !str.ptr; }
inline Bool String_bytes(struct String str) { return str.len; }

inline struct Buffer String_buffer(struct String str) { return Buffer_createRef(str.ptr, str.len); }

//Iteration

inline C8 *String_begin(struct String str) { 
	return String_isConstRef(str) ? NULL : str.ptr; 
}

inline C8 *String_end(struct String str) {
	return String_isConstRef(str) ? NULL : str.ptr + str.len;
}

inline C8 *String_charAt(struct String str, U64 off) { 
	return String_isConstRef(str) || off >= str.len ? NULL : str.ptr + off; 
}

inline const C8 *String_beginConst(struct String str) { return str.ptr; }
inline const C8 *String_endConst(struct String str) { return str.ptr + str.len; }

inline const C8 *String_charAtConst(struct String str, U64 off) { 
	return off >= str.len ? NULL : str.ptr + off; 
}

//

inline void String_clear(struct String *str) { 
	if(str) 
		str->len = 0; 
}

U64 String_calcStrLen(const C8 *ptr, U64 maxSize);
U64 String_hash(struct String s);

inline C8 String_getAt(struct String str, U64 i) { 
	return i < str.len && str.ptr ? str.ptr[i] : (C8) U8_MAX;
}

inline Bool String_setAt(struct String str, U64 i, C8 c) { 

	if(i >= str.len || String_isConstRef(str))
		return false;

	str.ptr[i] = c;
	return true;
}

//Freeing refs won't do anything, but is still recommended for consistency. 
//Const ref disallow modifying functions to be used.

inline struct String String_createEmpty() { return (struct String) { 0 }; }

inline struct String String_createRefConst(const C8 *ptr, U64 maxSize) { 
	return (struct String) { 
		.len = String_calcStrLen(ptr, maxSize),
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

//Only use this if string is created safely in code

inline struct String String_createRefUnsafeConst(const C8 *ptr) {
	return String_createRefConst(ptr, U64_MAX);
}

inline struct String String_createRefDynamic(C8 *ptr, U64 maxSize) { 
	return (struct String) { 
		.len = String_calcStrLen(ptr, maxSize),
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

inline struct String String_createRefSizedConst(const C8 *ptr, U64 size) {
	return (struct String) { 
		.len = size,
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

inline struct String String_createRefSizedDynamic(C8 *ptr, U64 size) {
	return (struct String) { 
		.len = size,
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

inline struct String String_createRefShortStringConst(const ShortString str) {
	return (struct String) {
		.len = String_calcStrLen(str, ShortString_LEN - 1),
		.ptr = (C8*) str,
		.capacity = U64_MAX		//Flag as const
	};
}

inline struct String String_createRefLongStringConst(const LongString str) {
	return (struct String) {
		.len = String_calcStrLen(str, LongString_LEN - 1),
		.ptr = (C8*) str,
		.capacity = U64_MAX		//Flag as const
	};
}

inline struct String String_createRefShortStringDynamic(ShortString str) {
	return (struct String) {
		.len = String_calcStrLen(str, ShortString_LEN - 1),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

inline struct String String_createRefLongStringDynamic(LongString str) {
	return (struct String) {
		.len = String_calcStrLen(str, LongString_LEN - 1),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

//Strings that HAVE to be freed (anything that uses an allocator needs freeing)
//These reside on the heap/free space (or wherever allocator allocates them)
//Which means that they aren't const nor refs

struct Error String_free(struct String *str, struct Allocator alloc);

struct Error String_create(C8 c, U64 size, struct Allocator alloc, struct String *result);
struct Error String_createCopy(struct String str, struct Allocator alloc, struct String *result);

struct Error String_createNyto(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result);
struct Error String_createHex(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result);
struct Error String_createDec(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result);
struct Error String_createOct(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result);
struct Error String_createBin(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result);

struct Error String_split(
	struct String s,
	C8 c, 
	enum StringCase casing, 
	struct Allocator allocator,
	struct StringList *result
);

struct Error String_splitString(
	struct String s,
	struct String other,
	enum StringCase casing,
	struct Allocator allocator,
	struct StringList *result
);

//This will operate on this string, so it will need a heap allocated string

struct Error String_resize(struct String *str, U64 siz, C8 defaultChar, struct Allocator alloc);
struct Error String_reserve(struct String *str, U64 siz, struct Allocator alloc);

struct Error String_append(struct String *s, C8 c, struct Allocator allocator);
struct Error String_appendString(struct String *s, struct String other, struct Allocator allocator);

struct Error String_insert(struct String *s, C8 c, U64 i, struct Allocator allocator);
struct Error String_insertString(struct String *s, struct String other, U64 i, struct Allocator allocator);

struct Error String_replaceAllString(
	struct String *s, 
	struct String search, 
	struct String replace, 
	enum StringCase caseSensitive, 
	struct Allocator allocator
);

struct Error String_replaceString(
	struct String *s, 
	struct String search, 
	struct String replace, 
	enum StringCase caseSensitive,
	struct Allocator allocator,
	Bool isFirst
);

inline struct Error String_replaceFirstString(
	struct String *s,
	struct String search,
	struct String replace,
	enum StringCase caseSensitive,
	struct Allocator allocator
) {
	return String_replaceString(s, search, replace, caseSensitive, allocator, true);
}

inline struct Error String_replaceLastString(
	struct String *s, 
	struct String search, 
	struct String replace, 
	enum StringCase caseSensitive,
	struct Allocator allocator
) {
	return String_replaceString(s, search, replace, caseSensitive, allocator, false);
}

//Simple checks (consts)

inline Bool String_contains(struct String str, C8 c, enum StringCase caseSensitive) { 
	return String_findFirst(str, c, caseSensitive) != U64_MAX;
}

inline Bool String_containsString(struct String str, struct String other, enum StringCase caseSensitive)  { 
	return String_findFirstString(str, other, caseSensitive) != U64_MAX; 
}

inline Bool String_startsWith(struct String str, C8 c, enum StringCase caseSensitive) { 
	return 
		str.len && str.ptr && 
		C8_transform(*str.ptr, (enum StringTransform) caseSensitive) == 
		C8_transform(c, (enum StringTransform) caseSensitive);
}

Bool String_startsWithString(struct String str, struct String other, enum StringCase caseSensitive);

inline Bool String_endsWith(struct String str, C8 c, enum StringCase caseSensitive) {
	return 
		str.len && str.ptr && 
		C8_transform(str.ptr[str.len - 1], (enum StringTransform) caseSensitive) == 
		C8_transform(c, (enum StringTransform) caseSensitive);
}

Bool String_endsWithString(struct String str, struct String other, enum StringCase caseSensitive);

U64 String_countAll(struct String s, C8 c, enum StringCase caseSensitive);
U64 String_countAllString(struct String s, struct String other, enum StringCase caseSensitive);

//Returns the locations (U64[])

struct Error String_findAll(
	struct String s, 
	C8 c, 
	struct Allocator alloc,
	enum StringCase caseSensitive,
	struct List *result
);

struct Error String_findAllString(
	struct String s, 
	struct String other,
	struct Allocator alloc,
	enum StringCase caseSensitive,
	struct List *result
);

//

U64 String_findFirst(struct String s, C8 c, enum StringCase caseSensitive);
U64 String_findLast(struct String s, C8 c, enum StringCase caseSensitive);

inline U64 String_find(struct String s, C8 c, enum StringCase caseSensitive, Bool isFirst) {
	return isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);
}

U64 String_findFirstString(struct String s, struct String other, enum StringCase caseSensitive);
U64 String_findLastString(struct String s, struct String other, enum StringCase caseSensitive);

inline U64 String_findString(struct String s, struct String other, enum StringCase caseSensitive, Bool isFirst) {
	return isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);
}

Bool String_equalsString(struct String s, struct String other, enum StringCase caseSensitive);
Bool String_equals(struct String s, C8 c, enum StringCase caseSensitive);

Bool String_parseNyto(struct String s, U64 *result);
Bool String_parseHex(struct String s, U64 *result);
Bool String_parseDec(struct String s, U64 *result);
Bool String_parseDecSigned(struct String s, I64 *result);
Bool String_parseFloat(struct String s, F32 *result);
Bool String_parseOct(struct String s, U64 *result);
Bool String_parseBin(struct String s, U64 *result);

Bool String_isNyto(struct String s);				//[0-9A-Za-z_$]+
Bool String_isAlphaNumeric(struct String s);		//[0-9A-Za-z_]+
Bool String_isHex(struct String s);					//[0-9A-Fa-f]+
Bool String_isDec(struct String s);					//[0-9]+
Bool String_isOct(struct String s);					//[0-7]+
Bool String_isBin(struct String s);					//[0-1]+

Bool String_isUnsignedNumber(struct String s);		//[+]?[0-9]+
Bool String_isSignedNumber(struct String s);		//[-+]?[0-9]+
Bool String_isFloat(struct String s);				//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

struct Error String_offsetAsRef(struct String s, U64 off, struct String *result);

//Things that perform on this string to reduce overhead

Bool String_cutBegin(struct String s, U64 off, struct String *result);
Bool String_cutEnd(struct String s, U64 siz, struct String *result);
Bool String_cut(struct String s, U64 off, U64 siz, struct String *result);

Bool String_cutAfter(struct String s, C8 c, enum StringCase caseSensitive, Bool isFirst, struct String *result);

inline Bool String_cutAfterLast(struct String s, C8 c, enum StringCase caseSensitive, struct String *result) {
	return String_cutAfter(s, c, caseSensitive, false, result);
}

inline Bool String_cutAfterFirst(struct String s, C8 c, enum StringCase caseSensitive, struct String *result) {
	return String_cutAfter(s, c, caseSensitive, true, result);
}

Bool String_cutAfterString(
	struct String s, struct String other, enum StringCase caseSensitive, Bool isFirst, struct String *result
);

inline Bool String_cutAfterLastString(
	struct String s, struct String other, enum StringCase caseSensitive, struct String *result
) {
	return String_cutAfterString(s, other, caseSensitive, false, result);
}

inline Bool String_cutAfterFirstString(
	struct String s, struct String other, enum StringCase caseSensitive, struct String *result
) {
	return String_cutAfterString(s, other, caseSensitive, true, result);
}

Bool String_cutBefore(struct String s, C8 c, enum StringCase caseSensitive, Bool isFirst, struct String *result);

inline Bool String_cutBeforeLast(struct String s, C8 c, enum StringCase caseSensitive, struct String *result) {
	return String_cutBefore(s, c, caseSensitive, false, result);
}

inline Bool String_cutBeforeFirst(struct String s, C8 c, enum StringCase caseSensitive, struct String *result) {
	return String_cutBefore(s, c, caseSensitive, true, result);
}

Bool String_cutBeforeString(
	struct String s, struct String other, enum StringCase caseSensitive, Bool isFirst, struct String *result
);

inline Bool String_cutBeforeLastString(
	struct String s, struct String other, enum StringCase caseSensitive, struct String *result
) {
	return String_cutBeforeString(s, other, caseSensitive, false, result);
}

inline Bool String_cutBeforeFirstString(
	struct String s, struct String other, enum StringCase caseSensitive, struct String *result
) {
	return String_cutBeforeString(s, other, caseSensitive, true, result);
}

Bool String_eraseAll(struct String *s, C8 c, enum StringCase caseSensitive);
Bool String_eraseAllString(struct String *s, struct String other, enum StringCase caseSensitive);

Bool String_erase(struct String *s, C8 c, enum StringCase caseSensitive, Bool isFirst);

inline Bool String_eraseFirst(struct String *s, C8 c, enum StringCase caseSensitive) {
	return String_erase(s, c, caseSensitive, true);
}

inline Bool String_eraseLast(struct String *s, C8 c, enum StringCase caseSensitive) {
	return String_erase(s, c, caseSensitive, false);
}

Bool String_eraseString(struct String *s, struct String other, enum StringCase caseSensitive, Bool isFirst);

Bool String_eraseFirstString(struct String *s, struct String other, enum StringCase caseSensitive){
	return String_eraseString(s, other, caseSensitive, true);
}

Bool String_eraseLastString(struct String *s, struct String other, enum StringCase caseSensitive) {
	return String_eraseString(s, other, caseSensitive, false);
}

Bool String_replaceAll(struct String *s, C8 c, C8 v, enum StringCase caseSensitive);
Bool String_replace(struct String *s, C8 c, C8 v, enum StringCase caseSensitive, Bool isFirst);

inline Bool String_replaceFirst(struct String *s, C8 c, C8 v, enum StringCase caseSensitive) {
	return String_replace(s, c, v, caseSensitive, true);
}

Bool String_replaceLast(struct String *s, C8 c, C8 v, enum StringCase caseSensitive) {
	return String_replace(s, c, v, caseSensitive, false);
}

struct String String_trim(struct String s);		//Returns a substring ref in a string

Bool String_transform(struct String s, enum StringTransform stringTransform);

inline Bool String_toLower(struct String str) {
	return String_transform(str, StringTransform_Lower);
}

inline Bool String_toUpper(struct String str) {
	return String_transform(str, StringTransform_Upper);
}

//TODO: Regex

//String list
//To return from string operations
//These strings will be a ref into existing string to prevent copies, use StringList_createCopy to move them to the heap.
//Because they contain mixed strings, the functions still need allocators to free heap strings.
//This is different than List<String> because that one won't enforce string allocation properly.

struct StringList {
	U64 len;
	struct String *ptr;
};

struct Error StringList_create(U64 len, struct Allocator alloc, struct StringList *result);
struct Error StringList_free(struct StringList *arr, struct Allocator alloc);

struct Error StringList_createCopy(const struct StringList *toCopy, struct StringList **arr, struct Allocator alloc);

//Store the string directly into StringList (no copy)
//The allocator is used to free strings if they are heap allocated

struct Error StringList_set(struct StringList arr, U64 i, struct String str, struct Allocator alloc);
struct Error StringList_unset(struct StringList arr, U64 i, struct Allocator alloc);
