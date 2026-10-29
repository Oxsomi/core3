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

//types/base/lock.c

#ifndef _POSIX_C_SOURCE
	#define _POSIX_C_SOURCE 200809L
#endif

#include "types/base/platform_types.h"
#include "types/base/lock.h"
#include "types/base/constants.h"
#include "types/base/thread.h"

#if _ARCH == ARCH_ARM64
	#define SHORT_SPIN 16
#else
	#define SHORT_SPIN 32
#endif

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define NOMINMAX
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#define CPU_PAUSE() YieldProcessor()
	#define THREAD_YIELD() SwitchToThread()
#else
	#include <sched.h>
	#include <time.h>
	#define THREAD_YIELD() sched_yield()
	#if _ARCH == ARCH_ARM64
		#if _COMPILER == COMPILER_CLANG
			#define CPU_PAUSE() __builtin_arm_yield()
		#else
			#define CPU_PAUSE() __asm__ __volatile__("yield")
		#endif
	#elif _ARCH == ARCH_WASM64

		//No pause hint is reachable from here: clang exposes no builtin for one, so even though a pause
		// instruction exists on paper in the shared-everything-threads proposal, nothing can emit it.
		//Emscripten's own primitives are empty for the same reason: its _mm_pause() compat shim is an empty
		// function and musl's a_spin() for the emscripten arch is empty with no -pthread variant.
		//THREAD_YIELD does not deschedule on web either, so a contended lock is a real busy loop.
		//Under -pthread, emscripten's sched_yield() runs _emscripten_yield(), which drains the proxying queue
		// and fires timers on the main runtime thread but returns at once on a worker.
		//A spinner does not starve the holder, since pthreads are Web Workers on OS threads the browser
		// preempts, but it burns a core: critical sections have to stay short on web.
		//The main runtime thread arm is load bearing: draining the proxying queue there is what lets a worker
		// blocked on a proxied call run to completion and release the lock.

		#define CPU_PAUSE() ((void)0)
	#else
		#define CPU_PAUSE() __builtin_ia32_pause()
	#endif
#endif

static Ns SpinLock_monotonicTime(U64 frequency) {

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		LARGE_INTEGER counter;

		if(!QueryPerformanceCounter(&counter))
			return U64_MAX;

		const U64 ticks = (U64) counter.QuadPart;

		return ticks / frequency * SECOND + (Ns) ((F64) (ticks % frequency) * SECOND / frequency);

	#else

		(void) frequency;

		struct timespec now;

		if(clock_gettime(CLOCK_MONOTONIC, &now))
			return U64_MAX;

		return (Ns) now.tv_sec * SECOND + (Ns) now.tv_nsec;

	#endif
}

ELockAcquire SpinLock_lock(SpinLock *l, Ns maxTime) {

	if(!l)
		return ELockAcquire_Invalid;

	const I64 tid = (I64) Thread_getId();
	const I64 prevValue = AtomicI64_cmpStore(&l->lockedThreadId, 0, tid);

	if(prevValue == tid)
		return ELockAcquire_AlreadyLocked;

	if(prevValue == 0)
		return ELockAcquire_Acquired;

	if(!maxTime)
		return ELockAcquire_TimedOut;

	const Bool bounded = maxTime != U64_MAX;
	U64 frequency = 0;
	Ns startTime = 0;

	if(bounded) {

		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			LARGE_INTEGER nativeFrequency;

			if(!QueryPerformanceFrequency(&nativeFrequency) || nativeFrequency.QuadPart <= 0)
				return ELockAcquire_Invalid;

			frequency = (U64) nativeFrequency.QuadPart;
		#endif

		startTime = SpinLock_monotonicTime(frequency);

		if(startTime == U64_MAX)
			return ELockAcquire_Invalid;
	}

	U8 spins = 0;

	while(true) {

		//Read the cache line without requesting exclusive ownership until the lock is available.

		if(!AtomicI64_load(&l->lockedThreadId) && !AtomicI64_cmpStore(&l->lockedThreadId, 0, tid))
			return ELockAcquire_Acquired;

		if(spins < SHORT_SPIN) {
			++spins;
			CPU_PAUSE();
		}

		else THREAD_YIELD();

		if(bounded) {

			const Ns now = SpinLock_monotonicTime(frequency);

			if(now == U64_MAX)
				return ELockAcquire_Invalid;

			if(now - startTime >= maxTime)
				return ELockAcquire_TimedOut;
		}
	}
}
