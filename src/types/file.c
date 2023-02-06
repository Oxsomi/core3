#include "math/math.h"
#include "types/string.h"
#include "types/allocator.h"
#include "types/file.h"
#include "types/error.h"

Bool FileInfo_free(FileInfo *info, Allocator alloc) {

	if(!info)
		return false;

	Bool freed = String_free(&info->path, alloc);
	*info = (FileInfo) { 0 };
	return freed;
}

Bool File_isVirtual(String loc) { return String_getAt(loc, 0) == '/' && String_getAt(loc, 1) == '/'; }

Error File_resolve(
	String loc, 
	Bool *isVirtual, 
	U64 maxFilePathLimit, 
	String absoluteDir, 
	Allocator alloc, 
	String *result
) {

	if(result && result->ptr)
		return Error_invalidOperation(0);

	StringList res = (StringList) { 0 };
	Error err = Error_none();

	if(!isVirtual || !result)
		_gotoIfError(clean, Error_nullPointer(!isVirtual ? 1 : 2, 0));

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	//Copy string so we can modifiy it

	_gotoIfError(clean, String_createCopy(loc, alloc, result));
	*isVirtual = File_isVirtual(loc);

	//Virtual files

	if (*isVirtual)
		_gotoIfError(clean, String_popFrontCount(result, 2));

	//Network drives are a thing on windows and allow starting a path with \\
	//We shouldn't be supporting this.
	//The reason; you can access servers from an app with a file read instead of a https read!
	//This can obfuscate the application's true intentions.
	//ex. \\0.0.0.0\ would make a file web request to 0.0.0.0.
	//Unix can map a folder to a webserver, but that link has to be created beforehand, not our app.
	//You can also read from hardware in a platform dependent way, which makes it harder to standardize.
	//TODO: We should however support this in the future as a custom instruction that allows it

	if (String_getAt(*result, 0) == '\\' && String_getAt(*result, 1) == '\\')
		_gotoIfError(clean, Error_unsupportedOperation(3));

	//Backslash is replaced with forward slash for easy windows compatibility

	if (!String_replaceAll(result, '\\', '/', EStringCase_Sensitive))
		_gotoIfError(clean, Error_invalidOperation(1));

	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)
	//We also obviously don't support 0:\ and such or A:/ on unix

	#ifdef _WIN32

		if(result->length >= 3 && result->ptr[1] == ':' && (result->ptr[2] != '/' || !C8_isAlpha(result->ptr[0])))
			_gotoIfError(clean, Error_unsupportedOperation(2));

	#else

		if(result->length >= 2 && result->ptr[1] == ':')
			_gotoIfError(clean, Error_invalidOperation(6));

	#endif

	//Now we have to discover the real directory it references to. This means resolving:
	//Empty filename and . to mean no difference and .. to step back

	_gotoIfError(clean, String_split(*result, '/', EStringCase_Sensitive, alloc, &res));

	U64 realSplitLen = res.length;		//We have to reset this before unallocating the StringList!

	String back = String_createConstRefUnsafe("..");

	for (U64 i = 0; i < res.length; ++i) {

		//We pop from StringList since it doesn't change anything
		//Starting with a / is valid with local files, so don't remove it. (not with virtual files)
		//Having multiple // after each other means empty file this is invalid.
		//So both empty file and . resolve to nothing

		if(
			(String_isEmpty(res.ptr[i]) && i && !*isVirtual) ||
			String_equals(res.ptr[i], '.', EStringCase_Sensitive, true)
		) {

			//Move to left

			for (U64 k = res.length - 1; k > i; --k) 
				res.ptr[k - 1] = res.ptr[k];			//This is OK, we're dealing with refs from split

			--i;			//Ensure we keep track of the removed element
			--res.length;
			continue;
		}

		//In this case, we have to pop StringList[j], so that's only possible if that's still there

		if (String_equalsString(res.ptr[i], back, EStringCase_Sensitive, true)) {

			if(!i) {
				res.length = realSplitLen;
				_gotoIfError(clean, Error_invalidParameter(0, 0, 0));
			}

			for (U64 k = res.length - 1; k > i + 1; --k) 
				res.ptr[k - 2] = res.ptr[k];			//This is OK, we're dealing with refs from split

			i -= 2;			//Ensure we keep track of the removed element
			res.length -= 2;
			continue;
		}

		//Validate file name

		if (!String_isValidFileName(res.ptr[i], i == res.length - 1)) {

			#ifdef _WIN32

				//Drive name

				if(!i && res.ptr[i].length == 2 && C8_isAlpha(res.ptr[0].ptr[0]) && res.ptr[0].ptr[1] == ':')
					continue;

			#endif

			res.length = realSplitLen;
			_gotoIfError(clean, Error_invalidParameter(0, 0, 1));
		}

		//Continue processing the path until it's done

		continue;
	}

	//If we have nothing left, we get current work/app directory

	if(!res.length) {
		res.length = realSplitLen;
		goto clean;
	}

	//Re-assemble path now

	String tmp = String_createNull();

	if ((err = StringList_concat(res, '/', alloc, &tmp)).genericError) {
		res.length = realSplitLen;
		StringList_free(&res, alloc);
	}

	String_free(result, alloc);		//This can't be done before concat, because the string is still in use.
	*result = tmp;

	res.length = realSplitLen;
	StringList_free(&res, alloc);

	//Check if we're an absolute or relative path

	Bool isAbsolute = false;

	#ifdef _WIN32	//Starts with [A-Z]:/ if absolute. If it starts with / it's unsupported!

		if (String_startsWith(*result, '/', EStringCase_Sensitive))
			_gotoIfError(clean, Error_unsupportedOperation(4));

		isAbsolute = result->length >= 2 && result->ptr[1] == ':';

	#else			//Starts with / if absolute
		isAbsolute = String_startsWith(*result, '/', EStringCase_Sensitive);
	#endif

	//Our path has to be made relative to our working directory.
	//This is to avoid access from folders we shouldn't be able to read in.

	if (isAbsolute) {

		if(!absoluteDir.length || !String_startsWithString(*result, absoluteDir, EStringCase_Insensitive))
			_gotoIfError(clean, Error_unauthorized(0));
	}

	//Prepend our path

	else if(absoluteDir.length) 
		_gotoIfError(clean, String_insertString(result, absoluteDir, 0, alloc));

	//Since we're gonna use this in file operations, we wanna have a null terminator

	if(String_getAt(*result, result->length - 1) != '\0')
		_gotoIfError(clean, String_insert(result, '\0', result->length, alloc));

	if(!maxFilePathLimit)
		maxFilePathLimit = 260;

	#ifdef _WIN32
		maxFilePathLimit = U64_min(260, maxFilePathLimit);		/* MAX_PATH */
	#endif

	if(result->length >= maxFilePathLimit)
		_gotoIfError(clean, Error_outOfBounds(0, 0, result->length, maxFilePathLimit));

	return Error_none();

clean:
	StringList_free(&res, alloc);
	String_free(result, alloc);
	return err;
}
