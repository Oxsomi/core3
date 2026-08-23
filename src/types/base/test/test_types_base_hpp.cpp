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

//types/base/test/test_types_base_hpp.cpp
//
//Type check for the C++ base wrappers.
//types/base/lock.hpp and types/base/thread.hpp are hand written and, until this file existed, no translation
//unit included them, so nothing had ever compiled them. This TU exists so both headers are built by every
//configuration the tests are, which is what stops them drifting from the C API they wrap.
//
//The body below is deliberately never CALLED. It names every public wrapper so the compiler has to instantiate
//and typecheck it against the current C headers; running it would spawn a thread and block on a lock, which the
//C suites that do run already cover. A link time reference is enough to keep it honest.

//std::move, for the move only wrappers below; the headers themselves don't need it.

#include <utility>

#include "types/base/lock.hpp"
#include "types/base/thread.hpp"

//Never invoked. See the file comment: this is a compile time check, not a test module.

extern "C" void Test_typesBaseHppTypeCheck(
	oxc::c::SpinLock *rawLock,
	const oxc::c::Allocator *alloc,
	oxc::c::ThreadCallbackFunction callback,
	void *obj,
	oxc::c::Error *e_rr
) {

	using namespace oxc;

	//========================= oxc::Lock and oxc::AcquiredLock =========================

	//Lock owns its SpinLock, so it hands out an AcquiredLock either on the default infinite wait or on a timeout.

	Lock lock;
	AcquiredLock acquired;

	(void) lock.acquire(acquired);
	(void) lock.acquire(acquired, 100 * c::MS);

	//The static acquire is the way in for a SpinLock the caller already owns, such as one inside a C struct.

	AcquiredLock borrowed;
	(void) AcquiredLock::acquire(*rawLock, borrowed);
	(void) AcquiredLock::acquire(*rawLock, borrowed, c::SECOND);
	(void) borrowed.lockedForThread();

	//Move only: the source releases what it held, so an acquire stays paired with exactly one owner.

	AcquiredLock movedLock(std::move(borrowed));
	movedLock = std::move(acquired);

	//========================= oxc::Thread =========================

	//init replaces whatever the target held, so the same object can be handed a second thread later.

	Thread thread;
	(void) Thread::init(thread, alloc, callback, obj, e_rr);
	(void) thread.join(e_rr);
	(void) thread.join();

	//Move only as well, since the destructor of the source would otherwise wait on a thread it no longer owns.

	Thread movedThread(std::move(thread));
	(void) movedThread.join();

	Thread target;
	target = std::move(movedThread);

	//Both statics work without an instance; sleep and getId are plain wrappers over the C entrypoints.

	(void) Thread::getId();
	(void) Thread::sleep(c::MS);
}
