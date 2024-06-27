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
#include "platforms/ext/formatx.h"
#include "types/parser.h"
#include "types/lexer.h"
#include "types/time.h"
#include "types/math.h"

TListImpl(Compiler);
TListImpl(CompileError);
TListImpl(IncludeInfo);
TListImpl(IncludedFile);
TListImpl(ListU16);
TListImpl(CompileResult);
TListNamedImpl(ListU16PtrConst);

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

void ListCompileResult_freeUnderlyingx(ListCompileResult* result) {
	ListCompileResult_freeUnderlying(result, Platform_instance.alloc);
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

		case ECompileResultType_Binary:
			Buffer_free(&result->binary, alloc);
			break;

		case ECompileResultType_SHEntryRuntime:
			ListSHEntryRuntime_freeUnderlying(&result->shEntriesRuntime, alloc);
			break;
	}

	*result = (CompileResult) { 0 };
}

void ListCompileResult_freeUnderlying(ListCompileResult *result, Allocator alloc) {

	if (!result)
		return;

	for (U64 i = 0; i < result->length; ++i)
		CompileResult_free(&result->ptrNonConst[i], alloc);

	ListCompileResult_free(result, alloc);
}

Bool Compiler_createx(Compiler *comp, Error *e_rr) {
	return Compiler_create(Platform_instance.alloc, comp, e_rr);
}

void Compiler_freex(Compiler *comp) {
	Compiler_free(comp, Platform_instance.alloc);
}

Bool Compiler_crc32cx(CharString str, U32 *crc32c, Error *e_rr) {
	return Compiler_crc32c(str, crc32c, Platform_instance.alloc, e_rr);
}

Bool Compiler_preprocessx(Compiler comp, CompilerSettings settings, CompileResult *result, Error *e_rr) {
	return Compiler_preprocess(comp, settings, Platform_instance.alloc, result, e_rr);
}

Bool Compiler_compilex(
	Compiler comp,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	CompileResult *result,
	Error *e_rr
) {
	return Compiler_compile(comp, settings, toCompile, Platform_instance.alloc, result, e_rr);
}

Bool Compiler_parsex(Compiler comp, CompilerSettings settings, Bool symbolsOnly, CompileResult *result, Error *e_rr) {
	return Compiler_parse(comp, settings, symbolsOnly, Platform_instance.alloc, result, e_rr);
}

Bool Compiler_mergeIncludeInfox(Compiler *comp, ListIncludeInfo *infos, Error *e_rr) {
	return Compiler_mergeIncludeInfo(comp, Platform_instance.alloc, infos, e_rr);
}

Bool Compiler_filterWarning(CharString str) {
	return 
		CharString_startsWithStringSensitive(str, CharString_createRefCStrConst("#pragma once in main file\n"), 0) ||
		CharString_containsStringSensitive(str, CharString_createRefCStrConst("-Wunknown-attributes"));
}

Bool Compiler_parseErrors(CharString errs, Allocator alloc, ListCompileError *errors, Bool *hasErrors, Error *e_rr) {

	CharString dxilSigning = CharString_createRefCStrConst("warning: DXIL signing library (dxil.dll,libdxil.so) not found.");

	errs = CharString_createRefStrConst(errs);

	U64 loc = CharString_findFirstStringSensitive(errs, dxilSigning, 0);

	if(loc != U64_MAX)
		errs.lenAndNullTerminated = loc | (errs.lenAndNullTerminated & ((U64)1 << 63));

	Bool s_uccess = true;
	U64 off = 0;

	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();

	CharString warning = CharString_createRefCStrConst(" warning: ");
	CharString note = CharString_createRefCStrConst(" note: ");
	CharString errStr = CharString_createRefCStrConst(" error: ");
	CharString fatalErrStr = CharString_createRefCStrConst(" fatal error: ");

	U64 prevOff = U64_MAX;

	CharString file = CharString_createNull();
	U64 lineId = 0;
	U64 lineOff = 0;
	Bool isError = false;

	//Internal compiler error can't be parsed the same way
	//If this happens, bad stuff is happening

	CharString internalCompileErrorRef = CharString_createRefCStrConst("Internal Compiler error: ");

	if (CharString_equalsStringSensitive(errs, internalCompileErrorRef)) {
		CompileError cerr = (CompileError) { .error = internalCompileErrorRef };
		gotoIfError2(clean, ListCompileError_pushBack(errors, cerr, alloc))
		return true;
	}

	//Regular parsing

	while(off < CharString_length(errs)) {

		//Find start of next error

		U64 firstColon = CharString_findFirstSensitive(errs, ':', off);
		U64 errorEnd = firstColon;

		//On Windows, D:/test.hlsl:10:10: warning: x is valid ofc, so we should skip that

		if((!off && firstColon == 1) || C8_isNewLine(CharString_getAt(errs, firstColon - 2)))
			errorEnd = firstColon = CharString_findFirstSensitive(errs, ':', firstColon + 1);

		if(firstColon == U64_MAX)
			break;

		//Sometimes our line starts with "In file included from ", which makes me angry

		{
			U64 i = firstColon;

			for(; i != U64_MAX && !C8_isNewLine(CharString_getAt(errs, i)); --i)
				;

			++i;

			CharString lameString = CharString_createRefCStrConst("In file included from ");

			if (CharString_startsWithStringSensitive(errs, lameString, i)) {

				//Find error start

				while(i != U64_MAX && C8_isNewLine(CharString_getAt(errs, i--)));

				++i;

				if(!off)
					errorEnd = i;

				Bool hasNewLine = false;

				while (true) {

					C8 next = CharString_getAt(errs, ++firstColon);

					if(next == C8_MAX)
						break;

					//Find first non newline once we have encountered the first newline

					Bool isNewLine = C8_isNewLine(next);

					if(hasNewLine && !isNewLine)
						break;

					hasNewLine = isNewLine;
				}

				//Reset colon to be correct

				off = firstColon;
				continue;
			}
		}

		//We ended with no more errors

		if(firstColon >= CharString_length(errs))
			break;

		//In case we are still parsing an error, we have to traverse our : to the first whitespace character.
		//As the start of the file name is the end of our error.
		//We can then finalize the error and push it.

		if (prevOff != U64_MAX) {

			U64 i = errorEnd;

			for(; i != U64_MAX; --i)
				if(C8_isWhitespace(errs.ptr[i]))
					break;

			if(i == U64_MAX)
				retError(clean, Error_invalidState(1, "Compiler_preprocess() couldn't parse error"))

			CharString errorStr = CharString_createRefSizedConst(errs.ptr + prevOff, i - prevOff, false);

			if(!Compiler_filterWarning(errorStr)) {

				gotoIfError2(clean, CharString_createCopy(errorStr, alloc, &tempStr))
				gotoIfError2(clean, CharString_createCopy(file, alloc, &tempStr2))

				CompileError cerr = (CompileError) {
					.lineId = (U16) lineId,
					.typeLineId = (U8) ((lineId >> 16) | ((U8)isError << 7)),
					.lineOffset = (U8) lineOff,
					.error = tempStr,
					.file = tempStr2
				};

				gotoIfError2(clean, ListCompileError_pushBack(errors, cerr, alloc))

				tempStr = CharString_createNull();
				tempStr2 = CharString_createNull();
			}

			file = CharString_createNull();
		}

		//Parse file

		if(firstColon == U64_MAX || !CharString_cut(errs, off, firstColon - off, &file))
			retError(clean, Error_invalidState(1, "Compiler_preprocess() couldn't parse file from error"))

		//If there's a preceeding \n, then we need to make sure to cut it off

		U64 fileStart = CharString_findLastSensitive(file, '\n', 0);

		if (fileStart != U64_MAX) {

			CharString tmp0 = CharString_createNull();

			if(!CharString_cut(file, fileStart + 1, 0, &tmp0))
				retError(clean, Error_invalidState(1, "Compiler_preprocess() couldn't parse file properly"))

			file = tmp0;		//It's a reference so it's safe
		}

		//Parse line

		U64 nextColon = CharString_findFirstSensitive(errs, ':', firstColon + 1);

		CharString tmp = CharString_createNull();

		if(nextColon == U64_MAX || !CharString_cut(errs, firstColon + 1, nextColon - (firstColon + 1), &tmp))
			retError(clean, Error_invalidState(2, "Compiler_preprocess() couldn't parse lineId from error"))

		if(!CharString_parseDec(tmp, &lineId) || (lineId >> (16 + 7)))
			retError(clean, Error_invalidState(3, "Compiler_preprocess() couldn't parse U23 lineId from error"))

		//Parse line offset

		tmp = CharString_createNull();
		off = nextColon + 1;
		nextColon = CharString_findFirstSensitive(errs, ':', off);

		if(nextColon == U64_MAX || !CharString_cut(errs, off, nextColon - off, &tmp))
			retError(clean, Error_invalidState(2, "Compiler_preprocess() couldn't parse lineOff from error"))

		if(!CharString_parseDec(tmp, &lineOff) || lineOff >> 8)
			retError(clean, Error_invalidState(3, "Compiler_preprocess() couldn't parse U8 lineOff from error"))

		off = nextColon + 1;

		//Parse warning type

		Bool isFatalError = CharString_startsWithStringInsensitive(errs, fatalErrStr, off);

		if(CharString_startsWithStringInsensitive(errs, errStr, off) || isFatalError) {
			isError = true;
			*hasErrors = true;
			off += isFatalError ? CharString_length(fatalErrStr) : CharString_length(errStr);
		}

		else if(CharString_startsWithStringInsensitive(errs, warning, off)) {
			isError = false;
			off += CharString_length(warning);
		}

		else if(CharString_startsWithStringInsensitive(errs, note, off)) {
			isError = false;
			off += CharString_length(note);
		}

		else retError(clean, Error_invalidState(4, "Compiler_preprocess() couldn't parse error type from error"))

		prevOff = off;
	}

	if (prevOff != U64_MAX) {

		CharString errorStr = CharString_createRefSizedConst(
			errs.ptr + prevOff, CharString_length(errs) - prevOff, false
		);

		if(!Compiler_filterWarning(errorStr)) {

			gotoIfError2(clean, CharString_createCopy(errorStr, alloc, &tempStr))
			gotoIfError2(clean, CharString_createCopy(file, alloc, &tempStr2))

			CompileError cerr = (CompileError) {
				.lineId = (U16) lineId,
				.typeLineId = (U8) ((lineId >> 16) | ((U8)isError << 7)),
				.lineOffset = (U8) lineOff,
				.error = tempStr,
				.file = tempStr2
			};

			gotoIfError2(clean, ListCompileError_pushBack(errors, cerr, alloc))

			tempStr = CharString_createNull();
			tempStr2 = CharString_createNull();
		}

		file = CharString_createNull();
	}

clean:
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	return s_uccess;
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

		case C8x2('P', 'A'):	//PAQ
			if(stageNameLen == 3 && extensionName.ptr[2] == 'Q')	return ESHExtension_PAQ;
			break;

		case C8x2('1', '6'):	//16BitTypes

			if(
				stageNameLen == 10 &&
				*(const U64*)&extensionName.ptr[2] == C8x8('B', 'i', 't', 'T', 'y', 'p', 'e', 's')
			)
				return ESHExtension_16BitTypes;

			break;

		case C8x2('M', 'u'):	//Multiview

			if(
				stageNameLen == 9 &&
				*(const U64*)&extensionName.ptr[1] == C8x8('u', 'l', 't', 'i', 'v', 'i', 'e', 'w')
			)
				return ESHExtension_Multiview;

			break;

		case C8x2('C', 'o'):	//ComputeDeriv

			if(
				stageNameLen == 12 &&
				*(const U64*)&extensionName.ptr[ 2] == C8x8('m', 'p', 'u', 't', 'e', 'D', 'e', 'r') &&
				*(const U16*)&extensionName.ptr[10] == C8x2('i', 'v')
			)
				return ESHExtension_ComputeDeriv;

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

Bool Compiler_registerExtension(U32 *extensions, U32 tokenId, Parser parser, Error *e_rr) {

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

Bool Compiler_registerExtensions(ListU32 *extensionsRegistered, U32 extensions, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(ListU32_contains(*extensionsRegistered, extensions, 0, NULL))
		retError(clean, Error_alreadyDefined(0, "Compiler_registerExtensions() extensions already defined"))

	gotoIfError2(clean, ListU32_pushBack(extensionsRegistered, extensions, alloc));

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

Bool Compiler_registerModel(ListU16 *vendors, U32 tokenId, Parser parser, Allocator alloc, Error *e_rr) {

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

	U16 version = OISH_SHADER_MODEL(6, (U16)F64_round((modelVersion - 6) / 0.1));

	if(ListU16_contains(*vendors, version, 0, NULL))
		retError(clean, Error_invalidParameter(
			0, 1, "Compiler_parse() model version was referenced multiple times"
		))

	gotoIfError2(clean, ListU16_pushBack(vendors, version, alloc))

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

	for(U64 i = 0; i < CharString_length(uniformName); ++i)
		if(C8_isSymbol(uniformName.ptr[i]) || C8_isWhitespace(uniformName.ptr[i]))
			retError(clean, Error_alreadyDefined(0, "Compiler_registerUniform() can't contain symbols or whitespace"))

	U16 currentUniforms = 0;

	if(isFirst)
		gotoIfError2(clean, ListU8_pushBack(&runtimeEntry->uniformsPerCompilation, 0, alloc))

	else currentUniforms = *ListU8_last(runtimeEntry->uniformsPerCompilation);

	//Scan last strings for the same uniform

	for(
		U64 i = (runtimeEntry->uniformNameValues.length >> 1) - 1;
		i != (runtimeEntry->uniformNameValues.length >> 1) - currentUniforms - 1;
		--i
	)
		if (CharString_equalsStringSensitive(uniformName, runtimeEntry->uniformNameValues.ptr[i << 1]))
			retError(clean, Error_alreadyDefined(0, "Compiler_registerUniform() already contains uniform"))

	//Insert uniformName

	uniformName = CharString_createRefStrConst(uniformName);
	gotoIfError2(clean, ListCharString_pushBack(&runtimeEntry->uniformNameValues, uniformName, alloc))
	inserted = runtimeEntry->uniformsPerCompilation.length - 1;

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

			uniformValue = CharString_createRefStrConst(uniformValue);
		}
	}

	//Insert uniformValue

	gotoIfError2(clean, ListCharString_pushBack(&runtimeEntry->uniformNameValues, uniformValue, alloc))
	++*ListU8_last(runtimeEntry->uniformsPerCompilation);

clean:

	if(!s_uccess && inserted != U64_MAX)
		ListCharString_erase(&runtimeEntry->uniformNameValues, inserted);

	return s_uccess;
}

U16 Compiler_minFeatureSetStage(ESHPipelineStage stage, U16 waveSize) {

	U16 minVersion = OISH_SHADER_MODEL(6, 5);

	if(stage == ESHPipelineStage_WorkgraphExt)
		minVersion = OISH_SHADER_MODEL(6, 8);

	if(waveSize & 0xF)
		minVersion = OISH_SHADER_MODEL(6, 6);

	if(waveSize >> 4)
		minVersion = OISH_SHADER_MODEL(6, 8);

	return minVersion;
}

U16 Compiler_minFeatureSetExtension(ESHExtension ext) {

	U16 minVersion = OISH_SHADER_MODEL(6, 5);

	if(ext & ESHExtension_AtomicI64)
		minVersion = U16_max(OISH_SHADER_MODEL(6, 6), minVersion);

	if(ext & ESHExtension_ComputeDeriv)
		minVersion = U16_max(OISH_SHADER_MODEL(6, 6), minVersion);

	if(ext & ESHExtension_PAQ)
		minVersion = U16_max(OISH_SHADER_MODEL(6, 6), minVersion);

	return minVersion;
}

Bool Compiler_crc32c(CharString str, U32 *crc32c, Allocator alloc, Error *e_rr) {

	U64 off = CharString_findFirstSensitive(str, '\n', 0);
	CharString tmp = CharString_createNull();
	Bool s_uccess = true;

	if(!crc32c)
		retError(clean, Error_nullPointer(1, "Compiler_crc32c()::crc32c is required"))

	//If we use \r\n (Windows) then we have to reallocate and replace \r\n with \n
	//Otherwise our unix and windows hashes won't match anymore.

	if(off != U64_MAX && off && str.ptr[off - 1] == '\r') {

		gotoIfError2(clean, CharString_createCopy(str, alloc, &tmp))

		if(!CharString_eraseAllSensitive(&tmp, '\r', 0))
			retError(clean, Error_invalidState(0, "Compiler_crc32c() replacing \\r\\n line endings failed"))
	}

	Buffer buf = CharString_bufferConst(tmp.ptr ? tmp : str);
	*crc32c = Buffer_crc32c(buf);

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

Bool Compiler_parse(
	Compiler comp,
	CompilerSettings settings,
	Bool symbolsOnly,
	Allocator alloc,
	CompileResult *result,
	Error *e_rr
) {

	(void)comp;		//No need for a compiler, we do it ourselves

	Lexer lexer = (Lexer) { 0 };
	Parser parser = (Parser) { 0 };
	CharString tmp = CharString_createNull();
	Bool s_uccess = true;

	SHEntryRuntime runtimeEntry = (SHEntryRuntime) { 0 };

	if(!result)
		retError(clean, Error_nullPointer(3, "Compiler_parse()::result is required"))

	//Lex & parse the file and afterwards, use it to classify all symbols

	gotoIfError3(clean, Lexer_create(settings.string, alloc, &lexer, e_rr))
	gotoIfError3(clean, Parser_create(&lexer, &parser, alloc, e_rr))
	gotoIfError3(clean, Parser_classify(&parser, alloc, e_rr))

	//If we want symbols only, then we can just ask the parser to output them for us.
	//But we do tell it that we only want symbols located in the current file

	if (symbolsOnly) {
		gotoIfError3(clean, Parser_printSymbols(parser, U32_MAX, true, alloc, &result->text, settings.path, e_rr))
		result->type = ECompileResultType_Text;
		result->isSuccess = true;
		goto clean;
	}

	result->type = ECompileResultType_SHEntryRuntime;

	gotoIfError2(clean, ListSHEntryRuntime_reserve(&result->shEntriesRuntime, parser.rootSymbols, alloc))

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

				//[uniforms()]
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
							// ^
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
								// ^

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

								CharString name = CharString_createRefStrConst(parser.symbolNames.ptr[sym.name]);
								runtimeEntry.entry.name = name;
							}

							break;

						case C8x4('s', 'h', 'a', 'd'):		//shader()

							//[shader("vertex")]
							// ^
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
								// ^

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

						case C8x4('n', 'u', 'm', 't'):		//numthreads(X, Y, Z)

							//[numthreads(X, Y, Z)]
							// ^
							if (
								tokLen == 10 &&
								*(const U64*)&tokStr.ptr[2] == C8x8('m', 't', 'h', 'r', 'e', 'a', 'd', 's')
							) {
							
								if(symj.tokenCount + 1 != 8)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() numthreads annotation expected numthreads(uint16, uint16, uint16)"
									))

								//[numthreads(X, Y, Z)]
								// ^

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_Integer  ||
									parser.tokens.ptr[symj.tokenId + 3].tokenType != ETokenType_Comma    ||
									parser.tokens.ptr[symj.tokenId + 4].tokenType != ETokenType_Integer  ||
									parser.tokens.ptr[symj.tokenId + 5].tokenType != ETokenType_Comma    ||
									parser.tokens.ptr[symj.tokenId + 6].tokenType != ETokenType_Integer  ||
									parser.tokens.ptr[symj.tokenId + 7].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() numthreads annotation expected numthreads(uint16, uint16, uint16)"
									))

								for(U8 k = 0; k < 3; ++k)
									if((parser.tokens.ptr[symj.tokenId + 2 + (k << 1)].valueu >> 16))
										retError(clean, Error_invalidParameter(
											0, 1,
											"Compiler_parse() numthreads annotation expected numthreads(uint16, uint16, uint16)"
										))

								runtimeEntry.entry.groupX = (U16) parser.tokens.ptr[symj.tokenId + 2].valueu;
								runtimeEntry.entry.groupY = (U16) parser.tokens.ptr[symj.tokenId + 4].valueu;
								runtimeEntry.entry.groupZ = (U16) parser.tokens.ptr[symj.tokenId + 6].valueu;

								U16 groupX = runtimeEntry.entry.groupX;
								U16 groupY = runtimeEntry.entry.groupY;
								U16 groupZ = runtimeEntry.entry.groupZ;

								U64 totalGroup = (U64)groupX * groupY * groupZ;

								if(!totalGroup || totalGroup > 512)
									retError(clean, Error_invalidOperation(
										0, "Compiler_parse() numthreads total group count must be in range [1, 512]"
									))

								if(U16_max(groupX, groupY) > 512)
									retError(clean, Error_invalidOperation(
										1, "Compiler_parse() numthreads x or y dimension can't exceed 512"
									))

								if(groupZ > 64)
									retError(clean, Error_invalidOperation(
										2, "Compiler_parse() numthreads z dimension can't exceed 64"
									))

							}

							break;

						case C8x4('W', 'a', 'v', 'e'):		//WaveSize(forced), WaveSize(min, max) or WaveSize(min, max, recom)

							//[WaveSize(...)]
							// ^
							if (tokLen == 8 && *(const U32*)&tokStr.ptr[4] == C8x4('S', 'i', 'z', 'e')) {
							
								if(symj.tokenCount + 1 != 4 && symj.tokenCount + 1 != 6 && symj.tokenCount + 1 != 8)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() WaveSize annotation expected WaveSize(forced), WaveSize(min, max) "
										" or WaveSize(min, max, recommended)"
									))

								//[WaveSize(X, Y, Z)]
								// ^

								U64 end = symj.tokenId + symj.tokenCount;

								if(
									parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
									parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_Integer  ||
									parser.tokens.ptr[end].tokenType != ETokenType_RoundParenthesisEnd
								)
									retError(clean, Error_invalidParameter(
										0, 1,
										"Compiler_parse() WaveSize annotation expected WaveSize(uint8)"
									))

								//4-128

								for (U8 k = 0; k < symj.tokenCount >> 1; ++k) {

									Token intToken = parser.tokens.ptr[symj.tokenId + (k << 1) + 2];

									if (k && (
										parser.tokens.ptr[symj.tokenId + (k << 1) + 1].tokenType != ETokenType_Comma ||
										intToken.tokenType != ETokenType_Integer
									))
										retError(clean, Error_invalidParameter(
											0, 1,
											"Compiler_parse() WaveSize annotation expected WaveSize(uint8, ...)"
										))

									if(intToken.valueu < 4 || intToken.valueu > 128)
										retError(clean, Error_invalidParameter(
											0, 1,
											"Compiler_parse() WaveSize annotation expected int in range [4, 128]"
										))

									F32 v = F32_log2((F32)intToken.valueu);

									if(v != (I32)v)
										retError(clean, Error_invalidParameter(
											0, 1,
											"Compiler_parse() WaveSize annotation expected base2 int in range [4, 128]"
										))

									if(symj.tokenCount + 1 == 4)
										runtimeEntry.entry.waveSize = (U8)v + 1;

									else runtimeEntry.entry.waveSize |= ((U16)v + 1) << ((k + 1) << 2);
								}
							}

							break;

						case C8x4('m', 'o', 'd', 'e'):		//model()

							//[model(6.8)]
							// ^
							if (tokLen == 5 && tokStr.ptr[4] == 'l') {
							
								if(symj.tokenCount + 1 != 4)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() model annotation expected model(double)"
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
									&runtimeEntry.shaderVersions, symj.tokenId + 2, parser, alloc, e_rr
								))
							}

							break;

						case C8x4('v', 'e', 'n', 'd'):		//vendor()

							//[vendor("NV", "AMD")]
							// ^
							if (tokLen == 6 && *(const U16*)&tokStr.ptr[4] == C8x2('o', 'r')) {
							
								if(symj.tokenCount + 1 < 4)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() vendor annotation expected vendor(string, ...)"
									))

								U32 tokenEnd = symj.tokenId + symj.tokenCount;

								if(runtimeEntry.vendorMask)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() vendor annotation can only be present once"
									))

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

						case C8x4('u', 'n', 'i', 'f'):		//uniforms()

							//[uniforms("X", "Y", "Z")]
							//[uniforms("X" = "123", "Y" = "ABC")]
							// ^
							if (tokLen == 8 && *(const U32*)&tokStr.ptr[4] == C8x4('o', 'r', 'm', 's')) {

								if(symj.tokenCount + 1 < 4)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() uniform annotation expected uniform(string, string = string, ...)"
									))

								U32 tokenEnd = symj.tokenId + symj.tokenCount;

								//[uniforms("X")]
								//         ^

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

								//[uniforms("X", "Y")]
								//             ^

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

								if(symj.tokenCount + 1 < 3)
									retError(clean, Error_invalidParameter(
										0, 0,
										"Compiler_parse() extension annotation expected extension() or extension(string, ...)"
									))

								//[extension()]
								// ^
								//Indicates an entrypoint without extensions

								if (symj.tokenCount + 1 == 3) {

									if(
										parser.tokens.ptr[symj.tokenId + 1].tokenType != ETokenType_RoundParenthesisStart ||
										parser.tokens.ptr[symj.tokenId + 2].tokenType != ETokenType_RoundParenthesisEnd
									)
										retError(clean, Error_invalidParameter(
											0, 1,
											"Compiler_parse() extension annotation expected extension()"
										))

									gotoIfError3(clean, Compiler_registerExtensions(&runtimeEntry.extensions, 0, alloc, e_rr))
									break;
								}

								U32 tokenEnd = symj.tokenId + symj.tokenCount;

								//[extension("16BitTypes")]
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

								U32 extensions = 0;

								gotoIfError3(clean, Compiler_registerExtension(
									&extensions, symj.tokenId + 2, parser, e_rr
								))

								//[extension("16BitTypes", "RayQuery")]
								//                       ^

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
										&extensions, k + 1, parser, e_rr
									))
								}

								gotoIfError3(clean, Compiler_registerExtensions(
									&runtimeEntry.extensions, extensions, alloc, e_rr
								))
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

				U16 minVersion = Compiler_minFeatureSetStage(runtimeEntry.entry.stage, runtimeEntry.entry.waveSize);

				//Ensure all shader versions are compatible with minimum featureset

				for (U64 j = 0; j < runtimeEntry.shaderVersions.length; ++j)
					if(runtimeEntry.shaderVersions.ptr[j] < minVersion)
						retError(clean, Error_invalidState(
							0,
							"Compiler_parse() one of the shader models was incompatible with minimum shader featureset "
							"of WaveSize or stage"
						))

				//Find a shader version that has the requirements.
				//If there's no model available, then it should be ok.

				Bool isRt =
					runtimeEntry.entry.stage >= ESHPipelineStage_RtStartExt &&
					runtimeEntry.entry.stage <= ESHPipelineStage_RtEndExt;

				for (U64 j = 0; j < runtimeEntry.extensions.length; ++j) {

					if(
						runtimeEntry.entry.stage != ESHPipelineStage_RaygenExt && 
						(runtimeEntry.extensions.ptr[j] & ESHExtension_RayReorder)
					)
						retError(clean, Error_invalidState(
							0,
							"Compiler_parse() one of the non raygen stages uses RayReorder, which isn't supported"
						))

					if(!isRt && (runtimeEntry.extensions.ptr[j] & ESHExtension_PAQ))
						retError(clean, Error_invalidState(
							0,
							"Compiler_parse() one of the non raytracing stages uses PAQ, which isn't supported"
						))
						
					if(
						runtimeEntry.entry.stage != ESHPipelineStage_Compute &&
						runtimeEntry.entry.stage != ESHPipelineStage_MeshExt &&
						runtimeEntry.entry.stage != ESHPipelineStage_TaskExt &&
						(runtimeEntry.extensions.ptr[j] & ESHExtension_ComputeDeriv)
					)
						retError(clean, Error_invalidState(
							0,
							"Compiler_parse() one of the non compute/mesh/task stages uses ComputeDeriv, which isn't supported"
						))

					if(!runtimeEntry.shaderVersions.length)
						continue;

					U16 reqVersion = Compiler_minFeatureSetExtension(runtimeEntry.extensions.ptr[j]);

					Bool containsValidVersion = false;

					for (U64 k = 0; k < runtimeEntry.shaderVersions.length; ++k)
						if (runtimeEntry.shaderVersions.ptr[k] >= reqVersion) {
							containsValidVersion = true;
							break;
						}

					if(!containsValidVersion)
						retError(clean, Error_invalidState(
							0, "Compiler_parse() one of the shader extensions was incompatible with all shader models"
						))
				}

				//Validate groups with stage

				Bool hasGroup   = runtimeEntry.entry.groupX | runtimeEntry.entry.groupY | runtimeEntry.entry.groupZ;
				Bool hasntGroup = !runtimeEntry.entry.groupX || !runtimeEntry.entry.groupY || !runtimeEntry.entry.groupZ;

				Bool isCompute =
					runtimeEntry.entry.stage == ESHPipelineStage_Compute ||
					runtimeEntry.entry.stage == ESHPipelineStage_WorkgraphExt;

				if(hasntGroup && isCompute)
					retError(clean, Error_invalidState(
						0,
						"Compiler_parse() found a compute/workgraph entrypoint without numthreads annotation or thread count 0"
					))

				if(hasGroup && !isCompute)
					retError(clean, Error_invalidState(
						0, "Compiler_parse() found a non compute/workgraph entrypoint with a numthreads annotation"
					))

				if(!runtimeEntry.vendorMask)
					runtimeEntry.vendorMask = U16_MAX;

				//Ready for push

				if(result->shEntriesRuntime.length + 1 >= U16_MAX)
					retError(clean, Error_invalidState(
						0, "Compiler_parse() found way too many SHEntries. Found U16_MAX!"
					))

				if(SHEntryRuntime_getCombinations(runtimeEntry) + 1 >= U16_MAX)
					retError(clean, Error_invalidState(
						0, "Compiler_parse() found way too runtimeEntry combinations. Found U16_MAX!"
					))

				gotoIfError2(clean, ListSHEntryRuntime_pushBack(&result->shEntriesRuntime, runtimeEntry, alloc));
				runtimeEntry = (SHEntryRuntime) { 0 };
			}
		}

		i += Symbol_size(sym);
	}

	result->isSuccess = true;

clean:

	if(!s_uccess && result)
		CompileResult_free(result, alloc);

	SHEntryRuntime_free(&runtimeEntry, alloc);
	Parser_free(&parser, alloc);
	Lexer_free(&lexer, alloc);
	CharString_free(&tmp, alloc);
	return s_uccess;
}
