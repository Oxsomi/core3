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

#pragma once

namespace oxc {

	namespace c {
		#include "types/base/lock.h"
	}

	//Acquired lock must be kept with the same thread.
	//An acquired lock is automatically re-entrant by design (the same thread locking things twice will be a no-op).
	class AcquiredLock {

		c::SpinLock *lock;
		c::ELockAcquire acq;

		inline AcquiredLock(c::SpinLock *lock, c::ELockAcquire acq) noexcept:
			lock(lock), acq(acq) {}

		inline void release() noexcept {

			if (lock && acq == c::ELockAcquire_Acquired)
				SpinLock_unlock(lock);

			acq = c::ELockAcquire_Invalid;
			lock = nullptr;
		}

	public:

		inline AcquiredLock() noexcept : lock(nullptr), acq(c::ELockAcquire_Invalid) {}

		inline ~AcquiredLock() noexcept {
			release();
		}

		[[nodiscard]] static inline c::Bool acquire(c::SpinLock &lock, AcquiredLock &acqLock, c::Ns maxWait = c::U64_MAX) noexcept {

			c::ELockAcquire acq = SpinLock_lock(&lock, maxWait);

			if (acq < c::ELockAcquire_Success)
				return false;

			acqLock = AcquiredLock(&lock, acq);
			return true;
		}

		[[nodiscard]] inline c::Bool lockedForThread() const {
			return SpinLock_isLockedForThread(lock);
		}

		inline AcquiredLock(AcquiredLock &&other) noexcept :
			lock(other.lock), acq(other.acq) {
			other.release();
		}

		inline AcquiredLock &operator=(AcquiredLock &&other) noexcept {
			release();
			lock = other.lock;
			acq = other.acq;
			other.lock = nullptr;
			other.acq = c::ELockAcquire_Invalid;
			return *this;
		}

		AcquiredLock &operator=(const AcquiredLock&) = delete;
		AcquiredLock(const AcquiredLock&) = delete;
	};

	class Lock {
		c::SpinLock spinLock;
	public:

		Lock(): spinLock{ 0 } {}
		~Lock() = default;		//Avoid acquiring a lock past their lifetime.

		Lock(const Lock&) = delete;
		Lock &operator=(const Lock&) = delete;
		Lock(Lock&&) = delete;
		Lock &operator=(Lock&&) = delete;

		[[nodiscard]] inline c::Bool acquire(AcquiredLock &acqLock, c::Ns maxWait = c::U64_MAX) noexcept {
			return AcquiredLock::acquire(spinLock, acqLock, maxWait);
		}
	};
}
