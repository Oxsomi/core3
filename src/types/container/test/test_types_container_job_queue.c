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

//web: threads can't spawn (no -pthread yet), and JobQueue's inline mode (threadCount 1) runs jobs on the waiting thread.
//All ordering/fan-out/failure semantics still hold, so the suite runs with 1 context there instead of skipping.
//The include is not optional: _PLATFORM_TYPE and PLATFORM_WEB come from platform_types.h,
// and while both are undefined the preprocessor compares 0 == 0 and quietly picks 1 on EVERY platform,
// which turns the whole multi threaded JobQueue suite into a single threaded one without failing anything.

#include "types/base/platform_types.h"

#if _PLATFORM_TYPE == PLATFORM_WEB && !defined(__EMSCRIPTEN_PTHREADS__)
	#define JOBQUEUE_TEST_THREADS 1
#else
	#define JOBQUEUE_TEST_THREADS 4
#endif

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

//--- JobGroup payloads & callbacks --------------------------------------------

typedef struct GroupResult {
	AtomicI64 finalizeRan;
	AtomicI64 destructorRan;
} GroupResult;

typedef struct GroupWork {
	JobGroup *group;
	AtomicI64 *workDone;
	Bool fail;
} GroupWork;

//Does a unit of work, optionally fails the group, then releases its token.

static Bool jobGroupWorker(void *data, U64 threadId, JobQueue *queue) {
	(void) threadId; (void) queue;
	GroupWork *w = (GroupWork*) data;
	AtomicI64_inc(w->workDone);
	if(w->fail)
		(void) JobGroup_fail(w->group, NULL);
	return JobGroup_leave(w->group, NULL);
}

//Group finalize (data = GroupResult*).

static Bool jobGroupFinalize(void *data, U64 threadId, JobQueue *queue) {
	(void) threadId; (void) queue;
	AtomicI64_inc(&((GroupResult*) data)->finalizeRan);
	return true;
}

static void jobGroupDestructor(void *data) {
	AtomicI64_inc(&((GroupResult*) data)->destructorRan);
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

		if (Test_assert(t, "create multi threaded", JobQueue_create(JOBQUEUE_TEST_THREADS, alloc, &q, e_rr))) {

			Test_assert(t, "threadCount honored", JobQueue_threadCount(&q) == JOBQUEUE_TEST_THREADS);

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

		if (Test_assert(t, "create for failure test", JobQueue_create(JOBQUEUE_TEST_THREADS, alloc, &q, e_rr))) {

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

		if (Test_assert(t, "create for fan-out", JobQueue_create(JOBQUEUE_TEST_THREADS, alloc, &q, e_rr))) {

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

		if (Test_assert(t, "create for threadId range", JobQueue_create(JOBQUEUE_TEST_THREADS, alloc, &q, e_rr))) {

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

	//7. JobGroup success: last token released fires finalize exactly once (not the destructor).

	{
		JobQueue q = (JobQueue) { 0 };

		if (Test_assert(t, "create for JobGroup success", JobQueue_create(JOBQUEUE_TEST_THREADS, alloc, &q, e_rr))) {

			GroupResult result = { 0 };
			AtomicI64 workDone = (AtomicI64) { 0 };
			const U64 N = 100;

			JobGroup group = (JobGroup) { 0 };
			Test_assert(t, "JobGroup: create", JobGroup_create(&group, &q, jobGroupFinalize, &result, jobGroupDestructor, e_rr));
			Test_assert(t, "JobGroup: enter", JobGroup_enter(&group, N, e_rr));      //Count known up front

			GroupWork works[100];
			for (U64 i = 0; i < N; ++i) {
				works[i] = (GroupWork) { .group = &group, .workDone = &workDone, .fail = false };
				JobQueue_push(&q, jobGroupWorker, &works[i], e_rr);
			}

			Test_assert(t, "JobGroup: wait", JobQueue_wait(&q, e_rr));
			Test_assert(t, "JobGroup: all work ran", AtomicI64_load(&workDone) == (I64) N);
			Test_assert(t, "JobGroup: finalize ran exactly once", AtomicI64_load(&result.finalizeRan) == 1);
			Test_assert(t, "JobGroup: destructor did not run on success", AtomicI64_load(&result.destructorRan) == 0);
			Test_assert(t, "JobGroup: isSuccess", JobGroup_isSuccess(&group));
		}

		JobQueue_free(&q);
	}

	//8. JobGroup failure: a failed unit skips finalize and runs the destructor instead.

	{
		JobQueue q = (JobQueue) { 0 };

		if (Test_assert(t, "create for JobGroup failure", JobQueue_create(JOBQUEUE_TEST_THREADS, alloc, &q, e_rr))) {

			GroupResult result = { 0 };
			AtomicI64 workDone = (AtomicI64) { 0 };
			const U64 N = 100;

			JobGroup group = (JobGroup) { 0 };
			Test_assert(t, "JobGroup: create", JobGroup_create(&group, &q, jobGroupFinalize, &result, jobGroupDestructor, e_rr));
			Test_assert(t, "JobGroup: enter", JobGroup_enter(&group, N, e_rr));

			GroupWork works[100];
			for (U64 i = 0; i < N; ++i) {
				works[i] = (GroupWork) { .group = &group, .workDone = &workDone, .fail = (i == 50) };
				JobQueue_push(&q, jobGroupWorker, &works[i], e_rr);
			}

			Test_assert(t, "JobGroup(fail): wait", JobQueue_wait(&q, e_rr));
			Test_assert(t, "JobGroup(fail): all work still ran", AtomicI64_load(&workDone) == (I64) N);
			Test_assert(t, "JobGroup(fail): finalize skipped", AtomicI64_load(&result.finalizeRan) == 0);
			Test_assert(t, "JobGroup(fail): destructor ran once", AtomicI64_load(&result.destructorRan) == 1);
			Test_assert(t, "JobGroup(fail): isSuccess false", !JobGroup_isSuccess(&group));
		}

		JobQueue_free(&q);
	}
}
