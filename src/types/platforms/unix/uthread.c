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
#include "types/thread.h"
#include "types/error.h"
#include "types/buffer.h"

#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

U64 Thread_getId() { return (U64)(void*)pthread_self(); }

Bool Thread_sleep(Ns ns) {

	struct timespec remaining, request = { (time_t)(ns / SECOND), (long)(ns % SECOND) };

	while(true) {

		errno = 0;
		int ret = nanosleep(&request, &remaining);

		if(!ret)
			return true;

		if(errno == EINTR)
			continue;

		return false;
	}
}

void *ThreadFunc(void *t) {
	
	Thread *thread = (Thread*) t;

	if(thread && thread->callback)
		thread->callback(thread->objectHandle);

	return NULL;
}

Error Thread_create(ThreadCallbackFunction callback, void *objectHandle, Thread **thread) {

	if(!thread)
		return Error_nullPointer(2, "Thread_create()::thread is required");

	if(*thread)
		return Error_invalidParameter(2, 0, "Thread_create()::*thread isn't NULL, might indicate memleak");

	if(!callback)
		return Error_nullPointer(0, "Thread_create()::callback is required");

	Buffer buf = Buffer_createNull();

	Error err = Platform_instance.alloc.alloc(Platform_instance.alloc.ptr, sizeof(Thread), &buf);

	if (err.genericError)
		return err;

	Thread *thr = (*thread = (Thread*) buf.ptr);

	thr->callback = callback;
	thr->objectHandle = objectHandle;

	if (pthread_create((pthread_t*)&thr->nativeHandle, NULL, ThreadFunc, thr)) {
		Thread_free(thread);
		return Error_platformError(0, errno, "Thread_wait() couldn't create thread");
	}

	return Error_none();
}

Error Thread_wait(Thread *thread) {

	if(!thread)
		return Error_nullPointer(0, "Thread_wait()::thread is required");

	if(pthread_join(thread->nativeHandle, NULL))
		return Error_timedOut(0, U64_MAX, "Thread_wait() couldn't wait on thread");

	return Error_none();
}
