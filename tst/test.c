#include "types/timer.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include <stdlib.h>

void *ourAlloc(void *allocator, U64 siz) {
	allocator;
	return malloc(siz);
}

void ourFree(void *allocator, struct Buffer buf) {
	allocator;
	free(buf.ptr);
}

int main() {

	//Test timer

	Ns now = Timer_now();
	TimerFormat nowStr;

	Timer_format(now, nowStr);

	Ns now2 = 0;
	enum EFormatStatus stat = Timer_parseFormat(&now2, nowStr);

	if (stat != FormatStatus_Success || now2 != now)
		return 1;

	struct Allocator alloc = (struct Allocator) {
		.alloc = ourAlloc,
		.free = ourFree,
		.ptr = NULL
	};

	//Test Buffer

	struct Buffer emp = Buffer_createNull();
	struct Error err = Buffer_createZeroBits(256, alloc, &emp);

	if(err.genericError)
		return 4;

	struct Buffer full = Buffer_createNull(); 
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