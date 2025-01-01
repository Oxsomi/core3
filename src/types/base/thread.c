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

#include "types/container/list_impl.h"
#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/container/buffer.h"

TListNamedImpl(ListThread);

Bool Thread_free(Allocator alloc, Thread **thread) {

	if(!thread || !*thread)
		return true;

	const Bool freed = alloc.free(alloc.ptr, Buffer_createManagedPtr(*thread, sizeof(Thread)));
	*thread = NULL;
	return freed;
}

Error Thread_waitAndCleanup(Allocator alloc, Thread **thread) {

	if(!thread || !*thread)
		return Error_nullPointer(0, "Thread_waitAndCleanup()::thread and *thread are required");

	const Error err = Thread_wait(*thread);

	if(err.genericError == EGenericError_TimedOut)
		return err;

	Thread_free(alloc, thread);
	return err;
}
