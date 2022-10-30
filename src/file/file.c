#include "file/file.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"

#include <stdio.h>

#ifndef _WIN32
	#define _ftelli64 ftell
#endif

Error File_write(Buffer buf, String loc) {

	if(!buf.siz || !buf.ptr)
		return (Error) { .genericError = EGenericError_NullPointer };

	//TODO: Test, does this properly clear a previous file if present?

	if(String_isEmpty(loc))
		return (Error) { .genericError = EGenericError_InvalidParameter, .paramId = 1 };

	FILE *f = fopen(loc.ptr, "wb");

	if(!f)
		return (Error) { .genericError = EGenericError_NotFound, .paramId = 1 };

	if (fwrite(buf.ptr, 1, buf.siz, f) != buf.siz) {
		fclose(f);
		return (Error) { .genericError = EGenericError_InvalidState };
	}

	fclose(f);
	return Error_none();
}

Error File_read(String loc, Allocator allocator, Buffer *output) {

	if(String_isEmpty(loc))
		return (Error) { .genericError = EGenericError_InvalidParameter };

	if(!allocator.alloc || !allocator.free)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 1 };

	if(output)
		return (Error) { .genericError = EGenericError_InvalidOperation };

	FILE *f = fopen(loc.ptr, "rb");

	if(!f)
		return (Error) { .genericError = EGenericError_NotFound };

	if(fseek(f, 0, SEEK_END)) {
		fclose(f);
		return (Error) { .genericError = EGenericError_InvalidState, .errorSubId = 0 };
	}

	Error err = Buffer_createUninitializedBytes((U64)_ftelli64(f), allocator, output);
	
	if(err.genericError) {
		fclose(f);
		return err;
	}

	if(fseek(f, 0, SEEK_SET)) {
		Buffer_free(output, allocator);
		fclose(f);
		return (Error) { .genericError = EGenericError_InvalidState, .errorSubId = 1 };
	}

	Buffer b = *output;

	if (fread(b.ptr, 1, b.siz, f) != b.siz) {
		Buffer_free(output, allocator);
		fclose(f);
		return (Error) { .genericError = EGenericError_InvalidState, .errorSubId = 2 };
	}

	fclose(f);
	return Error_none();
}
