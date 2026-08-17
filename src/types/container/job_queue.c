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

//types/container/job_queue.c

#include "types/container/list_impl.h"
#include "types/container/job_queue.h"
#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/constants.h"

TListImpl(Job);
TListNamedImpl(ListThreadHandle);

static const Ns JobQueue_idleSleep = 100000;        //100 * MU

//Pop the next job.
//Returns false if the queue is currently empty.

static Bool JobQueue_pop(JobQueue *queue, Job *job) {

	Bool popped = false;

	const ELockAcquire acq = SpinLock_lock(&queue->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	if(queue->jobs.length)
		popped = ListJob_popFront(&queue->jobs, job, NULL);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&queue->lock);

	return popped;
}

//Run one job and update bookkeeping.
//Returns false if no job was available.

static Bool JobQueue_runOne(JobQueue *queue, U64 threadId) {

	Job job = (Job) { 0 };

	if(!JobQueue_pop(queue, &job))
		return false;

	if(!job.callback || !job.callback(job.data, threadId, queue))
		AtomicI64_inc(&queue->failedJobs);

	AtomicI64_dec(&queue->pending);
	return true;
}

//Worker thread entrypoint

static void JobQueue_workerLoop(void *queuePtr) {

	JobQueue *queue = (JobQueue*) queuePtr;

	if(!queue)
		return;

	const U64 threadId = (U64) AtomicI64_inc(&queue->nextThreadId);     //Claim stable id (1 .. threadCount - 1)

	while(true) {

		if(JobQueue_runOne(queue, threadId))
			continue;

		if(AtomicI64_load(&queue->shutdown))
			break;

		Thread_sleep(JobQueue_idleSleep);
	}
}

Bool JobQueue_create(U64 threadCount, const Allocator *alloc, JobQueue *queue, Error *e_rr) {

	Bool s_uccess = true;

	if(!queue)
		retError(clean, Error_nullPointer(2, "JobQueue_create()::queue is required"));

	if(!alloc)
		retError(clean, Error_nullPointer(1, "JobQueue_create()::alloc is required"));

	if(queue->jobs.ptr || queue->threads.ptr)
		retError(clean, Error_invalidParameter(2, 0, "JobQueue_create()::queue wasn't zero initialized, might indicate memleak"));

	if(!threadCount)
		threadCount = 1;

	*queue = (JobQueue) {
		.alloc = alloc,
		.threadCount = threadCount
	};

	//The owner thread (which calls JobQueue_wait) is context 0, so only spawn threadCount - 1 workers.
	//In single threaded mode this spawns nothing and everything runs inline in JobQueue_wait.

	if (threadCount > 1) {

		gotoIfError3(clean, ListThreadHandle_resize(&queue->threads, threadCount - 1, alloc, e_rr));

		for(U64 i = 0; i < threadCount - 1; ++i)
			gotoIfError3(clean, Thread_create(
				alloc, JobQueue_workerLoop, queue, &queue->threads.ptrNonConst[i], e_rr
			));
	}

clean:

	if(!s_uccess && queue)
		JobQueue_free(queue);

	return s_uccess;
}

Bool JobQueue_pushDestructor(
	JobQueue *queue, JobCallback callback, void *data, JobDestructor destructor, Error *e_rr
) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!queue)
		retError(clean, Error_nullPointer(0, "JobQueue_push()::queue is required"));

	if(!callback)
		retError(clean, Error_nullPointer(1, "JobQueue_push()::callback is required"));

	if(AtomicI64_load(&queue->shutdown))
		retError(clean, Error_invalidState(0, "JobQueue_push() queue is shutting down"));

	acq = SpinLock_lock(&queue->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "JobQueue_push() couldn't acquire lock"));

	const Job job = (Job) { .callback = callback, .data = data, .destructor = destructor };
	gotoIfError3(clean, ListJob_pushBack(&queue->jobs, job, queue->alloc, e_rr));

	AtomicI64_inc(&queue->pending);

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&queue->lock);

	return s_uccess;
}

Bool JobQueue_push(JobQueue *queue, JobCallback callback, void *data, Error *e_rr) {
	return JobQueue_pushDestructor(queue, callback, data, NULL, e_rr);
}

//See JobInvoke in the header for why a wrapper's callback goes through here rather than being handed to the
// queue directly.
//The whole data pointer is forwarded, not the JobWrapperJob, so the wrapper can cast it back to its own type.

Bool JobQueue_wrapperCallback(void *data, U64 threadId, JobQueue *queue) {

	(void) queue;

	if(!data)
		return false;

	const JobWrapperJob *job = (const JobWrapperJob*) data;

	if(!job->invoke)
		return false;

	return job->invoke(data, threadId);
}

Bool JobQueue_wait(JobQueue *queue, Error *e_rr) {

	Bool s_uccess = true;

	if(!queue)
		retError(clean, Error_nullPointer(0, "JobQueue_wait()::queue is required"));

	//The waiting thread participates as execution context 0.
	//pending only hits 0 once all jobs *and* the jobs they spawned have finished,
	// so this also covers multi-stage fan-out pipelines.

	while (AtomicI64_load(&queue->pending)) {

		if(JobQueue_runOne(queue, 0))
			continue;

		//Nothing queued, but jobs are still running on workers and might spawn more.

		Thread_sleep(JobQueue_idleSleep);
	}

clean:
	return s_uccess;
}

Bool JobQueue_isSuccess(const JobQueue *queue) {
	return queue && !AtomicI64_load((AtomicI64*) &queue->failedJobs);
}

U64 JobQueue_threadCount(const JobQueue *queue) {
	return !queue ? 0 : queue->threadCount;
}

void JobQueue_free(JobQueue *queue) {

	if(!queue)
		return;

	//Discard jobs that haven't started and signal shutdown, then join workers.
	//Workers finish their current job before exiting.

	const ELockAcquire acq = SpinLock_lock(&queue->lock, U64_MAX);

	AtomicI64_sub(&queue->pending, (I64) queue->jobs.length);

	//Destroy jobs that never ran so allocator-backed job data (e.g. C++ callables) isn't leaked.

	for(U64 i = 0; i < queue->jobs.length; ++i) {
		const Job discarded = queue->jobs.ptr[i];
		if(discarded.destructor)
			discarded.destructor(discarded.data);
	}

	ListJob_clear(&queue->jobs, NULL);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&queue->lock);

	AtomicI64_store(&queue->shutdown, 1);

	for(U64 i = 0; i < queue->threads.length; ++i)
		if(queue->threads.ptr[i])       //Might be partially created if JobQueue_create failed midway
			Thread_waitAndCleanup(queue->alloc, &queue->threads.ptrNonConst[i], NULL);

	ListThreadHandle_free(&queue->threads, queue->alloc);
	ListJob_free(&queue->jobs, queue->alloc);

	*queue = (JobQueue) { 0 };
}

Bool JobGroup_create(
	JobGroup *group, JobQueue *queue, JobCallback finalize, void *data, JobDestructor dataDestructor, Error *e_rr
) {
	Bool s_uccess = true;

	if(!group)
		retError(clean, Error_nullPointer(0, "JobGroup_create()::group is required"));

	if(finalize && !queue)
		retError(clean, Error_nullPointer(1, "JobGroup_create()::queue is required when finalize is set"));

	if(group->finalize || group->data || AtomicI64_load(&group->outstanding))
		retError(clean, Error_invalidParameter(0, 0, "JobGroup_create()::group wasn't zero initialized"));

	*group = (JobGroup) {
		.queue = queue,
		.finalize = finalize,
		.data = data,
		.dataDestructor = dataDestructor
	};

clean:
	return s_uccess;
}

Bool JobGroup_enter(JobGroup *group, U64 count, Error *e_rr) {

	Bool s_uccess = true;

	if(!group)
		retError(clean, Error_nullPointer(0, "JobGroup_enter()::group is required"));

	if(count)
		AtomicI64_add(&group->outstanding, (I64) count);

clean:
	return s_uccess;
}

Bool JobGroup_fail(JobGroup *group, Error *e_rr) {

	Bool s_uccess = true;

	if(!group)
		retError(clean, Error_nullPointer(0, "JobGroup_fail()::group is required"));

	AtomicI64_store(&group->failed, 1);

clean:
	return s_uccess;
}

Bool JobGroup_isSuccess(const JobGroup *group) {
	return group && !AtomicI64_load((AtomicI64*) &group->failed);
}

Bool JobGroup_leave(JobGroup *group, Error *e_rr) {

	Bool s_uccess = true;

	if(!group)
		retError(clean, Error_nullPointer(0, "JobGroup_leave()::group is required"));

	const I64 remaining = AtomicI64_dec(&group->outstanding);

	if(remaining > 0)                   //Not the last token; nothing to do yet
		goto clean;

	if(remaining < 0)                   //Underflow: more leaves than enters (usage bug)
		retError(clean, Error_invalidState(0, "JobGroup_leave() released more tokens than were entered"));

	//Last token: fire finalize on success, otherwise free the accumulator.
	//The finalize job carries dataDestructor so data is still freed if it is discarded on shutdown.

	if(!AtomicI64_load(&group->failed) && group->finalize) {
		gotoIfError3(clean, JobQueue_pushDestructor(
			group->queue, group->finalize, group->data, group->dataDestructor, e_rr
		));
	}

	else if(group->dataDestructor)
		group->dataDestructor(group->data);

clean:
	return s_uccess;
}
