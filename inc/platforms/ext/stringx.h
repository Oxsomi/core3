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

#pragma once
#include "platforms/platform.h"
#include "types/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

Bool CharString_freex(CharString *str);

Error CharString_createx(C8 c, U64 size, CharString *result);
Error CharString_createCopyx(CharString str, CharString *result);
Error CharString_createNytox(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createHexx(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createDecx(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createOctx(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createBinx(U64 v, U8 leadingZeros, CharString *result);
Error CharString_createFromUTF16x(const U16 *ptr, U64 max, CharString *result);
Error CharString_toUTF16x(CharString s, ListU16 *arr);

Error CharString_splitx(CharString s, C8 c, EStringCase casing, ListCharString *result);
Error CharString_splitStringx(CharString s, CharString other, EStringCase casing, ListCharString *result);
Error CharString_splitLinex(CharString s, ListCharString *result);

Error CharString_splitSensitivex(CharString s, C8 c, ListCharString *result);
Error CharString_splitStringSensitivex(CharString s, CharString other, ListCharString *result);
Error CharString_splitInsensitivex(CharString s, C8 c, ListCharString *result);
Error CharString_splitStringInsensitivex(CharString s, CharString other, ListCharString *result);

Error CharString_resizex(CharString *str, U64 length, C8 defaultChar);
Error CharString_reservex(CharString *str, U64 length);

Error CharString_appendx(CharString *s, C8 c);
Error CharString_appendStringx(CharString *s, CharString other);
Error CharString_prependx(CharString *s, C8 c);
Error CharString_prependStringx(CharString *s, CharString other);

Error CharString_insertx(CharString *s, C8 c, U64 i);
Error CharString_insertStringx(CharString *s, CharString other, U64 i);

Error CharString_replaceAllStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive, U64 off);

Error CharString_replaceStringx(
	CharString *s,
	CharString search,
	CharString replace,
	EStringCase caseSensitive,
	Bool isFirst,
	U64 off
);

Error CharString_replaceFirstStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive, U64 off);
Error CharString_replaceLastStringx(CharString *s, CharString search, CharString replace, EStringCase caseSensitive, U64 off);

Error CharString_replaceAllStringSensitivex(CharString *s, CharString search, CharString replace, U64 off);
Error CharString_replaceStringSensitivex(CharString *s, CharString search, CharString replace, Bool isFirst, U64 off);

Error CharString_replaceFirstStringSensitivex(CharString *s, CharString search, CharString replace, U64 off);
Error CharString_replaceLastStringSensitivex(CharString *s, CharString search, CharString replace, U64 off);

Error CharString_replaceAllStringInsensitivex(CharString *s, CharString search, CharString replace, U64 off);
Error CharString_replaceStringInsensitivex(CharString *s, CharString search, CharString replace, Bool isFirst, U64 off);

Error CharString_replaceFirstStringInsensitivex(CharString *s, CharString search, CharString replace, U64 off);
Error CharString_replaceLastStringInsensitivex(CharString *s, CharString search, CharString replace, U64 off);

Error CharString_findAllx(CharString s, C8 c, EStringCase caseSensitive, ListU64 *result, U64 off);
Error CharString_findAllStringx(CharString s, CharString other, EStringCase caseSensitive, ListU64 *result, U64 off);

Error CharString_findAllSensitivex(CharString s, C8 c, U64 off, ListU64 *result);
Error CharString_findAllStringSensitivex(CharString s, CharString other, U64 off, ListU64 *result);

Error CharString_findAllInsensitivex(CharString s, C8 c, U64 off, ListU64 *result);
Error CharString_findAllStringInsensitivex(CharString s, CharString other, U64 off, ListU64 *result);

Bool ListCharString_freeUnderlyingx(ListCharString *arr);
Error ListCharString_createCopyUnderlyingx(ListCharString toCopy, ListCharString *arr);

Error ListCharString_combinex(ListCharString arr, CharString *result);

Error ListCharString_concatx(ListCharString arr, C8 between, CharString *result);
Error ListCharString_concatStringx(ListCharString arr, CharString between, CharString *result);

Error CharString_formatx(CharString *result, const C8 *format, ...);
Error CharString_formatVariadicx(CharString *result, const C8 *format, va_list args);

#ifdef __cplusplus
	}
#endif
