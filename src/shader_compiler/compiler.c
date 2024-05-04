/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "platforms/ext/listx_impl.h"
#include "shader_compiler/compiler.h"
#include "platforms/platform.h"

TListImpl(Compiler);
TListImpl(CompileError);

U32 CompileError_lineId(CompileError err) {
	return err.lineId | ((U32)(err.typeLineId & (U8)I8_MAX) << 16);
}

Bool ListCompiler_freeUnderlying(ListCompiler *compilers, Allocator alloc) {

	if(!compilers)
		return true;

	for(U64 i = 0; i < compilers->length; ++i)
		Compiler_free(&compilers->ptrNonConst[i], alloc);

	return ListCompiler_free(compilers, alloc);
}

Bool CompileResult_freex(CompileResult *result) {
	return CompileResult_free(result, Platform_instance.alloc);
}

Bool ListCompiler_freeUnderlyingx(ListCompiler *compilers) {
	return ListCompiler_freeUnderlying(compilers, Platform_instance.alloc);
}

Bool CompileResult_free(CompileResult *result, Allocator alloc) {

	if(!result)
		return true;

	for (U64 i = 0; i < result->compileErrors.length; ++i) {
		CompileError err = result->compileErrors.ptr[i];
		CharString_free(&err.file, alloc);
		CharString_free(&err.error, alloc);
	}

	ListCompileError_free(&result->compileErrors, alloc);

	switch (result->type) {

		default:
			CharString_free(&result->text, alloc);
			break;

		case ECompileResultType_Binaries:	

			for(U64 i = 0; i < result->binaries.length; ++i)
				Buffer_free(&result->binaries.ptrNonConst[i], alloc);

			ListBuffer_free(&result->binaries, alloc);
			break;

		case ECompileResultType_SHEntries:

			for(U64 i = 0; i < result->shEntries.length; ++i)
				CharString_free(&result->shEntries.ptrNonConst[i].name, alloc);

			ListSHEntry_free(&result->shEntries, alloc);
			break;
	}

	return true;
}

Error Compiler_createx(Compiler *comp) {
	return Compiler_create(Platform_instance.alloc, comp);
}

Bool Compiler_freex(Compiler *comp) {
	return Compiler_free(comp, Platform_instance.alloc);
}

Error Compiler_preprocessx(Compiler comp, CompilerSettings settings, CompileResult *result) {
	return Compiler_preprocess(comp, settings, Platform_instance.alloc, result);
}
