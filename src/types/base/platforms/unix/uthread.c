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

//types/base/platforms/unix/uthread.c

//glibc declares cpu_set_t and sched_setaffinity as GNU extensions; before any include, since the first one decides

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/buffer_base.h"
#include "types/base/platform_types.h"

#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#if _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID
	#include <sched.h>
	#include <sys/resource.h>
	#include <sys/syscall.h>
#elif _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
	#include <pthread/qos.h>
#endif

//Nothing to do for uthread, unlike wthread.
void Thread_freeExt(Thread *thread) { (void) thread; }

U64 Thread_getId() { return (U64) (uintptr_t) pthread_self(); }

Bool Thread_sleep(Ns ns) {

	struct timespec remaining, request = { (time_t)(ns / SECOND), (long) (ns % SECOND) };

	while(true) {

		errno = 0;
		int ret = nanosleep(&request, &remaining);

		if(!ret)
			return true;

		if(errno == EINTR) {
			request = remaining;
			continue;
		}

		return false;
	}
}

void *ThreadFunc(void *t) {

	Thread *thread = (Thread*) t;

	if(thread && thread->callback)
		thread->callback(thread->objectHandle);

	return NULL;
}

Bool Thread_create(const Allocator *alloc, ThreadCallbackFunction callback, void *objectHandle, Thread **thread, Error *e_rr) {

	Bool s_uccess = true;

	if(!thread)
		retError(clean, Error_nullPointer(2, "Thread_create()::thread is required"));

	if(*thread)
		retError(clean, Error_invalidParameter(2, 0, "Thread_create()::*thread isn't NULL, might indicate memleak"));

	if(!callback)
		retError(clean, Error_nullPointer(0, "Thread_create()::callback is required"));

	if(!alloc || !alloc->alloc || !alloc->free)
		retError(clean, Error_nullPointer(0, "Thread_create()::alloc is required"));

	Buffer buf = (Buffer) { 0 };
	gotoIfError3(clean, alloc->alloc(alloc->ptr, sizeof(Thread), &buf, e_rr));

	Thread *thr = (*thread = (Thread*) buf.ptr);

	thr->callback = callback;
	thr->objectHandle = objectHandle;
	thr->nativeHandle = NULL;

	const int result = pthread_create((pthread_t*) &thr->nativeHandle, NULL, ThreadFunc, thr);

	if(result) {

		alloc->free(alloc->ptr, buf);
		*thread = NULL;
		retError(clean, Error_platformError(0, (U64) result, "Thread_create() couldn't create thread"));
	}

clean:
	return s_uccess;
}

Bool Thread_wait(Thread *thread, Error *e_rr) {

	Bool s_uccess = true;

	if(!thread)
		retError(clean, Error_nullPointer(0, "Thread_wait()::thread is required"));

	if(!thread->nativeHandle)
		goto clean;

	const int result = pthread_join((pthread_t) thread->nativeHandle, NULL);

	if(result)
		retError(clean, Error_platformError(0, (U64) result, "Thread_wait() couldn't wait on thread"));

	thread->nativeHandle = NULL;

clean:
	return s_uccess;
}

Bool Thread_setPriority(EThreadPriority priority, Error *e_rr) {

	Bool s_uccess = true;

	if((U32) priority >= EThreadPriority_Count)
		retError(clean, Error_invalidEnum(0, (U64) priority, EThreadPriority_Count, "Thread_setPriority()::priority"));

	#if _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID

		//Raising priority, including restoring Normal after Background, can require CAP_SYS_NICE.

		const int nice = priority == EThreadPriority_Critical ? -5 : priority == EThreadPriority_Background ? 10 : 0;
		const pid_t tid = (pid_t) syscall(SYS_gettid);

		if(setpriority(PRIO_PROCESS, (id_t) tid, nice))
			retError(clean, Error_platformError(0, (U64) errno, "Thread_setPriority() setpriority failed"));

	#elif _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS

		//QoS is what Apple's scheduler places by: utility goes to the efficiency cores

		const qos_class_t qos =
			priority == EThreadPriority_Critical ? QOS_CLASS_USER_INTERACTIVE :
			priority == EThreadPriority_Background ? QOS_CLASS_UTILITY : QOS_CLASS_USER_INITIATED;

		const int err = pthread_set_qos_class_self_np(qos, 0);

		if(err)
			retError(clean, Error_platformError(0, (U64) err, "Thread_setPriority() pthread_set_qos_class_self_np failed"));

	#endif

clean:
	return s_uccess;
}

Bool Thread_setAffinity(const U32 *cpus, U64 count, Error *e_rr) {

	Bool s_uccess = true;

	if(count && !cpus)
		retError(clean, Error_nullPointer(0, "Thread_setAffinity()::cpus is required"));

	#if _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID

		static _Thread_local cpu_set_t originalSet;
		static _Thread_local Bool hasOriginalSet;

		cpu_set_t set;
		CPU_ZERO(&set);

		for(U64 i = 0; i < count; ++i) {

			if(cpus[i] >= CPU_SETSIZE)
				retError(clean, Error_outOfBounds(0, cpus[i], CPU_SETSIZE, "Thread_setAffinity()::cpus[i]"));

			CPU_SET(cpus[i], &set);
		}

		//Keep the inherited restriction so clearing our selection restores it, including process CPU limits.

		if(!count && !hasOriginalSet)
			goto clean;

		cpu_set_t previousSet;

		if(count && !hasOriginalSet && sched_getaffinity(0, sizeof(previousSet), &previousSet))
			retError(clean, Error_platformError(0, (U64) errno, "Thread_setAffinity() sched_getaffinity failed"));

		if(!count)
			set = originalSet;

		if(sched_setaffinity((pid_t) syscall(SYS_gettid), sizeof(set), &set))
			retError(clean, Error_platformError(0, (U64) errno, "Thread_setAffinity() sched_setaffinity failed"));

		if(count && !hasOriginalSet)
			originalSet = previousSet;

		hasOriginalSet = count != 0;

	#endif

clean:
	return s_uccess;
}
