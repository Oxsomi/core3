#pragma once
#include "platforms/platform.h"
#include "types/string.h"

inline Error String_freex(String *str) {
	return String_free(str, Platform_instance.alloc);
}

inline Error String_createx(C8 c, U64 size, String *result) {
	return String_create(c, size, Platform_instance.alloc, result);
}

inline Error String_createCopyx(String str, String *result) {
	return String_createCopy(str, Platform_instance.alloc, result);
}

inline Error String_createNytox(U64 v, Bool leadingZeros, String *result) {
	return String_createNyto(v, leadingZeros, Platform_instance.alloc, result);
}

inline Error String_createHexx(U64 v, Bool leadingZeros, String *result) {
	return String_createHex(v, leadingZeros, Platform_instance.alloc, result);
}

inline Error String_createDecx(U64 v, Bool leadingZeros, String *result) {
	return String_createDec(v, leadingZeros, Platform_instance.alloc, result);
}

inline Error String_createOctx(U64 v, Bool leadingZeros, String *result) {
	return String_createOct(v, leadingZeros, Platform_instance.alloc, result);
}

inline Error String_createBinx(U64 v, Bool leadingZeros, String *result) {
	return String_createBin(v, leadingZeros, Platform_instance.alloc, result);
}

inline Error String_splitx(
	String s,
	C8 c, 
	EStringCase casing, 
	StringList *result
) {
	return String_split(s, c, casing, Platform_instance.alloc, result);
}

inline Error String_splitStringx(
	String s,
	String other,
	EStringCase casing,
	StringList *result
) {
	return String_splitString(s, other, casing, Platform_instance.alloc, result);
}

inline Error String_resizex(String *str, U64 siz, C8 defaultChar) {
	return String_resize(str, siz, defaultChar, Platform_instance.alloc);
}

inline Error String_reservex(String *str, U64 siz) {
	return String_reserve(str, siz, Platform_instance.alloc);
}

inline Error String_appendx(String *s, C8 c) {
	return String_append(s, c, Platform_instance.alloc);
}

inline Error String_appendStringx(String *s, String other) {
	return String_appendString(s, other, Platform_instance.alloc);
}

inline Error String_insertx(String *s, C8 c, U64 i) {
	return String_insert(s, c, i, Platform_instance.alloc);
}

inline Error String_insertStringx(String *s, String other, U64 i) {
	return String_insertString(s, other, i, Platform_instance.alloc);
}

inline Error String_replaceAllStringx(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive
) {
	return String_replaceAllString(s, search, replace, caseSensitive, Platform_instance.alloc);
}

inline Error String_replaceStringx(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Bool isFirst
) {
	return String_replaceString(s, search, replace, caseSensitive, Platform_instance.alloc, isFirst);
}

inline Error String_replaceFirstStringx(
	String *s,
	String search,
	String replace,
	EStringCase caseSensitive
) {
	return String_replaceStringx(s, search, replace, caseSensitive, true);
}

inline Error String_replaceLastStringx(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive
) {
	return String_replaceStringx(s, search, replace, caseSensitive, false);
}

inline List String_findAllx(String s, C8 c, EStringCase caseSensitive) {
	return String_findAll(s, c, Platform_instance.alloc, caseSensitive);
}

inline List String_findAllStringx(String s, String other, EStringCase caseSensitive) {
	return String_findAllString(s, other, Platform_instance.alloc, caseSensitive);
}

inline Error StringList_createx(U64 len, StringList *result) {
	return StringList_create(len, Platform_instance.alloc, result);
}

inline Error StringList_freex(StringList *arr) {
	return StringList_free(arr, Platform_instance.alloc);
}

inline Error StringList_createCopyx(const StringList *toCopy, StringList *arr) {
	return StringList_createCopy(toCopy, Platform_instance.alloc, arr);
}

inline Error StringList_setx(StringList arr, U64 i, String str) {
	return StringList_set(arr, i, str, Platform_instance.alloc);
}

inline Error StringList_unsetx(StringList arr, U64 i) {
	return StringList_unset(arr, i, Platform_instance.alloc);
}

inline Error StringList_combinex(StringList arr, String *result) {
	return StringList_combine(arr, Platform_instance.alloc, result);
}

inline Error StringList_concatx(StringList arr, C8 between, String *result) {
	return StringList_concat(arr, between, Platform_instance.alloc, result);
}

inline Error StringList_concatStringx(StringList arr, String between, String *result) {
	return StringList_concatString(arr, between, Platform_instance.alloc, result);
}
