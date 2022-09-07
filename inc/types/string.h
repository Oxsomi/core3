#pragma once
#include "hash.h"

//For simplicity;
//A string is ALWAYS ASCII (7-bit) and null terminator. (For now)
//Size of string is cached for speed.
//
//There are four types of strings:
// 
//Stack strings (or heap if you manually allocate it there):
//	ShortString; 32 chars max (or 33 if limit is reached; null terminator is assumed)
//	LongString; 64 char max (or 65 if limit is reached)
//
//String; A string that goes on the heap (or wherever the allocator tells it to go)
//String(Ref); A reference to an already allocated string (String with capacity 0)
//String(Ref) const; A const reference that should not allow operations on it (String with capacity -1)

//Stack strings that are faster and easier to allocate

#define ShortString_LEN 32
#define LongString_LEN 64

typedef c8 ShortString[ShortString_LEN];
typedef c8 LongString[LongString_LEN];

//Heap string

struct String {
	usz len, capacity;
	c8 *ptr;
};

enum StringCase {
	StringCase_Sensitive,
	StringCase_Insensitive
};

enum StringTransform {
	StringTransform_None,
	StringTransform_Lower,
	StringTransform_Upper
};

//Simple helper functions (inlines)

inline bool String_isConstRef(struct String str) { return str.capacity == usz_MAX; }
inline bool String_isRef(struct String str) { return !str.capacity || String_isConstRef(str); }
inline bool String_isEmpty(struct String str) { return !str.len || !str.ptr; }

//Iteration

inline c8 *String_begin(struct String str) { 
	return String_isConstRef(str) ? NULL : str.ptr; 
}

inline c8 *String_end(struct String str) {
	return String_isConstRef(str) ? NULL : str.ptr + str.len;
}

inline c8 *String_charAt(struct String str, usz off) { 
	return String_isConstRef(str) || off >= str.len ? NULL : str.ptr + off; 
}

inline const c8 *String_beginConst(struct String str) { 
	return str.ptr; 
}

inline const c8 *String_endConst(struct String str) {
	return str.ptr + str.len;
}

inline const c8 *String_charAt(struct String str, usz off) { 
	return off >= str.len ? NULL : str.ptr + off; 
}

//

inline void String_clear(struct String *str) { 
	if(str && !String_isConstRef(*str)) 
		str->len = 0; 
}

inline usz String_calcStrLen(const c8 *ptr, usz maxSize) {

	usz i = 0;

	for(; i < maxSize && ptr[i]; ++i)
		;

	return i;
}

inline u64 String_hash(struct String s) { 

	u64 h = FNVHash_create();

	const u64 *ptr = (const u64*)s.ptr, *end = ptr + (s.len >> 3);

	for(; ptr < end; ++ptr)
		h = FNVHash_apply(h, *ptr);
		
	u64 last = 0, shift = 0;

	if (s.len & 4) {
		last = *(const u32*) ptr;
		shift = 4;
		ptr = (const u32*)ptr + 1;
	}
		
	if (s.len & 2) {
		last |= (u64)(*(const u16*)ptr) << shift;
		shift |= 2;
		ptr = (const u16*)ptr + 1;
	}
		
	if (s.len & 1)
		last |= (u64)(*(const u8*)ptr) << shift++;

	if(shift)
		h = FNVHash_apply(h, last);

	return h;
}

inline c8 String_getAt(struct String str, usz i) { return i < str.len ? str.ptr[i] : 0;  }

inline bool String_setAt(struct String str, usz i, c8 c) { 

	if(i >= str.len || String_isConstRef(str))
		return false;

	str.ptr[i] = c;
	return true;
}

//Refs shouldn't be freed and are only so string functions
//can easily be used. The const versions disallow modifying functions to be used.
//Freeing them is however fine, but won't do anything besides clearing the String variable.

struct String String_createRefConst(const c8 *ptr, usz maxSize) { 
	return (struct String) { 
		.len = String_calcStrLen(ptr, maxSize),
		.ptr = ptr,
		.capacity = usz_MAX		//Flag as const
	};
}

struct String String_createRefUnsafeConst(const c8 *ptr) {		//Only use this if string is created safely in code
	return String_createRefConst(ptr, usz_MAX);
}

struct String String_createRefDynamic(c8 *ptr, usz maxSize) { 
	return (struct String) { 
		.len = String_calcStrLen(ptr, maxSize),
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

struct String String_createRefSizedConst(const c8 *ptr, usz size) {
	return (struct String) { 
		.len = size,
		.ptr = ptr,
		.capacity = usz_MAX		//Flag as const
	};
}

struct String String_createRefSizedDynamic(c8 *ptr, usz size) {
	return (struct String) { 
		.len = size,
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

struct String String_createRefShortStringConst(const ShortString str) {
	return (struct String) {
		.len = String_calcStrLen(str, ShortString_LEN),
		.ptr = str,
		.capacity = usz_MAX		//Flag as const
	};
}

struct String String_createRefLongStringConst(const LongString str) {
	return (struct String) {
		.len = String_calcStrLen(str, LongString_LEN),
		.ptr = str,
		.capacity = usz_MAX		//Flag as const
	};
}

struct String String_createRefShortStringDynamic(ShortString str) {
	return (struct String) {
		.len = String_calcStrLen(str, ShortString_LEN),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

struct String String_createRefLongStringDynamic(LongString str) {
	return (struct String) {
		.len = String_calcStrLen(str, LongString_LEN),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

//Strings that have to be freed (anything that uses an allocator needs freeing)
//These reside on the heap (or wherever allocator allocates them)
//Which means that they aren't const nor refs

struct String String_create(c8 c, usz size, struct Allocator alloc);
struct String String_createCopy(struct String str, struct Allocator alloc);

struct Error String_free(struct String **str, struct Allocator alloc);

struct String String_createNyto(u64 v, usz leadingZeros, struct Allocator allocator);		//Nytodecimal
struct String String_createHex(u64 v, usz leadingZeros, struct Allocator allocator);
struct String String_createDec(u64 v, usz leadingZeros, struct Allocator allocator);
struct String String_createOctal(u64 v, usz leadingZeros, struct Allocator allocator);
struct String String_createBinary(u64 v, usz leadingZeros, struct Allocator allocator);

struct StringList String_split(struct String s, c8 c, struct Allocator allocator);
struct StringList String_splitString(struct String s, struct String other, struct Allocator allocator);

//This will operate on this string, so it will need a heap allocated string

struct String String_resize(struct String str, usz siz, struct Allocator alloc);
struct String String_reserve(struct String str, usz siz, struct Allocator alloc);

void String_append(struct String s, c8 c, struct Allocator allocator);
void String_appendString(struct String s, struct String other, struct Allocator allocator);

void String_insert(struct String s, c8 c, usz i, struct Allocator allocator);
void String_insertString(struct String s, struct String other, usz i, struct Allocator allocator);

struct String String_replaceAllString(
	struct String s, 
	struct String search, 
	struct String replace, 
	enum StringCase caseSensitive, 
	struct Allocator allocator
);

struct String String_replaceFirstString(
	struct String s, 
	struct String search, 
	struct String replace, 
	enum StringCase caseSensitive,
	struct Allocator allocator
);

struct String String_replaceLastString(
	struct String s, 
	struct String search, 
	struct String replace, 
	enum StringCase caseSensitive,
	struct Allocator allocator
);

//Simple checks (consts)

bool String_contains(struct String str, c8 c, enum StringCase caseSensitive);
bool String_containsString(struct String str, struct String other, enum StringCase caseSensitive);

c8 String_charToLower(c8 c);
c8 String_charToUpper(c8 c);
c8 String_charTransform(c8 c, enum StringTransform transform);

inline bool String_startsWith(struct String str, c8 c, enum StringCase caseSensitive) { 
	return 
		str.len && str.ptr && 
		String_charTransform(*str.ptr, (enum StringTransform) caseSensitive) == 
		String_charTransform(c, (enum StringTransform) caseSensitive);
}

bool String_startsWithString(struct String str, struct String other, enum StringCase caseSensitive);

inline bool String_endsWith(struct String str, c8 c, enum StringCase caseSensitive) {
	return 
		str.len && str.ptr && 
		String_charTransform(str.ptr[str.len - 1], (enum StringTransform) caseSensitive) == 
		String_charTransform(c, (enum StringTransform) caseSensitive);
}

bool String_endsWithString(struct String str, struct String other, enum StringCase caseSensitive);

usz String_countAll(struct String s, c8 c, enum StringCase caseSensitive);
usz String_countAllString(struct String s, struct String other, enum StringCase caseSensitive);

//Returns the locations (usz[])

struct Buffer String_findAll(
	struct String s, 
	c8 c, 
	struct Allocator alloc,
	enum StringCase caseSensitive
);

struct Buffer String_findAllString(
	struct String s, 
	struct String other,
	struct Allocator alloc,
	enum StringCase caseSensitive
);

//

usz String_findFirst(struct String s, c8 c, enum StringCase caseSensitive);
usz String_findFirstString(struct String s, struct String other, enum StringCase caseSensitive);
usz String_findLast(struct String s, c8 c, enum StringCase caseSensitive);
usz String_findLastString(struct String s, struct String other, enum StringCase caseSensitive);

bool String_equalsString(struct String s, struct String other, enum StringCase caseSensitive);
bool String_equals(struct String s, c8 c, enum StringCase caseSensitive);

u64 String_parseNyto(struct String s);
u64 String_parseHex(struct String s);
u64 String_parseDec(struct String s);
i64 String_parseDecSigned(struct String s);
f64 String_parseFloat(struct String s);
u64 String_parseOctal(struct String s);
u64 String_parseBinary(struct String s);

bool String_isNytoDecimal(struct String s);			//[0-9A-Za-z_$]+
bool String_isAlphaNumeric(struct String s);		//[0-9A-Za-z_]+
bool String_isHex(struct String s);					//[0-9A-Fa-f_]+
bool String_isUnsignedNumber(struct String s);		//[0-9]+
bool String_isSignedNumber(struct String s);		//[-+]?[0-9]+
bool String_isFloat(struct String s);				//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[e[-+]?[0-9]+]?

struct Error String_offsetAsRef(struct String s, usz off, struct String *result);

//Things that perform on this string to reduce overhead

bool String_cutBegin(struct String *s, usz offset);
bool String_cutEnd(struct String *s, usz siz);
bool String_cut(struct String *s, usz offset, usz siz);

bool String_cutAfterLast(struct String *s, c8 c, enum StringCase caseSensitive);
bool String_cutAfterFirst(struct String *s, c8 c, enum StringCase caseSensitive);
bool String_cutAfterLastString(struct String *s, struct String other, enum StringCase caseSensitive);
bool String_cutAfterFirstString(struct String *s, struct String other, enum StringCase caseSensitive);

bool String_cutBeforeLast(struct String *s, c8 c, enum StringCase caseSensitive);
bool String_cutBeforeFirst(struct String *s, c8 c, enum StringCase caseSensitive);
bool String_cutBeforeLastString(struct String *s, struct String other, enum StringCase caseSensitive);
bool String_cutBeforeFirstString(struct String *s, struct String other, enum StringCase caseSensitive);

bool String_eraseAll(struct String *s, c8 c, enum StringCase caseSensitive);
bool String_eraseAllString(struct String *s, struct String other, enum StringCase caseSensitive);

bool String_eraseFirst(struct String *s, c8 c, enum StringCase caseSensitive);
bool String_eraseLast(struct String *s, c8 c, enum StringCase caseSensitive);
bool String_eraseFirstString(struct String *s, struct String other, enum StringCase caseSensitive);
bool String_eraseLastString(struct String *s, struct String other, enum StringCase caseSensitive);

void String_replaceAll(struct String *s, c8 c, c8 v, enum StringCase caseSensitive);
void String_replaceFirst(struct String *s, c8 c, c8 v, enum StringCase caseSensitive);
void String_replaceLast(struct String *s, c8 c, c8 v, enum StringCase caseSensitive);

bool String_trim(struct String *s);		//Removes padding of string

bool String_toLower(struct String *s);
bool String_toUpper(struct String *s);

//TODO: Regex

//String list
//To return from string operations
//These strings will be on the heap in that case, even if done on a (const) ref.
//Because otherwise it might be possible that it references a now deleted string.
//If you manage them yourself, you can use refs or not though.

struct StringList {
	usz len;
	struct String *ptr;
};

struct Error StringList_create(usz len, struct Allocator alloc, struct StringList **result);
struct Error StringList_free(struct StringList **arr, struct Allocator alloc);

//Store the string directly into StringList (no copy)
//The allocator is used to free strings if referenced

struct Error StringList_set(struct StringList arr, usz i, struct String str);
struct Error StringList_unset(struct StringList arr, usz i, struct Allocator alloc, struct Allocator alloc);
