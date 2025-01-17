/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/container/archive.h"
#include "types/base/lock.h"
#include "platforms/dynamic_library.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct VirtualSection {

	CharString path;

	const void *dataExt;	//Information about how to load the virtual file

	Archive loadedData;		//If the data is in memory, this will be used

	Bool loaded;
	U8 padding[7];

	U64 lenExt;				//Dependent on platform if it contains the length of the section

} VirtualSection;

TList(VirtualSection);

typedef struct Platform {

	EPlatform platformType;
	U32 pad0;

	ListCharString args;
	CharString defaultDir;				//Either workDir or appDir based on 'useWorkingDir'
	CharString workDirectory;			//Contains a trailing slash to make file stuff easier
	CharString appDirectory;			//Installation location if useWorkingDir, otherwise same as workDirectory

	Allocator alloc;

	SpinLock virtualSectionsLock;
	ListVirtualSection virtualSections;

	void *data;

	Bool useWorkingDir;		//TODO: Find a better solution for this for dlls
	U8 pad1[3];

} Platform;

extern Platform *Platform_instance;

Error Platform_create(int cmdArgc, const C8 *cmdArgs[], void *data, void *allocator, Bool useWorkingDir);

impl void Platform_cleanupExt();
impl Bool Platform_initExt(Error *e_rr);

impl Bool Platform_checkCPUSupport();								//SIMD dependent: SSE, None, NEON
impl U64 Platform_getThreads();

void Platform_cleanup();			//Call on exit

impl void *Platform_getDataImpl(void *ptr);

#if _PLATFORM_TYPE == PLATFORM_ANDROID
	typedef struct android_app android_app;
	#define Platform_defineEntrypoint() void android_main(android_app *state)
	#define Platform_getData() (state)
	#define Platform_argc (0)
	#define Platform_argv (NULL)
	#define Platform_return(x) return
#else
	#define Platform_defineEntrypoint() int main(int argc, const char *argv[])
	#define Platform_getData() Platform_getDataImpl(NULL)
	#define Platform_argc argc
	#define Platform_argv argv
	#define Platform_return(...) return __VA_ARGS__
#endif

//Call this on allocate to make sure the platform's allocator tracks allocations

Error Platform_onAllocate(void *ptr, U64 length);
Bool Platform_onFree(void *ptr, U64 length);

//Default allocator

impl void *Platform_allocate(void *allocator, U64 length);
impl void Platform_free(void *allocator, void *ptr, U64 length);

//Debugging to see where allocations came from and how many are active

void Platform_printAllocations(U64 offset, U64 length, U64 minAllocationSize);

#ifdef __cplusplus
	}
#endif
