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

#pragma once
#include "platforms/platform.h"
#include "types/string.h"

Bool CharString_freex(CharString *str);

Error CharString_createx(C8 c, U64 size, CharString *result);
Error CharString_createCopyx(CharString str, CharString *result);
Error CharString_createNytox(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createHexx(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createDecx(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createOctx(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createBinx(U64 v, U8 leadingZeros, CharString *result);

Error CharString_splitx(CharString s, C8 c, EStringCase casing, CharStringList *result);
Error CharString_splitStringx(CharString s, CharString other, EStringCase casing, CharStringList *result);
Error CharString_splitLinex(CharString s, CharStringList *result);

Error CharString_resizex(CharString *str, U64 length, C8 defaultChar);
Error CharString_reservex(CharString *str, U64 length);

Error CharString_appendx(CharString *s, C8 c);
Error CharString_appendStringx(CharString *s, CharString other);
Error CharString_prependx(CharString *s, C8 c);
Error CharString_prependStringx(CharString *s, CharString other);

Error CharString_insertx(CharString *s, C8 c, U64 i);
Error CharString_insertStringx(CharString *s, CharString other, U64 i);

Error CharString_replaceAllStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive);

Error CharString_replaceStringx(
	CharString *s, 
	CharString search, 
	CharString replace, 
	EStringCase caseSensitive,
	Bool isFirst
);

Error CharString_replaceFirstStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive);
Error CharString_replaceLastStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive);

Error CharString_findAllx(CharString s, C8 c, EStringCase caseSensitive, List *result);
Error CharString_findAllStringx(CharString s, CharString other, EStringCase caseSensitive, List *result);

Bool CharStringList_freex(CharStringList *arr);

Error CharStringList_createx(U64 length, CharStringList *result);
Error CharStringList_createCopyx(CharStringList toCopy, CharStringList *arr);

Error CharStringList_setx(CharStringList arr, U64 i, CharString str);
Error CharStringList_unsetx(CharStringList arr, U64 i);

Error CharStringList_combinex(CharStringList arr, CharString *result);

Error CharStringList_concatx(CharStringList arr, C8 between, CharString *result);
Error CharStringList_concatStringx(CharStringList arr, CharString between, CharString *result);

Error CharString_formatx(CharString *result, const C8 *format, ...);
Error CharString_formatVariadicx(CharString *result, const C8 *format, va_list args);
