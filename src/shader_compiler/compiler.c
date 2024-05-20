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
#include "types/math.h"

TListImpl(Compiler);
TListImpl(CompileError);
TListImpl(IncludeInfo);
TListImpl(IncludedFile);
TListImpl(ListU16);

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

void ListListU16_freeUnderlying(ListListU16 *list, Allocator alloc) {

	if(!list)
		return;

	for(U16 i = 0; i < list->length; ++i)
		ListU16_free(&list->ptrNonConst[i], alloc);

	ListListU16_free(list, alloc);
}

void CompileError_freex(CompileError *err) {
	CompileError_free(err, Platform_instance.alloc);
}

void ListCompileError_freeUnderlyingx(ListCompileError *compileErrors) {
	ListCompileError_freeUnderlying(compileErrors, Platform_instance.alloc);
}

void ListListU16_freeUnderlyingx(ListListU16 *list) {
	ListListU16_freeUnderlying(list, Platform_instance.alloc);
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

		case ECompileResultType_SHEntryRuntime:
			ListSHEntryRuntime_freeUnderlying(&result->shEntriesRuntime, alloc);
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

ESHPipelineStage Compiler_parseStage(CharString stageName) {

	U64 stageNameLen = CharString_length(stageName);
	U32 c8x4 = stageNameLen < 4 ? 0 : *(const U32*)stageName.ptr;

	switch (c8x4) {

		default:
			break;

		case C8x4('v', 'e', 'r', 't'):		//vertex

			if(stageNameLen == 6 && *(const U16*)&stageName.ptr[4] == C8x2('e', 'x'))
				return ESHPipelineStage_Vertex;

			break;

		case C8x4('d', 'o', 'm', 'a'):		//domain

			if(stageNameLen == 6 && *(const U16*)&stageName.ptr[4] == C8x2('i', 'n'))
				return ESHPipelineStage_Domain;

			break;

		case C8x4('p', 'i', 'x', 'e'):		//pixel

			if(stageNameLen == 5 && stageName.ptr[4] == 'l')
				return ESHPipelineStage_Pixel;

			break;

		case C8x4('g', 'e', 'o', 'm'):		//geometry

			if(stageNameLen == 8 && *(const U32*)&stageName.ptr[4] == C8x4('e', 't', 'r', 'y'))
				return ESHPipelineStage_GeometryExt;

			break;

		case C8x4('c', 'o', 'm', 'p'):		//compute

			if(stageNameLen == 7 && *(const U32*)&stageName.ptr[3] == C8x4('p', 'u', 't', 'e'))
				return ESHPipelineStage_Compute;

			break;

		case C8x4('n', 'o', 'd', 'e'):		//node
			if(stageNameLen == 4)			return ESHPipelineStage_WorkgraphExt;
			break;

		case C8x4('m', 'e', 's', 'h'):		//mesh
			if(stageNameLen == 4)			return ESHPipelineStage_MeshExt;
			break;

		case C8x4('t', 'a', 's', 'k'):		//task
			if(stageNameLen == 4)			return ESHPipelineStage_TaskExt;
			break;

		case C8x4('h', 'u', 'l', 'l'):		//hull
			if(stageNameLen == 4)			return ESHPipelineStage_Hull;
			break;

		//Raytracing

		case C8x4('m', 'i', 's', 's'):		//miss
			if(stageNameLen == 4)			return ESHPipelineStage_MissExt;
			break;

		case C8x4('a', 'n', 'y', 'h'):		//anyhit

			if(stageNameLen == 6 && *(const U16*)&stageName.ptr[4] == C8x2('i', 't'))
				return ESHPipelineStage_AnyHitExt;

			break;

		case C8x4('c', 'l', 'o', 's'):		//closesthit

			if(
				stageNameLen == 10 && 
				*(const U64*)&stageName.ptr[2] == C8x8('o', 's', 'e', 's', 't', 'h', 'i', 't')
			)
				return ESHPipelineStage_ClosestHitExt;

			break;

		case C8x4('r', 'a', 'y', 'g'):		//raygeneration

			if(
				stageNameLen == 13 && 
				*(const U64*)&stageName.ptr[4] == C8x8('e', 'n', 'e', 'r', 'a', 't', 'i', 'o') &&
				stageName.ptr[12] == 'n'
			)
				return ESHPipelineStage_RaygenExt;

			break;

		case C8x4('i', 'n', 't', 'e'):		//intersection

			if(stageNameLen == 12 && *(const U64*)&stageName.ptr[4] == C8x8('r', 's', 'e', 'c', 't', 'i', 'o', 'n'))
				return ESHPipelineStage_IntersectionExt;

			break;
	}

	return ESHPipelineStage_Count;
}

ESHVendor Compiler_parseVendor(CharString vendor) {

	U64 len = CharString_length(vendor);

	switch(len) {
		
		default:
			return ESHVendor_Count;

		case 2:		//NV
			return *(const U16*)vendor.ptr == C8x2('N', 'V') ? ESHVendor_NV : ESHVendor_Count;

		case 3:		//AMD, ARM
			switch (*(const U16*)vendor.ptr) {
				default:				return ESHVendor_Count;
				case C8x2('A', 'M'):	return vendor.ptr[2] == 'D' ? ESHVendor_AMD : ESHVendor_Count;
				case C8x2('A', 'R'):	return vendor.ptr[2] == 'M' ? ESHVendor_ARM : ESHVendor_Count;
			}

		case 4:		//QCOM, INTC, IMGT, MSFT
			switch (*(const U32*)vendor.ptr) {
				default:							return ESHVendor_Count;
				case C8x4('Q', 'C', 'O', 'M'):		return ESHVendor_QCOM;
				case C8x4('I', 'N', 'T', 'C'):		return ESHVendor_INTC;
				case C8x4('I', 'M', 'G', 'T'):		return ESHVendor_IMGT;
				case C8x4('M', 'S', 'F', 'T'):		return ESHVendor_MSFT;
			}
	}
}

ESHExtension Compiler_parseExtension(CharString extensionName) {

	U64 stageNameLen = CharString_length(extensionName);
	U16 c8x2 = stageNameLen < 2 ? 0 : *(const U16*)extensionName.ptr;

	switch (c8x2) {

		case C8x2('F', '6'):	//F64
			if(stageNameLen == 3 && extensionName.ptr[2] == '4')	return ESHExtension_F64;
			break;

		case C8x2('I', '6'):	//I64
			if(stageNameLen == 3 && extensionName.ptr[2] == '4')	return ESHExtension_I64;
			break;

		case C8x2('F', '1'):	//F16
			if(stageNameLen == 3 && extensionName.ptr[2] == '6')	return ESHExtension_F16;
			break;

		case C8x2('I', '1'):	//I16
			if(stageNameLen == 3 && extensionName.ptr[2] == '6')	return ESHExtension_I16;
			break;

		case C8x2('A', 't'):	//AtomicI64, AtomicF32, AtomicF64

			if(stageNameLen == 9)
				switch (*(const U64*)&extensionName.ptr[1]) {
					case C8x8('t', 'o', 'm', 'i', 'c', 'I', '6', '4'):		return ESHExtension_AtomicI64;
					case C8x8('t', 'o', 'm', 'i', 'c', 'F', '3', '2'):		return ESHExtension_AtomicF32;
					case C8x8('t', 'o', 'm', 'i', 'c', 'F', '6', '4'):		return ESHExtension_AtomicF64;
				}

			break;

		case C8x2('S', 'u'):	//SubgroupArithmetic, SubgroupShuffle

			if(stageNameLen == 15) {			//SubgroupShuffle
				if(
					*(const U64*)&extensionName.ptr[0] == C8x8('S', 'u', 'b', 'g', 'r', 'o', 'u', 'p') &&
					*(const U64*)&extensionName.ptr[7] == C8x8('p', 'S', 'h', 'u', 'f', 'f', 'l', 'e')
				)
					return ESHExtension_SubgroupShuffle;
			}

			else if (stageNameLen == 18) {		//SubgroupArithmetic
				if(
					*(const U64*)&extensionName.ptr[ 2] == C8x8('b', 'g', 'r', 'o', 'u', 'p', 'A', 'r') &&
					*(const U64*)&extensionName.ptr[10] == C8x8('i', 't', 'h', 'm', 'e', 't', 'i', 'c')
				)
					return ESHExtension_SubgroupArithmetic;
			}

			break;

		case C8x2('R', 'a'):	//RayQuery, RayMicromapOpacity, RayMicromapDisplacement, RayMotionBlur, RayReorder

			if(stageNameLen == 8 && *(const U64*)&extensionName.ptr[0] == C8x8('R', 'a', 'y', 'Q', 'u', 'e', 'r', 'y'))
				return ESHExtension_RayQuery;

			else if(stageNameLen >= 10) 
				switch (*(const U64*)&extensionName.ptr[2]) {

					case C8x8('y', 'M', 'i', 'c', 'r', 'o', 'm', 'a'):		//RayMicromapDisplacement & Opacity

						if(
							stageNameLen == 23 &&
							*(const U64*)&extensionName.ptr[10] == C8x8('p', 'D', 'i', 's', 'p', 'l', 'a', 'c') &&
							*(const U64*)&extensionName.ptr[15] == C8x8('l', 'a', 'c', 'e', 'm', 'e', 'n', 't')
						)
							return ESHExtension_RayMicromapDisplacement;

						else if(
							stageNameLen == 18 &&
							*(const U64*)&extensionName.ptr[10] == C8x8('p', 'O', 'p', 'a', 'c', 'i', 't', 'y')
						)
							return ESHExtension_RayMicromapOpacity;


						break;

					case C8x8('y', 'M', 'o', 't', 'i', 'o', 'n', 'B'):		//RayMotionBlur

						if(stageNameLen == 13 && *(const U32*)&extensionName.ptr[10] == C8x4('B', 'l', 'u', 'r'))
							return ESHExtension_RayMotionBlur;

						break;

					case C8x8('y', 'R', 'e', 'o', 'r', 'd', 'e', 'r'):		//RayReorder

						if(stageNameLen == 10)
							return ESHExtension_RayReorder;

						break;
				}

			break;
	}

	return 1 << ESHExtension_Count;
}

Bool Compiler_registerExtension(ESHExtension *extensions, U32 tokenId, Parser parser, Error *e_rr) {

	Bool s_uccess = true;

	//Find extension

	CharString extensionName = Token_asString(parser.tokens.ptr[tokenId], &parser);
	ESHExtension extension = Compiler_parseExtension(extensionName);

	if(extension >> ESHExtension_Count)
		retError(clean, Error_invalidParameter(
			0, 5, "Compiler_parse() unrecognized extension in extension annotation"
		))

	if(*extensions & extension)
		retError(clean, Error_invalidParameter(
			0, 6, "Compiler_parse() duplicate extension found"
		))

	*extensions |= extension;

clean:
	return s_uccess;
}

Bool Compiler_registerVendor(U16 *vendors, U32 tokenId, Parser parser, Error *e_rr) {

	Bool s_uccess = true;

	//Find vendor

	CharString vendorName = Token_asString(parser.tokens.ptr[tokenId], &parser);
	ESHVendor vendor = Compiler_parseVendor(vendorName);

	if(vendor == ESHVendor_Count)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_parse() unrecognized vendor in vendor annotation"
		))

	if((*vendors >> vendor) & 1)
		retError(clean, Error_invalidParameter(
			0, 2, "Compiler_parse() duplicate vendor found"
		))

	*vendors |= (U16)(1 << vendor);

clean:
	return s_uccess;
}

#define DXC_SHADER_MODEL(maj, min) ((U16)((min) | ((maj) << 8)))

Bool Compiler_registerModel(ListU16 *vendors, U32 tokenId, Parser parser, Error *e_rr) {

	Bool s_uccess = true;

	//Find model version

	F64 modelVersion = parser.tokens.ptr[tokenId].valuef;

	if(modelVersion < 6)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_parse() model version is too low to be supported (must be >=6.0)"
		))

	if(modelVersion > 6.8)
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_parse() model version is too high, only supported up to 6.8"
		))

	U16 version = DXC_SHADER_MODEL(6, (U16)F64_round((modelVersion - 6) / 0.1));

	if(ListU16_contains(*vendors, version, 0, NULL))
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_parse() model version was referenced multiple times"
		))

	gotoIfError2(clean, ListU16_pushBackx(vendors, version))

clean:
	return s_uccess;
}

Bool Compiler_registerUniform(
	SHEntryRuntime *runtimeEntry,
	U32 *tokenCounter,
	U32 tokenEnd,
	Bool isFirst,
	Parser parser,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	//Register uniform name

	Token tok = parser.tokens.ptr[*tokenCounter];
	CharString uniformName = parser.parsedLiterals.ptr[tok.valueu];
	++*tokenCounter;

	U64 inserted = U64_MAX;

	U16 currentUniforms = 0;

	if(isFirst)
		gotoIfError2(clean, ListU8_pushBack(&runtimeEntry->uniformsPerCompilation, 0, alloc))

	else currentUniforms = *ListU8_last(runtimeEntry->uniformsPerCompilation);

	//Scan last strings for the same uniform

	for(
		U64 i = runtimeEntry->uniforms.length - 1;
		i != runtimeEntry->uniforms.length - currentUniforms - 1;
		--i
	)
		if (CharString_equalsStringSensitive(uniformName, runtimeEntry->uniforms.ptr[i]))
			retError(clean, Error_alreadyDefined(0, "Compiler_registerUniform() already contains uniform"))

	//Insert uniformName

	uniformName = CharString_createRefSizedConst(
		uniformName.ptr, CharString_length(uniformName), CharString_isNullTerminated(uniformName)
	);

	gotoIfError2(clean, ListCharString_pushBack(&runtimeEntry->uniforms, uniformName, alloc))
	inserted = runtimeEntry->uniforms.length - 1;

	//If next token is equals then uniformValue

	CharString uniformValue = CharString_createNull();

	if (*tokenCounter < tokenEnd) {

		ETokenType tokenType = parser.tokens.ptr[*tokenCounter].tokenType;

		if(tokenType != ETokenType_Comma) {

			if(tokenType != ETokenType_Asg)
				retError(clean, Error_invalidOperation(0, "Compiler_registerUniform() expected \"string\" = \"string\""))

			++*tokenCounter;

			if(*tokenCounter >= tokenEnd || parser.tokens.ptr[*tokenCounter].tokenType != ETokenType_String)
				retError(clean, Error_invalidOperation(1, "Compiler_registerUniform() expected \"string\" = \"string\""))

			uniformValue = parser.parsedLiterals.ptr[parser.tokens.ptr[*tokenCounter].valueu];
			++*tokenCounter;

			uniformValue = CharString_createRefSizedConst(
				uniformValue.ptr, CharString_length(uniformValue), CharString_isNullTerminated(uniformValue)
			);
		}
	}

	//Insert uniformValue

	gotoIfError2(clean, ListCharString_pushBack(&runtimeEntry->uniformValues, uniformValue, alloc))
	++*ListU8_last(runtimeEntry->uniformsPerCompilation);

clean:

	if(!s_uccess && inserted != U64_MAX)
		ListCharString_erase(&runtimeEntry->uniforms, inserted);

	return s_uccess;
}

Bool Compiler_parse(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result, Error *e_rr) {

	(void)comp;		//No need for a compiler, we do it ourselves

	Lexer lexer = (Lexer) { 0 };
	Parser parser = (Parser) { 0 };
	Bool s_uccess = true;

	SHEntryRuntime runtimeEntry = (SHEntryRuntime) { 0 };

	if(!result)
		retError(clean, Error_nullPointer(3, "Compiler_parse()::result is required"))

	//Lex & parse the file and afterwards, use it to classify all symbols

	gotoIfError3(clean, Lexer_create(settings.string, alloc, &lexer, e_rr))
	gotoIfError3(clean, Parser_create(&lexer, &parser, alloc, e_rr))
	gotoIfError3(clean, Parser_classify(&parser, alloc, e_rr))

	result->type = ECompileResultType_SHEntryRuntime;

	gotoIfError2(clean, ListSHEntryRuntime_reservex(&result->shEntriesRuntime, parser.rootSymbols))

	//Now we can find all entrypoints and return it into a ListSHEntryRuntime.
	//An entrypoint is defined as a stage with an annotation being shader("shaderType") or stage("stageType").

	for (U32 i = 0; i < parser.rootSymbols; ) {

		Symbol sym = parser.symbols.ptr[i];

		//We found a potential entrypoint, check for shader or stage annotation

		if (!(sym.flags & ESymbolFlag_HasTemplate) && sym.symbolType == ESymbolType_Function && sym.annotations) {

			runtimeEntry.entry.stage = ESHPipelineStage_Count;

			//Scan annotations

			for (U32 j = i + 1; j < i + Symbol_size(sym); ++j) {

				Symbol symj = parser.symbols.ptr[j];
				Token tok = parser.tokens.ptr[symj.tokenId];

				//[uniform()]
				//[extension()]
				//[vendor()]
				//[model()]
				//[stage()]
				//[shader()]
				// ^

				if (tok.tokenType == ETokenType_Identifier) {

					CharString tokStr = Token_asString(tok, &parser);
					U64 tokLen = CharString_length(tokStr);

					U32 c8x4 = tokLen < 4 ? 0 : *(const U32*)tokStr.ptr;

					switch (c8x4) {

						//Identify if we're parsing a real entrypoint

						case C8x4('s', 't', 'a', 'g'):		//stage()

							//[stage("vertex")]
							//      ^
							if (tokLen == 5 && tokStr.ptr[4] == 'e') {

								if(runtimeEntry.entry.stage != ESHPipelineStage_Count)
									retError(clean, Error_invalidParameter(
										0, 0, "Compiler_parse() stage already had shader or stage annotation"
									))

								if(symj.tokenCount + 1 != 4)
									retError(clean, Error_invalidParameter(
										0, 0, "Compiler_parse() stage annotation expected stage(string)"
									))

								//[stage("vertex")]
								//      ^

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_String ||
									parser.tokens.ptr[symj.tokenId + 3].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() stage annotation expected stage(string)"
									))

								CharString stageName = Token_asString(parser.tokens.ptr[symj.tokenId + 2], &parser);
								ESHPipelineStage stage = Compiler_parseStage(stageName);
								
								if(
									stage == ESHPipelineStage_Count || 
									stage == ESHPipelineStage_WorkgraphExt || 
									(stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt)
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() unrecognized stage in stage annotation. "
										"For raytracing shaders and workgraphs, shader(\"\") should be used instead"
									))

								runtimeEntry.entry.stage = stage;

								CharString name = parser.symbolNames.ptr[sym.name];
								name = CharString_createRefSizedConst(
									name.ptr, CharString_length(name), CharString_isNullTerminated(name)
								);

								runtimeEntry.entry.name = name;
							}

							break;

						case C8x4('s', 'h', 'a', 'd'):		//shader()

							//[shader("vertex")]
							//      ^
							if (tokLen == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('e', 'r')) {

								if(runtimeEntry.entry.stage != ESHPipelineStage_Count)
									retError(clean, Error_invalidParameter(
										0, 0, "Compiler_parse() shader already had shader or stage annotation"
									))
									
								if(symj.tokenCount + 1 != 4)
									retError(clean, Error_invalidParameter(
										0, 0, "Compiler_parse() shader annotation expected shader(string)"
									))

								//[shader("vertex")]
								//       ^

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_String ||
									parser.tokens.ptr[symj.tokenId + 3].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1, "Compiler_parse() shader annotation expected shader(string)"
									))

								CharString stageName = Token_asString(parser.tokens.ptr[symj.tokenId + 2], &parser);
								ESHPipelineStage stage = Compiler_parseStage(stageName);
								
								if(stage == ESHPipelineStage_Count)
									retError(clean, Error_invalidParameter(
										0, 1, "Compiler_parse() unrecognized stage in shader annotation"
									))

								runtimeEntry.entry.stage = stage;
								runtimeEntry.isShaderAnnotation = true;

								CharString name = parser.symbolNames.ptr[sym.name];
								name = CharString_createRefSizedConst(
									name.ptr, CharString_length(name), CharString_isNullTerminated(name)
								);

								runtimeEntry.entry.name = name;
							}

							break;

						case C8x4('m', 'o', 'd', 'e'):		//model()

							//[model(6.8, 6.4)]
							//      ^
							if (tokLen == 5 && tokStr.ptr[4] == 'l') {
							
								if(symj.tokenCount + 1 < 4)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() model annotation expected model(double, ...)"
									))

								U32 tokenEnd = symj.tokenId + symj.tokenCount;

								//[model(6.8)]
								//      ^

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_Double  ||
									parser.tokens.ptr[tokenEnd].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() model annotation expected model(double, ...)"
									))

								gotoIfError3(clean, Compiler_registerModel(
									&runtimeEntry.shaderVersions, symj.tokenId + 2, parser, e_rr
								))

								//[model(6.8, 6.3)]
								//          ^

								for (U32 k = symj.tokenId + 3; k < tokenEnd; ++k) {
								
									if(parser.tokens.ptr[k].tokenType != ETokenType_Comma)
										retError(clean, Error_invalidParameter(
											0, 3,
											"Compiler_parse() model annotation expected comma in model(double, ...)"
										))
								
									if(k + 1 >= tokenEnd || parser.tokens.ptr[k + 1].tokenType != ETokenType_Double)
										retError(clean, Error_invalidParameter(
											0, 4,
											"Compiler_parse() extension annotation expected double in model(double, ...)"
										))
										
									gotoIfError3(clean, Compiler_registerModel(
										&runtimeEntry.shaderVersions, k + 1, parser, e_rr
									))
								}
							}

							break;

						case C8x4('v', 'e', 'n', 'd'):		//vendor()

							//[vendor("NV", "AMD")]
							//      ^
							if (tokLen == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('o', 'r')) {
							
								if(symj.tokenCount + 1 < 4)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() vendor annotation expected vendor(string, ...)"
									))

								U32 tokenEnd = symj.tokenId + symj.tokenCount;

								//[vendor("NV")]
								//       ^

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_String ||
									parser.tokens.ptr[tokenEnd].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() vendor annotation expected vendor(string, ...)"
									))

								gotoIfError3(clean, Compiler_registerVendor(
									&runtimeEntry.vendorMask, symj.tokenId + 2, parser, e_rr
								))

								//[vendor("AMD", "NV")]
								//             ^

								for (U32 k = symj.tokenId + 3; k < tokenEnd; ++k) {
								
									if(parser.tokens.ptr[k].tokenType != ETokenType_Comma)
										retError(clean, Error_invalidParameter(
											0, 3,
											"Compiler_parse() vendor annotation expected comma in vendor(string, ...)"
										))
								
									if(k + 1 >= tokenEnd || parser.tokens.ptr[k + 1].tokenType != ETokenType_String)
										retError(clean, Error_invalidParameter(
											0, 4,
											"Compiler_parse() vendor annotation expected string in vendor(string, ...)"
										))

									gotoIfError3(clean, Compiler_registerVendor(
										&runtimeEntry.vendorMask, k + 1, parser, e_rr
									))
								}
							}

							break;

						case C8x4('u', 'n', 'i', 'f'):		//uniform()

							//[uniform("X", "Y", "Z")]
							//[uniform("X" = "123", "Y" = "ABC")]
							// ^
							if (tokLen == 7 && *(const U32*)&tokStr.ptr[3] == C8x4('f', 'o', 'r', 'm')) {

								if(symj.tokenCount + 1 < 4)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() uniform annotation expected uniform(string, string = string, ...)"
									))

								U32 tokenEnd = symj.tokenId + symj.tokenCount;

								//[uniform("X")]
								//        ^

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_String ||
									parser.tokens.ptr[tokenEnd].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() uniform annotation expected uniform(string, ...)"
									))

								U32 tokenCounter = symj.tokenId + 2;

								gotoIfError3(clean, Compiler_registerUniform(
									&runtimeEntry, &tokenCounter, tokenEnd, true, parser, alloc, e_rr
								))

								//[uniform("X", "Y")]
								//            ^

								for (U32 k = tokenCounter; k < tokenEnd; ) {
								
									if(parser.tokens.ptr[k].tokenType != ETokenType_Comma)
										retError(clean, Error_invalidParameter(
											0, 3,
											"Compiler_parse() uniform annotation expected comma in uniform(string, ...)"
										))
								
									if(k + 1 >= tokenEnd || parser.tokens.ptr[k + 1].tokenType != ETokenType_String)
										retError(clean, Error_invalidParameter(
											0, 4,
											"Compiler_parse() uniform annotation expected string in uniform(string, ...)"
										))

									++k;
									gotoIfError3(clean, Compiler_registerUniform(
										&runtimeEntry, &k, tokenEnd, false, parser, alloc, e_rr
									))
								}
							}

							break;

						case C8x4('e', 'x', 't', 'e'):		//extension()

							//[extension()]
							// ^
							if (
								tokLen == 9 && 
								*(const U64*)&tokStr.ptr[1] == C8x8('x', 't', 'e', 'n', 's', 'i', 'o', 'n')
							) {

								if(symj.tokenCount + 1 < 4)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() extension annotation expected extension(string, ...)"
									))

								U32 tokenEnd = symj.tokenId + symj.tokenCount;

								//[extension("I16")]
								//           ^

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_String ||
									parser.tokens.ptr[tokenEnd].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() extension annotation expected extension(string, ...)"
									))

								gotoIfError3(clean, Compiler_registerExtension(
									&runtimeEntry.extensions, symj.tokenId + 2, parser, e_rr
								))

								//[extension("I16", "F16")]
								//                ^

								for (U32 k = symj.tokenId + 3; k < tokenEnd; ++k) {
								
									if(parser.tokens.ptr[k].tokenType != ETokenType_Comma)
										retError(clean, Error_invalidParameter(
											0, 3,
											"Compiler_parse() extension annotation expected comma in extension(string, ...)"
										))
								
									if(k + 1 >= tokenEnd || parser.tokens.ptr[k + 1].tokenType != ETokenType_String)
										retError(clean, Error_invalidParameter(
											0, 4,
											"Compiler_parse() extension annotation expected string in extension(string, ...)"
										))

									gotoIfError3(clean, Compiler_registerExtension(
										&runtimeEntry.extensions, k + 1, parser, e_rr
									))
								}
							}

							break;
					}
				}
			}

			//If we didn't find a stage, but we did find annotations that match, we need to free them

			if (runtimeEntry.entry.stage == ESHPipelineStage_Count)
				SHEntryRuntime_free(&runtimeEntry, alloc);

			//Otherwise we found an entry

			else {

				//If we don't have any model version, we try the latest

				if(!runtimeEntry.shaderVersions.length)
					gotoIfError2(clean, ListU16_pushBackx(&runtimeEntry.shaderVersions, DXC_SHADER_MODEL(6, 8)))

				gotoIfError2(clean, ListSHEntryRuntime_pushBackx(&result->shEntriesRuntime, runtimeEntry));
				runtimeEntry = (SHEntryRuntime) { 0 };
			}
		}

		i += Symbol_size(sym);
	}

	result->isSuccess = true;

clean:

	if(!s_uccess && result)
		CompileResult_freex(result);

	SHEntryRuntime_free(&runtimeEntry, alloc);
	Parser_free(&parser, alloc);
	Lexer_free(&lexer, alloc);
	return s_uccess;
}
