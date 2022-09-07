#include "types/string.h"
#include "types/allocator.h"
#include "math/math.h"
#include <ctype.h>

c8 String_charToLower(c8 c) {
	return (c8) tolower(c);
}

c8 String_charToUpper(c8 c) {
	return (c8) toupper(c);
}

c8 String_charTransform(c8 c, enum StringTransform transform) {
	return transform == StringTransform_None ? c : (
		transform == StringTransform_Lower ? String_charToLower(c) :
		String_charToupper(c)
	);
}

//Simple checks (consts)

struct Error String_offsetAsRef(struct String s, usz off, struct String *result) {
	
	ocAssert("String out of bounds", off < s.len);

	*result = (struct String) {
		.ptr = s.ptr + off,
		.len = s.len - off
	};

	return Error_none();
}

bool String_isNytoDecimal(struct String s) {

	for(usz i = 0; i < s.len; ++i) {

		c8 c = s.ptr[i];

		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_' || c == '$')))
			return false;
	}

	return s.len;
}

bool String_isAlphaNumeric(struct String s) {

	for(usz i = 0; i < s.len; ++i) {

		c8 c = s.ptr[i];

		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_')))
			return false;
	}

	return s.len;
}

bool String_isHex(struct String s) {

	for(usz i = 0; i < s.len; ++i) {

		c8 c = s.ptr[i];

		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
			return false;
	}

	return s.len;
}

bool String_isUnsignedNumber(struct String s) {

	for(usz i = 0; i < s.len; ++i)
		if((s.ptr[i] < '0') || (s.ptr[i] > '9'))
			return false;

	return s.len;
}

bool String_isSignedNumber(struct String s) {
	return 
		s.len && (s.ptr[0] == '-' || s.ptr[0] == '+') ? 
		String_isUnsignedNumber(String_offsetRef(s, 1)) : 
		String_isUnsignedNumber(s);
}

bool String_isFloat(struct String s) {

	if(!s.len)
		return false;

	if(s.ptr[0] == '-' || s.ptr[0] == '+')
		s = String_offsetRef(s, 1);

	if(!s.len)
		return false;

	usz postDot = s.len;

	for(usz i = 0; i < s.len; ++i)

		if (s.ptr[i] == '.' || s.ptr[i] == 'e') {
			postDot = i + 1;
			break;
		}

		else if((s.ptr[i] < '0') || (s.ptr[i] > '9'))
			return false;

	//TODO:

	//198192e+4
	//93390e-4

	return true;
}

bool String_contains(struct String str, c8 c) {

	for(usz i = 0; i < str.len; ++i)
		if(str.ptr[i] == c)
			return true;

	return false;
}

bool String_containsIgnoreCase(struct String str, c8 c) {

	c = tolower(c);

	for(usz i = 0; i < str.len; ++i)
		if(tolower(str.ptr[i]) == c)
			return true;

	return false;
}

bool String_containsString(struct String str, struct String other);

bool String_containsStringIgnoreCase(struct String str, struct String other);

bool String_startsWithString(struct String str, struct String other);
bool String_startsWithIgnoreCase(struct String str, c8 c);
bool String_startsWithStringIgnoreCase(struct String str, struct String other);

bool String_endsWithString(struct String str, struct String other);
bool String_endsWithIgnoreCase(struct String str, c8 c);
bool String_endsWithStringIgnoreCase(struct String str, struct String other);

struct StringList String_findAll(struct String s, c8 c);
struct StringList String_findAllString(struct String s, struct String other);

usz String_findFirst(struct String s, c8 c);
usz String_findFirstString(struct String s, struct String other);

usz String_findLast(struct String s, c8 c);
usz String_findLastString(struct String s, struct String other);

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

//String's inline changes (no copy)

bool String_cutAfter(struct String *s, usz i) {

	if(i == usz_MAX)
		return false;

	s->ptr[i] = '\0';
	s->len = i;
	return true;
}

bool String_cutAfterLast(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findLast(*s, c));
}

bool String_cutAfterFirst(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findFirst(*s, c));
}

bool String_cutAfterLastString(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findLastString(*s, other));
}

bool String_cutAfterFirstString(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findFirstString(*s, other));
}

bool String_cutAfterLastIgnoreCase(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findLastIgnoreCase(*s, c));
}

bool String_cutAfterFirstIgnoreCase(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findFirstIgnoreCase(*s, c));
}

bool String_cutAfterLastStringIgnoreCase(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findLastStringIgnoreCase(*s, other));
}

bool String_cutAfterFirstStringIgnoreCase(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutAfter(s, String_findFirstStringIgnoreCase(*s, other));
}

bool String_cutBefore(struct String *s, usz i, usz off) {

	if(i == usz_MAX)
		return false;

	i += off;
	usz found = i;

	for(usz j = 0; i < s->len; ++i, ++j)
		s->ptr[j] = s->ptr[i];

	s->ptr[s->len - found] = '\0';
	s->len = s->len - found;
	return true;
}

bool String_cutBeforeLast(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findLast(*s, c), 1);
}

bool String_cutBeforeFirst(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findFirst(*s, c), 1);
}

bool String_cutBeforeLastString(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findLastString(*s, other), other.len);
}

bool String_cutBeforeFirstString(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findFirstString(*s, other), other.len);
}

bool String_cutBeforeLastIgnoreCase(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findLastIgnoreCase(*s, c), 1);
}

bool String_cutBeforeFirstIgnoreCase(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findFirstIgnoreCase(*s, c), 1);
}

bool String_cutBeforeLastStringIgnoreCase(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findLastStringIgnoreCase(*s, other), other.len);
}

bool String_cutBeforeFirstStringIgnoreCase(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_cutBefore(s, String_findFirstStringIgnoreCase(*s, other), other.len);
}

bool String_erase(struct String *s, usz i, usz off) {

	if(i == usz_MAX)
		return false;

	for(; i < s->len; ++i)
		s->ptr[i - off] = s->ptr[i];

	s->ptr[i - off] = '\0';
	s->len -= off;
	return true;
}

bool String_eraseFirst(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_erase(s, String_findFirst(*s, c), 1);
}

bool String_eraseLast(struct String *s, c8 c) {
	ocAssert("String function requires string", s);
	return String_erase(s, String_findLast(*s, c), 1);
}

bool String_eraseFirstString(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_erase(s, String_findFirstString(*s, other), other.len);
}

bool String_eraseLastString(struct String *s, struct String other) {
	ocAssert("String function requires string", s);
	return String_erase(s, String_findLastString(*s, other), other.len);
}

bool String_eraseAll(struct String *s, c8 c) {

	ocAssert("String function requires string", s);

	usz out = 0;

	for (usz i = 0; i < s->len; ++i) {

		if(s->ptr[i] == c)
			continue;

		s->ptr[out++] = s->ptr[i];
	}
	
	if(out == s->len)
		return false;

	s->ptr[out] = '\0';
	s->len = out;
	return true;
}

bool String_eraseAllString(struct String *s, struct String other) {

	ocAssert("String function requires string", s);

	if(!other.len)
		return false;

	usz out = 0;

	for (usz i = 0; i < s->len; ++i) {

		bool match = true;

		for (usz j = i, k = 0; j < s->len && k < other.len; ++j, ++k)
			if (s->ptr[j] != other.ptr[k]) {
				match = false;
				break;
			}

		if (match) {
			i += s->len - 1;
			continue;
		}

		s->ptr[out++] = s->ptr[i];
	}

	if(out == s->len)
		return false;

	s->ptr[out] = '\0';
	s->len = out;
	return true;
}

void String_replaceAll(struct String *s, c8 c, c8 v) {

	ocAssert("String function requires string", s);

	for(usz i = 0; i < s->len; ++i)
		if(s->ptr[i] == c)
			s->ptr[i] = v;
}

void String_replaceFirst(struct String *s, c8 c, c8 v) {

	ocAssert("String function requires string", s);

	for(usz i = 0; i < s->len; ++i)
		if(s->ptr[i] == c) {
			s->ptr[i] = v;
			break;
		}
}

bool String_replaceLast(struct String *s, c8 c, c8 v) {

	ocAssert("String function requires string", s);

	usz i = String_findLast(*s, c);

	if(i != usz_MAX)
		s->ptr[i] = v;
}

bool String_toLower(struct String *str) {

	if(!str || String_isRef(*str))
		return false;

	for(usz i = 0; i < str->len; ++i)
		str->ptr[i] = String_charToLower(str->ptr[i]);

	return true;
}

bool String_toUpper(struct String *str) {

	if(!str || String_isRef(*str))
		return false;

	for(usz i = 0; i < str->len; ++i)
		str->ptr[i] = String_charToUpper(str->ptr[i]);

	return true;
}

struct String String_trim(struct String s);

//StringList

struct Error StringList_create(usz len, struct Allocator alloc, struct StringList **arr) {

	struct StringList sl = (struct StringList) {
		.len = len
	};

	struct Error err = alloc.alloc(alloc.ptr, len * sizeof(struct String), &sl.ptr);

	if(err.genericError)
		return err;

	for(usz i = 0; i < sl.len; ++i)
		sl.ptr[i] = (struct String){ 0 };

	return Error_none();
}

struct Error StringList_free(struct StringList **arrPtr, struct Allocator alloc) {

	if(!arrPtr || !*arrPtr || !(*arrPtr)->len)
		return (struct Error){ .genericError = GenericError_NullPointer };

	struct StringList *arr = *arrPtr;

	struct Error freeErr = Error_none();

	for(usz i = 0; i < arr->len; ++i) {

		struct String *str = arr->ptr + i;
		struct Error err = String_free(&str, alloc);

		if(err.genericError)
			freeErr = err;
	}

	struct Error err = alloc.free(alloc.ptr, (struct Buffer) {
		.ptr = (u8*) arr->ptr,
		.siz = sizeof(struct String) * arr->len
	});

	if(err.genericError)
		freeErr = err;

	*arr = (struct StringList){ 0 };
	*arrPtr = NULL;
	return freeErr;
}

struct Error StringList_unset(struct StringList arr, usz i, struct Allocator alloc) {

	if(i >= arr.len)
		return (struct Error) { 
			.genericError = GenericError_OutOfBounds, 
			.paramValue0 = i, 
			.paramValue1 = arr.len 
		};

	struct String *pstr = arr.ptr + i;
	return String_free(&pstr, alloc);
}

struct Error StringList_set(struct StringList arr, usz i, struct String str, struct Allocator alloc) {

	struct Error err = StringList_unset(arr, i, alloc);

	if(err.genericError)
		return err;

	arr.ptr[i] = str;
	return Error_none();
}