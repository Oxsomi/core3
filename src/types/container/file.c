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

#include "types/math/math.h"
#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/container/file.h"
#include "types/base/error.h"

Bool FileInfo_free(FileInfo *info, Allocator alloc) {

	if(!info)
		return false;

	const Bool freed = CharString_free(&info->path, alloc);
	*info = (FileInfo) { 0 };
	return freed;
}

Bool File_isVirtual(CharString loc) { return CharString_getAt(loc, 0) == '/' && CharString_getAt(loc, 1) == '/'; }

Bool File_resolve(
	CharString loc,
	Bool *isVirtual,
	U64 maxFilePathLimit,
	CharString absoluteDir,
	Allocator alloc,
	CharString *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocate = false;
	ListCharString res = (ListCharString) { 0 };

	if(!isVirtual || !result)
		retError(clean, Error_nullPointer(!isVirtual ? 1 : 2, "File_resolve()::result and isVirtual are required"))

	if(result->ptr)
		retError(clean, Error_invalidOperation(0, "File_resolve()::result is not NULL, this might indicate a memleak"))

	loc = CharString_createRefStrConst(loc);
	absoluteDir = CharString_createRefStrConst(absoluteDir);

	if(CharString_getAt(loc, CharString_length(loc) - 1) == '/')						//myTest/ <--
		loc.lenAndNullTerminated = CharString_length(loc) - 1;

	if(CharString_getAt(absoluteDir, CharString_length(absoluteDir) - 1) == '/')	//base/ <--
		absoluteDir.lenAndNullTerminated = CharString_length(absoluteDir) - 1;

	U64 abDirLen = CharString_length(absoluteDir);

	if (CharString_equalsStringSensitive(loc, absoluteDir)) {
		gotoIfError2(clean, CharString_createCopy(loc, alloc, result))
		goto clean;
	}

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_resolve()::loc is not a valid file path"))

	//Copy string so we can modify it

	gotoIfError2(clean, CharString_createCopy(loc, alloc, result))
	allocate = true;
	*isVirtual = File_isVirtual(loc);

	//Virtual files

	if (*isVirtual)
		gotoIfError2(clean, CharString_popFrontCount(result, 2))

	//Network drives are a thing on windows and allow starting a path with "\\"
	//We shouldn't be supporting this.
	//The reason; you can access servers from an app with a file read instead of a https read,
	//which can obfuscate the application's true intentions.
	//ex. \\0.0.0.0\ would make a file request to 0.0.0.0.
	//Unix can map a folder to a webserver, but that link has to be created beforehand, not by an app.
	//You can also read from hardware in a platform dependent way, which makes it harder to standardize.
	//TODO: We should however support this in the future as a custom instruction that allows it such as //network/

	if (CharString_getAt(*result, 0) == '\\' && CharString_getAt(*result, 1) == '\\')
		retError(clean, Error_unsupportedOperation(3, "File_resolve()::loc can't start with \\\\"))

	//Backslash is replaced with forward slash for easy windows compatibility

	if (!CharString_replaceAllSensitive(result, '\\', '/', 0, 0))
		retError(clean, Error_invalidOperation(1, "File_resolve() can't replaceAll"))

	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)
	//We also obviously don't support 0:\ and such or A:/ on unix

	#ifdef _WIN32
		if(CharString_length(*result) >= 3 && result->ptr[1] == ':' && (result->ptr[2] != '/' || !C8_isAlpha(result->ptr[0])))
			retError(clean, Error_unsupportedOperation(2, "File_resolve() only supports Windows paths with [A-Z]:/*"))
	#else
		if(CharString_length(*result) >= 2 && result->ptr[1] == ':')
			retError(clean, Error_invalidOperation(6, "File_resolve() doesn't support Windows paths outside of Windows."))
	#endif

	//Now we have to discover the real directory it references to. This means resolving:
	//Empty filename and . to mean no difference and .. to step back

	gotoIfError2(clean, CharString_splitSensitive(*result, '/', alloc, &res))

	U64 realSplitLen = res.length;

	CharString back = CharString_createRefCStrConst("..");

	for (U64 i = 0; i < res.length; ++i) {

		//We pop from ListCharString since it doesn't change anything
		//Starting with a / is valid with local files, so don't remove it. (not with virtual files)
		//Having multiple // after each other means empty file this is invalid.
		//So both empty file and . resolve to nothing

		if(
			(CharString_isEmpty(res.ptr[i]) && i && !*isVirtual) ||
			CharString_equalsSensitive(res.ptr[i], '.')
		) {

			//Move to left

			for (U64 k = res.length - 1; k > i; --k)
				res.ptrNonConst[k - 1] = res.ptr[k];			//This is OK, we're dealing with refs from split

			--i;			//Ensure we keep track of the removed element
			--res.length;
			continue;
		}

		//In this case, we have to pop ListCharString[j], so that's only possible if that's still there

		if (CharString_equalsStringSensitive(res.ptr[i], back)) {

			if(!i) {
				res.length = realSplitLen;
				retError(clean, Error_invalidParameter(
					0, 0, "File_resolve()::loc tried to exit working directory, this is not allowed for security reasons"
				))
			}

			for (U64 k = res.length - 1; k > i + 1; --k)
				res.ptrNonConst[k - 2] = res.ptr[k];			//This is OK, we're dealing with refs from split

			i -= 2;												//Ensure we keep track of the removed element
			res.length -= 2;
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

			res.length = realSplitLen;
			retError(clean, Error_invalidParameter(0, 1, "File_resolve()::loc contains subpath with invalid file name"))
		}

		//Continue processing the path until it's done

		continue;
	}

	//If we have nothing left, we get current work/app directory

	if(!res.length) {
		res.length = realSplitLen;
		CharString_free(result, alloc);		//Release temp result
		gotoIfError2(clean, CharString_createCopy(absoluteDir, alloc, result))
		goto clean;
	}

	//Re-assemble path now

	CharString tmp = CharString_createNull();
	gotoIfError2(clean, ListCharString_concat(res, '/', alloc, &tmp))

	CharString_free(result, alloc);		//This can't be done before concat, because the string is still in use.
	*result = tmp;

	res.length = realSplitLen;
	ListCharString_free(&res, alloc);

	//Check if we're an absolute or relative path

	Bool isAbsolute = false;

	#ifdef _WIN32	//Starts with [A-Z]:/ if absolute. If it starts with / it's unsupported!

		if (CharString_startsWithSensitive(*result, '/', 0))
			retError(clean, Error_unsupportedOperation(
				4, "File_resolve()::loc contained Unix path (/absolute), which is unsupported on Windows"
			))

		isAbsolute = CharString_length(*result) >= 2 && result->ptr[1] == ':';

	#else			//Starts with / if absolute
		isAbsolute = CharString_startsWithSensitive(*result, '/', 0);
	#endif

	//Our path has to be made relative to our working directory.
	//This is to avoid access from folders we shouldn't be able to read in.

	if (isAbsolute) {

		if(
			!abDirLen ||
			!(
				CharString_startsWithStringSensitive(*result, absoluteDir, 0) &&
				(
					CharString_length(*result) == abDirLen || 
					CharString_getAt(*result, abDirLen) == '/'
				)
			)
		)
			retError(clean, Error_unauthorized(
				0, "File_resolve()::loc tried to escape working directory, which is unsupported for security reasons"
			))
	}

	//Prepend our path

	else if(abDirLen && !*isVirtual) {
		gotoIfError2(clean, CharString_insert(result, '/', 0, alloc))
		gotoIfError2(clean, CharString_insertString(result, absoluteDir, 0, alloc))
	}

	//Since we're going to use this in file operations, we want to have a null terminator

	if(!maxFilePathLimit)
		maxFilePathLimit = 260;

	#ifdef _WIN32
		maxFilePathLimit = U64_min(260, maxFilePathLimit);		/* MAX_PATH */
	#endif

	if(CharString_length(*result) >= maxFilePathLimit)
		retError(clean, Error_outOfBounds(
			0, CharString_length(*result), maxFilePathLimit, "File_resolve()::loc resolved path is longer than max file limit"
		))

clean:

	if(!s_uccess && allocate)
		CharString_free(result, alloc);

	ListCharString_free(&res, alloc);
	return s_uccess;
}

Bool File_makeRelative(
	CharString absoluteDir,
	CharString base,
	CharString subFile,
	U64 maxFilePathLimit,
	Allocator alloc,
	CharString *result,
	Error *e_rr
) {
	Bool s_uccess = true, isVirtual = false;
	CharString resolvedBase = CharString_createNull();
	CharString resolvedSubFile = CharString_createNull();

	if(!result)
		retError(clean, Error_nullPointer(5, "File_makeRelative()::result is required"))

	if(result->ptr)
		retError(clean, Error_invalidParameter(5, 0, "File_makeRelative()::*result must be empty"))

	gotoIfError3(clean, File_resolve(base, &isVirtual, maxFilePathLimit, absoluteDir, alloc, &resolvedBase, e_rr))
	gotoIfError3(clean, File_resolve(subFile, &isVirtual, maxFilePathLimit, absoluteDir, alloc, &resolvedSubFile, e_rr))

	CharString baseAbsDir = CharString_createNull();
	CharString subFileAbsDir = CharString_createNull();
	CharString_cut(resolvedBase, CharString_length(absoluteDir), 0, &baseAbsDir);
	CharString_cut(resolvedSubFile, CharString_length(absoluteDir), 0, &subFileAbsDir);

	U64 baseAbsSlashes = CharString_countAllSensitive(baseAbsDir, '/', 0);

	if (!baseAbsSlashes) {							//Base is working dir, so easy to compute
		gotoIfError2(clean, CharString_createCopy(subFileAbsDir, alloc, result))
		goto clean;
	}

	//Otherwise, check how many folders match. Find first one that doesn't match.
	//The first one that doesn't match, we need more ..s

	U64 i = 0;
	U64 subFileLen = 0;

	for (U64 it = 0, it2 = 0; i < baseAbsSlashes; ++i, ++it, ++it2) {

		U64 prev = it;
		it = CharString_findSensitive(baseAbsDir, '/', true, it, 0);

		U64 prev2 = it2;
		it2 = CharString_findSensitive(subFileAbsDir, '/', true, it2, 0);

		if(it == U64_MAX || it2 == U64_MAX)
			break;

		CharString sub = CharString_createNull();
		CharString_cut(baseAbsDir, prev, it - prev, &sub);

		CharString sub2 = CharString_createNull();
		CharString_cut(baseAbsDir, prev2, it2 - prev2, &sub2);

		if(!CharString_equalsStringSensitive(sub, sub2))
			break;

		subFileLen = it2 + 1;
	}

	gotoIfError2(clean, CharString_popFrontCount(&resolvedSubFile, CharString_length(absoluteDir)))
	gotoIfError2(clean, CharString_popFrontCount(&resolvedSubFile, subFileLen))

	for (U64 j = 0; j < baseAbsSlashes - i; ++j)
		gotoIfError2(clean, CharString_prependString(&resolvedSubFile, CharString_createRefCStrConst("../"), alloc))

	*result = resolvedSubFile;
	resolvedSubFile = CharString_createNull();

clean:
	CharString_free(&resolvedBase, alloc);
	CharString_free(&resolvedSubFile, alloc);
	return s_uccess;
}
