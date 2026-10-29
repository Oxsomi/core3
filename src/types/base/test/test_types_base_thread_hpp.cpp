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

//types/base/test/test_types_base_thread_hpp.cpp

#include <utility>
#include <stdio.h>
#include "types/base/thread.hpp"
#include "types/base/error.hpp"

namespace oxc { namespace c {
	#include "types/test/test.h"
} }

static void ThreadTest_callback(void *object) {
	++*(oxc::c::U64*) object;
}

extern "C" void Test_threadHpp(oxc::c::Test *test, const oxc::c::Allocator *alloc, const oxc::c::U64 *live) {

	using namespace oxc;

	c::U64 completed = 0;
	const c::Error stdErr = Error::stdError(7, "Thread C++ error header test");

	Test_assert(test, "error wrapper accepts stdio include order", stdErr.genericError == c::EGenericError_Stderr);
	Test_assert(test, "standard error wrapper preserves sub ID", stdErr.errorSubId == 7);
	Test_assert(test, "priority wrapper rejects invalid enum", !Thread::setPriority(c::EThreadPriority_Count));
	Test_assert(test, "affinity wrapper validates CPU pointer", !Thread::setAffinity(nullptr, 1));

	{

		Thread thread;

		if(!Test_assert(test, "thread init", Thread::init(thread, alloc, ThreadTest_callback, &completed, &test->err)))
			return;

		Thread *alias = &thread;

		thread = std::move(*alias);
		Test_assert(test, "join waits for callback", thread.join(&test->err) && completed == 1);
		Test_assert(test, "join releases thread storage", *live == 0);
		Test_assert(test, "joining twice succeeds", thread.join(&test->err));
	}

	Test_assert(test, "joined destructor leaves no storage", *live == 0);

	{
		Thread thread;
		Test_assert(test, "thread reinitialization", Thread::init(thread, alloc, ThreadTest_callback, &completed, &test->err));
	}

	Test_assert(test, "destructor waits and releases", completed == 2 && *live == 0);
}
