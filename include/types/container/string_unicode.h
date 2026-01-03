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
#include "types/container/string.h"
#include "types/container/list_predeclare.h"

#include <stdarg.h>

#ifdef __cplusplus
	extern "C" {
#endif

inline Bool CharString_isValidUTF8(const CharString str) {
	return Buffer_isUTF8(CharString_bufferConst(str), 1);
}

//Interop between wchar_t. Converts UTF16 or UTF32 to UTF8

Bool CharString_createFromUTF16(const U16 *ptr, U64 limit, const Allocator *allocator, CharString *result, Error *e_rr);
Bool CharString_createFromUTF32(const U32 *ptr, U64 limit, const Allocator *allocator, CharString *result, Error *e_rr);

//Interop between wide strings. Converts UTF8 to UTF16 or UTF32

Bool CharString_toUTF16(const CharString s, const Allocator *allocator, ListU16 *arr, Error *e_rr);
Bool CharString_toUTF32(const CharString s, const Allocator *allocator, ListU32 *arr, Error *e_rr);

//TODO: Bool CharString_setCodepointAt(const CharString str, U64 index, U32 codepoint, Error *e_rr);

//TODO: CharString_splitCodepoint

//TODO: CharString_appendCodepoint/prependCodepoint

//TODO: CharString_insertCodepoint
//TODO: CharString_findCodepoint
//TODO: CharString_containsCodepoint
//TODO: CharString_equalsCodepoint
//TODO: CharString_cut(Before/After)Codepoint
//TODO: CharString_eraseCodepoint
//TODO: CharString_replaceCodepoint
//TODO: UTF8 toLower/toUpper.
//TODO: CharString_replaceCodepoint
//TODO: CharString_countCodepoint/endsWithCodepoint/startsWithCodepoint
//TODO: CharString_resizeCodepoint?

//Returns U32_MAX if it wasn't a valid UTF8 codepoint
//TODO: U32 CharString_codepointAtByteConst(const CharString str, U64 startByteOffset, U8 *length);
//TODO: U32 CharString_codepointAtConst(const CharString str, U64 offset, U8 *length);

#ifdef __cplusplus
	}
#endif
