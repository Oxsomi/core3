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

//types/base/test/test_types_base_lock_hpp.cpp
//
//Runs the C++ lock wrappers, where test_types_base_hpp.cpp only compiles them: an AcquiredLock that is moved keeps
//the lock held by its new owner, and the last owner releases it.

#include <utility>

#include "types/base/lock.hpp"

//The C test framework carries an extern "C" guard, so it is included inside oxc::c after the wrapper, which already
// pulled its C header dependencies in there (test_types_container_hpp.cpp does the same).

namespace oxc { namespace c {
	#include "types/test/test.h"
}}

extern "C" void Test_lockHpp(oxc::c::Test *t) {

	using namespace oxc;

	Test_setModule(t, "Lock/hpp");

	c::SpinLock lock{};

	{
		AcquiredLock first;
		Test_assert(t, "acquire", AcquiredLock::acquire(lock, first, 0) && first.lockedForThread());

		AcquiredLock second(std::move(first));
		Test_assert(t, "move construct keeps the lock", second.lockedForThread() && c::SpinLock_isLockedForThread(&lock));

		AcquiredLock third;
		third = std::move(second);
		Test_assert(t, "move assign keeps the lock", third.lockedForThread() && !second.lockedForThread());
	}

	Test_assert(t, "the last owner releases it", !c::SpinLock_isLockedForThread(&lock));
}
