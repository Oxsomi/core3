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

Bool String_freex(String *str);

Error String_createx(C8 c, U64 size, String *result);
Error String_createCopyx(String str, String *result);
Error String_createNytox(U64 v, U8 leadingZeros, String *result);
Error String_createHexx(U64 v, U8 leadingZeros, String *result);
Error String_createDecx(U64 v, U8 leadingZeros, String *result);
Error String_createOctx(U64 v, U8 leadingZeros, String *result);
Error String_createBinx(U64 v, U8 leadingZeros, String *result);

Error String_splitx(String s, C8 c, EStringCase casing, StringList *result);
Error String_splitStringx(String s, String other, EStringCase casing, StringList *result);
Error String_splitLinex(String s, StringList *result);

Error String_resizex(String *str, U64 length, C8 defaultChar);
Error String_reservex(String *str, U64 length);

Error String_appendx(String *s, C8 c);
Error String_appendStringx(String *s, String other);

Error String_insertx(String *s, C8 c, U64 i);
Error String_insertStringx(String *s, String other, U64 i);

Error String_replaceAllStringx(String *s, String search, String replace, EStringCase caseSensitive);

Error String_replaceStringx(
	String *s, 
	String search, 
	String replace, 
	EStringCase caseSensitive,
	Bool isFirst
);

Error String_replaceFirstStringx(String *s, String search, String replace, EStringCase caseSensitive);
Error String_replaceLastStringx(String *s, String search, String replace, EStringCase caseSensitive);

Error String_findAllx(String s, C8 c, EStringCase caseSensitive, List *result);
Error String_findAllStringx(String s, String other, EStringCase caseSensitive, List *result);

Bool StringList_freex(StringList *arr);

Error StringList_createx(U64 length, StringList *result);
Error StringList_createCopyx(StringList toCopy, StringList *arr);

Error StringList_setx(StringList arr, U64 i, String str);
Error StringList_unsetx(StringList arr, U64 i);

Error StringList_combinex(StringList arr, String *result);

Error StringList_concatx(StringList arr, C8 between, String *result);
Error StringList_concatStringx(StringList arr, String between, String *result);

Error String_formatx(String *result, const C8 *format, ...);
Error String_formatVariadicx(String *result, const C8 *format, va_list args);
