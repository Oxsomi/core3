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

//types/base/test/test_types_base_thread.c

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include "test_types_base_shared.h"
#include "types/base/allocator.h"
#include "types/base/buffer_base.h"
#include "types/base/lock.h"
#include "types/base/platform_types.h"
#include "types/base/thread.h"
#include <stdalign.h>

#if _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID
	#include <sched.h>
	#include <sys/resource.h>
#elif _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
#endif

typedef struct ThreadTestAllocator {
	alignas(16) U8 storage[sizeof(Thread)];
	U64 live;
} ThreadTestAllocator;

static Bool ThreadTest_alloc(void *ptr, U64 length, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;
	ThreadTestAllocator *state = (ThreadTestAllocator*) ptr;

	if(state->live || length > sizeof(state->storage))
		retError(clean, Error_outOfMemory(0, "ThreadTest_alloc() storage is unavailable"));

	*result = Buffer_createManagedPtr(state->storage, length);
	++state->live;

clean:
	return s_uccess;
}

static void ThreadTest_free(void *ptr, Buffer allocation) {

	(void) allocation;

	ThreadTestAllocator *state = (ThreadTestAllocator*) ptr;

	--state->live;
}

void Test_threadHpp(Test *test, const Allocator *alloc, const U64 *live);

typedef struct ThreadTestLock {
	SpinLock lock;
	AtomicI64 waiting;
	ELockAcquire immediate, bounded, unlimited;
	Bool released;
} ThreadTestLock;

static void ThreadTest_waitForLock(void *ptr) {

	ThreadTestLock *state = (ThreadTestLock*) ptr;

	state->immediate = SpinLock_lock(&state->lock, 0);
	state->bounded = SpinLock_lock(&state->lock, 2 * MS);
	AtomicI64_store(&state->waiting, 1);
	state->unlimited = SpinLock_lock(&state->lock, U64_MAX);

	if(state->unlimited == ELockAcquire_Acquired)
		state->released = SpinLock_unlock(&state->lock);
}

typedef struct ThreadTestLifetime {
	AtomicI64 entered, completed;
} ThreadTestLifetime;

static void ThreadTest_finishBeforeFree(void *ptr) {

	ThreadTestLifetime *state = (ThreadTestLifetime*) ptr;

	AtomicI64_store(&state->entered, 1);
	Thread_sleep(50 * MS);
	AtomicI64_store(&state->completed, 1);
}

typedef struct ThreadTestPriority {
	Bool reported, inherited;
} ThreadTestPriority;

static void ThreadTest_changePriority(void *ptr) {

	ThreadTestPriority *state = (ThreadTestPriority*) ptr;
	Error err = Error_none();

	Thread_setPriority(EThreadPriority_Background, NULL);

	const Bool normal = Thread_setPriority(EThreadPriority_Normal, &err);

	state->reported = normal || err.genericError == EGenericError_PlatformError;
	state->inherited = true;

	#if _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID

		state->reported &= !normal || getpriority(PRIO_PROCESS, 0) == 0;

	#elif _PLATFORM_TYPE == PLATFORM_WINDOWS

		THREAD_POWER_THROTTLING_STATE throttling = { .Version = THREAD_POWER_THROTTLING_CURRENT_VERSION };

		if(GetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &throttling, sizeof(throttling)))
			state->inherited = !throttling.ControlMask;

	#endif
}

void Test_thread(Test *test) {

	Test_setModule(test, "Thread");

	void *captured[STACKTRACE_SIZE];

	for(U64 i = 0; i < STACKTRACE_SIZE; ++i)
		captured[i] = captured;

	Error_captureStackTrace(captured, 1, 100);
	Test_assert(test, "capture past the stack terminates empty output", !captured[0]);
	Test_assert(test, "capture respects output capacity", captured[1] == captured);

	Error_captureStackTrace(captured, STACKTRACE_SIZE, 0);

	Bool terminated = false;

	for(U64 i = 0; i < STACKTRACE_SIZE; ++i)
		terminated |= captured[i] == NULL;

	Test_assert(test, "short captured stack has a terminator", terminated);

	#if _PLATFORM_TYPE != PLATFORM_WEB || defined(__EMSCRIPTEN_PTHREADS__)

		ThreadTestAllocator state = (ThreadTestAllocator) { 0 };
		const Allocator alloc = { .ptr = &state, .alloc = ThreadTest_alloc, .free = ThreadTest_free };

		Test_threadHpp(test, &alloc, &state.live);

		ThreadTestLifetime lifetime = (ThreadTestLifetime) { 0 };
		Thread *liveThread = NULL;

		if (Test_assert(test, "start thread for direct free", Thread_create(
			&alloc, ThreadTest_finishBeforeFree, &lifetime, &liveThread, &test->err
		))) {

			while(!AtomicI64_load(&lifetime.entered))
				Thread_sleep(MS);

			Thread_free(&alloc, &liveThread);
			Test_assert(test, "direct free waits for callback", AtomicI64_load(&lifetime.completed) == 1);
			Test_assert(test, "direct free clears storage", !liveThread && !state.live);

			while(!AtomicI64_load(&lifetime.completed))
				Thread_sleep(MS);
		}

		ThreadTestLock contended = (ThreadTestLock) { 0 };
		Thread *thread = NULL;

		Test_assert(test, "acquire contention lock", SpinLock_lock(&contended.lock, 0) == ELockAcquire_Acquired);

		if (Test_assert(test, "start contending thread", Thread_create(
			&alloc, ThreadTest_waitForLock, &contended, &thread, &test->err
		))) {

			for(U64 i = 0; i < 1000 && !AtomicI64_load(&contended.waiting); ++i)
				Thread_sleep(5 * MS);

			Test_assert(test, "bounded wait completes while lock is held", AtomicI64_load(&contended.waiting) == 1);
			Thread_sleep(2 * MS);
			Test_assert(test, "release contention lock", SpinLock_unlock(&contended.lock));
			Test_assert(test, "join contending thread", Thread_waitAndCleanup(&alloc, &thread, &test->err));
			Test_assert(test, "contended zero timeout", contended.immediate == ELockAcquire_TimedOut);
			Test_assert(test, "contended bounded timeout", contended.bounded == ELockAcquire_TimedOut);
			Test_assert(test, "unlimited wait acquires after release", contended.unlimited == ELockAcquire_Acquired);
			Test_assert(test, "contending thread releases ownership", contended.released);
		}

		else SpinLock_unlock(&contended.lock);

		Test_assert(test, "contention test thread cleaned up", !state.live);

		lifetime = (ThreadTestLifetime) { 0 };

		if (Test_assert(test, "start repeatedly joined thread", Thread_create(
			&alloc, ThreadTest_finishBeforeFree, &lifetime, &liveThread, &test->err
		))) {

			Test_assert(test, "first C wait succeeds", Thread_wait(liveThread, &test->err));
			Test_assert(test, "repeated C wait succeeds", Thread_wait(liveThread, &test->err));
			Thread_free(&alloc, &liveThread);
			Test_assert(test, "free after wait clears storage", !liveThread && !state.live);
		}

		ThreadTestPriority priority = { 0 };

		if (Test_assert(test, "start priority test thread", Thread_create(
			&alloc, ThreadTest_changePriority, &priority, &liveThread, &test->err
		))) {

			Test_assert(test, "join priority test thread", Thread_waitAndCleanup(&alloc, &liveThread, &test->err));
			Test_assert(test, "priority reports OS refusal", priority.reported);
			Test_assert(test, "normal priority inherits process power policy", priority.inherited);
		}

	#endif

	#if _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID

		cpu_set_t original, selected, restored;

		if(!Test_assert(test, "read original affinity", !sched_getaffinity(0, sizeof(original), &original)))
			return;

		U32 cpu = 0;

		while(cpu < CPU_SETSIZE && !CPU_ISSET(cpu, &original))
			++cpu;

		if(!Test_assert(test, "original affinity has a CPU", cpu < CPU_SETSIZE))
			return;

		if(Test_assert(test, "restrict affinity", Thread_setAffinity(&cpu, 1, &test->err))) {

			Test_assert(test, "read restricted affinity", !sched_getaffinity(0, sizeof(selected), &selected));
			Test_assert(test, "only requested CPU selected", CPU_COUNT(&selected) == 1 && CPU_ISSET(cpu, &selected));
			Test_assert(test, "repeat affinity selection", Thread_setAffinity(&cpu, 1, &test->err));
			Test_assert(test, "restore original affinity", Thread_setAffinity(NULL, 0, &test->err));
			Test_assert(test, "read restored affinity", !sched_getaffinity(0, sizeof(restored), &restored));
			Test_assert(test, "restored affinity matches original", CPU_EQUAL(&original, &restored));
		}

		Test_assert(test, "restore test affinity", !sched_setaffinity(0, sizeof(original), &original));

	#endif
}
