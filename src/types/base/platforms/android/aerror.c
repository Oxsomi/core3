
/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//types/base/platforms/android/aerror.c

#include "types/base/error.h"

//Comparable to https://stackoverflow.com/questions/8115192/android-ndk-getting-the-backtrace

#include <unwind.h>
#include <dlfcn.h>

typedef struct Backtrace {
	void **current, **end;
	U64 skip;
} Backtrace;

_Unwind_Reason_Code unwindCallback(struct _Unwind_Context *context, Backtrace *state) {

	U64 pc = _Unwind_GetIP(context);

	if (state->current == state->end)
		return _URC_END_OF_STACK;

	if(state->skip)
		--state->skip;

	else *state->current++ = (void*)pc;

	if(!pc)
		return _URC_END_OF_STACK;

	return _URC_NO_REASON;
}

void Error_captureStackTrace(void **stack, U8 stackSize, U8 skipTmp) {

	if(!stack || !stackSize) {
		return;
	}

	Backtrace backtrace = (Backtrace) {
		.current = stack,
		.end = stack + stackSize,
		.skip = (U64) skipTmp + 1
	};
	
	_Unwind_Backtrace((_Unwind_Trace_Fn)unwindCallback, &backtrace);

	I32 count = backtrace.current - stack;

	if(count < stackSize)
		stack[count] = NULL;
}
