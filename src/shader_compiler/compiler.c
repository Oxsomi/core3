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

#include "platforms/ext/listx_impl.h"
#include "shader_compiler/compiler.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/parser.h"
#include "types/lexer.h"
#include "types/time.h"

TListImpl(Compiler);
TListImpl(CompileError);
TListImpl(IncludeInfo);
TListImpl(IncludedFile);

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

Bool CompileError_free(CompileError *err, Allocator alloc) {

	if(!err)
		return true;

	CharString_free(&err->file, alloc);
	CharString_free(&err->error, alloc);
	return true;
}

ECompareResult IncludeInfo_compare(const IncludeInfo *a, const IncludeInfo *b) {

	if(!a)	return ECompareResult_Lt;
	if(!b)	return ECompareResult_Gt;

	return a->counter < b->counter || (a->counter == b->counter && a->crc32c < b->crc32c);
}

Bool IncludeInfo_free(IncludeInfo *info, Allocator alloc) {

	if(!info)
		return true;

	CharString_free(&info->file, alloc);
	return true;
}

Bool CompileResult_freex(CompileResult *result) {
	return CompileResult_free(result, Platform_instance.alloc);
}

Bool ListCompiler_freeUnderlyingx(ListCompiler *compilers) {
	return ListCompiler_freeUnderlying(compilers, Platform_instance.alloc);
}

Bool ListCompileError_freeUnderlying(ListCompileError *compileErrors, Allocator alloc) {

	if(!compileErrors)
		return true;

	for(U64 i = 0; i < compileErrors->length; ++i)
		CompileError_free(&compileErrors->ptrNonConst[i], alloc);

	return ListCompileError_free(compileErrors, alloc);
}

Bool ListIncludeInfo_freeUnderlying(ListIncludeInfo *includeInfos, Allocator alloc) {

	if(!includeInfos)
		return true;

	for(U64 i = 0; i < includeInfos->length; ++i)
		IncludeInfo_free(&includeInfos->ptrNonConst[i], alloc);

	return ListIncludeInfo_free(includeInfos, alloc);
}

Bool CompileError_freex(CompileError *err) {
	return CompileError_free(err, Platform_instance.alloc);
}

Bool ListCompileError_freeUnderlyingx(ListCompileError *compileErrors) {
	return ListCompileError_freeUnderlying(compileErrors, Platform_instance.alloc);
}

Bool IncludeInfo_freex(IncludeInfo *info) {
	return IncludeInfo_free(info, Platform_instance.alloc);
}

Bool ListIncludeInfo_freeUnderlyingx(ListIncludeInfo *infos) {
	return ListIncludeInfo_freeUnderlying(infos, Platform_instance.alloc);
}

Error ListIncludeInfo_stringifyx(ListIncludeInfo files, CharString *tempStr) {
	return ListIncludeInfo_stringify(files, Platform_instance.alloc, tempStr);
}

Bool IncludedFile_freex(IncludedFile *file) {
	return IncludedFile_free(file, Platform_instance.alloc);
}

Bool ListIncludedFile_freeUnderlyingx(ListIncludedFile *file) {
	return ListIncludedFile_freeUnderlying(file, Platform_instance.alloc);
}

Bool IncludedFile_free(IncludedFile *file, Allocator alloc) {

	if(!file)
		return true;

	Bool success = CharString_free(&file->data, alloc);
	success &= IncludeInfo_free(&file->includeInfo, alloc);
	return success;
}

Bool ListIncludedFile_freeUnderlying(ListIncludedFile *file, Allocator alloc) {

	if(!file)
		return true;

	for(U64 i = 0; i < file->length; ++i)
		IncludedFile_free(&file->ptrNonConst[i], alloc);

	return ListIncludedFile_free(file, alloc);
}

Error ListIncludeInfo_stringify(ListIncludeInfo files, Allocator alloc, CharString *tempStr) {

	CharString tempStr2 = CharString_createNull();
	Error err = Error_none();

	//Info about includes

	gotoIfError(clean, CharString_createCopy(CharString_createRefCStrConst("Includes:\n"), alloc, tempStr))

	for(U64 i = 0; i < files.length; ++i) {

		IncludeInfo includeInfo = files.ptr[i];
		TimeFormat format = { 0 };

		if(includeInfo.timestamp)
			Time_format(includeInfo.timestamp, format, true);

		if(includeInfo.counter == 1)
			gotoIfError(clean, CharString_format(
				alloc, &tempStr2,
				"%08"PRIx32" %05"PRIu32" %s%s%s\n",
				includeInfo.crc32c, includeInfo.fileSize,
				includeInfo.timestamp ? format : "", includeInfo.timestamp ? " " : "",
				includeInfo.file.ptr
			))

		else gotoIfError(clean, CharString_format(
			alloc, &tempStr2,
			"%03"PRIu64" reference(s): %08"PRIx32" %05"PRIu32" %s%s%s\n",
			includeInfo.counter,
			includeInfo.crc32c, includeInfo.fileSize,
			includeInfo.timestamp ? format : "", includeInfo.timestamp ? " " : "",
			includeInfo.file.ptr
		))

		gotoIfError(clean, CharString_appendString(tempStr, tempStr2, alloc))
		CharString_free(&tempStr2, alloc);
	}

clean:

	if(err.genericError)
		CharString_free(tempStr, alloc);

	CharString_free(&tempStr2, alloc);
	return err;
}

Bool CompileResult_free(CompileResult *result, Allocator alloc) {

	if(!result)
		return true;

	ListCompileError_freeUnderlying(&result->compileErrors, alloc);
	ListIncludeInfo_freeUnderlying(&result->includeInfo, alloc);

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

Error Compiler_parsex(Compiler comp, CompilerSettings settings, CompileResult *result) {
	return Compiler_parse(comp, settings, Platform_instance.alloc, result);
}

Error Compiler_mergeIncludeInfox(Compiler *comp, ListIncludeInfo *infos) {
	return Compiler_mergeIncludeInfo(comp, Platform_instance.alloc, infos);
}

//Invoke parser

Error Compiler_parse(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result) {

	(void)comp;		//No need for a compiler, we do it ourselves

	//Lex & parse the file and afterwards, use it to classify all symbols

	Lexer lexer = (Lexer) { 0 };
	Parser parser = (Parser) { 0 };
	Error err = Error_none();
	gotoIfError(clean, Lexer_create(settings.string, alloc, &lexer))
	//Lexer_print(lexer, alloc);

	gotoIfError(clean, Parser_create(&lexer, &parser, alloc))
	//Parser_printTokens(parser, alloc);

	gotoIfError(clean, Parser_classify(&parser, alloc))
	Parser_printSymbols(parser, alloc);

	//After, we need to resolve the symbols into actual struct layouts.
	//As well as return shEntries

	(void)result;	//TODO:

clean:
	Parser_free(&parser, alloc);
	Lexer_free(&lexer, alloc);
	return err;
}
