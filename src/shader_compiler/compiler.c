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

//shader_compiler/compiler.c

#include "shader_compiler/compiler.h"
#include "types/container/list_impl.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut.h"
#include "types/base/allocator.h"
#include "types/base/time.h"
#include "types/base/c8.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

TListImpl(Compiler);
TListImpl(CompileError);
TListImpl(IncludeInfo);
TListImpl(IncludedFile);
TListImpl(CompilerEntrypoint);
TListImpl(CompileResult);
TListNamedImpl(ListU16PtrConst);
TListNamedImpl(ListU32PtrConst);

U32 CompileError_lineId(CompileError err) {
	return err.lineId | ((U32)(err.typeLineId & (U8)I8_MAX) << 16);
}

void ListCompiler_freeUnderlying(ListCompiler *compilers, const Allocator *alloc) {

	if(!compilers)
		return;

	for(U64 i = 0; i < compilers->length; ++i)
		Compiler_free(&compilers->ptrNonConst[i], alloc);

	ListCompiler_free(compilers, alloc);
}

void CompileError_free(CompileError *err, const Allocator *alloc) {

	if(!err)
		return;

	CharString_free(&err->file, alloc);
	CharString_free(&err->error, alloc);
}

ECompareResult IncludeInfo_compare(const IncludeInfo *a, const IncludeInfo *b) {

	if(!a)    return ECompareResult_Lt;
	if(!b)    return ECompareResult_Gt;

	return a->counter < b->counter || (a->counter == b->counter && a->crc32c < b->crc32c);
}

void IncludeInfo_free(IncludeInfo *info, const Allocator *alloc) {

	if(!info)
		return;

	CharString_free(&info->file, alloc);
}

void ListCompileError_freeUnderlying(ListCompileError *compileErrors, const Allocator *alloc) {

	if(!compileErrors)
		return;

	for(U64 i = 0; i < compileErrors->length; ++i)
		CompileError_free(&compileErrors->ptrNonConst[i], alloc);

	ListCompileError_free(compileErrors, alloc);
}

void ListIncludeInfo_freeUnderlying(ListIncludeInfo *includeInfos, const Allocator *alloc) {

	if(!includeInfos)
		return;

	for(U64 i = 0; i < includeInfos->length; ++i)
		IncludeInfo_free(&includeInfos->ptrNonConst[i], alloc);

	ListIncludeInfo_free(includeInfos, alloc);
}

void IncludedFile_free(IncludedFile *file, const Allocator *alloc) {

	if(!file)
		return;

	CharString_free(&file->data, alloc);
	IncludeInfo_free(&file->includeInfo, alloc);
}

void ListIncludedFile_freeUnderlying(ListIncludedFile *file, const Allocator *alloc) {

	if(!file)
		return;

	for(U64 i = 0; i < file->length; ++i)
		IncludedFile_free(&file->ptrNonConst[i], alloc);

	ListIncludedFile_free(file, alloc);
}

Bool ListIncludeInfo_stringify(const ListIncludeInfo *files, const Allocator *alloc, CharString *tempStr, Error *e_rr) {

	CharString tempStr2 = CharString_createNull();
	Bool s_uccess = true;

	//Info about includes

	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst("Includes:\n"), alloc, tempStr, e_rr));

	for(U64 i = 0; i < files->length; ++i) {

		IncludeInfo includeInfo = files->ptr[i];
		TimeFormat format = { 0 };

		if(includeInfo.timestamp)
			Time_format(includeInfo.timestamp, format, true);

		if(includeInfo.counter == 1) {
			gotoIfError3(clean, CharString_format(
				alloc, &tempStr2, e_rr,
				"%08"PRIx32" %05"PRIu32" %s%s%s\n",
				includeInfo.crc32c, includeInfo.fileSize,
				includeInfo.timestamp ? format : "", includeInfo.timestamp ? " " : "",
				includeInfo.file.ptr
			));
		}

		else {
			gotoIfError3(clean, CharString_format(
				alloc, &tempStr2, e_rr,
				"%03"PRIu64" reference(s): %08"PRIx32" %05"PRIu32" %s%s%s\n",
				includeInfo.counter,
				includeInfo.crc32c, includeInfo.fileSize,
				includeInfo.timestamp ? format : "", includeInfo.timestamp ? " " : "",
				includeInfo.file.ptr
			));
		}

		gotoIfError3(clean, CharString_appendString(tempStr, &tempStr2, alloc, e_rr));
		CharString_free(&tempStr2, alloc);
	}

clean:

	if(!s_uccess)
		CharString_free(tempStr, alloc);

	CharString_free(&tempStr2, alloc);
	return s_uccess;
}

void CompileResult_free(CompileResult *result, const Allocator *alloc) {

	if(!result)
		return;

	ListSHRegisterRuntime_freeUnderlying(&result->registers, alloc);
	ListCompileError_freeUnderlying(&result->compileErrors, alloc);
	ListIncludeInfo_freeUnderlying(&result->includeInfo, alloc);

	switch (result->type) {

		default:
		case ECompileResultType_Binary:
			Buffer_free(&result->binary, alloc);
			break;

		case ECompileResultType_SHEntryRuntime:
			ListSHEntryRuntime_freeUnderlying(&result->shEntriesRuntime, alloc);
			break;
	}

	*result = (CompileResult) { 0 };
}

void ListCompileResult_freeUnderlying(ListCompileResult *result, const Allocator *alloc) {

	if (!result)
		return;

	for (U64 i = 0; i < result->length; ++i)
		CompileResult_free(&result->ptrNonConst[i], alloc);

	ListCompileResult_free(result, alloc);
}

const C8 *ignoredWarnings[] = {

	//Can't be parsed properly:

	"validation errors",

	//Our compiler oxc:: annotations

	"unknown attribute '",        //Starts with

	"unknown attribute 'stage' ignored [-Wunknown-attributes]",        //Equals
	"unknown attribute 'model' ignored [-Wunknown-attributes]",
	"unknown attribute 'uniforms' ignored [-Wunknown-attributes]",
	"unknown attribute 'defines' ignored [-Wunknown-attributes]",
	"unknown attribute 'extension' ignored [-Wunknown-attributes]",
	"unknown attribute 'vendor' ignored [-Wunknown-attributes]",
	"unknown attribute 'binary' ignored [-Wunknown-attributes]",

	//Vulkan attributes:

	"'binding' attribute ignored",
	"'combinedImageSampler' attribute ignored",
	"'push_constant' attribute ignored",
	"'image_format' attribute ignored",

	//Inline SPIR-V attributes (used to hand-declare capabilities/extensions/instructions, e.g. atomic float add)

	"'ext_capability' attribute ignored",
	"'ext_extension' attribute ignored",
	"'ext_instruction' attribute ignored",
	"'ext_reference' attribute ignored"
};

static Bool Compiler_filterWarning(CharString str) {

	//The *Sensitive helpers take const CharString*, so materialize each ref into a named temp
	//(CharString_createRefCStrConst returns an rvalue whose address can't be taken).

	const CharString w1 = CharString_createRefCStrConst(ignoredWarnings[1]);

	if (CharString_startsWithStringSensitive(&str, &w1, 0)) {

		for (U64 i = 2; i <= 8; ++i) {
			const CharString w = CharString_createRefCStrConst(ignoredWarnings[i]);
			if (CharString_equalsStringSensitive(&str, &w))
				return true;
		}

		return false;
	}

	const CharString w0 = CharString_createRefCStrConst(ignoredWarnings[0]);

	if (CharString_startsWithStringSensitive(&str, &w0, 0))
		return true;

	for (U64 i = 9; i <= 16; ++i) {
		const CharString w = CharString_createRefCStrConst(ignoredWarnings[i]);
		if (CharString_equalsStringSensitive(&str, &w))
			return true;
	}

	return false;
}

Bool Compiler_parseErrors(CharString errs, const Allocator *alloc, ListCompileError *errors, Bool *hasErrors, Error *e_rr) {

	CharString validationFailed = CharString_createRefCStrConst("\nValidation failed.");

	errs = CharString_createRefStrConst(errs);

	U64 loc = CharString_findFirstStringSensitive(&errs, &validationFailed, 0, 0);

	if(loc != U64_MAX)
		errs.lenAndNullTerminated = loc;

	Bool s_uccess = true;
	U64 off = 0;

	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	CharString tempStr3 = CharString_createNull();

	//Error can contain "In file included from", which can throw off the parser.
	//We fix that.

	CharString lameString = CharString_createRefCStrConst("In file included from ");
	if (CharString_containsStringSensitive(&errs, &lameString, 0, 0)) {

		gotoIfError3(clean, CharString_createCopy(errs, alloc, &tempStr3, e_rr));

		U64 o = 0;
		while ((o = CharString_findFirstStringSensitive(&tempStr3, &lameString, o, 0)) != U64_MAX) {

			U64 end = CharString_findFirstSensitive(&tempStr3, '\n', o, 0);
			U64 dist = end - o + 1;

			if(end == U64_MAX)
				dist = CharString_length(tempStr3) - o;

			gotoIfError3(clean, CharString_eraseAtCount(&tempStr3, o, dist, e_rr));
		}

		errs = CharString_createRefStrConst(tempStr3);
	}

	//Error types

	CharString warning = CharString_createRefCStrConst("warning: ");
	CharString note = CharString_createRefCStrConst("note: ");
	CharString errStr = CharString_createRefCStrConst("error: ");
	CharString fatalOnly = CharString_createRefCStrConst("fatal ");

	U64 prevOff = U64_MAX;

	CharString file = CharString_createNull();
	CharString errorStart = CharString_createNull();        //First line of the error
	U64 lineId = 0;
	U64 lineOff = 0;
	Bool ignoreNextWarning = false;

	CharString prevFile = file;
	U64 prevLineId = lineId;
	U64 prevLineOff = lineOff;
	U8 prevProblemType = U8_MAX;

	//Internal compiler error can't be parsed the same way
	//If this happens, bad stuff is happening

	CharString internalCompileErrorRef = CharString_createRefCStrConst("Internal Compiler error: ");

	if (CharString_equalsStringSensitive(&errs, &internalCompileErrorRef)) {
		CompileError cerr = (CompileError) { .error = errs };
		gotoIfError3(clean, ListCompileError_pushBack(errors, cerr, alloc, e_rr));
		goto clean;
	}

	//Regular parsing

	U64 nextWarning = CharString_findFirstStringSensitive(&errs, &warning, off, 0);
	U64 nextError = CharString_findFirstStringSensitive(&errs, &errStr, off, 0);
	U64 nextNote = CharString_findFirstStringSensitive(&errs, &note, off, 0);
	U64 nextProblem = U64_min(U64_min(nextWarning, nextError), nextNote);
	U8 nextProblemType = nextProblem == nextWarning ? 0 : (nextProblem == nextNote ? 1 : 2);

	while(off < CharString_length(errs)) {

		if(nextProblem == U64_MAX)
			break;

		//" error: " can start with "fatal "

		if (nextProblemType == 2) {

			off = nextProblem + CharString_length(errStr);

			if(
				nextError >= CharString_length(fatalOnly) &&
				CharString_startsWithStringSensitive(&errs, &fatalOnly, nextError - CharString_length(fatalOnly))
			) {
				nextError -= CharString_length(fatalOnly);
				nextProblem = nextError;
				nextProblemType = 3;
			}
		}

		else if(nextProblemType == 1)
			off = nextProblem + CharString_length(note);

		else off = nextProblem + CharString_length(warning);

		//Lineless errors shouldn't try to parse x:(y:(z:)) w: where x = file, y = line, z = col, w = warning type.

		Bool isLinelessError = nextProblem == 0 || C8_isNewLine(errs.ptr[nextProblem - 1]);
		U64 ourErrorLineStart = nextProblem;

		if (!isLinelessError) {

			//We skip unrecognized errors, to prevent accidentally parsing code that contains the words "error:"
			//a:5: error:
			//     ^

			if(
				nextProblem < 5 ||
				errs.ptr[nextProblem - 1] != ' ' ||
				errs.ptr[nextProblem - 2] != ':' ||
				!C8_isDec(errs.ptr[nextProblem - 3])
			)
				goto next;

			//a:21: error:
			//   ^

			U64 it = nextProblem - 4;
			U64 multiplier = 10;
			U64 num = C8_dec(errs.ptr[nextProblem - 3]);

			for (; it != U64_MAX; --it) {

				C8 c = errs.ptr[it];
				U8 dec = C8_dec(c);

				if(dec == U8_MAX && c != ':')
					goto next;

				if(c == ':') {
					--it;
					break;
				}

				num += dec * multiplier;
				multiplier *= 10;

				if(num >> 32)        //Out of bounds
					retError(clean, Error_invalidState(0, "Compiler_parseErrors() num is limited to U32"));
			}

			if(it == U64_MAX || it == 0)    //Invalid, skip
				goto next;

			U64 secondNum = num;

			//a:20:21: error:
			//   ^
			//May also be:
			//a20:21 (where a20 = file)
			//  ^

			num = C8_dec(errs.ptr[it--]);
			multiplier = 10;

			Bool hasSecondNum = num != U8_MAX;

			if (hasSecondNum) {

				for (; it != U64_MAX; --it) {

					C8 c = errs.ptr[it];
					U8 dec = C8_dec(c);

					if (dec == U8_MAX && c != ':') {
						hasSecondNum = false;
						break;
					}

					if(c == ':') {
						--it;
						break;
					}

					num += dec * multiplier;
					multiplier *= 10;

					if(num >> 32)        //Out of bounds
						retError(clean, Error_invalidState(1, "Compiler_parseErrors() lineId is limited to U32"));
				}
			}

			//:lineId:offset: error:
			//:lineId: error:

			if (hasSecondNum) {

				if(secondNum >> 16)        //Out of U16
					retError(clean, Error_invalidState(1, "Compiler_parseErrors() charId is limited to U16"));

				lineId = (U32) num;
				lineOff = (U16) secondNum;
			}

			else lineId = (U32) num;

			//Prefixed by file (D:\test.hlsl:24:3: error: Whoopsie)

			U64 fileEnd = it;

			while(it && !C8_isNewLine(errs.ptr[it]))
				--it;

			U64 fileStart = it;

			file = CharString_createRefSizedConst(errs.ptr + fileStart, fileEnd - fileStart + 1, false);
			ourErrorLineStart = fileStart;
		}

		else {
			lineOff = 0;
			lineId = 0;
			file = CharString_createNull();
		}

		//Grab first line until eof or newline

		U64 lineStart = off;

		for(; off < CharString_length(errs); ++off)
			if (C8_isNewLine(errs.ptr[off])) {
				++off;
				break;
			}

		U64 lineEnd = off - 1;
		errorStart = CharString_createRefSizedConst(errs.ptr + lineStart, lineEnd - lineStart, false);

		//Now that we know we have an error, we have to register the previous one

		if (prevOff != U64_MAX && !ignoreNextWarning) {

			CharString errorStr = CharString_createRefSizedConst(errs.ptr + prevOff, ourErrorLineStart - prevOff, false);
			gotoIfError3(clean, CharString_createCopy(errorStr, alloc, &tempStr, e_rr));
			gotoIfError3(clean, CharString_createCopy(prevFile, alloc, &tempStr2, e_rr));

			CompileError cerr = (CompileError) {
				.lineId = (U16) prevLineId,
				.typeLineId = (U8) ((prevLineId >> 16) | ((U8)(prevProblemType >= 2) << 7)),
				.lineOffset = (U8) prevLineOff,
				.error = tempStr,
				.file = tempStr2
			};

			gotoIfError3(clean, ListCompileError_pushBack(errors, cerr, alloc, e_rr));
			tempStr = tempStr2 = CharString_createNull();
		}

		if(nextProblemType >= 2)
			if(hasErrors) *hasErrors = true;

		//Update for next

		if(Compiler_filterWarning(errorStart) || (nextProblemType == 1))
			ignoreNextWarning = true;

		else ignoreNextWarning = false;

		prevOff = lineStart;
		prevProblemType = nextProblemType;
		prevLineId = lineId;
		prevLineOff = lineOff;
		prevFile = file;

	next:

		//Search again to ensure we didn't accidentally skip anything

		if (nextProblemType == 0)
			nextWarning = CharString_findFirstStringSensitive(&errs, &warning, off, 0);

		else if(nextProblemType == 1)
			nextNote = CharString_findFirstStringSensitive(&errs, &note, off, 0);

		else nextError = CharString_findFirstStringSensitive(&errs, &errStr, off, 0);

		nextProblem = U64_min(U64_min(nextWarning, nextError), nextNote);
		nextProblemType = nextProblem == nextWarning ? 0 : (nextProblem == nextNote ? 1 : 2);
	}

	//Register last error

	if (prevOff != U64_MAX && !ignoreNextWarning) {

		CharString errorStr = CharString_createRefSizedConst(errs.ptr + prevOff, CharString_length(errs) - prevOff + 1, false);
		gotoIfError3(clean, CharString_createCopy(errorStr, alloc, &tempStr, e_rr));
		gotoIfError3(clean, CharString_createCopy(prevFile, alloc, &tempStr2, e_rr));

		CompileError cerr = (CompileError) {
			.lineId = (U16) prevLineId,
			.typeLineId = (U8) ((prevLineId >> 16) | ((U8)(prevProblemType >= 2) << 7)),
			.lineOffset = (U8) prevLineOff,
			.error = tempStr,
			.file = tempStr2
		};

		gotoIfError3(clean, ListCompileError_pushBack(errors, cerr, alloc, e_rr));
		tempStr = tempStr2 = CharString_createNull();
	}

clean:
	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	CharString_free(&tempStr3, alloc);
	return s_uccess;
}

static Bool Compiler_paddingCheck(
	const SBFile *sb,
	U64 binaryId,
	U8 binaryType,
	U16 parentId,
	U32 offset,
	U32 expectedSize,
	const SHRegisterRuntime *reg,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	Bool isPacked = sb->flags & ESBSettingsFlags_IsTightlyPacked;

	U32 startOffset = offset;
	U32 unpaddedOffset = offset;

	CharString parentName = parentId == U16_MAX ? reg->name : sb->names.entryStrings.ptr[sb->structs.length + parentId];

	for (U16 i = 0; i < (U16) sb->vars.length; ++i) {

		SBVar var = sb->vars.ptr[i];

		if(var.parentId != parentId)
			continue;

		CharString varName = sb->names.entryStrings.ptr[sb->structs.length + i];
		U32 var1D = 1;

		if(var.arrayDimOrArrayId) {

			//arrayDimOrArrayId: bit 15 set = arrayId into sb->arrays (multi-dim); otherwise an inline dimension.

			ListU32 array = (ListU32) { 0 };
			U32 v = var.arrayDimOrArrayId;

			if (var.arrayDimOrArrayId >> 15)
				array = sb->arrays.ptr[var.arrayDimOrArrayId & (U16)I16_MAX];
			else ListU32_createRefConst(&v, 1, &array, NULL);

			for(U64 j = 0; j < array.length; ++j)
				var1D *= array.ptr[j];
		}

		if (var.offset != unpaddedOffset) {

			if(var.offset > unpaddedOffset)
				Log_warnLn(
					alloc,
					"Binary %"PRIu64" has variable \"%.*s.%.*s\" (%.*s) which incurs "
					"%"PRIu32" bytes of padding in front of it. Might be inefficient and/or unexpected",
					binaryId,
					(int) CharString_length(parentName), parentName.ptr,
					(int) CharString_length(varName), varName.ptr,
					(int) CharString_length(reg->name), reg->name.ptr,
					var.offset - unpaddedOffset
				);

			unpaddedOffset = var.offset;
		}

		//If a type isn't properly packed, it will give a warning too

		if (var.structId == U16_MAX) {

			U32 siz = ESBType_getSize(var.type, isPacked);

			if (!isPacked) {

				U32 packedSize = ESBType_getSize(var.type, true);

				if(packedSize != siz)
					Log_warnLn(
						alloc,
						"Binary %"PRIu64" has variable \"%.*s.%.*s\" (%.*s) which incurs %"PRIu32" bytes of padding. "
						"Might be inefficient and/or unexpected",
						binaryId,
						(int) CharString_length(parentName), parentName.ptr,
						(int) CharString_length(varName), varName.ptr,
						(int) CharString_length(reg->name), reg->name.ptr,
						(siz - packedSize) * var1D
					);

				if(var1D > 1 && (siz & 15))
					Log_warnLn(
						alloc,
						"Binary %"PRIu64" has variable \"%.*s.%.*s\" (%.*s) which incurs %"PRIu32" bytes of padding "
						"per array index. "
						"Might be inefficient and/or unexpected (total of %"PRIu32" bytes)",
						binaryId,
						(int) CharString_length(parentName), parentName.ptr,
						(int) CharString_length(varName), varName.ptr,
						(int) CharString_length(reg->name), reg->name.ptr,
						16 - (siz & 15),
						(16 - (siz & 15)) * (var1D - 1)
					);
			}

			//Remember size so struct size can also be checked for padding

			unpaddedOffset += ((siz + 15) &~ 15) * (var1D - 1) + siz;
			continue;
		}

		//Struct, so recursive

		U32 stride = sb->structs.ptr[var.structId].stride;

		gotoIfError3(clean, Compiler_paddingCheck(
			sb, binaryId, binaryType, i, unpaddedOffset, stride, reg, alloc, e_rr
		));

		if(!isPacked && var1D > 1 && (stride & 15))
			Log_warnLn(
				alloc,
				"Binary %"PRIu64" has variable \"%.*s.%.*s\" (%.*s) which incurs %"PRIu32" bytes of padding per array index. "
				"Might be inefficient and/or unexpected (total of %"PRIu32" bytes)",
				binaryId,
				(int) CharString_length(parentName), parentName.ptr,
				(int) CharString_length(varName), varName.ptr,
				(int) CharString_length(reg->name), reg->name.ptr,
				16 - (stride & 15),
				(16 - (stride & 15)) * (var1D - 1)
			);

		unpaddedOffset += ((stride + 15) &~ 15) * (var1D - 1) + stride;
	}

	//Padding occurred in struct

	if (unpaddedOffset - startOffset < expectedSize)
		Log_warnLn(
			alloc,
			"Binary %"PRIu64" has variable \"%.*s\" (%.*s) which incurs %"PRIu32" bytes of padding at the end. "
			"Might be inefficient and/or unexpected",
			binaryId,
			(int) CharString_length(parentName), parentName.ptr,
			(int) CharString_length(reg->name), reg->name.ptr,
			expectedSize - (unpaddedOffset - startOffset)
		);

clean:
	return s_uccess;
}

Bool Compiler_handleExtraWarnings(const SHFile *file, ECompilerWarning warning, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!warning)
		goto clean;

	for (U64 i = 0; i < file->binaries.length; ++i)
		for (U64 j = 0; j < file->binaries.ptr[i].registers.length; ++j) {

			SHRegisterRuntime reg = file->binaries.ptr[i].registers.ptr[j];
			Bool hadFirstPaddingScan = false;

			for (U8 k = 0; k < EGfxBinaryType_Count; ++k) {

				//Unused register

				if (
					(warning & ECompilerWarning_UnusedRegisters) &&
					reg.reg.bindings.arrU64[k] != U64_MAX && !((reg.reg.isUsedFlag >> k) & 1)
				)
					Log_warnLn(
						alloc, "Binary %"PRIu64":%s has unused register \"%.*s\"",
						i,
						EGfxBinaryType_names[k],
						(int) CharString_length(reg.name), reg.name.ptr
					);

				//Unused constant or buffer padding

				if (
					(warning & (ECompilerWarning_UnusedConstants | ECompilerWarning_BufferPadding)) &&
					reg.reg.bindings.arrU64[k] != U64_MAX && reg.shaderBuffer.vars.ptr
				) {

					SBFile sb = reg.shaderBuffer;
					U16 parent = U16_MAX;

					//Unused constant checking

					if(warning & ECompilerWarning_UnusedConstants)
						for (U64 l = 0; l < sb.vars.length; ++l) {

							SBVar var = sb.vars.ptr[l];
							CharString varName = sb.names.entryStrings.ptr[sb.structs.length + l];

							if(var.parentId != parent || ((var.flags >> k) & 1))
								continue;

							Log_warnLn(
								alloc, "Binary %"PRIu64":%s has unused constant \"%.*s.%.*s\"",
								i,
								EGfxBinaryType_names[k],
								(int) CharString_length(reg.name), reg.name.ptr,
								(int) CharString_length(varName), varName.ptr
							);
						}

					//Padding in buffer

					if (warning & ECompilerWarning_BufferPadding) {

						if(!hadFirstPaddingScan)
							gotoIfError3(clean, Compiler_paddingCheck(
								&sb, i, k, U16_MAX, 0, reg.shaderBuffer.bufferSize, &reg, alloc, e_rr
							));

						hadFirstPaddingScan = true;
					}
				}
			}
		}

clean:
	return s_uccess;
}

Bool Compiler_disassembleSPIRV(Buffer buf, const Allocator *alloc, CharString *result, Error *e_rr);
Bool Compiler_disassembleDXIL(const Compiler *comp, Buffer buf, const Allocator *alloc, CharString *result, Error *e_rr);

Bool Compiler_disassemble(
	const Compiler *comp, EGfxBinaryType type, Buffer buf, const Allocator *alloc, CharString *result, Error *e_rr
) {

	Bool s_uccess = true;

	switch (type) {

		case EGfxBinaryType_SPIRV:
			gotoIfError3(clean, Compiler_disassembleSPIRV(buf, alloc, result, e_rr));
			break;

		case EGfxBinaryType_DXIL:
			gotoIfError3(clean, Compiler_disassembleDXIL(comp, buf, alloc, result, e_rr));
			break;

		default:
			retError(clean, Error_unimplemented(0, "Compiler_createDisassembly() has invalid type"));
	}

clean:
	return s_uccess;
}

Bool Compiler_assembleSPIRV(CharString text, const Allocator *alloc, Buffer *result, Error *e_rr);
Bool Compiler_assembleDXIL(const Compiler *comp, CharString text, const Allocator *alloc, Buffer *result, Error *e_rr);

Bool Compiler_assemble(
	const Compiler *comp, EGfxBinaryType type, CharString text, const Allocator *alloc, Buffer *result, Error *e_rr
) {

	Bool s_uccess = true;

	switch (type) {

		case EGfxBinaryType_SPIRV:
			gotoIfError3(clean, Compiler_assembleSPIRV(text, alloc, result, e_rr));
			break;

		case EGfxBinaryType_DXIL:
			gotoIfError3(clean, Compiler_assembleDXIL(comp, text, alloc, result, e_rr));
			break;

		default:
			retError(clean, Error_unimplemented(0, "Compiler_assemble() has invalid type"));
	}

clean:
	return s_uccess;
}

Bool Compiler_processSPIRV(
	Buffer *result,                      //Required; input & output SPIRV (will be optimized)
	ListSHRegisterRuntime *registers,    //Required; Output registers
	Bool isDebug,
	Bool keepRegisters,                  //Keep declared but unused resources bound and reflected
	const SHBinaryIdentifier *toCompile,
	SpinLock *lock,                      //If not NULL will be used before writing into entries
	const ListSHEntryRuntime *entries,   //Array contains the current buffer's reflection for the entry & compatibility checks
	Bool isLibTarget,
	ESHExtension *demotions,             //Required; specifies which extensions aren't used (useful for demoting unused ones)
	ListCompileError *errors,
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_processDXIL(
	const Compiler *compiler,            //To be able to get reflection data
	Buffer *result,                      //Required; input & output DXIL
	ListSHRegisterRuntime *registers,    //Required; Output registers
	Bool isDebug,
	const SHBinaryIdentifier *toCompile,
	SpinLock *lock,                      //If not NULL will be used before writing into entries
	const ListSHEntryRuntime *entries,   //Array contains the current buffer's reflection for the entry and compatibility checks
	ESHExtension *demotions,             //Required; specifies which extensions aren't used (useful for demoting unused ones)
	ListCompileError *errors,
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_process(
	const Compiler *compiler,
	EGfxBinaryType type,
	Buffer *result,
	ListSHRegisterRuntime *registers,
	Bool isDebug,
	Bool keepRegisters,
	const SHBinaryIdentifier *toCompile,
	SpinLock *lock,
	const ListSHEntryRuntime *entries,
	Bool isLib,
	ESHExtension *demotions,
	ListCompileError *errors,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	switch (type) {

		case EGfxBinaryType_SPIRV:

			gotoIfError3(clean, Compiler_processSPIRV(
				result, registers, isDebug, keepRegisters, toCompile, lock, entries, isLib, demotions, errors, alloc, e_rr
			));

			break;

		case EGfxBinaryType_DXIL:

			gotoIfError3(clean, Compiler_processDXIL(
				compiler, result, registers, isDebug, toCompile, lock, entries, demotions, errors, alloc, e_rr
			));

			break;

		default:
			retError(clean, Error_unimplemented(0, "Compiler_process() has invalid type"));
	}

clean:
	return s_uccess;
}

Bool Compiler_linkSPIRV(
	const Compiler *compiler,
	const ListBuffer *inputs,              //Input SPIRV(s); library data
	const ListSHUniformRuntime *uniforms,  //Uniform descriptions (to index uniformData and to link)
	Buffer uniformData,                    //Contents of the current compilation
	const CharString *entrypoint,          //Entrypoint specialization (empty = keep as lib, otherwise specialize)
	EGfxPipelineStage stage,               //Whether or not to be a final executable (EGfxPipelineStage_Count = keep library)
	ESHExtension exts,
	ListCompileError *errors,
	Buffer *result,                        //Output SPIRV: Either library or specialized binary (PS/GS/CS/etc.)
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_linkDXIL(
	const Compiler *compiler,
	const ListBuffer *inputs,              //Input DXIL(s); library data
	const ListSHUniformRuntime *uniforms,  //Uniform descriptions (to index uniformData and to link)
	Buffer uniformData,                    //Contents of the current compilation
	const CharString *entrypoint,          //Entrypoint specialization (empty = keep as lib, otherwise specialize)
	U16 shaderVersion,                     //U8 maj, minor
	EGfxPipelineStage stageType,
	ESHExtension exts,
	ListCompileError *errors,
	Buffer *result,                        //Output DXIL: Either library or specialized binary (PS/GS/CS/etc.)
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_link(
	const Compiler *compiler,
	EGfxBinaryType type,
	const ListBuffer *inputs,
	const ListSHUniformRuntime *uniforms,
	Buffer uniformData,
	const CharString *entrypoint,
	U16 shaderVersion,
	EGfxPipelineStage stageType,
	ESHExtension exts,
	ListCompileError *errors,
	Buffer *result,
	const Allocator *alloc,
	Error *e_rr
) {
	
	Bool s_uccess = true;

	if(uniforms->length >> 8)
		retError(clean, Error_invalidState(0, "Compiler_link() uniforms->length is capped to 256"));

	switch (type) {

		case EGfxBinaryType_SPIRV:

			gotoIfError3(clean, Compiler_linkSPIRV(
				compiler, inputs, uniforms, uniformData, entrypoint, stageType, exts, errors, result, alloc, e_rr
			));

			break;

		case EGfxBinaryType_DXIL:

			gotoIfError3(clean, Compiler_linkDXIL(
				compiler, inputs, uniforms, uniformData, entrypoint, shaderVersion, stageType, exts, errors, result,
				alloc, e_rr
			));

			break;

		default:
			retError(clean, Error_unimplemented(0, "Compiler_link() has invalid type"));
	}

clean:
	return s_uccess;
}

void ListCompilerEntrypoint_freeUnderlying(ListCompilerEntrypoint *entry, const Allocator *alloc) {

	if (!entry)
		return;

	for (U64 i = 0; i < entry->length; ++i)
		CharString_free(&entry->ptrNonConst[i].name, alloc);

	ListCompilerEntrypoint_free(entry, alloc);
}
