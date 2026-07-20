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

//types/container/job_queue.h

#pragma once
#include "types/container/list.h"
#include "types/base/lock.h"
#include "types/base/atomic.h"
#include "types/base/allocator.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct Thread Thread;
typedef struct JobQueue JobQueue;

//A job callback executes on one of the queue's execution contexts.
//threadId is stable per executing thread and in [0, JobQueue_threadCount(queue)>,
//so it can safely be used to index per-thread resources (e.g. one Compiler per thread) without locking.
//threadId 0 is always the thread that calls JobQueue_wait (the owner);
//in single threaded mode it is the only context.
//Returning false marks the job (and thus the whole queue via JobQueue_isSuccess) as failed;
//it does not stop other jobs from running.
//A job is allowed to push new jobs onto its own queue (JobQueue_push), enabling fan-out
//pipelines (e.g. precompile jobs spawning compile jobs spawning link jobs).
typedef Bool (*JobCallback)(void *data, U64 threadId, JobQueue *queue);

//Optional; invoked on a job's data ONLY when the job is discarded by JobQueue_free before it ran.
//Lets owners of allocator-backed job data (e.g. the C++ push wrapper) avoid leaking on shutdown.
//A job that actually runs is responsible for its own cleanup inside its callback instead.
typedef void (*JobDestructor)(void *data);

typedef struct Job {
	JobCallback callback;
	void *data;
	JobDestructor destructor;       //May be NULL; only used for jobs discarded on shutdown
} Job;

TList(Job);
TListNamed(Thread*, ListThreadHandle);

//JobQueue is a simple FIFO work queue built on SpinLock + Thread + AtomicI64.
//It has two modes, selected at create time:
//- threadCount <= 1: single threaded mode. No worker threads are created;
//  all jobs run inline (in push order, deterministically) on the thread calling JobQueue_wait.
//  This mode exists to make debugging job-based code trivial.
//- threadCount > 1: threadCount - 1 workers are created and the thread calling JobQueue_wait
//  participates as the final execution context, so there are exactly threadCount contexts.
//The struct is address stable after create (workers hold a pointer to it); don't move or copy it.

typedef struct JobQueue {

	ListJob jobs;                   //FIFO, guarded by lock
	ListThreadHandle threads;       //threadCount - 1 workers, empty in single threaded mode

	SpinLock lock;

	AtomicI64 pending;              //Queued + currently running jobs
	AtomicI64 shutdown;             //Set on free; workers exit once the queue is drained
	AtomicI64 failedJobs;           //Number of jobs whose callback returned false
	AtomicI64 nextThreadId;         //Used by workers to claim a stable threadId (1 .. threadCount - 1)

	const Allocator *alloc;         //Must outlive the queue (same contract as RefPtr etc.)

	U64 threadCount;                //Execution contexts (>= 1)

} JobQueue;

//threadCount <= 1 creates a single threaded (inline) queue; see JobQueue struct docs.
//The caller decides the count; e.g. pass Platform_getThreads() from a higher level lib.
//queue must be zero initialized or a previously freed queue.
Bool JobQueue_create(U64 threadCount, const Allocator *alloc, JobQueue *queue, Error *e_rr);

//Enqueue a job. Safe to call from the owner thread and from within running jobs.
//data is owned by the caller and must outlive the job's execution.
Bool JobQueue_push(JobQueue *queue, JobCallback callback, void *data, Error *e_rr);

//Like JobQueue_push, but 'destructor' (if non-NULL) is invoked on data when the job is discarded
//by JobQueue_free before it ran, so allocator-backed job data isn't leaked on shutdown.
//A job that runs to completion must clean up inside its callback; destructor is not called then.
Bool JobQueue_pushDestructor(
	JobQueue *queue, JobCallback callback, void *data, JobDestructor destructor, Error *e_rr
);

//Run/help run jobs until the queue is fully drained (including jobs pushed by other jobs).
//Must be called by the thread that created the queue (it becomes execution context 0).
//Returns false (with e_rr untouched) only on invalid usage;
//job failures are reported through JobQueue_isSuccess instead so all jobs still run.
Bool JobQueue_wait(JobQueue *queue, Error *e_rr);

//False if any job callback returned false since create.
Bool JobQueue_isSuccess(const JobQueue *queue);

//Number of execution contexts; the valid range of threadId in callbacks is [0, threadCount>.
U64 JobQueue_threadCount(const JobQueue *queue);

//Signals shutdown, discards jobs that haven't started yet, joins all workers and frees memory.
//Call JobQueue_wait first if all pushed jobs should complete.
void JobQueue_free(JobQueue *queue);

#ifdef __cplusplus
	}
#endif
