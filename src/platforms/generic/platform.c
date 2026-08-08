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

//platforms/generic/platform.c

#include "types/container/list_impl.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "formats/oiCA/ca_file.h"
#include "types/base/allocator.h"
#include "types/base/thread.h"
#include "types/base/constants.h"

#include <signal.h>
#include <stdlib.h>

TListImpl(VirtualSection);

//Error handling

void sigFunc(int signal) {

	const C8 *msg = "Undefined instruction";

	switch (signal) {
		case SIGABRT:    msg = "Abort was called";                    break;
		case SIGFPE:    msg = "Floating point error occurred";        break;
		case SIGILL:    msg = "Illegal instruction";                break;
		case SIGINT:    msg = "Interrupt was called";                break;
		case SIGSEGV:    msg = "Segfault";                            break;
		case SIGTERM:    msg = "Terminate was called";                break;
	}

	//Outputting to console is not technically allowed by the Windows docs
	//If this signal is triggered from the wrong thread it might cause stackoverflow,
	//but what are you gonna do? Crash again?
	//For debugging purposes however, this is very useful
	//Turn this off by defining _NO_SIGNAL_HANDLING

	CharString msgStr = CharString_createRefCStrConst(msg);

	Log_printStackTrace(Platform_instance->alloc, 1, ELogLevel_Error, ELogOptions_Default);
	Log_log(Platform_instance->alloc, ELogLevel_Error, ELogOptions_Default, &msgStr);
	exit(signal);
}

//Allocator keeps track of allocations on debug mode (and only about size/count on release)
//For these allocations, it has a special allocator that doesn't use itself.

AtomicI64 Allocator_memoryAllocationCount = { 0 };
AtomicI64 Allocator_memoryAllocationSize = { 0 };

typedef struct DebugAllocation {
	U64 location, length;
	StackTrace stack;
} DebugAllocation;

TList(DebugAllocation);
TListImpl(DebugAllocation);

Allocator Allocator_allocationsAllocator;
Allocator Allocator_trackedAllocator;
ListDebugAllocation Allocator_allocations;            //TODO: Use hashmap here!
SpinLock Allocator_lock;                            //Multi threading safety

//Allocation

Bool Platform_allocNoTracking(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	if(!output)
		retError(clean, Error_nullPointer(2, "allocCallbackNoCheck()::output is required"));

	void *ptr = Platform_allocate(allocator, length);

	if(!ptr)
		retError(clean, Error_outOfMemory(0, "allocCallbackNoCheck() malloc failed"));

	*output = Buffer_createManagedPtr(ptr, length);

clean:
	return s_uccess;
}

void Platform_freeNoTracking(void *allocator, Buffer buf) {
	Platform_free(allocator, buf.ptrNonConst, Buffer_length(buf));
}

//Normal allocator

Bool Platform_allocTracked(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;
	void *ptr = NULL;

	if(!output)
		retError(clean, Error_nullPointer(2, "allocCallback()::output is required"));

	ptr = Platform_allocate(allocator, length);
	allocated = true;

	if(!ptr)
		retError(clean, Error_outOfMemory(0, "allocCallback() malloc failed"));

	gotoIfError3(clean, Platform_onAllocate(ptr, length, e_rr));

	*output = Buffer_createManagedPtr(ptr, length);
clean:

	if(!s_uccess && allocated)
		Platform_free(allocator, ptr, length);

	return s_uccess;
}

void Platform_freeTracked(void *allocator, Buffer buf) {

	(void) allocator;

	if(Platform_onFree(buf.ptrNonConst, Buffer_length(buf)))
		Platform_free(allocator, buf.ptrNonConst, Buffer_length(buf));
}

Bool Platform_onAllocate(void *ptr, U64 length, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	(void)ptr; (void) e_rr;

	AtomicI64_add(&Allocator_memoryAllocationCount, 1);
	AtomicI64_add(&Allocator_memoryAllocationSize, length);

	#ifndef NDEBUG

		DebugAllocation captured = (DebugAllocation) { 0 };
		captured.location = (U64) ptr;
		captured.length = length;

		Error_captureStackTrace(captured.stack, STACKTRACE_SIZE, 1);

		acq = SpinLock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success)        //Should never happen
			retError(clean, Error_invalidState(0, "Platform_onAllocate() allocator lock failed to acquire"));

		gotoIfError3(clean, ListDebugAllocation_pushBack(
			&Allocator_allocations, captured, &Allocator_allocationsAllocator, e_rr
		));

	#endif

	goto clean;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Allocator_lock);

	return s_uccess;
}

Bool Platform_onFree(void *ptr, U64 len) {

	(void) ptr;

	//Validate if allocation and allocation size matches.
	//If not, warn here and return false

	ELockAcquire acq = ELockAcquire_Invalid;
	Error *e_rr = NULL;
	Bool s_uccess = true;

	(void)e_rr;

	#ifndef NDEBUG

		acq = SpinLock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success) {
			Log_errorLn(&Allocator_allocationsAllocator, "Platform_onFree Allocator_lock failed");
			retError(clean, Error_invalidState(0, "Platform_onFree() allocator lock failed to acquire"));
		}

		U64 i = 0;

		for (; i < Allocator_allocations.length; ++i) {

			const DebugAllocation *captured = &Allocator_allocations.ptrNonConst[i];

			if (captured->location == (U64)ptr) {

				if(len != captured->length) {

					Log_errorLn(
						&Allocator_allocationsAllocator,
						"Allocation at %p was allocated with length %"PRIu64" but freed with length %"PRIu64"!",
						ptr, captured->length, len
					);

					break;        //Still erase it, but reluctantly
				}

				break;
			}

			//Overlapping range, but likely by accident! Log it, but refuse to track it.

			else if ((U64)ptr >= captured->location && (U64)ptr < captured->location + captured->length)
				Log_errorLn(
					&Allocator_allocationsAllocator,
					"Allocation at %p with length %"PRIu64" collides with free at %p with length %"PRIu64" this is invalid!",
					captured->location, captured->length, ptr, len
				);
		}

		if(i == Allocator_allocations.length) {

			Log_errorLn(
				&Allocator_allocationsAllocator,
				"Allocation that was freed at %p with length %"PRIu64" was not found in the allocation list!",
				ptr, len
			);

			s_uccess = false;
			goto clean;
		}

		ListDebugAllocation_erase(&Allocator_allocations, i, NULL);

		if (acq == ELockAcquire_Acquired) {
			SpinLock_unlock(&Allocator_lock);
			acq = ELockAcquire_Invalid;
		}

	#endif

	//Free from counter

	AtomicI64_sub(&Allocator_memoryAllocationCount, 1);
	AtomicI64_sub(&Allocator_memoryAllocationSize, len);

	goto clean;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Allocator_lock);

	return s_uccess;
}

void Platform_printAllocations(U64 offset, U64 length, U64 minAllocationSize) {

	(void) offset; (void) length; (void) minAllocationSize;

	ELockAcquire acq = ELockAcquire_Invalid;
	Bool s_uccess = true;
	Error *e_rr = NULL;
	(void) s_uccess; (void)e_rr;

	#ifndef NDEBUG

		acq = SpinLock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success) {
			Log_errorLn(&Allocator_allocationsAllocator, "Platform_printAllocations Allocator_lock failed");
			retError(clean, Error_invalidState(0, "Platform_printAllocations() allocator lock failed to acquire"));
		}

		if(!length)
			length = Allocator_allocations.length;

		Log_debugLn(
			&Allocator_allocationsAllocator, "Showing up to %"PRIu64" allocations starting at offset %"PRIu64"", length, offset
		);

		U64 capturedLength = 0;

		for(U64 i = offset; i < offset + length && i < Allocator_allocations.length; ++i) {

			const DebugAllocation *captured = &Allocator_allocations.ptrNonConst[i];

			if(captured->length < minAllocationSize)
				continue;

			Log_debugLn(
				&Allocator_allocationsAllocator,
				"Allocation %"PRIu64" at %p with length %"PRIu64" allocated at:",
				i, captured->location, captured->length
			);

			Log_printCapturedStackTrace(
				&Allocator_allocationsAllocator, captured->stack, ELogLevel_Debug, ELogOptions_Default
			);

			capturedLength += captured->length;
		}

		Log_debugLn(&Allocator_allocationsAllocator, "Showed %"PRIu64" bytes of allocations", capturedLength);

	#endif

	goto clean;

clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Allocator_lock);
}

U64 Platform_getActiveAllocations(U64 minAllocationSize) {
	
	(void)minAllocationSize;

	U64 active = 0;

	Bool s_uccess = true;
	Error *e_rr = NULL;
	(void) s_uccess; (void)e_rr;

	ELockAcquire acq = SpinLock_lock(&Allocator_lock, U64_MAX);

	if (acq < ELockAcquire_Success) {
		Log_errorLn(&Allocator_allocationsAllocator, "Platform_getActiveAllocations Allocator_lock failed");
		retError(clean, Error_invalidState(0, "Platform_getActiveAllocations() allocator lock failed to acquire"));
	}

	#ifndef NDEBUG

		for (U64 i = 0; i < Allocator_allocations.length; ++i) {

			const DebugAllocation* captured = &Allocator_allocations.ptrNonConst[i];

			if (captured->length < minAllocationSize)
				continue;

			++active;
		}

	#else
		active = AtomicI64_add(&Allocator_memoryAllocationCount, 0);
	#endif

clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Allocator_lock);

	return active;
}

void Allocator_reportLeaks() {

	const I64 memCount = AtomicI64_add(&Allocator_memoryAllocationCount, 0);
	const I64 memSize = AtomicI64_add(&Allocator_memoryAllocationSize, 0);

	if (memCount || memSize) {

		Log_warnLn(&Allocator_allocationsAllocator, "Leaked %"PRIu64" bytes in %"PRIu64" allocations.", memSize, memCount);

		#ifndef NDEBUG
			Platform_printAllocations(0, 8, 0);
		#endif
	}
}

//Platform creation

Platform *Platform_instance = 0, platformInstance = { 0 };

Bool Platform_create(int cmdArgc, const C8 *cmdArgs[], void *data, void *allocator, Bool useWorkingDir, Error *e_rr) {

	Bool s_uccess = true;

	if(Platform_instance)
		retError(clean, Error_invalidOperation(0, "Platform_create() failed, platform was already initialized"));

	if(cmdArgc && !cmdArgs)
		retError(clean, Error_invalidParameter(
			!cmdArgc ? 0 : 1, 0, "Platform_create()::cmdArgs are required when cmdArgc is set"
		));

	#ifndef _NO_SIGNAL_HANDLING
		signal(SIGABRT, sigFunc);
		signal(SIGFPE,    sigFunc);
		signal(SIGILL,    sigFunc);
		signal(SIGINT,    sigFunc);
		signal(SIGSEGV, sigFunc);
		signal(SIGTERM, sigFunc);
	#endif

	Allocator_allocationsAllocator = (Allocator) {
		.alloc = Platform_allocNoTracking,
		.free = Platform_freeNoTracking
	};

	Allocator_trackedAllocator = (Allocator) {
		.ptr = allocator,
		.alloc = Platform_allocTracked,
		.free = Platform_freeTracked
	};

	#ifndef NDEBUG
		gotoIfError3(clean, ListDebugAllocation_reserve(&Allocator_allocations, 256, &Allocator_allocationsAllocator, e_rr));
	#endif

	Allocator_memoryAllocationCount = Allocator_memoryAllocationSize = (AtomicI64) { 0 };

	Platform_instance = &platformInstance;
	*Platform_instance = (Platform) {
		.platformType = _PLATFORM_TYPE,
		.cpuFeatures = Platform_detectCPUFeatures(),
		.data = data,
		.alloc = &Allocator_trackedAllocator
	};

	Platform_detectCPUInfo(&Platform_instance->cpuInfo);

	//After the instance exists on purpose: the SIMD backends diagnose a rejection through Log, which needs the
	// platform allocator, and nothing done above this point depends on the verdict (the failure path cleans it all up).

	if(!Platform_checkCPUSupport())
		retError(clean, Error_unsupportedOperation(
			0, "Platform_create() failed: Unsupported CPU. Please look at the minimum requirements to run OxC3"
		));

	ListCharString sl = (ListCharString) { 0 };

	if(cmdArgc > 1) {

		gotoIfError3(clean, ListCharString_create(cmdArgc - 1, Platform_instance->alloc, &sl, e_rr));

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptrNonConst[i - 1] = CharString_createRefCStrConst(cmdArgs[i]);
	}

	Platform_instance->args = sl;
	Platform_instance->useWorkingDir = useWorkingDir;

	gotoIfError3(clean, Platform_initExt(e_rr));

clean:

	if(!s_uccess)
		Platform_cleanup();

	return s_uccess;
}

void Platform_cleanup() {

	if(!Platform_instance)
		return;

	CharString_free(&Platform_instance->workDirectory, Platform_instance->alloc);
	CharString_free(&Platform_instance->appDirectory, Platform_instance->alloc);
	ListCharString_free(&Platform_instance->args, Platform_instance->alloc);

	SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {
		VirtualSection *sect = &Platform_instance->virtualSections.ptrNonConst[i];
		CharString_free(&sect->path, Platform_instance->alloc);
	}

	for(U64 i = 0; i < Platform_instance->archives.length; ++i)
		CAFile_free(&Platform_instance->archives.ptrNonConst[i], Platform_instance->alloc);

	ListCAFile_free(&Platform_instance->archives, Platform_instance->alloc);
	ListVirtualSection_free(&Platform_instance->virtualSections, Platform_instance->alloc);

	Allocator_reportLeaks();

	ListDebugAllocation_free(&Allocator_allocations, &Allocator_allocationsAllocator);

	Platform_cleanupExt();

	*Platform_instance = (Platform) { 0 };
	Platform_instance = NULL;
}
