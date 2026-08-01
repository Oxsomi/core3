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

//types/container/file_base.c

#include "types/base/mathi.h"
#include "types/base/allocator.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"
#include "types/container/string.h"
#include "types/container/string_helper.h"
#include "types/container/file_base.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/constants.h"

Bool File_resolve(
	const CharString *locPtr,
	Bool *isVirtual,
	U64 maxFilePathLimit,
	const CharString *absoluteDirPtr,
	const Allocator *alloc,
	CharString *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocate = false;
	ListCharString res = { 0 };

	if(!locPtr || !isVirtual || !absoluteDirPtr)
		retError(clean, Error_nullPointer(!locPtr ? 0 : (!isVirtual ? 1 : 3), "File_resolve()::isVirtual is required"));

	CharString loc = CharString_createRefStrConst(*locPtr);
	CharString absoluteDir = CharString_createRefStrConst(*absoluteDirPtr);

	//The bare virtual root ("//", "//.", "//./") has no section component and resolves to the empty virtual root.
	//Handle it explicitly: the trailing-slash trim below would collapse "//" into "/" (no longer virtual),
	// and the split machinery can't represent the empty remainder left after popping "//".

	if(File_isVirtual(loc)) {

		U64 afterLen = CharString_length(loc) - 2;                             //Part after the leading "//"

		if(afterLen && loc.ptr[CharString_length(loc) - 1] == '/')             //Ignore one trailing slash
			--afterLen;

		if(!afterLen || (afterLen == 1 && loc.ptr[2] == '.')) {                //Empty or a single "."
			*isVirtual = true;
			*result = CharString_createNull();                                 //Empty == virtual root
			goto clean;
		}
	}

	if(CharString_getAt(loc, CharString_length(loc) - 1) == '/')                    //myTest/ <--
		loc.lenAndNullTerminated = CharString_length(loc) - 1;

	if(CharString_getAt(absoluteDir, CharString_length(absoluteDir) - 1) == '/')    //base/ <--
		absoluteDir.lenAndNullTerminated = CharString_length(absoluteDir) - 1;

	U64 abDirLen = CharString_length(absoluteDir);

	if (CharString_equalsStringSensitive(&loc, &absoluteDir)) {
		gotoIfError3(clean, CharString_createCopy(loc, alloc, result, e_rr));
		goto clean;
	}

	if (!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_resolve()::loc is not a valid file path"));

	//Copy string so we can modify it

	gotoIfError3(clean, CharString_createCopy(loc, alloc, result, e_rr));
	allocate = true;
	*isVirtual = File_isVirtual(loc);

	//Virtual files

	if (*isVirtual)
		gotoIfError3(clean, CharString_popFrontCount(result, 2, e_rr));

	//Network drives are a thing on windows and allow starting a path with "\\"
	//We shouldn't be supporting this.
	//The reason; you can access servers from an app with a file read instead of a https read,
	//which can obfuscate the application's true intentions.
	//ex. \\0.0.0.0\ would make a file request to 0.0.0.0.
	//Unix can map a folder to a webserver, but that link has to be created beforehand, not by an app.
	//You can also read from hardware in a platform dependent way, which makes it harder to standardize.
	//TODO: We should however support this in the future as a custom instruction that allows it such as //network/

	if (CharString_getAt(*result, 0) == '\\' && CharString_getAt(*result, 1) == '\\')
		retError(clean, Error_unsupportedOperation(3, "File_resolve()::loc can't start with \\\\"));

	//Backslash is replaced with forward slash for easy windows compatibility

	if (!CharString_replaceAllSensitive(result, '\\', '/', 0, 0))
		retError(clean, Error_invalidOperation(1, "File_resolve() can't replaceAll"));

	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)
	//We also obviously don't support 0:\ and such or A:/ on unix

	#ifdef _WIN32
		if(CharString_length(*result) >= 3 && result->ptr[1] == ':' && (result->ptr[2] != '/' || !C8_isAlpha(result->ptr[0])))
			retError(clean, Error_unsupportedOperation(2, "File_resolve() only supports Windows paths with [A-Z]:/*"));
	#else
		if(CharString_length(*result) >= 2 && result->ptr[1] == ':')
			retError(clean, Error_invalidOperation(6, "File_resolve() doesn't support Windows paths outside of Windows."));
	#endif

	//Now we have to discover the real directory it references to. This means resolving:
	//Empty filename and . to mean no difference and .. to step back

	const CharStringSplit split = { .s = result, .allocator = alloc, .result = &res };
	gotoIfError3(clean, CharString_splitSensitive(&split, '/', e_rr));

	U64 fakeSplitLen = res.length;

	const CharString back = CharString_createRefCStrConst("..");

	for (U64 i = 0; i < fakeSplitLen; ++i) {

		//We pop from ListCharString since it doesn't change anything
		//Starting with a / is valid with local files, so don't remove it. (not with virtual files)
		//Having multiple // after each other means empty file this is invalid.
		//So both empty file and . resolve to nothing

		if(
			(CharString_isEmpty(res.ptr[i]) && i && !*isVirtual) ||
			CharString_equalsSensitive(res.ptr[i], '.')
		) {

			//Move to left

			//This is OK, we're dealing with refs from split
			const U64 moveCount = fakeSplitLen - 1 - i;

			if(moveCount)
				Buffer_memmove(
					Buffer_createRef(&res.ptrNonConst[i], moveCount * sizeof(res.ptr[0])),
					Buffer_createRef(&res.ptrNonConst[i + 1], moveCount * sizeof(res.ptr[0]))
				);

			--i;            //Ensure we keep track of the removed element
			--fakeSplitLen;
			continue;
		}

		//In this case, we have to pop ListCharString[j], so that's only possible if that's still there

		if (CharString_equalsStringSensitive(&res.ptr[i], &back)) {

			if(!i)
				retError(clean, Error_invalidParameter(
					0, 0, "File_resolve()::loc tried to exit working directory, this is not allowed for security reasons"
				));

			//This is OK, we're dealing with refs from split
			const U64 moveCount = fakeSplitLen - 1 - i;

			if(moveCount)
				Buffer_memmove(
					Buffer_createRef(&res.ptrNonConst[i - 1], moveCount * sizeof(res.ptr[0])),
					Buffer_createRef(&res.ptrNonConst[i + 1], moveCount * sizeof(res.ptr[0]))
				);

			i -= 2;                                                //Ensure we keep track of the removed element
			fakeSplitLen -= 2;
			continue;
		}

		//If we start with /, it's still valid for non Windows systems.

		#ifndef _WIN32
			if(!i && CharString_isEmpty(res.ptr[i]))
				continue;
		#endif

		//Validate file name

		if (!CharString_isValidFileName(res.ptr[i])) {

			#ifdef _WIN32

				//Drive name

				if(!i && CharString_length(res.ptr[i]) == 2 && C8_isAlpha(res.ptr[0].ptr[0]) && res.ptr[0].ptr[1] == ':')
					continue;

			#endif

			retError(clean, Error_invalidParameter(0, 1, "File_resolve()::loc contains subpath with invalid file name"));
		}

		//Continue processing the path until it's done

		continue;
	}

	//If we have nothing left, we get current work/app directory

	if(!fakeSplitLen) {

		if (*isVirtual) {                          //A virtual root (e.g. "//") stays empty
			CharString_free(result, alloc);        //Release temp result
			goto clean;
		}

		//A non-virtual path that collapsed to nothing (e.g. "." or "./") means the working/app directory

		CharString_free(result, alloc);        //Release temp result
		gotoIfError3(clean, CharString_createCopy(absoluteDir, alloc, result, e_rr));
		goto clean;
	}

	//Re-assemble path now

	CharString tmp = CharString_createNull();
	ListCharString tmpList = ListCharString_createRefFromList(res);
	tmpList.length = fakeSplitLen;
	const ListCharStringConcat concat = { .arr = &tmpList, .alloc = alloc, .result = &tmp };
	gotoIfError3(clean, ListCharString_concat(&concat, '/', e_rr));

	CharString_free(result, alloc);        //This can't be done before concat, because the string is still in use.
	*result = tmp;

	ListCharString_free(&res, alloc);

	//Check if we're an absolute or relative path

	Bool isAbsolute = false;

	#ifdef _WIN32    //Starts with [A-Z]:/ if absolute. If it starts with / it's unsupported!

		if (CharString_startsWithSensitive(*result, '/', 0))
			retError(clean, Error_unsupportedOperation(
				4, "File_resolve()::loc contained Unix path (/absolute), which is unsupported on Windows"
			));

		isAbsolute = CharString_length(*result) >= 2 && result->ptr[1] == ':';

	#else            //Starts with / if absolute
		isAbsolute = CharString_startsWithSensitive(*result, '/', 0);
	#endif

	//Our path has to be made relative to our working directory.
	//This is to avoid access from folders we shouldn't be able to read in.

	if (isAbsolute) {

		if(
			!abDirLen ||
			!(
				CharString_startsWithStringSensitive(result, &absoluteDir, 0) &&
				(CharString_length(*result) == abDirLen || CharString_getAt(*result, abDirLen) == '/')
			)
		)
			retError(clean, Error_unauthorized(
				0, "File_resolve()::loc tried to escape working directory, which is unsupported for security reasons"
			));
	}

	//Prepend our path

	else if(abDirLen && !*isVirtual) {
		gotoIfError3(clean, CharString_insert(result, '/', 0, alloc, e_rr));
		gotoIfError3(clean, CharString_insertString(result, &absoluteDir, 0, alloc, e_rr));
	}

	//Ensure we can use stack allocated strings for example since we know the max size.

	if(CharString_length(*result) > MAX_OXC_PATH)
		retError(clean, Error_outOfBounds(
			0, CharString_length(*result), maxFilePathLimit, "File_resolve()::loc resolved path is longer than max file limit"
		));

clean:

	if(!s_uccess && allocate)
		CharString_free(result, alloc);

	ListCharString_free(&res, alloc);
	return s_uccess;
}

Bool File_makeRelative(
	const CharString absoluteDir,
	const CharString base,
	const CharString subFile,
	U64 maxFilePathLimit,
	const Allocator *alloc,
	CharString *result,
	Error *e_rr
) {
	Bool s_uccess = true;
	Bool isVirtualBase = false, isVirtualSub = false;
	CharString resolvedBase = CharString_createNull();
	CharString resolvedSubFile = CharString_createNull();
	CharString relPath = CharString_createNull();

	if(!result)
		retError(clean, Error_nullPointer(5, "File_makeRelative()::result is required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(5, 0, "File_makeRelative()::*result must be empty"));

	U64 absDirLen = CharString_length(absoluteDir);

	gotoIfError3(clean, File_resolve(&base, &isVirtualBase, maxFilePathLimit, &absoluteDir, alloc, &resolvedBase, e_rr));
	gotoIfError3(clean, File_resolve(&subFile, &isVirtualSub, maxFilePathLimit, &absoluteDir, alloc, &resolvedSubFile, e_rr));

	if(isVirtualBase != isVirtualSub)
		retError(clean, Error_invalidParameter(
			5, 0, "File_makeRelative() one of base or subFile is virtual while the other isn't"
		));

	CharString baseAbsDir = CharString_createNull();
	CharString subFileAbsDir = CharString_createNull();
	CharString_cut(&resolvedBase, absDirLen, 0, &baseAbsDir);
	CharString_cut(&resolvedSubFile, absDirLen, 0, &subFileAbsDir);

	U64 it        = 0;
	U64 baseLen    = CharString_length(baseAbsDir);
	U64 subLen    = CharString_length(subFileAbsDir);

	U64 commonEnd = 0;    //Index of last character in common

	//Find all common folders

	while (it < baseLen && it < subLen) {

		U64 baseSlash = CharString_findFirstSensitive(&baseAbsDir, '/', it, 0);
		U64 subSlash  = CharString_findFirstSensitive(&subFileAbsDir, '/', it, 0);

		if (baseSlash != subSlash || baseSlash == U64_MAX)
			break;

		U64 baseSegLen = baseSlash != U64_MAX ? baseSlash - it : baseLen - it;
		U64 subSegLen = subSlash != U64_MAX ? subSlash - it : subLen - it;

		const CharString baseStr = CharString_createRefSizedConst(baseAbsDir.ptr + it, baseSegLen, false);
		const CharString subStr = CharString_createRefSizedConst(subFileAbsDir.ptr + it, subSegLen, false);

		if (!CharString_equalsStringSensitive(&baseStr, &subStr))
			break;

		it = (baseSlash != U64_MAX) ? baseSlash + 1 : baseLen;;
		commonEnd = it;
	}
	
	//Reserve relPath first

	U64 remainingBaseSlashes = CharString_countAllSensitive(&baseAbsDir, '/', commonEnd);
	U64 reserveSize = 3 * remainingBaseSlashes + (subLen > commonEnd ? subLen - commonEnd : 0);
	gotoIfError3(clean, CharString_reserve(&relPath, reserveSize, alloc, e_rr));

	//Append ../ as much as needed until we're back to the common folder

	const CharString dotDotSlash = CharString_createRefCStrConst("../");

	U64 j = commonEnd;
	while (j < baseLen) {

		U64 nextNonSlash = CharString_findFirstSensitive(&baseAbsDir, '/', j, 0);

		if (nextNonSlash == j) {
			++j;
			continue;
		}

		if (nextNonSlash == U64_MAX)
			nextNonSlash = baseLen;

		gotoIfError3(clean, CharString_appendString(&relPath, &dotDotSlash, alloc, e_rr));
		j = nextNonSlash + 1;
	}

	//Append the last part that's different

	if (commonEnd < subLen) {
		CharString remainder = CharString_createRefSizedConst(subFileAbsDir.ptr + commonEnd, subLen - commonEnd, false);
		gotoIfError3(clean, CharString_appendString(&relPath, &remainder, alloc, e_rr));
	}

	*result = relPath;
	relPath = CharString_createNull();

clean:
	CharString_free(&relPath, alloc);
	CharString_free(&resolvedBase, alloc);
	CharString_free(&resolvedSubFile, alloc);
	return s_uccess;
}

void FileInfo_free(FileInfo *info, const Allocator *alloc) {

	if (!info)
		return;

	CharString_free(&info->path, alloc);
	*info = (FileInfo){ 0 };
}
