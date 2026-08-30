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

#include "types/base/platform_types.h"
#include "types/base/lock.h"
#include "types/base/constants.h"
#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/time.h"
#include "types/base/mathi.h"

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
	#define THREAD_YIELD() sched_yield()
	#if _ARCH == ARCH_ARM64
		#ifdef __clang__
			#define CPU_PAUSE() __builtin_arm_yield()
		#else
			#define CPU_PAUSE() __asm__ __volatile__("yield")
		#endif
	#elif _ARCH == ARCH_WASM64

		//wasm has no pause hint: there is no such instruction and clang exposes no builtin for one.
		//Emscripten's own primitives are empty for the same reason: its _mm_pause() compat shim is an empty
		// function and musl's a_spin() for the emscripten arch is empty with no -pthread variant.
		//THREAD_YIELD does not deschedule on web either, so a contended lock is a real busy loop.
		//Emscripten's sched_yield() runs _emscripten_yield(), which drains the proxying queue and fires timers
		// on the main runtime thread but returns at once on a worker.
		//Spinning is still safe because pthreads are Web Workers on OS threads that the browser preempts, so a
		// spinner never starves the holder, but it burns a core: critical sections have to stay short on web.
		//The main runtime thread arm is load bearing: draining the proxying queue there is what lets a worker
		// blocked on a proxied call run to completion and release the lock.

		#define CPU_PAUSE() ((void)0)
	#else
		#define CPU_PAUSE() __builtin_ia32_pause()
	#endif
#endif

ELockAcquire SpinLock_lock(SpinLock *l, Ns maxTime) {

	if (l) {

		const Ns startTime = maxTime == U64_MAX ? 0 : Time_now();

		const I64 tid = (I64) Thread_getId();
		I64 prevValue = AtomicI64_cmpStore(&l->lockedThreadId, 0, tid);

		if(prevValue == tid)        //Already locked
			return ELockAcquire_AlreadyLocked;

		if(prevValue == 0)
			return ELockAcquire_Acquired;

		if(maxTime == 0)
			return ELockAcquire_TimedOut;

		//If the previous result was not 0 then it means someone already locked this.
		//We have to wait for it and attempt to lock it again.
		//It's possible that within this time someone else is waiting for it and locked it before us.
		//So we have to wait in a loop.

		//Short spin, useful for super short waits

		for (U8 i = 0; i < SHORT_SPIN; ++i) {

			if (AtomicI64_cmpStore(&l->lockedThreadId, 0, tid) == 0)
				return ELockAcquire_Acquired;

			CPU_PAUSE();

			if(Time_now() - startTime >= maxTime)
				return ELockAcquire_TimedOut;
		}

		//Longer wait

		while(Time_now() - startTime < maxTime) {

			if (AtomicI64_cmpStore(&l->lockedThreadId, 0, tid) == 0)
				return ELockAcquire_Acquired;

			THREAD_YIELD();
		}

		return ELockAcquire_TimedOut;
	}

	return ELockAcquire_Invalid;
}
