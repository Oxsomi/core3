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

#include "types/base/lock.h"
#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/time.h"
#include "types/math/math.h"

ELockAcquire SpinLock_lock(SpinLock *l, Ns maxTime) {

	if (l) {

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

		const Ns time = maxTime == U64_MAX ? 0 : Time_now();

		while(prevValue != 0) {

			prevValue = AtomicI64_cmpStore(&l->lockedThreadId, 0, tid);

			if(prevValue != 0 && maxTime != U64_MAX && Time_now() - time >= maxTime)
				return ELockAcquire_TimedOut;
		}

		return prevValue == 0 ? ELockAcquire_Acquired : ELockAcquire_Invalid;
	}

	return ELockAcquire_Invalid;
}

Bool SpinLock_unlock(SpinLock *l) {

	if (l) {
		const U64 tid = Thread_getId();
		return AtomicI64_cmpStore(&l->lockedThreadId, tid, 0) == (I64) tid;
	}

	return false;
}
