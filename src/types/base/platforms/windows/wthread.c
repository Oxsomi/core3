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

//types/base/platforms/windows/wthread.c

#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/buffer_base.h"
#include "types/base/mathi.h"
#include "types/base/allocator.h"
#include "types/base/constants.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>

void Thread_freeExt(Thread *thread) {
	if(thread->nativeHandle)
		CloseHandle(thread->nativeHandle);
}

U64 Thread_getId() { return GetCurrentThreadId(); }

Bool Thread_sleep(Ns ns) {

	const LARGE_INTEGER ft = (LARGE_INTEGER) { .QuadPart = -(I64)((U64_min(ns, I64_MAX) + 99) / 100) };
	
	const HANDLE timer = CreateWaitableTimerExW(
			NULL,
			NULL,
			CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
			TIMER_ALL_ACCESS
	);

	if(!timer)
		return false;

	if (!SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0)) {
		CloseHandle(timer);
		return false;
	}

	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
	return true;
}

DWORD ThreadFunc(Thread *thread) {

	if(thread && thread->callback)
		thread->callback(thread->objectHandle);

	return 0;
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

	Buffer buf = Buffer_createNull();
	gotoIfError3(clean, alloc->alloc(alloc->ptr, sizeof(Thread), &buf, e_rr));

	Thread *thr = (*thread = (Thread*) buf.ptr);

	thr->callback = callback;
	thr->objectHandle = objectHandle;

	thr->nativeHandle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, thr, 0, NULL);

	if (!thr->nativeHandle) {
		Thread_free(alloc, thread);
		retError(clean, Error_platformError(0, GetLastError(), "Thread_wait() couldn't create thread"));
	}

clean:
	return s_uccess;
}

Bool Thread_wait(Thread *thread, Error *e_rr) {

	Bool s_uccess = true;

	if(!thread)
		retError(clean, Error_nullPointer(0, "Thread_wait()::thread is required"));

	if(WaitForSingleObject(thread->nativeHandle, U32_MAX) == WAIT_FAILED)
		retError(clean, Error_timedOut(0, U32_MAX, "Thread_wait() couldn't wait on thread"));

clean:
	return s_uccess;
}

Bool Thread_setPriority(EThreadPriority priority, Error *e_rr) {

	Bool s_uccess = true;

	if(priority >= EThreadPriority_Count)
		retError(clean, Error_invalidEnum(0, (U64) priority, EThreadPriority_Count, "Thread_setPriority()::priority"));

	const int level =
		priority == EThreadPriority_Critical ? THREAD_PRIORITY_ABOVE_NORMAL :
		priority == EThreadPriority_Background ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL;

	if(!SetThreadPriority(GetCurrentThread(), level))
		retError(clean, Error_platformError(0, GetLastError(), "Thread_setPriority() SetThreadPriority failed"));

	//EcoQoS: a throttled thread is what the scheduler moves to efficiency cores, and slows on battery

	THREAD_POWER_THROTTLING_STATE state = (THREAD_POWER_THROTTLING_STATE) {
		.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION,
		.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED,
		.StateMask = priority == EThreadPriority_Background ? THREAD_POWER_THROTTLING_EXECUTION_SPEED : 0
	};

	//Older Windows doesn't know the class; the priority above still applies there

	SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &state, sizeof(state));

clean:
	return s_uccess;
}

Bool Thread_setAffinity(const U32 *cpus, U64 count, Error *e_rr) {

	Bool s_uccess = true;

	if(count && !cpus)
		retError(clean, Error_nullPointer(0, "Thread_setAffinity()::cpus is required"));

	if(count > U32_MAX)
		retError(clean, Error_outOfBounds(1, count, U32_MAX, "Thread_setAffinity()::count"));

	if(!SetThreadSelectedCpuSets(GetCurrentThread(), (const ULONG*) cpus, (ULONG) count))
		retError(clean, Error_platformError(0, GetLastError(), "Thread_setAffinity() SetThreadSelectedCpuSets failed"));

clean:
	return s_uccess;
}
