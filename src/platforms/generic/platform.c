/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "platforms/ext/archivex.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/thread.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"

#ifndef _MSC_VER
	#include <cpuid.h>
#endif

#include <signal.h>
#include <stdlib.h>

//Error handling

void sigFunc(int signal) {

	const C8 *msg = "Undefined instruction";

	switch (signal) {
		case SIGABRT:	msg = "Abort was called";					break;
		case SIGFPE:	msg = "Floating point error occurred";		break;
		case SIGILL:	msg = "Illegal instruction";				break;
		case SIGINT:	msg = "Interrupt was called";				break;
		case SIGSEGV:	msg = "Segfault";							break;
		case SIGTERM:	msg = "Terminate was called";				break;
	}

	//Outputting to console is not technically allowed by the Windows docs
	//If this signal is triggered from the wrong thread it might cause stackoverflow,
	//but what are you gonna do? Crash again?
	//For debugging purposed however, this is very useful
	//Turn this off by defining _NO_SIGNAL_HANDLING

	Log_printStackTracex(1, ELogLevel_Error, ELogOptions_Default);
	Log_logx(ELogLevel_Error, ELogOptions_Default, CharString_createRefCStrConst(msg));
	exit(signal);
}

//Allocator keeps track of allocations on debug mode (and only about size/count on release)
//For these allocations, it has a special allocator that doesn't use itself.

AtomicI64 Allocator_memoryAllocationCount;
AtomicI64 Allocator_memoryAllocationSize;

typedef struct DebugAllocation {
	U64 location, length;
	StackTrace stack;
} DebugAllocation;

TList(DebugAllocation);
TListImpl(DebugAllocation);

Allocator Allocator_allocationsAllocator;
ListDebugAllocation Allocator_allocations;						//TODO: Use hashmap here!
SpinLock Allocator_lock;							//Multi threading safety

//Allocation

Error Platform_allocNoTracking(void *allocator, U64 length, Buffer *output) {

	(void)allocator;

	if(!output)
		return Error_nullPointer(2, "allocCallbackNoCheck()::output is required");

	void *ptr = Platform_allocate(allocator, length);

	if(!ptr)
		return Error_outOfMemory(0, "allocCallbackNoCheck() malloc failed");

	*output = Buffer_createManagedPtr(ptr, length);
	return Error_none();
}

Bool Platform_freeNoTracking(void *allocator, Buffer buf) {
	(void)allocator;
	Platform_free(allocator, (U8*) buf.ptr, Buffer_length(buf));
	return true;
}

//Normal allocator

Error Platform_allocTracked(void *allocator, U64 length, Buffer *output) {

	(void) allocator;

	if(!output)
		return Error_nullPointer(2, "allocCallback()::output is required");

	void *ptr = Platform_allocate(allocator, length);

	if(!ptr)
		return Error_outOfMemory(0, "allocCallback() malloc failed");

	const Error err = Platform_onAllocate(ptr, length);

	if (err.genericError) {
		free(ptr);
		return err;
	}

	*output = Buffer_createManagedPtr(ptr, length);
	return Error_none();
}

Bool Platform_freeTracked(void *allocator, Buffer buf) {

	(void) allocator;

	const Bool canFree = Platform_onFree((void*) buf.ptr, Buffer_length(buf));

	if(canFree)
		Platform_free(allocator, (void*) buf.ptr, Buffer_length(buf));

	return canFree;
}

Error Platform_onAllocate(void *ptr, U64 length) {

	(void)ptr;

	AtomicI64_add(&Allocator_memoryAllocationCount, 1);
	AtomicI64_add(&Allocator_memoryAllocationSize, length);

	#ifndef NDEBUG

		DebugAllocation captured = (DebugAllocation) { 0 };
		captured.location = (U64) ptr;
		captured.length = length;

		Log_captureStackTrace(Allocator_allocationsAllocator, captured.stack, STACKTRACE_SIZE, 1);

		const ELockAcquire acq = SpinLock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			return Error_invalidState(0, "allocCallback() allocator lock failed to acquire");			//Should never happen

		const Error err = ListDebugAllocation_pushBack(&Allocator_allocations, captured, Allocator_allocationsAllocator);

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&Allocator_lock);

		if(err.genericError)
			return err;

	#endif

	return Error_none();
}

Bool Platform_onFree(void *ptr, U64 len) {

	(void) ptr;

	//Validate if allocation and allocation size matches.
	//If not, warn here and return false

	ELockAcquire acq = ELockAcquire_Invalid;
	Bool success = true;

	#ifndef NDEBUG

		acq = SpinLock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			return false;

		U64 i = 0;

		for (; i < Allocator_allocations.length; ++i) {

			const DebugAllocation *captured = &Allocator_allocations.ptrNonConst[i];

			if (captured->location == (U64)ptr) {

				if(len != captured->length) {

					Log_errorLn(
						Allocator_allocationsAllocator,
						"Allocation at %p was allocated with length %"PRIu64" but freed with length %"PRIu64"!",
						ptr, captured->length, len
					);

					break;		//Still erase it, but reluctantly
				}

				break;
			}
		}

		if(i == Allocator_allocations.length) {

			Log_errorLn(
				Allocator_allocationsAllocator,
				"Allocation that was freed at %p with length %"PRIu64" was not found in the allocation list!",
				ptr, len
			);

			success = false;
			goto clean;
		}

		ListDebugAllocation_erase(&Allocator_allocations, i);

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&Allocator_lock);

	#endif

	//Free from counter

	AtomicI64_sub(&Allocator_memoryAllocationCount, 1);
	AtomicI64_sub(&Allocator_memoryAllocationSize, len);

	goto clean;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Allocator_lock);

	return success;
}

void Platform_printAllocations(U64 offset, U64 length, U64 minAllocationSize) {

	(void) offset; (void) length; (void) minAllocationSize;

	#ifndef NDEBUG

		const ELockAcquire acq = SpinLock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			return;

		if(!length)
			length = Allocator_allocations.length;

		Log_debugLn(
			Allocator_allocationsAllocator, "Showing up to %"PRIu64" allocations starting at offset %"PRIu64"", length, offset
		);

		U64 capturedLength = 0;

		for(U64 i = offset; i < offset + length && i < Allocator_allocations.length; ++i) {

			const DebugAllocation *captured = &Allocator_allocations.ptrNonConst[i];

			if(captured->length < minAllocationSize)
				continue;

			Log_debugLn(
				Allocator_allocationsAllocator,
				"Allocation %"PRIu64" at %p with length %"PRIu64" allocated at:",
				i, captured->location, captured->length
			);

			Log_printCapturedStackTrace(
				Allocator_allocationsAllocator, captured->stack, ELogLevel_Debug, ELogOptions_Default
			);

			capturedLength += captured->length;
		}

		Log_debugLn(Allocator_allocationsAllocator, "Showed %"PRIu64" bytes of allocations", capturedLength);

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&Allocator_lock);

	#endif
}

void Allocator_reportLeaks() {

	const I64 memCount = AtomicI64_add(&Allocator_memoryAllocationCount, 0);
	const I64 memSize = AtomicI64_add(&Allocator_memoryAllocationSize, 0);

	if (memCount || memSize) {

		Log_warnLn(Allocator_allocationsAllocator, "Leaked %"PRIu64" bytes in %"PRIu64" allocations.", memSize, memCount);

		#ifndef NDEBUG
			Platform_printAllocations(0, 16, 0);
		#endif
	}
}

//Virtual section

TListImpl(VirtualSection);

//Platform creation

Platform *Platform_instance = 0, platformInstance = { 0 };

Error Platform_create(int cmdArgc, const C8 *cmdArgs[], void *data, void *allocator, Bool useWorkingDir) {

	U16 v = 1;

	if(!*(U8*)&v)
		return Error_unsupportedOperation(
			0, "Platform_create() failed, invalid endianness (only little endian supported)"
		);

	const Bool isSupported = Platform_checkCPUSupport();

	if(!isSupported)
		return Error_unsupportedOperation(
			0,
			"Platform_create() failed: Unsupported CPU. The following extensions are required: "
			"SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AES, RDRAND, BMI1, PCLMULQDQ"
		);

	if(Platform_instance)
		return Error_invalidOperation(0, "Platform_create() failed, platform was already initialized");

	if(!cmdArgc || !cmdArgs)
		return Error_invalidParameter(
			!cmdArgc ? 0 : 1, 0, "Platform_create()::cmdArgc and cmdArgs are required"
		);

	#ifndef _NO_SIGNAL_HANDLING
		signal(SIGABRT, sigFunc);
		signal(SIGFPE,	sigFunc);
		signal(SIGILL,	sigFunc);
		signal(SIGINT,	sigFunc);
		signal(SIGSEGV, sigFunc);
		signal(SIGTERM, sigFunc);
	#endif

	Allocator_allocationsAllocator = (Allocator) {
		.alloc = Platform_allocNoTracking,
		.free = Platform_freeNoTracking
	};

	#ifndef NDEBUG
		{
			const Error err = ListDebugAllocation_reserve(&Allocator_allocations, 256, Allocator_allocationsAllocator);

			if(err.genericError)
				return err;

			Allocator_memoryAllocationCount = Allocator_memoryAllocationSize = (AtomicI64) { 0 };
		}
	#endif

	Platform_instance = &platformInstance;
	*Platform_instance = (Platform) {
		.platformType = _PLATFORM_TYPE,
		.data = data,
		.alloc = (Allocator) {
			.free = Platform_freeTracked,
			.alloc = Platform_allocTracked,
			.ptr = allocator
		}
	};

	Error err;
	CharString appDir = CharString_createNull();

	ListCharString sl = (ListCharString) { 0 };

	if(cmdArgc > 1) {

		gotoIfError(clean, ListCharString_createx(cmdArgc - 1, &sl));

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptrNonConst[i - 1] = CharString_createRefCStrConst(cmdArgs[i]);
	}

	Platform_instance->args = sl;
	Platform_instance->useWorkingDir = useWorkingDir;

	if(!useWorkingDir) {

		//Grab app directory of where the exe is installed

		gotoIfError(clean, CharString_createCopyx(CharString_createRefCStrConst(cmdArgs[0]), &appDir));

		CharString_replaceAllSensitive(&appDir, '\\', '/', 0, 0);

		const U64 loc = CharString_findLastSensitive(appDir, '/', 0, 0);
		CharString basePath = CharString_createNull();

		if (loc == CharString_length(appDir))
			basePath = CharString_createRefAutoConst(appDir.ptr, CharString_length(appDir));

		else CharString_cut(appDir, 0, loc + 1, &basePath);

		gotoIfError(clean, CharString_createCopyx(basePath, &Platform_instance->workingDirectory));

		if(!CharString_endsWithSensitive(basePath, '/', 0))
			gotoIfError(clean, CharString_appendx(&Platform_instance->workingDirectory, '/'));
	}

	gotoIfError(clean, Platform_initExt());

clean:

	CharString_freex(&appDir);

	if(err.genericError)
		Platform_cleanup();

	return err;
}

void Platform_cleanup() {

	if(!Platform_instance)
		return;

	CharString_freex(&Platform_instance->workingDirectory);
	ListCharString_freex(&Platform_instance->args);

	//Properly clean virtual files

	SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {
		VirtualSection *sect = &Platform_instance->virtualSections.ptrNonConst[i];
		CharString_freex(&sect->path);
		Archive_freex(&sect->loadedData);
	}

	ListVirtualSection_freex(&Platform_instance->virtualSections);

	//Reset console text color

	Allocator_reportLeaks();

	ListDebugAllocation_free(&Allocator_allocations, Allocator_allocationsAllocator);

	Platform_cleanupExt();

	*Platform_instance = (Platform) { 0 };
	Platform_instance = NULL;
}

Bool SpinLock_isLockedForThread(SpinLock *l) {
	return AtomicI64_add(&l->lockedThreadId, 0) == (I64) Thread_getId();
}

void Log_printCapturedStackTracex(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTrace(Platform_instance->alloc, stackTrace, lvl, options);
}
