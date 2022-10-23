#include "file/file.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"
#include <stdio.h>

#ifndef _WIN32
	#define _ftelli64 ftell
#endif

struct Error File_write(struct Buffer buf, struct String loc) {

	if(!buf.siz || !buf.ptr)
		return (struct Error){ .genericError = GenericError_NullPointer };

	//TODO: Test, does this properly clear a previous file if present?

	if(String_isEmpty(loc))
		return (struct Error){ .genericError = GenericError_InvalidParameter, .paramId = 1 };

	FILE *f = fopen(loc.ptr, "wb");

	if(!f)
		return (struct Error){ .genericError = GenericError_NotFound, .paramId = 1 };

	if (fwrite(buf.ptr, 1, buf.siz, f) != buf.siz) {
		fclose(f);
		return (struct Error){ .genericError = GenericError_InvalidState };
	}

	fclose(f);
	return Error_none();
}

struct Error File_read(struct String loc, struct Allocator allocator, struct Buffer *output) {

	if(String_isEmpty(loc))
		return (struct Error){ .genericError = GenericError_InvalidParameter };

	if(!allocator.alloc || !allocator.free)
		return (struct Error){ .genericError = GenericError_NullPointer, .paramId = 1 };

	if(!output)
		return (struct Error){ .genericError = GenericError_NullPointer, .paramId = 2 };

	FILE *f = fopen(loc.ptr, "rb");

	if(!f)
		return (struct Error){ .genericError = GenericError_NotFound };

	if(fseek(f, 0, SEEK_END)) {
		fclose(f);
		return (struct Error){ .genericError = GenericError_InvalidState, .errorSubId = 0 };
	}

	struct Error err = Buffer_createUninitializedBytes((U64)_ftelli64(f), allocator, &output);
	
	if(err.genericError) {
		fclose(f);
		return err;
	}

	if(fseek(f, 0, SEEK_SET)) {
		Buffer_free(&output, allocator);
		fclose(f);
		return (struct Error){ .genericError = GenericError_InvalidState, .errorSubId = 1 };
	}

	struct Buffer b = *output;

	if (fread(b.ptr, 1, b.siz, f) != b.siz) {
		Buffer_free(&b, allocator);
		fclose(f);
		return (struct Error){ .genericError = GenericError_InvalidState, .errorSubId = 2 };
	}

	fclose(f);
	return Error_none();
}
