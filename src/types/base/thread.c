/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/container/buffer.h"

impl void Thread_freeExt(Thread *thread);

void Thread_free(const Allocator *alloc, Thread **thread) {

	if(!thread || !*thread)
		return;

	Thread_freeExt(*thread);
	alloc->free(alloc->ptr, Buffer_createManagedPtr(*thread, sizeof(Thread)));
	*thread = NULL;
}

Bool Thread_waitAndCleanup(const Allocator *alloc, Thread **thread, Error *e_rr) {

	Bool s_uccess = true;

	if(!thread || !*thread)
		retError(clean, Error_nullPointer(0, "Thread_waitAndCleanup()::thread and *thread are required"));

	if(!alloc || !alloc->free)
		retError(clean, Error_nullPointer(0, "Thread_waitAndCleanup()::alloc and alloc->free are required"));

	gotoIfError3(clean, Thread_wait(*thread, e_rr));
	Thread_free(alloc, thread);

clean:
	return s_uccess;
}
