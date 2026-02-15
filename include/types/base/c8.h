/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/base/constants.h"
#include <limits.h>

//Transforming a string to a U16, U32 or U64

#define C8x2(x, y) ((((U16)y) << 8) | x)
#define C8x4(x, y, z, w) ((((U32)C8x2(z, w)) << 16) | C8x2(x, y))
#define C8x8(x, y, z, w, a, b, c, d) ((((U64)C8x4(a, b, c, d)) << 32) | C8x4(x, y, z, w))

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EStringCase {
	EStringCase_Sensitive,			//Prefer when possible; avoids transforming the character
	EStringCase_Insensitive
} EStringCase;

typedef enum EStringTransform {
	EStringTransform_None,
	EStringTransform_Lower,
	EStringTransform_Upper
} EStringTransform;

static inline C8 C8_toLower(C8 c) {
	return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static inline C8 C8_toUpper(C8 c) {
	return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

static inline C8 C8_transform(C8 c, EStringTransform transform) {
	return transform == EStringTransform_None ? c : (
		transform == EStringTransform_Lower ? C8_toLower(c) :
		C8_toUpper(c)
	);
}

static inline Bool C8_isBin(C8 c) { return c == '0' || c == '1'; }
static inline Bool C8_isOct(C8 c) { return c >= '0' && c <= '7'; }
static inline Bool C8_isDec(C8 c) { return c >= '0' && c <= '9'; }

static inline Bool C8_isUpperCase(C8 c) { return c >= 'A' && c <= 'Z'; }
static inline Bool C8_isLowerCase(C8 c) { return c >= 'a' && c <= 'z'; }
static inline Bool C8_isUpperCaseHex(C8 c) { return c >= 'A' && c <= 'F'; }
static inline Bool C8_isLowerCaseHex(C8 c) { return c >= 'a' && c <= 'f'; }
static inline Bool C8_isWhitespace(C8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static inline Bool C8_isNewLine(C8 c) { return c == '\n' || c == '\r'; }

static inline Bool C8_isHex(C8 c) { return C8_isDec(c) || C8_isUpperCaseHex(c) || C8_isLowerCaseHex(c); }
static inline Bool C8_isNyto(C8 c) { return C8_isDec(c) || C8_isUpperCase(c) || C8_isLowerCase(c) || c == '_' || c == '$'; }
static inline Bool C8_isAlphaNumeric(C8 c) { return C8_isDec(c) || C8_isUpperCase(c) || C8_isLowerCase(c); }
static inline Bool C8_isAlpha(C8 c) { return C8_isUpperCase(c) || C8_isLowerCase(c); }

static inline Bool C8_isSymbol(C8 c) {

	Bool symbolRange0 = c > ' ' && c < '0';		//~"#%&'()*+,-./$
	Bool symbolRange1 = c > '9' && c < 'A';		//:;<=>?@
	Bool symbolRange2 = c > 'Z' && c < 'a';		//[\]^`_
	Bool symbolRange3 = c > 'z' && c < 0x7F;	//{|}~

	return symbolRange0 || symbolRange1 || symbolRange2 || symbolRange3;
}

static inline Bool C8_isValidAscii(C8 c) { return (c >= 0x20 && c < 0x7F) || c == '\t' || c == '\n' || c == '\r'; }

static inline Bool C8_isValidFileName(C8 c) {

	#if CHAR_MIN < 0
		if(c < 0)
			return true;
	#else
		if(c >= 0x80)
			return true;
	#endif

	return
		(c >= 0x20 && c < 0x7F) &&
		c != '<' && c != '>' && c != ':' && c != '"' && c != '|' &&
		c != '?' && c != '*' && c != '/' && c != '\\';
}

static inline U8 C8_bin(C8 c) { return c == '0' ? 0 : (c == '1' ? 1 : U8_MAX); }
static inline U8 C8_oct(C8 c) { return C8_isOct(c) ? c - '0' : U8_MAX; }
static inline U8 C8_dec(C8 c) { return C8_isDec(c) ? c - '0' : U8_MAX; }

static inline U8 C8_hex(C8 c) {

	if (C8_isDec(c))
		return c - '0';

	if (C8_isUpperCaseHex(c))
		return c - 'A' + 10;

	if (C8_isLowerCaseHex(c))
		return c - 'a' + 10;

	return U8_MAX;
}

static inline U8 C8_nyto(C8 c) {

	if (C8_isDec(c))
		return c - '0';

	if (C8_isUpperCase(c))
		return c - 'A' + 10;

	if (C8_isLowerCase(c))
		return c - 'a' + 36;

	if (c == '_')
		return 62;

	return c == '$' ? 63 : U8_MAX;
}

static inline C8 C8_createBin(U8 v) { return (v == 0 ? '0' : (v == 1 ? '1' : C8_MAX)); }
static inline C8 C8_createOct(U8 v) { return v < 8 ? '0' + v : C8_MAX; }
static inline C8 C8_createDec(U8 v) { return v < 10 ? '0' + v : C8_MAX; }
static inline C8 C8_createHex(U8 v) { return v < 10 ? '0' + v : (v < 16 ? 'A' + v - 10 : C8_MAX); }

//Nytodecimal: 0-9A-Za-z_$

static inline C8 C8_createNyto(U8 v) {
	return v < 10 ? '0' + v : (
		v < 36 ? 'A' + v - 10 : (
			v < 62 ? 'a' + v - 36 : (
				v == 62 ? '_' : (
					v == 63 ? '$' : C8_MAX
				)
			)
		)
	);
}

#ifdef __cplusplus
	}
#endif
