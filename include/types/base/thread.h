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

//types/base/thread.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;

typedef void (*ThreadCallbackFunction)(void*);

typedef struct Thread {
	ThreadCallbackFunction callback;
	void *nativeHandle, *objectHandle;
} Thread;

impl U64 Thread_getId();                    //Current thread id

impl Bool Thread_sleep(Ns ns);              //Can be in a different time unit. Ex. on Windows it's rounded up to 100ns

impl Bool Thread_create(
	const Allocator *alloc,
	ThreadCallbackFunction callback,
	void *objectHandle,
	Thread **thread,
	Error *e_rr
);

void Thread_free(const Allocator *alloc, Thread **thread);

impl Bool Thread_wait(Thread *thread, Error *e_rr);
Bool Thread_waitAndCleanup(const Allocator *alloc, Thread **thread, Error *e_rr);

//How the OS should weigh the CALLING thread against the rest: Critical for work a frame waits on, Background for work
// that can wait. On a hybrid CPU Background also asks for the efficiency cores (EcoQoS on Windows, QoS classes on
// Apple). Best effort: what the OS refuses an unprivileged process (raising a priority on Linux) is left as it was.

typedef enum EThreadPriority {
	EThreadPriority_Normal,
	EThreadPriority_Critical,
	EThreadPriority_Background,
	EThreadPriority_Count
} EThreadPriority;

impl Bool Thread_setPriority(EThreadPriority priority, Error *e_rr);

//Restricts the CALLING thread to cpus, as PlatformCPUInfo's lists name them (CPU set ids on Windows, cpu indices on
// Linux and Android). A count of 0 lifts the restriction. Apple and the web don't let a thread pick its cores, so there
// it does nothing and succeeds.

impl Bool Thread_setAffinity(const U32 *cpus, U64 count, Error *e_rr);

#ifdef __cplusplus
	}
#endif
