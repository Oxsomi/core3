#include "types/timer.h"
#include "types/bit.h"
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

	Math_initPlatform();

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

	//Test Bit helper

	struct Buffer emp = (struct Buffer) { 0 };

	struct Error err = Bit_createEmpty(256, alloc, &emp);

	if(err.genericError)
		return 4;

	struct Buffer full = (struct Buffer) { 0 }; 
	err = Bit_createFull(256, alloc, &full);

	if(err.genericError) {
		Bit_free(&emp, alloc);
		return 5;
	}

	Bool res = false;

	if (Bit_eq(emp, full, &res).genericError || res) {
		Bit_free(&emp, alloc);
		Bit_free(&full, alloc);
		return 2;
	}

	Bit_setRange(emp, 9, 240);
	Bit_unsetRange(full, 9, 240);

	Bit_not(emp);

	if (Bit_neq(emp, full, &res).genericError || res) {
		Bit_free(&emp, alloc);
		Bit_free(&full, alloc);
		return 3;
	}

	Bit_free(&emp, alloc);
	Bit_free(&full, alloc);

	//TODO: Test vectors
	//TODO: Test quaternions
	//TODO: Test transform
	//TODO: Test string
	//TODO: Test math
	//TODO: Test file

	//

	return 0;
}