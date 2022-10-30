#include "types/string.h"
#include "types/allocator.h"
#include "math/math.h"
#include "types/buffer.h"
#include "types/list.h"

#include <ctype.h>

//Simple checks (consts)

Error String_offsetAsRef(String s, U64 off, String *result) {
	
	if (!result)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 2 };

	if (String_isEmpty(s)) {
		*result = String_createEmpty();
		return Error_none();
	}

	if(off >= s.len)
		return (Error) { 
			.genericError = EGenericError_OutOfBounds, 
			.paramId = 1,
			.paramValue0 = off,
			.paramValue1 = s.len 
		};

	*result = (String) {
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
		String_createRefUnsafeConst(num),									\
		EStringCase_Insensitive												\
	) ? String_createRefUnsafeConst(num).len : 0							\
)

Bool String_isNyto(String s) {
	String_matchesPatternNum(C8_isNyto, "0n");
}

Bool String_isAlphaNumeric(String s) {
	String_matchesPattern(C8_isAlphaNumeric, 0);
}

Bool String_isHex(String s) {
	String_matchesPatternNum(C8_isHex, "0x");
}

Bool String_isOct(String s) {
	String_matchesPatternNum(C8_isOct, "0o");
}

Bool String_isBin(String s) {
	String_matchesPatternNum(C8_isBin, "0b");
}

Bool String_isDec(String s) {
	String_matchesPattern(C8_isDec, 0);
}

Bool String_isSignedNumber(String s) {
	String_matchesPattern(
		C8_isDec,
		String_startsWith(s, '-', EStringCase_Sensitive) ||
		String_startsWith(s, '+', EStringCase_Sensitive)
	);
}

Bool String_isUnsignedNumber(String s) {
	String_matchesPattern(
		C8_isDec,
		String_startsWith(s, '+', EStringCase_Sensitive)
	);
}

Bool String_isFloat(String s) {

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

Error String_create(C8 c, U64 size, Allocator alloc, String *result) {

	if (!c)
		return (Error) { .genericError = EGenericError_InvalidParameter };

	if (!alloc.alloc)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 2 };

	if (!result)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 3 };

	if (!size) {
		*result = String_createEmpty();
		return Error_none();
	}

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, size, &b);

	if (err.genericError)
		return err;

	U64 totalSize = size;

	//Quick fill

	U16 cc2 = ((U16)c << 8) | ((U16)c << 0);
	U32 cc4 = ((U32)cc2 << 16) | cc2;
	U64 cc8 = ((U64)cc4 << 32) | cc4;

	if (size >= 8) {

		for (U64 i = 0; i < size >> 3; ++i)
			*((U64*)b.ptr + i) = cc8;

		size &= 7;
	}

	if (size >= 4) {
		*((U32*)b.ptr + (size >> 3 << 1)) = cc4;
		size &= 3;
	}

	if (size >= 2) {
		*((U16*)b.ptr + (size >> 2 << 1)) = cc2;
		size &= 1;
	}

	if(size >= 1)
		b.ptr[totalSize - 1] = c;

	*result = (String) { .len = totalSize, .capacity = totalSize, .ptr = (C8*) b.ptr };
	return Error_none();
}

Error String_createCopy(String str, Allocator alloc, String *result) {

	if (!alloc.alloc)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 1 };

	if (!result)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 2 };

	if (!str.len) {
		*result = String_createEmpty();
		return Error_none();
	}

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, str.len, &b);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < str.len >> 3; ++i)
		*((U64*)b.ptr + i) = *((const U64*)str.ptr + i);

	for (U64 i = str.len >> 3 << 3; i < str.len; ++i)
		b.ptr[i] = str.ptr[i];

	*result = (String) { .ptr = (C8*) b.ptr, .len = str.len, .capacity = str.len };
	return Error_none();
}

Error String_free(String *str, Allocator alloc) {

	if (!str)
		return (Error) { .genericError = EGenericError_NullPointer };

	if (!alloc.free)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 1 };

	Error err = Error_none();

	if(!String_isRef(*str))
		err = alloc.free(alloc.ptr, Buffer_createRef(str->ptr, str->capacity));

	*str = String_createEmpty();
	return err;
}

#define String_createNum(maxVal, func, prefixRaw, ...)					\
																		\
	String prefix = String_createRefUnsafeConst(prefixRaw);				\
																		\
	if (result->len)													\
		return (Error) { 												\
			.genericError = EGenericError_InvalidOperation, 				\
			.errorSubId = 1 											\
		};																\
																		\
	*result = String_createEmpty();										\
	Error err = String_reserve(											\
		result, maxVal + prefix.len, allocator							\
	);																	\
																		\
	if (err.genericError) 												\
		return err;														\
																		\
	err = String_appendString(result, prefix, allocator);				\
																		\
	if (err.genericError) {												\
		String_free(result, allocator);									\
		return err;														\
	}																	\
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

Error String_createNyto(U64 v, Bool leadingZeros, Allocator allocator, String *result){
	String_createNum(11, Nyto, "0n", (v >> (6 * i)) & 63);
}

Error String_createHex(U64 v, Bool leadingZeros, Allocator allocator, String *result) {
	String_createNum(16, Hex, "0x", (v >> (4 * i)) & 15);
}

Error String_createDec(U64 v, Bool leadingZeros, Allocator allocator, String *result) {
	String_createNum(20, Dec, "", (v / U64_pow10(i)) % 10);
}

Error String_createOct(U64 v, Bool leadingZeros, Allocator allocator, String *result) {
	String_createNum(22, Oct, "0o", (v >> (3 * i)) & 7);
}

Error String_createBin(U64 v, Bool leadingZeros, Allocator allocator, String *result) {
	String_createNum(64, Bin, "0b", (v >> i) & 1);
}

Error String_split(
	String s, 
	C8 c, 
	EStringCase casing, 
	Allocator allocator, 
	StringList *result
) {

	U64 len = String_countAll(s, c, casing);

	Error err = StringList_create(len, allocator, result);

	if (err.genericError)
		return err;

	if (result) {

		StringList str = *result;

		c = C8_transform(c, (EStringTransform) casing);

		U64 count = 0, last = 0;

		for (U64 i = 0; i < s.len; ++i)
			if (C8_transform(s.ptr[i], (EStringTransform) casing) == c) {

				str.ptr[count++] = 
					String_isConstRef(s) ? String_createRefConst(s.ptr + last, i - last) : 
					String_createRefDynamic(s.ptr + last, i - last);

				last = i + 1;
			}
	}

	return Error_none();
}

Error String_splitString(
	String s, 
	String other, 
	EStringCase casing, 
	Allocator allocator, 
	StringList *result
) {

	U64 len = String_countAllString(s, other, casing);

	Error err = StringList_create(len, allocator, result);

	if (err.genericError)
		return err;

	if (result) {

		StringList str = *result;

		U64 count = 0, last = 0;

		for (U64 i = 0; i < s.len; ++i) {

			Bool match = true;

			for (U64 j = i, k = 0; j < s.len && k < other.len; ++j, ++k)
				if (
					C8_transform(s.ptr[j], (EStringTransform)casing) !=
					C8_transform(other.ptr[k], (EStringTransform)casing)
				) {
					match = false;
					break;
				}

			if (match) {

				str.ptr[count++] = 
					String_isConstRef(s) ? String_createRefConst(s.ptr + last, i - last) : 
					String_createRefDynamic(s.ptr + last, i - last);

				last = i + other.len;
				i += other.len - 1;
			}
		}
	}

	return Error_none();
}

Error String_reserve(String *str, U64 siz, Allocator alloc) {

	if (!str)
		return (Error) { .genericError = EGenericError_NullPointer };

	if (String_isRef(*str) && str->len)
		return (Error) { .genericError = EGenericError_InvalidOperation };

	if (!alloc.alloc || !alloc.free)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 2 };

	if (siz <= str->capacity)
		return Error_none();

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, siz, &b);

	if (err.genericError)
		return err;

	Buffer_copy(b, String_buffer(*str));

	err = alloc.free(alloc.ptr, Buffer_createRef(str->ptr, str->capacity));

	str->capacity = b.siz;
	str->ptr = (C8*) b.ptr;
	return err;
}

Error String_resize(String *str, U64 siz, C8 defaultChar, Allocator alloc) {

	if (!str)
		return (Error) { .genericError = EGenericError_NullPointer };

	if (String_isRef(*str) && str->len)
		return (Error) { .genericError = EGenericError_InvalidOperation };

	if (!alloc.alloc || !alloc.free)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 2 };

	if (siz == str->len)
		return Error_none();

	//Simple resize; we cut off the tail

	if (siz < str->len) {
		str->len = siz;
		return Error_none();
	}

	//Resize that triggers buffer resize

	if (siz > str->capacity) {

		if (siz * 3 / 3 != siz)
			return (Error) { .genericError = EGenericError_Overflow };

		//Reserve 50% more to ensure we don't resize too many times

		Error err = String_reserve(str, siz * 3 / 2, alloc);

		if (err.genericError)
			return err;
	}

	//Our capacity is big enough to handle it:

	for (U64 i = str->len; i < siz; ++i)
		str->ptr[i] = defaultChar;

	str->len = siz;
	return Error_none();
}

Error String_append(String *s, C8 c, Allocator allocator) {

	if (!s)
		return (Error) { .genericError = EGenericError_NullPointer };

	return String_resize(s, s->len + 1, c, allocator);
}

Error String_appendString(String *s, String other, Allocator allocator) {

	if (!other.len || !other.ptr)
		return Error_none();

	if (!s)
		return (Error) { .genericError = EGenericError_NullPointer };

	U64 oldLen = s->len;
	Error err = String_resize(s, s->len + other.len, other.ptr[0], allocator);

	if (err.genericError)
		return err;

	Buffer_copy(Buffer_createRef(s->ptr + oldLen, other.len), String_buffer(other));
	return Error_none();
}

Error String_insert(String *s, C8 c, U64 i, Allocator allocator) {

	if (!s)
		return (Error) { .genericError = EGenericError_NullPointer };

	if(i > s->len)
		return (Error) { .genericError = EGenericError_OutOfBounds, .paramValue0 = i, .paramValue1 = s->len };

	Error err = String_resize(s, s->len + 1, c, allocator);

	if (err.genericError)
		return err;

	//If it's not append (otherwise it's already handled)

	if (i != s->len) {

		//Move one to the right

		Buffer_revCopy(
			Buffer_createRef(s->ptr + i + 1, s->len - 1),
			Buffer_createRef(s->ptr + i, s->len - 1)
		);

		s->ptr[i] = c;
	}

	return Error_none();
}

Error String_insertString(String *s, String other, U64 i, Allocator allocator) {

	if (!s)
		return (Error) { .genericError = EGenericError_NullPointer };

	if (!other.len)
		return Error_none();

	U64 oldLen = s->len;
	Error err = String_resize(s, s->len + other.len, ' ', allocator);

	if (err.genericError)
		return err;

	//Move one to the right

	Buffer_revCopy(
		Buffer_createRef(s->ptr + i + other.len, oldLen - i),
		Buffer_createRef(s->ptr + i, oldLen - i)
	);

	Buffer_copy(
		Buffer_createRef(s->ptr + i, other.len),
		String_buffer(other)
	);

	return Error_none();
}

Error String_replaceAllString(
	String *s,
	String search,
	String replace,
	EStringCase caseSensitive,
	Allocator allocator
) {

	if (!s)
		return (Error) { .genericError = EGenericError_NullPointer };

	if(String_isRef(*s))
		return (Error) { .genericError = EGenericError_InvalidOperation };

	List finds = String_findAllString(*s, search, allocator, caseSensitive);

	if (!finds.length)
		return Error_none();

	//Easy replace

	const U64 *ptr = (const U64*) finds.ptr;

	if (search.len == replace.len) {

		for (U64 i = 0; i < finds.length; ++i)
			for (U64 j = ptr[i], k = j + replace.len, l = 0; j < k; ++j, ++l)
				s->ptr[j] = replace.ptr[l];

		return List_free(&finds, allocator);
	}

	//Shrink replaces

	if (search.len > replace.len) {

		U64 diff = search.len - replace.len;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			//We move our replacement string to iCurrent

			Buffer_copy(Buffer_createRef(s->ptr + iCurrent, replace.len), String_buffer(replace));
			iCurrent += replace.len;

			//We then move the tail of the string 

			const U64 iStart = ptr[i] + search.len;
			const U64 iEnd = i == finds.length - 1 ? s->len : ptr[i + 1];

			Buffer_copy(
				Buffer_createRef(s->ptr + iCurrent, iEnd - iStart), 
				Buffer_createRef(s->ptr + iStart, iEnd - iStart)
			);

			iCurrent += iEnd - iStart;
		}

		//Ensure the string is now the right size

		Error err = String_resize(s, s->len - diff * finds.length, ' ', allocator);
		
		if (err.genericError) {
			List_free(&finds, allocator);
			return err;
		}

		return List_free(&finds, allocator);
	}

	//Grow replaces

	//Ensure the string is now the right size

	U64 diff = replace.len - search.len;
	U64 sOldLen = s->len;

	Error err = String_resize(s, s->len + diff * finds.length, ' ', allocator);

	if (err.genericError) {
		List_free(&finds, allocator);
		return err;
	}

	//Move from right to left

	U64 newLoc = s->len - 1;

	for (U64 i = finds.length - 1; i != U64_MAX; ++i) {

		//Find tail

		const U64 iStart = ptr[i] + search.len;
		const U64 iEnd = i == finds.length - 1 ? sOldLen : ptr[i + 1];

		for (U64 j = iEnd - 1; j != U64_MAX && j >= iStart; --j)
			s->ptr[newLoc--] = s->ptr[j];

		Buffer_revCopy(
			Buffer_createRef(s->ptr + newLoc - (iEnd - iStart), iEnd - iStart),
			Buffer_createRef(s->ptr + iStart, iEnd - iStart)
		);

		newLoc -= iEnd - iStart;

		//Apply replacement before tail

		Buffer_revCopy(Buffer_createRef(s->ptr + newLoc - replace.len, replace.len), String_buffer(replace));
		newLoc -= replace.len;
	}

	return List_free(&finds, allocator);
}

Error String_replaceString(
	String *s,
	String search,
	String replace,
	EStringCase caseSensitive,
	Allocator allocator,
	Bool isFirst
) {

	if (!s)
		return (Error) { .genericError = EGenericError_NullPointer };

	if(String_isRef(*s))
		return (Error) { .genericError = EGenericError_InvalidOperation };

	U64 res = isFirst ? String_findFirstString(*s, search, caseSensitive) : 
		String_findLastString(*s, search, caseSensitive);

	if (res == s->len)
		return Error_none();

	//Easy, early exit. Strings are same size.

	if (search.len == replace.len) {
		Buffer_copy(Buffer_createRef(s->ptr + res, replace.len), String_buffer(replace));
		return Error_none();
	}

	//Replacement is smaller than our search
	//So we can just move from left to right
	
	if (replace.len < search.len) {

		U64 diff = search.len - replace.len;	//How much we have to shrink

		//Copy our data over first

		Buffer_copy(
			Buffer_createRef(s->ptr + res + replace.len, s->len - (res + search.len)), 
			Buffer_createRef(s->ptr + res + search.len, s->len - (res + search.len)) 
		);

		//Throw our replacement in there

		Buffer_copy(Buffer_createRef(s->ptr + res, replace.len), String_buffer(replace));

		//Shrink the string; this is done after because we need to read the right of the string first

		Error err = String_resize(s, s->len - diff, ' ', allocator);

		if (err.genericError)
			return err;

		return Error_none();
	}

	//Replacement is bigger than our search;
	//We need to grow first and move from right to left

	U64 diff = replace.len - search.len;

	Error err = String_resize(s, s->len + diff, ' ', allocator);

	if (err.genericError)
		return err;

	//Copy our data over first

	Buffer_revCopy(
		Buffer_createRef(s->ptr + res + replace.len, s->len - (res + search.len)), 
		Buffer_createRef(s->ptr + res + search.len, s->len - (res + search.len)) 
	);

	//Throw our replacement in there

	Buffer_copy(Buffer_createRef(s->ptr + res, replace.len), String_buffer(replace));
	return Error_none();
}

Bool String_startsWithString(String str, String other, EStringCase caseSensitive) {

	if (other.len > str.len)
		return false;

	for (U64 i = 0; i < other.len; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_endsWithString(String str, String other, EStringCase caseSensitive) {

	if (other.len > str.len)
		return false;

	for (U64 i = str.len - other.len; i < str.len; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i - (str.len - other.len)], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

U64 String_countAll(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	U64 count = 0;

	for (U64 i = 0; i < s.len; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			++count;

	return count;
}

U64 String_countAllString(String s, String other, EStringCase casing) {

	if(!other.len)
		return 0;

	U64 j = 0;

	for (U64 i = 0; i < s.len; ++i) {

		Bool match = true;

		for (U64 l = i, k = 0; l < s.len && k < other.len; ++l, ++k)
			if (
				C8_transform(s.ptr[l], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
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

List String_findAll(
	String s,
	C8 c,
	Allocator alloc,
	EStringCase caseSensitive
) {

	List l = List_createEmpty(sizeof(U64));
	Error err = List_reserve(&l, s.len / 25 + 16, alloc);

	if (err.genericError)
		return List_createEmpty(sizeof(U64));

	c = C8_transform(c, (EStringTransform) caseSensitive);

	for (U64 i = 0; i < s.len; ++i)
		if (c == C8_transform(s.ptr[i], (EStringTransform)caseSensitive))
			if ((err = List_pushBack(&l, Buffer_createRef(&i, sizeof(i)), alloc)).genericError) {
				List_free(&l, alloc);
				return List_createEmpty(sizeof(U64));
			}

	return l;
}

List String_findAllString(
	String s,
	String other,
	Allocator alloc,
	EStringCase casing
) {

	if(!other.len)
		return List_createEmpty(sizeof(U64));

	List l = List_createEmpty(sizeof(U64));
	Error err = List_reserve(&l, s.len / other.len / 25 + 16, alloc);

	if (err.genericError)
		return List_createEmpty(sizeof(U64));

	for (U64 i = 0; i < s.len; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.len && k < other.len; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {

			if ((err = List_pushBack(&l, Buffer_createRef(&i, sizeof(i)), alloc)).genericError) {
				List_free(&l, alloc);
				return List_createEmpty(sizeof(U64));
			}

			i += s.len - 1;
		}
	}

	return l;
}

U64 String_findFirst(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = 0; i < s.len; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return s.len;
}

U64 String_findLast(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = s.len - 1; i != U64_MAX; --i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	//TODO: Don't return len on end!

	return s.len;
}

U64 String_findFirstString(String s, String other, EStringCase casing) {

	if(!other.len)
		return s.len;

	U64 i = 0;

	for (; i < s.len; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.len && k < other.len; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match)
			break;
	}

	return i;
}

U64 String_findLastString(String s, String other, EStringCase casing) {
	
	if(!other.len)
		return 0;

	U64 i = s.len - 1;

	for (; i != U64_MAX; --i) {

		Bool match = true;

		for (U64 j = i, k = other.len - 1; j != U64_MAX && k != U64_MAX; --j, --k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match)
			break;
	}

	return i;
}

Bool String_equalsString(String s, String other, EStringCase caseSensitive) {

	if (s.len != other.len)
		return false;

	for (U64 i = 0; i < s.len; ++i)
		if (
			C8_transform(s.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_equals(String s, C8 c, EStringCase caseSensitive) {
	return s.len == 1 && s.ptr && 
		C8_transform(s.ptr[0], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_parseNyto(String s, U64 *result){

	String prefix = String_createRefUnsafeConst("0n");

	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.len : 0, &s
	);

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

Bool String_parseHex(String s, U64 *result){

	String prefix = String_createRefUnsafeConst("0x");
	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.len : 0, &s
	);

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

Bool String_parseDec(String s, U64 *result) {

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

Bool String_parseDecSigned(String s, I64 *result) {

	Bool neg = String_startsWith(s, '-', EStringCase_Sensitive);

	Error err = String_offsetAsRef(s, neg, &s);

	if (err.genericError)
		return false;

	Bool b = String_parseDec(s, (U64*) result);

	if (b) {

		if (neg && *(U64*)result == (U64)I64_MIN) {
			*result = I64_MIN;
			return true;
		}

		if (!neg && *(U64*)result > (U64) I64_MAX)
			return false;

		if(neg)
			*result = -*(I64*)result;

		return true;
	}

	return false;
}

//Approximately equal to: [-+]?[0-9]*[.[0-9]*]?[[eE][-+]?[0-9]+]?

Bool String_parseFloat(String s, F32 *result) {

	if (!result)
		return false;

	//Parse sign

	Bool sign = false;

	if (String_startsWith(s, '-', EStringCase_Sensitive)) {

		sign = true;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (String_startsWith(s, '+', EStringCase_Sensitive)) {

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	//Parse integer part

	F32 intPart = 0;

	while(
		!String_startsWith(s, '.', EStringCase_Sensitive) && 
		!String_startsWith(s, 'e', EStringCase_Insensitive)
	) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		intPart = intPart * 10 + v;

		if(!F32_isValid(intPart))
			return false;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;

		if (!s.len) {
			*result = sign ? -intPart : intPart;
			return true;
		}
	}

	//Parse fraction

	if (
		String_startsWith(s, '.', EStringCase_Sensitive) && 
		String_offsetAsRef(s, 1, &s).genericError
	)
		return false;

	F32 fraction = 0, multiplier = 0.1f;

	while(!String_startsWith(s, 'e', EStringCase_Insensitive)) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		fraction += v * multiplier;
		multiplier *= 0.1f;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;

		if (!s.len) {
			*result = sign ? -(intPart + fraction) : intPart + fraction;
			return F32_isValid(*result);
		}
	}

	//Parse exponent sign

	if (String_offsetAsRef(s, 1, &s).genericError)
		return false;

	Bool esign = false;

	if (String_startsWith(s, '-', EStringCase_Sensitive)) {

		esign = true;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (String_startsWith(s, '+', EStringCase_Sensitive)) {

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

	Error err = F32_exp10(esign ? -exponent : exponent, &exponent);

	if(err.genericError)
		return false;

	*result = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;
	return F32_isValid(*result);
}

Bool String_parseOct(String s, U64 *result) {

	String prefix = String_createRefUnsafeConst("0o");
	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.len : 0, &s
	);

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

Bool String_parseBin(String s, U64 *result) {

	String prefix = String_createRefUnsafeConst("0b");
	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.len : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.len > 64)
		return false;

	*result = 0;

	for (U64 i = s.len - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

		U8 v = C8_bin(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(v)
			*result |= j;
	}

	return true;
}

Bool String_cut(String s, U64 offset, U64 siz, String *result) {

	if(!result)
		return false;

	if (!s.len && !offset && !siz) {
		*result = String_createEmpty();
		return true;
	}

	if(offset >= s.len)
		return false;

	if(!siz) 
		siz = s.len - offset;

	if (offset + siz > s.len)
		return false;

	if (offset == s.len) {
		*result = String_createEmpty();
		return false;
	}

	*result = String_isConstRef(s) ? String_createRefConst(s.ptr + offset, siz) :
		String_createRefDynamic(s.ptr + offset, siz);

	return true;
}

Bool String_cutAfter(String s, C8 c, EStringCase caseSensitive, Bool isFirst, String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);
	return String_cut(s, 0, found, result);
}

Bool String_cutAfterString(String s, String other, EStringCase caseSensitive, Bool isFirst, String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);

	return String_cut(s, 0, found, result);
}

Bool String_cutBefore(String s, C8 c, EStringCase caseSensitive, Bool isFirst, String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);

	if (found == s.len)
		return false;

	++found;	//The end of the occurence is the begin of the next string
	return String_cut(s, found, 0, result);
}

Bool String_cutBeforeString(String s, String other, EStringCase caseSensitive, Bool isFirst, String *result) {

	if (!result || !other.len)
		return false;

	U64 found = isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);

	if (found == s.len)
		return false;

	found += other.len;	//The end of the occurence is the begin of the next string
	return String_cut(s, found, 0, result);
}

Bool String_erase(String *s, C8 c, EStringCase caseSensitive, Bool isFirst) {

	if(!s || String_isRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	Bool hadMatch = false;

	//Skipping first match

	U64 find = String_find(*s, c, caseSensitive, isFirst);

	if(find == s->len)
		return false;

	Buffer_copy(
		Buffer_createRef(s->ptr + find, s->len - find - 1),
		Buffer_createRef(s->ptr + find + 1, s->len - find - 1)
	);

	if (hadMatch)
		--s->len;

	return true;
}

Bool String_eraseString(String *s, String other, EStringCase casing, Bool isFirst) {

	if(!s || !other.len || String_isRef(*s))
		return false;

	//Skipping first match

	U64 find = String_findString(*s, other, casing, isFirst);

	if(find == s->len)
		return false;

	Buffer_copy(
		Buffer_createRef(s->ptr + find, s->len - find - other.len),
		Buffer_createRef(s->ptr + find + other.len, s->len - find - other.len)
	);

	s->len -= other.len;
	return true;
}

//String's inline changes (no copy)

Bool String_cutEnd(String s, U64 i, String *result) {
	return String_cut(s, 0, i, result);
}

Bool String_cutBegin(String s, U64 i, String *result) {

	if (i > s.len)
		return false;

	return String_cut(s, i, 0, result);
}

Bool String_eraseAll(String *s, C8 c, EStringCase casing) {

	if(!s || String_isRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform) casing);

	U64 out = 0;

	for (U64 i = 0; i < s->len; ++i) {

		if(C8_transform(s->ptr[i], (EStringTransform) casing) == c)
			continue;

		s->ptr[out++] = s->ptr[i];
	}
	
	if(out == s->len)
		return false;

	s->len = out;
	return true;
}

Bool String_eraseAllString(String *s, String other, EStringCase casing) {

	if(!s || !other.len || String_isRef(*s))
		return false;

	U64 out = 0;

	for (U64 i = 0; i < s->len; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s->len && k < other.len; ++j, ++k)
			if (
				C8_transform(s->ptr[j], (EStringTransform) casing) != 
				C8_transform(other.ptr[k], (EStringTransform) casing)
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

Bool String_replaceAll(String *s, C8 c, C8 v, EStringCase caseSensitive) {

	if (!s || String_isConstRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for(U64 i = 0; i < s->len; ++i)
		if(C8_transform(s->ptr[i], (EStringTransform)caseSensitive) == c)
			s->ptr[i] = v;

	return true;
}

Bool String_replace(String *s, C8 c, C8 v, EStringCase caseSensitive, Bool isFirst) {

	if (!s || String_isRef(*s))
		return false;

	U64 i = isFirst ? String_findFirst(*s, c, caseSensitive) : String_findLast(*s, c, caseSensitive);

	if(i != U64_MAX)
		s->ptr[i] = v;

	return true;
}

Bool String_transform(String str, EStringTransform c) {

	if(String_isConstRef(str))
		return false;

	for(U64 i = 0; i < str.len; ++i)
		str.ptr[i] = C8_transform(str.ptr[i], c);

	return true;
}

String String_trim(String s) {

	U64 first = s.len;

	for (U64 i = 0; i < s.len; ++i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			first = i;
			break;
		}

	if (first == s.len)
		return String_createEmpty();

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

Error StringList_create(U64 len, Allocator alloc, StringList *arr) {

	StringList sl = (StringList) { .len = len };

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, len * sizeof(String), &b);

	sl.ptr = (String*) sl.ptr;

	if(err.genericError)
		return err;

	for(U64 i = 0; i < sl.len; ++i)
		sl.ptr[i] = String_createEmpty();

	*arr = sl;
	return Error_none();
}

Error StringList_free(StringList *arr, Allocator alloc) {

	if(!arr || !arr->len)
		return (Error){ .genericError = EGenericError_NullPointer };

	Error freeErr = Error_none();

	for(U64 i = 0; i < arr->len; ++i) {

		String *str = arr->ptr + i;
		Error err = String_free(str, alloc);

		if(err.genericError)
			freeErr = err;
	}

	Error err = alloc.free(alloc.ptr, Buffer_createRef(
		arr->ptr,
		sizeof(String) * arr->len
	));

	if(err.genericError)
		freeErr = err;

	*arr = (StringList){ 0 };
	return freeErr;
}

Error StringList_createCopy(const StringList *toCopy, StringList *arr, Allocator alloc) {

	if(!toCopy || !toCopy->len)
		return (Error){ .genericError = EGenericError_NullPointer };

	Error err = StringList_create(toCopy->len, alloc, arr);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < toCopy->len; ++i) {

		err = String_createCopy(toCopy->ptr[i], alloc, arr->ptr + i);

		if (err.genericError) {
			StringList_free(arr, alloc);
			return err;
		}
	}

	return Error_none();
}

Error StringList_unset(StringList arr, U64 i, Allocator alloc) {

	if(i >= arr.len)
		return (Error) { 
			.genericError = EGenericError_OutOfBounds, 
			.paramValue0 = i, 
			.paramValue1 = arr.len 
		};

	String *pstr = arr.ptr + i;
	return String_free(pstr, alloc);
}

Error StringList_set(StringList arr, U64 i, String str, Allocator alloc) {

	Error err = StringList_unset(arr, i, alloc);

	if(err.genericError)
		return err;

	arr.ptr[i] = str;
	return Error_none();
}

U64 String_calcStrLen(const C8 *ptr, U64 maxSize) {

	U64 i = 0;

	for(; i < maxSize && ptr[i]; ++i)
		;

	return i;
}

U64 String_hash(String s) { 

	U64 h = FNVHash_create();

	const U64 *ptr = (const U64*)s.ptr, *end = ptr + (s.len >> 3);

	for(; ptr < end; ++ptr)
		h = FNVHash_apply(h, *ptr);

	U64 last = 0, shift = 0;

	if (s.len & 4) {
		last = *(const U32*) ptr;
		shift = 4;
		ptr = (const U64*)((const U32*)ptr + 1);
	}

	if (s.len & 2) {
		last |= (U64)(*(const U16*)ptr) << shift;
		shift |= 2;
		ptr = (const U64*)((const U16*)ptr + 1);
	}

	if (s.len & 1)
		last |= (U64)(*(const U8*)ptr) << shift++;

	if(shift)
		h = FNVHash_apply(h, last);

	return h;
}

String String_getFilePath(String *str) {

	if(!String_formatPath(str))
		return String_createEmpty();

	String res;

	if(String_cutBefore(*str, '/', EStringCase_Insensitive, false, &res))
		return res;
	
	return String_createEmpty();
}

String String_getBasePath(String *str) {

	if(!String_formatPath(str))
		return String_createEmpty();

	String res;

	if(String_cutAfter(*str, '/', EStringCase_Insensitive, false, &res))
		return res;

	return String_createEmpty();
}
