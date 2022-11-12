#include "platforms/file.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"

#include <stdio.h>

//This is fine in file.c instead of wfile.c because unix + windows systems all share very similar ideas about filesystems
//But windows is a bit stricter in some parts (like the characters you can include) and has some weird quirks
//Still, it's common enough here to not require separation

#ifndef _WIN32
	#define _ftelli64 ftell
#endif

Error File_resolve(String loc, Bool *isVirtual, String *result) {

	if(!isVirtual || !result)
		return Error_nullPointer(!isVirtual ? 1 : 2, 0);

	//Copy string so we can modifiy it

	Error err = String_createCopyx(loc, result);

	if(err.genericError)
		return err;

	*isVirtual = false;

	//Virtual files

	String virtualPrefix = String_createConstRefUnsafe("//");

	if (String_startsWithString(*result, virtualPrefix, EStringCase_Sensitive)) {

		if(!String_eraseFirstString(result, virtualPrefix, EStringCase_Sensitive)) {
			String_freex(result);
			return Error_invalidOperation(0);
		}

		*isVirtual = true;
	}

	//Backslash is replaced with forward slash for easy windows compatibility

	if (!String_replaceAll(result, '\\', '/', EStringCase_Sensitive)) {
		String_freex(result);
		return Error_invalidOperation(1);
	}

	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)

	#ifdef _WIN32

		if(result->length >= 3 && result->ptr[1] == ':' && result->ptr[2] != '/') {
			String_freex(result);
			return Error_unsupportedOperation(2);
		}

	#endif

	//Network drives are a thing on windows and allow starting a path with \\
	//We shouldn't be supporting this.
	//The reason; you can access servers from an app with a file read instead of a https read!
	//This can obfuscate the application's true intentions.
	//ex. \\0.0.0.0\ would make a web request to 0.0.0.0.
	//Unix can map a folder to a webserver, but that link has to be created beforehand, not by the app.
	//You can also read from hardware in a platform dependent way, which makes it harder to standardize.

	if (String_startsWithString(*result, virtualPrefix, EStringCase_Sensitive)) {
		String_freex(result);
		return Error_unsupportedOperation(3);
	}

	//Now we have to discover the real directory it references to. This means resolving:
	//Empty filename and . to mean no difference and .. to step back

	StringList res;
	if ((err = String_splitx(*result, '/', EStringCase_Sensitive, &res)).genericError) {
		String_freex(result);
		return err;
	}

	U64 realSplitLen = res.length;		//We have to reset this before unallocating the StringList!

	String back = String_createConstRefUnsafe("..");

	for (U64 i = 0; i < res.length; ++i) {

		//We pop from StringList since it doesn't change anything
		//Starting with a / is valid with local files, so don't remove it. (not with virtual files)
		//Having multiple // after each other means empty file this is invalid.
		//So both empty file and . resolve to nothing

		if(
			(String_isEmpty(res.ptr[i]) && i && !*isVirtual) ||
			String_equals(res.ptr[i], '.', EStringCase_Sensitive)
		) {

			//Move to left

			for (U64 k = res.length - 1; k > i; --k) 
				res.ptr[k - 1] = res.ptr[k];			//This is OK, we're dealing with refs from split

			--i;			//Ensure we keep track of the removed element
			--res.length;
			continue;
		}

		//In this case, we have to pop StringList[j], so that's only possible if that's still there

		if (String_equalsString(res.ptr[i], back, EStringCase_Sensitive)) {

			if(!i) {
				res.length = realSplitLen;
				StringList_freex(&res);
				String_freex(result);
				return Error_invalidParameter(0, 0, 0);
			}

			for (U64 k = res.length - 1; k > i + 1; --k) 
				res.ptr[k - 2] = res.ptr[k];			//This is OK, we're dealing with refs from split

			i -= 2;			//Ensure we keep track of the removed element
			res.length -= 2;
			continue;
		}

		//Virtual file names need to be nytodecimal (0-9A-Za-z$_)+ compatible (excluding /, . and ..)

		if (*isVirtual && !String_isNytoFile(res.ptr[i])) {
			res.length = realSplitLen;
			StringList_freex(&res);
			String_freex(result);
			return Error_invalidParameter(0, 0, 1);
		}

		//Validation to make sure we're not using weird legacy MS DOS keywords
		//Because these will not be writable correctly!
		//TODO:

		//Nothing needs to happen if it passes our tests,
		//It just has to continue processing the path until it's done

		continue;
	}

	//If we have nothing left, we assume current work/app directory

	if(!res.length) {
		res.length = realSplitLen;
		StringList_freex(&res);
		String_freex(result);		//This resets our string to be empty
		return Error_none();
	}

	//Re-assemble path now

	String_freex(result);

	if ((err = StringList_concatx(res, '/', result)).genericError) {
		res.length = realSplitLen;
		StringList_freex(&res);
		return err;
	}

	res.length = realSplitLen;

	if ((err = StringList_freex(&res)).genericError) {
		String_freex(result);
		return err;
	}

	//Check if we're an absolute or relative path

	Bool isAbsolute = false;

	#ifdef _WIN32	//Starts with [A-Z]:/ if absolute. If it starts with / it's unsupported!

		if (String_startsWith(*result, '/', EStringCase_Sensitive)) {
			String_freex(result);
			return Error_unsupportedOperation(4);
		}

		isAbsolute = result->length >= 2 && C8_isAlpha(*result->ptr) && result->ptr[1] == ':';

	#else			//Starts with / if absolute
		isAbsolute = String_startsWith(*result, '/', EStringCase_Sensitive);
	#endif

	//Our path has to be made relative to our working directory.
	//This is to avoid access from folders we shouldn't be able to read in.

	if (isAbsolute && !String_startsWithString(*result, Platform_instance.workingDirectory, EStringCase_Insensitive)) {
		String_freex(result);
		return Error_unauthorized(0);
	}

	//Prepend our path

	else if ((err = String_insertStringx(result, Platform_instance.workingDirectory, 0)).genericError) {
		String_freex(result);
		return err;
	}

	//Since we're gonna use this in file operations, we wanna have a null terminator

	if ((err = String_insertx(result, '\0', result->length)).genericError) {
		String_freex(result);
		return err;
	}

	return Error_none();
}

Error File_writeLocal(Buffer buf, String loc) {

	if(!buf.siz || !buf.ptr) 
		return Error_nullPointer(0, 0);

	if(String_isEmpty(loc))
		return Error_invalidParameter(1, 0, 0);

	String resolved;
	Bool isVirtual = false;
	Error err = File_resolve(loc, &isVirtual, &resolved);

	if(err.genericError)
		return err;

	if(isVirtual) {
		String_freex(&resolved);
		return Error_invalidOperation(0);
	}

	//TODO: Test, does this properly clear a previous file if present?

	FILE *f = fopen(resolved.ptr, "wb");

	if(!f) {
		String_freex(&resolved);
		return Error_notFound(1, 0, 0);
	}

	if (fwrite(buf.ptr, 1, buf.siz, f) != buf.siz) {
		String_freex(&resolved);
		fclose(f);
		return Error_invalidState(0);
	}

	fclose(f);
	String_freex(&resolved);
	return Error_none();
}

Error File_readLocal(String loc, Buffer *output) {

	if(String_isEmpty(loc))
		return Error_invalidParameter(1, 0, 0);

	if(output)
		return Error_invalidOperation(0);

	String resolved;
	Bool isVirtual = false;
	Error err = File_resolve(loc, &isVirtual, &resolved);

	if(err.genericError)
		return err;

	if(isVirtual) {
		String_freex(&resolved);
		return Error_invalidOperation(0);
	}

	FILE *f = fopen(resolved.ptr, "rb");

	if(!f) {
		String_freex(&resolved);
		return Error_notFound(0, 0, 0);
	}

	if(fseek(f, 0, SEEK_END)) {
		String_freex(&resolved);
		fclose(f);
		return Error_invalidState(0);
	}

	if((err = Buffer_createUninitializedBytesx((U64)_ftelli64(f), output)).genericError) {
		String_freex(&resolved);
		fclose(f);
		return err;
	}

	if(fseek(f, 0, SEEK_SET)) {
		String_freex(&resolved);
		Buffer_freex(output);
		fclose(f);
		return Error_invalidState(1);
	}

	Buffer b = *output;

	if (fread(b.ptr, 1, b.siz, f) != b.siz) {
		String_freex(&resolved);
		Buffer_freex(output);
		fclose(f);
		return Error_invalidState(2);
	}

	String_freex(&resolved);
	fclose(f);
	return Error_none();
}
