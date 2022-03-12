#include "timer.h"
#include "bit_helper.h"
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
	enum FormatStatus stat = Timer_parseFormat(&now2, nowStr);

	if (stat != FormatStatus_Success || now2 != now)
		return 1;

	//Test Bit helper

	struct Buffer emp = Bit_empty(256, ourAlloc, NULL);
	struct Buffer full = Bit_full(256, ourAlloc, NULL);

	if (Bit_eq(emp, full))
		return 2;

	Bit_setRange(emp, 9, 240);
	Bit_unsetRange(full, 9, 240);

	Bit_not(emp);

	if (Bit_neq(emp, full))
		return 3;

	//

	return 0;
}