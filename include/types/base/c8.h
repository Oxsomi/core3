/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Char
//All char helper functions assume ASCII, otherwise use string functions

C8 C8_toLower(C8 c);
C8 C8_toUpper(C8 c);

C8 C8_transform(C8 c, EStringTransform transform);

Bool C8_isBin(C8 c);
Bool C8_isOct(C8 c);
Bool C8_isDec(C8 c);

Bool C8_isUpperCase(C8 c);
Bool C8_isLowerCase(C8 c);
Bool C8_isUpperCaseHex(C8 c);
Bool C8_isLowerCaseHex(C8 c);
Bool C8_isWhitespace(C8 c);
Bool C8_isNewLine(C8 c);

Bool C8_isHex(C8 c);
Bool C8_isNyto(C8 c);
Bool C8_isAlphaNumeric(C8 c);
Bool C8_isAlpha(C8 c);

Bool C8_isSymbol(C8 c);			//~"#%&'()*+,-./$:;<=>?@[\]^`_{|}~
Bool C8_isLexerSymbol(C8 c);	//isSymbol but excluding $_

Bool C8_isValidAscii(C8 c);
Bool C8_isValidFileName(C8 c);

U8 C8_bin(C8 c);
U8 C8_oct(C8 c);
U8 C8_dec(C8 c);

U8 C8_hex(C8 c);
U8 C8_nyto(C8 c);

C8 C8_createBin(U8 v);
C8 C8_createOct(U8 v);
C8 C8_createDec(U8 v);
C8 C8_createHex(U8 v);

//Nytodecimal: 0-9A-Za-z_$
C8 C8_createNyto(U8 v);

//Transforming a string to a U16, U32 or U64

#define C8x2(x, y) ((((U16)y) << 8) | x)
#define C8x4(x, y, z, w) ((((U32)C8x2(z, w)) << 16) | C8x2(x, y))
#define C8x8(x, y, z, w, a, b, c, d) ((((U64)C8x4(a, b, c, d)) << 32) | C8x4(x, y, z, w))

#ifdef __cplusplus
	}
#endif
