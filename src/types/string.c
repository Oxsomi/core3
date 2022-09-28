#include "types/string.h"
#include "types/allocator.h"
#include "math/math.h"
#include "types/bit.h"
#include <ctype.h>

//Simple checks (consts)

struct Error String_offsetAsRef(struct String s, U64 off, struct String *result) {
	
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
		.len = s.len - off,
		.capacity = String_isConstRef(s) ? U64_MAX : 0
	};

	return Error_none();
}

#define String_matchesPattern(testFunc, ...)		\
													\
	for(U64 i = (__VA_ARGS__); i < s.len; ++i) {	\
													\
		C8 c = s.ptr[i];							\
													\
		if(!testFunc(c))							\
			return false;							\
	}												\
													\
	return s.len;

#define String_matchesPatternNum(testFunc, num) String_matchesPattern(		\
	testFunc,																\
	String_startsWithString(												\
		s,																	\
		String_createRefUnsafeConst(num),								\
		StringCase_Insensitive									\
	) ? String_createRefUnsafeConst(num).len : 0							\
)

Bool String_isNyto(struct String s) {
	String_matchesPatternNum(C8_isNyto, "0n");
}

Bool String_isAlphaNumeric(struct String s) {
	String_matchesPattern(C8_isAlphaNumeric, 0);
}

Bool String_isHex(struct String s) {
	String_matchesPatternNum(C8_isHex, "0x");
}

Bool String_isOct(struct String s) {
	String_matchesPatternNum(C8_isOct, "0o");
}

Bool String_isBin(struct String s) {
	String_matchesPatternNum(C8_isBin, "0b");
}

Bool String_isDec(struct String s) {
	String_matchesPattern(C8_isDec, 0);
}

Bool String_isSignedNumber(struct String s) {
	String_matchesPattern(
		C8_isDec,
		String_startsWith(s, '-', StringCase_Sensitive) ||
		String_startsWith(s, '+', StringCase_Sensitive)
	);
}

Bool String_isUnsignedNumber(struct String s) {
	String_matchesPattern(
		C8_isDec,
		String_startsWith(s, '+', StringCase_Sensitive)
	);
}

Bool String_isFloat(struct String s) {

	if(!s.len || !s.ptr)
		return false;

	//Validate sign

	U64 i = 0;

	if (s.ptr[0] == '-' || s.ptr[0] == '+') {

		if (++i >= s.len)
			return false;
	}

	//Validate first int 

	for(; i < s.len; ++i)

		if (s.ptr[i] == '.' || s.ptr[i] == 'e' || s.ptr[i] == 'E')
			break;

		else if(!C8_isDec(s.ptr[i]))
			return false;

	if (i == s.len)		//It's just an int
		return true;

	//Validate fraction

	if (s.ptr[i] == '.') {

		if (++i == s.len)		//It's just an int
			return true;

		//Check for int until e/E

		for(; i < s.len; ++i)

			if (s.ptr[i] == 'e' || s.ptr[i] == 'E')
				break;

			else if(!C8_isDec(s.ptr[i]))
				return false;
	}

	if (i == s.len)		//It's just [-+]?[0-9]*[.]?[0-9]*
		return true;

	//Validate exponent

	if (s.ptr[i] == 'E' || s.ptr[i] == 'e') {

		if (++i == s.len)
			return false;

		//e-NNN, e+NNN

		if (s.ptr[i] == '-' || s.ptr[i] == '+') {

			if (++i == s.len)
				return false;
		}

		for(; i < s.len; ++i)
			if(!C8_isDec(s.ptr[i]))
				return false;
	}

	return true;
}

struct Error String_create(C8 c, U64 size, struct Allocator alloc, struct String *result) {

	if (!c)
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	if (!alloc.alloc)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if (!result)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 3 };

	if (!size) {
		*result = String_createEmpty();
		return Error_none();
	}

	struct Buffer b = (struct Buffer) { 0 };
	struct Error err = alloc.alloc(alloc.ptr, size, &b);

	if (err.genericError)
		return err;

	U64 totalSize = size;

	//Quick fill

	if (size >= 8) {

		U64 c8 =
			((U64)c << 56) | ((U64)c << 48) | ((U64)c << 40) | ((U64)c << 32) |
			((U64)c << 24) | ((U64)c << 16) | ((U64)c <<  8) | ((U64)c <<  0);

		for (U64 i = 0; i < size >> 3; ++i)
			*((U64*)b.ptr + i) = c8;

		size &= 7;
	}

	if (size >= 4) {
		U32 cc4 = ((U32)c << 24) | ((U32)c << 16) | ((U32)c << 8) | ((U32)c << 0);
		*((U32*)b.ptr + ((totalSize & ~7) >> 2)) = cc4;
		size &= 3;
	}

	if (size >= 2) {
		U16 cc2 = ((U16)c << 8) | ((U16)c << 0);
		*((U16*)b.ptr + ((totalSize & ~3) >> 1)) = cc2;
		size &= 3;
	}

	if(size >= 1)
		b.ptr[totalSize - 1] = c;

	*result = (struct String) { .len = totalSize, .capacity = totalSize, .ptr = b.ptr };
	return Error_none();
}

struct Error String_createCopy(struct String str, struct Allocator alloc, struct String *result) {

	if (!alloc.alloc)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 1 };

	if (!result)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 2 };

	if (!str.len) {
		*result = String_createEmpty();
		return Error_none();
	}

	struct Buffer b = (struct Buffer) { 0 };
	struct Error err = alloc.alloc(alloc.ptr, str.len, &b);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < str.len >> 3; ++i)
		*((U64*)b.ptr + i) = *((const U64*)str.ptr + i);

	for (U64 i = str.len >> 3 << 3; i < str.len; ++i)
		b.ptr[i] = str.ptr[i];

	*result = (struct String) { .ptr = b.ptr, .len = str.len, .capacity = str.len };
	return Error_none();
}

struct Error String_free(struct String *str, struct Allocator alloc) {

	if (!str)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (!alloc.free)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 1 };

	struct Error err = alloc.free(alloc.ptr, (struct Buffer) { .ptr = str->ptr, .siz = str->capacity });
	*str = String_createEmpty();
	return err;
}

#define String_createNum(maxVal, func, prefixRaw, ...)					\
																		\
	struct String prefix = String_createRefUnsafeConst(prefixRaw);		\
																		\
	*result = String_createEmpty();										\
																		\
	struct Error err = String_reserve(									\
		result, maxVal + prefix.len, allocator							\
	);																	\
																		\
	if (err.genericError)												\
		return err;														\
																		\
	err = String_appendString(&result, prefix, allocator);				\
																		\
	if (err.genericError)												\
		return err;														\
																		\
	Bool foundFirstNonZero = leadingZeros;								\
																		\
	for (U64 i = maxVal - 1; i != U64_MAX; --i) {						\
																		\
		C8 c = C8_create##func(__VA_ARGS__);							\
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

struct Error String_createNyto(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result){
	String_createNum(11, Nyto, "0n", (v >> (6 * i)) & 63);
}

struct Error String_createHex(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(16, Hex, "0x", (v >> (4 * i)) & 15);
}

struct Error String_createDec(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(20, Dec, "", (v / U64_pow10(i)) % 10);
}

struct Error String_createOct(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(22, Oct, "0o", (v >> (3 * i)) & 7);
}

struct Error String_createBin(U64 v, Bool leadingZeros, struct Allocator allocator, struct String *result) {
	String_createNum(64, Bin, "0b", (v >> i) & 1);
}

struct Error String_split(
	struct String s, 
	C8 c, 
	enum StringCase casing, 
	struct Allocator allocator, 
	struct StringList **result
) {

	U64 len = String_countAll(s, c, casing);

	struct Error err = StringList_create(len, allocator, result);

	if (err.genericError)
		return err;

	if (result) {

		struct StringList *str = *result;

		c = C8_transform(c, (enum StringTransform) casing);

		U64 count = 0, last = 0;

		for (U64 i = 0; i < s.len; ++i)
			if (C8_transform(s.ptr[i], (enum StringTransform) casing) == c) {
				str->ptr[count++] = String_createRefConst(s.ptr + last, i - last);
				last = i + 1;
			}
	}

	return Error_none();
}

struct Error String_splitString(
	struct String s, 
	struct String other, 
	enum StringCase casing, 
	struct Allocator allocator, 
	struct StringList **result
) {

	U64 len = String_countAllString(s, other, casing);

	struct Error err = StringList_create(len, allocator, result);

	if (err.genericError)
		return err;

	if (result) {

		struct StringList *str = *result;

		U64 count = 0, last = 0;

		for (U64 i = 0; i < s.len; ++i) {

			Bool match = true;

			for (U64 j = i, k = 0; j < s.len && k < other.len; ++j, ++k)
				if (
					C8_transform(s.ptr[j], (enum StringTransform)casing) !=
					C8_transform(other.ptr[k], (enum StringTransform)casing)
				) {
					match = false;
					break;
				}

			if (match) {
				str->ptr[count++] = String_createRefConst(s.ptr + last, i - last);
				last = i + other.len;
			}
		}
	}

	return Error_none();
}

struct Error String_reserve(struct String *str, U64 siz, struct Allocator alloc) {

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

	for (U64 i = 0; i < str->len; ++i)		//Preserve old data
		b.ptr[i] = str->ptr[i];

	err = alloc.free(alloc.ptr, (struct Buffer){ .ptr = str->ptr, .siz = str->capacity });

	str->capacity = b.siz;
	str->len = siz;
	str->ptr = b.ptr;
	return err;
}

struct Error String_resize(struct String *str, U64 siz, C8 defaultChar, struct Allocator alloc) {

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
		return Error_none();
	}

	//Resize that triggers buffer resize

	if (siz > str->capacity) {

		//Reserve 50% more to ensure we don't resize too many times

		struct Error err = String_reserve(str, siz * 3 / 2, alloc);

		if (err.genericError)
			return err;
	}

	//Our capacity is big enough to handle it:

	for (U64 i = str->len; i < siz; ++i)
		str->ptr[i] = defaultChar;

	str->len = siz;
	return Error_none();
}

struct Error String_append(struct String *s, C8 c, struct Allocator allocator) {

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	return String_resize(s, s->len + 1, c, allocator);
}

struct Error String_appendString(struct String *s, struct String other, struct Allocator allocator) {

	if (!other.len || !other.ptr)
		return Error_none();

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	struct Error err = String_resize(s, s->len + other.len, other.ptr[0], allocator);

	if (err.genericError)
		return err;

	for (U64 i = 1; i < other.len; ++i)		//We can skip 0 since all N new chars are set to ptr[0]
		s->ptr[i] = other.ptr[i];

	return Error_none();
}

struct Error String_insert(struct String *s, C8 c, U64 i, struct Allocator allocator) {

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(i > s->len)
		return (struct Error) { .genericError = GenericError_OutOfBounds, .paramValue0 = i, .paramValue1 = s->len };

	struct Error err = String_resize(s, s->len + 1, c, allocator);

	if (err.genericError)
		return err;

	//If it's not append (otherwise it's already handled)

	if (i != s->len) {

		//Move one to the right

		for (U64 j = s->len; j != i; --j)
			s->ptr[j] = s->ptr[j - 1];

		s->ptr[i] = c;
	}

	return Error_none();
}

struct Error String_insertString(struct String *s, struct String other, U64 i, struct Allocator allocator) {

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (!other.len)
		return Error_none();

	U64 oldLen = s->len;
	struct Error err = String_resize(s, s->len + other.len, ' ', allocator);

	if (err.genericError)
		return err;

	for (U64 j = s->len; j > i + other.len; --j)
		s->ptr[j - 1] = s->ptr[j - other.len - 1];

	for (U64 j = i + other.len, k = other.len - 1; j > i; --j, --k)
		s->ptr[j - 1] = other.ptr[k];

	return Error_none();
}

struct Error String_replaceAllString(
	struct String *s,
	struct String search,
	struct String replace,
	enum StringCase caseSensitive,
	struct Allocator allocator
) {

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(String_isRef(*s))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	struct Buffer res = String_findAllString(*s, search, allocator, caseSensitive);

	if (!res.siz)
		return Error_none();

	//Easy replace

	U64 occurrences = res.siz / sizeof(U64);
	const U64 *ptr = (const U64*) res.ptr;

	if (search.len == replace.len) {

		for (U64 i = 0; i < occurrences; ++i)
			for (U64 j = ptr[i], k = j + replace.len, l = 0; j < k; ++j, ++l)
				s->ptr[j] = replace.ptr[l];

		return Bit_free(&res, allocator);
	}

	//Shrink replaces

	if (search.len > replace.len) {

		U64 diff = search.len - replace.len;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < occurrences; ++i) {

			//We move our replacement string to iCurrent

			for (U64 j = 0; j < replace.len; ++j)
				s->ptr[iCurrent++] = replace.ptr[j];

			//We then move the tail of the string 

			const U64 iStart = ptr[i] + search.len;
			const U64 iEnd = i == occurrences - 1 ? s->len : ptr[i + 1];

			for (U64 j = iStart; j < iEnd; ++j)
				s->ptr[iCurrent++] = s->ptr[j];
		}

		//Ensure the string is now the right size

		struct Error err = String_resize(s, s->len - diff * occurrences, ' ', allocator);
		
		if (err.genericError) {
			Bit_free(&res, allocator);
			return err;
		}

		return Bit_free(&res, allocator);
	}

	//Grow replaces

	//Ensure the string is now the right size

	U64 diff = replace.len - search.len;
	U64 sOldLen = s->len;

	struct Error err = String_resize(s, s->len + diff * occurrences, ' ', allocator);

	if (err.genericError) {
		Bit_free(&res, allocator);
		return err;
	}

	//Move from right to left

	U64 newLoc = s->len - 1;

	for (U64 i = occurrences - 1; i != U64_MAX; ++i) {

		//Find tail

		const U64 iStart = ptr[i] + search.len;
		const U64 iEnd = i == occurrences - 1 ? sOldLen : ptr[i + 1];

		for (U64 j = iEnd - 1; j != U64_MAX && j >= iStart; --j)
			s->ptr[newLoc--] = s->ptr[j];

		//Apply replacement before tail

		for (U64 j = replace.len - 1; j != U64_MAX; --j)
			s->ptr[newLoc--] = replace.ptr[j];
	}

	return Bit_free(&res, allocator);
}

struct Error String_replaceString(
	struct String *s,
	struct String search,
	struct String replace,
	enum StringCase caseSensitive,
	struct Allocator allocator,
	Bool isFirst
) {

	if (!s)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(String_isRef(*s))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	U64 res = isFirst ? String_findFirstString(*s, search, caseSensitive) : String_findLastString(*s, search, caseSensitive);

	if (res == s->len)
		return Error_none();

	//Easy, early exit. Strings are same size.

	if (search.len == replace.len) {

		for (U64 i = res, j = 0; j < replace.len; ++i, ++j)
			s->ptr[i] = replace.ptr[j];

		return Error_none();
	}

	//Replacement is smaller than our search
	//So we can just move from left to right
	
	if (replace.len < search.len) {

		U64 diff = search.len - replace.len;	//How much we have to shrink

		for (U64 i = res, j = 0; i < s->len - diff; ++i, ++j)
			s->ptr[i] = j < replace.len ? replace.ptr[j] : s->ptr[i + diff];

		//Shrink the string; this is done after because we need to read the right of the string first

		struct Error err = String_resize(s, s->len - diff, ' ', allocator);

		if (err.genericError)
			return err;

		return Error_none();
	}

	//Replacement is bigger than our search;
	//We need to grow first and move from right to left

	U64 diff = replace.len - search.len;

	struct Error err = String_resize(s, s->len + diff, ' ', allocator);

	if (err.genericError)
		return err;

	//Now we move from right to left to preserve data

	for (U64 i = s->len - 1; i != U64_MAX && i >= res; --i)
		s->ptr[i] = i < res + replace.len ? replace.ptr[i - res] : s->ptr[i - diff];

	return Error_none();
}

Bool String_startsWithString(struct String str, struct String other, enum StringCase caseSensitive) {

	if (other.len > str.len)
		return false;

	for (U64 i = 0; i < other.len; ++i)
		if (
			C8_transform(str.ptr[i], (enum StringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (enum StringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_endsWithString(struct String str, struct String other, enum StringCase caseSensitive) {

	if (other.len > str.len)
		return false;

	for (U64 i = str.len - other.len; i < str.len; ++i)
		if (
			C8_transform(str.ptr[i], (enum StringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (enum StringTransform)caseSensitive)
		)
			return false;

	return true;
}

U64 String_countAll(struct String s, C8 c, enum StringCase caseSensitive) {

	c = C8_transform(c, (enum StringTransform)caseSensitive);

	U64 count = 0;

	for (U64 i = 0; i < s.len; ++i)
		if (C8_transform(s.ptr[i], (enum StringTransform)caseSensitive) == c)
			++count;

	return count;
}

U64 String_countAllString(struct String s, struct String other, enum StringCase casing) {

	if(!other.len)
		return 0;

	U64 j = 0;

	for (U64 i = 0; i < s.len; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.len && k < other.len; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (enum StringTransform)casing) !=
				C8_transform(other.ptr[k], (enum StringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {
			i += s.len - 1;
			++j;
		}
	}

	return j;
}

struct Buffer String_findAll(
	struct String s,
	C8 c,
	struct Allocator alloc,
	enum StringCase caseSensitive
) {

	U64 count = String_countAll(s, c, caseSensitive);

	if (!count)
		return (struct Buffer) { 0 };

	//TODO: Array

	struct Buffer b = (struct Buffer) { 0 };
	struct Error err = Bit_createBytes(sizeof(U64) * count, alloc, &b);

	if (err.genericError)
		return (struct Buffer) { 0 };

	c = C8_transform(c, (enum StringTransform) caseSensitive);

	U64 *v = (U64*) b.ptr, j = 0;

	for (U64 i = 0; i < s.len; ++i)
		if (c == C8_transform(s.ptr[i], (enum StringTransform)caseSensitive))
			v[j++] = i;

	return b;
}

struct Buffer String_findAllString(
	struct String s,
	struct String other,
	struct Allocator alloc,
	enum StringCase casing
) {

	U64 count = String_countAllString(s, other, casing);

	if (!count)
		return (struct Buffer) { 0 };

	struct Buffer b = (struct Buffer) { 0 };
	struct Error err = Bit_createBytes(sizeof(U64) * count, alloc, &b);

	if (err.genericError)
		return (struct Buffer) { 0 };

	U64 *v = (U64*) b.ptr, j = 0;

	for (U64 i = 0; i < s.len; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.len && k < other.len; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (enum StringTransform)casing) !=
				C8_transform(other.ptr[k], (enum StringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {
			v[j++] = i;
			i += s.len - 1;
		}
	}

	return b;
}

U64 String_findFirst(struct String s, C8 c, enum StringCase caseSensitive) {

	c = C8_transform(c, (enum StringTransform)caseSensitive);

	for (U64 i = 0; i < s.len; ++i)
		if (C8_transform(s.ptr[i], (enum StringTransform)caseSensitive) == c)
			return i;

	return s.len;
}

U64 String_findLast(struct String s, C8 c, enum StringCase caseSensitive) {

	c = C8_transform(c, (enum StringTransform)caseSensitive);

	for (U64 i = s.len - 1; i != U64_MAX; --i)
		if (C8_transform(s.ptr[i], (enum StringTransform)caseSensitive) == c)
			return i;

	return s.len;
}

U64 String_findFirstString(struct String s, struct String other, enum StringCase casing) {

	if(!other.len)
		return s.len;

	U64 i = 0;

	for (; i < s.len; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.len && k < other.len; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (enum StringTransform)casing) !=
				C8_transform(other.ptr[k], (enum StringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match)
			break;
	}

	return i;
}

U64 String_findLastString(struct String s, struct String other, enum StringCase casing) {
	
	if(!other.len)
		return 0;

	U64 j = 0;

	for (U64 i = s.len - 1; i != U64_MAX; --i) {

		Bool match = true;

		for (U64 j = i, k = other.len - 1; j != U64_MAX && k != U64_MAX; --j, --k)
			if (
				C8_transform(s.ptr[j], (enum StringTransform)casing) !=
				C8_transform(other.ptr[k], (enum StringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match)
			++j;
	}

	return j;
}

Bool String_equalsString(struct String s, struct String other, enum StringCase caseSensitive) {

	if (s.len != other.len)
		return false;

	for (U64 i = 0; i < s.len; ++i)
		if (
			C8_transform(s.ptr[i], (enum StringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (enum StringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_equals(struct String s, C8 c, enum StringCase caseSensitive) {
	return s.len == 1 && s.ptr && 
		C8_transform(s.ptr[0], (enum StringTransform) caseSensitive) == 
		C8_transform(c, (enum StringTransform) caseSensitive);
}

Bool String_parseNyto(struct String s, U64 *result){

	struct String prefix = String_createRefUnsafeConst("0n");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 11)
		return false;

	*result = 0;

	for (U64 i = s.len - 1, j = 1; i != U64_MAX; --i, j <<= 6) {

		U8 v = C8_nyto(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64)1 << (10 * 6)) && v >= (1 << 4))		//We have 4 bits left
			return false;

		*result |= j * v;
	}

	return true;
}

Bool String_parseHex(struct String s, U64 *result){

	struct String prefix = String_createRefUnsafeConst("0x");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 16)
		return false;

	*result = 0;

	for (U64 i = s.len - 1, j = 1; i != U64_MAX; --i, j <<= 4) {

		U8 v = C8_hex(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		*result |= j * v;
	}

	return true;
}

Bool String_parseDec(struct String s, U64 *result) {

	if (!result || !s.ptr || s.len > 20)
		return false;

	*result = 0;

	for (U64 i = s.len - 1, j = 1; i != U64_MAX; --i, j *= 10) {

		U8 v = C8_dec(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == (U64) 1e19 && v > 1)		//Out of value
			return false;

		*result |= j * v;
	}

	return true;
}

Bool String_parseDecSigned(struct String s, I64 *result) {

	Bool neg = String_startsWith(s, '-', StringCase_Sensitive);

	struct Error err = String_offsetAsRef(s, neg, &s);

	if (err.genericError)
		return false;

	Bool b = String_parseDec(s, (U64*) result);

	if (b) {

		if (*(U64*)result > I64_MAX)
			return false;

		if(neg)
			*result = -*result;

		return true;
	}

	return false;
}

//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

Bool String_parseFloat(struct String s, F32 *result) {

	if (!result)
		return false;

	//Parse sign

	Bool sign = false;

	if (String_startsWith(s, '-', StringCase_Sensitive)) {

		sign = true;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (String_startsWith(s, '+', StringCase_Sensitive)) {

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	//Parse integer part

	F32 intPart = 0;

	while(
		!String_startsWith(s, '.', StringCase_Sensitive) && 
		!String_startsWith(s, 'e', StringCase_Insensitive)
	) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		intPart = intPart * 10 + v;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;

		if (!s.len) {
			*result = sign ? -intPart : intPart;
			return true;
		}
	}

	//Parse fraction

	if (
		String_startsWith(s, '.', StringCase_Sensitive) && 
		String_offsetAsRef(s, 1, &s).genericError
	)
		return false;

	F32 fraction = 0, multiplier = 0.1f;

	while(!String_startsWith(s, 'e', StringCase_Insensitive)) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		fraction += v * multiplier;
		multiplier *= 0.1f;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;

		if (!s.len) {
			*result = sign ? -(intPart + fraction) : intPart + fraction;
			return true;
		}
	}

	//Parse exponent sign

	if (String_offsetAsRef(s, 1, &s).genericError)
		return false;

	Bool esign = false;

	if (String_startsWith(s, '-', StringCase_Sensitive)) {

		esign = true;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (String_startsWith(s, '+', StringCase_Sensitive)) {

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	//Parse exponent (must be int)

	F32 exponent = 0;

	while(s.len) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		exponent = exponent * 10 + v;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	exponent = f32_exp10(esign ? -exponent : exponent);
	*result = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;
	return true;
}

Bool String_parseOct(struct String s, U64 *result) {

	struct String prefix = String_createRefUnsafeConst("0o");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 22)
		return false;

	*result = 0;

	for (U64 i = s.len - 1, j = 1; i != U64_MAX; --i, j <<= 3) {

		U8 v = C8_oct(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64)1 << (21 * 3)) && v > 1)		//Out of value
			return false;

		*result |= j * v;
	}

	return true;
}

Bool String_parseBin(struct String s, U64 *result) {

	struct String prefix = String_createRefUnsafeConst("0b");
	struct Error err = String_offsetAsRef(s, String_startsWithString(s, prefix, StringCase_Insensitive) ? prefix.len : 0, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 64)
		return false;

	*result = 0;

	for (U64 i = s.len - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

		U8 v = C8_bin(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		*result |= j * v;
	}

	return true;
}

Bool String_cut(struct String s, U64 offset, U64 siz, struct String *result) {

	if (!result || offset + siz > s.len)
		return false;

	if (offset == s.len) {
		*result = String_createEmpty();
		return false;
	}

	*result = String_isConstRef(s) ? String_createRefConst(s.ptr + offset, siz) :
		String_createRefDynamic(s.ptr + offset, siz);

	return true;
}

Bool String_cutAfter(struct String s, C8 c, enum StringCase caseSensitive, Bool isFirst, struct String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);
	return String_cut(s, 0, found, result);
}

Bool String_cutAfterString(struct String s, struct String other, enum StringCase caseSensitive, Bool isFirst, struct String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirstString(s, other, caseSensitive) : String_findLastString(s, other, caseSensitive);
	return String_cut(s, 0, found, result);
}

Bool String_cutBefore(struct String s, C8 c, enum StringCase caseSensitive, Bool isFirst, struct String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);

	if (found == s.len)
		return false;

	++found;	//The end of the occurence is the begin of the next string
	return String_cut(s, found, s.len - found, result);
}

Bool String_cutBeforeString(struct String s, struct String other, enum StringCase caseSensitive, Bool isFirst, struct String *result) {

	if (!result || !other.len)
		return false;

	U64 found = isFirst ? String_findFirstString(s, other, caseSensitive) : String_findLastString(s, other, caseSensitive);

	if (found == s.len)
		return false;

	found += other.len;	//The end of the occurence is the begin of the next string
	return String_cut(s, found, s.len - found, result);
}

Bool String_erase(struct String *s, C8 c, enum StringCase caseSensitive, Bool isFirst) {

	if(!s || String_isRef(*s))
		return false;

	c = C8_transform(c, (enum StringTransform) caseSensitive);

	U64 j = 0;
	Bool hadMatch = false;

	//Skipping first match

	if(isFirst)
		for (U64 i = 0; i < s->len; ++i) {

			if (!hadMatch && C8_transform(s->ptr[i], (enum StringTransform)caseSensitive) == c) {
				hadMatch = true;
				continue;
			}

			if (!hadMatch) {
				++j;
				continue;
			}

			s->ptr[j++] = s->ptr[i];
		}

	//Skipping last match

	else {

		U64 v = String_findLast(*s, c, caseSensitive);

		if (v != s->len) {

			for (U64 i = v; i < s->len; ++i)
				s->ptr[i - 1] = s->ptr[i];

			hadMatch = true;
		}
	}

	if (hadMatch)
		--s->len;

	return true;
}

Bool String_eraseString(struct String *s, struct String other, enum StringCase casing, Bool isFirst) {

	if(!s || !other.len || String_isRef(*s))
		return false;

	U64 out = 0;
	Bool hadMatch = false;

	if(isFirst)
		for (U64 i = 0; i < s->len; ++i) {

			if (!hadMatch) {

				Bool match = true;

				for (U64 j = i, k = 0; j < s->len && k < other.len; ++j, ++k)
					if (
						C8_transform(s->ptr[j], (enum StringTransform)casing) !=
						C8_transform(other.ptr[k], (enum StringTransform)casing)
					) {
						match = false;
						break;
					}

				if (match) {
					i += s->len - 1;	//-1 because we increment by 1 every time
					continue;
				}

				hadMatch = true;
			}

			s->ptr[out++] = s->ptr[i];
		}

	else {

		U64 match = String_findLastString(*s, other, casing);

		if (match == s->len)
			return true;

		for (U64 j = match + other.len, i = j; i < s->len; ++i)
			s->ptr[match + (i - j)] = s->ptr[i];
	}

	if(out == s->len)
		return true;

	s->len = out;
	return true;
}

//String's inline changes (no copy)

Bool String_cutEnd(struct String s, U64 i, struct String *result) {
	return String_cut(s, 0, i, result);
}

Bool String_cutBegin(struct String s, U64 i, struct String *result) {

	if (i > s.len)
		return false;

	return String_cut(s, i, s.len - i, result);
}

Bool String_eraseAll(struct String *s, C8 c, enum StringCase casing) {

	if(!s || String_isRef(*s))
		return false;

	c = C8_transform(s, (enum StringTransform) casing);

	U64 out = 0;

	for (U64 i = 0; i < s->len; ++i) {

		if(C8_transform(s->ptr[i], (enum StringTransform) casing) == c)
			continue;

		s->ptr[out++] = s->ptr[i];
	}
	
	if(out == s->len)
		return false;

	s->len = out;
	return true;
}

Bool String_eraseAllString(struct String *s, struct String other, enum StringCase casing) {

	if(!s || !other.len || String_isRef(*s))
		return false;

	U64 out = 0;

	for (U64 i = 0; i < s->len; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s->len && k < other.len; ++j, ++k)
			if (
				C8_transform(s->ptr[j], (enum StringTransform) casing) != 
				C8_transform(other.ptr[k], (enum StringTransform) casing)
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

	s->len = out;
	return true;
}

Bool String_replaceAll(struct String *s, C8 c, C8 v, enum StringCase caseSensitive) {

	if (!s || String_isConstRef(*s))
		return false;

	c = C8_transform(c, (enum StringTransform)caseSensitive);

	for(U64 i = 0; i < s->len; ++i)
		if(C8_transform(s->ptr[i], (enum StringTransform)caseSensitive) == c)
			s->ptr[i] = v;

	return true;
}

Bool String_replace(struct String *s, C8 c, C8 v, enum StringCase caseSensitive, Bool isFirst) {

	if (!s || String_isRef(*s))
		return false;

	U64 i = isFirst ? String_findFirst(*s, c, caseSensitive) : String_findLast(*s, c, caseSensitive);

	if(i != U64_MAX)
		s->ptr[i] = v;

	return true;
}

Bool String_transform(struct String str, enum StringTransform c) {

	if(String_isConstRef(str))
		return false;

	for(U64 i = 0; i < str.len; ++i)
		str.ptr[i] = C8_transform(str.ptr[i], c);

	return true;
}

struct String String_trim(struct String s) {

	U64 first = s.len;

	for (U64 i = 0; i < s.len; ++i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			first = i;
			break;
		}

	if (first == s.len)
		return (struct String) { 0 };

	U64 last = s.len;

	for (U64 i = s.len - 1; i != U64_MAX; --i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			last = i;
			break;
		}

	return String_isConstRef(s) ? String_createRefConst(s.ptr + first, last - first) :
		String_createRefDynamic(s.ptr + first, last - first);
}

//StringList

struct Error StringList_create(U64 len, struct Allocator alloc, struct StringList *arr) {

	struct StringList sl = (struct StringList) {
		.len = len
	};

	struct Buffer b = (struct Buffer) { 0 };
	struct Error err = alloc.alloc(alloc.ptr, len * sizeof(struct String), &b);

	sl.ptr = (struct String*) sl.ptr;

	if(err.genericError)
		return err;

	for(U64 i = 0; i < sl.len; ++i)
		sl.ptr[i] = String_createEmpty();

	*arr = sl;
	return Error_none();
}

struct Error StringList_free(struct StringList *arr, struct Allocator alloc) {

	if(!arr || !arr->len)
		return (struct Error){ .genericError = GenericError_NullPointer };

	struct Error freeErr = Error_none();

	for(U64 i = 0; i < arr->len; ++i) {

		struct String *str = arr->ptr + i;
		struct Error err = String_free(&str, alloc);

		if(err.genericError)
			freeErr = err;
	}

	struct Error err = alloc.free(alloc.ptr, (struct Buffer) {
		.ptr = (U8*) arr->ptr,
		.siz = sizeof(struct String) * arr->len
	});

	if(err.genericError)
		freeErr = err;

	*arr = (struct StringList){ 0 };
	return freeErr;
}

struct Error StringList_createCopy(const struct StringList *toCopy, struct StringList **arr, struct Allocator alloc) {

	if(!toCopy || !toCopy->len)
		return (struct Error){ .genericError = GenericError_NullPointer };

	struct Error err = StringList_create(toCopy->len, alloc, arr);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < toCopy->len; ++i) {

		err = String_createCopy(toCopy->ptr[i], alloc, (*arr)->ptr + i);

		if (err.genericError) {
			StringList_free(arr, alloc);
			return err;
		}
	}

	return Error_none();
}

struct Error StringList_unset(struct StringList arr, U64 i, struct Allocator alloc) {

	if(i >= arr.len)
		return (struct Error) { 
			.genericError = GenericError_OutOfBounds, 
			.paramValue0 = i, 
			.paramValue1 = arr.len 
		};

	struct String *pstr = arr.ptr + i;
	return String_free(&pstr, alloc);
}

struct Error StringList_set(struct StringList arr, U64 i, struct String str, struct Allocator alloc) {

	struct Error err = StringList_unset(arr, i, alloc);

	if(err.genericError)
		return err;

	arr.ptr[i] = str;
	return Error_none();
}
