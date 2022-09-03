#include "file/file.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/bit.h"
#include <stdio.h>

#ifndef _WIN32
	#define _ftelli64 ftell
#endif

struct Error File_write(struct Buffer buf, const c8 *loc) {

	if(!loc)
		return (struct Error){ .genericError = GenericError_InvalidParameter, .paramId = 1 };

	ocAssert("Invalid file name or buffer", loc && buf.siz && buf.ptr);

	FILE *f = fopen(loc, "wb");
	ocAssert("File not found", f);

	fwrite(buf.ptr, 1, buf.siz, f);
	fclose(f);
}

struct Buffer File_read(const c8 *loc, struct Allocator allocator) {

	ocAssert("Invalid file name or alloc func", loc && alloc);

	FILE *f = fopen(loc, "rb");
	ocAssert("File not found", f);

	fseek(f, 0, SEEK_END);

	struct Buffer b = Bit_bytes((usz)_ftelli64(f), allocator);

	fseek(f, 0, SEEK_SET);
	fread(b.ptr, 1, b.siz, f);
	fclose(f);
	return b;
}