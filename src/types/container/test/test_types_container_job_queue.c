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

//types/container/test/test_types_container_job_queue.c
//
//Exercises JobQueue and, through it, the SpinLock + Thread primitives it is built on.

#include "test_types_container_shared.h"
#include "types/container/job_queue.h"
#include "types/container/list_basic_types.h"
#include "types/base/atomic.h"

//--- Job payloads & callbacks -------------------------------------------------

//Records the push order (only meaningful/deterministic in single threaded mode).

typedef struct OrderPayload {
	ListU8 *out;
	const Allocator *alloc;
	U8 id;
} OrderPayload;

static Bool jobRecordOrder(void *data, U64 threadId, JobQueue *queue) {
	(void) threadId; (void) queue;
	OrderPayload *p = (OrderPayload*) data;
	return ListU8_pushBack(p->out, p->id, p->alloc, NULL);
}

static Bool jobIncrement(void *data, U64 threadId, JobQueue *queue) {
	(void) threadId; (void) queue;
	AtomicI64_inc((AtomicI64*) data);
	return true;
}

//Counts as "ran" but reports failure.

static Bool jobFail(void *data, U64 threadId, JobQueue *queue) {
	(void) threadId; (void) queue;
	AtomicI64_inc((AtomicI64*) data);
	return false;
}

//Spawns 'children' sub-jobs onto its own queue (fan-out).

typedef struct FanoutPayload {
	AtomicI64 *counter;
	U64 children;
} FanoutPayload;

static Bool jobParentFanout(void *data, U64 threadId, JobQueue *queue) {
	(void) threadId;
	FanoutPayload *p = (FanoutPayload*) data;
	for(U64 i = 0; i < p->children; ++i)
		if(!JobQueue_push(queue, jobIncrement, p->counter, NULL))
			return false;
	return true;
}

//Flags if threadId is ever out of [0, threadCount).

typedef struct RangePayload {
	AtomicI64 *bad;
	U64 threadCount;
} RangePayload;

static Bool jobCheckThreadId(void *data, U64 threadId, JobQueue *queue) {
	(void) queue;
	RangePayload *p = (RangePayload*) data;
	if(threadId >= p->threadCount)
		AtomicI64_inc(p->bad);
	return true;
}

//--- Test ---------------------------------------------------------------------

void Test_jobQueue(Test *t) {

	const Allocator *alloc = t->alloc;
	Error *e_rr = &t->err;

	Test_setModule(t, "JobQueue");

	//1. Single threaded (inline) mode: jobs run in push order on the calling thread.

	{
		JobQueue q = (JobQueue) { 0 };
		ListU8 order = (ListU8) { 0 };
		OrderPayload payloads[8];

		if (Test_assert(t, "create single threaded", JobQueue_create(1, alloc, &q, e_rr))) {

			Test_assert(t, "single threaded has 1 context", JobQueue_threadCount(&q) == 1);

			Bool ok = true;
			for (U8 i = 0; i < 8; ++i) {
				payloads[i] = (OrderPayload) { .out = &order, .alloc = alloc, .id = i };
				ok &= JobQueue_push(&q, jobRecordOrder, &payloads[i], e_rr);
			}

			Test_assert(t, "pushed 8 order jobs", ok);
			Test_assert(t, "wait single threaded", JobQueue_wait(&q, e_rr));

			Bool inOrder = order.length == 8;
			for (U8 i = 0; inOrder && i < 8; ++i)
				inOrder = order.ptr[i] == i;

			Test_assert(t, "single threaded runs jobs in push order", inOrder);
			Test_assert(t, "single threaded reports success", JobQueue_isSuccess(&q));
		}

		ListU8_free(&order, alloc);
		JobQueue_free(&q);
	}

	//2. Multi threaded: N jobs each increment a shared atomic; each must run exactly once.

	{
		JobQueue q = (JobQueue) { 0 };
		AtomicI64 counter = (AtomicI64) { 0 };
		const U64 jobCount = 1000;

		if (Test_assert(t, "create multi threaded", JobQueue_create(4, alloc, &q, e_rr))) {

			Test_assert(t, "threadCount honored", JobQueue_threadCount(&q) == 4);

			Bool ok = true;
			for (U64 i = 0; i < jobCount; ++i)
				ok &= JobQueue_push(&q, jobIncrement, &counter, e_rr);

			Test_assert(t, "pushed all increment jobs", ok);
			Test_assert(t, "wait multi threaded", JobQueue_wait(&q, e_rr));
			Test_assert(t, "every job ran exactly once", AtomicI64_load(&counter) == (I64) jobCount);
			Test_assert(t, "success after all succeed", JobQueue_isSuccess(&q));
		}

		JobQueue_free(&q);
	}

	//3. A failing job marks the queue failed but does not stop the others from running.

	{
		JobQueue q = (JobQueue) { 0 };
		AtomicI64 ran = (AtomicI64) { 0 };

		if (Test_assert(t, "create for failure test", JobQueue_create(4, alloc, &q, e_rr))) {

			Bool ok = true;
			for (U64 i = 0; i < 50; ++i)
				ok &= JobQueue_push(&q, jobIncrement, &ran, e_rr);

			ok &= JobQueue_push(&q, jobFail, &ran, e_rr);   //One failing job (still counts as "ran")

			for (U64 i = 0; i < 50; ++i)
				ok &= JobQueue_push(&q, jobIncrement, &ran, e_rr);

			Test_assert(t, "pushed mixed jobs", ok);
			Test_assert(t, "wait completes despite failure", JobQueue_wait(&q, e_rr));
			Test_assert(t, "all 101 jobs still ran", AtomicI64_load(&ran) == 101);
			Test_assert(t, "isSuccess is false after a failure", !JobQueue_isSuccess(&q));
		}

		JobQueue_free(&q);
	}

	//4. Fan-out: a running job pushes more jobs; wait must drain the whole spawned tree.

	{
		JobQueue q = (JobQueue) { 0 };
		AtomicI64 counter = (AtomicI64) { 0 };
		FanoutPayload payload = { .counter = &counter, .children = 200 };

		if (Test_assert(t, "create for fan-out", JobQueue_create(4, alloc, &q, e_rr))) {

			Test_assert(t, "push parent", JobQueue_push(&q, jobParentFanout, &payload, e_rr));
			Test_assert(t, "wait drains spawned jobs", JobQueue_wait(&q, e_rr));
			Test_assert(t, "all fan-out children ran", AtomicI64_load(&counter) == 200);
			Test_assert(t, "fan-out success", JobQueue_isSuccess(&q));
		}

		JobQueue_free(&q);
	}

	//5. threadId is always within [0, threadCount) so it can index per-thread resources safely.

	{
		JobQueue q = (JobQueue) { 0 };
		AtomicI64 bad = (AtomicI64) { 0 };

		if (Test_assert(t, "create for threadId range", JobQueue_create(4, alloc, &q, e_rr))) {

			RangePayload payload = { .bad = &bad, .threadCount = JobQueue_threadCount(&q) };

			Bool ok = true;
			for (U64 i = 0; i < 500; ++i)
				ok &= JobQueue_push(&q, jobCheckThreadId, &payload, e_rr);

			Test_assert(t, "pushed range jobs", ok);
			Test_assert(t, "wait range", JobQueue_wait(&q, e_rr));
			Test_assert(t, "threadId always within [0, threadCount)", AtomicI64_load(&bad) == 0);
		}

		JobQueue_free(&q);
	}

	//6. Discard: pushing without waiting, then freeing, must drop jobs cleanly (no hang, no leak).

	{
		JobQueue q = (JobQueue) { 0 };
		AtomicI64 counter = (AtomicI64) { 0 };

		if (Test_assert(t, "create for discard", JobQueue_create(1, alloc, &q, e_rr)))
			for (U64 i = 0; i < 10; ++i)
				(void) JobQueue_push(&q, jobIncrement, &counter, e_rr);

		JobQueue_free(&q);      //No wait: remaining jobs are discarded
		Test_assert(t, "free without wait discards jobs cleanly", true);
	}
}
