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

#include "types/base/platform_types.h"
#include "types/base/lock.h"
#include "types/base/constants.h"
#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/time.h"
#include "types/base/math.h"

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
		#define CPU_PAUSE() __builtin_arm_yield()
	#else
		#define CPU_PAUSE() __builtin_ia32_pause()
	#endif
#endif

ELockAcquire SpinLock_lock(SpinLock *l, Ns maxTime) {

	if (l) {

		const Ns startTime = maxTime == U64_MAX ? 0 : Time_now();

		const I64 tid = (I64) Thread_getId();
		I64 prevValue = AtomicI64_cmpStore(&l->lockedThreadId, 0, tid);

		if(prevValue == tid)		//Already locked
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
