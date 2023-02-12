/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "types/string.h"
#include "types/allocator.h"
#include "math/math.h"
#include "types/buffer.h"
#include "types/list.h"
#include "types/error.h"

#include <ctype.h>

Bool String_isConstRef(String str) { return str.capacity == U64_MAX; }
Bool String_isRef(String str) { return !str.capacity || String_isConstRef(str); }
Bool String_isEmpty(String str) { return !str.length || !str.ptr; }
U64  String_bytes(String str) { return str.length; }

Buffer String_buffer(String str) { 
	return String_isConstRef(str) ? Buffer_createNull() : Buffer_createRef(str.ptr, str.length); 
}

Buffer String_bufferConst(String str) { return Buffer_createConstRef(str.ptr, str.length); }

//Iteration

C8 *String_begin(String str) { 
	return String_isConstRef(str) ? NULL : str.ptr; 
}

C8 *String_end(String str) {
	return String_isConstRef(str) ? NULL : str.ptr + str.length;
}

C8 *String_charAt(String str, U64 off) { 
	return String_isConstRef(str) || off >= str.length ? NULL : str.ptr + off; 
}

const C8 *String_beginConst(String str) { return str.ptr; }
const C8 *String_endConst(String str) { return str.ptr + str.length; }

const C8 *String_charAtConst(String str, U64 off) { 
	return off >= str.length ? NULL : str.ptr + off; 
}

Bool String_isValidAscii(String str) {

	for(U64 i = 0; i < str.length; ++i)
		if(!C8_isValidAscii(str.ptr[i]))
			return false;

	return true;
}

Bool String_isValidFileName(String str, Bool acceptTrailingNull) {

	for(U64 i = 0; i < str.length; ++i)
		if(!C8_isValidFileName(str.ptr[i]) && !(i == str.length - 1 && !str.ptr[i] && acceptTrailingNull))
			return false;
	
	Bool hasTrailingNull = acceptTrailingNull && str.length && str.ptr[str.length - 1] == '\0';

	//Validation to make sure we're not using weird legacy MS DOS keywords
	//Because these will not be writable correctly!

	if (str.length == 3 + hasTrailingNull ) {

		if(
			String_startsWithString(str, String_createConstRefUnsafe("CON"), EStringCase_Insensitive) ||
			String_startsWithString(str, String_createConstRefUnsafe("AUX"), EStringCase_Insensitive) ||
			String_startsWithString(str, String_createConstRefUnsafe("NUL"), EStringCase_Insensitive) ||
			String_startsWithString(str, String_createConstRefUnsafe("PRN"), EStringCase_Insensitive)
		)
			return false;
	}

	else if (str.length == 4 + hasTrailingNull) {

		if(
			(
				String_startsWithString(str, String_createConstRefUnsafe("COM"), EStringCase_Insensitive) ||
				String_startsWithString(str, String_createConstRefUnsafe("LPT"), EStringCase_Insensitive)
			) &&
			C8_isDec(str.ptr[3])
		)
			return false;
	}

	//Can't end with trailing . (so . and .. are illegal)

	if (str.length > hasTrailingNull && str.ptr[str.length - 1 - hasTrailingNull] == '.')
		return false;

	//If string is not empty then it's a valid string

	return str.length;
}

Bool String_isValidFilePath(String str) {

	//Don't allow ending with .

	if(str.length && (
		str.ptr[str.length - 1] == '.' || 
		(str.length >= 2 && str.ptr[str.length - 2] == '.'&& str.ptr[str.length - 1] == '\0')
	))
		return false;

	for(U64 i = 0; i < str.length; ++i)
		if(
			!C8_isValidFileName(str.ptr[i]) && str.ptr[i] != '/' && str.ptr[i] != '\\' && 
			!(i == str.length - 1 && !str.ptr[i]) &&						//Allow null terminator
			!(i == 1 && str.ptr[i] == ':')									//Allow drive names for Windows
		)
			return false;

	return str.length;
}

void String_clear(String *str) { 
	if(str) 
		str->length = 0; 
}

C8 String_getAt(String str, U64 i) { 
	return i < str.length ? str.ptr[i] : C8_MAX;
}

Bool String_setAt(String str, U64 i, C8 c) { 

	if(i >= str.length || String_isConstRef(str))
		return false;

	str.ptr[i] = c;
	return true;
}

String String_createNull() { return (String) { 0 }; }

String String_createConstRefAuto(const C8 *ptr, U64 maxSize) { 
	return (String) { 
		.length = String_calcStrLen(ptr, maxSize),
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

String String_createConstRefUnsafe(const C8 *ptr) {
	return String_createConstRefAuto(ptr, U64_MAX);
}

String String_createRefAuto(C8 *ptr, U64 maxSize) { 
	return (String) { 
		.length = String_calcStrLen(ptr, maxSize),
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

String String_createConstRefSized(const C8 *ptr, U64 size) {
	return (String) { 
		.length = size,
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

String String_createRefSized(C8 *ptr, U64 size) {
	return (String) { 
		.length = size,
		.ptr = ptr,
		.capacity = 0			//Flag as dynamic ref
	};
}

String String_createConstRefShortString(const ShortString str) {
	return (String) {
		.length = String_calcStrLen(str, _SHORTSTRING_LEN - 1),
		.ptr = (C8*) str,
		.capacity = U64_MAX		//Flag as const
	};
}

String String_createConstRefLongString(const LongString str) {
	return (String) {
		.length = String_calcStrLen(str, _LONGSTRING_LEN - 1),
		.ptr = (C8*) str,
		.capacity = U64_MAX		//Flag as const
	};
}

String String_createRefShortString(ShortString str) {
	return (String) {
		.length = String_calcStrLen(str, _SHORTSTRING_LEN - 1),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

String String_createRefLongString(LongString str) {
	return (String) {
		.length = String_calcStrLen(str, _LONGSTRING_LEN - 1),
		.ptr = str,
		.capacity = 0			//Flag as dynamic
	};
}

//Simple checks (consts)

Error String_offsetAsRef(String s, U64 off, String *result) {
	
	if (!result)
		return Error_nullPointer(2, 0);

	if (String_isEmpty(s)) {
		*result = String_createNull();
		return Error_none();
	}

	if(off >= s.length)
		return Error_outOfBounds(1, 0, off, s.length);

	*result = (String) {
		.ptr = s.ptr + off,
		.length = s.length - off,
		.capacity = String_isConstRef(s) ? U64_MAX : 0
	};

	return Error_none();
}

#define String_matchesPattern(testFunc, ...)		\
													\
	for(U64 i = (__VA_ARGS__); i < s.length; ++i) {	\
													\
		C8 c = s.ptr[i];							\
													\
		if(!testFunc(c))							\
			return false;							\
	}												\
													\
	return s.length;

#define String_matchesPatternNum(testFunc, num) String_matchesPattern(		\
	testFunc,																\
	String_startsWithString(												\
		s,																	\
		String_createConstRefUnsafe(num),									\
		EStringCase_Insensitive												\
	) ? String_createConstRefUnsafe(num).length : 0							\
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

	if(!s.length || !s.ptr)
		return false;

	//Validate sign

	U64 i = 0;

	if (s.ptr[0] == '-' || s.ptr[0] == '+') {

		if (++i >= s.length)
			return false;
	}

	//Validate first int 

	for(; i < s.length; ++i)

		if (s.ptr[i] == '.' || s.ptr[i] == 'e' || s.ptr[i] == 'E')
			break;

		else if(!C8_isDec(s.ptr[i]))
			return false;

	if (i == s.length)		//It's just an int
		return true;

	//Validate fraction

	if (s.ptr[i] == '.') {

		if (++i == s.length)		//It's just an int
			return true;

		//Check for int until e/E

		for(; i < s.length; ++i)

			if (s.ptr[i] == 'e' || s.ptr[i] == 'E')
				break;

			else if(!C8_isDec(s.ptr[i]))
				return false;
	}

	if (i == s.length)		//It's just [-+]?[0-9]*[.]?[0-9]*
		return true;

	//Validate exponent

	if (s.ptr[i] == 'E' || s.ptr[i] == 'e') {

		if (++i == s.length)
			return false;

		//e-NNN, e+NNN

		if (s.ptr[i] == '-' || s.ptr[i] == '+') {

			if (++i == s.length)
				return false;
		}

		for(; i < s.length; ++i)
			if(!C8_isDec(s.ptr[i]))
				return false;
	}

	return true;
}

Error String_create(C8 c, U64 size, Allocator alloc, String *result) {

	if (!alloc.alloc)
		return Error_nullPointer(2, 0);

	if (!result)
		return Error_nullPointer(3, 0);

	if (result->ptr)
		return Error_invalidOperation(0);

	if (!size) {
		*result = String_createNull();
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

	if (size & 4) {
		*(U32*)(b.ptr + (totalSize >> 3 << 3)) = cc4;
		size &= 3;
	}

	if (size & 2) {
		*(U16*)(b.ptr + (totalSize >> 2 << 2)) = cc2;
		size &= 1;
	}

	if(size & 1)
		b.ptr[totalSize - 1] = c;

	*result = (String) { .length = totalSize, .capacity = totalSize, .ptr = (C8*) b.ptr };
	return Error_none();
}

Error String_createCopy(String str, Allocator alloc, String *result) {

	if (!alloc.alloc || !result)
		return Error_nullPointer(!result ? 2 : 1, 0);

	if (result->ptr)
		return Error_invalidOperation(0);

	if (!str.length) {
		*result = String_createNull();
		return Error_none();
	}

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, str.length, &b);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < str.length >> 3; ++i)
		*((U64*)b.ptr + i) = *((const U64*)str.ptr + i);

	for (U64 i = str.length >> 3 << 3; i < str.length; ++i)
		b.ptr[i] = str.ptr[i];

	*result = (String) { .ptr = (C8*) b.ptr, .length = str.length, .capacity = str.length };
	return Error_none();
}

Bool String_free(String *str, Allocator alloc) {

	if (!str)
		return true;

	if(!alloc.free)
		return false;

	Bool freed = true;

	if(!String_isRef(*str))
		freed = alloc.free(alloc.ptr, Buffer_createManagedPtr(str->ptr, str->capacity));

	*str = String_createNull();
	return freed;
}

#define String_createNum(maxVal, func, prefixRaw, ...)					\
																		\
	String prefix = String_createConstRefUnsafe(prefixRaw);				\
																		\
	if (result->ptr)													\
		return Error_invalidOperation(0);								\
																		\
	*result = String_createNull();										\
	Error err = String_reserve(											\
		result, maxVal + prefix.length, allocator						\
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
	if (!result->length) {												\
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
	String_createNum(20, Dec, "", (v / U64_10pow(i)) % 10);
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

	U64 length = String_countAll(s, c, casing);

	Error err = StringList_create(length + 1, allocator, result);

	if (err.genericError)
		return err;

	if (!length) {

		result->ptr[0] = String_isConstRef(s) ? String_createConstRefSized(s.ptr, s.length) :
			String_createRefSized(s.ptr, s.length);

		return Error_none();
	}

	StringList str = *result;

	c = C8_transform(c, (EStringTransform) casing);

	U64 count = 0, last = 0;

	for (U64 i = 0; i < s.length; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform) casing) == c) {

			str.ptr[count++] = 
				String_isConstRef(s) ? String_createConstRefSized(s.ptr + last, i - last) : 
				String_createRefSized(s.ptr + last, i - last);

			last = i + 1;
		}

	str.ptr[count++] = 
		String_isConstRef(s) ? String_createConstRefSized(s.ptr + last, s.length - last) : 
		String_createRefSized(s.ptr + last, s.length - last);

	return Error_none();
}

Error String_splitString(
	String s, 
	String other, 
	EStringCase casing, 
	Allocator allocator, 
	StringList *result
) {

	U64 length = String_countAllString(s, other, casing);

	Error err = StringList_create(length ? length : 1, allocator, result);

	if (err.genericError)
		return err;

	if (!length) {

		result->ptr[0] = String_isConstRef(s) ? String_createConstRefSized(s.ptr, s.length) :
			String_createRefSized(s.ptr, s.length);

		return Error_none();
	}

	StringList str = *result;

	U64 count = 0, last = 0;

	for (U64 i = 0; i < s.length; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.length && k < other.length; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {

			str.ptr[count++] = 
				String_isConstRef(s) ? String_createConstRefSized(s.ptr + last, i - last) : 
				String_createRefSized(s.ptr + last, i - last);

			last = i + other.length;
			i += other.length - 1;
		}
	}

	return Error_none();
}

Error String_splitLine(String s, Allocator alloc, StringList *result) {

	if(!result)
		return Error_nullPointer(2, 0);

	if(result->ptr)
		return Error_invalidParameter(2, 1, 0);

	U64 v = 0, lastLineEnd = U64_MAX;

	for(U64 i = 0; i < s.length; ++i)

		if (s.ptr[i] == '\n') {			//Unix line endings
			++v;
			lastLineEnd = i;
			continue;
		}

		else if (s.ptr[i] == '\r') {	//Windows/Legacy Mac line endings

			if(i + 1 < s.length && s.ptr[i + 1] == '\n')	//Windows
				++i;

			++v;
			lastLineEnd = i;
			continue;
		}

	++v;

	Error err = StringList_create(v, alloc, result);

	if (err.genericError)
		return err;

	v = 0;
	lastLineEnd = 0;

	for(U64 i = 0; i < s.length; ++i) {

		Bool isLineEnd = false;

		U64 iOld = i;

		if (s.ptr[i] == '\n')			//Unix line endings
			isLineEnd = true;

		else if (s.ptr[i] == '\r') {	//Windows/Legacy Mac line endings

			if(i + 1 < s.length && s.ptr[i + 1] == '\n')	//Windows
				++i;

			isLineEnd = true;
		}

		if(!isLineEnd)
			continue;

		result->ptr[v++] = String_isConstRef(s) ? 
			String_createConstRefSized(s.ptr + lastLineEnd, iOld - lastLineEnd) : 
			String_createRefSized(s.ptr + lastLineEnd, iOld - lastLineEnd);

		lastLineEnd = i + 1;
	}

	if(lastLineEnd != s.length)
		result->ptr[v++] = String_isConstRef(s) ? 
			String_createConstRefSized(s.ptr + lastLineEnd, s.length - lastLineEnd) : 
			String_createRefSized(s.ptr + lastLineEnd, s.length - lastLineEnd);

	return Error_none();
}

Error String_reserve(String *str, U64 length, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0, 0);

	if (String_isRef(*str) && str->length)
		return Error_invalidOperation(0);

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(2, 0);

	if (length <= str->capacity)
		return Error_none();

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, length, &b);

	if (err.genericError)
		return err;

	Buffer_copy(b, String_bufferConst(*str));

	err = 
		alloc.free(alloc.ptr, Buffer_createManagedPtr(str->ptr, str->capacity)) ? 
		Error_none() : Error_invalidOperation(0);

	str->capacity = Buffer_length(b);
	str->ptr = (C8*) b.ptr;
	return err;
}

Error String_resize(String *str, U64 length, C8 defaultChar, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0, 0);

	if (String_isRef(*str) && str->length)
		return Error_invalidOperation(0);

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(3, 0);

	if (length == str->length)
		return Error_none();

	//Simple resize; we cut off the tail

	if (length < str->length) {
		str->length = length;
		return Error_none();
	}

	//Resize that triggers buffer resize

	if (length > str->capacity) {

		if (length * 3 / 3 != length)
			return Error_overflow(1, 0, length * 3, U64_MAX);

		//Reserve 50% more to ensure we don't resize too many times

		Error err = String_reserve(str, length * 3 / 2, alloc);

		if (err.genericError)
			return err;
	}

	//Our capacity is big enough to handle it:

	for (U64 i = str->length; i < length; ++i)
		str->ptr[i] = defaultChar;

	str->length = length;
	return Error_none();
}

Error String_append(String *s, C8 c, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, 0);

	return String_resize(s, s->length + 1, c, allocator);
}

#if _WIN32
	String String_newLine() { return String_createConstRefUnsafe("\r\n"); }
#else
	String String_newLine() { return String_createConstRefUnsafe("\n"); }
#endif

Error String_appendString(String *s, String other, Allocator allocator) {

	if (!other.length || !other.ptr)
		return Error_none();

	if (!s)
		return Error_nullPointer(0, 0);

	U64 oldLen = s->length;
	Error err = String_resize(s, s->length + other.length, other.ptr[0], allocator);

	if (err.genericError)
		return err;

	Buffer_copy(Buffer_createRef(s->ptr + oldLen, other.length), String_bufferConst(other));
	return Error_none();
}

Error String_insert(String *s, C8 c, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, 0);

	if(i > s->length)
		return Error_outOfBounds(2, 0, i, s->length);

	Error err = String_resize(s, s->length + 1, c, allocator);

	if (err.genericError)
		return err;

	//If it's not append (otherwise it's already handled)

	if (i != s->length - 1) {

		//Move one to the right

		Buffer_revCopy(
			Buffer_createRef(s->ptr + i + 1, s->length - 1),
			Buffer_createConstRef(s->ptr + i, s->length - 1)
		);

		s->ptr[i] = c;
	}

	return Error_none();
}

Error String_insertString(String *s, String other, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, 0);

	if (!other.length)
		return Error_none();

	U64 oldLen = s->length;
	Error err = String_resize(s, s->length + other.length, ' ', allocator);

	if (err.genericError)
		return err;

	//Move one to the right

	Buffer_revCopy(
		Buffer_createRef(s->ptr + i + other.length, oldLen - i),
		Buffer_createConstRef(s->ptr + i, oldLen - i)
	);

	Buffer_copy(
		Buffer_createRef(s->ptr + i, other.length),
		String_bufferConst(other)
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
		return Error_nullPointer(0, 0);

	if(String_isRef(*s))
		return Error_invalidOperation(0);

	List finds = List_createEmpty(sizeof(U64));
	Error err = String_findAllString(*s, search, allocator, caseSensitive, &finds);

	if(err.genericError)
		return err;

	if (!finds.length)
		return Error_none();

	//Easy replace

	const U64 *ptr = (const U64*) finds.ptr;

	if (search.length == replace.length) {

		for (U64 i = 0; i < finds.length; ++i)
			for (U64 j = ptr[i], k = j + replace.length, l = 0; j < k; ++j, ++l)
				s->ptr[j] = replace.ptr[l];

		goto clean;
	}

	//Shrink replaces

	if (search.length > replace.length) {

		U64 diff = search.length - replace.length;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			//We move our replacement string to iCurrent

			Buffer_copy(Buffer_createRef(s->ptr + iCurrent, replace.length), String_bufferConst(replace));
			iCurrent += replace.length;

			//We then move the tail of the string 

			const U64 iStart = ptr[i] + search.length;
			const U64 iEnd = i == finds.length - 1 ? s->length : ptr[i + 1];

			Buffer_copy(
				Buffer_createRef(s->ptr + iCurrent, iEnd - iStart), 
				Buffer_createConstRef(s->ptr + iStart, iEnd - iStart)
			);

			iCurrent += iEnd - iStart;
		}

		//Ensure the string is now the right size

		err = String_resize(s, s->length - diff * finds.length, ' ', allocator);
		goto clean;
	}

	//Grow replaces

	//Ensure the string is now the right size

	U64 diff = replace.length - search.length;
	U64 sOldLen = s->length;

	_gotoIfError(clean, String_resize(s, s->length + diff * finds.length, ' ', allocator));

	//Move from right to left

	U64 newLoc = s->length - 1;

	for (U64 i = finds.length - 1; i != U64_MAX; ++i) {

		//Find tail

		const U64 iStart = ptr[i] + search.length;
		const U64 iEnd = i == finds.length - 1 ? sOldLen : ptr[i + 1];

		for (U64 j = iEnd - 1; j != U64_MAX && j >= iStart; --j)
			s->ptr[newLoc--] = s->ptr[j];

		Buffer_revCopy(
			Buffer_createRef(s->ptr + newLoc - (iEnd - iStart), iEnd - iStart),
			Buffer_createConstRef(s->ptr + iStart, iEnd - iStart)
		);

		newLoc -= iEnd - iStart;

		//Apply replacement before tail

		Buffer_revCopy(
			Buffer_createRef(s->ptr + newLoc - replace.length, replace.length), 
			String_bufferConst(replace)
		);

		newLoc -= replace.length;
	}

clean:
	List_free(&finds, allocator);
	return err;
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
		return Error_nullPointer(0, 0);

	if(String_isRef(*s))
		return Error_invalidOperation(0);

	U64 res = isFirst ? String_findFirstString(*s, search, caseSensitive) : 
		String_findLastString(*s, search, caseSensitive);

	if (res == s->length)
		return Error_none();

	//Easy, early exit. Strings are same size.

	if (search.length == replace.length) {
		Buffer_copy(Buffer_createRef(s->ptr + res, replace.length), String_bufferConst(replace));
		return Error_none();
	}

	//Replacement is smaller than our search
	//So we can just move from left to right
	
	if (replace.length < search.length) {

		U64 diff = search.length - replace.length;	//How much we have to shrink

		//Copy our data over first

		Buffer_copy(
			Buffer_createRef(s->ptr + res + replace.length, s->length - (res + search.length)), 
			Buffer_createConstRef(s->ptr + res + search.length, s->length - (res + search.length)) 
		);

		//Throw our replacement in there

		Buffer_copy(Buffer_createRef(s->ptr + res, replace.length), String_bufferConst(replace));

		//Shrink the string; this is done after because we need to read the right of the string first

		Error err = String_resize(s, s->length - diff, ' ', allocator);

		if (err.genericError)
			return err;

		return Error_none();
	}

	//Replacement is bigger than our search;
	//We need to grow first and move from right to left

	U64 diff = replace.length - search.length;

	Error err = String_resize(s, s->length + diff, ' ', allocator);

	if (err.genericError)
		return err;

	//Copy our data over first

	Buffer_revCopy(
		Buffer_createRef(s->ptr + res + replace.length, s->length - (res + search.length)), 
		Buffer_createConstRef(s->ptr + res + search.length, s->length - (res + search.length)) 
	);

	//Throw our replacement in there

	Buffer_copy(Buffer_createRef(s->ptr + res, replace.length), String_bufferConst(replace));
	return Error_none();
}

Error String_replaceFirstString(
	String *s,
	String search,
	String replace,
	EStringCase caseSensitive,
	Allocator allocator
) {
	return String_replaceString(s, search, replace, caseSensitive, allocator, true);
}

Error String_replaceLastString(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Allocator allocator
) {
	return String_replaceString(s, search, replace, caseSensitive, allocator, false);
}

Bool String_startsWith(String str, C8 c, EStringCase caseSensitive) { 
	return 
		str.length && str.ptr && 
		C8_transform(*str.ptr, (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_endsWith(String str, C8 c, EStringCase caseSensitive) {
	return 
		str.length && str.ptr && 
		C8_transform(str.ptr[str.length - 1], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_startsWithString(String str, String other, EStringCase caseSensitive) {

	if (other.length > str.length)
		return false;

	for (U64 i = 0; i < other.length; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_endsWithString(String str, String other, EStringCase caseSensitive) {

	if (other.length > str.length)
		return false;

	for (U64 i = str.length - other.length; i < str.length; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i - (str.length - other.length)], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

U64 String_countAll(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	U64 count = 0;

	for (U64 i = 0; i < s.length; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			++count;

	return count;
}

U64 String_countAllString(String s, String other, EStringCase casing) {

	if(!other.length || s.length < other.length)
		return 0;

	U64 j = 0;

	for (U64 i = 0; i < s.length; ++i) {

		Bool match = true;

		for (U64 l = i, k = 0; l < s.length && k < other.length; ++l, ++k)
			if (
				C8_transform(s.ptr[l], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {
			i += s.length - 1;
			++j;
		}
	}

	return j;
}

Error String_findAll(
	String s,
	C8 c,
	Allocator alloc,
	EStringCase caseSensitive,
	List *result
) {

	if(!result)
		return Error_nullPointer(4, 0);

	if(result->ptr)
		return Error_invalidParameter(4, 0, 0);

	List l = List_createEmpty(sizeof(U64));
	Error err = List_reserve(&l, s.length / 25 + 16, alloc);

	if (err.genericError)
		return err;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	for (U64 i = 0; i < s.length; ++i)
		if (c == C8_transform(s.ptr[i], (EStringTransform)caseSensitive))
			if ((err = List_pushBack(&l, Buffer_createConstRef(&i, sizeof(i)), alloc)).genericError) {
				List_free(&l, alloc);
				return err;
			}

	*result = l;
	return Error_none();
}

Error String_findAllString(
	String s,
	String other,
	Allocator alloc,
	EStringCase casing,
	List *result
) {

	if(!result)
		return Error_nullPointer(4, 0);

	if(result->ptr)
		return Error_invalidParameter(4, 0, 0);

	if(!other.length)
		return Error_invalidParameter(1, 0, 0);

	List l = List_createEmpty(sizeof(U64));

	if(s.length < other.length) {
		*result = l;
		return Error_none();
	}

	Error err = List_reserve(&l, s.length / other.length / 25 + 16, alloc);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < s.length; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.length && k < other.length; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {

			if ((err = List_pushBack(&l, Buffer_createConstRef(&i, sizeof(i)), alloc)).genericError) {
				List_free(&l, alloc);
				return err;
			}

			i += s.length - 1;
		}
	}

	*result = l;
	return Error_none();
}

U64 String_findFirst(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = 0; i < s.length; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return s.length;
}

U64 String_findLast(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = s.length - 1; i != U64_MAX; --i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	//TODO: Don't return length on end!

	return s.length;
}

U64 String_findFirstString(String s, String other, EStringCase casing) {

	if(!other.length || s.length < other.length)
		return s.length;

	U64 i = 0;

	for (; i < s.length; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s.length && k < other.length; ++j, ++k)
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
	
	if(!other.length || s.length < other.length)
		return 0;

	U64 i = s.length - 1;

	for (; i != U64_MAX; --i) {

		Bool match = true;

		for (U64 j = i, k = other.length - 1; j != U64_MAX && k != U64_MAX; --j, --k)
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

Bool String_equalsString(String s, String other, EStringCase caseSensitive, Bool ignoreNull) {

	if (ignoreNull && s.length && !s.ptr[s.length - 1])
		--s.length;

	if (ignoreNull && other.length && !other.ptr[other.length - 1])
		--other.length;

	if (s.length != other.length)
		return false;

	for (U64 i = 0; i < s.length; ++i)
		if (
			C8_transform(s.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_equals(String s, C8 c, EStringCase caseSensitive, Bool ignoreNull) {

	if (ignoreNull && s.length && !s.ptr[s.length - 1])
		--s.length;

	return s.length == 1 && s.ptr && 
		C8_transform(s.ptr[0], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_parseNyto(String s, U64 *result){

	String prefix = String_createConstRefUnsafe("0n");

	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.length : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.length > 11)
		return false;

	*result = 0;

	for (U64 i = s.length - 1, j = 1; i != U64_MAX; --i, j <<= 6) {

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

	String prefix = String_createConstRefUnsafe("0x");
	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.length : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.length > 16)
		return false;

	*result = 0;

	for (U64 i = s.length - 1, j = 0; i != U64_MAX; --i, j += 4) {

		U8 v = C8_hex(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		*result |= (U64)v << j;
	}

	return true;
}

Bool String_parseDec(String s, U64 *result) {

	if (!result || !s.ptr || s.length > 20)
		return false;

	*result = 0;

	for (U64 i = s.length - 1, j = 1; i != U64_MAX; --i, j *= 10) {

		U8 v = C8_dec(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == (U64) 1e19 && v > 1)		//Out of value
			return false;

		*result += j * v;
	}

	return true;
}

Bool String_parseDecSigned(String s, I64 *result) {

	Bool neg = String_startsWith(s, '-', EStringCase_Sensitive);

	Error err = String_offsetAsRef(s, neg, &s);

	if (err.genericError)
		return false;

	Bool b = String_parseDec(s, (U64*) result);

	if(!b)
		return false;

	if (neg && *(U64*)result == (U64)I64_MIN) {		//int min is 1 "higher" than I64_MAX
		*result = I64_MIN;
		return true;
	}

	if (*(U64*)result > (U64) I64_MAX)				//Guard against int overflow
		return false;

	if(neg)
		*result *= -1;

	return true;
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

		if (!s.length) {
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

		if(!F32_isValid(fraction))
			return false;

		multiplier *= 0.1f;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;

		if (!s.length) {
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

	while(s.length) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		exponent = exponent * 10 + v;

		if(!F32_isValid(fraction))
			return false;

		if (String_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	Error err = F32_exp10(esign ? -exponent : exponent, &exponent);

	if(err.genericError)
		return false;

	F32 res = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;

	if(!F32_isValid(res))
		return false;

	*result = res;
	return true;
}

Bool String_parseOct(String s, U64 *result) {

	String prefix = String_createConstRefUnsafe("0o");
	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.length : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.length > 22)
		return false;

	*result = 0;

	for (U64 i = s.length - 1, j = 0; i != U64_MAX; --i, j += 3) {

		U8 v = C8_oct(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64)1 << (21 * 3)) && v > 1)		//Out of value
			return false;

		*result |= (U64)v << j;
	}

	return true;
}

Bool String_parseBin(String s, U64 *result) {

	String prefix = String_createConstRefUnsafe("0b");
	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? prefix.length : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || s.length > 64)
		return false;

	*result = 0;

	for (U64 i = s.length - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

		U8 v = C8_bin(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(v)
			*result |= j;
	}

	return true;
}

Bool String_cut(String s, U64 offset, U64 length, String *result) {

	if(!result || result->ptr)
		return false;

	if (!s.length && !offset && !length) {
		*result = String_createNull();
		return true;
	}

	if(offset >= s.length)
		return false;

	if(!length) 
		length = s.length - offset;

	if (offset + length > s.length)
		return false;

	if (offset == s.length) {
		*result = String_createNull();
		return false;
	}

	*result = String_isConstRef(s) ? String_createConstRefSized(s.ptr + offset, length) :
		String_createRefSized(s.ptr + offset, length);

	return true;
}

Bool String_cutAfter(
	String s, 
	C8 c, 
	EStringCase caseSensitive, 
	Bool isFirst, 
	String *result
) {

	U64 found = isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);

	if (found == s.length)
		return false;

	return String_cut(s, 0, found, result);
}

Bool String_cutAfterString(
	String s, 
	String other, 
	EStringCase caseSensitive, 
	Bool isFirst, 
	String *result
) {

	U64 found = isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);

	if (found == s.length)
		return false;

	return String_cut(s, 0, found, result);
}

Bool String_cutBefore(String s, C8 c, EStringCase caseSensitive, Bool isFirst, String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);

	if (found == s.length)
		return false;

	++found;	//The end of the occurence is the begin of the next string
	return String_cut(s, found, 0, result);
}

Bool String_cutBeforeString(String s, String other, EStringCase caseSensitive, Bool isFirst, String *result) {

	U64 found = isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);

	if (found == s.length)
		return false;

	found += other.length;	//The end of the occurence is the begin of the next string
	return String_cut(s, found, 0, result);
}

Bool String_erase(String *s, C8 c, EStringCase caseSensitive, Bool isFirst) {

	if(!s || String_isRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	Bool hadMatch = false;

	//Skipping first match

	U64 find = String_find(*s, c, caseSensitive, isFirst);

	if(find == s->length)
		return false;

	Buffer_copy(
		Buffer_createRef(s->ptr + find, s->length - find - 1),
		Buffer_createConstRef(s->ptr + find + 1, s->length - find - 1)
	);

	if (hadMatch)
		--s->length;

	return true;
}

Error String_eraseAtCount(String *s, U64 i, U64 count) {

	if(!s)
		return Error_nullPointer(0, 0);

	if(!count)
		return Error_invalidParameter(2, 0, 0);

	if(String_isRef(*s))
		return Error_constData(0, 0);

	if(i + count > s->length)
		return Error_outOfBounds(0, 0, i + count, s->length);

	Buffer_copy(
		Buffer_createRef(s->ptr + i, s->length - i - count),
		Buffer_createConstRef(s->ptr + i + count, s->length - i - count)
	);

	s->length -= count;
	return Error_none();
}

Bool String_eraseString(String *s, String other, EStringCase casing, Bool isFirst) {

	if(!s || !other.length || String_isRef(*s))
		return false;

	//Skipping first match

	U64 find = String_findString(*s, other, casing, isFirst);

	if(find == s->length)
		return false;

	Buffer_copy(
		Buffer_createRef(s->ptr + find, s->length - find - other.length),
		Buffer_createConstRef(s->ptr + find + other.length, s->length - find - other.length)
	);

	s->length -= other.length;
	return true;
}

//String's inline changes (no copy)

Bool String_cutEnd(String s, U64 i, String *result) {
	return String_cut(s, 0, i, result);
}

Bool String_cutBegin(String s, U64 i, String *result) {

	if (i > s.length)
		return false;

	return String_cut(s, i, 0, result);
}

Bool String_eraseAll(String *s, C8 c, EStringCase casing) {

	if(!s || String_isRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform) casing);

	U64 out = 0;

	for (U64 i = 0; i < s->length; ++i) {

		if(C8_transform(s->ptr[i], (EStringTransform) casing) == c)
			continue;

		s->ptr[out++] = s->ptr[i];
	}
	
	if(out == s->length)
		return false;

	s->length = out;
	return true;
}

Bool String_eraseAllString(String *s, String other, EStringCase casing) {

	if(!s || !other.length || String_isRef(*s))
		return false;

	U64 out = 0;

	for (U64 i = 0; i < s->length; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < s->length && k < other.length; ++j, ++k)
			if (
				C8_transform(s->ptr[j], (EStringTransform) casing) != 
				C8_transform(other.ptr[k], (EStringTransform) casing)
			) {
				match = false;
				break;
			}

		if (match) {
			i += s->length - 1;	//-1 because we increment by 1 every time
			continue;
		}

		s->ptr[out++] = s->ptr[i];
	}

	if(out == s->length)
		return true;

	s->length = out;
	return true;
}

Bool String_replaceAll(String *s, C8 c, C8 v, EStringCase caseSensitive) {

	if (!s || String_isConstRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for(U64 i = 0; i < s->length; ++i)
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

	for(U64 i = 0; i < str.length; ++i)
		str.ptr[i] = C8_transform(str.ptr[i], c);

	return true;
}

String String_trim(String s) {

	U64 first = s.length;

	for (U64 i = 0; i < s.length; ++i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			first = i;
			break;
		}

	if (first == s.length)
		return String_createNull();

	U64 last = s.length;

	for (U64 i = s.length - 1; i != U64_MAX; --i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			last = i;
			break;
		}

	++last;		//We wanna include the last character that's non whitespace too
	
	return String_isConstRef(s) ? String_createConstRefSized(s.ptr + first, last - first) :
		String_createRefSized(s.ptr + first, last - first);
}

//StringList

Error StringList_create(U64 length, Allocator alloc, StringList *arr) {

	if (!arr)
		return Error_nullPointer(2, 0);

	if (arr->ptr)
		return Error_invalidOperation(0);

	StringList sl = (StringList) { .length = length };

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, length * sizeof(String), &b);

	sl.ptr = (String*) b.ptr;

	if(err.genericError)
		return err;

	for(U64 i = 0; i < sl.length; ++i)
		sl.ptr[i] = String_createNull();

	*arr = sl;
	return Error_none();
}

Bool StringList_free(StringList *arr, Allocator alloc) {

	if(!arr || !arr->length)
		return true;

	Bool freed = true;

	for(U64 i = 0; i < arr->length; ++i) {
		String *str = arr->ptr + i;
		freed &= String_free(str, alloc);
	}

	freed &= alloc.free(alloc.ptr, Buffer_createManagedPtr(
		arr->ptr,
		sizeof(String) * arr->length
	));

	*arr = (StringList){ 0 };
	return freed;
}

Error StringList_createCopy(StringList toCopy, Allocator alloc, StringList *arr) {

	if(!toCopy.length)
		return Error_nullPointer(0, 0);

	Error err = StringList_create(toCopy.length, alloc, arr);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < toCopy.length; ++i) {

		err = String_createCopy(toCopy.ptr[i], alloc, arr->ptr + i);

		if (err.genericError) {
			StringList_free(arr, alloc);
			return err;
		}
	}

	return Error_none();
}

Error StringList_unset(StringList arr, U64 i, Allocator alloc) {

	if(i >= arr.length)
		return Error_outOfBounds(1, 0, i, arr.length);

	String *pstr = arr.ptr + i;
	String_free(pstr, alloc);
	return Error_none();
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

/*U64 String_hash(String s) {			//TODO: FNV for <10 and xxh64 for >

	U64 h = FNVHash_create();

	const U64 *ptr = (const U64*)s.ptr, *end = ptr + (s.length >> 3);

	for(; ptr < end; ++ptr)
		h = FNVHash_apply(h, *ptr);

	U64 last = 0, shift = 0;

	if (s.length & 4) {
		last = *(const U32*) ptr;
		shift = 4;
		ptr = (const U64*)((const U32*)ptr + 1);
	}

	if (s.length & 2) {
		last |= (U64)(*(const U16*)ptr) << shift;
		shift |= 2;
		ptr = (const U64*)((const U16*)ptr + 1);
	}

	if (s.length & 1)
		last |= (U64)(*(const U8*)ptr) << shift++;

	if(shift)
		h = FNVHash_apply(h, last);

	return h;
}*/

String String_getFilePath(String *str) {

	if(!String_formatPath(str))
		return String_createNull();

	String res = String_createNull();
	String_cutBefore(*str, '/', EStringCase_Sensitive, false, &res);
	return res;
}

String String_getBasePath(String *str) {

	if(!String_formatPath(str))
		return String_createNull();

	String res = String_createNull();
	String_cutAfter(*str, '/', EStringCase_Sensitive, false, &res);
	return res;
}

Error StringList_concat(StringList arr, C8 between, Allocator alloc, String *result) {

	U64 length = 0;

	for(U64 i = 0; i < arr.length; ++i)
		length += arr.ptr[i].length;

	if(arr.length > 1)
		length += arr.length - 1;

	Error err = String_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0; k < arr.ptr[i].length; ++k, ++j)
			result->ptr[j] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			result->ptr[j++] = between;
	}

	return Error_none();
}

Error StringList_concatString(StringList arr, String between, Allocator alloc, String *result) {

	U64 length = 0;

	for(U64 i = 0; i < arr.length; ++i)
		length += arr.ptr[i].length;

	if(arr.length > 1)
		length += (arr.length - 1) * between.length;

	Error err = String_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0; k < arr.ptr[i].length; ++k, ++j)
			result->ptr[j] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			for(U64 k = 0; k < between.length; ++k, ++j)
				result->ptr[j] = between.ptr[k];
	}

	return Error_none();
}

Error StringList_combine(StringList arr, Allocator alloc, String *result) {
	return StringList_concatString(arr, String_createNull(), alloc, result);
}

U64 String_find(String s, C8 c, EStringCase caseSensitive, Bool isFirst) {
	return isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);
}

U64 String_findString(String s, String other, EStringCase caseSensitive, Bool isFirst) {
	return isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);
}

Bool String_contains(String str, C8 c, EStringCase caseSensitive) { 
	return String_findFirst(str, c, caseSensitive) != U64_MAX;
}

Bool String_containsString(String str, String other, EStringCase caseSensitive) { 
	return String_findFirstString(str, other, caseSensitive) != U64_MAX; 
}

Bool String_cutAfterLast(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutAfter(s, c, caseSensitive, false, result);
}

Bool String_cutAfterFirst(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutAfter(s, c, caseSensitive, true, result);
}

Bool String_cutAfterLastString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutAfterString(s, other, caseSensitive, false, result);
}

Bool String_cutAfterFirstString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutAfterString(s, other, caseSensitive, true, result);
}

Bool String_cutBeforeLast(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutBefore(s, c, caseSensitive, false, result);
}

Bool String_cutBeforeFirst(String s, C8 c, EStringCase caseSensitive, String *result) {
	return String_cutBefore(s, c, caseSensitive, true, result);
}

Bool String_cutBeforeLastString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutBeforeString(s, other, caseSensitive, false, result);
}

Bool String_cutBeforeFirstString(String s, String other, EStringCase caseSensitive, String *result) {
	return String_cutBeforeString(s, other, caseSensitive, true, result);
}

Error String_popFrontCount(String *s, U64 count) { return String_eraseAtCount(s, 0, count); }
Error String_popEndCount(String *s, U64 count) { return String_eraseAtCount(s, s ? s->length : 0, count); }

Error String_eraseAt(String *s, U64 i) { return String_eraseAtCount(s, i, 1); }
Error String_popFront(String *s) { return String_eraseAt(s, 0); }
Error String_popEnd(String *s) { return String_eraseAt(s, s ? s->length : 0); }

Bool String_eraseFirst(String *s, C8 c, EStringCase caseSensitive) {
	return String_erase(s, c, caseSensitive, true);
}

Bool String_eraseLast(String *s, C8 c, EStringCase caseSensitive) {
	return String_erase(s, c, caseSensitive, false);
}

Bool String_eraseFirstString(String *s, String other, EStringCase caseSensitive){
	return String_eraseString(s, other, caseSensitive, true);
}

Bool String_eraseLastString(String *s, String other, EStringCase caseSensitive) {
	return String_eraseString(s, other, caseSensitive, false);
}

Bool String_replaceFirst(String *s, C8 c, C8 v, EStringCase caseSensitive) {
	return String_replace(s, c, v, caseSensitive, true);
}

Bool String_replaceLast(String *s, C8 c, C8 v, EStringCase caseSensitive) {
	return String_replace(s, c, v, caseSensitive, false);
}

Bool String_toLower(String str) { return String_transform(str, EStringTransform_Lower); }
Bool String_toUpper(String str) { return String_transform(str, EStringTransform_Upper); }

//Simple file utils

Bool String_formatPath(String *str) { 
	return String_replaceAll(str, '\\', '/', EStringCase_Insensitive);
}
