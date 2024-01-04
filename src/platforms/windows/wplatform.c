/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/atomic.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/archivex.h"
#include "platforms/windows/wplatform_ext.h"

#include <locale.h>
#include <signal.h>
#include <stdlib.h>

#include <intrin.h>

CharString Error_formatPlatformError(Allocator alloc, Error err) {

	if(err.genericError != EGenericError_PlatformError)
		return CharString_createNull();

	if(!FAILED(err.paramValue0))
		return CharString_createNull();

	C8 *lpBuffer = NULL;

	DWORD f = FormatMessageA(

		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,

		NULL,
		(DWORD) err.paramValue0,

		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),

		(LPTSTR)&lpBuffer,
		0,

		NULL
	);

	//Unfortunately we have to copy, because we can't tell CharString to use LocalFree instead of free

	if(!f)
		return CharString_createNull();

	CharString res;
	if((err = CharString_createCopy(CharString_createConstRefSized(lpBuffer, f, true), alloc, &res)).genericError) {
		LocalFree(lpBuffer);
		return CharString_createNull();
	}

	//

	LocalFree(lpBuffer);
	return res;
}

//Handle crash signals

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
	Log_logx(ELogLevel_Fatal, ELogOptions_Default, CharString_createConstRefCStr(msg));
	exit(signal);
}

AtomicI64 Allocator_memoryAllocationCount;
AtomicI64 Allocator_memoryAllocationSize;

//Allocator has special allocator for the allocations

typedef struct DebugAllocation {

	U64 location, length;
	StackTrace stack;

} DebugAllocation;

TList(DebugAllocation);
TListImpl(DebugAllocation);

Allocator Allocator_allocationsAllocator;
ListDebugAllocation Allocator_allocations;						//TODO: Use hashmap here!
Lock Allocator_lock;							//Multi threading safety

Error allocCallbackNoCheck(void *allocator, U64 length, Buffer *output) {

	allocator;

	if(!output)
		return Error_nullPointer(2, "allocCallbackNoCheck()::output is required");

	void *ptr = malloc(length);

	if(!ptr)
		return Error_outOfMemory(0, "allocCallbackNoCheck() malloc failed");

	*output = Buffer_createManagedPtr(ptr, length);
	return Error_none();
}

Bool freeCallbackNoCheck(void *allocator, Buffer buf) {
	allocator;
	free((U8*) buf.ptr);
	return true;
}

//Normal allocator

Error allocCallback(void *allocator, U64 length, Buffer *output) {

	allocator;

	if(!output)
		return Error_nullPointer(2, "allocCallback()::output is required");

	void *ptr = malloc(length);

	if(!ptr)
		return Error_outOfMemory(0, "allocCallback() malloc failed");

	AtomicI64_add(&Allocator_memoryAllocationCount, 1);
	AtomicI64_add(&Allocator_memoryAllocationSize, length);
	
	#ifndef NDEBUG

		DebugAllocation captured = (DebugAllocation) { 0 };
		captured.location = (U64) ptr;
		captured.length = length;

		Log_captureStackTrace(captured.stack, _STACKTRACE_SIZE, 1);

		ELockAcquire acq = Lock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			return Error_invalidState(0, "allocCallback() allocator lock failed to acquire");			//Should never happen

		Error err = ListDebugAllocation_pushBack(&Allocator_allocations, captured, Allocator_allocationsAllocator);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&Allocator_lock);

		if(err.genericError)
			return err;

	#endif

	*output = Buffer_createManagedPtr(ptr, length);
	return Error_none();
}

Bool freeCallback(void *allocator, Buffer buf) {

	allocator;

	//Validate if allocation and allocation size matches.
	//If not, warn here and return false

	ELockAcquire acq = ELockAcquire_Invalid;
	Bool success = true;

	#ifndef NDEBUG

		acq = Lock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			return false;

		U64 i = 0;

		for (; i < Allocator_allocations.length; ++i) {

			DebugAllocation *captured = &Allocator_allocations.ptrNonConst[i];

			if (captured->location == (U64)buf.ptr) {

				if(Buffer_length(buf) != captured->length) {

					Log_errorLn(
						Allocator_allocationsAllocator,
						"Allocation at %p was allocated with length %llu but freed with length %llu!",
						buf.ptr,
						captured->length,
						Buffer_length(buf)
					);

					success = false;
					goto clean;
				}

				break;
			}
		}

		if(i == Allocator_allocations.length) {

			Log_errorLn(
				Allocator_allocationsAllocator,
				"Allocation that was freed at %p with length %llu was not found in the allocation list!",
				buf.ptr,
				Buffer_length(buf)
			);

			success = false;
			goto clean;
		}

		ListDebugAllocation_erase(&Allocator_allocations, i);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&Allocator_lock);

	#endif

	//Free from counter

	AtomicI64_sub(&Allocator_memoryAllocationCount, 1);
	AtomicI64_sub(&Allocator_memoryAllocationSize, Buffer_length(buf));

	//Free mem

	free((U8*) buf.ptr);
	goto clean;

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&Allocator_lock);

	return success;
}

void Platform_printAllocations(U64 offset, U64 length, U64 minAllocationSize) {

	offset; length; minAllocationSize;

	#ifndef NDEBUG

		ELockAcquire acq = Lock_lock(&Allocator_lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			return;

		if(!length)
			length = Allocator_allocations.length;

		Log_debugLn(
			Allocator_allocationsAllocator, "Showing up to %llu allocations starting at offset %llu", length, offset
		);

		U64 capturedLength = 0;
	
		for(U64 i = offset; i < offset + length && i < Allocator_allocations.length; ++i) {

			DebugAllocation *captured = &Allocator_allocations.ptrNonConst[i];

			if(captured->length < minAllocationSize)
				continue;

			Log_debugLn(
				Allocator_allocationsAllocator, 
				"Allocation %llu at %p with length %llu allocated at:", 
				i, captured->location, captured->length
			);

			Log_printCapturedStackTrace(Allocator_allocationsAllocator, captured->stack, ELogLevel_Debug, ELogOptions_Default);

			capturedLength += captured->length;
		}

		Log_debugLn(Allocator_allocationsAllocator, "Showed %llu bytes of allocations", capturedLength);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&Allocator_lock);

	#endif
}

void Allocator_reportLeaks() {

	I64 memCount = AtomicI64_add(&Allocator_memoryAllocationCount, 0);
	I64 memSize = AtomicI64_add(&Allocator_memoryAllocationSize, 0);

	if (memCount || memSize) {

		Log_warnLn(Allocator_allocationsAllocator, "Leaked %llu bytes in %llu allocations.", memSize, memCount);

		#ifndef NDEBUG
			Platform_printAllocations(0, 16, 0);
		#endif
	}
}

WORD oldColor = 0;

int main(int argc, const char *argv[]) {
	
	#ifndef _NO_SIGNAL_HANDLING
		signal(SIGABRT, sigFunc);
		signal(SIGFPE,	sigFunc);
		signal(SIGILL,	sigFunc);
		signal(SIGINT,	sigFunc);
		signal(SIGSEGV, sigFunc);
		signal(SIGTERM, sigFunc);
	#endif
	
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	oldColor = info.wAttributes;

	Allocator_allocationsAllocator = (Allocator) {
		.alloc = allocCallbackNoCheck,
		.free = freeCallbackNoCheck
	};

	#ifndef NDEBUG
		{
			Error err = ListDebugAllocation_reserve(&Allocator_allocations, 256, Allocator_allocationsAllocator);

			if(err.genericError)
				return -1;

			Allocator_lock = Lock_create();
			Allocator_memoryAllocationCount = Allocator_memoryAllocationSize = (AtomicI64) { 0 };
		}
	#endif

	Error err = Platform_create(argc, argv, GetModuleHandleA(NULL), freeCallback, allocCallback, NULL);

	if(err.genericError)
		return -3;

	#if _SIMD == SIMD_SSE

		//We need to double check that our CPU supports 
		//SSE4.2, SSE4.1, (S)SSE3, SSE2, SSE, AES, PCLMULQDQ, BMI1 and RDRAND
		//https://gist.github.com/hi2p-perim/7855506
		//https://en.wikipedia.org/wiki/CPUID

		int mask3 = (1 << 25) | (1 << 26);										//SSE, SSE2

		//SSE3, PCLMULQDQ, SSSE3, SSE4.1, SSE4.2, AES, RDRAND
		int mask2 = (1 << 0) | (1 << 1) | (1 << 9) | (1 << 19) | (1 << 20) | (1 << 25) | (1 << 30);

		int cpuInfo[4];
		__cpuid(cpuInfo, 1);

		int cpuInfo1[4];
		__cpuidex(cpuInfo1, 7, 0);

		int mask1_1 = 1 << 3;				//BMI1

		//Unsupported CPU

		if ((cpuInfo[3] & mask3) != mask3 || (cpuInfo[2] & mask2) != mask2 || (cpuInfo1[1] & mask1_1) != mask1_1) {

			Log_errorx(
				ELogOptions_Default,
				"Unsupported CPU. The following extensions are required: "
				"SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AES, RDRAND, BMI1, PCLMULQDQ"
			);

			Platform_cleanup();
			return -4;
		}

	#endif

	int res = Program_run();
	Program_exit();
	Platform_cleanup();

	return res;
}

void Platform_cleanupExt() {

	//Properly clean virtual files

	Lock_free(&Platform_instance.virtualSectionsLock);

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {
		VirtualSection *sect = &Platform_instance.virtualSections.ptrNonConst[i];
		CharString_freex(&sect->path);
		Archive_freex(&sect->loadedData);
	}

	ListVirtualSection_freex(&Platform_instance.virtualSections);

	//Cleanup platform ext

	if(Platform_instance.dataExt) {
		Buffer buf = Buffer_createManagedPtr(Platform_instance.dataExt, sizeof(PlatformExt));
		Buffer_freex(&buf);
		Platform_instance.dataExt = NULL;
	}

	//Reset console text color

	Allocator_reportLeaks();

	ListDebugAllocation_free(&Allocator_allocations, Allocator_allocationsAllocator);

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), oldColor);
}

typedef struct EnumerateFiles {
	ListVirtualSection *sections;
	Bool b;
} EnumerateFiles;

BOOL enumerateFiles(HMODULE mod, LPCSTR unused, LPSTR name, EnumerateFiles *sections) {

	mod; unused;

	CharString str = CharString_createConstRefCStr(name);

	Error err = Error_none();
	CharString copy = CharString_createNull();

	if(CharString_countAllSensitive(str, '/') != 1)
		Log_warnLnx("Executable contained unrecognized RCDATA. Ignoring it...");

	else {

		_gotoIfError(clean, CharString_createCopyx(str, &copy));

		VirtualSection section = (VirtualSection) { .path = copy };
		_gotoIfError(clean, ListVirtualSection_pushBackx(sections->sections, section));
	}

clean:

	if(err.genericError) {
		sections->b = true;			//Signal that we failed
		CharString_freex(&copy);
	}

	return !err.genericError;
}

Error Platform_initExt(CharString currAppDir) {

	//

	Buffer platformExt = Buffer_createNull();
	Error err = Buffer_createEmptyBytesx(sizeof(PlatformExt), &platformExt);

	if(err.genericError)
		return err;

	PlatformExt *pext = (PlatformExt*) platformExt.ptr;

	pext->ntdll = GetModuleHandleA("ntdll.dll");

	if (!pext->ntdll) {
		Buffer_freex(&platformExt);
		return Error_platformError(0, GetLastError(), "Platform_initExt() couldn't find ntdll");
	}

	*((void**)&pext->ntDelayExecution) = (void*)GetProcAddress(pext->ntdll, "NtDelayExecution");

	if (!pext->ntDelayExecution) {
		Buffer_freex(&platformExt);
		return Error_platformError(1, GetLastError(), "Platform_initExt() couldn't find NtDelayExecution");
	}
	
	if(!Platform_useWorkingDirectory) {

		//Grab app directory of where the exe is installed

		CharString appDir = CharString_createNull();
		if ((err = CharString_createCopyx(currAppDir, &appDir)).genericError) {
			CharString_freex(&appDir);
			Buffer_freex(&platformExt);
			return Error_platformError(1, GetLastError(), "Platform_initExt()::currAppDir couldn't createCopyx");
		}

		CharString_replaceAllSensitive(&appDir, '\\', '/');

		U64 loc = CharString_findLastSensitive(appDir, '/');
		CharString basePath = CharString_createNull();

		if (loc == CharString_length(appDir))
			basePath = CharString_createConstRefAuto(appDir.ptr, CharString_length(appDir));
	
		else CharString_cut(appDir, 0, loc + 1, &basePath);

		CharString workDir = CharString_createNull();

		if ((err = CharString_createCopyx(basePath, &workDir)).genericError) {
			CharString_freex(&appDir);
			Buffer_freex(&platformExt);
			return Error_platformError(1, GetLastError(), "Platform_initExt() basePath couldn't createCopyx");
		}

		if(
			!CharString_endsWithSensitive(basePath, '/') && 
			(err = CharString_appendx(&workDir, '/')).genericError
		)  {
			CharString_freex(&appDir);
			CharString_freex(&workDir);
			Buffer_freex(&platformExt);
			return err;
		}

		CharString_freex(&appDir);
		Platform_instance.workingDirectory = workDir;

	} else {

		//Init working dir

		C8 buff[MAX_PATH + 1];
		DWORD chars = GetCurrentDirectoryA(MAX_PATH + 1, buff);

		if(!chars) {

			Buffer_freex(&platformExt);

			//Needs no additional cleaning. cleanup(Ext) will handle it

			return Error_platformError(
				0, GetLastError(), "Platform_initExt() GetCurrentDirectory failed"
			);
		}

		//Move to heap and standardize

		if((err = CharString_createCopyx(
			CharString_createConstRefSized(buff, chars, true), &Platform_instance.workingDirectory
		)).genericError) {
			Buffer_freex(&platformExt);
			return err;
		}

		CharString_replaceAllSensitive(&Platform_instance.workingDirectory, '\\', '/');

		if ((err = CharString_appendx(&Platform_instance.workingDirectory, '/')).genericError)  {
			CharString_freex(&Platform_instance.workingDirectory);
			Buffer_freex(&platformExt);
			return err;
		}
	}
	
	//Init virtual files

	EnumerateFiles files = (EnumerateFiles) { .sections = &Platform_instance.virtualSections };

	if (!EnumResourceNamesA(
		NULL, RT_RCDATA,
		(ENUMRESNAMEPROCA)enumerateFiles,
		(LONG_PTR)&Platform_instance.virtualSections
	)) {

		//Enum resource names also fails if we don't have any resources.
		//To counter this, enumerateFiles sets stride to 0 if the reason it returned false was because of the function.

		if(files.b) {
			ListVirtualSection_freex(&Platform_instance.virtualSections);
			CharString_freex(&Platform_instance.workingDirectory);
			Buffer_freex(&platformExt);
			return Error_invalidState(1, "Platform_initExt() EnumResourceNames failed");
		}
	}

	Platform_instance.virtualSectionsLock = Lock_create();

	//Set platformExt

	Platform_instance.dataExt = pext;

	return Error_none();
}
