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
#include "types/container/list_basic_types.h"

TListImpl(Job);
TListNamedImpl(ListThreadHandle);

static const Ns JobQueue_idleSleep = 100000;        //100 * MU

//Which workers a thread id is (see the JobQueue struct docs); 0 is the owner

typedef enum EJobWorker {
	EJobWorker_Owner,
	EJobWorker_Reserved,
	EJobWorker_Performance,
	EJobWorker_Efficiency
} EJobWorker;

static EJobWorker JobQueue_worker(const JobQueue *queue, U64 threadId) {

	if(!threadId)
		return EJobWorker_Owner;

	if(threadId <= queue->reservedWorkers)
		return EJobWorker_Reserved;

	return threadId + queue->efficiencyWorkers >= queue->threadCount ? EJobWorker_Efficiency : EJobWorker_Performance;
}

//The lanes a worker takes, in the order it takes them

static const EJobLane JobQueue_order[4][EJobLane_Count] = {
	{ EJobLane_Critical, EJobLane_Normal, EJobLane_Background },     //Owner
	{ EJobLane_Critical, EJobLane_Normal, EJobLane_Count },          //Reserved
	{ EJobLane_Critical, EJobLane_Normal, EJobLane_Background },     //Performance
	{ EJobLane_Normal, EJobLane_Background, EJobLane_Count }         //Efficiency
};

//Pop the next job this thread takes, or when filtered, the next one tagged with 'group' in any lane.
//A filtered pop skips jobs it doesn't match rather than waiting for them, so a caller draining its own
// group never takes on work that could want a lock it is already holding.
//Returns false if the queue currently holds no such job.

static Bool JobQueue_pop(JobQueue *queue, U64 threadId, Bool filtered, const JobGroup *group, Job *job) {

	Bool popped = false;

	const ELockAcquire acq = SpinLock_lock(&queue->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	const EJobLane *order = JobQueue_order[filtered ? EJobWorker_Owner : JobQueue_worker(queue, threadId)];

	for(U64 k = 0; !popped && k < EJobLane_Count && order[k] != EJobLane_Count; ++k) {

		ListJob *jobs = &queue->jobs[order[k]];

		if(!filtered) {
			if(jobs->length)
				popped = ListJob_popFront(jobs, job, NULL);
		}

		else for(U64 i = 0; i < jobs->length; ++i)
			if (jobs->ptr[i].group == group) {
				popped = ListJob_popLocation(jobs, i, job, NULL);
				break;
			}
	}

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&queue->lock);

	return popped;
}

//Run one job and update bookkeeping.
//Returns false if no job was available.

static Bool JobQueue_runOne(JobQueue *queue, U64 threadId, Bool filtered, const JobGroup *group) {

	Job job = (Job) { 0 };

	if(!JobQueue_pop(queue, threadId, filtered, group, &job))
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

	//Placed by class; best effort, since a worker that couldn't be placed still runs its lanes

	const EJobWorker worker = JobQueue_worker(queue, threadId);
	const ListU32 cpus = worker == EJobWorker_Efficiency ? queue->efficiencyCpus : queue->performanceCpus;

	if(cpus.length)
		Thread_setAffinity(cpus.ptr, cpus.length, NULL);

	if(worker == EJobWorker_Efficiency)
		Thread_setPriority(EThreadPriority_Background, NULL);

	else if(worker == EJobWorker_Reserved)
		Thread_setPriority(EThreadPriority_Critical, NULL);

	while(true) {

		if(JobQueue_runOne(queue, threadId, false, NULL))
			continue;

		if(AtomicI64_load(&queue->shutdown))
			break;

		Thread_sleep(JobQueue_idleSleep);
	}
}

Bool JobQueue_create(U64 threadCount, const Allocator *alloc, JobQueue *queue, Error *e_rr) {
	const JobQueueInfo info = (JobQueueInfo) { .threadCount = threadCount };
	return JobQueue_createInfo(&info, alloc, queue, e_rr);
}

Bool JobQueue_createInfo(const JobQueueInfo *info, const Allocator *alloc, JobQueue *queue, Error *e_rr) {

	Bool s_uccess = true;

	if(!queue || !info)
		retError(clean, Error_nullPointer(!queue ? 2 : 0, "JobQueue_createInfo()::info and queue are required"));

	if(!alloc)
		retError(clean, Error_nullPointer(1, "JobQueue_createInfo()::alloc is required"));

	for(U64 i = 0; i < EJobLane_Count; ++i)
		if(queue->jobs[i].ptr)
			retError(clean, Error_invalidParameter(
				2, 0, "JobQueue_createInfo()::queue wasn't zero initialized, might indicate memleak"
			));

	if(queue->threads.ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "JobQueue_createInfo()::queue wasn't zero initialized, might indicate memleak"
		));

	const U64 threadCount = info->threadCount ? info->threadCount : 1;

	if((U64) info->reservedWorkers + info->efficiencyWorkers > threadCount - 1)
		retError(clean, Error_outOfBounds(
			0, (U64) info->reservedWorkers + info->efficiencyWorkers, threadCount - 1,
			"JobQueue_createInfo()::info has more reserved and efficiency workers than workers"
		));

	if((info->performanceCpuCount && !info->performanceCpus) || (info->efficiencyCpuCount && !info->efficiencyCpus))
		retError(clean, Error_nullPointer(0, "JobQueue_createInfo()::info's cpu lists are required for a count"));

	*queue = (JobQueue) {
		.alloc = alloc,
		.threadCount = threadCount,
		.reservedWorkers = info->reservedWorkers,
		.efficiencyWorkers = info->efficiencyWorkers
	};

	ListU32 cpus = (ListU32) { 0 };

	if(info->performanceCpuCount) {
		gotoIfError3(clean, ListU32_createRefConst(info->performanceCpus, info->performanceCpuCount, &cpus, e_rr));
		gotoIfError3(clean, ListU32_createCopy(cpus, alloc, &queue->performanceCpus, e_rr));
	}

	if(info->efficiencyCpuCount) {
		gotoIfError3(clean, ListU32_createRefConst(info->efficiencyCpus, info->efficiencyCpuCount, &cpus, e_rr));
		gotoIfError3(clean, ListU32_createCopy(cpus, alloc, &queue->efficiencyCpus, e_rr));
	}

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

Bool JobQueue_pushGroup(
	JobQueue *queue, JobCallback callback, void *data, JobDestructor destructor, JobGroup *group, Error *e_rr
) {
	return JobQueue_pushLane(queue, EJobLane_Normal, callback, data, destructor, group, e_rr);
}

Bool JobQueue_pushLane(
	JobQueue *queue,
	EJobLane lane,
	JobCallback callback,
	void *data,
	JobDestructor destructor,
	JobGroup *group,
	Error *e_rr
) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!queue)
		retError(clean, Error_nullPointer(0, "JobQueue_push()::queue is required"));

	if(lane >= EJobLane_Count)
		retError(clean, Error_invalidEnum(1, (U64) lane, EJobLane_Count, "JobQueue_pushLane()::lane"));

	if(!callback)
		retError(clean, Error_nullPointer(1, "JobQueue_push()::callback is required"));

	if(AtomicI64_load(&queue->shutdown))
		retError(clean, Error_invalidState(0, "JobQueue_push() queue is shutting down"));

	acq = SpinLock_lock(&queue->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "JobQueue_push() couldn't acquire lock"));

	const Job job = (Job) {
		.callback = callback, .data = data, .destructor = destructor, .group = group, .lane = lane
	};

	gotoIfError3(clean, ListJob_pushBack(&queue->jobs[lane], job, queue->alloc, e_rr));

	AtomicI64_inc(&queue->pending);

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&queue->lock);

	return s_uccess;
}

Bool JobQueue_pushDestructor(
	JobQueue *queue, JobCallback callback, void *data, JobDestructor destructor, Error *e_rr
) {
	return JobQueue_pushGroup(queue, callback, data, destructor, NULL, e_rr);
}

Bool JobQueue_push(JobQueue *queue, JobCallback callback, void *data, Error *e_rr) {
	return JobQueue_pushGroup(queue, callback, data, NULL, NULL, e_rr);
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

		if(JobQueue_runOne(queue, 0, false, NULL))
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

	//Destroy jobs that never ran so allocator-backed job data (e.g. C++ callables) isn't leaked.

	for(U64 lane = 0; lane < EJobLane_Count; ++lane) {

		ListJob *jobs = &queue->jobs[lane];
		AtomicI64_sub(&queue->pending, (I64) jobs->length);

		for(U64 i = 0; i < jobs->length; ++i) {
			const Job discarded = jobs->ptr[i];
			if(discarded.destructor)
				discarded.destructor(discarded.data);
		}

		ListJob_clear(jobs, NULL);
	}

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&queue->lock);

	AtomicI64_store(&queue->shutdown, 1);

	for(U64 i = 0; i < queue->threads.length; ++i)
		if(queue->threads.ptr[i])       //Might be partially created if JobQueue_create failed midway
			Thread_waitAndCleanup(queue->alloc, &queue->threads.ptrNonConst[i], NULL);

	ListThreadHandle_free(&queue->threads, queue->alloc);
	ListU32_free(&queue->performanceCpus, queue->alloc);
	ListU32_free(&queue->efficiencyCpus, queue->alloc);

	for(U64 lane = 0; lane < EJobLane_Count; ++lane)
		ListJob_free(&queue->jobs[lane], queue->alloc);

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

Bool JobGroup_wait(JobGroup *group, Error *e_rr) {

	Bool s_uccess = true;

	if(!group)
		retError(clean, Error_nullPointer(0, "JobGroup_wait()::group is required"));

	if(!group->queue)
		retError(clean, Error_nullPointer(0, "JobGroup_wait()::group->queue is required"));

	while (AtomicI64_load(&group->outstanding)) {

		if(JobQueue_runOne(group->queue, 0, true, group))
			continue;

		//Tokens are still held, but by jobs already running on workers or not pushed yet.

		Thread_sleep(JobQueue_idleSleep);
	}

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
