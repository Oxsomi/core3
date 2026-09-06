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

//shader_compiler/compiler_private.hpp

#pragma once
#include "shader_compiler/compiler.h"

//Internal helpers shared between the DXC wrapper TUs.
// (compiler.cpp, compiler_compile.cpp, compiler_annotate.cpp, compiler_parse.cpp)
//These have C++ linkage and are not part of the public compiler interface.
//Include this after dxcapi.h and dxcreflect.h, since it references DXC interface types.

class IncludeHandler;
struct IDxcIncludeHandler;
struct D3D12_HLSL_ANNOTATION;

//What's wrong with UTF8?

#if _PLATFORM_TYPE == PLATFORM_WINDOWS

	#define Compiler_defineStrings                                                                       \
		ListU16 tempStrUTF16 = ListU16{};                                                                \
		ListListU16 stringsUTF16 = ListListU16{};                                                        \
		ListU16PtrConst strings = ListU16PtrConst{}

	#define Compiler_freeStrings                                                                           \
		ListU16_free(&tempStrUTF16, alloc);                                                                \
		ListListU16_freeUnderlying(&stringsUTF16, alloc);                                                  \
		ListU16PtrConst_free(&strings, alloc)

	#define Compiler_convertToWString(strs, label)                                                       \
		for(U64 ii = 0; ii < strs.length; ++ii) {                                                        \
			gotoIfError3(label, CharString_toUTF16(strs.ptr[ii], alloc, &tempStrUTF16, e_rr));           \
			gotoIfError3(label, ListU16PtrConst_pushBack(&strings, tempStrUTF16.ptr, alloc, e_rr));      \
			gotoIfError3(label, ListListU16_pushBack(&stringsUTF16, tempStrUTF16, alloc, e_rr));         \
			tempStrUTF16 = ListU16{};                                                                    \
		}
#else
	#define Compiler_defineStrings                                                                       \
		ListU32 tempStrUTF32 = ListU32{};                                                                \
		ListListU32 stringsUTF32 = ListListU32{};                                                        \
		ListU32PtrConst strings = ListU32PtrConst{}

	#define Compiler_freeStrings                                                                           \
		ListU32_free(&tempStrUTF32, alloc);                                                                \
		ListListU32_freeUnderlying(&stringsUTF32, alloc);                                                  \
		ListU32PtrConst_free(&strings, alloc)

	#define Compiler_convertToWString(strs, label)                                                       \
		for(U64 ii = 0; ii < strs.length; ++ii) {                                                        \
			gotoIfError3(label, CharString_toUTF32(strs.ptr[ii], alloc, &tempStrUTF32, e_rr));           \
			gotoIfError3(label, ListU32PtrConst_pushBack(&strings, tempStrUTF32.ptr, alloc, e_rr));      \
			gotoIfError3(label, ListListU32_pushBack(&stringsUTF32, tempStrUTF32, alloc, e_rr));         \
			tempStrUTF32 = ListU32{};                                                                    \
		}
#endif

//Defined in compiler.cpp

Bool Compiler_setupIncludePaths(ListCharString *dst, const CompilerSettings *settings, const Allocator *alloc, Error *e_rr);

Bool Compiler_copyIncludes(CompileResult *result, IncludeHandler *includeHandler, const Allocator *alloc, Error *e_rr);

void Compiler_resetIncludeHandler(IncludeHandler *includeHandler);

IDxcIncludeHandler *Compiler_getIncludeHandler(IncludeHandler *includeHandler);

Bool Compiler_registerArgStr(ListCharString *strings, CharString str, const Allocator *alloc, Error *e_rr);

Bool Compiler_registerArgStrConst(ListCharString *strings, CharString str, const Allocator *alloc, Error *e_rr);

//Only with const C8* that will always be in mem
Bool Compiler_registerArgCStr(ListCharString *strings, const C8 *str, const Allocator *alloc, Error *e_rr);

//Defined in compiler_annotate.cpp

Bool Compiler_skipWhitespace(const C8 *&ptr);

Bool Compiler_skipRndBracket(const C8 *&str, Bool isLeft, Error *e_rr);

Bool Compiler_consumeIdentifier(const C8 *&str, const C8 *&start, Error *e_rr);

Bool Compiler_parseAnnot(
	const D3D12_HLSL_ANNOTATION &annot,
	CharString funcName,
	SHEntryRuntime &entry,
	const Allocator *alloc,
	Error *e_rr
);

//Defined in compiler_parse.cpp

Bool Compiler_parseUniformsAnnot(SHEntryRuntime &entry, const C8 *&str, const Allocator *alloc, Error *e_rr);
