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

void ListCompiler_freeUnderlying(ListCompiler *compilers, Allocator alloc) {

	if(!compilers)
		return;

	for(U64 i = 0; i < compilers->length; ++i)
		Compiler_free(&compilers->ptrNonConst[i], alloc);

	ListCompiler_free(compilers, alloc);
}

void CompileError_free(CompileError *err, Allocator alloc) {

	if(!err)
		return;

	CharString_free(&err->file, alloc);
	CharString_free(&err->error, alloc);
}

ECompareResult IncludeInfo_compare(const IncludeInfo *a, const IncludeInfo *b) {

	if(!a)	return ECompareResult_Lt;
	if(!b)	return ECompareResult_Gt;

	return a->counter < b->counter || (a->counter == b->counter && a->crc32c < b->crc32c);
}

void IncludeInfo_free(IncludeInfo *info, Allocator alloc) {

	if(!info)
		return;

	CharString_free(&info->file, alloc);
}

void CompileResult_freex(CompileResult *result) {
	CompileResult_free(result, Platform_instance.alloc);
}

void ListCompiler_freeUnderlyingx(ListCompiler *compilers) {
	ListCompiler_freeUnderlying(compilers, Platform_instance.alloc);
}

void ListCompileError_freeUnderlying(ListCompileError *compileErrors, Allocator alloc) {

	if(!compileErrors)
		return;

	for(U64 i = 0; i < compileErrors->length; ++i)
		CompileError_free(&compileErrors->ptrNonConst[i], alloc);

	ListCompileError_free(compileErrors, alloc);
}

void ListIncludeInfo_freeUnderlying(ListIncludeInfo *includeInfos, Allocator alloc) {

	if(!includeInfos)
		return;

	for(U64 i = 0; i < includeInfos->length; ++i)
		IncludeInfo_free(&includeInfos->ptrNonConst[i], alloc);

	ListIncludeInfo_free(includeInfos, alloc);
}

void CompileError_freex(CompileError *err) {
	CompileError_free(err, Platform_instance.alloc);
}

void ListCompileError_freeUnderlyingx(ListCompileError *compileErrors) {
	ListCompileError_freeUnderlying(compileErrors, Platform_instance.alloc);
}

void IncludeInfo_freex(IncludeInfo *info) {
	IncludeInfo_free(info, Platform_instance.alloc);
}

void ListIncludeInfo_freeUnderlyingx(ListIncludeInfo *infos) {
	ListIncludeInfo_freeUnderlying(infos, Platform_instance.alloc);
}

Bool ListIncludeInfo_stringifyx(ListIncludeInfo files, CharString *tempStr, Error *e_rr) {
	return ListIncludeInfo_stringify(files, Platform_instance.alloc, tempStr, e_rr);
}

void IncludedFile_freex(IncludedFile *file) {
	IncludedFile_free(file, Platform_instance.alloc);
}

void ListIncludedFile_freeUnderlyingx(ListIncludedFile *file) {
	ListIncludedFile_freeUnderlying(file, Platform_instance.alloc);
}

void IncludedFile_free(IncludedFile *file, Allocator alloc) {

	if(!file)
		return;

	CharString_free(&file->data, alloc);
	IncludeInfo_free(&file->includeInfo, alloc);
}

void ListIncludedFile_freeUnderlying(ListIncludedFile *file, Allocator alloc) {

	if(!file)
		return;

	for(U64 i = 0; i < file->length; ++i)
		IncludedFile_free(&file->ptrNonConst[i], alloc);

	ListIncludedFile_free(file, alloc);
}

Bool ListIncludeInfo_stringify(ListIncludeInfo files, Allocator alloc, CharString *tempStr, Error *e_rr) {

	CharString tempStr2 = CharString_createNull();
	Bool s_uccess = true;

	//Info about includes

	gotoIfError2(clean, CharString_createCopy(CharString_createRefCStrConst("Includes:\n"), alloc, tempStr))

	for(U64 i = 0; i < files.length; ++i) {

		IncludeInfo includeInfo = files.ptr[i];
		TimeFormat format = { 0 };

		if(includeInfo.timestamp)
			Time_format(includeInfo.timestamp, format, true);

		if(includeInfo.counter == 1)
			gotoIfError2(clean, CharString_format(
				alloc, &tempStr2,
				"%08"PRIx32" %05"PRIu32" %s%s%s\n",
				includeInfo.crc32c, includeInfo.fileSize,
				includeInfo.timestamp ? format : "", includeInfo.timestamp ? " " : "",
				includeInfo.file.ptr
			))

		else gotoIfError2(clean, CharString_format(
			alloc, &tempStr2,
			"%03"PRIu64" reference(s): %08"PRIx32" %05"PRIu32" %s%s%s\n",
			includeInfo.counter,
			includeInfo.crc32c, includeInfo.fileSize,
			includeInfo.timestamp ? format : "", includeInfo.timestamp ? " " : "",
			includeInfo.file.ptr
		))

		gotoIfError2(clean, CharString_appendString(tempStr, tempStr2, alloc))
		CharString_free(&tempStr2, alloc);
	}

clean:

	if(!s_uccess)
		CharString_free(tempStr, alloc);

	CharString_free(&tempStr2, alloc);
	return s_uccess;
}

void CompileResult_free(CompileResult *result, Allocator alloc) {

	if(!result)
		return;

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
}

Bool Compiler_createx(Compiler *comp, Error *e_rr) {
	return Compiler_create(Platform_instance.alloc, comp, e_rr);
}

void Compiler_freex(Compiler *comp) {
	Compiler_free(comp, Platform_instance.alloc);
}

Bool Compiler_preprocessx(Compiler comp, CompilerSettings settings, CompileResult *result, Error *e_rr) {
	return Compiler_preprocess(comp, settings, Platform_instance.alloc, result, e_rr);
}

Bool Compiler_parsex(Compiler comp, CompilerSettings settings, CompileResult *result, Error *e_rr) {
	return Compiler_parse(comp, settings, Platform_instance.alloc, result, e_rr);
}

Bool Compiler_mergeIncludeInfox(Compiler *comp, ListIncludeInfo *infos, Error *e_rr) {
	return Compiler_mergeIncludeInfo(comp, Platform_instance.alloc, infos, e_rr);
}

//Invoke parser

Bool Compiler_parse(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result, Error *e_rr) {

	(void)comp;		//No need for a compiler, we do it ourselves

	//Lex & parse the file and afterwards, use it to classify all symbols

	Lexer lexer = (Lexer) { 0 };
	Parser parser = (Parser) { 0 };
	Bool s_uccess = true;

	gotoIfError3(clean, Lexer_create(settings.string, alloc, &lexer, e_rr))
	//Lexer_print(lexer, alloc);

	gotoIfError3(clean, Parser_create(&lexer, &parser, alloc, e_rr))
	//Parser_printTokens(parser, alloc);

	gotoIfError3(clean, Parser_classify(&parser, alloc, e_rr))
	Parser_printSymbols(parser, U32_MAX, true, alloc);

	//After, we need to resolve the symbols into actual struct layouts.
	//As well as return shEntries

	(void)result;	//TODO:

clean:
	Parser_free(&parser, alloc);
	Lexer_free(&lexer, alloc);
	return s_uccess;
}
