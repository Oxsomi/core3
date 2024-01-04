/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
*  
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "types/list_impl.h"
#include "types/string.h"
#include "types/allocator.h"
#include "types/math.h"
#include "types/buffer.h"
#include "types/error.h"

#include <ctype.h>
#include <stdio.h>

TListImpl(CharString);
TListNamedImpl(ListConstC8);

Bool ListCharString_sort(ListCharString list, EStringCase stringCase) { 
	return GenericList_sortString(ListCharString_toList(list), stringCase); 
}

Bool ListCharString_sortSensitive(ListCharString list) { 
	return GenericList_sortStringSensitive(ListCharString_toList(list)); 
}

Bool ListCharString_sortInsensitive(ListCharString list) { 
	return GenericList_sortStringInsensitive(ListCharString_toList(list)); 
}

Bool CharString_isConstRef(CharString str) { return str.capacityAndRefInfo == U64_MAX; }
Bool CharString_isRef(CharString str) { return !str.capacityAndRefInfo || CharString_isConstRef(str); }
U64  CharString_bytes(CharString str) { return str.lenAndNullTerminated << 1 >> 1; }
U64  CharString_length(CharString str) { return CharString_bytes(str); }
Bool CharString_isEmpty(CharString str) { return !CharString_bytes(str); }
Bool CharString_isNullTerminated(CharString str) { return str.lenAndNullTerminated >> 63; }

Buffer CharString_buffer(CharString str) { 
	return CharString_isConstRef(str) ? Buffer_createNull() : Buffer_createRef((U8*)str.ptr, CharString_bytes(str)); 
}

Buffer CharString_bufferConst(CharString str) { 
	return Buffer_createRefConst(str.ptr, CharString_bytes(str));
}

//Iteration

C8 *CharString_begin(CharString str) { return CharString_isConstRef(str) ? NULL : (C8*)str.ptr; }
const C8 *CharString_beginConst(CharString str) { return str.ptr; }

C8 *CharString_end(CharString str) { return CharString_isConstRef(str) ? NULL : (C8*)str.ptr + CharString_length(str); }
const C8 *CharString_endConst(CharString str) { return str.ptr + CharString_length(str); }

C8 *CharString_charAt(CharString str, U64 off) { 
	return CharString_isConstRef(str) || off >= CharString_length(str) ? NULL : (C8*)str.ptr + off;
}

const C8 *CharString_charAtConst(CharString str, U64 off) { 
	return off >= CharString_length(str) ? NULL : str.ptr + off; 
}

Bool CharString_isValidAscii(CharString a) {

	for(U64 i = 0; i < CharString_length(a); ++i)
		if(!C8_isValidAscii(a.ptr[i]))
			return false;

	return true;
}

Bool CharString_isValidFileName(CharString str) {
	
	for(U64 i = 0; i < CharString_length(str); ++i)
		if(!C8_isValidFileName(str.ptr[i]))
			return false;

	//Trailing or leading space is illegal

	if(CharString_endsWithSensitive(str, ' '))
		return false;

	if(CharString_startsWithSensitive(str, ' '))
		return false;
	
	//Validation to make sure we're not using weird legacy MS DOS keywords
	//Because these will not be writable correctly!

	U64 illegalStart = 0, strl = CharString_length(str);

	if (strl >= 3) {

		if(
			CharString_startsWithStringInsensitive(str, CharString_createConstRefCStr("CON")) ||
			CharString_startsWithStringInsensitive(str, CharString_createConstRefCStr("AUX")) ||
			CharString_startsWithStringInsensitive(str, CharString_createConstRefCStr("NUL")) ||
			CharString_startsWithStringInsensitive(str, CharString_createConstRefCStr("PRN"))
		)
			illegalStart = 3;

		else if (strl >= 4) {

			if(
				(
					CharString_startsWithStringInsensitive(str, CharString_createConstRefCStr("COM")) ||
					CharString_startsWithStringInsensitive(str, CharString_createConstRefCStr("LPT"))
				) &&
				C8_isDec(str.ptr[3])
			)
				illegalStart = 4;
		}
	}

	//PRNtscreen.pdf is valid, but PRN.pdf isn't.
	///NULlpointer.txt is valid, NUL.txt isn't.

	if(illegalStart && (strl == illegalStart || CharString_getAt(str, illegalStart) == '.'))
		return false;

	//Can't end with trailing . (so . and .. are illegal)

	if (strl && str.ptr[strl - 1] == '.')
		return false;

	//If string is not empty then it's a valid string

	return strl;
}

//We support valid file names or ., .. in file path parts.

Bool CharString_isSupportedInFilePath(CharString str) {
	return 
		CharString_isValidFileName(str) || 
		(CharString_getAt(str, 0) == '.' && (
			CharString_length(str) == 1 || (CharString_getAt(str, 1) == '.' && CharString_length(str) == 2)
		));
}

//File_resolve but without validating if it'd be a valid (permissioned) path on disk.

Bool CharString_isValidFilePath(CharString str) {

	//myTest/ <-- or myTest\ to myTest
		
	str = CharString_createConstRefSized(str.ptr, CharString_length(str), CharString_isNullTerminated(str));

	if(CharString_getAt(str, CharString_length(str) - 1) == '/' || CharString_getAt(str, CharString_length(str) - 1) == '\\')
		str.lenAndNullTerminated = CharString_length(str) - 1;
		
	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)
	//We also obviously don't support 0:\ and such or A:/ on unix

	Bool hasPrefix = false;

	#ifdef _WIN32

		if(
			CharString_length(str) >= 3 && 
			str.ptr[1] == ':' && ((str.ptr[2] != '/' && str.ptr[2] != '\\') || !C8_isAlpha(str.ptr[0]))
		)
			return false;

		//Absolute

		if(str.ptr[1] == ':') {
			str.ptr += 3;
			str.lenAndNullTerminated -= 3;
			hasPrefix = true;
		}

	#else

		if(CharString_length(str) >= 2 && str.ptr[1] == ':')
			return false;

	#endif

	//Virtual files

	if(CharString_getAt(str, 0) == '/' && CharString_getAt(str, 1) == '/') {

		if(hasPrefix)
			return false;

		str.ptr += 2;
		str.lenAndNullTerminated -= 2;
	}

	//Absolute path

	if(CharString_getAt(str, 0) == '/' || CharString_getAt(str, 0) == '\\') {

		if(hasPrefix)
			return false;

		++str.ptr;
		--str.lenAndNullTerminated;
		hasPrefix = true;
	}

	//Windows network paths, this is unsupported currently

	if(CharString_getAt(str, 0) == '\\' && CharString_getAt(str, 1) == '\\') 
		return false;

	//Split by / or \.

	U64 prev = 0;
	U64 strl = CharString_length(str);

	for (U64 i = 0; i < strl; ++i) {

		C8 c = str.ptr[i];

		//Push previous

		if (c == '/' || c == '\\') {

			if(!(i - prev))
				return false;

			CharString part = CharString_createConstRefSized(str.ptr + prev, i - prev, false);

			if(!CharString_isSupportedInFilePath(part))
				return false;

			prev = i + 1;
		}
	}

	//Validate ending

	CharString part = CharString_createConstRefSized(str.ptr + prev, strl - prev, CharString_isNullTerminated(str));

	if(!CharString_isSupportedInFilePath(part))
		return false;

	return strl;
}

Bool CharString_clear(CharString *str) { 

	if(!str || CharString_isRef(*str) || !str->capacityAndRefInfo) 
		return false;

	if(str->lenAndNullTerminated >> 63)					//If null terminated, we want to keep it null terminated
		((C8*)str->ptr)[0] = '\0';

	str->lenAndNullTerminated &= ~(((U64)1 << 63) - 1);		//Clear size
	return true;
}

C8 CharString_getAt(CharString str, U64 i) { 
	return i < CharString_length(str) ? str.ptr[i] : C8_MAX;
}

Bool CharString_setAt(CharString str, U64 i, C8 c) { 

	if(i >= CharString_length(str) || CharString_isConstRef(str) || !c)
		return false;

	((C8*)str.ptr)[i] = c;
	return true;
}

CharString CharString_createNull() { return (CharString) { 0 }; }

CharString CharString_createConstRefAuto(const C8 *ptr, U64 maxSize) { 

	if(!ptr)
		return CharString_createNull();

	U64 strl = CharString_calcStrLen(ptr, maxSize);

	return (CharString) { 
		.lenAndNullTerminated = strl | ((U64)(strl != maxSize) << 63),
		.ptr = (C8*) ptr,
		.capacityAndRefInfo = U64_MAX		//Flag as const
	};
}

CharString CharString_createConstRefCStr(const C8 *ptr) {
	return CharString_createConstRefAuto(ptr, U64_MAX);
}

CharString CharString_createRefAuto(C8 *ptr, U64 maxSize) { 
	CharString str = CharString_createConstRefAuto(ptr, maxSize);
	str.capacityAndRefInfo = 0;	//Flag as dynamic
	return str;
}

CharString CharString_createConstRefSized(const C8 *ptr, U64 size, Bool isNullTerminated) {

	if(!ptr || (size >> 48))
		return CharString_createNull();

	if(isNullTerminated && ptr[size])	//Invalid!
		return CharString_createNull();

	if(!isNullTerminated && size) {

		isNullTerminated = !ptr[size - 1];

		if(isNullTerminated)
			--size;
	}

	return (CharString) { 
		.lenAndNullTerminated = size | ((U64)isNullTerminated << 63),
		.ptr = (C8*) ptr,
		.capacityAndRefInfo = U64_MAX		//Flag as const
	};
}

CharString CharString_createRefSized(C8 *ptr, U64 size, Bool isNullTerminated) {
	CharString str = CharString_createConstRefSized(ptr, size, isNullTerminated);
	str.capacityAndRefInfo = 0;	//Flag as dynamic
	return str;
}

CharString CharString_createConstRefShortString(const ShortString str) {
	return CharString_createConstRefAuto(str, _SHORTSTRING_LEN);
}

CharString CharString_createConstRefLongString(const LongString str) {
	return CharString_createConstRefAuto(str, _LONGSTRING_LEN);
}

CharString CharString_createRefShortString(ShortString str) {
	return CharString_createRefAuto(str, _SHORTSTRING_LEN);
}

CharString CharString_createRefLongString(LongString str) {
	return CharString_createRefAuto(str, _LONGSTRING_LEN);
}

//Simple checks (consts)

Error CharString_offsetAsRef(CharString s, U64 off, CharString *result) {
	
	if (!result)
		return Error_nullPointer(2, "CharString_offsetAsRef()::result is required");

	if (CharString_isEmpty(s)) {
		*result = CharString_createNull();
		return Error_none();
	}

	U64 strl = CharString_length(s);

	if(off >= strl)
		return Error_outOfBounds(1, off, strl, "CharString_offsetAsRef()::off is out of bounds");

	*result = (CharString) {
		.ptr = s.ptr + off,
		.lenAndNullTerminated = (strl - off) | ((U64)CharString_isNullTerminated(s) << 63),
		.capacityAndRefInfo = CharString_isConstRef(s) ? U64_MAX : 0
	};

	return Error_none();
}

#define CharString_matchesPattern(testFunc, ...)								\
																				\
	U64 strl = CharString_length(s);											\
																				\
	for(U64 i = (__VA_ARGS__); i < CharString_length(s); ++i) {					\
																				\
		C8 c = s.ptr[i];														\
																				\
		if(!testFunc(c))														\
			return false;														\
	}																			\
																				\
	return strl;

#define CharString_matchesPatternNum(testFunc, num) CharString_matchesPattern(	\
	testFunc,																	\
	CharString_startsWithStringInsensitive(										\
		s, CharString_createConstRefCStr(num)									\
	) ? CharString_calcStrLen(num, U64_MAX) : 0									\
)

Bool CharString_isNyto(CharString s) {
	CharString_matchesPatternNum(C8_isNyto, "0n");
}

Bool CharString_isAlphaNumeric(CharString s) {
	CharString_matchesPattern(C8_isAlphaNumeric, 0);
}

Bool CharString_isHex(CharString s) {
	CharString_matchesPatternNum(C8_isHex, "0x");
}

Bool CharString_isOct(CharString s) {
	CharString_matchesPatternNum(C8_isOct, "0o");
}

Bool CharString_isBin(CharString s) {
	CharString_matchesPatternNum(C8_isBin, "0b");
}

Bool CharString_isDec(CharString s) {
	CharString_matchesPattern(C8_isDec, 0);
}

Bool CharString_isSignedNumber(CharString s) {
	CharString_matchesPattern(
		C8_isDec,
		CharString_startsWithSensitive(s, '-') || CharString_startsWithSensitive(s, '+')
	);
}

Bool CharString_isUnsignedNumber(CharString s) {
	CharString_matchesPattern(
		C8_isDec,
		CharString_startsWithSensitive(s, '+')
	);
}

Bool CharString_isFloat(CharString s) {

	U64 strl = CharString_length(s);

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

Error CharString_create(C8 c, U64 size, Allocator alloc, CharString *result) {

	if (!alloc.alloc)
		return Error_nullPointer(2, "CharString_create()::alloc is required");

	if (!result)
		return Error_nullPointer(3, "CharString_create()::result is required");

	if (result->ptr)
		return Error_invalidOperation(0, "CharString_create()::result isn't empty, might indicate memleak");

	if (size >> 48)
		return Error_invalidOperation(1, "CharString_create()::size must be 48-bit");

	if (!size) {
		*result = CharString_createNull();
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

	for (U64 i = 0; i < size >> 3; ++i)
		*((U64*)b.ptr + i) = cc8;

	size &= 7;

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

	*result = (CharString) { 
		.lenAndNullTerminated = (realSize | ((U64)1 << 63)), 
		.capacityAndRefInfo = realSize + 1, 
		.ptr = (C8*)b.ptr 
	};

	return Error_none();
}

Error CharString_createCopy(CharString str, Allocator alloc, CharString *result) {

	if (!alloc.alloc || !result)
		return Error_nullPointer(!result ? 2 : 1, "CharString_createCopy()::alloc and result are required");

	if (result->ptr)
		return Error_invalidOperation(0, "CharString_createCopy()::result wasn't empty, might indicate a memleak");

	U64 strl = CharString_length(str);

	if (!strl) {
		*result = CharString_createNull();
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

	*result = (CharString) { 
		.ptr = (C8*)b.ptr, 
		.lenAndNullTerminated = (strl | ((U64)1 << 63)), 
		.capacityAndRefInfo = strl + 1 
	};

	return Error_none();
}

Bool CharString_free(CharString *str, Allocator alloc) {

	if (!str)
		return true;

	if(!alloc.free)
		return false;

	Bool freed = true;

	if(!CharString_isRef(*str))
		freed = alloc.free(alloc.ptr, Buffer_createManagedPtr((U8*)str->ptr, str->capacityAndRefInfo));

	*str = CharString_createNull();
	return freed;
}

#define CharString_createNum(maxVal, func, prefixRaw, ...)															\
																													\
	if (!result)																									\
		return Error_nullPointer(3, "CharString_createNum()::result is required");									\
																													\
	CharString prefix = CharString_createConstRefCStr(prefixRaw);													\
																													\
	if (result->ptr)																								\
		return Error_invalidOperation(0, "CharString_createNum()::result wasn't empty, might indicate memleak");	\
																													\
	*result = CharString_createNull();																				\
	Error err = CharString_reserve(																					\
		result, maxVal + CharString_length(prefix) + 1, allocator													\
	);																												\
																													\
	if (err.genericError) 																							\
		return err;																									\
																													\
	err = CharString_appendString(result, prefix, allocator);														\
																													\
	if (err.genericError) {																							\
		CharString_free(result, allocator);																			\
		return err;																									\
	}																												\
																													\
	Bool foundFirstNonZero = false;																					\
																													\
	for (U64 i = maxVal - 1; i != U64_MAX; --i) {																	\
																													\
		C8 c = C8_create##func(__VA_ARGS__);																		\
																													\
		if (!foundFirstNonZero)																						\
			foundFirstNonZero = c != '0' || i < leadingZeros;														\
																													\
		if (foundFirstNonZero) {																					\
																													\
			err = CharString_append(result, c, allocator);															\
																													\
			if (err.genericError) {																					\
				CharString_free(result, allocator);																	\
				return err;																							\
			}																										\
		}																											\
	}																												\
																													\
	/* Ensure we don't return an empty string on 0 */																\
																													\
	if (!v && !foundFirstNonZero) {																					\
																													\
		err = CharString_append(result, '0', allocator);															\
																													\
		if (err.genericError) {																						\
			CharString_free(result, allocator);																		\
			return err;																								\
		}																											\
	}																												\
																													\
	((C8*)result->ptr)[CharString_length(*result)] = '\0';															\
	return Error_none();

Error CharString_createNyto(U64 v, U8 leadingZeros, Allocator allocator, CharString *result){
	CharString_createNum(11, Nyto, "0n", (v >> (6 * i)) & 63);
}

Error CharString_createHex(U64 v, U8 leadingZeros, Allocator allocator, CharString *result) {
	CharString_createNum(16, Hex, "0x", (v >> (4 * i)) & 15);
}

Error CharString_createDec(U64 v, U8 leadingZeros, Allocator allocator, CharString *result) {
	CharString_createNum(20, Dec, "", (v / U64_exp10(i)) % 10);
}

Error CharString_createOct(U64 v, U8 leadingZeros, Allocator allocator, CharString *result) {
	CharString_createNum(22, Oct, "0o", (v >> (3 * i)) & 7);
}

Error CharString_createBin(U64 v, U8 leadingZeros, Allocator allocator, CharString *result) {
	CharString_createNum(64, Bin, "0b", (v >> i) & 1);
}

Error CharString_split(
	CharString s, 
	C8 c, 
	EStringCase casing, 
	Allocator allocator, 
	CharStringList *result
) {

	U64 length = CharString_countAll(s, c, casing);

	Error err = CharStringList_create(length + 1, allocator, result);

	if (err.genericError)
		return err;

	U64 strl = CharString_length(s);

	if (!length) {

		Bool b = CharString_isNullTerminated(s);

		result->ptr[0] = CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr, strl, b) :
			CharString_createRefSized((C8*)s.ptr, strl, b);

		return Error_none();
	}

	CharStringList str = *result;

	c = C8_transform(c, (EStringTransform) casing);

	U64 count = 0, last = 0;

	for (U64 i = 0; i < strl; ++i)
		if (C8_transform(s.ptr[i], (EStringTransform) casing) == c) {

			str.ptr[count++] = 
				CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr + last, i - last, false) : 
				CharString_createRefSized((C8*)s.ptr + last, i - last, false);

			last = i + 1;
		}

	Bool b = CharString_isNullTerminated(s);

	str.ptr[count++] = 
		CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr + last, strl - last, b) : 
		CharString_createRefSized((C8*)s.ptr + last, strl - last, b);

	return Error_none();
}

Error CharString_splitString(
	CharString s, 
	CharString other, 
	EStringCase casing, 
	Allocator allocator, 
	CharStringList *result
) {

	U64 length = CharString_countAllString(s, other, casing);

	Error err = CharStringList_create(length + 1, allocator, result);

	if (err.genericError)
		return err;

	Bool b = CharString_isNullTerminated(s);
	U64 strl = CharString_length(s);

	if (!length) {

		*result->ptr = CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr, strl, b) :
			CharString_createRefSized((C8*)s.ptr, strl, b);

		return Error_none();
	}

	CharStringList str = *result;

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
				CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr + last, i - last, false) : 
				CharString_createRefSized((C8*)s.ptr + last, i - last, false);

			last = i + strl;
			i += strl - 1;
		}
	}

	str.ptr[count++] = 
		CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr + last, strl - last, b) : 
		CharString_createRefSized((C8*)s.ptr + last, strl - last, b);

	return Error_none();
}

Error CharString_splitLine(CharString s, Allocator alloc, CharStringList *result) {

	if(!result)
		return Error_nullPointer(2, "CharString_splitLine()::result is invalid");

	if(result->ptr)
		return Error_invalidParameter(2, 1, "CharString_splitLine()::result wasn't empty, might indicate memleak");

	U64 v = 0, lastLineEnd = U64_MAX;
	U64 strl = CharString_length(s);

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

	Error err = CharStringList_create(v, alloc, result);

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

		result->ptr[v++] = CharString_isConstRef(s) ? 
			CharString_createConstRefSized(s.ptr + lastLineEnd, iOld - lastLineEnd, false) : 
			CharString_createRefSized((C8*)s.ptr + lastLineEnd, iOld - lastLineEnd, false);

		lastLineEnd = i + 1;
	}

	if(lastLineEnd != strl)
		result->ptr[v++] = CharString_isConstRef(s) ? 
			CharString_createConstRefSized(s.ptr + lastLineEnd, strl - lastLineEnd, CharString_isNullTerminated(s)) : 
			CharString_createRefSized((C8*)s.ptr + lastLineEnd, strl - lastLineEnd, CharString_isNullTerminated(s));

	return Error_none();
}

Error CharString_splitSensitive(CharString s, C8 c, Allocator allocator, CharStringList *result) {
	return CharString_split(s, c, EStringCase_Sensitive, allocator, result);
}

Error CharString_splitInsensitive(CharString s, C8 c, Allocator allocator, CharStringList *result) {
	return CharString_split(s, c, EStringCase_Insensitive, allocator, result);
}

Error CharString_splitStringSensitive(CharString s, CharString other, Allocator allocator, CharStringList *result) {
	return CharString_splitString(s, other, EStringCase_Sensitive, allocator, result);
}

Error CharString_splitStringInsensitive(CharString s, CharString other, Allocator allocator, CharStringList *result) {
	return CharString_splitString(s, other, EStringCase_Insensitive, allocator, result);
}

Error CharString_reserve(CharString *str, U64 length, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0, "CharString_reserve()::str is required");

	if (CharString_isRef(*str) && CharString_length(*str))
		return Error_invalidOperation(0, "CharString_reserve()::str has to be managed memory");

	if (length >> 48)
		return Error_invalidOperation(1, "CharString_reserve()::length should be 48-bit");

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(2, "CharString_reserve()::alloc is required");

	if (length + 1 <= str->capacityAndRefInfo)
		return Error_none();

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, length + 1, &b);

	if (err.genericError)
		return err;

	Buffer_copy(b, CharString_bufferConst(*str));

	((C8*)b.ptr)[length] = '\0';
	str->lenAndNullTerminated |= (U64)1 << 63;

	if(str->capacityAndRefInfo)
		err = 
			alloc.free(alloc.ptr, Buffer_createManagedPtr((U8*)str->ptr, str->capacityAndRefInfo)) ? 
			Error_none() : Error_invalidOperation(0, "CharString_reserve() free failed");

	str->capacityAndRefInfo = Buffer_length(b);
	str->ptr = (const C8*) b.ptr;
	return err;
}

Error CharString_resize(CharString *str, U64 length, C8 defaultChar, Allocator alloc) {

	if (!str)
		return Error_nullPointer(0, "CharString_resize()::str is required");

	U64 strl = CharString_length(*str);

	if (CharString_isRef(*str) && strl)
		return Error_invalidOperation(0, "CharString_resize()::str needs to be managed memory");

	if (length >> 48)
		return Error_invalidOperation(1, "CharString_resize()::length should be 48-bit");

	if (!alloc.alloc || !alloc.free)
		return Error_nullPointer(3, "CharString_resize()::alloc is required");

	if (length == strl && CharString_isNullTerminated(*str))
		return Error_none();

	//Simple resize; we cut off the tail

	if (length < strl) {

		Bool b = CharString_isNullTerminated(*str);

		str->lenAndNullTerminated = ((U64)b << 63) | length;

		if(b)
			((C8*)str->ptr)[length] = '\0';

		return Error_none();
	}

	//Resize that triggers buffer resize

	if (length + 1 > str->capacityAndRefInfo) {

		//Reserve 50% more to ensure we don't resize too many times

		Error err = CharString_reserve(str, U64_max(64, length * 3 / 2) + 1, alloc);

		if (err.genericError)
			return err;
	}

	//Our capacity is big enough to handle it:

	for (U64 i = strl; i < length; ++i)
		((C8*)str->ptr)[i] = defaultChar;

	((C8*)str->ptr)[length] = '\0';
	str->lenAndNullTerminated = length | ((U64)1 << 63);
	return Error_none();
}

Error CharString_append(CharString *s, C8 c, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, "CharString_append()::s is required");

	if(!c && CharString_isNullTerminated(*s))
		return Error_none();

	return CharString_resize(s, CharString_length(*s) + (Bool)c, c, allocator);
}

CharString CharString_newLine() { return CharString_createConstRefCStr("\n"); }

Error CharString_appendString(CharString *s, CharString other, Allocator allocator) {

	U64 otherl = CharString_length(other);

	if (!otherl)
		return Error_none();

	other = CharString_createConstRefSized(other.ptr, otherl, CharString_isNullTerminated(other));

	if (!s)
		return Error_nullPointer(0, "CharString_appendString()::s is required");

	U64 oldLen = CharString_length(*s);

	if (CharString_isRef(*s) && oldLen)
		return Error_invalidParameter(0, 0, "CharString_appendString()::s has to be managed memory");

	Error err = CharString_resize(s, oldLen + otherl, other.ptr[0], allocator);

	if (err.genericError)
		return err;

	Buffer_copy(Buffer_createRef((U8*)s->ptr + oldLen, otherl), CharString_bufferConst(other));
	return Error_none();
}

Error CharString_prepend(CharString *s, C8 c, Allocator allocator) {
	return CharString_insert(s, c, 0, allocator);
}

Error CharString_prependString(CharString *s, CharString other, Allocator allocator) {
	return CharString_insertString(s, other, 0, allocator);
}

Error CharString_insert(CharString *s, C8 c, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, "CharString_insert()::s is required");

	U64 strl = CharString_length(*s);

	if (CharString_isRef(*s) && strl)
		return Error_invalidParameter(0, 0, "CharString_insert()::s should be managed memory");

	if(i > strl)
		return Error_outOfBounds(2, i, strl, "CharString_insert()::i is out of bounds");

	if(i == strl && !c && CharString_isNullTerminated(*s))
		return Error_none();

	if(!c && i != strl)
		return Error_invalidOperation(0, "CharString_insert()::c is 0, which isn't allowed if i != strl");

	Error err = CharString_resize(s, strl + 1, c, allocator);

	if (err.genericError)
		return err;

	//If it's not append (otherwise it's already handled)

	if (i != strl - 1) {

		//Move one to the right

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + i + 1,  strl),
			Buffer_createRefConst(s->ptr + i, strl)
		);

		((C8*)s->ptr)[i] = c;
	}

	return Error_none();
}

Error CharString_insertString(CharString *s, CharString other, U64 i, Allocator allocator) {

	if (!s)
		return Error_nullPointer(0, "CharString_insertString()::s is required");

	U64 oldLen = CharString_length(*s);

	if (CharString_isRef(*s) && oldLen)
		return Error_invalidParameter(0, 0, "CharString_insertString()::s should be managed memory");

	U64 otherl = CharString_length(other);

	if (!otherl)
		return Error_none();

	Error err = CharString_resize(s, oldLen + otherl, ' ', allocator);

	if (err.genericError)
		return err;

	//Move one to the right

	Buffer_revCopy(
		Buffer_createRef((U8*)s->ptr + i + otherl, oldLen - i),
		Buffer_createRefConst(s->ptr + i, oldLen - i)
	);

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + i, otherl),
		CharString_bufferConst(other)
	);

	return Error_none();
}

Error CharString_replaceAllString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator
) {

	if (!s)
		return Error_nullPointer(0, "CharString_replaceAllString()::s is required");

	if(CharString_isRef(*s))
		return Error_constData(0, 0, "CharString_replaceAllString()::s must be managed memory");

	ListU64 finds = { 0 };
	Error err = CharString_findAllString(*s, search, allocator, caseSensitive, &finds);

	if(err.genericError)
		return err;

	if (!finds.length)
		return Error_none();

	//Easy replace

	const U64 *ptr = finds.ptr;

	U64 searchl = CharString_length(search);
	U64 replacel = CharString_length(replace);
	U64 strl = CharString_length(*s);

	if (searchl == replacel) {

		for (U64 i = 0; i < finds.length; ++i)
			for (U64 j = ptr[i], k = j + replacel, l = 0; j < k; ++j, ++l)
				((C8*)s->ptr)[j] = replace.ptr[l];

		goto clean;
	}

	//Shrink replaces

	if (searchl > replacel) {

		U64 diff = searchl - replacel;

		U64 iCurrent = ptr[0];

		for (U64 i = 0; i < finds.length; ++i) {

			//We move our replacement string to iCurrent

			Buffer_copy(
				Buffer_createRef((U8*)s->ptr + iCurrent, replacel), 
				CharString_bufferConst(replace)
			);

			iCurrent += replacel;

			//We then move the tail of the string 

			const U64 iStart = ptr[i] + searchl;
			const U64 iEnd = i == finds.length - 1 ? strl : ptr[i + 1];

			Buffer_copy(
				Buffer_createRef((U8*)s->ptr + iCurrent, iEnd - iStart), 
				Buffer_createRefConst(s->ptr + iStart, iEnd - iStart)
			);

			iCurrent += iEnd - iStart;
		}

		//Ensure the string is now the right size

		err = CharString_resize(s, strl - diff * finds.length, ' ', allocator);
		goto clean;
	}

	//Grow replaces

	//Ensure the string is now the right size

	U64 diff = replacel - searchl;

	_gotoIfError(clean, CharString_resize(s, strl + diff * finds.length, ' ', allocator));

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
			Buffer_createRefConst(s->ptr + iStart, iEnd - iStart)
		);

		newLoc -= iEnd - iStart;

		//Apply replacement before tail

		Buffer_revCopy(
			Buffer_createRef((U8*)s->ptr + newLoc - replacel, replacel), 
			CharString_bufferConst(replace)
		);

		newLoc -= replacel;
	}

clean:
	ListU64_free(&finds, allocator);
	return err;
}

Error CharString_replaceString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator,
	Bool isFirst
) {

	if (!s)
		return Error_nullPointer(0, "CharString_replaceString()::s is required");

	if(CharString_isRef(*s))
		return Error_constData(0, 0, "CharString_replaceString()::s must use managed memory");

	U64 res = isFirst ? CharString_findFirstString(*s, search, caseSensitive) : 
		CharString_findLastString(*s, search, caseSensitive);

	if (res == U64_MAX)
		return Error_none();

	U64 searchl = CharString_length(search);
	U64 replacel = CharString_length(replace);
	U64 strl = CharString_length(*s);

	//Easy, early exit. Strings are same size.

	if (searchl == replacel) {
		Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));
		return Error_none();
	}

	//Replacement is smaller than our search
	//So we can just move from left to right
	
	if (replacel < searchl) {

		U64 diff = searchl - replacel;	//How much we have to shrink

		//Copy our data over first

		Buffer_copy(
			Buffer_createRef((U8*)s->ptr + res + replacel, strl - (res + searchl)), 
			Buffer_createRefConst(s->ptr + res + searchl, strl - (res + searchl)) 
		);

		//Throw our replacement in there

		Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));

		//Shrink the string; this is done after because we need to read the right of the string first

		Error err = CharString_resize(s, strl - diff, ' ', allocator);

		if (err.genericError)
			return err;

		return Error_none();
	}

	//Replacement is bigger than our search;
	//We need to grow first and move from right to left

	U64 diff = replacel - searchl;

	Error err = CharString_resize(s, strl + diff, ' ', allocator);

	if (err.genericError)
		return err;

	//Copy our data over first

	Buffer_revCopy(
		Buffer_createRef((U8*)s->ptr + res + replacel, strl - (res + searchl)), 
		Buffer_createRefConst(s->ptr + res + searchl, strl - (res + searchl)) 
	);

	//Throw our replacement in there

	Buffer_copy(Buffer_createRef((U8*)s->ptr + res, replacel), CharString_bufferConst(replace));
	return Error_none();
}

Error CharString_replaceFirstString(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Allocator allocator
) {
	return CharString_replaceString(s, search, replace, caseSensitive, allocator, true);
}

Error CharString_replaceLastString(
	CharString *s, 
	CharString search, 
	CharString replace, 
	EStringCase caseSensitive,
	Allocator allocator
) {
	return CharString_replaceString(s, search, replace, caseSensitive, allocator, false);
}

Error CharString_replaceAllStringSensitive(CharString *s, CharString search, CharString replace, Allocator allocator) {
	return CharString_replaceAllString(s, search, replace, EStringCase_Sensitive, allocator);
}

Error CharString_replaceAllStringInsensitive(CharString *s, CharString search, CharString replace, Allocator allocator) {
	return CharString_replaceAllString(s, search, replace, EStringCase_Insensitive, allocator);
}

Error CharString_replaceStringSensitive(
	CharString *s,
	CharString search,
	CharString replace,
	Allocator allocator,
	Bool isFirst
) {
	return CharString_replaceString(s, search, replace, EStringCase_Sensitive, allocator, isFirst);
}

Error CharString_replaceStringInsensitive(
	CharString *s, 
	CharString search, 
	CharString replace, 
	Allocator allocator, 
	Bool isFirst
) {
	return CharString_replaceString(s, search, replace, EStringCase_Insensitive, allocator, isFirst);
}

Error CharString_replaceFirstStringSensitive(CharString *s, CharString search, CharString replace, Allocator allocator) {
	return CharString_replaceFirstString(s, search, replace, EStringCase_Sensitive, allocator);
}

Error CharString_replaceFirstStringInsensitive(CharString *s, CharString search, CharString replace, Allocator allocator) {
	return CharString_replaceFirstString(s, search, replace, EStringCase_Insensitive, allocator);
}

Error CharString_replaceLastStringSensitive(CharString *s, CharString search, CharString replace, Allocator allocator) {
	return CharString_replaceLastString(s, search, replace, EStringCase_Sensitive, allocator);
}

Error CharString_replaceLastStringInsensitive(CharString *s, CharString search, CharString replace, Allocator allocator) {
	return CharString_replaceLastString(s, search, replace, EStringCase_Insensitive, allocator);
}

Bool CharString_startsWith(CharString str, C8 c, EStringCase caseSensitive) { 
	return 
		CharString_length(str) && str.ptr && 
		C8_transform(*str.ptr, (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool CharString_endsWith(CharString str, C8 c, EStringCase caseSensitive) {
	return 
		CharString_length(str) && str.ptr && 
		C8_transform(str.ptr[CharString_length(str) - 1], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool CharString_startsWithString(CharString str, CharString other, EStringCase caseSensitive) {

	U64 otherl = CharString_length(other);
	U64 strl = CharString_length(str);

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

Bool CharString_endsWithString(CharString str, CharString other, EStringCase caseSensitive) {

	U64 otherl = CharString_length(other);
	U64 strl = CharString_length(str);

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

U64 CharString_countAll(CharString s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	U64 count = 0;

	for (U64 i = 0; i < CharString_length(s); ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			++count;

	return count;
}

U64 CharString_countAllString(CharString s, CharString other, EStringCase casing) {

	U64 otherl = CharString_length(other);
	U64 strl = CharString_length(s);

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

Bool CharString_startsWithSensitive(CharString str, C8 c) { return CharString_startsWith(str, c, EStringCase_Sensitive); }
Bool CharString_startsWithInsensitive(CharString str, C8 c) { return CharString_startsWith(str, c, EStringCase_Insensitive); }

Bool CharString_startsWithStringSensitive(CharString str, CharString s) {
	return CharString_startsWithString(str, s, EStringCase_Sensitive);
}

Bool CharString_startsWithStringInsensitive(CharString str, CharString s) {
	return CharString_startsWithString(str, s, EStringCase_Insensitive);
}

Bool CharString_endsWithSensitive(CharString str, C8 c) { return CharString_endsWith(str, c, EStringCase_Sensitive); }
Bool CharString_endsWithInsensitive(CharString str, C8 c) { return CharString_endsWith(str, c, EStringCase_Insensitive); }

Bool CharString_endsWithStringSensitive(CharString str, CharString s) {
	return CharString_endsWithString(str, s, EStringCase_Sensitive);
}

Bool CharString_endsWithStringInsensitive(CharString str, CharString s) {
	return CharString_endsWithString(str, s, EStringCase_Insensitive);
}

U64 CharString_countAllSensitive(CharString s, C8 c) { return CharString_countAll(s, c, EStringCase_Sensitive); }
U64 CharString_countAllInsensitive(CharString s, C8 c) { return CharString_countAll(s, c, EStringCase_Insensitive); }

U64 CharString_countAllStringSensitive(CharString str, CharString s) {
	return CharString_countAllString(str, s, EStringCase_Sensitive);
}

U64 CharString_countAllStringInsensitive(CharString str, CharString s) {
	return CharString_countAllString(str, s, EStringCase_Insensitive);
}

Error CharString_findAll(
	CharString s,
	C8 c,
	Allocator alloc,
	EStringCase caseSensitive,
	ListU64 *result
) {

	if(!result)
		return Error_nullPointer(4, "CharString_findAll()::result is required");

	if(result->ptr)
		return Error_invalidParameter(4, 0, "CharString_findAll()::result wasn't empty, might indicate memleak");

	U64 strl = CharString_length(s);

	ListU64 l = (ListU64) { 0 };
	Error err = ListU64_reserve(&l, strl / 25 + 16, alloc);

	if (err.genericError)
		return err;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	for (U64 i = 0; i < strl; ++i)
		if (c == C8_transform(s.ptr[i], (EStringTransform)caseSensitive))
			if ((err = ListU64_pushBack(&l, i, alloc)).genericError) {
				ListU64_free(&l, alloc);
				return err;
			}

	*result = l;
	return Error_none();
}

Error CharString_findAllString(
	CharString s,
	CharString other,
	Allocator alloc,
	EStringCase casing,
	ListU64 *result
) {

	if(!result)
		return Error_nullPointer(4, "CharString_findAllString()::result is required");

	if(result->ptr)
		return Error_invalidParameter(4, 0, "CharString_findAllString()::result wasn't empty, might indicate memleak");

	U64 otherl = CharString_length(other);
	U64 strl = CharString_length(s);

	if(!otherl)
		return Error_invalidParameter(1, 0, "CharString_findAllString()::other is empty");

	ListU64 l = (ListU64) { 0 };

	if(strl < otherl) {
		*result = l;
		return Error_none();
	}

	Error err = ListU64_reserve(&l, strl / otherl / 25 + 16, alloc);

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

			if ((err = ListU64_pushBack(&l, i, alloc)).genericError) {
				ListU64_free(&l, alloc);
				return err;
			}

			i += strl - 1;
		}
	}

	*result = l;
	return Error_none();
}

U64 CharString_findFirst(CharString s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = 0; i < CharString_length(s); ++i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return U64_MAX;
}

U64 CharString_findLast(CharString s, C8 c, EStringCase caseSensitive) {

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for (U64 i = CharString_length(s) - 1; i != U64_MAX; --i)
		if (C8_transform(s.ptr[i], (EStringTransform)caseSensitive) == c)
			return i;

	return U64_MAX;
}

U64 CharString_findFirstString(CharString s, CharString other, EStringCase casing) {

	U64 otherl = CharString_length(other);
	U64 strl = CharString_length(s);

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

U64 CharString_findLastString(CharString s, CharString other, EStringCase casing) {
	
	U64 strl = CharString_length(s);
	U64 otherl = CharString_length(other);

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

Bool CharString_equalsString(CharString s, CharString other, EStringCase caseSensitive) {

	U64 strl = CharString_length(s);
	U64 otherl = CharString_length(other);

	if (strl != otherl)
		return false;

	if (caseSensitive == EStringCase_Sensitive)
		return Buffer_eq(CharString_bufferConst(s), CharString_bufferConst(other));

	for (U64 i = 0; i < strl; ++i)
		if (C8_toLower(s.ptr[i]) != C8_toLower(other.ptr[i]))
			return false;

	return true;
}

Bool CharString_equals(CharString s, C8 c, EStringCase caseSensitive) {

	return CharString_length(s) == 1 && s.ptr && 
		C8_transform(s.ptr[0], (EStringTransform) caseSensitive) == 
		C8_transform(c, (EStringTransform) caseSensitive);
}

Bool CharString_equalsSensitive(CharString s, C8 c) { return CharString_equals(s, c, EStringCase_Sensitive); }
Bool CharString_equalsInsensitive(CharString s, C8 c) { return CharString_equals(s, c, EStringCase_Insensitive); }

Bool CharString_equalsStringSensitive(CharString s, CharString c) { 
	return CharString_equalsString(s, c, EStringCase_Sensitive); 
}

Bool CharString_equalsStringInsensitive(CharString s, CharString c) { 
	return CharString_equalsString(s, c, EStringCase_Insensitive); 
}

Bool CharString_parseNyto(CharString s, U64 *result){

	CharString prefix = CharString_createConstRefCStr("0n");

	Error err = CharString_offsetAsRef(
		s, CharString_startsWithStringInsensitive(s, prefix) ? CharString_length(prefix) : 0, &s
	);

	U64 strl = CharString_length(s);

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

Bool CharString_parseHex(CharString s, U64 *result){

	CharString prefix = CharString_createConstRefCStr("0x");
	Error err = CharString_offsetAsRef(
		s, CharString_startsWithStringInsensitive(s, prefix) ? CharString_length(prefix) : 0, &s
	);

	if (err.genericError)
		return false;

	U64 strl = CharString_length(s);

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

Bool CharString_parseDec(CharString s, U64 *result) {

	U64 strl = CharString_length(s);

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

Bool CharString_parseDecSigned(CharString s, I64 *result) {

	Bool neg = CharString_startsWithSensitive(s, '-');

	Error err = CharString_offsetAsRef(s, neg, &s);

	if (err.genericError)
		return false;

	Bool b = CharString_parseDec(s, (U64*) result);

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

Bool CharString_parseDouble(CharString s, F64 *result) {

	if (!result)
		return false;

	//Parse sign

	Bool sign = false;

	if (CharString_startsWithSensitive(s, '-')) {

		sign = true;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (CharString_startsWithSensitive(s, '+')) {

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	//Parse integer part

	F64 intPart = 0;

	while(
		!CharString_startsWithSensitive(s, '.') && 
		!CharString_startsWithInsensitive(s, 'e')
	) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		intPart = intPart * 10 + v;

		if(!F64_isValid(intPart))
			return false;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;

		if (!CharString_length(s)) {
			*result = sign ? -intPart : intPart;
			return true;
		}
	}

	//Parse fraction

	if (
		CharString_startsWithSensitive(s, '.') && 
		CharString_offsetAsRef(s, 1, &s).genericError
	)
		return false;

	F64 fraction = 0, multiplier = 0.1;

	while(!CharString_startsWithInsensitive(s, 'e')) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		fraction += v * multiplier;

		if(!F64_isValid(fraction))
			return false;

		multiplier *= 0.1;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;

		if (!CharString_length(s)) {
			*result = sign ? -(intPart + fraction) : intPart + fraction;
			return F64_isValid(*result);
		}
	}

	//Parse exponent sign

	if (CharString_offsetAsRef(s, 1, &s).genericError)
		return false;

	Bool esign = false;

	if (CharString_startsWithSensitive(s, '-')) {

		esign = true;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (CharString_startsWithSensitive(s, '+')) {

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	//Parse exponent (must be int)

	F64 exponent = 0;

	while(CharString_length(s)) {

		U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX)
			return false;

		exponent = exponent * 10 + v;

		if(!F64_isValid(fraction))
			return false;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	Error err = F64_exp10(esign ? -exponent : exponent, &exponent);

	if(err.genericError)
		return false;

	F64 res = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;

	if(!F64_isValid(res))
		return false;

	*result = res;
	return true;
}

Bool CharString_parseFloat(CharString s, F32 *result) {

	F64 dub = 0;
	if(!CharString_parseDouble(s, &dub))
		return false;

	if(!F32_isValid((F32)dub))
		return false;

	*result = (F32)dub;
	return true;
}

Bool CharString_parseOct(CharString s, U64 *result) {

	CharString prefix = CharString_createConstRefCStr("0o");
	Error err = CharString_offsetAsRef(
		s, CharString_startsWithStringInsensitive(s, prefix) ? CharString_length(prefix) : 0, &s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || CharString_length(s) > 22)
		return false;

	*result = 0;

	for (U64 i = CharString_length(s) - 1, j = 0; i != U64_MAX; --i, j += 3) {

		U8 v = C8_oct(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64)1 << (21 * 3)) && v > 1)		//Out of value
			return false;

		*result |= (U64)v << j;
	}

	return true;
}

Bool CharString_parseBin(CharString s, U64 *result) {

	CharString prefix = CharString_createConstRefCStr("0b");
	Error err = CharString_offsetAsRef(
		s, 
		CharString_startsWithStringInsensitive(s, prefix) ? 
		CharString_length(prefix) : 0, 
		&s
	);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || CharString_length(s) > 64)
		return false;

	*result = 0;

	for (U64 i = CharString_length(s) - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

		U8 v = C8_bin(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(v)
			*result |= j;
	}

	return true;
}

Bool CharString_parseU64(CharString s, U64 *result) {

	if (CharString_startsWithSensitive(s, '0'))
		switch (CharString_getAt(s, 1)) {
			case 'B':	case 'b':	return CharString_parseBin(s, result);
			case 'O':	case 'o':	return CharString_parseOct(s, result);
			case 'X':	case 'x':	return CharString_parseHex(s, result);
			case 'N':	case 'n':	return CharString_parseNyto(s, result);
		}

	return CharString_parseDec(s, result);
}

Bool CharString_cut(CharString s, U64 offset, U64 length, CharString *result) {

	if(!result || result->ptr)
		return false;

	U64 strl = CharString_length(s);

	if (!strl && !offset && !length) {
		*result = CharString_createNull();
		return true;
	}

	if(offset >= strl)
		return false;

	if(!length) 
		length = strl - offset;

	if (offset + length > strl)
		return false;

	if (offset == strl) {
		*result = CharString_createNull();
		return false;
	}

	Bool isNullTerm = CharString_isNullTerminated(s) && offset + length == strl;

	*result = CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr + offset, length, isNullTerm) :
		CharString_createRefSized((C8*)s.ptr + offset, length, isNullTerm);

	return true;
}

Bool CharString_cutAfter(
	CharString s, 
	C8 c, 
	EStringCase caseSensitive, 
	Bool isFirst, 
	CharString *result
) {

	U64 found = isFirst ? CharString_findFirst(s, c, caseSensitive) : CharString_findLast(s, c, caseSensitive);

	if (found == U64_MAX)
		return false;

	return CharString_cut(s, 0, found, result);
}

Bool CharString_cutAfterString(
	CharString s, 
	CharString other, 
	EStringCase caseSensitive, 
	Bool isFirst, 
	CharString *result
) {

	U64 found = isFirst ? CharString_findFirstString(s, other, caseSensitive) : 
		CharString_findLastString(s, other, caseSensitive);

	if (found == U64_MAX)
		return false;

	return CharString_cut(s, 0, found, result);
}

Bool CharString_cutBefore(CharString s, C8 c, EStringCase caseSensitive, Bool isFirst, CharString *result) {

	if (!result)
		return false;

	U64 found = isFirst ? CharString_findFirst(s, c, caseSensitive) : CharString_findLast(s, c, caseSensitive);

	if (found == U64_MAX)
		return false;

	++found;	//The end of the occurence is the begin of the next string
	return CharString_cut(s, found, 0, result);
}

Bool CharString_cutBeforeString(CharString s, CharString other, EStringCase caseSensitive, Bool isFirst, CharString *result) {

	U64 found = isFirst ? CharString_findFirstString(s, other, caseSensitive) : 
		CharString_findLastString(s, other, caseSensitive);

	if (found == U64_MAX)
		return false;

	found += CharString_length(other);	//The end of the occurence is the begin of the next string
	return CharString_cut(s, found, 0, result);
}

Bool CharString_erase(CharString *s, C8 c, EStringCase caseSensitive, Bool isFirst) {

	if(!s || CharString_isRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform) caseSensitive);

	//Skipping first match

	U64 find = CharString_find(*s, c, caseSensitive, isFirst);

	if(find == U64_MAX)
		return false;

	U64 strl = CharString_length(*s);

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + find, strl - find - 1),
		Buffer_createRefConst(s->ptr + find + 1, strl - find - 1)
	);

	((C8*)s->ptr)[strl - 1] = '\0';
	s->lenAndNullTerminated = (strl - 1) | ((U64)1 << 63);

	return true;
}

Error CharString_eraseAtCount(CharString *s, U64 i, U64 count) {

	if(!s)
		return Error_nullPointer(0, "CharString_eraseAtCount()::s is required");

	if(!count)
		return Error_none();

	if(CharString_isRef(*s))
		return Error_constData(0, 0, "CharString_eraseAtCount()::s should be managed memory");

	U64 strl = CharString_length(*s);

	if(i + count > strl)
		return Error_outOfBounds(0, i + count, strl, "CharString_eraseAtCount()::i + count is out of bounds");

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + i, strl - i - count),
		Buffer_createRefConst(s->ptr + i + count, strl - i - count)
	);

	U64 end = strl - count;
	((C8*)s->ptr)[end] = '\0';
	s->lenAndNullTerminated = end | ((U64)1 << 63);
	return Error_none();
}

Bool CharString_eraseString(CharString *s, CharString other, EStringCase casing, Bool isFirst) {

	U64 otherl = CharString_length(other);

	if(!s || !otherl || CharString_isRef(*s))
		return false;

	U64 strl = CharString_length(*s);

	//Skipping first match

	U64 find = CharString_findString(*s, other, casing, isFirst);

	if(find == U64_MAX)
		return false;

	Buffer_copy(
		Buffer_createRef((U8*)s->ptr + find, strl - find - otherl),
		Buffer_createRefConst(s->ptr + find + otherl, strl - find - otherl)
	);

	U64 end = strl - otherl;
	((C8*)s->ptr)[end] = '\0';
	s->lenAndNullTerminated = end | ((U64)1 << 63);
	return true;
}

//CharString's inline changes (no copy)

Bool CharString_cutEnd(CharString s, U64 i, CharString *result) {
	return CharString_cut(s, 0, i, result);
}

Bool CharString_cutBegin(CharString s, U64 i, CharString *result) {

	if (i > CharString_length(s))
		return false;

	return CharString_cut(s, i, 0, result);
}

Bool CharString_eraseAll(CharString *s, C8 c, EStringCase casing) {

	if(!s || CharString_isRef(*s))
		return false;

	U64 strl = CharString_length(*s);

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
	s->lenAndNullTerminated = out | ((U64)1 << 63);
	return true;
}

Bool CharString_eraseAllString(CharString *s, CharString other, EStringCase casing) {

	U64 otherl = CharString_length(other);

	if(!s || !otherl || CharString_isRef(*s))
		return false;

	U64 strl = CharString_length(*s);

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
	s->lenAndNullTerminated = out | ((U64)1 << 63);
	return true;
}

Bool CharString_replaceAll(CharString *s, C8 c, C8 v, EStringCase caseSensitive) {

	if (!s || CharString_isConstRef(*s))
		return false;

	c = C8_transform(c, (EStringTransform)caseSensitive);

	for(U64 i = 0; i < CharString_length(*s); ++i)
		if(C8_transform(s->ptr[i], (EStringTransform)caseSensitive) == c)
			((C8*)s->ptr)[i] = v;

	return true;
}

Bool CharString_replace(CharString *s, C8 c, C8 v, EStringCase caseSensitive, Bool isFirst) {

	if (!s || CharString_isRef(*s))
		return false;

	U64 i = isFirst ? CharString_findFirst(*s, c, caseSensitive) : CharString_findLast(*s, c, caseSensitive);

	if(i != U64_MAX)
		((C8*)s->ptr)[i] = v;

	return true;
}

Bool CharString_transform(CharString str, EStringTransform c) {

	if(CharString_isConstRef(str))
		return false;

	for(U64 i = 0; i < CharString_length(str); ++i)
		((C8*)str.ptr)[i] = C8_transform(str.ptr[i], c);

	return true;
}

CharString CharString_trim(CharString s) {

	U64 strl = CharString_length(s);
	U64 first = strl;

	for (U64 i = 0; i < strl; ++i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			first = i;
			break;
		}

	if (first == strl)
		return CharString_createNull();

	U64 last = strl;

	for (U64 i = strl - 1; i != U64_MAX; --i) 
		if (!C8_isWhitespace(s.ptr[i])) {
			last = i;
			break;
		}

	++last;		//We wanna include the last character that's non whitespace too

	Bool isNullTerm = CharString_isNullTerminated(s) && last == strl;
	
	return CharString_isConstRef(s) ? CharString_createConstRefSized(s.ptr + first, last - first, isNullTerm) :
		CharString_createRefSized((C8*)s.ptr + first, last - first, isNullTerm);
}

//CharStringList

Error CharStringList_create(U64 length, Allocator alloc, CharStringList *arr) {

	if (!arr)
		return Error_nullPointer(2, "CharStringList_create()::arr is required");

	if (arr->ptr)
		return Error_invalidOperation(0, "CharStringList_create()::arr isn't empty, might indicate memleak");

	CharStringList sl = (CharStringList) { .length = length };

	Buffer b = Buffer_createNull();
	Error err = alloc.alloc(alloc.ptr, length * sizeof(CharString), &b);

	sl.ptr = (CharString*) b.ptr;

	if(err.genericError)
		return err;

	for(U64 i = 0; i < sl.length; ++i)
		sl.ptr[i] = CharString_createNull();

	*arr = sl;
	return Error_none();
}

Bool CharStringList_free(CharStringList *arr, Allocator alloc) {

	if(!arr || !arr->length)
		return true;

	Bool freed = true;

	for(U64 i = 0; i < arr->length; ++i) {
		CharString *str = arr->ptr + i;
		freed &= CharString_free(str, alloc);
	}

	freed &= alloc.free(alloc.ptr, Buffer_createManagedPtr(
		arr->ptr,
		sizeof(CharString) * arr->length
	));

	*arr = (CharStringList) { 0 };
	return freed;
}

Error CharStringList_createCopy(CharStringList toCopy, Allocator alloc, CharStringList *arr) {

	if(!toCopy.length)
		return Error_nullPointer(0, "CharStringList_createCopy()::toCopy.length can't be 0");

	Error err = CharStringList_create(toCopy.length, alloc, arr);

	if (err.genericError)
		return err;

	for (U64 i = 0; i < toCopy.length; ++i) {

		err = CharString_createCopy(toCopy.ptr[i], alloc, arr->ptr + i);

		if (err.genericError) {
			CharStringList_free(arr, alloc);
			return err;
		}
	}

	return Error_none();
}

Error CharStringList_unset(CharStringList arr, U64 i, Allocator alloc) {

	if(i >= arr.length)
		return Error_outOfBounds(1, i, arr.length, "CharStringList_unset()::i out of bounds");

	CharString *pstr = arr.ptr + i;
	CharString_free(pstr, alloc);
	return Error_none();
}

Error CharStringList_set(CharStringList arr, U64 i, CharString str, Allocator alloc) {

	Error err = CharStringList_unset(arr, i, alloc);

	if(err.genericError)
		return err;

	arr.ptr[i] = str;
	return Error_none();
}

U64 CharString_calcStrLen(const C8 *ptr, U64 maxSize) {

	U64 i = 0;

	if(ptr)
		for(; i < maxSize && ptr[i]; ++i)
			;

	return i;
}

/*U64 CharString_hash(CharString s) {			//TODO: FNV for <10 and xxh64 for >

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

CharString CharString_getFilePath(CharString *str) {

	if(!CharString_formatPath(str))
		return CharString_createNull();

	CharString res = CharString_createNull();
	CharString_cutBefore(*str, '/', EStringCase_Sensitive, false, &res);
	return res;
}

CharString CharString_getBasePath(CharString *str) {

	if(!CharString_formatPath(str))
		return CharString_createNull();

	CharString res = CharString_createNull();
	CharString_cutAfter(*str, '/', EStringCase_Sensitive, false, &res);
	return res;
}

Error CharStringList_concat(CharStringList arr, C8 between, Allocator alloc, CharString *result) {

	U64 length = 0;

	for(U64 i = 0; i < arr.length; ++i)
		length += CharString_length(arr.ptr[i]);

	if(arr.length > 1)
		length += arr.length - 1;

	Error err = CharString_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0, l = CharString_length(arr.ptr[i]); k < l; ++k, ++j)
			((C8*)result->ptr)[j] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			((C8*)result->ptr)[j++] = between;
	}

	return Error_none();
}

Error CharStringList_concatString(CharStringList arr, CharString between, Allocator alloc, CharString *result) {

	U64 length = 0;

	for(U64 i = 0; i < arr.length; ++i)
		length += CharString_length(arr.ptr[i]);

	if(arr.length > 1)
		length += (arr.length - 1) * CharString_length(between);

	Error err = CharString_create(' ', length, alloc, result);

	if(err.genericError)
		return err;

	for(U64 i = 0, j = 0; i < arr.length; ++i) {

		for(U64 k = 0, l = CharString_length(arr.ptr[i]); k < l; ++k, ++j)
			((C8*)result->ptr)[j] = arr.ptr[i].ptr[k];

		if (i != arr.length - 1)
			for(U64 k = 0; k < CharString_length(between); ++k, ++j)
				((C8*)result->ptr)[j] = between.ptr[k];
	}

	return Error_none();
}

Error CharStringList_combine(CharStringList arr, Allocator alloc, CharString *result) {
	return CharStringList_concatString(arr, CharString_createNull(), alloc, result);
}

U64 CharString_find(CharString s, C8 c, EStringCase caseSensitive, Bool isFirst) {
	return isFirst ? CharString_findFirst(s, c, caseSensitive) : CharString_findLast(s, c, caseSensitive);
}

U64 CharString_findString(CharString s, CharString other, EStringCase caseSensitive, Bool isFirst) {
	return isFirst ? CharString_findFirstString(s, other, caseSensitive) : 
		CharString_findLastString(s, other, caseSensitive);
}

Error CharString_findAllSensitive(CharString s, C8 c, Allocator alloc, ListU64 *result) {
	return CharString_findAll(s, c, alloc, EStringCase_Sensitive, result);
}

Error CharString_findAllInsensitive(CharString s, C8 c, Allocator alloc, ListU64 *result) {
	return CharString_findAll(s, c, alloc, EStringCase_Insensitive, result);
}

Error CharString_findAllStringSensitive(CharString s, CharString other, Allocator alloc, ListU64 *result) {
	return CharString_findAllString(s, other, alloc, EStringCase_Sensitive, result);
}

Error CharString_findAllStringInsensitive(CharString s, CharString other, Allocator alloc, ListU64 *result) {
	return CharString_findAllString(s, other, alloc, EStringCase_Insensitive, result);
}

U64 CharString_findFirstSensitive(CharString s, C8 c) { return CharString_findFirst(s, c, EStringCase_Sensitive); }
U64 CharString_findFirstInsensitive(CharString s, C8 c) { return CharString_findFirst(s, c, EStringCase_Insensitive); }

U64 CharString_findLastSensitive(CharString s, C8 c) { return CharString_findLast(s, c, EStringCase_Sensitive); }
U64 CharString_findLastInsensitive(CharString s, C8 c) { return CharString_findLast(s, c, EStringCase_Insensitive); }

U64 CharString_findSensitive(CharString s, C8 c, Bool isFirst) { 
	return CharString_find(s, c, EStringCase_Sensitive, isFirst);
}

U64 CharString_findInsensitive(CharString s, C8 c, Bool isFirst) { 
	return CharString_find(s, c, EStringCase_Insensitive, isFirst);
}

U64 CharString_findFirstStringSensitive(CharString s, CharString other) { 
	return CharString_findFirstString(s, other, EStringCase_Sensitive); 
}

U64 CharString_findFirstStringInsensitive(CharString s, CharString other) { 
	return CharString_findFirstString(s, other, EStringCase_Insensitive); 
}

U64 CharString_findLastStringSensitive(CharString s, CharString other) { 
	return CharString_findLastString(s, other, EStringCase_Sensitive); 
}

U64 CharString_findLastStringInsensitive(CharString s, CharString other) { 
	return CharString_findLastString(s, other, EStringCase_Insensitive); 
}

U64 CharString_findStringSensitive(CharString s, CharString other, Bool isFirst) { 
	return CharString_findString(s, other, EStringCase_Sensitive, isFirst);
}

U64 CharString_findStringInsensitive(CharString s, CharString other, Bool isFirst) { 
	return CharString_findString(s, other, EStringCase_Insensitive, isFirst);
}

Bool CharString_contains(CharString str, C8 c, EStringCase caseSensitive) { 
	return CharString_findFirst(str, c, caseSensitive) != U64_MAX;
}

Bool CharString_containsString(CharString str, CharString other, EStringCase caseSensitive) { 
	return CharString_findFirstString(str, other, caseSensitive) != U64_MAX; 
}

Bool CharString_containsSensitive(CharString str, C8 c) { return CharString_contains(str, c, EStringCase_Sensitive); }
Bool CharString_containsInsensitive(CharString str, C8 c) { return CharString_contains(str, c, EStringCase_Insensitive); }

Bool CharString_containsStringSensitive(CharString str, CharString other) {
	return CharString_containsString(str, other, EStringCase_Sensitive);
}

Bool CharString_containsStringInsensitive(CharString str, CharString other) {
	return CharString_containsString(str, other, EStringCase_Insensitive);
}

Bool CharString_cutAfterLast(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfter(s, c, caseSensitive, false, result);
}

Bool CharString_cutAfterFirst(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfter(s, c, caseSensitive, true, result);
}

Bool CharString_cutAfterLastString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfterString(s, other, caseSensitive, false, result);
}

Bool CharString_cutAfterFirstString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutAfterString(s, other, caseSensitive, true, result);
}

Bool CharString_cutBeforeLast(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBefore(s, c, caseSensitive, false, result);
}

Bool CharString_cutBeforeFirst(CharString s, C8 c, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBefore(s, c, caseSensitive, true, result);
}

Bool CharString_cutBeforeLastString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBeforeString(s, other, caseSensitive, false, result);
}

Bool CharString_cutBeforeFirstString(CharString s, CharString other, EStringCase caseSensitive, CharString *result) {
	return CharString_cutBeforeString(s, other, caseSensitive, true, result);
}


Bool CharString_cutAfterSensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutAfter(s, c, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutAfterInsensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutAfter(s, c, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutAfterLastSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterLast(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterLastInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterLast(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutAfterFirstSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterFirst(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterFirstInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutAfterFirst(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutAfterStringSensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutAfterString(s, other, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutAfterStringInsensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutAfterString(s, other, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutAfterFirstStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterFirstString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterFirstStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterFirstString(s, other, EStringCase_Insensitive, result);
}

Bool CharString_cutAfterLastStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterLastString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutAfterLastStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutAfterLastString(s, other, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeSensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutBefore(s, c, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutBeforeInsensitive(CharString s, C8 c, Bool isFirst, CharString *result) {
	return CharString_cutBefore(s, c, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutBeforeLastSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeLast(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeLastInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeLast(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeFirstSensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeFirst(s, c, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeFirstInsensitive(CharString s, C8 c, CharString *result) {
	return CharString_cutBeforeFirst(s, c, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeStringSensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutBeforeString(s, other, EStringCase_Sensitive, isFirst, result);
}

Bool CharString_cutBeforeStringInsensitive(CharString s, CharString other, Bool isFirst, CharString *result) {
	return CharString_cutBeforeString(s, other, EStringCase_Insensitive, isFirst, result);
}

Bool CharString_cutBeforeFirstStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeFirstString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeFirstStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeFirstString(s, other, EStringCase_Insensitive, result);
}

Bool CharString_cutBeforeLastStringSensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeLastString(s, other, EStringCase_Sensitive, result);
}

Bool CharString_cutBeforeLastStringInsensitive(CharString s, CharString other, CharString *result) {
	return CharString_cutBeforeLastString(s, other, EStringCase_Insensitive, result);
}

Error CharString_popEndCount(CharString *s, U64 count) { 
	return CharString_eraseAtCount(s, s ? CharString_length(*s) : 0, count); 
}

Error CharString_popFrontCount(CharString *s, U64 count) { return CharString_eraseAtCount(s, 0, count); }

Error CharString_eraseAt(CharString *s, U64 i) { return CharString_eraseAtCount(s, i, 1); }
Error CharString_popFront(CharString *s) { return CharString_eraseAt(s, 0); }
Error CharString_popEnd(CharString *s) { return CharString_eraseAt(s, s ? CharString_length(*s) : 0); }

Bool CharString_eraseFirst(CharString *s, C8 c, EStringCase caseSensitive) {
	return CharString_erase(s, c, caseSensitive, true);
}

Bool CharString_eraseLast(CharString *s, C8 c, EStringCase caseSensitive) {
	return CharString_erase(s, c, caseSensitive, false);
}

Bool CharString_eraseFirstString(CharString *s, CharString other, EStringCase caseSensitive){
	return CharString_eraseString(s, other, caseSensitive, true);
}

Bool CharString_eraseLastString(CharString *s, CharString other, EStringCase caseSensitive) {
	return CharString_eraseString(s, other, caseSensitive, false);
}

Bool CharString_eraseAllSensitive(CharString *s, C8 c) { return CharString_eraseAll(s, c, EStringCase_Sensitive); }
Bool CharString_eraseAllInsensitive(CharString *s, C8 c) { return CharString_eraseAll(s, c, EStringCase_Insensitive); }

Bool CharString_eraseSensitive(CharString *s, C8 c, Bool isFirst) { 
	return CharString_erase(s, c, EStringCase_Sensitive, isFirst);
}

Bool CharString_eraseInsensitive(CharString *s, C8 c, Bool isFirst) { 
	return CharString_erase(s, c, EStringCase_Insensitive, isFirst);
}

Bool CharString_eraseFirstSensitive(CharString *s, C8 c) { return CharString_eraseFirst(s, c, EStringCase_Sensitive); }
Bool CharString_eraseFirstInsensitive(CharString *s, C8 c) { return CharString_eraseFirst(s, c, EStringCase_Insensitive); }

Bool CharString_eraseLastSensitive(CharString *s, C8 c) { return CharString_eraseLast(s, c, EStringCase_Sensitive); }
Bool CharString_eraseLastInsensitive(CharString *s, C8 c) { return CharString_eraseLast(s, c, EStringCase_Insensitive); }

Bool CharString_eraseAllStringSensitive(CharString *s, CharString other) { 
	return CharString_eraseAllString(s, other, EStringCase_Sensitive);
}

Bool CharString_eraseAllStringInsensitive(CharString *s, CharString other) { 
	return CharString_eraseAllString(s, other, EStringCase_Insensitive);
}

Bool CharString_eraseStringSensitive(CharString *s, CharString other, Bool isFirst) { 
	return CharString_eraseString(s, other, EStringCase_Sensitive, isFirst);
}

Bool CharString_eraseStringInsensitive(CharString *s, CharString other, Bool isFirst) { 
	return CharString_eraseString(s, other, EStringCase_Insensitive, isFirst);
}

Bool CharString_eraseFirstStringSensitive(CharString *s, CharString other) { 
	return CharString_eraseFirstString(s, other, EStringCase_Sensitive);
}

Bool CharString_eraseFirstStringInsensitive(CharString *s, CharString other) { 
	return CharString_eraseFirstString(s, other, EStringCase_Insensitive);
}

Bool CharString_eraseLastStringSensitive(CharString *s, CharString other) { 
	return CharString_eraseLastString(s, other, EStringCase_Sensitive);
}

Bool CharString_eraseLastStringInsensitive(CharString *s, CharString other) { 
	return CharString_eraseLastString(s, other, EStringCase_Insensitive);
}

Bool CharString_replaceFirst(CharString *s, C8 c, C8 v, EStringCase caseSensitive) {
	return CharString_replace(s, c, v, caseSensitive, true);
}

Bool CharString_replaceLast(CharString *s, C8 c, C8 v, EStringCase caseSensitive) {
	return CharString_replace(s, c, v, caseSensitive, false);
}

Bool CharString_replaceAllSensitive(CharString *s, C8 c, C8 v) {
	return CharString_replaceAll(s, c, v, EStringCase_Sensitive);
}

Bool CharString_replaceAllInsensitive(CharString *s, C8 c, C8 v) {
	return CharString_replaceAll(s, c, v, EStringCase_Insensitive);
}

Bool CharString_replaceSensitive(CharString *s, C8 c, C8 v, Bool isFirst) {
	return CharString_replace(s, c, v, EStringCase_Sensitive, isFirst);
}

Bool CharString_replaceInsensitive(CharString *s, C8 c, C8 v, Bool isFirst) {
	return CharString_replace(s, c, v, EStringCase_Insensitive, isFirst);
}

Bool CharString_replaceFirstSensitive(CharString *s, C8 c, C8 v) {
	return CharString_replaceFirst(s, c, v, EStringCase_Sensitive);
}

Bool CharString_replaceFirstInsensitive(CharString *s, C8 c, C8 v) {
	return CharString_replaceFirst(s, c, v, EStringCase_Insensitive);
}

Bool CharString_replaceLastSensitive(CharString *s, C8 c, C8 v) {
	return CharString_replaceLast(s, c, v, EStringCase_Sensitive);
}

Bool CharString_replaceLastInsensitive(CharString *s, C8 c, C8 v) {
	return CharString_replaceLast(s, c, v, EStringCase_Insensitive);
}

Bool CharString_toLower(CharString str) { return CharString_transform(str, EStringTransform_Lower); }
Bool CharString_toUpper(CharString str) { return CharString_transform(str, EStringTransform_Upper); }

//Simple file utils

Bool CharString_formatPath(CharString *str) { 
	return CharString_replaceAllSensitive(str, '\\', '/');
}

ECompareResult CharString_compare(CharString a, CharString b, EStringCase caseSensitive) {

	U64 al = CharString_length(a);
	U64 bl = CharString_length(b);

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

ECompareResult CharString_compareSensitive(CharString a, CharString b) {
	return CharString_compare(a, b, EStringCase_Sensitive);
}

ECompareResult CharString_compareInsensitive(CharString a, CharString b) {
	return CharString_compare(a, b, EStringCase_Insensitive);
}

//Format
//https://stackoverflow.com/questions/56331128/call-to-snprintf-and-vsnprintf-inside-a-variadic-function

#ifdef _WIN32
	#define calcFormatLen _vscprintf
#else
	#define calcFormatLen vsnprintf
#endif

Error CharString_formatVariadic(Allocator alloc, CharString *result, const C8 *format, va_list args) {

	if(!result || !format)
		return Error_nullPointer(!result ? 1 : 2, "CharString_formatVariadic()::result and format are required");

	va_list arg2;
	va_copy(arg2, args);

	int len = calcFormatLen(format, arg2);

	if(len < 0)
		return Error_stderr(0, "CharString_formatVariadic() len can't be <0");

	if (result->ptr)
		return Error_invalidOperation(0, "CharString_formatVariadic()::result isn't empty, could indicate memleak");

	if (len == 0) {
		*result = CharString_createNull();
		return Error_none();
	}

	Error err = CharString_create('\0', (U64) len, alloc, result);

	if(err.genericError)
		return err;

	if(vsnprintf((C8*)result->ptr, len + 1, format, args) < 0)
		return Error_stderr(0, "CharString_formatVariadic() vsnprintf failed");

	return Error_none();
}

Error CharString_format(Allocator alloc, CharString *result, const C8 *format, ...) {

	if(!result || !format)
		return Error_nullPointer(!result ? 1 : 2, "CharString_format()::result and format are required");

	va_list arg1;
	va_start(arg1, format);
	Error err = CharString_formatVariadic(alloc, result, format, arg1);
	va_end(arg1);

	return err;
}
