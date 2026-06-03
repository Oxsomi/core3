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

//platforms/platform.h

#pragma once
#include "formats/oiCA/ca_file.h"
#include "types/container/string.h"
#include "types/base/platform_types.h"
#include "types/base/string_base.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Thread Thread;
typedef struct CAFile CAFile;

typedef struct VirtualSection {

	CharString path;

	const void *dataExt;       //Information about how to load the virtual file

	U64 loadedAndId;           //(loaded << 63) | id (0 indicates not loaded)

	U64 lenExt;                //Dependent on platform if it contains the length of the section

} VirtualSection;

TList(VirtualSection);

typedef struct Platform {

	EPlatform platformType;

	Bool useWorkingDir;        //TODO: Find a better solution for this for dlls
	U8 pad1[3];

	ListCharString args;
	CharString defaultDir;              //Either workDir or appDir based on 'useWorkingDir'
	CharString workDirectory;           //Contains a trailing slash to make file stuff easier
	CharString appDirectory;            //Installation location if useWorkingDir, otherwise same as workDirectory

	const Allocator *alloc;

	SpinLock virtualSectionsLock;

	ListVirtualSection virtualSections;
	ListCAFile archives;

	void *data;

	void *data1;                        //If present can contain the executable file
	U64 size1;

} Platform;

extern Platform *Platform_instance;     //DLLs that call this need to also call Platform_create or get the same pointer passed.

Bool Platform_create(int cmdArgc, const C8 *cmdArgs[], void *data, void *allocator, Bool useWorkingDir, Error *e_rr);

impl void Platform_cleanupExt();
impl Bool Platform_initExt(Error *e_rr);

impl Bool Platform_checkCPUSupport();   //SIMD dependent: SSE, None, NEON
impl U64 Platform_getThreads();

//If the device has an on screen keyboard (e.g. Android) you need to call this before handling text input.
//Also make sure to hide it later with isVisible=false to avoid having a keyboard taking up half of your screen.
impl Bool Platform_setKeyboardVisible(Bool isVisible);

void Platform_cleanup();                //Call on exit

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

Bool Platform_onAllocate(void *ptr, U64 length, Error *e_rr);
Bool Platform_onFree(void *ptr, U64 length);

//Default allocator

impl void *Platform_allocate(void *allocator, U64 length);
impl void Platform_free(void *allocator, void *ptr, U64 length);

//Debugging to see where allocations came from and how many are active

void Platform_printAllocations(U64 offset, U64 length, U64 minAllocationSize);

//Get active allocations (to detect leaks) minAllocationSize is only respected in Debug mode.
U64 Platform_getActiveAllocations(U64 minAllocationSize);

#ifdef __cplusplus
	}
#endif
