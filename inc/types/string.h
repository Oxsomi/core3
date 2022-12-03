#pragma once
#include "types.h"

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

#define _SHORTSTRING_LEN 32
#define _LONGSTRING_LEN 64

typedef C8 ShortString[_SHORTSTRING_LEN];
typedef C8 LongString[_LONGSTRING_LEN];

typedef struct List List;
typedef struct Buffer Buffer;
typedef struct Allocator Allocator;
typedef struct Error Error;

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

Bool String_isConstRef(String str);
Bool String_isRef(String str);
Bool String_isEmpty(String str);
U64  String_bytes(String str);

Buffer String_buffer(String str);

//Iteration

C8 *String_begin(String str);
C8 *String_end(String str);

C8 *String_charAt(String str, U64 off);

const C8 *String_beginConst(String str);
const C8 *String_endConst(String str);

const C8 *String_charAtConst(String str, U64 off);
Bool String_isValidAscii(String str);
Bool String_isValidFileName(String str, Bool acceptTrailingNull);

//Only checks characters. Please use resolvePath to actually validate

Bool String_isValidFilePath(String str);
void String_clear(String *str);

U64 String_calcStrLen(const C8 *ptr, U64 maxSize);
//U64 String_hash(String s);								TODO:

C8 String_getAt(String str, U64 i);
Bool String_setAt(String str, U64 i, C8 c);

//Freeing refs won't do anything, but is still recommended for consistency. 
//Const ref disallow modifying functions to be used.

String String_createNull();

String String_createConstRef(const C8 *ptr, U64 maxSize);

//Only use this if string is created safely in code

String String_createConstRefUnsafe(const C8 *ptr);
String String_createRef(C8 *ptr, U64 maxSize);

String String_createConstRefSized(const C8 *ptr, U64 size);
String String_createRefSized(C8 *ptr, U64 size);

String String_createConstRefShortString(const ShortString str);
String String_createConstRefLongString(const LongString str);

String String_createRefShortString(ShortString str);
String String_createRefLongString(LongString str);

//Strings that HAVE to be freed (anything that uses an allocator needs freeing)
//These reside on the heap/free space (or wherever allocator allocates them)
//Which means that they aren't const nor refs

Bool String_free(String *str, Allocator alloc);

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

Error String_replaceFirstString(
	String *s,
	String search,
	String replace,
	EStringCase caseSensitive,
	Allocator allocator
);

Error String_replaceLastString(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Allocator allocator
);

//Simple checks (consts)

Bool String_startsWith(String str, C8 c, EStringCase caseSensitive);
Bool String_startsWithString(String str, String other, EStringCase caseSensitive);

Bool String_endsWith(String str, C8 c, EStringCase caseSensitive);
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

U64 String_find(String s, C8 c, EStringCase caseSensitive, Bool isFirst);

U64 String_findFirstString(String s, String other, EStringCase caseSensitive);
U64 String_findLastString(String s, String other, EStringCase caseSensitive);
U64 String_findString(String s, String other, EStringCase caseSensitive, Bool isFirst);

Bool String_contains(String str, C8 c, EStringCase caseSensitive);
Bool String_containsString(String str, String other, EStringCase caseSensitive);

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
Bool String_isAlphaNumeric(String s);		//[0-9A-Za-z]+
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

Bool String_cutAfter(
	String s, 
	C8 c, 
	EStringCase caseSensitive, 
	Bool isFirst, 
	String *result
);

Bool String_cutAfterLast(String s, C8 c, EStringCase caseSensitive, String *result);
Bool String_cutAfterFirst(String s, C8 c, EStringCase caseSensitive, String *result);

Bool String_cutAfterString(
	String s,
	String other,
	EStringCase caseSensitive,
	Bool isFirst,
	String *result
);

Bool String_cutAfterLastString(String s, String other, EStringCase caseSensitive, String *result);
Bool String_cutAfterFirstString(String s, String other, EStringCase caseSensitive, String *result);

Bool String_cutBefore(
	String s, 
	C8 c, 
	EStringCase caseSensitive, 
	Bool isFirst, 
	String *result
);

Bool String_cutBeforeLast(String s, C8 c, EStringCase caseSensitive, String *result);
Bool String_cutBeforeFirst(String s, C8 c, EStringCase caseSensitive, String *result);

Bool String_cutBeforeString(
	String s, 
	String other, 
	EStringCase caseSensitive, 
	Bool isFirst, 
	String *result
);

Bool String_cutBeforeLastString(String s, String other, EStringCase caseSensitive, String *result);
Bool String_cutBeforeFirstString(String s, String other, EStringCase caseSensitive, String *result);

Error String_eraseAtCount(String *s, U64 i, U64 count);
Error String_popFrontCount(String *s, U64 count);
Error String_popEndCount(String *s, U64 count);

Error String_eraseAt(String *s, U64 i);
Error String_popFront(String *s);
Error String_popEnd(String *s);

Bool String_eraseAll(String *s, C8 c, EStringCase caseSensitive);
Bool String_eraseAllString(String *s, String other, EStringCase caseSensitive);

Bool String_erase(String *s, C8 c, EStringCase caseSensitive, Bool isFirst);

Bool String_eraseFirst(String *s, C8 c, EStringCase caseSensitive);
Bool String_eraseLast(String *s, C8 c, EStringCase caseSensitive);

Bool String_eraseString(String *s, String other, EStringCase caseSensitive, Bool isFirst);

Bool String_eraseFirstString(String *s, String other, EStringCase caseSensitive);
Bool String_eraseLastString(String *s, String other, EStringCase caseSensitive);

Bool String_replaceAll(String *s, C8 c, C8 v, EStringCase caseSensitive);

Bool String_replace(
	String *s, 
	C8 c, 
	C8 v, 
	EStringCase caseSensitive, 
	Bool isFirst
);

Bool String_replaceFirst(String *s, C8 c, C8 v, EStringCase caseSensitive);
Bool String_replaceLast(String *s, C8 c, C8 v, EStringCase caseSensitive);

String String_trim(String s);		//Returns a substring ref in a string

Bool String_transform(String s, EStringTransform stringTransform);

Bool String_toLower(String str);
Bool String_toUpper(String str);

//Simple file utils

Bool String_formatPath(String *str);

String String_getFilePath(String *str);	//Formats on string first to ensure it's proper
String String_getBasePath(String *str);	//Formats on string first to ensure it's proper

//TODO: Regex

//String list
//To return from string operations
//These strings will be a ref into existing string to prevent copies, use StringList_createCopy to move them to the heap.
//Because they contain mixed strings, the functions still need allocators to free heap strings.
//This is different than List<String> because that one won't enforce string allocation properly.

Error StringList_create(U64 length, Allocator alloc, StringList *result);
Bool StringList_free(StringList *arr, Allocator alloc);

Error StringList_createCopy(const StringList *toCopy, Allocator alloc, StringList *arr);

//Store the string directly into StringList (no copy)
//The allocator is used to free strings if they are heap allocated

Error StringList_set(StringList arr, U64 i, String str, Allocator alloc);
Error StringList_unset(StringList arr, U64 i, Allocator alloc);

//Combining all strings into one

Error StringList_combine(StringList arr, Allocator alloc, String *result);

Error StringList_concat(StringList arr, C8 between, Allocator alloc, String *result);
Error StringList_concatString(StringList arr, String between, Allocator alloc, String *result);
