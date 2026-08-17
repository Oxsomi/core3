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

//types/container/test/test_types_container_hpp.cpp
//
//Exercises the C++ RAII wrappers oxc::List and oxc::JobQueue (the C++ equivalents of the
//test_types_container_list.c and test_types_container_job_queue.c coverage), including
//C++ callables with captures and the shutdown-discard path (which must free their storage).

#include "types/container/list.hpp"
#include "types/container/job_queue.hpp"

//The C test framework carries an extern "C" guard, so include it inside oxc::c (after the wrappers,
// which already pulled its C-header deps into oxc::c).
//Its declarations then live in oxc::c with C linkage, matching the C-compiled framework,
// and share the same oxc::c::Allocator / Error types, so t->alloc is already an oxc::c::Allocator* (no cast, no shim).
namespace oxc { namespace c {
	#include "types/test/test.h"
}}

//Bind the wrapper to the C ListU32 (declared inside oxc::c by list.hpp), at global scope.
OXC_BIND_LIST(ListU32)

//Descending comparator for sortCustom, defined in C in test_types_container_list.c rather than here.
//A definition in this TU would return oxc::c::ECompareResult, since the C headers are included inside that
// namespace above, and the C sort that calls it expects the plain C enum.
//The two are the same type to the linker but not to -fsanitize=function, which reads that as a call through
// an incorrect function type; declaring it here and defining it in C keeps both sides honest.
extern "C" oxc::c::ECompareResult cmpU32Desc(const void *a, const void *b, void *context);

extern "C" void Test_hpp(oxc::c::Test *t) {

	using namespace oxc;

	const c::Allocator &alloc = *t->alloc;
	c::Error *e_rr = &t->err;

	Test_setModule(t, "hpp");

	//========================= oxc::List<ListU32> =========================

	{
		List<c::ListU32> list(alloc);

		c::Bool ok = true;
		for (c::U32 i = 0; i < 16; ++i)
			ok &= list.pushBack(i * 3, e_rr);
		Test_assert(t, "List: pushBack", ok);
		Test_assert(t, "List: size", list.size() == 16);

		c::Bool values = true;
		for (c::U32 i = 0; i < 16; ++i)
			values &= list[i] == i * 3;
		Test_assert(t, "List: indexing", values);

		Test_assert(t, "List: pushFront", list.pushFront(999, e_rr) && list[0] == 999);
		Test_assert(t, "List: insert", list.insert(2, 777, e_rr) && list[2] == 777);

		Test_assert(t, "List: findFirst", list.findFirst(777) == 2);
		Test_assert(t, "List: findFirst missing", list.findFirst(0xDEAD) == (c::U64) -1);   //U64_MAX
		Test_assert(t, "List: contains", list.contains(999) && !list.contains(0xDEAD));

		Test_assert(t, "List: erase", list.erase(2, e_rr) && !list.contains(777));

		Test_assert(t, "List: pushBack duplicate", list.pushBack(999, e_rr));
		Test_assert(t, "List: eraseAll", list.eraseAll(999, e_rr) && !list.contains(999));

		c::U32 popped = 0;
		Test_assert(t, "List: popBack", list.popBack(popped, e_rr) && popped == 45);
		Test_assert(t, "List: popFront", list.popFront(popped, e_rr) && popped == 0);

		Test_assert(t, "List: reserve", list.reserve(128, e_rr));
		Test_assert(t, "List: resize", list.resize(4, e_rr) && list.size() == 4);
		list.clear();
		Test_assert(t, "List: clear", list.empty());

		//sortCustom (descending) + reverse + swap

		c::Bool ok2 = true;
		const c::U32 vals[5] = { 5, 1, 4, 2, 3 };
		for (int i = 0; i < 5; ++i)
			ok2 &= list.pushBack(vals[i], e_rr);
		Test_assert(t, "List: pushBack for sort", ok2);

		Test_assert(t, "List: sortCustom (descending)", list.sortCustom(cmpU32Desc));
		Test_assert(t, "List: sortCustom result", list[0] == 5 && list[4] == 1);
		Test_assert(t, "List: reverse", list.reverse() && list[0] == 1 && list[4] == 5);
		Test_assert(t, "List: swap", list.swap(0, 4, e_rr) && list[0] == 5 && list[4] == 1);

		//A C++ comparator, captures and all, with no C linkage anywhere in sight.
		//It ranks by a table that lives outside the elements, so the capture is the only thing that can
		// express the order: values 1..5 rank 30, 10, 50, 20, 40, which sorts them 2, 4, 1, 5, 3.

		const c::U32 rank[6] = { 0, 30, 10, 50, 20, 40 };

		Test_assert(t, "List: sortCustom (callable)", list.sortCustom(
			[&rank](const c::U32 &a, const c::U32 &b) {
				return rank[a] < rank[b]
					? c::ECompareResult_Lt
					: (rank[a] > rank[b] ? c::ECompareResult_Gt : c::ECompareResult_Eq);
			}
		));

		Test_assert(
			t, "List: sortCustom callable followed the capture",
			list[0] == 2 && list[1] == 4 && list[2] == 1 && list[3] == 5 && list[4] == 3
		);

		//range-for over begin()/end()

		c::U64 sum = 0;
		for (c::U32 v : list)
			sum += v;
		Test_assert(t, "List: range-for", sum == 15);

		//createRepeated + operator== / operator!=

		List<c::ListU32> rep(alloc), rep2(alloc);
		Test_assert(t, "List: createRepeated", rep.createRepeated(4, 7, e_rr));
		Test_assert(t, "List: createRepeated values", rep.size() == 4 && rep[0] == 7 && rep[3] == 7);
		Test_assert(t, "List: createRepeated 2", rep2.createRepeated(4, 7, e_rr));
		Test_assert(t, "List: operator==", rep == rep2);
		rep2[0] = 8;
		Test_assert(t, "List: operator!=", rep != rep2);
	}

	//========================= oxc::JobQueue =========================

	//Multi threaded: N C++ callables each increment a shared atomic exactly once.

	{
		JobQueue queue;
		Test_assert(t, "JobQueue: init", queue.init(4, alloc));
		Test_assert(t, "JobQueue: threadCount", queue.threadCount() == 4);

		c::AtomicI64 counter{};
		c::Bool pushed = true;
		for (int i = 0; i < 500; ++i)
			pushed &= queue.push([&counter](c::U64) { c::AtomicI64_inc(&counter); return true; });

		Test_assert(t, "JobQueue: push callables", pushed);
		Test_assert(t, "JobQueue: wait", queue.wait());
		Test_assert(t, "JobQueue: every callable ran once", c::AtomicI64_load(&counter) == 500);
		Test_assert(t, "JobQueue: success", queue.isSuccess());
	}

	//A failing callable marks the queue failed but the rest still run.

	{
		JobQueue queue;
		Test_assert(t, "JobQueue: init for failure", queue.init(4, alloc));

		c::AtomicI64 ran{};
		c::Bool ok = true;
		for (int i = 0; i < 50; ++i)
			ok &= queue.push([&ran](c::U64) { c::AtomicI64_inc(&ran); return true; });
		ok &= queue.push([&ran](c::U64) { c::AtomicI64_inc(&ran); return false; });
		for (int i = 0; i < 50; ++i)
			ok &= queue.push([&ran](c::U64) { c::AtomicI64_inc(&ran); return true; });

		Test_assert(t, "JobQueue: pushed mixed", ok);
		Test_assert(t, "JobQueue: wait despite failure", queue.wait());
		Test_assert(t, "JobQueue: all 101 ran", c::AtomicI64_load(&ran) == 101);
		Test_assert(t, "JobQueue: isSuccess false", !queue.isSuccess());
	}

	//Fan-out: a callable pushes more callables; wait must drain the whole tree.

	{
		JobQueue queue;
		Test_assert(t, "JobQueue: init for fan-out", queue.init(4, alloc));

		c::AtomicI64 counter{};
		Test_assert(t, "JobQueue: push parent", queue.push([&counter, &queue](c::U64) {
			c::Bool ok = true;
			for (int i = 0; i < 200; ++i)
				ok &= queue.push([&counter](c::U64) { c::AtomicI64_inc(&counter); return true; });
			return (bool) ok;
		}));

		Test_assert(t, "JobQueue: fan-out wait", queue.wait());
		Test_assert(t, "JobQueue: fan-out children ran", c::AtomicI64_load(&counter) == 200);
		Test_assert(t, "JobQueue: fan-out success", queue.isSuccess());
	}

	//Discard path: push C++ callables, never wait; ~JobQueue must destroy their storage
	//(the suite's leak detector is the real check).

	{
		JobQueue queue;
		Test_assert(t, "JobQueue: init for discard", queue.init(1, alloc));
		for (int i = 0; i < 20; ++i)
			(void) queue.push([](c::U64) { return true; });
	}
	Test_assert(t, "JobQueue: discard path frees storage (see leak check)", true);
}
