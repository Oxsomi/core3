#include "types/time.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include <stdlib.h>

Error ourAlloc(void *allocator, U64 siz, Buffer *output) {

	allocator;

	void *ptr = malloc(siz);

	if(!output)
		return Error_nullPointer(2, 0);

	if(!ptr)
		return Error_outOfMemory(0);

	*output = (Buffer) { .ptr = ptr, .siz = siz };
	return Error_none();
}

Error ourFree(void *allocator, Buffer buf) {
	allocator;
	free(buf.ptr);
	return Error_none();
}

//Required to compile

void Error_fillStackTrace(Error *err) {
	if(err)
		err->stackTrace[0] = NULL;
}

String Error_formatPlatformError(Error err) {
	err;
	return String_createEmpty();
}

//

int main() {

	//Test timer

	Ns now = Time_now();
	TimerFormat nowStr;

	Time_format(now, nowStr);

	Ns now2 = 0;
	EFormatStatus stat = Time_parseFormat(&now2, nowStr);

	if (stat != EFormatStatus_Success || now2 != now)
		return 1;

	Allocator alloc = (Allocator) {
		.alloc = ourAlloc,
		.free = ourFree,
		.ptr = NULL
	};

	//Test Buffer

	Buffer emp = Buffer_createNull();
	Error err = Buffer_createZeroBits(256, alloc, &emp);

	if(err.genericError)
		return 4;

	Buffer full = Buffer_createNull(); 
	err = Buffer_createOneBits(256, alloc, &full);

	if(err.genericError) {
		Buffer_free(&emp, alloc);
		return 5;
	}

	Bool res = false;

	if (Buffer_eq(emp, full, &res).genericError || res) {
		Buffer_free(&emp, alloc);
		Buffer_free(&full, alloc);
		return 2;
	}

	Buffer_setBitRange(emp, 9, 240);
	Buffer_unsetBitRange(full, 9, 240);

	Buffer_bitwiseNot(emp);

	if (Buffer_neq(emp, full, &res).genericError || res) {
		Buffer_free(&emp, alloc);
		Buffer_free(&full, alloc);
		return 3;
	}

	Buffer_free(&emp, alloc);
	Buffer_free(&full, alloc);

	//TODO: Test vectors
	//TODO: Test quaternions
	//TODO: Test transform
	//TODO: Test string
	//TODO: Test math
	//TODO: Test file
	//TODO: Test list

	//

	return 0;
}