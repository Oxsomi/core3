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

//types/base/lock.h

#pragma once
#include "types/base/atomic.h"
#include "types/base/thread.h"
#include <stdalign.h>
#include <assert.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct SpinLock {
	alignas(64) AtomicI64 lockedThreadId;
} SpinLock;

typedef struct Error Error;

//Even though maxTime is in Ns it may be interpreted
//As a different unit by the runtime.
//Ex. Windows uses ms so it'll round up to ms

typedef enum ELockAcquire {

	ELockAcquire_Invalid = -2,
	ELockAcquire_TimedOut = -1,
	ELockAcquire_Acquired = 0,
	ELockAcquire_AlreadyLocked = 1,

	ELockAcquire_Success = 0			//Anything geq this is success

} ELockAcquire;

ELockAcquire SpinLock_lock(SpinLock *l, Ns maxTime);

static inline Bool SpinLock_unlock(SpinLock *l) {

	if (l) {
		const U64 tid = Thread_getId();
		Bool unlocked = AtomicI64_cmpStore(&l->lockedThreadId, tid, 0) == (I64) tid;
		assert(unlocked && "Thread tried unlocking for a SpinLock it didn't own");
		return unlocked;
	}

	return false;
}

static inline Bool SpinLock_isLockedForThread(SpinLock *l) {
	return l && AtomicI64_load(&l->lockedThreadId) == (I64)Thread_getId();
}

#ifdef __cplusplus
	}
#endif
