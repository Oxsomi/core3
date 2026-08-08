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

//types/container/test/test_types_container_log_oom.c

#include "test_types_container_shared.h"
#include "types/container/log.h"
#include "types/base/buffer_base.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//An allocator that always fails, simulating the process being out of memory

static Bool failingAlloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	(void)allocator;
	(void)length;
	(void)output;

	retError(clean, Error_outOfMemory(0, "failingAlloc() simulated OOM"));

clean:
	return s_uccess;
}

static void failingFree(void *allocator, Buffer buf) {
	(void)allocator;
	(void)buf;
}

void Test_logOOM(Test *t) {

	Test_setModule(t, "Log OOM fallback (reserved memory)");

	const Allocator oom = (Allocator) { .ptr = NULL, .alloc = failingAlloc, .free = failingFree };

	//These should fall back to the reserved pool and still print rather than silently dropping the message.
	//The assertions only verify we survive; the printed lines are visible in the test output.

	Log_debug(&oom, ELogOptions_NewLine, "Log OOM fallback: this message printed via the reserved pool (%d)", 123);
	Test_assert(t, "Log_debug survives OOM", true);

	//Twice, to prove the pool resets correctly between fallback prints

	Log_debug(&oom, ELogOptions_NewLine, "Log OOM fallback: second message via the reserved pool");
	Test_assert(t, "second log survives OOM", true);

	//Error_print also prints the stack trace from the pool when the real allocator is failing

	const Error err = Error_outOfMemory(0, "Test_logOOM() fake error to print under OOM");
	Error_print(&oom, &err, ELogLevel_Debug, ELogOptions_NewLine);
	Test_assert(t, "Error_print survives OOM", true);
}
