#include "types/string.h"
#include "types/allocator.h"
#include "math/math.h"
#include <ctype.h>

//Simple checks (consts)

struct Error String_offsetAsRef(struct String s, u64 off, struct String *result) {
	
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

#define String_matchesPattern(testFunc, ...)		\
													\
	for(u64 i = (__VA_ARGS__); i < s.len; ++i) {	\
													\
		c8 c = s.ptr[i];							\
													\
		if(!testFunc(c))							\
			return false;							\
	}												\
													\
	return s.len;									\

#define String_matchesPatternNum(testFunc, num) String_matchesPattern(		\
	testFunc,																\
	String_startsWithString(												\
		s,																	\
		String_createRefUnsafeConst(num),									\
		StringCase_Insensitive												\
	) ? String_createRefUnsafeConst(num).len : 0							\
)

bool String_isNyto(struct String s) {
	String_matchesPatternNum(c8_isNyto, "0n");
}

bool String_isAlphaNumeric(struct String s) {
	String_matchesPattern(c8_isAlphaNumeric, 0);
}

bool String_isHex(struct String s) {
	String_matchesPatternNum(c8_isHex, "0x");
}

bool String_isOct(struct String s) {
	String_matchesPatternNum(c8_isOct, "0o");
}

bool String_isBin(struct String s) {
	String_matchesPatternNum(c8_isBin, "0b");
}

bool String_isDecimal(struct String s) {
	String_matchesPattern(c8_isDec, 0);
}

bool String_isSignedNumber(struct String s) {
	String_matchesPattern(
		c8_isDec,
		String_startsWith(s, '-', StringCase_Insensitive) ||
		String_startsWith(s, '+', StringCase_Insensitive)
	);
}

bool String_isUnsignedNumber(struct String s) {
	String_matchesPattern(
		c8_isDec,
		String_startsWith(s, '+', StringCase_Insensitive)
	);
}

bool String_isFloat(struct String s) {

	if(!s.len)
		return false;

	u64 start = 0;

	if (s.ptr[0] == '-' || s.ptr[0] == '+')
		start = 1;

	if(!s.len)
		return false;

	u64 postDot = s.len;

	for(u64 i = start; i < s.len; ++i)

		if (s.ptr[i] == '.' || s.ptr[i] == 'e') {
			postDot = i + 1;
			break;
		}

		else if(!c8_isDec(s.ptr[i]))
			return false;

	//TODO:

	//198192e+4
	//93390e-4

	return true;
}

struct Error String_create(c8 c, u64 size, struct Allocator alloc, struct String *result) {

	if (!c)
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	if (!alloc.alloc)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if (!result)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 3 };

	if (!size) {
		*result =  (struct String) { 0 };
		return Error_none();
	}

	struct Buffer b = (struct Buffer) { 0 };
	struct Error err = alloc.alloc(alloc.ptr, size + 1, &b);

	if (err.genericError)
		return err;

	u64 totalSize = size;

	//Quick fill

	if (size >= 8) {

		u64 cc8 =
			((u64)c << 56) | ((u64)c << 48) | ((u64)c << 40) | ((u64)c << 32) |
			((u64)c << 24) | ((u64)c << 16) | ((u64)c <<  8) | ((u64)c <<  0);

		for (u64 i = 0; i < size >> 3; ++i)
			*((u64*)b.ptr + i) = cc8;

		size &= 7;
	}

	if (size >= 4) {
		u32 cc4 = ((u32)c << 24) | ((u32)c << 16) | ((u32)c << 8) | ((u32)c << 0);
		*((u32*)b.ptr + ((totalSize & ~7) >> 2)) = cc4;
		size &= 3;
	}

	if (size >= 2) {
		u16 cc2 = ((u16)c << 8) | ((u16)c << 0);
		*((u16*)b.ptr + ((totalSize & ~3) >> 1)) = cc2;
		size &= 3;
	}

	if(size >= 1)
		b.ptr[totalSize - 1] = c;

	b.ptr[totalSize] = '\0';

	*result = (struct String) { .len = totalSize, .capacity = totalSize + 1, .ptr = b.ptr };
	return Error_none();
}

struct Error String_createCopy(struct String str, struct Allocator alloc, struct String *result);

struct Error String_free(struct String *str, struct Allocator alloc) {

	if (!str)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (!alloc.free)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 1 };

	struct Error err = alloc.free(alloc.ptr, (struct Buffer) { .ptr = str->ptr, .siz = str->capacity });
	*str = (struct String) { 0 };
	return err;
}

#define String_createNum(maxVal, func, prefixRaw, ...)					\
																		\
	struct String prefix = String_createRefUnsafeConst(prefixRaw);		\
																		\
	struct Error err = String_create('0', 0, allocator, result);		\
																		\
	if (err.genericError)												\
		return err;														\
																		\
	err = String_reserve(result, maxVal + prefix.len, allocator);		\
																		\
	if (err.genericError)												\
		return err;														\
																		\
	err = String_appendString(&result, prefix, allocator);				\
																		\
	if (err.genericError)												\
		return err;														\
																		\
	bool foundFirstNonZero = leadingZeros;								\
																		\
	for (u64 i = maxVal - 1; i != u64_MAX; --i) {						\
																		\
		c8 c = c8_create##func(__VA_ARGS__);							\
																		\
		if (!foundFirstNonZero)											\
			foundFirstNonZero = c != '0';								\
																		\
		if (foundFirstNonZero) {										\
																		\
			err = String_append(result, c, allocator);					\
																		\
			if (err.genericError) {										\
				String_free(result, allocator);							\
				return err;												\
			}															\
		}																\
	}																	\
																		\
	/* Ensure we don't return an empty string on 0 */					\
																		\
	if (!result->len) {													\
																		\
		err = String_append(result, '0', allocator);					\
																		\
		if (err.genericError) {											\
			String_free(result, allocator);								\
			return err;													\
		}																\
	}																	\
																		\
	return Error_none();

struct Error String_createNyto(u64 v, bool leadingZeros, struct Allocator allocator, struct String *result){
	String_createNum(11, Nyto, "0n", (v >> (6 * i)) & 63);
}

struct Error String_createHex(u64 v, bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(16, Hex, "0x", (v >> (4 * i)) & 15);
}

struct Error String_createDec(u64 v, bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(20, Dec, "", (v / u64_pow10(i)) % 10);
}

struct Error String_createOct(u64 v, bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(22, Oct, "0o", (v >> (3 * i)) & 7);
}

struct Error String_createBin(u64 v, bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(64, Bin, "0b", (v >> i) & 1);
}

struct Error String_split(struct String s, c8 c, struct Allocator allocator, struct StringList **result);
struct Error String_splitString(struct String s, struct String other, struct Allocator allocator, struct StringList **result);

struct Error String_reserve(struct String *str, u64 siz, struct Allocator alloc) {

	++siz;

	if (!str)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (String_isRef(*str))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if (!alloc.alloc || !alloc.free)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if (siz <= str->capacity)
		return Error_none();

	struct Buffer b = (struct Buffer) { 0 };
	struct Error err = alloc.alloc(alloc.ptr, siz, &b);

	if (err.genericError)
		return err;

	for (u64 i = 0; i < str->len; ++i)		//Preserve old data
		b.ptr[i] = str->ptr[i];

	err = alloc.free(alloc.ptr, (struct Buffer){ .ptr = str->ptr, .siz = str->capacity });

	str->capacity = b.siz;
	str->len = siz;
	str->ptr = b.ptr;
	return err;
}

struct Error String_resize(struct String *str, u64 siz, struct Allocator alloc, c8 defaultChar) {

	if (!str)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (String_isRef(*str))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if (!alloc.alloc || !alloc.free)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if (siz == str->len)
		return Error_none();

	//Simple resize; we cut off the tail

	if (siz < str->len) {
		str->len = siz;
		str->ptr[str->len] = '\0';
		return Error_none();
	}

	//Resize that triggers buffer resize

	if (siz >= str->capacity) {

		//Reserve 50% more to ensure we don't resize too many times

		struct Error err = String_reserve(str, siz * 3 / 2, alloc);

		if (err.genericError)
			return err;
	}

	//Our capacity is big enough to handle it:

	for (u64 i = str->len; i < siz; ++i)
		str->ptr[i] = defaultChar;

	str->len = siz;
	str->ptr[str->len] = '\0';

	return Error_none();
}

struct Error String_append(struct String *s, c8 c, struct Allocator allocator) {

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	return String_resize(s, s->len + 1, allocator, c);
}

struct Error String_appendString(struct String *s, struct String other, struct Allocator allocator) {

	if (!other.len || !other.ptr)
		return Error_none();

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	struct Error err = String_resize(s, s->len + other.len, allocator, other.ptr[0]);

	if (err.genericError)
		return err;

	for (u64 i = 1; i < other.len; ++i)		//We can skip 0 since all N new chars are set to ptr[0]
		s->ptr[i] = other.ptr[i];

	return Error_none();
}

struct Error String_insert(struct String *s, c8 c, u64 i, struct Allocator allocator) {

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(i > s->len)
		return (struct Error) { .genericError = GenericError_OutOfBounds, .paramValue0 = i, .paramValue1 = s->len };

	struct Error err = String_resize(s, s->len + 1, allocator, c);

	if (err.genericError)
		return err;

	//If it's not append (otherwise it's already handled)

	if (i != s->len) {

		//Move one to the right

		for (u64 j = s->len; j != i; --j)
			s->ptr[j] = s->ptr[j - 1];

		s->ptr[i] = c;
	}

	return Error_none();
}

struct Error String_insertString(struct String *s, struct String other, u64 i, struct Allocator allocator);

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

u64 String_countAll(struct String s, c8 c, enum StringCase caseSensitive) {

	c = c8_transform(c, (enum StringTransform)caseSensitive);

	u64 count = 0;

	for (u64 i = 0; i < s.len; ++i)
		if (c8_transform(s.ptr[i], (enum StringTransform)caseSensitive) == c)
			++count;

	return count;
}

u64 String_countAllString(struct String s, struct String other, enum StringCase caseSensitive);

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

u64 String_findFirst(struct String s, c8 c, enum StringCase caseSensitive) {

	c = c8_transform(c, (enum StringTransform)caseSensitive);

	for (u64 i = 0; i < s.len; ++i)
		if (c8_transform(s.ptr[i], (enum StringTransform)caseSensitive) == c)
			return i;

	return s.len;
}

u64 String_findLast(struct String s, c8 c, enum StringCase caseSensitive) {

	c = c8_transform(c, (enum StringTransform)caseSensitive);

	for (u64 i = s.len - 1; i != u64_MAX; --i)
		if (c8_transform(s.ptr[i], (enum StringTransform)caseSensitive) == c)
			return i;

	return s.len;
}

u64 String_findFirstString(struct String s, struct String other, enum StringCase caseSensitive);
u64 String_findLastString(struct String s, struct String other, enum StringCase caseSensitive);

bool String_equalsString(struct String s, struct String other, enum StringCase caseSensitive);

bool String_equals(struct String s, c8 c, enum StringCase caseSensitive) {
	return s.len == 1 && s.ptr && 
		c8_transform(s.ptr[0], (enum StringTransform) caseSensitive) == 
		c8_transform(c, (enum StringTransform) caseSensitive);
}

bool String_parseNyto(struct String s, u64 *result){

	struct String prefix = String_createRefUnsafeConst("0n");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 11)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j <<= 6) {

		u8 v = c8_nyto(s.ptr[i]);

		if (v == u8_MAX)
			return false;

		if(j == ((u64)1 << (10 * 6)) && v >= (1 << 4))		//We have 4 bits left
			return false;

		*result |= j * v;
	}

	return true;
}

bool String_parseHex(struct String s, u64 *result){

	struct String prefix = String_createRefUnsafeConst("0x");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 16)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j <<= 4) {

		u8 v = c8_hex(s.ptr[i]);

		if (v == u8_MAX)
			return false;

		*result |= j * v;
	}

	return true;
}

bool String_parseDec(struct String s, u64 *result) {

	if (!result || !s.ptr || s.len > 20)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j *= 10) {

		u8 v = c8_dec(s.ptr[i]);

		if (v == u8_MAX)
			return false;

		if(j == (u64) 1e19 && v > 1)		//Out of value
			return false;

		*result |= j * v;
	}

	return true;
}

bool String_parseDecSigned(struct String s, i64 *result) {

	bool neg = String_startsWith(s, '-', StringCase_Insensitive);

	struct Error err = String_offsetAsRef(s, neg, &s);

	if (err.genericError)
		return false;

	bool b = String_parseDec(s, (u64*) result);

	if (b) {

		if (*(u64*)result > i64_MAX)
			return false;

		if(neg)
			*result = -*result;

		return true;
	}

	return false;
}

bool String_parseFloat(struct String s, f64 *result);

bool String_parseOctal(struct String s, u64 *result) {

	struct String prefix = String_createRefUnsafeConst("0o");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 22)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j <<= 3) {

		u8 v = c8_oct(s.ptr[i]);

		if (v == u8_MAX)
			return false;

		if(j == ((u64)1 << (21 * 3)) && v > 1)		//Out of value
			return false;

		*result |= j * v;
	}

	return true;
}

bool String_parseBinary(struct String s, u64 *result) {

	struct String prefix = String_createRefUnsafeConst("0b");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 64)
		return false;

	*result = 0;

	for (u64 i = s.len - 1, j = 1; i != u64_MAX; --i, j <<= 1) {

		u8 v = c8_bin(s.ptr[i]);

		if (v == u8_MAX)
			return false;

		*result |= j * v;
	}

	return true;
}

bool String_cut(struct String *s, u64 offset, u64 siz);

bool String_cutAfter(struct String *s, c8 c, enum StringCase caseSensitive, bool isFirst);
bool String_cutAfterLastString(struct String *s, struct String other, enum StringCase caseSensitive, bool isFirst);

bool String_cutBefore(struct String *s, c8 c, enum StringCase caseSensitive, bool isFirst);
bool String_cutBeforeString(struct String *s, struct String other, enum StringCase caseSensitive, bool isFirst);

bool String_erase(struct String *s, c8 c, enum StringCase caseSensitive, bool isFirst);
bool String_eraseString(struct String *s, struct String other, enum StringCase caseSensitive, bool isFirst);

//String's inline changes (no copy)

bool String_cutEnd(struct String *s, u64 i) {

	if(!s)
		return false;

	if (i >= s->len)
		return true;

	s->ptr[i] = '\0';
	s->len = i;
	return true;
}

bool String_cutBegin(struct String *s, u64 i) {

	if(!s)
		return false;

	for(u64 j = 0; i < s->len; ++i, ++j)
		s->ptr[j] = s->ptr[i];

	s->len = i <= s->len ? s->len - i : 0;
	s->ptr[s->len] = '\0';
	return true;
}

bool String_eraseAll(struct String *s, c8 c, enum StringCase casing) {

	if(!s)
		return false;

	c = c8_transform(s, (enum StringTransform) casing);

	u64 out = 0;

	for (u64 i = 0; i < s->len; ++i) {

		if(c8_transform(s->ptr[i], (enum StringTransform) casing) == c)
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

	u64 out = 0;

	for (u64 i = 0; i < s->len; ++i) {

		bool match = true;

		for (u64 j = i, k = 0; j < s->len && k < other.len; ++j, ++k)
			if (
				c8_transform(s->ptr[j], (enum StringTransform) casing) != 
				c8_transform(other.ptr[k], (enum StringTransform) casing)
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

	c = c8_transform(c, (enum StringTransform)caseSensitive);

	for(u64 i = 0; i < s->len; ++i)
		if(c8_transform(s->ptr[i], (enum StringTransform)caseSensitive) == c)
			s->ptr[i] = v;

	return true;
}

bool String_replace(struct String *s, c8 c, c8 v, enum StringCase caseSensitive, bool isFirst) {

	if (!s || !String_isConstRef(*s))
		return false;

	u64 i = isFirst ? String_findFirst(*s, c, caseSensitive) : String_findLast(*s, c, caseSensitive);

	if(i != u64_MAX)
		s->ptr[i] = v;

	return true;
}

bool String_transform(struct String *str, enum StringTransform c) {

	if(!str || String_isConstRef(*str))
		return false;

	for(u64 i = 0; i < str->len; ++i)
		str->ptr[i] = c8_transform(str->ptr[i], c);

	return true;
}

struct String String_trim(struct String s) {

	u64 first = s.len;

	for (u64 i = 0; i < s.len; ++i) 
		if (s.ptr[i] != ' ' && s.ptr[i] != '\t' && s.ptr[i] != '\n' && s.ptr[i] != '\r') {
			first = i;
			break;
		}

	if (first == s.len)
		return (struct String) { 0 };

	u64 last = s.len;

	for (u64 i = s.len - 1; i != u64_MAX; --i) 
		if (s.ptr[i] != ' ' && s.ptr[i] != '\t' && s.ptr[i] != '\n' && s.ptr[i] != '\r') {
			last = i;
			break;
		}

	return String_isConstRef(s) ? String_createRefConst(s.ptr + first, last - first) :
		String_createRefDynamic(s.ptr + first, last - first);
}

//StringList

struct Error StringList_create(u64 len, struct Allocator alloc, struct StringList **arr) {

	struct StringList sl = (struct StringList) {
		.len = len
	};

	struct Error err = alloc.alloc(alloc.ptr, len * sizeof(struct String), &sl.ptr);

	if(err.genericError)
		return err;

	for(u64 i = 0; i < sl.len; ++i)
		sl.ptr[i] = (struct String){ 0 };

	return Error_none();
}

struct Error StringList_free(struct StringList **arrPtr, struct Allocator alloc) {

	if(!arrPtr || !*arrPtr || !(*arrPtr)->len)
		return (struct Error){ .genericError = GenericError_NullPointer };

	struct StringList *arr = *arrPtr;

	struct Error freeErr = Error_none();

	for(u64 i = 0; i < arr->len; ++i) {

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

	for (u64 i = 0; i < toCopy->len; ++i) {

		err = String_createCopy(toCopy->ptr[i], alloc, (*arr)->ptr + i);

		if (err.genericError) {
			StringList_free(arr, alloc);
			return err;
		}
	}

	return Error_none();
}

struct Error StringList_unset(struct StringList arr, u64 i, struct Allocator alloc) {

	if(i >= arr.len)
		return (struct Error) { 
			.genericError = GenericError_OutOfBounds, 
			.paramValue0 = i, 
			.paramValue1 = arr.len 
		};

	struct String *pstr = arr.ptr + i;
	return String_free(&pstr, alloc);
}

struct Error StringList_set(struct StringList arr, u64 i, struct String str, struct Allocator alloc) {

	struct Error err = StringList_unset(arr, i, alloc);

	if(err.genericError)
		return err;

	arr.ptr[i] = str;
	return Error_none();
}
