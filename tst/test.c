#include "types/timer.h"
#include "types/bit.h"
#include "types/allocator.h"
#include <stdlib.h>

void *ourAlloc(void *allocator, usz siz) {
	allocator;
	return malloc(siz);
}

void ourFree(void *allocator, struct Buffer buf) {
	allocator;
	free(buf.ptr);
}

int main() {

	//Test timer

	ns now = Timer_now();
	TimerFormat nowStr;

	Timer_format(now, nowStr);

	ns now2 = 0;
	enum EFormatStatus stat = Timer_parseFormat(&now2, nowStr);

	if (stat != FormatStatus_Success || now2 != now)
		return 1;

	struct Allocator alloc = (struct Allocator) {
		.alloc = ourAlloc,
		.free = ourFree,
		.ptr = NULL
	};

	//Test Bit helper

	struct Buffer emp = Bit_empty(256, alloc);
	struct Buffer full = Bit_full(256, alloc);

	if (Bit_eq(emp, full)) {
		Bit_free(&emp, alloc);
		Bit_free(&full, alloc);
		return 2;
	}

	Bit_setRange(emp, 9, 240);
	Bit_unsetRange(full, 9, 240);

	Bit_not(emp);

	if (Bit_neq(emp, full)) {
		Bit_free(&emp, alloc);
		Bit_free(&full, alloc);
		return 3;
	}

	//TODO: Test vectors
	//TODO: Test quaternions
	//TODO: Test transform

	Bit_free(&emp, alloc);
	Bit_free(&full, alloc);

	//

	return 0;
}