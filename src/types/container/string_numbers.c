/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/math/math.h"

#define CharString_matchesPattern(testFunc, ...)								\
																				\
	U64 strl = CharString_length(s);											\
																				\
	for(U64 i = (__VA_ARGS__); i < CharString_length(s); ++i)					\
		if(!testFunc(s.ptr[i]))													\
			return false;														\
																				\
	return !!strl;

#define CharString_matchesPatternNum(testFunc, num) CharString_matchesPattern(	\
	testFunc,																	\
	CharString_startsWithStringInsensitive(										\
		s, CharString_createRefCStrConst(num), 0								\
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
		CharString_startsWithSensitive(s, '-', 0) || CharString_startsWithSensitive(s, '+', 0)
	);
}

Bool CharString_isUnsignedNumber(CharString s) {
	CharString_matchesPattern(
		C8_isDec,
		CharString_startsWithSensitive(s, '+', 0)
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

	//It's just [-+]?[0-9]*[.]?[0-9]*[fF]
	if (i == strl || (i + 1 == strl && (s.ptr[i] == 'f' || s.ptr[i] == 'F')))
		return true;

	return false;
}

#define CharString_createNum(maxVal, func, prefixRaw, ...)															\
																													\
	if (!result)																									\
		return Error_nullPointer(3, "CharString_createNum()::result is required");									\
																													\
	CharString prefix = CharString_createRefCStrConst(prefixRaw);													\
																													\
	if (result->ptr)																								\
		return Error_invalidOperation(0, "CharString_createNum()::result wasn't empty, might indicate memleak");	\
																													\
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
	result->ptrNonConst[CharString_length(*result)] = '\0';															\
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

Error CharString_offsetAsRef(CharString s, U64 off, CharString *result) {

	if (!result)
		return Error_nullPointer(2, "CharString_offsetAsRef()::result is required");

	if (CharString_isEmpty(s)) {
		*result = CharString_createNull();
		return Error_none();
	}

	const U64 strl = CharString_length(s);

	if(off >= strl)
		return Error_outOfBounds(1, off, strl, "CharString_offsetAsRef()::off is out of bounds");

	*result = (CharString) {
		.ptr = s.ptr + off,
		.lenAndNullTerminated = (strl - off) | ((U64)CharString_isNullTerminated(s) << 63),
		.capacityAndRefInfo = CharString_isConstRef(s) ? U64_MAX : 0
	};

	return Error_none();
}

Bool CharString_parseNyto(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0n");

	const U64 prepend = CharString_startsWithStringInsensitive(s, prefix, 0) ? CharString_length(prefix) : 0;
	const Error err = CharString_offsetAsRef(s, prepend, &s);

	const U64 strl = CharString_length(s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || strl > 11)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 1; i != U64_MAX; --i, j <<= 6) {

		const U8 v = C8_nyto(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == ((U64)1 << (10 * 6)) && v >= (1 << 4))		//We have 4 bits left
			return false;

		*result |= j * v;
	}

	return true;
}

Bool CharString_parseHex(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0x");
	const U64 prepend = CharString_startsWithStringInsensitive(s, prefix, 0) ? CharString_length(prefix) : 0;
	const Error err = CharString_offsetAsRef(s, prepend, &s);

	if (err.genericError)
		return false;

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || strl > 16)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 0; i != U64_MAX; --i, j += 4) {

		const U8 v = C8_hex(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		*result |= (U64)v << j;
	}

	return true;
}

Bool CharString_parseOct(CharString s, U64 *result) {

	const CharString prefix = CharString_createRefCStrConst("0o");
	const U64 prepend = CharString_startsWithStringInsensitive(s, prefix, 0) ? CharString_length(prefix) : 0;
	const Error err = CharString_offsetAsRef(s, prepend, &s);

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

	const CharString prefix = CharString_createRefCStrConst("0b");
	const U64 prepend = CharString_startsWithStringInsensitive(s, prefix, 0) ? CharString_length(prefix) : 0;
	const Error err = CharString_offsetAsRef(s, prepend, &s);

	if (err.genericError)
		return false;

	if (!result || !s.ptr || CharString_length(s) > 64)
		return false;

	*result = 0;

	for (U64 i = CharString_length(s) - 1, j = 1; i != U64_MAX; --i, j <<= 1) {

		const U8 v = C8_bin(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(v)
			*result |= j;
	}

	return true;
}

Bool CharString_parseDec(CharString s, U64 *result) {

	const U64 strl = CharString_length(s);

	if (!result || !s.ptr || strl > 20)
		return false;

	*result = 0;

	for (U64 i = strl - 1, j = 1; i != U64_MAX; --i, j *= 10) {

		const U8 v = C8_dec(s.ptr[i]);

		if (v == U8_MAX)
			return false;

		if(j == (U64) 1e19 && v > 1)		//Out of value
			return false;

		*result += j * v;
	}

	return true;
}

Bool CharString_parseDecSigned(CharString s, I64 *result) {

	const Bool neg = CharString_startsWithSensitive(s, '-', 0);

	const Error err = CharString_offsetAsRef(s, neg, &s);

	if (err.genericError)
		return false;

	const Bool b = CharString_parseDec(s, (U64*) result);

	if(!b)
		return false;

	if (!neg && *result == I64_MIN)			//Int min is the same as 63 bit max + 1
		return true;

	if (*(U64*)result >> 63)				//Guard against int overflow
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

	if (CharString_startsWithSensitive(s, '-', 0)) {

		sign = true;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (CharString_startsWithSensitive(s, '+', 0)) {

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	//Parse integer part

	F64 intPart = 0;
	Bool hasDigit = false;

	while(
		!CharString_startsWithSensitive(s, '.', 0) &&
		!CharString_startsWithInsensitive(s, 'e', 0)
	) {

		const U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX) {

			if((s.ptr[0] == 'f' || s.ptr[0] == 'F') && hasDigit && CharString_length(s) == 1) {
				*result = sign ? -intPart : intPart;
				return true;
			}

			return false;
		}

		hasDigit = true;
		intPart = intPart * 10 + v;

		if(!F64_isValid(intPart))
			return false;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			s = CharString_createNull();

		if (!CharString_length(s)) {
			*result = sign ? -intPart : intPart;
			return true;
		}
	}

	//Parse fraction

	if (CharString_startsWithSensitive(s, '.', 0) && CharString_length(s) == 1) {
		*result = sign ? -intPart : intPart;
		return true;
	}

	if(CharString_startsWithSensitive(s, '.', 0))
		CharString_offsetAsRef(s, 1, &s);

	F64 fraction = 0, multiplier = 0.1;

	while(!CharString_startsWithInsensitive(s, 'e', 0)) {

		const U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX) {

			if((s.ptr[0] == 'f' || s.ptr[0] == 'F') && hasDigit && CharString_length(s) == 1)  {
				*result = sign ? -(intPart + fraction) : intPart + fraction;
				return F64_isValid(*result);
			}

			return false;
		}

		hasDigit = true;
		fraction += v * multiplier;

		if(!F64_isValid(fraction))
			return false;

		multiplier *= 0.1;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			s = CharString_createNull();

		if (!CharString_length(s)) {
			*result = sign ? -(intPart + fraction) : intPart + fraction;
			return F64_isValid(*result);
		}
	}

	//Parse exponent sign

	Bool nextE = CharString_startsWithInsensitive(s, 'e', 0);

	if (CharString_offsetAsRef(s, 1, &s).genericError) {

		if (!nextE) {
			*result = sign ? -(intPart + fraction) : intPart + fraction;
			return F64_isValid(*result);
		}

		return false;
	}

	Bool esign = false;

	if (CharString_startsWithSensitive(s, '-', 0)) {

		esign = true;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	else if (CharString_startsWithSensitive(s, '+', 0)) {

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			return false;
	}

	//Parse exponent (must be int)

	F64 exponent = 0;

	while(CharString_length(s)) {

		const U8 v = C8_dec(s.ptr[0]);

		if (v == U8_MAX) {

			if(s.ptr[0] == 'f' || s.ptr[0] == 'F') {

				const Error err = F64_exp10(esign ? -exponent : exponent, &exponent);

				if(err.genericError)
					return false;

				const F64 res = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;

				if(!F64_isValid(res))
					return false;

				*result = res;
				return hasDigit && CharString_length(s) == 1;
			}

			return false;
		}

		exponent = exponent * 10 + v;

		if(!F64_isValid(fraction))
			return false;

		if (CharString_offsetAsRef(s, 1, &s).genericError)
			break;
	}

	const Error err = F64_exp10(esign ? -exponent : exponent, &exponent);

	if(err.genericError)
		return false;

	const F64 res = (sign ? -(intPart + fraction) : intPart + fraction) * exponent;

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

Bool CharString_parseU64(CharString s, U64 *result) {

	if (CharString_startsWithSensitive(s, '0', 0))
		switch (CharString_getAt(s, 1)) {

			//0 prefix is also octal.
			//For clarity, we even pass 08 and 09, so they can error.
			//Otherwise it's inconsistent behavior

			case '0':	case '1':	case '2':	case '3':	case '4':
			case '5':	case '6':	case '7':	case '8':	case '9':
			case 'O':	case 'o':
				return CharString_parseOct(s, result);

			case 'B':	case 'b':	return CharString_parseBin(s, result);
			case 'X':	case 'x':	return CharString_parseHex(s, result);
			case 'N':	case 'n':	return CharString_parseNyto(s, result);
		}

	return CharString_parseDec(s, result);
}
