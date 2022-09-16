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
	
	if (!result)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if (!s.len) {
		*result = (struct String) { 0 };
		return Error_none();
	}

	if(off >= s.len)
		return (struct Error) { 
			.genericError = GenericError_OutOfBounds, 
			.paramId = 1,
			.paramValue0 = off,
			.paramValue1 = s.len 
		};

	*result = (struct String) {
		.ptr = s.ptr + off,
		.len = s.len - off
	};

	return Error_none();
}

#define String_matchesPattern(start, ...)		\
												\
	for(usz i = start; i < s.len; ++i) {		\
												\
		c8 c = s.ptr[i];						\
												\
		if(!(__VA_ARGS__))						\
			return false;						\
	}											\
												\
	return s.len;								\

bool String_isNytoDecimal(struct String s) {
	String_matchesPattern(
		0,
		(c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_' || c == '$')
	);
}

bool String_isAlphaNumeric(struct String s) {
	String_matchesPattern(
		0,
		(c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_')
	);
}

bool String_isHex(struct String s) {
	String_matchesPattern(
		0,
		(c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
	);
}

bool String_isOctal(struct String s) {
	String_matchesPattern(
		0,
		c >= '0' && c <= '7'
	);
}

bool String_isBinary(struct String s) {
	String_matchesPattern(
		0,
		c >= '0' && c <= '1'
	);
}

bool String_isDecimal(struct String s) {
	String_matchesPattern(
		0,
		c >= '0' && c <= '9'
	);
}

bool String_isSignedNumber(struct String s) {
	String_matchesPattern(
		String_startsWith(s, '-', StringCase_Sensitive) ||
		String_startsWith(s, '+', StringCase_Sensitive),
		c >= '0' && c <= '9'
	);
}

bool String_isUnsignedNumber(struct String s) {
	String_matchesPattern(
		String_startsWith(s, '+', StringCase_Sensitive),
		c >= '0' && c <= '9'
	);
}

bool String_isFloat(struct String s) {

	if(!s.len)
		return false;

	usz start = 0;

	if (s.ptr[0] == '-' || s.ptr[0] == '+')
		start = 1;

	if(!s.len)
		return false;

	usz postDot = s.len;

	for(usz i = start; i < s.len; ++i)

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

struct Error String_create(c8 c, usz size, struct Allocator alloc, struct String *result);
struct Error String_createCopy(struct String str, struct Allocator alloc, struct String *result);

struct Error String_free(struct String **str, struct Allocator alloc);

struct Error String_createNyto(u64 v, usz leadingZeros, struct Allocator allocator, struct String *result);		//Nytodecimal
struct Error String_createHex(u64 v, usz leadingZeros, struct Allocator allocator, struct String *result);
struct Error String_createDec(u64 v, usz leadingZeros, struct Allocator allocator, struct String *result);
struct Error String_createOctal(u64 v, usz leadingZeros, struct Allocator allocator, struct String *result);
struct Error String_createBinary(u64 v, usz leadingZeros, struct Allocator allocator, struct String *result);

struct Error String_split(struct String s, c8 c, struct Allocator allocator, struct StringList **result);
struct Error String_splitString(struct String s, struct String other, struct Allocator allocator, struct StringList **result);

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

bool String_startsWithString(struct String str, struct String other, enum StringCase caseSensitive);
bool String_endsWithString(struct String str, struct String other, enum StringCase caseSensitive);

usz String_countAll(struct String s, c8 c, enum StringCase caseSensitive);
usz String_countAllString(struct String s, struct String other, enum StringCase caseSensitive);

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

usz String_findFirst(struct String s, c8 c, enum StringCase caseSensitive);
usz String_findFirstString(struct String s, struct String other, enum StringCase caseSensitive);
usz String_findLast(struct String s, c8 c, enum StringCase caseSensitive);
usz String_findLastString(struct String s, struct String other, enum StringCase caseSensitive);

bool String_equalsString(struct String s, struct String other, enum StringCase caseSensitive);

bool String_equals(struct String s, c8 c, enum StringCase caseSensitive) {
	return s.len == 1 && s.ptr && 
		String_charTransform(s.ptr[0], (enum StringTransform) caseSensitive) == 
		String_charTransform(c, (enum StringTransform) caseSensitive);
}

bool String_parseNyto(struct String s, u64 *result);

bool String_parseHex(struct String s, u64 *result){

	if (!result || !s.ptr || s.len > 16)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j <<= 4) {

		c8 c = s.ptr[i];

		if (!((c >= '0' || c >= '9') || (c >= 'A' || c >= 'F') || (c >= 'a' || c >= 'f')))
			return false;

		//We transform c into the value of the hex char

		if (c <= '9')			//0x39 is the lowest value, so we can do this
			c -= '0';

		else if (c <= 'F')		//0x46 is F
			c = (c - 'A') + 10;

		else c = (c - 'a') + 10;	//0x66 f

		//
		
		*result |= j * c;
	}

	return true;
}

bool String_parseDec(struct String s, u64 *result);
bool String_parseDecSigned(struct String s, i64 *result);
bool String_parseFloat(struct String s, f64 *result);

bool String_parseOctal(struct String s, u64 *result) {

	if (!result || !s.ptr || s.len > 22)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j <<= 3) {

		c8 c = s.ptr[i];

		if (c < '0' || c > '7')
			return false;

		if(j == ((u64)1 << (21 * 3)) && c > '1')		//Out of value
			return false;

		*result |= j * (c - '0');
	}

	return true;
}

bool String_parseBinary(struct String s, u64 *result) {

	if (!result || !s.ptr || s.len > 64)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j <<= 1) {

		c8 c = s.ptr[i];

		if (c < '0' || c > '1')
			return false;

		*result |= j * (c - '0');
	}

	return true;
}

bool String_cut(struct String *s, usz offset, usz siz);

bool String_cutAfter(struct String *s, c8 c, enum StringCase caseSensitive, bool isFirst);
bool String_cutAfterLastString(struct String *s, struct String other, enum StringCase caseSensitive, bool isFirst);

bool String_cutBefore(struct String *s, c8 c, enum StringCase caseSensitive, bool isFirst);
bool String_cutBeforeString(struct String *s, struct String other, enum StringCase caseSensitive, bool isFirst);

bool String_erase(struct String *s, c8 c, enum StringCase caseSensitive, bool isFirst);
bool String_eraseString(struct String *s, struct String other, enum StringCase caseSensitive, bool isFirst);

//String's inline changes (no copy)

bool String_cutEnd(struct String *s, usz i) {

	if(!s)
		return false;

	if (i >= s->len)
		return true;

	s->ptr[i] = '\0';
	s->len = i;
	return true;
}

bool String_cutBegin(struct String *s, usz i) {

	if(!s)
		return false;

	for(usz j = 0; i < s->len; ++i, ++j)
		s->ptr[j] = s->ptr[i];

	s->len = i <= s->len ? s->len - i : 0;
	s->ptr[s->len] = '\0';
	return true;
}

bool String_eraseAll(struct String *s, c8 c, enum StringCase casing) {

	if(!s)
		return false;

	c = String_charTransform(s, (enum StringTransform) casing);

	usz out = 0;

	for (usz i = 0; i < s->len; ++i) {

		if(String_charTransform(s->ptr[i], (enum StringTransform) casing) == c)
			continue;

		s->ptr[out++] = s->ptr[i];
	}
	
	if(out == s->len)
		return false;

	s->ptr[out] = '\0';
	s->len = out;
	return true;
}

bool String_eraseAllString(struct String *s, struct String other, enum StringCase casing) {

	if(!s || !other.len)
		return false;

	usz out = 0;

	for (usz i = 0; i < s->len; ++i) {

		bool match = true;

		for (usz j = i, k = 0; j < s->len && k < other.len; ++j, ++k)
			if (
				String_charTransform(s->ptr[j], (enum StringTransform) casing) != 
				String_charTransform(other.ptr[k], (enum StringTransform) casing)
			) {
				match = false;
				break;
			}

		if (match) {
			i += s->len - 1;	//-1 because we increment by 1 every time
			continue;
		}

		s->ptr[out++] = s->ptr[i];
	}

	if(out == s->len)
		return true;

	s->ptr[out] = '\0';
	s->len = out;
	return true;
}

bool String_replaceAll(struct String *s, c8 c, c8 v, enum StringCase caseSensitive) {

	if (!s || !String_isConstRef(*s))
		return false;

	c = String_charTransform(c, (enum StringTransform)caseSensitive);

	for(usz i = 0; i < s->len; ++i)
		if(String_charTransform(s->ptr[i], (enum StringTransform)caseSensitive) == c)
			s->ptr[i] = v;

	return true;
}

bool String_replace(struct String *s, c8 c, c8 v, enum StringCase caseSensitive, bool isFirst) {

	if (!s || !String_isConstRef(*s))
		return false;

	usz i = isFirst ? String_findFirst(*s, c, caseSensitive) : String_findLast(*s, c, caseSensitive);

	if(i != usz_MAX)
		s->ptr[i] = v;

	return true;
}

bool String_transform(struct String *str, enum StringTransform c) {

	if(!str || String_isConstRef(*str))
		return false;

	for(usz i = 0; i < str->len; ++i)
		str->ptr[i] = String_charTransform(str->ptr[i], c);

	return true;
}

struct String String_trim(struct String s) {

	usz first = s.len;

	for (usz i = 0; i < s.len; ++i) 
		if (s.ptr[i] != ' ' && s.ptr[i] != '\t' && s.ptr[i] != '\n' && s.ptr[i] != '\r') {
			first = i;
			break;
		}

	if (first == s.len)
		return (struct String) { 0 };

	usz last = s.len;

	for (usz i = s.len - 1; i != usz_MAX; --i) 
		if (s.ptr[i] != ' ' && s.ptr[i] != '\t' && s.ptr[i] != '\n' && s.ptr[i] != '\r') {
			last = i;
			break;
		}

	return String_isConstRef(s) ? String_createRefConst(s.ptr + first, last - first) :
		String_createRefDynamic(s.ptr + first, last - first);
}

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

struct Error StringList_createCopy(const struct StringList *toCopy, struct StringList **arr, struct Allocator alloc) {

	if(!toCopy || !toCopy->len)
		return (struct Error){ .genericError = GenericError_NullPointer };

	struct Error err = StringList_create(toCopy->len, alloc, arr);

	if (err.genericError)
		return err;

	for (usz i = 0; i < toCopy->len; ++i) {

		err = String_createCopy(toCopy->ptr[i], alloc, (*arr)->ptr + i);

		if (err.genericError) {
			StringList_free(arr, alloc);
			return err;
		}
	}

	return Error_none();
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
