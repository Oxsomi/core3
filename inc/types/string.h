#pragma once
#include "hash.h"
#include "assert.h"

//For simplicity;
//A string is ALWAYS ASCII (7-bit).

//Stack strings that are faster and easier to allocate

typedef c8 ShortString[32];
typedef c8 LongString[64];

//Heap string

struct String {
	usz len, capacity;
	c8 *ptr;
};

//Simple helper functions (inlines)

inline bool String_isRef(struct String str) { return !str.capacity; }
inline bool String_isEmpty(struct String str) { return !str.len; }

inline c8 *String_begin(struct String str) { return str.ptr; }
inline c8 *String_end(struct String str) { return str.ptr + str.len; }
inline c8 *String_charAt(struct String str, usz off) { return str.ptr + off; }

inline void String_clear(struct String *str) { str->len = 0; }

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
		
	if (s.len & 4) {
		h = FNVHash_apply(h, *(const u32*)ptr);
		ptr = (const u32*)ptr + 1;
	}
		
	if (s.len & 2) {
		h = FNVHash_apply(h, *(const u16*)ptr);
		ptr = (const u16*)ptr + 1;
	}
		
	if (s.len & 1)
		h = FNVHash_apply(h, *(const u8*)ptr);

	return h;
}

inline c8 String_getAt(struct String str, usz i) {
	ocAssert("String out of bounds", str.len);
	return str.ptr[i]; 
}

inline void String_setAt(struct String str, usz i, c8 c) {
	ocAssert("String out of bounds", str.len);
	str.ptr[i] = c;
}

//Refs shouldn't be freed and are only so string functions
//can easily be used for readonly functions, but no write functions.
//And so the size is easily cached

struct String String_createRef(const c8 *ptr, usz maxSize) { 
	return (struct String) { 
		.len = String_calcStrLen(ptr, maxSize),
		.ptr = ptr
	};
}

struct String String_createRefSized(const c8 *ptr, usz size) {
	return (struct String) { 
		.len = size,
		.ptr = ptr
	};
}

struct String String_createRefShortString(const ShortString str) {
	return (struct String) {
		.len = String_calcStrLen(str, 32),
		.ptr = str
	};
}

struct String String_createRefLongString(const LongString str) {
	return (struct String) {
		.len = String_calcStrLen(str, 64),
		.ptr = str
	};
}

//Strings that have to be freed (anything that uses an allocator needs freeing)

struct String String_create(c8 c, usz size, struct Allocator alloc);
struct String String_createCopy(const c8 *ptr, usz maxSize, struct Allocator alloc);
struct String String_createCopySized(const c8 *ptr, usz size, struct Allocator alloc);

struct String String_copy(struct String str, struct Allocator alloc);

void String_free(struct String *str, struct Allocator alloc);

struct String String_resize(struct String str, usz siz, struct Allocator alloc);
struct String String_reserve(struct String str, usz siz, struct Allocator alloc);

struct String String_createHex(u64 v, usz leadingZeros, struct Allocator allocator);
struct String String_createDec(u64 v, usz leadingZeros, struct Allocator allocator);
struct String String_createOctal(u64 v, usz leadingZeros, struct Allocator allocator);
struct String String_createBinary(u64 v, usz leadingZeros, struct Allocator allocator);

struct StringList String_split(struct String s, c8 c, struct Allocator allocator);
struct StringList String_splitString(struct String s, struct String other, struct Allocator allocator);

struct String String_replaceAllString(struct String s, struct String other, struct Allocator allocator);
struct String String_replaceFirstString(struct String s, struct String other, struct Allocator allocator);
struct String String_replaceLastString(struct String s, struct String other, struct Allocator allocator);

struct String String_replaceAllStringIgnoreCase(struct String s, struct String other, struct Allocator allocator);
struct String String_replaceFirstStringIgnoreCase(struct String s, struct String other, struct Allocator allocator);
struct String String_replaceLastStringIgnoreCase(struct String s, struct String other, struct Allocator allocator);

void String_append(struct String s, c8 c, struct Allocator allocator);
void String_appendString(struct String s, struct String other, struct Allocator allocator);

void String_insert(struct String s, c8 c, usz i, struct Allocator allocator);
void String_insertString(struct String s, struct String other, usz i, struct Allocator allocator);

//Simple checks (consts)

bool String_contains(struct String str, c8 c);
bool String_containsString(struct String str, struct String other);
bool String_containsIgnoreCase(struct String str, c8 c);
bool String_containsStringIgnoreCase(struct String str, struct String other);

inline bool String_startsWith(struct String str, c8 c) {
	ocAssert("String out of bounds", str.len);
	return *str.ptr == c;
}

bool String_startsWithString(struct String str, struct String other);
bool String_startsWithIgnoreCase(struct String str, c8 c);
bool String_startsWithStringIgnoreCase(struct String str, struct String other);

inline bool String_endsWith(struct String str, c8 c) {
	ocAssert("String out of bounds", str.len);
	return str.len && str.ptr[str.len - 1] == c;
}

bool String_endsWithString(struct String str, struct String other);
bool String_endsWithIgnoreCase(struct String str, c8 c);
bool String_endsWithStringIgnoreCase(struct String str, struct String other);

struct StringList String_findAll(struct String s, c8 c);
struct StringList String_findAllString(struct String s, struct String other);

usz String_findFirst(struct String s, c8 c);
usz String_findFirstString(struct String s, struct String other);
usz String_findLast(struct String s, c8 c);
usz String_findLastString(struct String s, struct String other);

usz String_findFirstIgnoreCase(struct String s, c8 c);
usz String_findFirstStringIgnoreCase(struct String s, struct String other);
usz String_findLastIgnoreCase(struct String s, c8 c);
usz String_findLastStringIgnoreCase(struct String s, struct String other);

bool String_equalsString(struct String s, struct String other);
bool String_equalsIgnoreCaseString(struct String s, struct String other);

bool String_equals(struct String s, c8 c);
bool String_equalsIgnoreCase(struct String s, c8 c);

u64 String_parseHex(struct String s);
u64 String_parseDec(struct String s);
i64 String_parseDecSigned(struct String s);
f64 String_parseFloat(struct String s);
u64 String_parseOctal(struct String s);
u64 String_parseBinary(struct String s);

bool String_isAlphaNumeric(struct String s);		//[0-9A-Za-z_]+
bool String_isUnsignedNumber(struct String s);		//[0-9]+
bool String_isSignedNumber(struct String s);		//[-+]?[0-9]+
bool String_isFloat(struct String s);				//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[e[-+]?[0-9]+]?

struct String String_offsetRef(struct String s, usz off);

//Things that perform on this string to reduce overhead

bool String_cutBegin(struct String *s, usz offset);
bool String_cutEnd(struct String *s, usz siz);
bool String_cut(struct String *s, usz offset, usz siz);

bool String_cutAfterLast(struct String *s, c8 c);
bool String_cutAfterFirst(struct String *s, c8 c);
bool String_cutAfterLastString(struct String *s, struct String other);
bool String_cutAfterFirstString(struct String *s, struct String other);

bool String_cutBeforeLast(struct String *s, c8 c);
bool String_cutBeforeFirst(struct String *s, c8 c);
bool String_cutBeforeLastString(struct String *s, struct String other);
bool String_cutBeforeFirstString(struct String *s, struct String other);

bool String_cutAfterLastIgnoreCase(struct String *s, c8 c);
bool String_cutAfterFirstIgnoreCase(struct String *s, c8 c);
bool String_cutAfterLastStringIgnoreCase(struct String *s, struct String other);
bool String_cutAfterFirstStringIgnoreCase(struct String *s, struct String other);

bool String_cutBeforeLastIgnoreCase(struct String *s, c8 c);
bool String_cutBeforeFirstIgnoreCase(struct String *s, c8 c);
bool String_cutBeforeLastStringIgnoreCase(struct String *s, struct String other);
bool String_cutBeforeFirstStringIgnoreCase(struct String *s, struct String other);

bool String_eraseAll(struct String *s, c8 c);
bool String_eraseAllString(struct String *s, struct String other);
bool String_eraseAllIgnoreCase(struct String *s, c8 c);
bool String_eraseAllStringIgnoreCase(struct String *s, struct String other);

bool String_eraseFirst(struct String *s, c8 c);
bool String_eraseLast(struct String *s, c8 c);
bool String_eraseFirstString(struct String *s, struct String other);
bool String_eraseLastString(struct String *s, struct String other);

bool String_eraseFirstIgnoreCase(struct String *s, c8 c);
bool String_eraseLastIgnoreCase(struct String *s, c8 c);
bool String_eraseFirstStringIgnoreCase(struct String *s, struct String other);
bool String_eraseLastStringIgnoreCase(struct String *s, struct String other);

void String_replaceAll(struct String *s, c8 c, c8 v);
void String_replaceFirst(struct String *s, c8 c, c8 v);
void String_replaceLast(struct String *s, c8 c, c8 v);

void String_replaceAllIgnoreCase(struct String *s, c8 c, c8 v);
void String_replaceFirstIgnoreCase(struct String *s, c8 c, c8 v);
void String_replaceLastIgnoreCase(struct String *s, c8 c, c8 v);

void String_trim(struct String *s);

void String_toLower(struct String *s);
void String_toUpper(struct String *s);

//TODO: Regex

//Heap string list
//To return from 

struct StringList {
	usz len;
	struct String *ptr;
};

struct StringList StringList_create(usz len, struct Allocator alloc);
void StringList_free(struct StringList *arr, struct Allocator alloc);

//Give ownership to StringList (to avoid that, create a string ref)

void StringList_set(struct StringList arr, usz i, struct String str, struct Allocator alloc);
void StringList_unset(struct StringList arr, usz i, struct Allocator alloc);