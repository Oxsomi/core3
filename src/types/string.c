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
#include <stdio.h>

Bool String_isConstRef(String str) { return str.capacity == U64_MAX; }
Bool String_isRef(String str) { return !str.capacity || String_isConstRef(str); }
U64  String_bytes(String str) { return str.len << 1 >> 1; }
U64  String_length(String str) { return String_bytes(str); }
Bool String_isEmpty(String str) { return !String_bytes(str); }
Bool String_isNullTerminated(String str) { return str.len >> 63; }

Buffer String_buffer(String str) { 
	return String_isConstRef(str) ? Buffer_createNull() : Buffer_createRef((U8*)str.ptr, String_bytes(str)); 
}

Buffer String_bufferConst(String str) { 
	return Buffer_createConstRef(str.ptr, String_bytes(str));
}

//Iteration

C8 *String_begin(String str) { return String_isConstRef(str) ? NULL : (C8*)str.ptr; }
const C8 *String_beginConst(String str) { return str.ptr; }

C8 *String_end(String str) { return String_isConstRef(str) ? NULL : (C8*)str.ptr + String_length(str); }
const C8 *String_endConst(String str) { return str.ptr + String_length(str); }

C8 *String_charAt(String str, U64 off) { 
	return String_isConstRef(str) || off >= String_length(str) ? NULL : (C8*)str.ptr + off;
}

const C8 *String_charAtConst(String str, U64 off) { 
	return off >= String_length(str) ? NULL : str.ptr + off; 
}

Bool String_isValidAscii(String a) {

	for(U64 i = 0; i < String_length(a); ++i)
		if(!C8_isValidAscii(a.ptr[i]))
			return false;

	return true;
}

Bool String_isValidFileName(String str) {
	
	for(U64 i = 0; i < String_length(str); ++i)
		if(!C8_isValidFileName(str.ptr[i]))
			return false;
	
	//Validation to make sure we're not using weird legacy MS DOS keywords
	//Because these will not be writable correctly!

	U64 illegalStart = 0, strl = String_length(str);

	if (strl >= 3) {

		if(
			String_startsWithString(str, String_createConstRefUnsafe("CON"), EStringCase_Insensitive) ||
			String_startsWithString(str, String_createConstRefUnsafe("AUX"), EStringCase_Insensitive) ||
			String_startsWithString(str, String_createConstRefUnsafe("NUL"), EStringCase_Insensitive) ||
			String_startsWithString(str, String_createConstRefUnsafe("PRN"), EStringCase_Insensitive)
		)
			illegalStart = 3;

		else if (strl >= 4) {

			if(
				(
					String_startsWithString(str, String_createConstRefUnsafe("COM"), EStringCase_Insensitive) ||
					String_startsWithString(str, String_createConstRefUnsafe("LPT"), EStringCase_Insensitive)
				) &&
				C8_isDec(str.ptr[3])
			)
				illegalStart = 4;
		}
	}

	//PRNtscreen.pdf is valid, but PRN.pdf isn't.
	///NULlpointer.txt is valid, NUL.txt isn't.

	if(illegalStart && (strl == illegalStart || String_getAt(str, illegalStart) == '.'))
		return false;

	//Can't end with trailing . (so . and .. are illegal)

	if (strl && str.ptr[strl - 1] == '.')
		return false;

	//If string is not empty then it's a valid string

	return strl;
}

//We support valid file names or ., .. in file path parts.

Bool String_isSupportedInFilePath(String str) {
	return 
		String_isValidFileName(str) || 
		(String_getAt(str, 0) == '.' && (
			String_length(str) == 1 || (String_getAt(str, 1) == '.' && String_length(str) == 2)
		));
}

//File_resolve but without validating if it'd be a valid (permissioned) path on disk.

Bool String_isValidFilePath(String str) {

	//myTest/ <-- or myTest\ to myTest
		
	str = String_createConstRefSized(str.ptr, String_length(str), String_isNullTerminated(str));

	if(String_getAt(str, String_length(str) - 1) == '/' || String_getAt(str, String_length(str) - 1) == '\\')
		str.len = String_length(str) - 1;
		
	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)
	//We also obviously don't support 0:\ and such or A:/ on unix

	Bool hasPrefix = false;

	#ifdef _WIN32

		if(
			String_length(str) >= 3 && 
			str.ptr[1] == ':' && ((str.ptr[2] != '/' && str.ptr[2] != '\\') || !C8_isAlpha(str.ptr[0]))
		)
			return false;

		//Absolute

		if(str.ptr[1] == ':') {
			str.ptr += 3;
			str.len -= 3;
			hasPrefix = true;
		}

	#else

		if(String_length(str) >= 2 && str.ptr[1] == ':')
			return false;

	#endif

	//Virtual files

	if(String_getAt(str, 0) == '/' && String_getAt(str, 1) == '/') {

		if(hasPrefix)
			return false;

		str.ptr += 2;
		str.len -= 2;
	}

	//Absolute path

	if(String_getAt(str, 0) == '/' || String_getAt(str, 0) == '\\') {

		if(hasPrefix)
			return false;

		++str.ptr;
		--str.len;
		hasPrefix = true;
	}

	//Windows network paths, this is unsupported currently

	if(String_getAt(str, 0) == '\\' && String_getAt(str, 1) == '\\') 
		return false;

	//Split by / or \.

	U64 prev = 0;
	U64 strl = String_length(str);

	for (U64 i = 0; i < strl; ++i) {

		C8 c = str.ptr[i];

		//Push previous

		if (c == '/' || c == '\\') {

			if(!(i - prev))
				return false;

			String part = String_createConstRefSized(str.ptr + prev, i - prev, false);

			if(!String_isSupportedInFilePath(part))
				return false;

			prev = i + 1;
		}
	}

	//Validate ending

	String part = String_createConstRefSized(str.ptr + prev, strl - prev, String_isNullTerminated(str));

	if(!String_isSupportedInFilePath(part))
		return false;

	return strl;
}

Bool String_clear(String *str) { 

	if(!str || !String_isRef(*str) || !str->capacity) 
		return false;

	if(str->len >> 63)					//If null terminated, we want to keep it null terminated
		((C8*)str->ptr)[0] = '\0';

	str->len &= ~(((U64)1 << 63) - 1);		//Clear size
	return true;
}

C8 String_getAt(String str, U64 i) { 
	return i < String_length(str) ? str.ptr[i] : C8_MAX;
}

Bool String_setAt(String str, U64 i, C8 c) { 

	if(i >= String_length(str) || String_isConstRef(str) || !c)
		return false;

	((C8*)str.ptr)[i] = c;
	return true;
}

String String_createNull() { return (String) { 0 }; }

String String_createConstRefAuto(const C8 *ptr, U64 maxSize) { 

	if(!ptr)
		return String_createNull();

	U64 strl = String_calcStrLen(ptr, maxSize);

	return (String) { 
		.len = strl | ((U64)(strl != maxSize) << 63),
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

String String_createConstRefUnsafe(const C8 *ptr) {
	return String_createConstRefAuto(ptr, U64_MAX);
}

String String_createRefAuto(C8 *ptr, U64 maxSize) { 
	String str = String_createConstRefAuto(ptr, maxSize);
	str.capacity = 0;	//Flag as dynamic
	return str;
}

String String_createConstRefSized(const C8 *ptr, U64 size, Bool isNullTerminated) {

	if(!ptr || (size >> 48))
		return String_createNull();

	if(isNullTerminated && ptr[size])	//Invalid!
		return String_createNull();

	if(!isNullTerminated && size) {

		isNullTerminated = !ptr[size - 1];

		if(isNullTerminated)
			--size;
	}

	return (String) { 
		.len = size | ((U64)isNullTerminated << 63),
		.ptr = (C8*) ptr,
		.capacity = U64_MAX		//Flag as const
	};
}

String String_createRefSized(C8 *ptr, U64 size, Bool isNullTerminated) {
	String str = String_createConstRefSized(ptr, size, isNullTerminated);
	str.capacity = 0;	//Flag as dynamic
	return str;
}

String String_createConstRefShortString(const ShortString str) {
	return String_createConstRefAuto(str, _SHORTSTRING_LEN);
}

String String_createConstRefLongString(const LongString str) {
	return String_createConstRefAuto(str, _LONGSTRING_LEN);
}

String String_createRefShortString(ShortString str) {
	return String_createRefAuto(str, _SHORTSTRING_LEN);
}

String String_createRefLongString(LongString str) {
	return String_createRefAuto(str, _LONGSTRING_LEN);
}

//Simple checks (consts)

Error String_offsetAsRef(String s, U64 off, String *result) {
	
	if (!result)
		return Error_nullPointer(2);

	if (String_isEmpty(s)) {
		*result = String_createNull();
		return Error_none();
	}

	U64 strl = String_length(s);

	if(off >= strl)
		return Error_outOfBounds(1, off, strl);

	*result = (String) {
		.ptr = s.ptr + off,
		.len = (strl - off) | ((U64)String_isNullTerminated(s) << 63),
		.capacity = String_isConstRef(s) ? U64_MAX : 0
	};

	return Error_none();
}

#define String_matchesPattern(testFunc, ...)					\
																\
	U64 strl = String_length(s);								\
																\
	for(U64 i = (__VA_ARGS__); i < String_length(s); ++i) {		\
																\
		C8 c = s.ptr[i];										\
																\
		if(!testFunc(c))										\
			return false;										\
	}															\
																\
	return strl;

#define String_matchesPatternNum(testFunc, num) String_matchesPattern(		\
	testFunc,																\
	String_startsWithString(												\
		s,																	\
		String_createConstRefUnsafe(num),									\
		EStringCase_Insensitive												\
	) ? String_calcStrLen(num, U64_MAX) : 0									\
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

	U64 strl = String_length(s);

	if(!strl || !s.ptr)
		return false;

	//Validate sign

	U64 i = 0;

	if (s.ptr[0] == '-' || s.ptr[0] == '+') {

		if (++i >= strl)
			return false;
	}

	//Validate first int 

	for(; i < strl; ++i)

		if (s.ptr[i] == '.' || s.ptr[i] == 'e' || s.ptr[i] == 'E')
			break;

		else if(!C8_isDec(s.ptr[i]))
			return false;

	if (i == strl)				//It's just an int
		return true;

	//Validate fraction

	if (s.ptr[i] == '.') {

		if (++i == strl)		//It's just an int
			return true;

		//Check for int until e/E

		for(; i < strl; ++i)

			if (s.ptr[i] == 'e' || s.ptr[i] == 'E')
				break;

			else if(!C8_isDec(s.ptr[i]))
				return false;
	}

	if (i == strl)				//It's just [-+]?[0-9]*[.]?[0-9]*
		return true;

	//Validate exponent

	if (s.ptr[i] == 'E' || s.ptr[i] == 'e') {

		if (++i == strl)
			return false;

		//e-NNN, e+NNN

		if (s.ptr[i] == '-' || s.ptr[i] == '+') {

			if (++i == strl)
				return false;
		}

		for(; i < strl; ++i)
			if(!C8_isDec(s.ptr[i]))
				return false;
	}

	return true;
}

Error String_create(C8 c, U64 size, Allocator alloc, String *result) {

	if (!alloc.alloc)
		return Error_nullPointer(2);

	if (!result)
		return Error_nullPointer(3);

	if (result->ptr)
		return Error_invalidOperation(0);

	if (size >> 48)
		return Error_invalidOperation(1);

	if (!size) {
		*result = String_createNull();
		return Error_none();
	}

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, size + 1, &b);

	if (err.genericError)
		return err;

	U64 realSize = size;

	((C8*)b.ptr)[size] = '\0';

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
		*(U32*)(b.ptr + (realSize >> 3 << 3)) = cc4;
		size &= 3;
	}

	if (size & 2) {
		*(U16*)(b.ptr + (realSize >> 2 << 2)) = cc2;
		size &= 1;
	}

	if(size & 1)
		((C8*)b.ptr)[realSize - 1] = c;

	*result = (String) { .len = (realSize | ((U64)1 << 63)), .capacity = realSize + 1, .ptr = (C8*)b.ptr };
	return Error_none();
}

Error String_createCopy(String str, Allocator alloc, String *result) {

	if (!alloc.alloc || !result)
		return Error_nullPointer(!result ? 2 : 1);

	if (result->ptr)
		return Error_invalidOperation(0);

	U64 strl = String_length(str);

	if (!strl) {
		*result = String_createNull();
		return Error_none();
	}

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, strl + 1, &b);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < strl >> 3; ++i)
		*((U64*)b.ptr + i) = *((const U64*)str.ptr + i);

	for (U64 i = strl >> 3 << 3; i < strl; ++i)
		((C8*)b.ptr)[i] = str.ptr[i];

	((C8*)b.ptr)[strl] = '\0';

	*result = (String) { .ptr = (C8*)b.ptr, .len = (strl | ((U64)1 << 63)), .capacity = strl + 1 };
	return Error_none();
}

Bool String_free(String *str, Allocator alloc) {

	if (!str)
		return true;

	if(!alloc.free)
		return false;

	Bool freed = true;

	if(!String_isRef(*str))
		freed = alloc.free(alloc.ptr, Buffer_createManagedPtr((U8*)str->ptr, str->capacity));

	*str = String_createNull();
	return freed;
}

#define String_createNum(maxVal, func, prefixRaw, ...)					\
																		\
	if (!result)														\
		return Error_nullPointer(3);									\
																		\
	String prefix = String_createConstRefUnsafe(prefixRaw);				\
																		\
	if (result->ptr)													\
		return Error_invalidOperation(0);								\
																		\
	*result = String_createNull();										\
	Error err = String_reserve(											\
		result, maxVal + String_length(prefix) + 1, allocator			\
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
	Bool foundFirstNonZero = false;										\
																		\
	for (U64 i = maxVal - 1; i != U64_MAX; --i) {						\
																		\
		C8 c = C8_create##func(__VA_ARGS__);							\
																		\
		if (!foundFirstNonZero)											\
			foundFirstNonZero = c != '0' || i < leadingZeros;			\
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
	if (!v && !foundFirstNonZero) {										\
																		\
		err = String_append(result, '0', allocator);					\
																		\
		if (err.genericError) {											\
			String_free(result, allocator);								\
			return err;													\
		}																\
	}																	\
																		\
	((C8*)result->ptr)[String_length(*result)] = '\0';					\
	return Error_none();

Error String_createNyto(U64 v, U8 leadingZeros, Allocator allocator, String *result){
	String_createNum(11, Nyto, "0n", (v >> (6 * i)) & 63);
}

Error String_createHex(U64 v, U8 leadingZeros, Allocator allocator, String *result) {
	String_createNum(16, Hex, "0x", (v >> (4 * i)) & 15);
}

Error String_createDec(U64 v, U8 leadingZeros, Allocator allocator, String *result) {
	String_createNum(20, Dec, "", (v / U64_10pow(i)) % 10);
}

Error String_createOct(U64 v, U8 leadingZeros, Allocator allocator, String *result) {
	String_createNum(22, Oct, "0o", (v >> (3 * i)) & 7);
}

Error String_createBin(U64 v, U8 leadingZeros, Allocator allocator, String *result) {
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

	U64 strl = String_length(s);

	if (!length) {

		Bool b = String_isNullTerminated(s);

		result->ptr[0] = String_isConstRef(s) ? String_createConstRefSized(s.ptr, strl, b) :
			String_createRefSized((C8*)s.ptr, strl, b);

		return Error_none();
	}

	StringList str = *result;

	c = C8_transform(c, (EStringTransform) casing);

	U64 count = 0, last = 0;

	for (U64 i = 0; i < strl; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform) casing) == c) {

			str.ptr[count++] = 
				String_isConstRef(s) ? String_createConstRefSized(s.ptr + last, i - last, false) : 
				String_createRefSized((C8*)s.ptr + last, i - last, false);

			last = i + 1;
		}

	Bool b = String_isNullTerminated(s);

	str.ptr[count++] = 
		String_isConstRef(s) ? String_createConstRefSized(s.ptr + last, strl - last, b) : 
		String_createRefSized((C8*)s.ptr + last, strl - last, b);

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

	Error err = StringList_create(length + 1, allocator, result);

	if (err.genericError)
		return err;

	Bool b = String_isNullTerminated(s);
	U64 strl = String_length(s);

	if (!length) {

		*result->ptr = String_isConstRef(s) ? String_createConstRefSized(s.ptr, strl, b) :
			String_createRefSized((C8*)s.ptr, strl, b);

		return Error_none();
	}

	StringList str = *result;

	U64 count = 0, last = 0;

	for (U64 i = 0; i < strl; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < strl; ++j, ++k)
			if (
				C8_transform(s.ptr[j], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {

			str.ptr[count++] = 
				String_isConstRef(s) ? String_createConstRefSized(s.ptr + last, i - last, false) : 
				String_createRefSized((C8*)s.ptr + last, i - last, false);

			last = i + strl;
			i += strl - 1;
		}
	}

	str.ptr[count++] = 
		String_isConstRef(s) ? String_createConstRefSized(s.ptr + last, strl - last, b) : 
		String_createRefSized((C8*)s.ptr + last, strl - last, b);

	return Error_none();
}

Error String_splitLine(String s, Allocator alloc, StringList *result) {

	if(!result)
		return Error_nullPointer(2);

	if(result->ptr)
		return Error_invalidParameter(2, 1);

	U64 v = 0, lastLineEnd = U64_MAX;
	U64 strl = String_length(s);

	for(U64 i = 0; i < strl; ++i)

		if (s.ptr[i] == '\n') {			//Unix line endings
			++v;
			lastLineEnd = i;
			continue;
		}

		else if (s.ptr[i] == '\r') {	//Windows/Legacy Mac line endings

			if(i + 1 < strl && s.ptr[i + 1] == '\n')		//Windows
				++i;

			++v;
			lastLineEnd = i;
			continue;
		}

	if(lastLineEnd != strl)
		++v;

	Error err = StringList_create(v, alloc, result);

	if (err.genericError)
		return err;

	v = 0;
	lastLineEnd = 0;

	for(U64 i = 0; i < strl; ++i) {

		Bool isLineEnd = false;

		U64 iOld = i;

		if (s.ptr[i] == '\n')			//Unix line endings
			isLineEnd = true;

		else if (s.ptr[i] == '\r') {	//Windows/Legacy Mac line endings

			if(i + 1 < strl && s.ptr[i + 1] == '\n')		//Windows
				++i;

			isLineEnd = true;
		}

		if(!isLineEnd)
			continue;

		result->ptr[v++] = String_isConstRef(s) ? 
			String_createConstRefSized(s.ptr + lastLineEnd, iOld - lastLineEnd, false) : 
			String_createRefSized((C8*)s.ptr + lastLineEnd, iOld - lastLineEnd, false);

		lastLineEnd = i + 1;
	}

	if(lastLineEnd != strl)
		result->ptr[v++] = String_isConstRef(s) ? 
			String_createConstRefSized(s.ptr + lastLineEnd, strl - lastLineEnd, String_isNullTerminated(s)) : 
			String_createRefSized((C8*)s.ptr + lastLineEnd, strl - lastLineEnd, String_isNullTerminated(s));

	return Error_none();
}

Error String_reserve(String *str, U64 length, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0);

	if (String_isRef(*str) && String_length(*str))
		return Error_invalidOperation(0);

	if (length >> 48)
		return Error_invalidOperation(1);

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(2);

	if (length + 1 <= str->capacity)
		return Error_none();

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, length + 1, &b);

	if (err.genericError)
		return err;

	Buffer_copy(b, String_bufferConst(*str));

	((C8*)b.ptr)[length] = '\0';
	str->len |= (U64)1 << 63;

	err = 
		alloc.free(alloc.ptr, Buffer_createManagedPtr((U8*)str->ptr, str->capacity)) ? 
		Error_none() : Error_invalidOperation(0);

	str->capacity = Buffer_length(b);
	str->ptr = (const C8*) b.ptr;
	return err;
}

Error String_resize(String *str, U64 length, C8 defaultChar, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0);

	U64 strl = String_length(*str);

	if (String_isRef(*str) && strl)
		return Error_invalidOperation(0);

	if (length >> 48)
		return Error_invalidOperation(1);

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(3);

	if (length == strl && String_isNullTerminated(*str))
		return Error_none();

	//Simple resize; we cut off the tail

	if (length < strl) {

		Bool b = String_isNullTerminated(*str);

		str->len = ((U64)b << 63) | length;

		if(b)
			((C8*)str->ptr)[length] = '\0';

		return Error_none();
	}

	//Resize that triggers buffer resize

	if (length + 1 > str->capacity) {

		//Reserve 50% more to ensure we don't resize too many times

		Error err = String_reserve(str, U64_max(64, length * 3 / 2) + 1, alloc);

		if (err.genericError)
			return err;
	}

	//Our capacity is big enough to handle it:

	for (U64 i = strl; i < length; ++i)
		((C8*)str->ptr)[i] = defaultChar;

	((C8*)str->ptr)[length] = '\0';
	str->len = length | ((U64)1 << 63);
	return Error_none();
}

Error String_append(String *s, C8 c, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0);

	if(!c && String_isNullTerminated(*s))
		return Error_none();

	return String_resize(s, String_length(*s) + (Bool)c, c, allocator);
}

#if _WIN32
	String String_newLine() { return String_createConstRefUnsafe("\r\n"); }
#else
	String String_newLine() { return String_createConstRefUnsafe("\n"); }
#endif

Error String_appendString(String *s, String other, Allocator allocator) {

	U64 otherl = String_length(other);

	if (!otherl)
		return Error_none();

	other = String_createConstRefSized(other.ptr, otherl, String_isNullTerminated(other));

	if (!s)
		return Error_nullPointer(0);

	U64 oldLen = String_length(*s);

	if (String_isRef(*s) && oldLen)
		return Error_invalidParameter(0, 0);

	Error err = String_resize(s, oldLen + otherl, other.ptr[0], allocator);

	if (err.genericError)
		return err;

	Buffer_copy(Buffer_createRef((U8*)s->ptr + oldLen, otherl), String_bufferConst(other));
	return Error_none();
}

Error String_insert(String *s, C8 c, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0);

	U64 strl = String_length(*s);

	if (String_isRef(*s) && strl)
		return Error_invalidParameter(0, 0);

	if(i > strl)
		return Error_outOfBounds(2, i, strl);

	if(i == strl && !c && String_isNullTerminated(*s))
		return Error_none();

	if(!c && i != strl)
		return Error_invalidOperation(0);

	Error err = String_resize(s, strl + 1, c, allocator);

	if (err.genericError)
		return err;

	//If it's not append (otherwise it's already handled)

	if (i != strl - 1) {

		//Move one to the right

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + i + 1,  strl),
			Buffer_createConstRef(s->ptr + i, strl)
		);

		((C8*)s->ptr)[i] = c;
	}

	return Error_none();
}

Error String_insertString(String *s, String other, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0);

	U64 oldLen = String_length(*s);

	if (String_isRef(*s) && oldLen)
		return Error_invalidParameter(0, 0);

	U64 otherl = String_length(other);

	if (!otherl)
		return Error_none();

	Error err = String_resize(s, oldLen + otherl, ' ', allocator);

	if (err.genericError)
		return err;

	//Move one to the right

	Buffer_revCopy(
		Buffer_createRef((U8*)s->ptr + i + otherl, oldLen - i),
		Buffer_createConstRef(s->ptr + i, oldLen - i)
	);

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + i, otherl),
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
		return Error_nullPointer(0);

	if(String_isConstRef(*s))
		return Error_constData(0, 0);

	List finds = List_createEmpty(sizeof(U64));
	Error err = String_findAllString(*s, search, allocator, caseSensitive, &finds);

	if(err.genericError)
		return err;

	if (!finds.length)
		return Error_none();

	//Easy replace

	const U64 *ptr = (const U64*) finds.ptr;

	U64 searchl = String_length(search);
	U64 replacel = String_length(replace);
	U64 strl = String_length(*s);

	if (searchl == replacel) {

		for (U64 i = 0; i < finds.length; ++i)
			for (U64 j = ptr[i], k = j + replacel, l = 0; j < k; ++j, ++l)
				((C8*)s->ptr)[j] = replace.ptr[l];

		goto clean;
	}

	if(String_isRef(*s))
		return Error_invalidOperation(0);

	//Shrink replaces

	if (searchl > replacel) {

		U64 diff = searchl - replacel;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			//We move our replacement string to iCurrent

			Buffer_copy(
				Buffer_createRef((U8*)s->ptr + iCurrent, replacel), 
				String_bufferConst(replace)
			);

			iCurrent += replacel;

			//We then move the tail of the string 

			const U64 iStart = ptr[i] + searchl;
			const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

			Buffer_copy(
				Buffer_createRef((U8*)s->ptr + iCurrent, iEnd - iStart), 
				Buffer_createConstRef(s->ptr + iStart, iEnd - iStart)
			);

			iCurrent += iEnd - iStart;
		}

		//Ensure the string is now the right size

		err = String_resize(s, strl - diff * finds.length, ' ', allocator);
		goto clean;
	}

	//Grow replaces

	//Ensure the string is now the right size

	U64 diff = replacel - searchl;

	_gotoIfError(clean, String_resize(s, strl + diff * finds.length, ' ', allocator));

	//Move from right to left

	U64 newLoc = strl - 1;

	for (U64 i = finds.length - 1; i != U64_MAX; ++i) {

		//Find tail

		const U64 iStart = ptr[i] + searchl;
		const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

		for (U64 j = iEnd - 1; j != U64_MAX && j >= iStart; --j)
			((C8*)s->ptr)[newLoc--] = s->ptr[j];

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + newLoc - (iEnd - iStart), iEnd - iStart),
			Buffer_createConstRef(s->ptr + iStart, iEnd - iStart)
		);

		newLoc -= iEnd - iStart;

		//Apply replacement before tail

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + newLoc - replacel, replacel), 
			String_bufferConst(replace)
		);

		newLoc -= replacel;
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
		return Error_nullPointer(0);

	if(String_isConstRef(*s))
		return Error_constData(0, 0);

	U64 res = isFirst ? String_findFirstString(*s, search, caseSensitive) : 
		String_findLastString(*s, search, caseSensitive);

	if (res == U64_MAX)
		return Error_none();

	U64 searchl = String_length(search);
	U64 replacel = String_length(replace);
	U64 strl = String_length(*s);

	//Easy, early exit. Strings are same size.

	if (searchl == replacel) {
		Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), String_bufferConst(replace));
		return Error_none();
	}

	if(String_isRef(*s))
		return Error_invalidOperation(0);

	//Replacement is smaller than our search
	//So we can just move from left to right
	
	if (replacel < searchl) {

		U64 diff = searchl - replacel;	//How much we have to shrink

		//Copy our data over first

		Buffer_copy(
			Buffer_createRef((U8*)s->ptr + res + replacel, strl - (res + searchl)), 
			Buffer_createConstRef(s->ptr + res + searchl, strl - (res + searchl)) 
		);

		//Throw our replacement in there

		Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), String_bufferConst(replace));

		//Shrink the string; this is done after because we need to read the right of the string first

		Error err = String_resize(s, strl - diff, ' ', allocator);

		if (err.genericError)
			return err;

		return Error_none();
	}

	//Replacement is bigger than our search;
	//We need to grow first and move from right to left

	U64 diff = replacel - searchl;

	Error err = String_resize(s, strl + diff, ' ', allocator);

	if (err.genericError)
		return err;

	//Copy our data over first

	Buffer_revCopy(
		Buffer_createRef((U8*)s->ptr + res + replacel, strl - (res + searchl)), 
		Buffer_createConstRef(s->ptr + res + searchl, strl - (res + searchl)) 
	);

	//Throw our replacement in there

	Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), String_bufferConst(replace));
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
		String_length(str) && str.ptr && 
		C8_transform(*str.ptr, (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_endsWith(String str, C8 c, EStringCase caseSensitive) {
	return 
		String_length(str) && str.ptr && 
		C8_transform(str.ptr[String_length(str) - 1], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_startsWithString(String str, String other, EStringCase caseSensitive) {

	U64 otherl = String_length(other);
	U64 strl = String_length(str);

	if(!otherl)
		return true;

	if (otherl > strl)
		return false;

	for (U64 i = 0; i < otherl; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_endsWithString(String str, String other, EStringCase caseSensitive) {

	U64 otherl = String_length(other);
	U64 strl = String_length(str);

	if(!otherl)
		return true;

	if (otherl > strl)
		return false;

	for (U64 i = strl - otherl; i < strl; ++i)
		if (
			C8_transform(str.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i - (strl - otherl)], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

U64 String_countAll(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	U64 count = 0;

	for (U64 i = 0; i < String_length(s); ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			++count;

	return count;
}

U64 String_countAllString(String s, String other, EStringCase casing) {

	U64 otherl = String_length(other);
	U64 strl = String_length(s);

	if(!otherl || strl < otherl)
		return 0;

	U64 j = 0;

	for (U64 i = 0; i < strl; ++i) {

		Bool match = true;

		for (U64 l = i, k = 0; l < strl && k < otherl; ++l, ++k)
			if (
				C8_transform(s.ptr[l], (EStringTransform)casing) !=
				C8_transform(other.ptr[k], (EStringTransform)casing)
			) {
				match = false;
				break;
			}

		if (match) {
			i += strl - 1;
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
		return Error_nullPointer(4);

	if(result->ptr)
		return Error_invalidParameter(4, 0);

	U64 strl = String_length(s);

	List l = List_createEmpty(sizeof(U64));
	Error err = List_reserve(&l, strl / 25 + 16, alloc);

	if (err.genericError)
		return err;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	for (U64 i = 0; i < strl; ++i)
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
		return Error_nullPointer(4);

	if(result->ptr)
		return Error_invalidParameter(4, 0);

	U64 otherl = String_length(other);
	U64 strl = String_length(s);

	if(!otherl)
		return Error_invalidParameter(1, 0);

	List l = List_createEmpty(sizeof(U64));

	if(strl < otherl) {
		*result = l;
		return Error_none();
	}

	Error err = List_reserve(&l, strl / otherl / 25 + 16, alloc);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < strl; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
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

			i += strl - 1;
		}
	}

	*result = l;
	return Error_none();
}

U64 String_findFirst(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = 0; i < String_length(s); ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return U64_MAX;
}

U64 String_findLast(String s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = String_length(s) - 1; i != U64_MAX; --i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return U64_MAX;
}

U64 String_findFirstString(String s, String other, EStringCase casing) {

	U64 otherl = String_length(other);
	U64 strl = String_length(s);

	if(!otherl || strl < otherl)
		return U64_MAX;

	U64 i = 0;

	for (; i < strl; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
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
	
	U64 strl = String_length(s);
	U64 otherl = String_length(other);

	if(!otherl || strl < otherl)
		return U64_MAX;

	U64 i = strl - 1;

	for (; i != U64_MAX; --i) {

		Bool match = true;

		for (U64 j = i, k = otherl - 1; j != U64_MAX && k != U64_MAX; --j, --k)
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

	U64 strl = String_length(s);
	U64 otherl = String_length(other);

	if (strl != otherl)
		return false;

	for (U64 i = 0; i < strl; ++i)
		if (
			C8_transform(s.ptr[i], (EStringTransform)caseSensitive) != 
			C8_transform(other.ptr[i], (EStringTransform)caseSensitive)
		)
			return false;

	return true;
}

Bool String_equals(String s, C8 c, EStringCase caseSensitive) {

	return String_length(s) == 1 && s.ptr && 
		C8_transform(s.ptr[0], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool String_parseNyto(String s, U64 *result){

	String prefix = String_createConstRefUnsafe("0n");

	Error err = String_offsetAsRef(
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? String_length(prefix) : 0, &s
	);

	U64 strl = String_length(s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || strl > 11)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 1; i != U64_MAX; --i, j <<= 6) {

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
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? String_length(prefix) : 0, &s
	);

	if (err.genericError)
		return false;

	U64 strl = String_length(s);

	if (!result || !s.ptr || strl > 16)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 0; i != U64_MAX; --i, j += 4) {

		U8 v = C8_hex(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		*result |= (U64)v << j;
	}

	return true;
}

Bool String_parseDec(String s, U64 *result) {

	U64 strl = String_length(s);

	if (!result || !s.ptr || strl > 20)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 1; i != U64_MAX; --i, j *= 10) {

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

		if (!String_length(s)) {
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

		if (!String_length(s)) {
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

	while(String_length(s)) {

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
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? String_length(prefix) : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || String_length(s) > 22)
		return false;

	*result = 0;

	for (U64 i = String_length(s) - 1, j = 0; i != U64_MAX; --i, j += 3) {

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
		s, String_startsWithString(s, prefix, EStringCase_Insensitive) ? String_length(prefix) : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || String_length(s) > 64)
		return false;

	*result = 0;

	for (U64 i = String_length(s) - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

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

	U64 strl = String_length(s);

	if (!strl && !offset && !length) {
		*result = String_createNull();
		return true;
	}

	if(offset >= strl)
		return false;

	if(!length) 
		length = strl - offset;

	if (offset + length > strl)
		return false;

	if (offset == strl) {
		*result = String_createNull();
		return false;
	}

	Bool isNullTerm = String_isNullTerminated(s) && offset + length == strl;

	*result = String_isConstRef(s) ? String_createConstRefSized(s.ptr + offset, length, isNullTerm) :
		String_createRefSized((C8*)s.ptr + offset, length, isNullTerm);

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

	if (found == U64_MAX)
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

	if (found == U64_MAX)
		return false;

	return String_cut(s, 0, found, result);
}

Bool String_cutBefore(String s, C8 c, EStringCase caseSensitive, Bool isFirst, String *result) {

	if (!result)
		return false;

	U64 found = isFirst ? String_findFirst(s, c, caseSensitive) : String_findLast(s, c, caseSensitive);

	if (found == U64_MAX)
		return false;

	++found;	//The end of the occurence is the begin of the next string
	return String_cut(s, found, 0, result);
}

Bool String_cutBeforeString(String s, String other, EStringCase caseSensitive, Bool isFirst, String *result) {

	U64 found = isFirst ? String_findFirstString(s, other, caseSensitive) : 
		String_findLastString(s, other, caseSensitive);

	if (found == U64_MAX)
		return false;

	found += String_length(other);	//The end of the occurence is the begin of the next string
	return String_cut(s, found, 0, result);
}

Bool String_erase(String *s, C8 c, EStringCase caseSensitive, Bool isFirst) {

	if(!s || String_isRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	//Skipping first match

	U64 find = String_find(*s, c, caseSensitive, isFirst);

	if(find == U64_MAX)
		return false;

	U64 strl = String_length(*s);

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + find, strl - find - 1),
		Buffer_createConstRef(s->ptr + find + 1, strl - find - 1)
	);

	((C8*)s->ptr)[strl - 1] = '\0';
	s->len = (strl - 1) | ((U64)1 << 63);

	return true;
}

Error String_eraseAtCount(String *s, U64 i, U64 count) {

	if(!s)
		return Error_nullPointer(0);

	if(!count)
		return Error_invalidParameter(2, 0);

	if(String_isRef(*s))
		return Error_constData(0, 0);

	U64 strl = String_length(*s);

	if(i + count > strl)
		return Error_outOfBounds(0, i + count, strl);

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + i, strl - i - count),
		Buffer_createConstRef(s->ptr + i + count, strl - i - count)
	);

	U64 end = strl - count;
	((C8*)s->ptr)[end] = '\0';
	s->len = end | ((U64)1 << 63);
	return Error_none();
}

Bool String_eraseString(String *s, String other, EStringCase casing, Bool isFirst) {

	U64 otherl = String_length(other);

	if(!s || !otherl || String_isRef(*s))
		return false;

	U64 strl = String_length(*s);

	//Skipping first match

	U64 find = String_findString(*s, other, casing, isFirst);

	if(find == U64_MAX)
		return false;

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + find, strl - find - otherl),
		Buffer_createConstRef(s->ptr + find + otherl, strl - find - otherl)
	);

	U64 end = strl - otherl;
	((C8*)s->ptr)[end] = '\0';
	s->len = end | ((U64)1 << 63);
	return true;
}

//String's inline changes (no copy)

Bool String_cutEnd(String s, U64 i, String *result) {
	return String_cut(s, 0, i, result);
}

Bool String_cutBegin(String s, U64 i, String *result) {

	if (i > String_length(s))
		return false;

	return String_cut(s, i, 0, result);
}

Bool String_eraseAll(String *s, C8 c, EStringCase casing) {

	if(!s || String_isRef(*s))
		return false;

	U64 strl = String_length(*s);

	c = C8_transform(c, (EStringTransform) casing);

	U64 out = 0;

	for (U64 i = 0; i < strl; ++i) {

		if(C8_transform(s->ptr[i], (EStringTransform) casing) == c)
			continue;

		((C8*)s->ptr)[out++] = s->ptr[i];
	}
	
	if(out == strl)
		return false;

	((C8*)s->ptr)[out] = '\0';
	s->len = out | ((U64)1 << 63);
	return true;
}

Bool String_eraseAllString(String *s, String other, EStringCase casing) {

	U64 otherl = String_length(other);

	if(!s || !otherl || String_isRef(*s))
		return false;

	U64 strl = String_length(*s);

	U64 out = 0;

	for (U64 i = 0; i < strl; ++i) {

		Bool match = true;

		for (U64 j = i, k = 0; j < strl && k < otherl; ++j, ++k)
			if (
				C8_transform(s->ptr[j], (EStringTransform) casing) != 
				C8_transform(other.ptr[k], (EStringTransform) casing)
			) {
				match = false;
				break;
			}

		if (match) {
			i += strl - 1;	//-1 because we increment by 1 every time
			continue;
		}

		((C8*)s->ptr)[out++] = s->ptr[i];
	}

	if(out == strl)
		return true;

	((C8*)s->ptr)[out] = '\0';
	s->len = out | ((U64)1 << 63);
	return true;
}

Bool String_replaceAll(String *s, C8 c, C8 v, EStringCase caseSensitive) {

	if (!s || String_isConstRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for(U64 i = 0; i < String_length(*s); ++i)
		if(C8_transform(s->ptr[i], (EStringTransform)caseSensitive) == c)
			((C8*)s->ptr)[i] = v;

	return true;
}

Bool String_replace(String *s, C8 c, C8 v, EStringCase caseSensitive, Bool isFirst) {

	if (!s || String_isRef(*s))
		return false;

	U64 i = isFirst ? String_findFirst(*s, c, caseSensitive) : String_findLast(*s, c, caseSensitive);

	if(i != U64_MAX)
		((C8*)s->ptr)[i] = v;

	return true;
}

Bool String_transform(String str, EStringTransform c) {

	if(String_isConstRef(str))
		return false;

	for(U64 i = 0; i < String_length(str); ++i)
		((C8*)str.ptr)[i] = C8_transform(str.ptr[i], c);

	return true;
}

String String_trim(String s) {

	U64 strl = String_length(s);
	U64 first = strl;

	for (U64 i = 0; i < strl; ++i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			first = i;
			break;
		}

	if (first == strl)
		return String_createNull();

	U64 last = strl;

	for (U64 i = strl - 1; i != U64_MAX; --i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			last = i;
			break;
		}

	++last;		//We wanna include the last character that's non whitespace too

	Bool isNullTerm = String_isNullTerminated(s) && last == strl;
	
	return String_isConstRef(s) ? String_createConstRefSized(s.ptr + first, last - first, isNullTerm) :
		String_createRefSized((C8*)s.ptr + first, last - first, isNullTerm);
}

//StringList

Error StringList_create(U64 length, Allocator alloc, StringList *arr) {

	if (!arr)
		return Error_nullPointer(2);

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

	*arr = (StringList) { 0 };
	return freed;
}

Error StringList_createCopy(StringList toCopy, Allocator alloc, StringList *arr) {

	if(!toCopy.length)
		return Error_nullPointer(0);

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
		return Error_outOfBounds(1, i, arr.length);

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

	if(ptr)
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
		length += String_length(arr.ptr[i]);

	if(arr.length > 1)
		length += arr.length - 1;

	Error err = String_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0, l = String_length(arr.ptr[i]); k < l; ++k, ++j)
			((C8*)result->ptr)[j] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			((C8*)result->ptr)[j++] = between;
	}

	return Error_none();
}

Error StringList_concatString(StringList arr, String between, Allocator alloc, String *result) {

	U64 length = 0;

	for(U64 i = 0; i < arr.length; ++i)
		length += String_length(arr.ptr[i]);

	if(arr.length > 1)
		length += (arr.length - 1) * String_length(between);

	Error err = String_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0, l = String_length(arr.ptr[i]); k < l; ++k, ++j)
			((C8*)result->ptr)[j] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			for(U64 k = 0; k < String_length(between); ++k, ++j)
				((C8*)result->ptr)[j] = between.ptr[k];
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
Error String_popEndCount(String *s, U64 count) { return String_eraseAtCount(s, s ? String_length(*s) : 0, count); }

Error String_eraseAt(String *s, U64 i) { return String_eraseAtCount(s, i, 1); }
Error String_popFront(String *s) { return String_eraseAt(s, 0); }
Error String_popEnd(String *s) { return String_eraseAt(s, s ? String_length(*s) : 0); }

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

ECompareResult String_compare(String a, String b, EStringCase caseSensitive) {

	U64 al = String_length(a);
	U64 bl = String_length(b);

	//We want to sort on contents
	//Provided it's the same level of parenting.
	//This ensures things with the same parent also stay at the same location

	for (U64 i = 0; i < al && i < bl; ++i) {

		C8 ai = C8_transform(a.ptr[i], (EStringTransform) caseSensitive);
		C8 bi = C8_transform(b.ptr[i], (EStringTransform) caseSensitive);

		if (ai < bi)
			return ECompareResult_Lt;

		if (ai > bi)
			return ECompareResult_Gt;
	}

	//If they start with the same thing, we want to sort on length

	if (al < bl)
		return ECompareResult_Lt;

	if (al > bl)
		return ECompareResult_Gt;

	return ECompareResult_Eq;
}

//Format
//https://stackoverflow.com/questions/56331128/call-to-snprintf-and-vsnprintf-inside-a-variadic-function

#ifdef _WIN32
	#define calcFormatLen _vscprintf
#else
	#define calcFormatLen vsnprintf
#endif

Error String_formatVariadic(Allocator alloc, String *result, const C8 *format, va_list args) {

	if(!result || !format)
		return Error_nullPointer(!result ? 1 : 2);

	va_list arg2;
	va_copy(arg2, args);

	int len = calcFormatLen(format, arg2);

	if(len < 0)
		return Error_stderr(0);

	Error err = String_create('\0', (U64) len, alloc, result);

	if(err.genericError)
		return err;

	if(vsnprintf((C8*)result->ptr, len + 1, format, args) < 0)
		return Error_stderr(0);

	return Error_none();
}

Error String_format(Allocator alloc, String *result, const C8 *format, ...) {

	if(!result || !format)
		return Error_nullPointer(!result ? 1 : 2);

	va_list arg1;
	va_start(arg1, format);
	Error err = String_formatVariadic(alloc, result, format, arg1);
	va_end(arg1);

	return err;
}
