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
		return Error_nullPointer(0, 0);

	//TODO: Test, does this properly clear a previous file if present?

	if(String_isEmpty(loc))
		return Error_invalidParameter(1, 0, 0);

	FILE *f = fopen(loc.ptr, "wb");

	if(!f)
		return Error_notFound(1, 0, 0);

	if (fwrite(buf.ptr, 1, buf.siz, f) != buf.siz) {
		fclose(f);
		return Error_invalidState(0);
	}

	fclose(f);
	return Error_none();
}

Error File_read(String loc, Allocator allocator, Buffer *output) {

	if(String_isEmpty(loc))
		return Error_invalidParameter(1, 0, 0);

	if(!allocator.alloc || !allocator.free)
		return Error_nullPointer(1, 0);

	if(output)
		return Error_invalidOperation(0);

	FILE *f = fopen(loc.ptr, "rb");

	if(!f)
		return Error_notFound(0, 0, 0);

	if(fseek(f, 0, SEEK_END)) {
		fclose(f);
		return Error_invalidState(0);
	}

	Error err = Buffer_createUninitializedBytes((U64)_ftelli64(f), allocator, output);
	
	if(err.genericError) {
		fclose(f);
		return err;
	}

	if(fseek(f, 0, SEEK_SET)) {
		Buffer_free(output, allocator);
		fclose(f);
		return Error_invalidState(1);
	}

	Buffer b = *output;

	if (fread(b.ptr, 1, b.siz, f) != b.siz) {
		Buffer_free(output, allocator);
		fclose(f);
		return Error_invalidState(2);
	}

	fclose(f);
	return Error_none();
}
