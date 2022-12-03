#pragma once
#include "platforms/platform.h"
#include "types/string.h"

Bool String_freex(String *str);

Error String_createx(C8 c, U64 size, String *result);
Error String_createCopyx(String str, String *result);
Error String_createNytox(U64 v, Bool leadingZeros, String *result);
Error String_createHexx(U64 v, Bool leadingZeros, String *result);
Error String_createDecx(U64 v, Bool leadingZeros, String *result);
Error String_createOctx(U64 v, Bool leadingZeros, String *result);
Error String_createBinx(U64 v, Bool leadingZeros, String *result);

Error String_splitx(String s, C8 c, EStringCase casing, StringList *result);
Error String_splitStringx(String s, String other, EStringCase casing, StringList *result);

Error String_resizex(String *str, U64 length, C8 defaultChar);
Error String_reservex(String *str, U64 length);

Error String_appendx(String *s, C8 c);
Error String_appendStringx(String *s, String other);

Error String_insertx(String *s, C8 c, U64 i);
Error String_insertStringx(String *s, String other, U64 i);

Error String_replaceAllStringx(String *s, String search, String replace, EStringCase caseSensitive);

Error String_replaceStringx(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Bool isFirst
);

Error String_replaceFirstStringx(String *s, String search, String replace, EStringCase caseSensitive);
Error String_replaceLastStringx(String *s, String search, String replace, EStringCase caseSensitive);

List String_findAllx(String s, C8 c, EStringCase caseSensitive);
List String_findAllStringx(String s, String other, EStringCase caseSensitive);

Bool StringList_freex(StringList *arr);

Error StringList_createx(U64 length, StringList *result);
Error StringList_createCopyx(const StringList *toCopy, StringList *arr);

Error StringList_setx(StringList arr, U64 i, String str);
Error StringList_unsetx(StringList arr, U64 i);

Error StringList_combinex(StringList arr, String *result);

Error StringList_concatx(StringList arr, C8 between, String *result);
Error StringList_concatStringx(StringList arr, String between, String *result);
