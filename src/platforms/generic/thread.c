/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/platform.h"
#include "platforms/thread.h"
#include "types/error.h"
#include "types/buffer.h"

Bool Thread_free(Thread **thread) {

	if(!thread || !*thread)
		return true;

	FreeFunc free = Platform_instance.alloc.free;
	void *allocator = Platform_instance.alloc.ptr;

	Bool freed = free(allocator, Buffer_createManagedPtr(*thread, sizeof(Thread)));
	*thread = NULL;
	return freed;
}

Error Thread_waitAndCleanup(Thread **thread, U32 maxWaitTime) {

	if(!thread || !*thread)
		return Error_nullPointer(0, "Thread_waitAndCleanup()::thread and *thread are required");

	Error err = Thread_wait(*thread, maxWaitTime);

	if(err.genericError == EGenericError_TimedOut)
		return err;

	Thread_free(thread);
	return err;
}
